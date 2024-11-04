  /* 
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/esp32-web-server-websocket-sliders/
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.
  
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*/
#include <SPI.h>
#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "SPIFFS.h"
#include <Arduino_JSON.h>


// Replace with your network credentials
const char* ssid = "LockTrack1";
const char* password = "";

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
// Create a WebSocket object

AsyncWebSocket ws("/ws");

// ESP ADC channels used for monitoring laser transmission signals
const int ADC1_CH5 = 10; // PD laser 1
// Relay bank 1 set pins
const int relay11 = 19; //integrator piezo 1 (IP1)
const int relay12 = 21; //integrator piezo 2 (IP2)
const int relay13 = 33; //integrator LD 1 (IL1)
const int relay14 = 34; //integrator LD 2 (IL2)


 
int osci_trigger_laser1 = 5; 

// Variable for storing laser transmission values obtained by ESP ADCs
int Value_laser1 = 0;

unsigned long timer_off_lock_laser1 = 0;





//////////For relock

// Some defintions for SPI bus connecting the dig pot to the microcontroller
// Piezo
#define VSPI_MISO   13 //SDO
#define VSPI_MOSI   42 //SDI
#define VSPI_SCLK   41
#define VSPI_SS     40

// LD
#define HSPI_MISO   18 //SDO
#define HSPI_MOSI   17 //SDI
#define HSPI_SCLK   16
#define HSPI_SS     15

#define VSPI FSPI

static const int spiClk = 10000000; // 10 MHz

//uninitalised pointers to SPI objects
SPIClass * vspi = NULL;
SPIClass * hspi = NULL;

void spiCommand(SPIClass *spi, byte data1, byte data2) {
  //use it as you would the regular arduino SPI API
  spi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE0));
  digitalWrite(spi->pinSS(), LOW); //pull SS slow to prep other end for transfer
  spi->transfer(data1); 
  // The digital potentiometer MCP4231 wants 16 bits of data for the write command and it
  // works if we send them as two separate bytes, like this
  spi->transfer(data2);
  digitalWrite(spi->pinSS(), HIGH); //pull ss high to signify end of data transfer
  spi->endTransaction();
}




float amplitude1 = 0.25; // Amplitude of the fast sine function, this gives a large amplitude to cover piezo range
float DC_offset1 = 0; //Some calibrated value for the offset
float amplitude2_max = 0.05; // Amplitude of the slow sine function
float DC_offset2 = 0; //Some calibrated value for the offset
int amp_incr_cnt = 0; // This is the counter for increasing the slow sine amplitude (one modulating the LD current) in integer steps until the 
//lock condition is found
const int num_amp_steps = 10; //Number of slow sine amplitude steps
float amp_incr = amplitude2_max/num_amp_steps;
float amplitude2 = amp_incr; //Starting amplitude of the slow sine is equal to amp_incr
hw_timer_t *timer = timerBegin(1, 80, true);  // Timer 1, prescaler 80, count up
int Piezo_relock_amp;
int LD_relock_amp;
float ramp_amp = 0.5;
int ra;
int rv=0; //Buffer variable for the piezo dig pot offset
int rv_LD=0; //Buffer variable for the LD dig pot offset


// Sine LookUpTable & Index Variable
int SampleIdx1_laser1 = 0; // Index for going through the fast sine function
int SampleIdx2_laser1 = 0; // Index for going throuhg the slow sine function 
int SampleIdx3_laser1 = 0; // Index for going through the ramp waveform




boolean toogle_laser1 = true; // toogle_laser1 for the oscilloscope ramp trigger



const int ramp10LookupTable[] = { //Lookup table for the ramp, this is going to be applied to the LD
       -127,  -99,  -71,  -42,  -14,   14,   43,   71,   99,  128};

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



       
bool next_sample = 0; // Timer conter variable

void IRAM_ATTR timer1_ISR() { 
  // Timer interrupt routine
  // It is used only for giving a flag, which indicates to go to he next sample in the waveform generation, to keep it short
  next_sample = 1;
}

void timer1_setup(uint32_t interval) {
  // Initialize Timer 1
  
  // Attach the ISR function to Timer 1
  timerAttachInterrupt(timer, &timer1_ISR, true);

  // Set the timer to repeat every 'interval' microseconds
  timerAlarmWrite(timer, interval, true);

  
  timerAlarmEnable(timer);
}

  


// WAVEFORM GENERATION ROUTINES

