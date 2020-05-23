#include <Arduino.h>
//#include <ArduinoJson.h>
//#include <PubSubClient.h>

// #ifdef ESP32
//   #include <esp_wifi.h>
//   #include <WiFi.h>
//   #include <WifiMulti.h>
//   #include <WiFiClient.h>
//   WiFiMulti wifiMulti;     // Create an instance of the ESP8266WiFiMulti class, called 'wifiMulti'

//   #define CHIP_ID   ((uint32_t)ESP.getEfuseMac())
//   #define LED_ON      HIGH
//   #define LED_OFF     LOW
//   #define BOARD ESP32
// #else

// assume it's a heltec wifi kit 8
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <ESP8266WiFiMulti.h>
//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>

// Include the correct display library

// For a connection via I2C using the Arduino Wire include:
#include <Wire.h>               // Only needed for Arduino 1.6.5 and earlier
#include "SSD1306Wire.h"        // legacy: #include "SSD1306.h"
// OR #include "SH1106Wire.h"   // legacy: #include "SH1106.h"


ESP8266WiFiMulti wifiMulti;     // Create an instance of the ESP8266WiFiMulti class, called 'wifiMulti'
SSD1306Wire display(0x3c, SDA, SCL, GEOMETRY_128_32);  // ADDRESS, SDA, SCL, OLEDDISPLAY_GEOMETRY  -  Extra param required for 128x32 displays.

const int s_width = 128;
int values[s_width];

#define BOARD ESP8266

#define CHIP_ID   ((uint32_t)ESP.getChipId())

#define LED_ON      LOW
#define LED_OFF     HIGH

#include <ArduinoOTA.h>

String get_device_id() {
  uint64_t mac_integer = CHIP_ID;
  uint8_t *mac = (uint8_t*) &mac_integer;
  char mac_chars[13];
  for (int i=0; i<6; i++) {
    sprintf(mac_chars + i * 2, "%" PRIx8, mac[i]);
  }
  return mac_chars;
}

const char* process_ota_error(ota_error_t error) {
    if (error == OTA_AUTH_ERROR) return "Auth Failed";
    else if (error == OTA_BEGIN_ERROR) return "Begin Failed";
    else if (error == OTA_CONNECT_ERROR) return "Connect Failed";
    else if (error == OTA_RECEIVE_ERROR) return "Receive Failed";
    else if (error == OTA_END_ERROR) return "End Failed";
    else return "Even Failing Failed! Err = "+ int(error);
}

void setup(){
      // Initialising the UI will init the display too.
    display.init();
    display.flipScreenVertically();
    display.setContrast(255);
    display.setFont(ArialMT_Plain_10);
    display.println("Hey there!");
    display.display();
    
    Serial.begin(115200);
    delay(10);
    Serial.println('\n');

    wifiMulti.addAP(WIFI_SSID, WIFI_PASS);
    Serial.println("Connecting ...");
    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawString(0,0,"Connecting ...");
    display.display();
  int i = 0;
  while (wifiMulti.run() != WL_CONNECTED) { // Wait for the Wi-Fi to connect
    delay(250);
    Serial.print('.');
    display.drawString(i*8,8,".");
    display.display();
    i++;
  }
  Serial.println('\n');
  Serial.print("Connected to ");
  Serial.println(WiFi.SSID());              // Tell us what network we're connected to
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());           // Send the IP address of the ESP8266 to the computer
  Serial.print("Device Id:\t");
  Serial.println(get_device_id());
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0,0,"Connected to: " + WiFi.SSID() + '\n' + WiFi.localIP().toString() + ':' + get_device_id());
  display.display();

  ArduinoOTA.setHostname(get_device_id().c_str());
  ArduinoOTA.setPassword(get_device_id().c_str());

  ArduinoOTA.onError([](ota_error_t error) {
    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
    display.drawString(display.getWidth()/2, display.getHeight()/2 - 10, process_ota_error(error));
    display.display();
  });

  ArduinoOTA.onStart([]() {
    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
    display.drawString(display.getWidth()/2, display.getHeight()/2 - 10, "OTA Update");
    display.display();
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    display.clear();
    display.drawString(0, 4, "Updating");
    display.drawProgressBar(4, 32, 120, 8, progress / (total / 100) );
    display.drawString(32, 4, String(progress / (total / 100)) + "%");
    display.drawString(64, 4, '('+String(progress)+')');
    display.display();
  });

  ArduinoOTA.onEnd([]() {
    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
    display.drawString(display.getWidth()/2, display.getHeight()/2, "Restart");
    display.display();
  });

  ArduinoOTA.begin();
  display.clear();
  // Align text vertical/horizontal center
  display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
  display.setFont(ArialMT_Plain_10);
  display.drawString(display.getWidth()/2, display.getHeight()/2, "Ready for OTA:" + WiFi.SSID() + '\n' + WiFi.localIP().toString() + ':' + get_device_id());
  display.display();
  
}

unsigned long previousTime = millis();

const unsigned long interval = 1000;

void loop() {
  ArduinoOTA.handle();
  unsigned long diff = millis() - previousTime;
  if(diff > interval) {
    previousTime += diff;
    Serial.println(millis());
  }
}