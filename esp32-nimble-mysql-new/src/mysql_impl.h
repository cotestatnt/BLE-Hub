#include "variables.h"
#include "mysql_queries.h"
#include <NimBLEDevice.h>      //  https://github.com/h2zero/NimBLE-Arduino


#define MAX_QUERY_LEN       512     // MAX query string length
#define CLOSE_CONNECTION    false   // Since most online test DB has limits on connections number, keep this opened by default

enum ParseCommand {NONE, PRINT, LIST, MESSAGE};


bool checkAndCreateTables() {
  // Create tables if not exists
  Serial.println("\nCreate table if not exists");
  DataQuery_t data;
  if (!queryExecute(data, createHubs, tableHubs)) {
    if (strcmp(sql.getLastSQLSTATE(), "42S01") != 0)
      return false;
  }

  if (!queryExecute(data, createBleDevices, tableDevices)) {
    if (strcmp(sql.getLastSQLSTATE(), "42S01") != 0)
      return false;
  }

  if (!queryExecute(data, createConnections, tableConnections)) {
    if (strcmp(sql.getLastSQLSTATE(), "42S01") != 0)
      return false;
  }

  /*
    Create a trigger for 'connections' table:
    When a new message is putted out from BLE device,
    every target linked to device was updated with that message
  */
  if (queryExecute(data, newMessageTrigger, database.c_str())) {
    Serial.println("Created trigger on 'connections' table");
  }
  return true;
}

// Print the content of vector used for store links
void printLinksList() {
  Serial.println("\nConnections list handled from this HUB:");
  for (DeviceLink_t &link : links) {
    Serial.print("Device: ");
    Serial.print(link.id);
    Serial.print(" (");
    Serial.print(link.name);
    Serial.print("), MAC address: ");
    Serial.print(link.address);
    Serial.print(", Targets: ");
    Serial.println(link.targets);
  }
}

// Establish connection with MySQL database according to the variables defined (/setup webpage)
bool connectToDatabase() {
  if (sql.connected()) {
    return true;
  }
  Serial.printf("\nConnecting to MySQL server %s on DataBase %s...\n", dbHost.c_str(), database.c_str());
  if (sql.connect(user.c_str(), password.c_str(), database.c_str())) {
    delay(200);
    return true;
  }
  Serial.println("Fails!");
  sql.disconnect();
  return false;
}


// Variadic function that will execute the query selected with passed parameters
bool queryExecute(DataQuery_t& data, const char* queryStr, ...) {
  if (connectToDatabase()) {
    char buf[MAX_QUERY_LEN];
    va_list args;
    va_start (args, queryStr);
    vsnprintf (buf, sizeof(buf), queryStr, args);
    va_end (args);

    // Execute the query
    // Serial.printf("Execute SQL query: %s\n", buf);
    return sql.query(data, buf);
  }
  return false;
}


// Print the result of select query (don't use with queries which don't response with a dataset)
int parseQueryResult(DataQuery_t& data, uint8_t cmd) {

  // Iterate the query records
  int numRow = 0;
  for (Record_t rec : data.records) {

    // Create a copy of string fields to use below
    const char* address = data.getRowValue(numRow, "ble_address");
    const char* id = data.getRowValue(numRow, "ble_id");

    // A message was read from one of MySQL DB row.
    if (cmd == MESSAGE) {
      // Save message text for future use
      dataFromSQL = data.getRowValue(numRow, "msg_buffer");

      // Get a std::list of all connected BLE peripherals
      std::list<NimBLEClient*>* ret = BLEDevice::getClientList();

      // Iterate the std::list to check if one of connected peripherals is the target of message
      for (NimBLEClient* pClient : *ret) {
        String peerAddr = pClient->getPeerAddress().toString().c_str();

        // If the target address is equal to connected BLE peripherale address, send message
        if (peerAddr.equalsIgnoreCase(address) && dataFromSQL.length()) {

          // Get reference to service and characteristic
          BLERemoteService* pService = pClient->getService(customServiceUUID);
          BLERemoteCharacteristic*  pRemoteChar = pService->getCharacteristic(stringCharUUID);

          // Write local connected BLE device characteristic
          if (pRemoteChar->writeValue(dataFromSQL.c_str(), dataFromSQL.length())) {
            // Clear devices.msg_buffer to set message as read
            DataQuery_t update;
            if (queryExecute( update, "UPDATE %s SET msg_buffer ='' WHERE ble_address = '%s'", tableDevices, address)) {
              Serial.println("Device record update (message marked as read)");
            }
          }
          else {
            Serial.println("BLE characteristic write error!\n Check if device is connected to HUB");
          }
        }
      }
    }
    else if (cmd == LIST) {
      // Create local copy of links vector (only one for)
      DeviceLink_t newLink;
      newLink.address = address;
      newLink.id = id;
      newLink.targets = data.getRowValue(numRow, "targets");
      newLink.name = data.getRowValue(numRow, "ble_name");
      links.push_back(newLink);
    }
    numRow++;
  }

  return numRow;
}


/*
* Get the list of BLE devices that should be connected to this hub
*/
bool getDeviceListFromDB() {
  DataQuery_t data;
  if (queryExecute(data, selectDevices, hubName.c_str())) {
    // Save local copy of fields vector (name and max size) for data validation
    myFields = *data.getFields();
    // Print table structure
    sql.printResult(data, Serial);
    sql.printResult(data, *myWebServer.getWebSocketServer());
    // Parse data to get field (column) list
    links.clear();
    links.shrink_to_fit();
    std::vector<DeviceLink_t>().swap(links);
    parseQueryResult(data, ParseCommand::LIST);
    // Print the links we are looking for
    printLinksList();
    return true;
  }
  return false;
}

// ************************************************************************************** //
// ***********************  FreeRTOS task for MySQL polling ***************************** //
// ************************************************************************************** //
void mysqlTask(void* args) {
  static uint32_t lastPollTime;

  // Wait for WiFi before start MySQL connection
  while (!wifiConnected) {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }

  // Check if working table exists and create if not
  if (connectToDatabase()) {
    vTaskDelay(500 / portTICK_PERIOD_MS);
    if (!checkAndCreateTables()) {
      Serial.println("Error! Tables not created properly");
      vTaskDelete(mysqlHandler);
    }
    // get a list of BLE devices we should connect to.
    getDeviceListFromDB();
  }
  else {
    Serial.println("\nDatabase connection failed. This task will be closed.");
    Serial.println("Check your connection and ESP");
    delay(1000);
    vTaskDelete(mysqlHandler);
    return;
  }

  vTaskDelay(5000 / portTICK_PERIOD_MS);
  MySQLdone = true;
  for(;;) {
    // Update data every "pollTime" milliseconds
    // Anch check is there are messages for node connected to this HUB
    if (millis() - lastPollTime > pollTime) {
      lastPollTime = millis();

      // Check each devices connected to this esp32 HUB
      DataQuery_t data;
      if (queryExecute(data, checkMessages, tableDevices, hubName.c_str())) {
        parseQueryResult(data, ParseCommand::MESSAGE);
        sql.printResult(data, Serial);
        sql.printResult(data, *myWebServer.getWebSocketServer());
      }

      // Serial.printf("%d - Total free: %6d - Max block: %6d\n",
      //           nPeripheralConnected, heap_caps_get_free_size(0), heap_caps_get_largest_free_block(0)) ;
    }
    // Avoid to keep task running "full throttle"
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }

  vTaskDelete(mysqlHandler);
}
