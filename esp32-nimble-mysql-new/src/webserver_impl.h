#include "variables.h"
#include "index_htm.h"      // The default webpage (scan BLE devices)
#include "wifi_impl.h"


void addNewDeviceDB(const String& address,  const String& hub, const String& id, const String& name, bool rewrite = false) {
  /*
  Insert (or edit) device in MySQL database
  */
  String msqlStr;
  if (rewrite)
    msqlStr = newBleDeviceRewrite;   // Update the whole record with new data (ble_address exists)
  else
    msqlStr = newBleDevice;          // Don't change ble_id id defined

  DataQuery_t data;
  if (queryExecute( data, msqlStr.c_str(), address.c_str(), id.c_str(), hub.c_str(), name.c_str())) {
    myWebServer.send(200, "text/plain", "Device record updated/added to database");
    Serial.println("MySQL device record inserted succesfully");
  }
  else {
    myWebServer.send(500, "text/plain", sql.getLastError());
  }

  // Update current BLE device list
  getDeviceListFromDB();
}


////////////////////////////  HTTP Request Handlers  ////////////////////////////////////


// This function will add/update a device record in 'ble_devices' table
void handleNewDevice() {
  if (myWebServer.hasArg("ble_address")) {
    String address =  myWebServer.arg("ble_address");
    String id =  myWebServer.arg("ble_id");
    String hub =  myWebServer.arg("connected_to");
    String name =  myWebServer.arg("ble_name");
    addNewDeviceDB(address, hub, id, name, true);
  }
  myWebServer.send(500, "text/plain", sql.getLastError());
}


// This function will add/update a connection row in 'connections' table
void handleNewConnection() {
   if (myWebServer.hasArg("ble_id")) {
    String id =  myWebServer.arg("ble_id");
    String targets =  myWebServer.arg("targets");

    // Add new connection to database inserting new record
    DataQuery_t data;
    if (queryExecute( data,
          "INSERT INTO %s (ble_id, targets) VALUES ('%s', '%s')",
          tableConnections, id.c_str(), targets.c_str()))
    {
      myWebServer.send(200, "text/plain", "Connection record updated/added to database");
      Serial.println("MySQL connection record inserted succesfully");
    }

    // Record already present (ble_address+targets must be univoque)
    else {
      Serial.println(sql.getLastError());
      myWebServer.send(200, "text/plain", sql.getLastError());
    }
  }
  myWebServer.send(500, "text/plain", sql.getLastError());
}


// Scan BLE devices with customServiceUUID and send to client
void handleGetDeviceList() {
  // Get all BLE peripherals still not connected
  String json;
  scanForDevices(false, &json);
  Serial.printf("Scan not connected BLE devices:\n%s\n>", json.c_str());

  JsonDocument doc;
  deserializeJson(doc, json);

  // Add devices already connected
  JsonArray array = doc["ble_devices"].as<JsonArray>();
  std::list<NimBLEClient*>* connectedDevices = BLEDevice::getClientList();
  for (NimBLEClient* pClient : *connectedDevices) {
    uint8_t battery = 0;
    BLERemoteService* pBatteryService = pClient->getService(BLEUUID(UUID16_SVC_BATTERY));
    if (pBatteryService != nullptr) {
      BLERemoteCharacteristic* pBatteryChar = pBatteryService->getCharacteristic(BLEUUID(UUID16_CHR_BATTERY_LEVEL));
      if (pBatteryChar != nullptr) {
        battery = pBatteryChar->readValue<uint8_t>();
      }
    }

    if (pClient->getRssi()) {
      JsonObject newBle = array.add<JsonObject>();
      newBle["address"] = pClient->getPeerAddress().toString();
      newBle["rssi"]  = pClient->getRssi();
      newBle["connected"] = true;
      newBle["battery"] = battery;
    }
  }

  // Add also device name if already present in MySQL DB
  for (JsonObject value : array) {
    String addr = value["address"];
    // Check if is already stored in DB
    for (DeviceLink_t &link : links) {
      if (addr.equalsIgnoreCase(link.address)) {
        value["id"] = link.id;
        value["targets"] = link.targets;
        value["name"] = link.name;
        value["db_present"] = true;
      }
    }
    // Device is connected but still not recorded in DB (id = name_addresss)
    for (ConnectedDevice_t &device : bleConnected) {
      if (addr.equalsIgnoreCase(device.address) && !value.containsKey("id")) {
        String newId = value["address"].as<String>();
        newId.replace(":", "");
        value["id"] = newId;
        value["device_name"] = device.name;
      }
    }
  }

  JsonArray devices = doc["devices"].to<JsonArray>();
  JsonArray hubs = doc["hubs"].to<JsonArray>();

  // Get list of BLE devices and HUBs from MySQL (will fill HTML selection box)
  DataQuery_t data;
  if (queryExecute(data, selectDevicesHubs, tableDevices, tableHubs)) {
    int numRow = 0;
    for (Record_t rec : data.records) {
      const char* type = data.getRowValue(numRow, "type");
      const char* name = data.getRowValue(numRow, "name");
      if (strcmp(type, "device") == 0) {
        devices.add(name);
      }
      if (strcmp(type, "hub") == 0) {
        hubs.add(name);
      }
      numRow++;
    }
  }

  // Finally add HUB name to JSON
  doc["hubName"] = hubName;
  serializeJsonPretty(doc, json);
  serializeJsonPretty(doc, Serial);
  myWebServer.send(200, "application/json", json);
}


