// ESP32 Arduino core included library
#include <NimBLEDevice.h>      //  https://github.com/h2zero/NimBLE-Arduino
#include <MySQL.h>             //  https://github.com/cotestatnt/Arduino-MySQL

// Update at least to version 2.0.6
#include <esp-fs-webserver.h>  //  https://github.com/cotestatnt/esp-fs-webserver

#define HUB_VERSION  "1.0.10"

// Board pins
#define syncButton 2
#define wpsButton  0
#define ledOnboard RGB_BUILTIN

// Local files
#include "variables.h"
#include "ble_impl.h"       // BLE stuffs
#include "mysql_impl.h"     // MySQL stuffs
#include "webserver_impl.h" // web server stuff

void setup() {
  pinMode(ledOnboard, OUTPUT);
  digitalWrite(ledOnboard, LOW);
  pinMode(syncButton, INPUT_PULLUP);
  pinMode(wpsButton, INPUT_PULLUP);

  dataFromSQL.reserve(MAX_LEN);
  dataFromPeripheral.reserve(MAX_LEN);
  Serial.begin(115200);
  Serial.println("\n\n\n\nStarting ESP32 BLE-WiFi gateway...");
  Serial.print("Device MAC address: ");
  Serial.println(WiFi.macAddress());

  /* Init and start configuration webserver */
  startWebServer();

  /* Init and start MySQL task */
  xTaskCreate(mysqlTask, "mysqlTask", 12000, nullptr, 5, &mysqlHandler);

  /*
    Before begin BLE scanning, it would better wait for MySQL init
    so we know which BLE devices we should connect to.
  */
  uint32_t timeout = millis();
  while (!MySQLdone) {
    if (millis() - timeout > 10000) {
      Serial.println("!!! ERROR !!! - Something wrong with MySQL");
      break;
    }
    yield();
  }

  /* Print field list with max allowed data length */
  if (MySQLdone) {
    Serial.println(myFields.size() ? "\nField size used for data validation:" : "");
    for (Field_t field : myFields) {
      Serial.printf( "Field %s, max size %d\n", field.name.c_str(), field.size);
    }
    delay(100);

    /*
      Get the number of BLE devices to be connected
      MAC address must be univoque, so let's use a temporary "std::set" container
    */
    std::set<String> ble_macs;
    for (DeviceLink_t &link : links) {
      ble_macs.emplace(link.address);
    }
    nPeripheralToBeConnected = ble_macs.size();
    Serial.printf("\nExpected number of peripherals to be connected: %d\n", nPeripheralToBeConnected);
  }

  /*
    Scan BLE for peripherals with service uuid "555a0002-0000-467a-9538-01f0652c74e8"
    Once we discover a peripheral, perform a connection and subscribe required characteristics
  */
  NimBLEDevice::init("NimBLE-ESP32 Central");
  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);

  /*
    Scan nearby BLE devices
    If device advertise the expected service, a connection will be formed with it
  */
  scanForDevices(true, nullptr);

  Serial.println("\n******************* Start main loop *******************\n");
}


void loop() {
  myWebServer.run();

  // Start WPS
  static uint32_t btnWpsDebounce;
  if (millis() - btnWpsDebounce > 200 && digitalRead(wpsButton) == LOW) {
    btnWpsDebounce = millis();
    WiFi.mode(WIFI_MODE_STA);
    Serial.println("\nStarting WPS... ");
    wpsInitConfig();
    wpsStart();
  }

  // Scan nearby BLE devices and sync MySQL database
  static uint32_t btnSyncDebounce;
  if (millis() - btnSyncDebounce > 200 && digitalRead(syncButton) == LOW) {
    btnSyncDebounce = millis();
    Serial.println("Scan nearby BLE devices and sync MySQL database");
    addAllNearbyDevices();
  }

  // Check if the number of connected peripherals is equal to expected (from DB rows)
  static uint32_t startScan;
  if ((nPeripheralConnected < nPeripheralToBeConnected) && (millis() - startScan > 10000)) {
    startScan = millis();
    // Scan BLE for peripherals with service uuid "555a0002-0000-467a-9538-01f0652c74e8"
    scanForDevices(true, nullptr);
  }

  // If new message from one of connected BLE device...
  if (newMessageFrom != nullptr) {
    // Read the value of the characteristic.
    BLERemoteCharacteristic* pRemoteChar = newMessageFrom->getCharacteristic(stringCharUUID);
    if(pRemoteChar->canRead()) {
      String addr = newMessageFrom->getClient()->getPeerAddress().toString().c_str();

      // Some chars can break the SQL query and need to be escaped
      dataFromPeripheral = pRemoteChar->readValue().c_str();
      Serial.printf("Local message from %s >> %s\n", addr.c_str(), dataFromPeripheral.c_str());

      // The std::vector links contains relevant info for all connected BLE devices
      // It's possible connect the same device to multiple remote devices or even to ALL
      for (DeviceLink_t &link : links) {

        if (addr.equalsIgnoreCase(link.address)) {
          // Set TX_BUFFER once received text from local connected BLE device
          // Some chars can break the SQL query and needs to be escaped
          String esc = escape(dataFromPeripheral.c_str(), {'"', '\''});
          esc.trim();

          // Write out a new message in DB (table 'connections')
          DataQuery_t newData;
          if (queryExecute(newData, newMessage, tableConnections, esc.c_str(), link.id.c_str(), link.targets.c_str())) {
            Serial.println("MySQL record update succesfully");
            // Clear pointer to BLERemoteService
            newMessageFrom = nullptr;
          }
          else {
            Serial.println(sql.getLastError());
            delay(100);
          }
        }
      }

    }
  }

  // Forward data from HW Serial to BLE
  while (Serial.available()) {
    delay(2); // Delay to wait for enough input, since we have a limited transmission buffer
    char buf[MAX_LEN];
    int count = Serial.readBytes(buf, sizeof(buf));   // Read data from serial
    buf[count] = '\0';                                // Add string terminator

    // Broadcast serial message text to all connected peripherals
    std::list<NimBLEClient*>* connectedDevices = BLEDevice::getClientList();
    for (NimBLEClient* pClient : *connectedDevices) {
      BLERemoteService* pService = pClient->getService(customServiceUUID);
      BLERemoteCharacteristic*  pRemoteChar = pService->getCharacteristic(stringCharUUID);
      pRemoteChar->writeValue(buf, count);
    }
  }

  if (WiFi.status() != WL_CONNECTED && wpsEnabled){
    blinkConnectionLed();
  }
}


////////////////////////////////////  Utilities ////////////////////////////////////////////////

// A function to add the escape character to some characters
String escape(const char* src, const std::set<char> escapeChars) {
  String escaped;
  while (char ch = *src++) {
    if (escapeChars.find(ch) != escapeChars.end())
      escaped += '\\';
    escaped += ch;
  }
  return escaped;
}


// Checks whether the text length is allowable for the specific field
bool fieldDataValidation (const char* fname, const char* value) {
  for (Field_t field : myFields) {
    if (strncasecmp(field.name.c_str(), fname, strlen(fname)) == 0) {
      if (value != nullptr) {
        if (strlen(value) <= field.size) {
          return true;
        }
      }
    }
  }
  return false;
}
