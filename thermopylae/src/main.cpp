#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

#ifdef ESP8266
  #include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
  #define CHIP_ID   ((uint32_t)ESP.getChipId())
#else
  #include <WiFi.h>
  #include <WiFiClientSecure.h>
  #define CHIP_ID   ((uint32_t)ESP.getEfuseMac())
#endif

WiFiClient wifi_client;

#define LED_ON      HIGH
#define LED_OFF     LOW

//needed for library
#include <DNSServer.h>
#include <ArduinoOTA.h>
#include <Wire.h>
#include <Arduino.h>
#include <SPI.h>
#include <SSD1306.h>
//#include "soc/efuse_reg.h"
#include "mqtt.h"
PubSubClient mqtt_client(wifi_client);

#define LEDPIN 2

#define OLED_I2C_ADDR 0x3C
#define OLED_RESET 16
#define OLED_SDA 4
#define OLED_SCL 15
#define BAT_PIN 37
#define RELAY_PIN 12

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

unsigned int counter = 0;

SSD1306 display (OLED_I2C_ADDR, OLED_SDA, OLED_SCL);

#define LOGO_HEIGHT   16
#define LOGO_WIDTH    16

const String MODULE = "thermopylae";
const String TOPIC_PREFIX = MODULE+"/";
const String PUB_TOPIC_SUFFIX = "/state";
const String SUB_TOPIC_SUFFIX = "/control";

const int DEEP_SLEEP_MINUTES = 10; // CHANGE ME TO 20
const int DEEP_SLEEP_SECONDS = 60 * DEEP_SLEEP_MINUTES;

String get_device_id() {
  uint64_t mac_integer = CHIP_ID;
  uint8_t *mac = (uint8_t*) &mac_integer;
  char mac_chars[13];
  for (int i=0; i<6; i++) {
    sprintf(mac_chars + i * 2, "%" PRIx8, mac[i]);
  }
  return mac_chars;
}

String device_id;
String client_id;
String pub_topic;
String sub_topic;

void callback(char* topic, byte* payload, unsigned int length) {
 
  Serial.print("Message arrived in topic: ");
  Serial.println(topic);
 
  Serial.print("Message:");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
 
  Serial.println();
  Serial.println("-----------------------");

  StaticJsonDocument<256> doc;
  deserializeJson(doc, payload, length);
  JsonObject object = doc.as<JsonObject>();
  String new_state = object["state"];
  Serial.println(new_state);
  
}

boolean reconnect_mqtt() {
  if (mqtt_client.connected()) {
    Serial.println(F("Still connected to MQTT broker"));
  }
  Serial.println(F("Reconnecting to MQTT broker"));
  mqtt_client.connect(client_id.c_str());
  if (mqtt_client.connected()) {
    Serial.println(F("Connected to MQTT broker"));
    mqtt_client.subscribe(sub_topic.c_str());
    return true;
  } else {
    Serial.println(F("Failed to connect to MQTT broker"));
    Serial.print(mqtt_client.state());
    Serial.println(" try again in 5 seconds");
    // Wait 5 seconds before retrying
    delay(5000);
    return false;
  }
}

const char* process_ota_error(ota_error_t error) {
    if (error == OTA_AUTH_ERROR) return "Auth Failed";
    else if (error == OTA_BEGIN_ERROR) return "Begin Failed";
    else if (error == OTA_CONNECT_ERROR) return "Connect Failed";
    else if (error == OTA_RECEIVE_ERROR) return "Receive Failed";
    else if (error == OTA_END_ERROR) return "End Failed";
    else return "Even Failing Failed! Err = "+ int(error);
}

unsigned long previousTime = millis();

const unsigned long interval = 10000;

bool relay_state = false;
unsigned int battery_raw = 0;

void print_info() {
  Serial.println(F("------------------------------------------------------------"));
  Serial.print(F("Device id:      "));
  Serial.println(device_id);
  Serial.print(F("Mqtt broker:    "));
  Serial.print(MQTT_BROKER_HOST);
  Serial.print(F(":"));
  Serial.println(MQTT_BROKER_PORT);
  Serial.print(F("Mqtt topic:     "));
  Serial.println(pub_topic);
  Serial.print(F("Mqtt control topic:     "));
  Serial.println(sub_topic);
  Serial.print(F("Mqtt client id: "));
  Serial.println(client_id);
  Serial.println(F("------------------------------------------------------------"));
}