void addAllNearbyDevices() {
  // Get all BLE peripherals still not connected
  String json;
  scanForDevices(false, &json);
  JsonDocument doc;
  deserializeJson(doc, json);

  // Add devices already connected
  JsonArray array = doc["ble_devices"].as<JsonArray>();
  for (ConnectedDevice_t &device : bleConnected) {
    JsonObject newBle = array.add<JsonObject>();
    newBle["name"] = device.name;
    newBle["address"] = device.address;
  }

  /*
    Sync MySQL database
    If device already defined, only connected_to field will be updated
    Otherwise a new record will be created with an unique auto-generated name
  */
  Serial.println("Add or update BLE device records");
  for (JsonVariant value : array) {
    // Automatic update hub field for this device
    String newId = value["address"].as<String>();
    newId.replace(":", "");
    addNewDeviceDB(value["address"].as<const char*>(),
                   hubName, newId.c_str(), value["name"].as<const char*>(), false);
  }

  if (myWebServer.client()) {
    myWebServer.send(200, "application/json",
            "Added all nearby BLE devices to MySQL table."
            "Check results with this query:<br><br>SELECT * FROM ble_devices");
  }
}


void handleSqlScript() {
  if (myWebServer.hasArg("sql")) {
    String sqlstr =  myWebServer.arg("sql");
    DataQuery_t data;
    myWebServer.broadcastWebSocket(sqlstr);
    myWebServer.broadcastWebSocket("\n");
    if (queryExecute(data, sqlstr.c_str())) {
      myWebServer.send(200, "text/plain", "SQL script executed");
      sql.printResult(data, Serial);
      sql.printResult(data, *myWebServer.getWebSocketServer());
    }
    else {
      Serial.println(sql.getLastError());
      myWebServer.send(202, "text/plain", "SQL script error");
      myWebServer.broadcastWebSocket(sql.getLastError());
    }
  }
  else
    myWebServer.send(500, "text/plain", "handler not defined");
}

void handleNewHub() {
  myWebServer.send(500, "text/plain", "handler not defined");
}




////////////////////  Load application options from filesystem  ////////////////////
bool loadOptions() {
  if (LittleFS.exists(myWebServer.getConfigFilepath())) {
    myWebServer.getOptionValue(HUB_NAME, hubName);
    myWebServer.getOptionValue(MY_SQL_HOST, dbHost);
    myWebServer.getOptionValue(MY_SQL_PORT, dbPort);
    myWebServer.getOptionValue(MY_SQL_DB, database);
    myWebServer.getOptionValue(MY_SQL_USER, user);
    myWebServer.getOptionValue(MY_SQL_PASS, password);
    myWebServer.getOptionValue(MY_SQL_POLL, pollTime);
    Serial.printf(HUB_NAME ": %s\n", hubName.c_str());
    Serial.printf(MY_SQL_HOST ": %s\n", dbHost.c_str());
    Serial.printf(MY_SQL_PORT ": %d\n", dbPort);
    Serial.printf(MY_SQL_DB ": %s\n", database.c_str());
    Serial.printf(MY_SQL_USER ": %s\n", user.c_str());
    Serial.printf(MY_SQL_PASS ": %s\n", password.c_str());
    Serial.printf(MY_SQL_POLL ": %d\n", pollTime);
    return true;
  }
  else
    Serial.println(F("File \"/config/config.json\" not exist"));
  return false;
}