void gen_relock_wave(){
  // This routine geenrates re-lock waveforms for LD (slow) and piezo (fast)
  // It goes through values in the respective lookup tables and sends
  // those values to the respective trimpots via SPI bus
  if (next_sample == 0){
    return;
  }
  next_sample = 0; // Erase the timer flag
 
  Piezo_relock_amp = int((sine30LookupTable[SampleIdx1_laser1++]+DC_offset1)*amplitude1+rv); // Going thorught the fast sine values, value from -128 to 128 are transposed to 0 to 
  LD_relock_amp = int(sine900LookupTable[SampleIdx2_laser1++]*amplitude2+DC_offset2+rv_LD); // Going thorught the slow sine values
  // The frequency ratio of fast to slow sine functions is defined by the ratio of their number of points
  
  if(Piezo_relock_amp > 255){
    Piezo_relock_amp = 255;
  }else if (Piezo_relock_amp < 0){
    Piezo_relock_amp = 0;
  }
  if(SampleIdx1_laser1 == 30){
    SampleIdx1_laser1 = 0;
  }
  
  if(LD_relock_amp > 255){
    LD_relock_amp = 255;
  }else if (LD_relock_amp < 0){
    LD_relock_amp = 0;
  }
  
  if(SampleIdx2_laser1 == 900){
    SampleIdx2_laser1 = 0;
    amplitude2 = amplitude2 + amp_incr;
    amp_incr_cnt++;
    if (amp_incr_cnt == num_amp_steps){
      amplitude2 = amp_incr;
      amp_incr_cnt = 0;
      }
  }
  spiCommand(hspi, 00, LD_relock_amp); // The slow (LD) relock waveform goes to port A of the dig trimpot 1
  spiCommand(vspi, 00, Piezo_relock_amp); // The fast (piezo) relock waveform goes to port A of the dig trimpot 2
  }


void gen_next_FSR_wave(){
  
  // This routine generates piezo (fast) and LD (slow) waveforms for searching next 00 mode (going to the next FSR)
  // Goes through a full period (2pi) of path lenght piezo phase by going through values in the respective lookup table and sends
  // those values to the respective trimpot via SPI bus
  // It does that with the maximium loop execution speed (no timer involved)
  // Once it goes through a full piezo waveform period, it increases the value of the LD dig pot for the minimum amount, adding to the present LD offset value
  
  SampleIdx1_laser1 = SampleIdx1_laser1 + 3; // Outputing every thrid sample in from the sine30LookupTable
  Piezo_relock_amp = int((sine30LookupTable[SampleIdx1_laser1++]+DC_offset1)*amplitude1+rv); // Going through fast sine values, values from -128 to 128 are transposed to 0 to 255
  
  if(Piezo_relock_amp > 255){
    Piezo_relock_amp = 255;
  }else if (Piezo_relock_amp < 0){
    Piezo_relock_amp = 0;
  }
  if(SampleIdx1_laser1 == 30){
    SampleIdx1_laser1 = 0;
    
    // After completing a whole piezo waveform period
    // setting the next LD dig pot value, towards the next 00 mode
    rv_LD = rv_LD + 1; // Using the same variable as for the offset entered by the user, such that it is easier to update the respective offset web server slider afterwards
    spiCommand(hspi, 00, rv_LD); 
  }
  
  spiCommand(vspi, 00, Piezo_relock_amp); // The fast (piezo) relock waveform goes to port A of the dig trimpot 2
  }


void gen_ramp(){ 
  // This routine generates the ramp waveform for the LD
  // It goes through values in the respective lookup table 
  // those values to the respective trimpot via SPI bus
  if (next_sample == 0){
    return;
  }
  next_sample = 0; // Erase the timer flag
  
  ra = int(ramp10LookupTable[SampleIdx3_laser1++]*ramp_amp+128); // Going through the ramp values
  if(SampleIdx3_laser1 == 10){
    SampleIdx3_laser1 = 0;
    toogle_laser1 = !toogle_laser1;
    digitalWrite(osci_trigger_laser1, toogle_laser1); // Generating a sync trigger signal for the oscilloscope
  }
  if(ra > 255){
    ra = 255;
  }else if (ra < 0){
    ra = 0;
  }
  spiCommand(hspi, 00, ra); // The ramps waveform goes to port A of the dig trimpot 1 (LD) 
}








// ADC conversion factors
int Volt_to_number_8 = 3100.0;
int Volt_to_number_10 = 3100.0;

