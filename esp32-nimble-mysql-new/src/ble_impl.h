#include "variables.h"


/**
* Callback class invoked when a peripheral connect or disconnect
*/
class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    Serial.println("Peripheral onConnect");
    nPeripheralConnected++;
  }
  void onDisconnect(BLEClient* pclient) {
    Serial.println("Peripheral onDisconnect");
    nPeripheralConnected--;
  }
};

/**
* Callback function invoked when a peripheral notify a characteristic
* @param pChar pointer to BLERemoteCharacteristic
* @param pData data buffer
* @param length number of bytes of data
* @param isNotify true if this is a notification
**/
static void notifyCallback(BLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {

  // Store the service which send us notification
  if (pChar->getUUID().toString() == dataReadyCharUUID.toString()) {
    newMessageFrom = pChar->getRemoteService();
  }
}

static void batteryCallback(BLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
  BLEAddress peerAddress = pChar->getRemoteService()->getClient()->getPeerAddress();
  Serial.print(peerAddress.toString().c_str());
  Serial.print(" - Battery level: ") ;
  Serial.println((uint8_t)*pData);
}

static void gpioCallback(BLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
  BLEAddress peerAddress = pChar->getRemoteService()->getClient()->getPeerAddress();
  Serial.print(peerAddress.toString().c_str());
  uint16_t value = *(pData+1) + *pData;
  Serial.printf(" - GPIOs 0b " PRINTF_BINARY_PATTERN_INT16 "\n", PRINTF_BYTE_TO_BINARY_INT16(value));
}


/**
* Callback function invoked when a peripheral notify a characteristic
* @param myDevice pointer to discovered BLE device
**/
bool connectToServer(BLEAdvertisedDevice* myDevice) {
  Serial.print("\nForming a connection to ");
  Serial.println(myDevice->getAddress().toString().c_str());

  pClient = BLEDevice::createClient();
  Serial.println(" - Created client");
  pClient->setClientCallbacks(new MyClientCallback());

  // Connect to the remote BLE Server.
  pClient->connect(myDevice);  // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
  Serial.println(" - Connected to server");

  // Obtain a reference to the service we are after in the remote BLE server.
  BLERemoteService* pRemoteService = pClient->getService(customServiceUUID);
  if (pRemoteService == nullptr) {
    Serial.print("Failed to find our service UUID: ");
    Serial.println(customServiceUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.println(" - Found service");

  // Read the value of the string buffer characteristic.
  BLERemoteCharacteristic* pRemoteCharacteristic = pRemoteService->getCharacteristic(stringCharUUID);
  // if(pRemoteCharacteristic->canRead()) {
  //   std::string value = pRemoteCharacteristic->readValue();
  //   Serial.print("The \"string buffer\" characteristic value was: ");
  //   Serial.println(value.c_str());
  // }

  // Obtain a reference to the txBuffer characteristic and subscribe
  pRemoteCharacteristic = pRemoteService->getCharacteristic(dataReadyCharUUID);
  if (pRemoteCharacteristic == nullptr) {
    Serial.print("Failed to find our \"Data ready\" characteristic UUID: ");
    Serial.println(dataReadyCharUUID.toString().c_str());
    // pClient->disconnect();
    // return false;
  }

  // Subscribe notification for this characteristic (data ready)
  if (pRemoteCharacteristic->canNotify()) {
    pRemoteCharacteristic->subscribe(true, notifyCallback, true);
    Serial.println(" - \"Data ready\" characteristic subscribed");
  }

  // Subscribe notification for digital gpio (Automation IO service)
  BLERemoteService* pGpioService = pClient->getService(BLEUUID(UUID16_SVC_AUTOMATION_IO));
  if (pGpioService != nullptr) {
    BLERemoteCharacteristic* pGpioChar = pGpioService->getCharacteristic(BLEUUID(UUID16_CHR_DIGITAL));
    if (pGpioChar != nullptr) {
      if (pGpioChar->canNotify()) {
        pGpioChar->subscribe(true, gpioCallback, true);
        Serial.println(" - \"Digital gpio\" characteristic subscribed");
      }
    }
  }
  else {
    Serial.println("Automation IO service not found");
  }

  // Subscribe notification for battery level
  BLERemoteService* pBatteryService = pClient->getService(BLEUUID(UUID16_SVC_BATTERY));
  if (pBatteryService != nullptr) {
    BLERemoteCharacteristic* pBatteryChar = pBatteryService->getCharacteristic(BLEUUID(UUID16_CHR_BATTERY_LEVEL));
    if (pBatteryChar != nullptr) {
      if (pBatteryChar->canNotify()) {
        pBatteryChar->subscribe(true, batteryCallback, true);
        Serial.println(" - \"Battery level\" characteristic subscribed");
      }
    }
  }
  else {
    Serial.println("Battery service not found");
  }

  return true;
}


/**
 * Scan for BLE servers and find the peripherals that advertises the service we are looking for.
 */
void scanForDevices(bool connect, String* json) {

  Serial.println("\nStart BLE device scan");
  BLEScanResults foundDevices = pBLEScan->start(scanTime, true);
  Serial.print("Total BLE devices found: ");
  Serial.println(foundDevices.getCount());

  JsonDocument doc;
  JsonArray array = doc["ble_devices"].to<JsonArray>();

  for (uint8_t i = 0; i < foundDevices.getCount(); i++) {
    BLEAdvertisedDevice* advertisedDevice = new BLEAdvertisedDevice(foundDevices.getDevice(i));

    // We have found a device, let us now see if it contains the service we are looking for.
    if (advertisedDevice->haveServiceUUID() && advertisedDevice->isAdvertisingService(BLEUUID(customServiceUUID))) {
      if (json != nullptr) {
        JsonObject newBle = array.add<JsonObject>();
        String address = advertisedDevice->getAddress().toString().c_str();
        newBle["address"] = address;
        address.replace(":", "");
        newBle["id"] = address;
        newBle["device_name"] = advertisedDevice->getName();
        newBle["rssi"]  = advertisedDevice->getRSSI();
        newBle["connected"] = false;

        // Get battery level (and eventually button state) from manufacturer data packet,
        customData_t data = advertisedDevice->getManufacturerData<customData_t>();
        newBle["battery"] = data.battery;
      }

      // Try to establish a connection with BLE device with customServiceUUID
      if (connect) {
        if (connectToServer(advertisedDevice)) {
          Serial.println("Connection done.");
          // Store a list of connected device to handle following requests
          ConnectedDevice_t device;
          device.address = advertisedDevice->getAddress().toString().c_str();;
          device.name = advertisedDevice->getName().c_str();
          bleConnected.push_back(device);
        }
        else
          Serial.println("Connection fails!");
      }
    }
    delete advertisedDevice;
  }
  pBLEScan->clearResults();
  Serial.println("End BLE scan");
  if (json != nullptr) {
    serializeJsonPretty(doc, *json);
  }
}
