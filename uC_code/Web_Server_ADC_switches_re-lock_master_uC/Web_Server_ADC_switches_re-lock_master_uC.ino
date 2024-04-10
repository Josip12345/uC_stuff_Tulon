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
const int relay11 = 19; //GPIO15, laser 1 (Dragsa), integrator piezo 1 (IP1)
const int relay12 = 21; //GPIO14, laser 1 (Dragsa), integrator piezo 2 (IP2)
const int relay13 = 33; //GPIO29, laser 1 (Dragsa), integrator LD 1 (IL1)
const int relay14 = 34; //GPIO30, laser 1 (Dragsa), integrator LD 2 (IL2)
// Relay bank 2 set pins
const int relay21 = 35; //GPIO31, laser 2 (Asgard), integrator piezo 1 (IP1)
const int relay22 = 36; //GPIO32, laser 2 (Asgard), integrator piezo 2 (IP2)
const int relay23 = 37; //GPIO33, laser 2 (Asgard), integrator LD 1 (IL1)
const int relay24 = 38; //GPIO34, laser 2 (Asgard), integrator LD 2 (IL2)
const int flag_to_slave = 39; 

// Variable for storing laser transmission values obtained by ESP ADCs
int Value_laser1 = 0;
int Value_laser2 = 0;
unsigned long timer_off_lock_laser1 = 0;
unsigned long timer_off_lock_laser2 = 0;








//////////For re-lock
/////////////////////
int dacpin1 = DAC1; // pin 17 on ESP
int dacpin2 = DAC2; // pin 18 on ESP

float amplitude1 = 0.3; // Amplitude of the fast sine function, 0.3 correpsonds to 1V DAC ouput
float DC_offset1 = 193; //Offset of the fast sine function is 2.5V, (2.5/3.3)*256
float amplitude2_max = 0.06; // Amplitude of the slow sine function, 0.06 correpsonds to 200mV DAC ouput
float DC_offset2 = 193; //Offset of the slow sine function is 2.5V, (2.5/3.3)*256
int amp_incr_cnt = 0; // This is the counter for increasing the slow sine amplitude (one modulating the LD current) in integer steps until the 
//lock condition is found
const int num_amp_steps = 5; //Number of slow sine amplitude steps
float amp_incr = amplitude2_max/num_amp_steps;
float amplitude2 = amp_incr; //Starting amplitude of the slow sine is equal to amp_incr
hw_timer_t *timer = timerBegin(1, 80, true);  // Timer 1, prescaler 80, count up

// Sine LookUpTable & Index Variable
int SampleIdx1 = 0; // Index for going through the fast sine function
int SampleIdx2 = 0; // Index for going throuhg the slow sine function

const int sine30LookupTable[] = { // Lookup table for the fast sine function
   1,   28,   54,   78,   98,  114,  124,  128,  127,  119,  106,
         89,   66,   41,   14,  -13,  -40,  -65,  -88, -105, -118, -126,
       -127, -123, -113,  -97,  -77,  -53,  -27,    0};