// Initial threshold values
int threshold_engage1 = Volt_to_number_8*1.0; // Integrators engaged above PD value of 1 V
int threshold_disengage1 = Volt_to_number_8*0.8; // Integrators disengaged below a PD value of 0.8 V
                                               // Like this we have needed hysteresis
                                     

String message = "";
String sliderValue1 = "0";
String sliderValue2 = "0";
String sliderValue3 = "0";
String sliderValue4 = "64";
String sliderValue5 = "64";
int lock_fail_counter = 0;
String elp = "0";

int engage_relock_track_laser1 = 0; // This flag determines if the laser 1 lock status should be tracked. Does not imply outputing relock waveforms.

int engage_ramp_laser1 = 0; // Flag that determines if the ramp waveform should be outputed for laser 1 (if it has a value of 1) or not (if it has a value of 0). 
                            // Apart from this flag to be 1, for the ramp to be outputed on laser 1, the engage_relock_track_laser1 flag should be 0.

bool jump_FSRP = 0; // Flag that determines weather to jump one FSR in the LD current (frequency) increasing direction
 





//Json Variable to Hold Slider Values
JSONVar sliderValues;

//Get Slider Values
String getSliderValues(){
  sliderValues["sliderValue1"] = String(sliderValue1);
  sliderValues["sliderValue2"] = String(sliderValue2);
  sliderValues["sliderValue3"] = String(sliderValue3);
  sliderValues["sliderValue4"] = String(sliderValue4);
  sliderValues["sliderValue5"] = String(sliderValue5);
  sliderValues["lock_fail_counter"] = String(lock_fail_counter);
  
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
    
    if (message.indexOf("1REL") >= 0){
      elp = message.substring(4);
      engage_relock_track_laser1 = elp.toInt();
      //Serial.println(switchStatus);  
      }


    if (message.indexOf("1RAM") >= 0){
      elp = message.substring(4);
      engage_ramp_laser1 = elp.toInt();
      //Serial.println(switchStatus);  
      }

     if (message.indexOf("FSRP") >= 0){
      jump_FSRP = true; //If FSRP button is clicked, then set FSRP flag to true. This indicates to the loop to go to the FSR jump positive routine
      //Serial.println(switchStatus);  
      }
      

      
    if (message.indexOf("1s") >= 0) {
      sliderValue1 = message.substring(2);
      //threshold_engage1 = map(sliderValue1.toInt(), 0, 3300, 0, 3.3);
      threshold_engage1 = sliderValue1.toInt();
      threshold_engage1 = threshold_engage1*Volt_to_number_8/1000;
      //Serial.println(threshold_engage1);
      //Serial.print(getSliderValues());
      notifyClients(getSliderValues());
    }
    if (message.indexOf("2s") >= 0) {
      sliderValue2 = message.substring(2);
      //threshold_disengage1 = map(sliderValue1.toInt(), 0, 3300, 0, 3.3);
      threshold_disengage1 = sliderValue2.toInt();
      threshold_disengage1 = threshold_disengage1*Volt_to_number_8/1000;
      //Serial.println(threshold_disengage1);
      //Serial.print(getSliderValues());
      notifyClients(getSliderValues());
    }      

    if (message.indexOf("3s") >= 0) {
      sliderValue3 = message.substring(2);
      //threshold_disengage1 = map(sliderValue1.toInt(), 0, 3300, 0, 3.3);
      ramp_amp = sliderValue3.toFloat();
      ramp_amp = ramp_amp/100;
      //Serial.println(threshold_disengage1);
      //Serial.print(getSliderValues());
      notifyClients(getSliderValues());
    }

    if (message.indexOf("4s") >= 0) {
      sliderValue4 = message.substring(2);
      rv = sliderValue4.toInt();
      spiCommand(vspi, 00, rv); // Setting the offset piezo value entered by the user via slider 4
      notifyClients(getSliderValues());
    }

    if (message.indexOf("5s") >= 0) {
      sliderValue5 = message.substring(2);
      rv_LD = sliderValue5.toInt();
      rv_LD = map(rv_LD, 0, 128, 18, 128); // Accounting for the non-linearity caused by the parallel resistance of the offset diff amp input resistors
      spiCommand(hspi, 00, rv_LD); // Setting the offset LD value entered by the user via slider 5
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
  //initialise the instance of the SPIClasses
  vspi = new SPIClass(VSPI);
  hspi = new SPIClass(HSPI);
  // route through GPIO pins of your choice
  vspi->begin(VSPI_SCLK, VSPI_MISO, VSPI_MOSI, VSPI_SS); //SCLK, MISO, MOSI, SS
  hspi->begin(HSPI_SCLK, HSPI_MISO, HSPI_MOSI, HSPI_SS); //SCLK, MISO, MOSI, SS
  //set up slave select pins as outputs as the Arduino API
  //doesn't handle automatically pulling SS low
  pinMode(vspi->pinSS(), OUTPUT); //VSPI SS
  pinMode(hspi->pinSS(), OUTPUT); //HSPI SS
  
  Serial.begin(115200);
  // Set relay control pins 
  pinMode(relay11, OUTPUT);
  pinMode(relay12, OUTPUT);
  pinMode(relay13, OUTPUT);
  pinMode(relay14, OUTPUT);

  pinMode(osci_trigger_laser1, OUTPUT);
  //Initially all realys and outputs should be in the OFF state
  digitalWrite(relay11, LOW);
  digitalWrite(relay12, LOW);
  digitalWrite(relay13, LOW);
  digitalWrite(relay14, LOW);

  timer1_setup(1000); // 10 is 2ms interval; like this sine with 30 points is around 21Hz and the one with 900 points is around 0.5 Hz

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



bool lock_fail_counter_flag = 1; //This flag indicates wheather to increase the re-lock couner or not

 
int delta_millis = 0; 

void loop() {

  // Reading PD value
  Value_laser1 = analogRead(ADC1_CH5);
  Value_laser1 = analogRead(ADC1_CH5);
  Value_laser1 = analogRead(ADC1_CH5);


if (jump_FSRP){
  SampleIdx2_laser1 = 0; // Resetting LD relock waveform to start from 0, next time re-lock is activated
                         // otherwise it could cause a too large jump of the LD current
  gen_next_FSR_wave();   // Generating next sample of the next 00 mode finding waveform

  if (Value_laser1 > threshold_engage1){ // If the next 00 mode is found
    jump_FSRP = false; // Set this flag to false to stop searching for the next 00 mode

    // Update the LD offset slider in the web server
    sliderValue5 = rv_LD;
    notifyClients(getSliderValues());
    }
  
  
  
  }else if (engage_relock_track_laser1 == 1){ //Go to lock tracking only if the "Engage lock tracking laser 1" is ticked
    
    



  if (Value_laser1 > threshold_engage1){
    timer_off_lock_laser1 = 0;
    delta_millis = millis(); //Get the exact time when the lock condition started to be satisfied
   
    // Order of engaging integrators matters!
    digitalWrite(relay12, LOW);  //laser 1, integrator piezo 2 (IP2)
    delay(20);
    digitalWrite(relay11, LOW);  //laser 1, integrator piezo 1 (IP1)
    delay(20);
    digitalWrite(relay13, LOW);  //laser 1, integrator LD 1 (IL1)
    delay(20);
    digitalWrite(relay14, LOW);  //laser 1, integrator LD 2 (IL2)
    delay(20);
    lock_fail_counter_flag = 0;
    
    }
    
   if (Value_laser1 < threshold_disengage1){
    if (timer_off_lock_laser1 > 5000) {  //Here we wait for some time between laser transmission amplitude goes below the lock threshold and disengaging the lock/starting re-lock waveform 
    digitalWrite(relay11, HIGH);
    digitalWrite(relay12, HIGH);
    digitalWrite(relay13, HIGH);
    digitalWrite(relay14, HIGH);
    if (lock_fail_counter_flag == 0){ // Just at the first passage of this loop increase the re-lock counter
                                    // otherwise re-lock_counter would continue to increase all the time while the laser is not locked
      lock_fail_counter++;
      notifyClients(getSliderValues());
      lock_fail_counter_flag = 1;
      }
    gen_relock_wave(); // Generating the next sample of the relock waveforms
    }
    timer_off_lock_laser1 = timer_off_lock_laser1 + millis() - delta_millis; // Subsctract the exact time when the lock condition started to be satisfied
                                                                             // to get the time from when that happened
    }
}else{
  // If "Engage lock tracking" is unticked, disengage the integrators
  digitalWrite(relay11, LOW);
  digitalWrite(relay12, LOW);
  digitalWrite(relay13, LOW);
  digitalWrite(relay14, LOW);
  
  if (engage_ramp_laser1 == 1){ // If the Engage ramp laser 1 is ticked, start generating the ramp on laser 1, now when the relock track is off
  gen_ramp();
  }

  ws.cleanupClients();
  //delay(200);
}
}
