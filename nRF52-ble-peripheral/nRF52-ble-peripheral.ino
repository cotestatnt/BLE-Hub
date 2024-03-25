#include <Adafruit_TinyUSB.h>
#include <bluefruit.h>


#define BAT_CHARGE_STATE  23
#define R1 940.0F                   // Calibrate the value of R1/R2 to compensate resistor tolerance
#define R2 510.0F
#define VBAT_RATIO (R1 + R2) / R2   // Decrease ratio if Vbat measured with multimeter > Vbat reported
#define VBAT_SCALING 3.6 / 1024     // 3.6 reference and 10 bit resolution

#define MIN_BAT_VOLTAGE 3.0F
#define MAX_BAT_VOLTAGE 4.2F
#define BATTERY_FULL_SCALE (MAX_BAT_VOLTAGE - MIN_BAT_VOLTAGE)

#define MAX_LEN 250
char strBuffer[MAX_LEN];

struct ATTR_PACKED {
  uint8_t   battery = 100;
  uint16_t  gpios  = 0;
} customData;


enum BatteryState {BATTERY, CHARGING, USB_POWERED};
uint8_t batteryState = BatteryState::BATTERY;

const uint8_t       buttonPin0 = 0;
const uint8_t       buttonPin1 = 1;
const uint8_t       buttonPin2 = 2;


// 555a0002-val-467a-9538-01f0652c74e8"
#define CUSTOM_UUID(val) (const uint8_t[]) { 0xe8, 0x74, 0x2c, 0x65, 0xf0, 0x01, 0x38, 0x95, 0x7a, 0x46, \
                                            (uint8_t)(val & 0xff), (uint8_t)(val >> 8), 0x02, 0x00, 0x5a, 0x55 }

// Custom BLE service
BLEService service(CUSTOM_UUID(0x0000));

/*
  For characteristics that use 'notify' or 'indicate', the max data length is 20 bytes.
  We will use this flag to notify the central about new data ready to be read
  https://learn.adafruit.com/bluefruit-nrf52-feather-learning-guide/blecharacteristic
*/
BLECharacteristic dataReadyCharacteristic(CUSTOM_UUID(0x0001));

// Text data buffered characteristic (can be up to max 512 bytes)
BLECharacteristic stringCharacteristic(CUSTOM_UUID(0x0002));

// Builtin BLE Battery service
BLEBas  blebas;  // battery

// Automation IO service
#define UUID16_CHR_DIGITAL            0x2A56
BLEService gpioService(UUID16_SVC_AUTOMATION_IO);
BLECharacteristic gpioCharacteristic(UUID16_CHR_DIGITAL);


/**
 * Callback invoked when central connects
 * @param conn_handle connection where this event happens
 */
void connect_callback(uint16_t conn_handle) {
  // Get the reference to current connection
  BLEConnection* connection = Bluefruit.Connection(conn_handle);

  char central_name[32] = { 0 };
  connection->getPeerName(central_name, sizeof(central_name));

  Serial.print("Connected to central: ");
  Serial.println(central_name);
}

/**
 * Callback invoked when a connection is dropped
 * @param conn_handle connection where this event happens
 * @param reason is a BLE_HCI_STATUS_CODE which can be found in ble_hci.h
 */
void disconnect_callback(uint16_t conn_handle, uint8_t reason) {
  (void)conn_handle;
  (void)reason;

  Serial.println();
  Serial.print("Disconnected, reason = 0x");
  Serial.println(reason, HEX);
}

/**
 * Callback invoked when a central write the characteristic
 * @param conn_handle connection where this event happens
 * @param data pointer to data
 * @param len total byte length of data
 */
void str_write_callback(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len) {
  memcpy(strBuffer, data, len);                         // Copy data to strBuffer
  strBuffer[len] = '\0';                                // Set string terminator

  Serial.print("Remote message: ");    // Do something with data
  Serial.println(strBuffer);
}

/* Start Advertising
  * - Enable auto advertising if disconnected
  * - Interval:  fast mode = 20 ms, slow mode = 152.5 ms
  * - Timeout for fast mode is 30 seconds
  * - Start(timeout) with timeout = 0 will advertise forever (until connected)
  *
  * For recommended advertising interval
  * https://developer.apple.com/library/content/qa/qa1931/_index.html
  */
void startAdv(void) {
  // Advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  // Bluefruit.Advertising.addTxPower();

  // Include custom service 128-bit uuid
  Bluefruit.Advertising.addService(service);
  // Include battery service
  Bluefruit.Advertising.addService(blebas);
  // Add customer data to advertising packer, so we don't need to connect to obatin battery level and button state
  Bluefruit.Advertising.addData(BLE_GAP_AD_TYPE_MANUFACTURER_SPECIFIC_DATA, &customData, sizeof(customData));

  // Secondary Scan Response packet (optional)
  // Since there is no room for 'Name' in Advertising packet
  Bluefruit.ScanResponse.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);  // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30);    // number of seconds in fast mode
  Bluefruit.Advertising.start(0);              // 0 = Don't stop advertising after n seconds
}

float getBatteryStats() {
  uint32_t USB_Status = NRF_POWER->USBREGSTATUS;
  digitalWrite(VBAT_ENABLE, LOW);
  float batVoltage = analogRead(PIN_VBAT) * VBAT_SCALING * VBAT_RATIO;
  customData.battery = constrain((batVoltage - MIN_BAT_VOLTAGE) / BATTERY_FULL_SCALE * 100, 0, 100);
  // digitalWrite(VBAT_ENABLE, HIGH);
  if (digitalRead(BAT_CHARGE_STATE) == LOW) {
    batteryState = BatteryState::CHARGING;
  }
  else if (USB_Status) {
    batteryState = BatteryState::USB_POWERED;
  }
  else {
    batteryState = BatteryState::BATTERY;
  }

  return batVoltage;
}