// Lookup table for the slow sine function
const int sine900LookupTable[] = { 1,    1,    2,    3,    4,    5,    6,    7,    8,    9,    9,
         10,   11,   12,   13,   14,   15,   16,   17,   17,   18,   19,
         20,   21,   22,   23,   24,   25,   25,   26,   27,   28,   29,
         30,   31,   31,   32,   33,   34,   35,   36,   37,   38,   38,
         39,   40,   41,   42,   43,   43,   44,   45,   46,   47,   48,
         49,   49,   50,   51,   52,   53,   53,   54,   55,   56,   57,
         57,   58,   59,   60,   61,   61,   62,   63,   64,   65,   65,
         66,   67,   68,   68,   69,   70,   71,   71,   72,   73,   74,
         74,   75,   76,   77,   77,   78,   79,   79,   80,   81,   81,
         82,   83,   84,   84,   85,   86,   86,   87,   88,   88,   89,
         89,   90,   91,   91,   92,   93,   93,   94,   94,   95,   96,
         96,   97,   97,   98,   99,   99,  100,  100,  101,  101,  102,
        103,  103,  104,  104,  105,  105,  106,  106,  107,  107,  108,
        108,  109,  109,  110,  110,  111,  111,  111,  112,  112,  113,
        113,  114,  114,  114,  115,  115,  116,  116,  116,  117,  117,
        118,  118,  118,  119,  119,  119,  120,  120,  120,  121,  121,
        121,  121,  122,  122,  122,  123,  123,  123,  123,  124,  124,
        124,  124,  125,  125,  125,  125,  125,  126,  126,  126,  126,
        126,  126,  127,  127,  127,  127,  127,  127,  127,  128,  128,
        128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,
        128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,
        128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,
        128,  127,  127,  127,  127,  127,  127,  127,  127,  126,  126,
        126,  126,  126,  125,  125,  125,  125,  125,  124,  124,  124,
        124,  123,  123,  123,  123,  122,  122,  122,  122,  121,  121,
        121,  120,  120,  120,  119,  119,  119,  118,  118,  118,  117,
        117,  117,  116,  116,  115,  115,  115,  114,  114,  113,  113,
        113,  112,  112,  111,  111,  110,  110,  109,  109,  108,  108,
        107,  107,  106,  106,  105,  105,  104,  104,  103,  103,  102,
        102,  101,  101,  100,   99,   99,   98,   98,   97,   97,   96,
         95,   95,   94,   94,   93,   92,   92,   91,   90,   90,   89,
         89,   88,   87,   87,   86,   85,   85,   84,   83,   83,   82,
         81,   80,   80,   79,   78,   78,   77,   76,   75,   75,   74,
         73,   73,   72,   71,   70,   70,   69,   68,   67,   66,   66,
         65,   64,   63,   63,   62,   61,   60,   59,   59,   58,   57,
         56,   55,   55,   54,   53,   52,   51,   51,   50,   49,   48,
         47,   46,   46,   45,   44,   43,   42,   41,   41,   40,   39,
         38,   37,   36,   35,   35,   34,   33,   32,   31,   30,   29,
         28,   28,   27,   26,   25,   24,   23,   22,   21,   21,   20,
         19,   18,   17,   16,   15,   14,   13,   13,   12,   11,   10,
          9,    8,    7,    6,    5,    5,    4,    3,    2,    1,    0,
         -1,   -2,   -3,   -4,   -4,   -5,   -6,   -7,   -8,   -9,  -10,
        -11,  -12,  -12,  -13,  -14,  -15,  -16,  -17,  -18,  -19,  -20,
        -20,  -21,  -22,  -23,  -24,  -25,  -26,  -27,  -27,  -28,  -29,
        -30,  -31,  -32,  -33,  -34,  -34,  -35,  -36,  -37,  -38,  -39,
        -40,  -40,  -41,  -42,  -43,  -44,  -45,  -45,  -46,  -47,  -48,
        -49,  -50,  -50,  -51,  -52,  -53,  -54,  -54,  -55,  -56,  -57,
        -58,  -58,  -59,  -60,  -61,  -62,  -62,  -63,  -64,  -65,  -65,
        -66,  -67,  -68,  -69,  -69,  -70,  -71,  -72,  -72,  -73,  -74,
        -74,  -75,  -76,  -77,  -77,  -78,  -79,  -79,  -80,  -81,  -82,
        -82,  -83,  -84,  -84,  -85,  -86,  -86,  -87,  -88,  -88,  -89,
        -89,  -90,  -91,  -91,  -92,  -93,  -93,  -94,  -94,  -95,  -96,
        -96,  -97,  -97,  -98,  -98,  -99, -100, -100, -101, -101, -102,
       -102, -103, -103, -104, -104, -105, -105, -106, -106, -107, -107,
       -108, -108, -109, -109, -110, -110, -111, -111, -112, -112, -112,
       -113, -113, -114, -114, -114, -115, -115, -116, -116, -116, -117,
       -117, -117, -118, -118, -118, -119, -119, -119, -120, -120, -120,
       -121, -121, -121, -121, -122, -122, -122, -122, -123, -123, -123,
       -123, -124, -124, -124, -124, -124, -125, -125, -125, -125, -125,
       -126, -126, -126, -126, -126, -126, -126, -126, -127, -127, -127,
       -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127,
       -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127,
       -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127,
       -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125,
       -125, -125, -124, -124, -124, -124, -124, -123, -123, -123, -123,
       -122, -122, -122, -122, -121, -121, -121, -120, -120, -120, -120,
       -119, -119, -119, -118, -118, -118, -117, -117, -117, -116, -116,
       -115, -115, -115, -114, -114, -113, -113, -113, -112, -112, -111,
       -111, -110, -110, -110, -109, -109, -108, -108, -107, -107, -106,
       -106, -105, -105, -104, -104, -103, -103, -102, -102, -101, -100,
       -100,  -99,  -99,  -98,  -98,  -97,  -96,  -96,  -95,  -95,  -94,
        -93,  -93,  -92,  -92,  -91,  -90,  -90,  -89,  -88,  -88,  -87,
        -87,  -86,  -85,  -85,  -84,  -83,  -83,  -82,  -81,  -80,  -80,
        -79,  -78,  -78,  -77,  -76,  -76,  -75,  -74,  -73,  -73,  -72,
        -71,  -70,  -70,  -69,  -68,  -67,  -67,  -66,  -65,  -64,  -64,
        -63,  -62,  -61,  -60,  -60,  -59,  -58,  -57,  -56,  -56,  -55,
        -54,  -53,  -52,  -52,  -51,  -50,  -49,  -48,  -48,  -47,  -46,
        -45,  -44,  -43,  -42,  -42,  -41,  -40,  -39,  -38,  -37,  -37,
        -36,  -35,  -34,  -33,  -32,  -31,  -30,  -30,  -29,  -28,  -27,
        -26,  -25,  -24,  -24,  -23,  -22,  -21,  -20,  -19,  -18,  -17,
        -16,  -16,  -15,  -14,  -13,  -12,  -11,  -10,   -9,   -8,   -8,
         -7,   -6,   -5,   -4,   -3,   -2,   -1,    0,    0};