void display_message(String message){
    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
    display.drawString(display.getWidth()/2, display.getHeight()/2, message);
    display.display();
}

void setup_battery_monitor(){
#ifdef ESP8266
  pinMode(BAT_PIN, INPUT);
#else
  adcAttachPin(BAT_PIN);
  analogSetClockDiv(255);
#endif
}

void setup(){
    // reset the OLED
    pinMode(OLED_RESET,OUTPUT);
    digitalWrite(OLED_RESET, LOW);
    delay(50);
    digitalWrite(OLED_RESET, HIGH);
  
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);

    setup_battery_monitor();

    display.init();
    display.flipScreenVertically();
    display.setContrast(255);
    display.setFont(ArialMT_Plain_10);
    display.println("Hey there!");
    display.display();
    
    Serial.begin(115200);
    delay(10);
    Serial.println('\n');

    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.println("Connecting ...");
    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawString(0,0,"Connecting ...");
    display.display();

  int i = 0;
  while (WiFi.status() != WL_CONNECTED) { // Wait for the Wi-Fi to connect
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
    display_message(process_ota_error(error));
  });

  ArduinoOTA.onStart([]() {
    display_message("OTA UPDATE");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawString(0, 0, "Updating");
    display.drawProgressBar(4, 32, 120, 8, progress / (total / 100) );
    display.drawString(0, 8, String(progress / (total / 100)) + "%");
    display.drawString(0, 16, '('+String(progress)+')');
    display.display();
  });

  ArduinoOTA.onEnd([]() {
    display_message("Restart");
  });

  ArduinoOTA.begin();
  device_id = get_device_id();
  client_id = "esp32_" + device_id;
  pub_topic = TOPIC_PREFIX + device_id + PUB_TOPIC_SUFFIX;
  sub_topic = TOPIC_PREFIX + device_id + SUB_TOPIC_SUFFIX;
  print_info();
  mqtt_client.setServer(MQTT_BROKER_HOST, String(MQTT_BROKER_PORT).toInt());
  mqtt_client.setCallback(callback);
  mqtt_client.subscribe(sub_topic.c_str());
}

float get_battery_value(){
  float val = battery_raw;
  val = val * (3.3/4095) * ((220+110)/110);
  return val;
}

String stringify_time(unsigned int time_ms){
  int seconds = time_ms/1000;
  int minutes = seconds / 60;
  int hours = minutes / 60;
  int days = hours / 24;
  return String(days)+':'
        +String(hours%24)+':'
        +String(minutes%60)+':'
        +String(seconds%60);
}

void draw_default_screen(){
  display.clear();
  // Align text vertical/horizontal center
  display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
  display.setFont(ArialMT_Plain_10);
  display.drawString(display.getWidth()/2, display.getHeight()/2, "OTA:" + WiFi.SSID() + '\n' 
                      + WiFi.localIP().toString() + ':' + get_device_id() + '\n'
                      + stringify_time(millis()) + "s, B:" + String(get_battery_value())
                    );
  display.drawProgressBar(0,0,128,8,map(battery_raw, 1700, 2400, 0, 100));
  display.display();
  
}

void read_and_send_data() {
  DynamicJsonDocument root(1024);
  
  root["device_id"] = device_id;
  root["state"] = (int) relay_state;
  root["battery"] = get_battery_value();
  root["battery_raw"] = battery_raw;
  root["uptime"] = previousTime;

  Serial.print(F("Sending payload: "));
  serializeJson(root, Serial);

  if(reconnect_mqtt()) {
    String payload;
    serializeJson(root, payload);
    if (mqtt_client.beginPublish(pub_topic.c_str(), payload.length(), true)) {
      mqtt_client.print(payload);
      mqtt_client.endPublish();
      Serial.println("Success sending message");
    } else {
      Serial.println("Error sending message");
    }
  } else {
    Serial.println("Failed to reconnect_mqtt");
  }
}

void update_state(){
  previousTime = millis();
  digitalWrite(RELAY_PIN, relay_state);
  battery_raw = analogRead(BAT_PIN);
}

void loop() {
  ArduinoOTA.handle();
  unsigned long diff = millis() - previousTime;
  if(diff > interval) {
    previousTime = millis();
    Serial.println(previousTime);
    update_state();
    draw_default_screen();
    read_and_send_data();
  }
  if (millis()>(60*1000)){
    ESP.deepSleep(DEEP_SLEEP_SECONDS * 1000000);
  }
}