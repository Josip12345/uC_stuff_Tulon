/* 
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/esp32-web-server-websocket-sliders/
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.
  
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*/

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "SPIFFS.h"
#include <Arduino_JSON.h>

// Replace with your network credentials
const char* ssid = "LockTrack";
const char* password = "";

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
// Create a WebSocket object

AsyncWebSocket ws("/ws");

// ESP ADC channels used for monitoring laser transmission signals
const int ADC1_CH2 = 8; // PD laser 1
const int ADC1_CH4 = 10; // PD laser 2

// Relay bank 1 set pins
const int relay11 = 19; //GPIO15
const int relay12 = 21; //GPIO14
const int relay13 = 33; //GPIO29
const int relay14 = 34; //GPIO30
// Relay bank 2 set pins
const int relay21 = 35; //GPIO31
const int relay22 = 36; //GPIO32
const int relay23 = 37; //GPIO33
const int relay24 = 38; //GPIO34

// Variable for storing laser transmission values obtained by ESP ADCs
int Value_laser1 = 0;
int Value_laser2 = 0;
unsigned long timer_off_lock_laser1 = 0;
unsigned long timer_off_lock_laser2 = 0;


// ADC conversion factors
int Volt_to_number_8 = 3100.0;
int Volt_to_number_10 = 3100.0;

// Initial threshold values
int threshold_engage1 = Volt_to_number_8*1.0; // Integrators engaged above PD value of 1 V
int threshold_disengage1 = Volt_to_number_8*0.8; // Integrators disengaged below a PD value of 0.8 V
                                               // Like this we have needed hysteresis


int threshold_engage2 = Volt_to_number_10*1.5; // Integrators engaged above PD value of 1.5 V
int threshold_disengage2 = Volt_to_number_10*1.2; // Integrators disengaged below a PD value of 1.2 V
                                               // Like this we have needed hysteres 

                                              

String message = "";
String sliderValue1 = "0";
String sliderValue2 = "0";
String sliderValue3 = "0";
String sliderValue4 = "0";
String switchString = "0";
int switchStatus = 0;



//Json Variable to Hold Slider Values
JSONVar sliderValues;

//Get Slider Values
String getSliderValues(){
  sliderValues["sliderValue1"] = String(sliderValue1);
  sliderValues["sliderValue2"] = String(sliderValue2);
  sliderValues["sliderValue3"] = String(sliderValue3);
  sliderValues["sliderValue4"] = String(sliderValue4);

  String jsonString = JSON.stringify(sliderValues);
  return jsonString;
}

// Initialize SPIFFS
void initFS() {
  if (!SPIFFS.begin()) {
    Serial.println("An error has occurred while mounting SPIFFS");
  }
  else{
   Serial.println("SPIFFS mounted successfully");
  }
}


IPAddress local_IP(192,168,4,7);
IPAddress gateway(192,168,4,3);
IPAddress subnet(255,255,255,0);

// Initialize WiFi
void initWiFi() {
  delay(1000);
  Serial.begin(115200);
  Serial.println();
  
  Serial.print("Setting soft-AP configuration ... ");
  Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "Ready" : "Failed!");
  
  Serial.print("Setting soft-AP ... ");
  Serial.println(WiFi.softAP(ssid, password) ? "Ready" : "Failed!");
  
  Serial.print("Soft-AP IP address = ");
  Serial.println(WiFi.softAPIP());
  server.begin();
}

void notifyClients(String sliderValues) {
  ws.textAll(sliderValues);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    message = (char*)data;
    
    if (message.indexOf("0C") >= 0){
      switchString = message.substring(2);
      switchStatus = switchString.toInt();
      Serial.println(switchStatus);  
      }
      
    if (message.indexOf("1s") >= 0) {
      sliderValue1 = message.substring(2);
      //threshold_engage1 = map(sliderValue1.toInt(), 0, 3300, 0, 3.3);
      threshold_engage1 = sliderValue1.toInt();
      threshold_engage1 = threshold_engage1*Volt_to_number_8/1000;
      Serial.println(threshold_engage1);
      //Serial.print(getSliderValues());
      notifyClients(getSliderValues());
    }
    if (message.indexOf("2s") >= 0) {
      sliderValue2 = message.substring(2);
      //threshold_disengage1 = map(sliderValue1.toInt(), 0, 3300, 0, 3.3);
      threshold_disengage1 = sliderValue2.toInt();
      threshold_disengage1 = threshold_disengage1*Volt_to_number_8/1000;
      Serial.println(threshold_disengage1);
      //Serial.print(getSliderValues());
      notifyClients(getSliderValues());
    }    
    if (message.indexOf("3s") >= 0) {
      sliderValue3 = message.substring(2);
      //threshold_engage2 = map(sliderValue3.toInt(), 0, 3300, 0, 3.3);
      threshold_engage2 = sliderValue3.toInt();
      threshold_engage2 = threshold_engage2*Volt_to_number_10/1000;
      Serial.println(threshold_engage2);
      //Serial.print(getSliderValues());
      notifyClients(getSliderValues());
    }
    if (message.indexOf("4s") >= 0) {
      sliderValue4 = message.substring(2);
      //threshold_disengage2 = map(sliderValue4.toInt(), 0, 3300, 0, 3.3);
      threshold_disengage2 = sliderValue4.toInt();
      threshold_disengage2 = threshold_disengage2*Volt_to_number_10/1000;
      Serial.println(threshold_disengage2);
      //Serial.print(getSliderValues());
      notifyClients(getSliderValues());
    }    
    if (strcmp((char*)data, "getValues") == 0) {
      notifyClients(getSliderValues());
    }
  }
}
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}