void IRAM_ATTR timer1_ISR() { // Timer interrupt routine
// Send SineTable Values To DAC One By One
  int ramp_amp1;
  int ramp_amp2;
  ramp_amp1 = int(sine30LookupTable[SampleIdx1++]*amplitude1+DC_offset1); // Going thorught the fast sine values
  ramp_amp2 = int(sine900LookupTable[SampleIdx2++]*amplitude2+DC_offset2); // Going thorught the fast sine values
  // The frequency ratio of fast to slow sine functions is defined by the ratio of their number of points
  
  if(ramp_amp1 > 255){
    ramp_amp1 = 255;
  }else if (ramp_amp1 < 0){
    ramp_amp1 = 0;
  }
  if(SampleIdx1 == 30){
    SampleIdx1 = 0;
  }
  
  if(ramp_amp2 > 255){
    ramp_amp2 = 255;
  }else if (ramp_amp2 < 0){
    ramp_amp2 = 0;
  }
  
  if(SampleIdx2 == 900){
    SampleIdx2 = 0;
    amplitude2 = amplitude2 + amp_incr;
    amp_incr_cnt++;
    if (amp_incr_cnt == num_amp_steps){
      amplitude2 = amp_incr;
      amp_incr_cnt = 0;
      }
    
    
  }
  dacWrite(dacpin1, ramp_amp1);
  dacWrite(dacpin2, ramp_amp2);
}

void timer1_setup(uint32_t interval) {
  // Initialize Timer 1
  
  // Attach the ISR function to Timer 1
  timerAttachInterrupt(timer, &timer1_ISR, true);

  // Set the timer to repeat every 'interval' microseconds
  timerAlarmWrite(timer, interval, true);

  // Disable the timer initially
  timerAlarmDisable(timer);
  //timerAlarmEnable(timer);
}










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
  pinMode(flag_to_slave, OUTPUT);

  //Initially all realys should be off
  digitalWrite(relay11, HIGH);
  digitalWrite(relay12, HIGH);
  digitalWrite(relay13, HIGH);
  digitalWrite(relay14, HIGH);
  digitalWrite(relay21, HIGH);
  digitalWrite(relay22, HIGH);
  digitalWrite(relay23, HIGH);
  digitalWrite(relay24, HIGH);
  timer1_setup(2000); // 10 is 2ms interval; like this sine with 30 points is around 21Hz and the one with 900 points is around 0.5 Hz

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
    
    digitalWrite(flag_to_slave, LOW); // Send the command to the slave uC to stop generating re-lock waveform
    // Order of engaging integrators matters!
    digitalWrite(relay12, LOW);  //laser 1 (Dragsa), integrator piezo 2 (IP2)
    delay(200);
    digitalWrite(relay11, LOW);  //laser 1 (Dragsa), integrator piezo 1 (IP1)
    delay(200);
    digitalWrite(relay13, LOW);  //laser 1 (Dragsa), integrator LD 1 (IL1)
    delay(200);
    digitalWrite(relay14, LOW);  //laser 1 (Dragsa), integrator LD 2 (IL2)
    delay(200);
    }
    
   if (Value_laser1 < threshold_disengage1){
    if (timer_off_lock_laser1 > 5000) { 
    digitalWrite(relay11, HIGH);
    digitalWrite(relay12, HIGH);
    digitalWrite(relay13, HIGH);
    digitalWrite(relay14, HIGH);
    digitalWrite(flag_to_slave, HIGH); // Send the command to the slave uC to start generating re-lock waveform 
    }
    timer_off_lock_laser1 = timer_off_lock_laser1 + millis();
    }
    
   if (Value_laser2 > threshold_engage2){
    timer_off_lock_laser2 = 0;
    timerAlarmDisable(timer); // Stop generating re-lock waveforms
    // Order of engaging integrators matters!
    digitalWrite(relay22, LOW); //GPIO32, laser 2 (Asgard), integrator piezo 2 (IP2)
    delay(200);
    digitalWrite(relay21, LOW); //GPIO31, laser 2 (Asgard), integrator piezo 1 (IP1)
    delay(200);
    digitalWrite(relay23, LOW); //GPIO33, laser 2 (Asgard), integrator LD 1 (IL1)
    delay(200);
    digitalWrite(relay24, LOW); //GPIO34, laser 2 (Asgard), integrator LD 2 (IL2)
    delay(200);
    }
    
   if (Value_laser2 < threshold_disengage2){
    if (timer_off_lock_laser2 > 5000) { 
    digitalWrite(relay21, HIGH);
    digitalWrite(relay22, HIGH);
    digitalWrite(relay23, HIGH);
    digitalWrite(relay24, HIGH);
    timerAlarmEnable(timer); // Start generating re-lock waveforms
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
  timerAlarmDisable(timer); // Stop generating re-lock waveforms
  digitalWrite(flag_to_slave, LOW); // Send the command to the slave uC to stop generating re-lock waveform
  }
  ws.cleanupClients();
  //delay(200);
}
