#include "WiFi.h"
#include "esp_wps.h"

#include "variables.h"

/*
Change the definition of the WPS mode
from WPS_TYPE_PBC to WPS_TYPE_PIN in
the case that you are using pin type
WPS
*/
#define ESP_WPS_MODE      WPS_TYPE_PBC
#define ESP_MANUFACTURER  "ESPRESSIF"
#define ESP_MODEL_NUMBER  "ESP32"
#define ESP_MODEL_NAME    "ESPRESSIF IOT"
#define ESP_DEVICE_NAME   "ESP STATION"

static esp_wps_config_t config;

void wpsInitConfig(){
  config.wps_type = ESP_WPS_MODE;
  strcpy(config.factory_info.manufacturer, ESP_MANUFACTURER);
  strcpy(config.factory_info.model_number, ESP_MODEL_NUMBER);
  strcpy(config.factory_info.model_name, ESP_MODEL_NAME);
  strcpy(config.factory_info.device_name, ESP_DEVICE_NAME);
}

void wpsStart(){
  if(esp_wifi_wps_enable(&config)){
    Serial.println("WPS Enable Failed");
  } else if(esp_wifi_wps_start(0)){
    Serial.println("WPS Start Failed");
  }
  wpsEnabled = true;
}

void wpsStop(){
  if(esp_wifi_wps_disable()){
    Serial.println("WPS Disable Failed");
  }
  wpsEnabled = false;
}

String wpspin2string(uint8_t a[]){
  char wps_pin[9];
  for(int i=0;i<8;i++){
    wps_pin[i] = a[i];
  }
  wps_pin[8] = '\0';
  return (String)wps_pin;
}


// WARNING: WiFiEvent is called from a separate FreeRTOS task (thread)!
void WiFiEvent(WiFiEvent_t event, arduino_event_info_t info){
  switch(event){
    case ARDUINO_EVENT_WIFI_STA_START:
      Serial.println("\nStation Mode Started");
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      // WiFi.mode(WIFI_MODE_STA);
      Serial.println("\nConnected to :" + String(WiFi.SSID()));
      Serial.print("Got IP: ");
      Serial.println(WiFi.localIP());
      wifiConnected = true;
      wpsEnabled = false;
      digitalWrite(ledOnboard, HIGH);
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      //   Serial.println("Disconnected from station, attempting reconnection");
      WiFi.reconnect();
      delay(250);
      wifiConnected = false;
      wpsEnabled = false;
      digitalWrite(ledOnboard, LOW);
      break;
    case ARDUINO_EVENT_WPS_ER_SUCCESS:
      Serial.println("WPS Successfull, stopping WPS and connecting to: " + String(WiFi.SSID()));
      wpsStop();
      delay(10);
      WiFi.begin();
      break;
    case ARDUINO_EVENT_WPS_ER_FAILED:
      Serial.println("WPS Failed, retrying");
      wpsStop();
      wpsStart();
      break;
    case ARDUINO_EVENT_WPS_ER_TIMEOUT:
      Serial.println("WPS Timedout, retrying");
      WiFi.disconnect(false, true);
      wpsStop();
      // wpsStart();
      break;
    case ARDUINO_EVENT_WPS_ER_PIN:
      Serial.println("WPS_PIN = " + wpspin2string(info.wps_er_pin.pin_code));
      break;
    default:
      break;
  }
}

void blinkConnectionLed() {
  static uint32_t blinkTime;
  static bool blink;
  if (millis() - blinkTime > 150) {
    blinkTime = millis();
    blink = !blink;
    digitalWrite(ledOnboard, blink);
  }
}