////////////////////////////////  Filesystem  /////////////////////////////////////////

// Configure and start webserver
void startWebServer() {
  // FILESYSTEM INIT
  if (!LittleFS.begin()) {
    Serial.println("ERROR on mounting filesystem. It will be formmatted!");
    LittleFS.format();
    ESP.restart();
  }

  // Load configuration (if not present, default will be created when webserver will start)
  Serial.println("Load application otions:");
  if (!loadOptions())
    Serial.println("Error!! Options NOT loaded!");
  Serial.println();

  WiFi.onEvent(WiFiEvent);

  // Set IP address when in AP mode
  myWebServer.setIPaddressAP(IPAddress(192,168,4,1));
  myWebServer.setFirmwareVersion(HUB_VERSION);

  // Try to connect to stored SSID, start AP if fails after timeout of 15 seconds
  myWebServer.setAP("", "");
  serverIP = myWebServer.startWiFi(15000, true);

  // Configure /setup page and start Web Server
  myWebServer.addOptionBox("MySQL setup");
  myWebServer.addOption(HUB_NAME, hubName);
  myWebServer.addOption(MY_SQL_HOST, dbHost);
  myWebServer.addOption(MY_SQL_PORT, dbPort);
  myWebServer.addOption(MY_SQL_DB, database);
  myWebServer.addOption(MY_SQL_USER, user);
  myWebServer.addOption(MY_SQL_PASS, password);
  myWebServer.addOption(MY_SQL_POLL, pollTime);
  myWebServer.addOptionBox("BLE Device List");
  myWebServer.addHTML(listble_html, "blelist");
  myWebServer.addOptionBox("MySQL console");
  myWebServer.addHTML(mysqlconsole_html, "mysql");
  myWebServer.addJavascript(listble_script, "blelist");
  myWebServer.addJavascript(mysqlconsole_js, "mysql");

  // Enable built-in websocket server
  myWebServer.enableWebsocket(81, nullptr);


  // Since version 2.0.4, subtitle was added as builtin functionality
  // If you update, we can remove this block of code and subtitle_js array
  if (strcmp(myWebServer.getVersion(), "2.0.3") <= 0) {
    String subtitle = "Version " HUB_VERSION ", Address ";
    Serial.println(subtitle);
    subtitle += serverIP.toString();
    myWebServer.saveOptionValue("subtitle-hidden", subtitle);
    myWebServer.addOption("subtitle-hidden", subtitle);
    myWebServer.addJavascript(subtitle_js, "subtitle");
  }

  // Add endpoints request handlers
  // - insert a new device record
  myWebServer.on("/addDevice", HTTP_GET, handleNewDevice);
  // - insert a new connection record
  myWebServer.on("/addConnection", HTTP_GET, handleNewConnection);
  // - insert a new hub record
  myWebServer.on("/addHub", HTTP_GET, handleNewHub);
  // - get ble device list with service == customServiceUUID
  myWebServer.on("/listble", HTTP_GET, handleGetDeviceList);
  // - execute a SQL script passed from browser
  myWebServer.on("/executeSql", HTTP_POST, handleSqlScript);
  // - add all nearby BLE devices
  myWebServer.on("/addAllBle", HTTP_GET, addAllNearbyDevices);

  // Enable ACE FS file web editor and add FS info callback fucntion
  myWebServer.enableFsCodeEditor([](fsInfo_t* fsInfo) {
    fsInfo->totalBytes = LittleFS.totalBytes();
    fsInfo->usedBytes = LittleFS.usedBytes();
    fsInfo->fsName = "LittleFS";
  });

  // Start the webserver
  myWebServer.begin();
  Serial.print("\n\nESP Web Server started on IP Address: ");
  Serial.println(serverIP);
  Serial.println(
    "Open /setup page to configure optional parameters.\n"
    "Open /edit page to view, edit or upload example or your custom webserver source files."
  );
}