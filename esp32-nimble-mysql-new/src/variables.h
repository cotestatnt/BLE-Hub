#ifndef VARIABLES_H
#define VARIABLES_H

#include <Arduino.h>
#include <WiFi.h>
#include <FS.h>
#include <LittleFS.h>

// C/C++ std::containers
#include <string>
#include <vector>
#include <set>
#include <ArduinoJson.h>
#include <NimBLEDevice.h>      //  https://github.com/h2zero/NimBLE-Arduino
#include <MySQL.h>             //  https://github.com/cotestatnt/Arduino-MySQL
#include <esp-fs-webserver.h>  //  https://github.com/cotestatnt/esp-fs-webserver


bool wifiConnected = false;
bool wpsEnabled = false;

// Function prototypes (needed because it's used also inside other included files)
bool connectToDatabase();
int parseQueryResult(DataQuery_t&, uint8_t);
bool queryExecute(DataQuery_t&, const char*, ...);
String escape(const char* src, const std::set<char>);
bool fieldDataValidation (const char*, const char*);
void addNewDeviceDB(const String&,  const String&, const String&, const String&, bool);
void scanForDevices(bool, String*);
bool queryExecute(DataQuery_t&, const char*, ...);

IPAddress serverIP;

// BLE custom manufacturer data struct
struct customData_t {
  uint8_t   battery = 255;   // Default
  uint16_t  button  = 0;
};


// Simple struct to store a device link configuration
struct DeviceLink_t {
  String address;
  String id;
  String targets;
  String name;
};


// Simple struct to store BLE device connected
struct ConnectedDevice_t {
  String address;
  String name;
};

// Keep in memory a list of active links (loaded from DB records)
std::vector<DeviceLink_t> links;

// Keep in memory a list of connected devices
std::vector<ConnectedDevice_t> bleConnected;

// Store a vector with properties for each field in order to validate data
std::vector<Field_t> myFields;

// The char array used as buffer for text exchange between ESP32 and peripherals
#define MAX_LEN 250
String dataFromSQL;
String dataFromPeripheral;

bool MySQLdone = false;

const char* tableHubs = "hubs";
const char* tableDevices = "ble_devices";
const char* tableConnections = "connections";


// This set of variables can be updated using webpage http://<esp-ip-address>/setup
String user = "cotestatnt";                    // MySQL user login username
String password = "Tnt33cts";                  // MySQL user login password
String dbHost = "192.168.1.10";                // MySQL hostname/URL
String database = "sql11661351";               // Database name
uint16_t dbPort = 3306;                        // MySQL host port
uint32_t pollTime = 5000;                      // Waiting time between one request and the next
String hubName = "ROME01";

// Var labels (in /setup webpage)
#define MY_SQL_HOST "MySQL Hostname"
#define MY_SQL_PORT "MySQL Port"
#define MY_SQL_DB "MySQL Database name"
#define MY_SQL_USER "MySQL Username"
#define MY_SQL_PASS "MySQL Password"
#define MY_SQL_POLL "MySQL polling time"
#define HUB_NAME "ESP32 HUB Name"

// FreeRTOS handlers
TaskHandle_t mysqlHandler;
FSWebServer myWebServer(LittleFS, 80, "esp32blehub");
WiFiClient client;                  // WiFi client passed to MySQL connector
MySQL sql(&client, dbHost.c_str(), dbPort);


// Pointer to device which sent a message
BLERemoteService* newMessageFrom = nullptr;

// Duration of scan in seconds
#define scanTime 3
static BLEScan* pBLEScan;
static BLEClient* pClient;

const uint16_t UUID16_SVC_BATTERY = 0x180F;
const uint16_t UUID16_CHR_BATTERY_LEVEL = 0x2A19;

const uint16_t UUID16_SVC_AUTOMATION_IO = 0x1815;
const uint16_t UUID16_CHR_DIGITAL = 0x2A56;


// The remote service we wish to connect to.
static BLEUUID customServiceUUID    ("555a0002-0000-467a-9538-01f0652c74e8");

// The characteristic of the remote service we are interested in.
static BLEUUID stringCharUUID       ("555a0002-0002-467a-9538-01f0652c74e8");
/*
  For characteristics that use 'notify' or 'indicate', the max data length is 20 bytes.
  We will subscribe this "int" characteristic to know when peripheral has new text ready.
  https://learn.adafruit.com/bluefruit-nrf52-feather-learning-guide/blecharacteristic
*/
static BLEUUID dataReadyCharUUID    ("555a0002-0001-467a-9538-01f0652c74e8");

int8_t nPeripheralConnected = 0;
int8_t nPeripheralToBeConnected = 0;


/* --- PRINTF_BYTE_TO_BINARY macro's --- */
#define PRINTF_BINARY_SEPARATOR  " "
#define PRINTF_BINARY_PATTERN_INT8 "%c%c%c%c%c%c%c%c"
#define PRINTF_BYTE_TO_BINARY_INT8(i)    \
    (((i) & 0x80ll) ? '1' : '0'), \
    (((i) & 0x40ll) ? '1' : '0'), \
    (((i) & 0x20ll) ? '1' : '0'), \
    (((i) & 0x10ll) ? '1' : '0'), \
    (((i) & 0x08ll) ? '1' : '0'), \
    (((i) & 0x04ll) ? '1' : '0'), \
    (((i) & 0x02ll) ? '1' : '0'), \
    (((i) & 0x01ll) ? '1' : '0')

#define PRINTF_BINARY_PATTERN_INT16 \
    PRINTF_BINARY_PATTERN_INT8               PRINTF_BINARY_SEPARATOR              PRINTF_BINARY_PATTERN_INT8
#define PRINTF_BYTE_TO_BINARY_INT16(i) \
    PRINTF_BYTE_TO_BINARY_INT8((i) >> 8),   PRINTF_BYTE_TO_BINARY_INT8(i)
#define PRINTF_BINARY_PATTERN_INT32 \
    PRINTF_BINARY_PATTERN_INT16              PRINTF_BINARY_SEPARATOR              PRINTF_BINARY_PATTERN_INT16
#define PRINTF_BYTE_TO_BINARY_INT32(i) \
    PRINTF_BYTE_TO_BINARY_INT16((i) >> 16), PRINTF_BYTE_TO_BINARY_INT16(i)
#define PRINTF_BINARY_PATTERN_INT64    \
    PRINTF_BINARY_PATTERN_INT32              PRINTF_BINARY_SEPARATOR              PRINTF_BINARY_PATTERN_INT32
#define PRINTF_BYTE_TO_BINARY_INT64(i) \
    PRINTF_BYTE_TO_BINARY_INT32((i) >> 32), PRINTF_BYTE_TO_BINARY_INT32(i)
/* --- end macros --- */


#endif