void setup() {
  Serial.begin(115200);
  // Set relay control pins 
  pinMode(relay11, OUTPUT);
  pinMode(relay12, OUTPUT);
  pinMode(relay13, OUTPUT);
  pinMode(relay14, OUTPUT);

  pinMode(relay21, OUTPUT);
  pinMode(relay22, OUTPUT);
  pinMode(relay23, OUTPUT);
  pinMode(relay24, OUTPUT);

  //Initially all realys should be off
  digitalWrite(relay11, HIGH);
  digitalWrite(relay12, HIGH);
  digitalWrite(relay13, HIGH);
  digitalWrite(relay14, HIGH);
  digitalWrite(relay21, HIGH);
  digitalWrite(relay22, HIGH);
  digitalWrite(relay23, HIGH);
  digitalWrite(relay24, HIGH);

  delay(1000);

  initFS();
  initWiFi();

  initWebSocket();
  
  // Web Server Root URL
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", "text/html");
  });
  
  server.serveStatic("/", SPIFFS, "/");

  // Start server
  server.begin();

}

void loop() {

  // Reading potentiometer value
  Value_laser2 = analogRead(ADC1_CH4);
  Value_laser2 = analogRead(ADC1_CH4);
  Value_laser2 = analogRead(ADC1_CH4);
  Value_laser1 = analogRead(ADC1_CH2);
  Value_laser1 = analogRead(ADC1_CH2);
  Value_laser1 = analogRead(ADC1_CH2);

  //Serial.println(Value_laser1);
  //Serial.println(Value_laser2);
  //Serial.println(threshold_engage1);
//  delay(200);
//  Serial.println(threshold_disengage1);
//  delay(200);
//  Serial.println(threshold_engage1);
//  delay(50);
//  Serial.println(threshold_disengage2);
//  delay(50);

if (switchStatus == 1){ //Go to lock tracking only if the "Engage lock tracking" is ticked
  
  if (Value_laser1 > threshold_engage1){
    timer_off_lock_laser1 = 0;
    digitalWrite(relay11, LOW);
    digitalWrite(relay12, LOW);
    digitalWrite(relay13, LOW);
    digitalWrite(relay14, LOW);
    }
    
   if (Value_laser1 < threshold_disengage1){
    if (timer_off_lock_laser1 > 5000) { 
    digitalWrite(relay11, HIGH);
    digitalWrite(relay12, HIGH);
    digitalWrite(relay13, HIGH);
    digitalWrite(relay14, HIGH);
    }
    Serial.println(timer_off_lock_laser1);
    timer_off_lock_laser1 = timer_off_lock_laser1 + millis();
    }
    
   if (Value_laser2 > threshold_engage2){
    timer_off_lock_laser2 = 0;
    digitalWrite(relay21, LOW);
    digitalWrite(relay22, LOW);
    digitalWrite(relay23, LOW);
    digitalWrite(relay24, LOW);
    }
    
   if (Value_laser2 < threshold_disengage2){
    if (timer_off_lock_laser2 > 5000) { 
    digitalWrite(relay21, HIGH);
    digitalWrite(relay22, HIGH);
    digitalWrite(relay23, HIGH);
    digitalWrite(relay24, HIGH);
    }
    timer_off_lock_laser2 = timer_off_lock_laser2 + millis();
    }
}else{
  // If "Engage lock tracking" is unticked, disengage all the relays
  digitalWrite(relay11, HIGH);
  digitalWrite(relay12, HIGH);
  digitalWrite(relay13, HIGH);
  digitalWrite(relay14, HIGH);
  digitalWrite(relay21, HIGH);
  digitalWrite(relay22, HIGH);
  digitalWrite(relay23, HIGH);
  digitalWrite(relay24, HIGH);
  }
  ws.cleanupClients();
  //delay(200);
}