void setup() {
  Serial.begin(115200);
  // while (!Serial) ;


  // Set GPIO low to enable battery voltage read
  pinMode(VBAT_ENABLE, OUTPUT);
  // Input configured as pullup -> active signal == LOW
  pinMode(buttonPin0, INPUT_PULLUP);
  pinMode(buttonPin1, INPUT_PULLUP);
  pinMode(buttonPin2, INPUT_PULLUP);
  pinMode(BAT_CHARGE_STATE, INPUT_PULLUP);

  Serial.println("BLE nRF52x test peripheral");
  Serial.println("---------------------------------\n");

  // Config the peripheral connection with maximum bandwidth
  // more SRAM required by SoftDevice
  // Note: All config***() function must be called before begin()

  Bluefruit.setName("XIAO-nRF52");
  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);
  Bluefruit.configUuid128Count(15);

  Bluefruit.begin();
  Bluefruit.setTxPower(4);  // Check bluefruit.h for supported values
  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);

  ble_gap_addr_t addr = Bluefruit.getAddr() ;
  Serial.printf(
    "MAC address: %02x:%02x:%02x:%02x:%02x:%02x\n",
    addr.addr[5], addr.addr[4], addr.addr[3],
    addr.addr[2], addr.addr[1], addr.addr[0]
  );

  // Configure and Start BLE custom Service
  service.begin();

  // Put some test data into strBuffer
  strcpy(strBuffer, "Hello world");

  // Setup stringCharacteristic
  stringCharacteristic.setProperties(CHR_PROPS_READ | CHR_PROPS_WRITE);
  stringCharacteristic.setPermission(SECMODE_OPEN, SECMODE_OPEN);
  stringCharacteristic.setFixedLen(MAX_LEN);
  stringCharacteristic.begin();
  stringCharacteristic.write(strBuffer, sizeof(strBuffer));
  stringCharacteristic.setWriteCallback(str_write_callback);

  // Setup dataReadyCharacteristic
  dataReadyCharacteristic.setProperties(CHR_PROPS_NOTIFY);
  dataReadyCharacteristic.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  dataReadyCharacteristic.setFixedLen(2);
  dataReadyCharacteristic.write16(strlen(strBuffer));
  dataReadyCharacteristic.begin();

  // Start digital gpio service
  gpioService.begin();

  // Configure digital gpio chracteristic
  gpioCharacteristic.setProperties(CHR_PROPS_NOTIFY | CHR_PROPS_READ);
  gpioCharacteristic.setPermission(SECMODE_OPEN, SECMODE_OPEN);
  gpioCharacteristic.setFixedLen(sizeof(uint16_t));
  gpioCharacteristic.write16(customData.gpios);
  gpioCharacteristic.begin();

  // Start BLE Battery Service
  getBatteryStats();
  blebas.begin();

  switch (batteryState) {
    case BatteryState::CHARGING:
      blebas.write(customData.battery + 100);
      break;
    case BatteryState::BATTERY:
      blebas.write(customData.battery);
      break;
    case BatteryState::USB_POWERED:
      blebas.write(0xFF);
      break;
  }


  // Set up and start advertising
  startAdv();
}

void loop() {

  // Check BLE connection status and restart advertising if needed
  if (!Bluefruit.connected() && !Bluefruit.Advertising.isRunning()) {
    Serial.println("Restart advertising");
    Bluefruit.Advertising.start(0);
    delay(100);
  }


  // Update all gpios state (from D0 to D10)
  static uint16_t oldGpios = 0;
  for (int i=0; i<=10; i++) {
    digitalRead(i) ? bitSet(customData.gpios , i) : bitClear(customData.gpios , i);
  }

  // If some of gpios has changed notify connected BLE central
  static uint32_t debounceTime = 0;
  if ((customData.gpios != oldGpios) && (millis() - debounceTime > 100)) {
    debounceTime = millis();
    oldGpios = customData.gpios;
    gpioCharacteristic.notify16(customData.gpios);
  }

  // Check battery level each 30 seconds
  static uint32_t batteryUpdateTime;
  if (millis() - batteryUpdateTime >= 10000) {
    batteryUpdateTime = millis();
    float vBat = getBatteryStats();
    Serial.printf("Battery voltage = %.3fV ", vBat);
    Serial.printf(", level: %d%%", customData.battery);
    Serial.printf(", state: %s\n", batteryState == BatteryState::CHARGING ? "charging" : "USB");

    // Notify BLE if connected
    if (Bluefruit.connected()){
      switch (batteryState) {
        case BatteryState::CHARGING:
          blebas.notify(customData.battery + 100);
          break;
        case BatteryState::BATTERY:
          blebas.notify(customData.battery);
          break;
        case BatteryState::USB_POWERED:
          blebas.notify(0xFF);
          break;
      }
    }
  }


  // Blink red led when central connected
  static uint32_t blinkTime;
  if (Bluefruit.connected() && (millis() - blinkTime >= 1000)) {
    blinkTime = millis();
    digitalToggle(LED_RED);
  }

  // Forward data from HW Serial to BLEUART
  if (Serial.available()) {
    String str = Serial.readStringUntil('\n');
    memset(strBuffer, '\0', MAX_LEN);
    sprintf(strBuffer, "(%d) %s", millis(), str.c_str());
    stringCharacteristic.write(strBuffer, strlen(strBuffer));     // Update stringCharacteristic value
    if (dataReadyCharacteristic.notifyEnabled()) {                // Notify connected client
      dataReadyCharacteristic.notify16(strlen(strBuffer));
      Serial.print("Data ready: ");
      Serial.println(strBuffer);
    }
  }
}

