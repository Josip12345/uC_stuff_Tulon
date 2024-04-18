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
const char* ssid = "LockTrack";
const char* password = "";

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
// Create a WebSocket object

AsyncWebSocket ws("/ws");

// ESP ADC channels used for monitoring laser transmission signals
const int ADC1_CH5 = 10; // PD laser 1
const int ADC1_CH6 = 8; // PD laser 2

// Relay bank 1 set pins
const int relay11 = 19; //laser 1, integrator piezo 1 (IP1)
const int relay12 = 21; //laser 1, integrator piezo 2 (IP2)
const int relay13 = 33; //laser 1, integrator LD 1 (IL1)
const int relay14 = 34; //laser 1, integrator LD 2 (IL2)
// Relay bank 2 set pins
const int relay21 = 35; //laser 2, integrator piezo 1 (IP1)
const int relay22 = 36; //laser 2, integrator piezo 2 (IP2)
const int relay23 = 37; //laser 2, integrator LD 1 (IL1)
const int relay24 = 38; //laser 2, integrator LD 2 (IL2)

 
int osci_trigger_laser1 = 5; 
int osci_trigger_laser2 = 6;

// Variable for storing laser transmission values obtained by ESP ADCs
int Value_laser1 = 0;
int Value_laser2 = 0;
unsigned long timer_off_lock_laser1 = 0;
unsigned long timer_off_lock_laser2 = 0;

int re_lock_gen_flag_laser1 = 0; // This flag indicates to the timer interrupt routine that relock waveforms should be outputed for laser 1; (if it has a value of 1) or not (if it has the value of 0)
int re_lock_gen_flag_laser2 = 0; // This flag indicates to the timer interrupt routine that relock waveforms should be outputed for laser 2; (if it has a value of 1) or not (if it has the value of 0)
int ramp_gen_flag_laser1 = 0; // This flag indicates to the timer interrupt routine that it should generate the ramp waveform for the laser 1; (if it has a value of 1) or not (if it has the value of 0)
int ramp_gen_flag_laser2 = 0; // This flag indicates to the timer interrupt routine that it should generate the ramp waveform for the laser 2; (if it has a value of 1) or not (if it has the value of 0)



//////////For relock

// Some defintions for SPI bus connecting the dig pot to the microcontroller
#define VSPI_MISO   13 //SDO
#define VSPI_MOSI   42 //SDI
#define VSPI_SCLK   41
#define VSPI_SS     40

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




float amplitude1 = 0.25; // Amplitude of the fast sine function, this value is calibrated for pp piezo voltage of approx 12 V
float DC_offset1 = 56; //Some calibrated value for the offset
float amplitude2_max = 0.2; // Amplitude of the slow sine function, calibrated for 200mV pp at the LD, with the 1.5kohm shunt resistor 
float DC_offset2 = 56; //Some calibrated value for the offset
int amp_incr_cnt = 0; // This is the counter for increasing the slow sine amplitude (one modulating the LD current) in integer steps until the 
//lock condition is found
const int num_amp_steps = 5; //Number of slow sine amplitude steps
float amp_incr = amplitude2_max/num_amp_steps;
float amplitude2 = amp_incr; //Starting amplitude of the slow sine is equal to amp_incr
hw_timer_t *timer = timerBegin(1, 80, true);  // Timer 1, prescaler 80, count up
int ramp_amp1;
int ramp_amp2;
int ramp_amp;

// Sine LookUpTable & Index Variable
int SampleIdx1_laser1 = 0; // Index for going through the fast sine function
int SampleIdx2_laser1 = 0; // Index for going throuhg the slow sine function
int SampleIdx3_laser1 = 0; // Index for going through the ramp waveform

int SampleIdx1_laser2 = 0; // Index for going through the fast sine function
int SampleIdx2_laser2 = 0; // Index for going throuhg the slow sine function
int SampleIdx3_laser2 = 0; // Index for going through the ramp waveform


boolean toogle_laser1 = true; // toogle_laser1 for the oscilloscope ramp trigger
boolean toogle_laser2 = true; // toogle_laser2 for the oscilloscope ramp trigger


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

void IRAM_ATTR timer1_ISR() { // Timer interrupt routine
  
///// LASER 1
////////////
  if (re_lock_gen_flag_laser1 == 1){ // If re_lock_gen_flag_laser2 == 1 means that the relock waveforms should be outputed
// Send SineTable Values To DAC One By One
  
  ramp_amp1 = int(sine30LookupTable[SampleIdx1_laser1++]*amplitude1+DC_offset1); // Going thorught the fast sine values
  ramp_amp2 = int(sine900LookupTable[SampleIdx2_laser1++]*amplitude2+DC_offset2); // Going thorught the slow sine values
  // The frequency ratio of fast to slow sine functions is defined by the ratio of their number of points
  
  if(ramp_amp1 > 255){
    ramp_amp1 = 255;
  }else if (ramp_amp1 < 0){
    ramp_amp1 = 0;
  }
  if(SampleIdx1_laser1 == 30){
    SampleIdx1_laser1 = 0;
  }
  
  if(ramp_amp2 > 255){
    ramp_amp2 = 255;
  }else if (ramp_amp2 < 0){
    ramp_amp2 = 0;
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
  spiCommand(hspi, 00, ramp_amp2); // The slow (LD) relock waveform goes to port A of the dig trimpot 1
  spiCommand(vspi, 00, ramp_amp1); // The fast (piezo) relock waveform goes to port A of the dig trimpot 2
  }

  
  if(ramp_gen_flag_laser1 == 1){ 
    ramp_amp = int(ramp10LookupTable[SampleIdx3_laser1++]*0.7+DC_offset2); // Going through the ramp values
    if(SampleIdx3_laser1 == 10){
      SampleIdx3_laser1 = 0;
      toogle_laser1 = !toogle_laser1;
      digitalWrite(osci_trigger_laser1, toogle_laser1); // Generating a sync trigger signal for the oscilloscope
    }
    if(ramp_amp > 255){
      ramp_amp = 255;
    }else if (ramp_amp < 0){
      ramp_amp = 0;
    }
    spiCommand(hspi, 00, ramp_amp); // The ramps waveform goes to port A of the dig trimpot 1 (LD)
 }

///// LASER 2
/////////////  
  if (re_lock_gen_flag_laser2 == 1){ // If re_lock_gen_flag_laser2 == 1 means that the relock waveforms should be outputed
// Send SineTable Values To DAC One By One
  
  ramp_amp1 = int(sine30LookupTable[SampleIdx1_laser2++]*amplitude1+DC_offset1); // Going thorught the fast sine values
  ramp_amp2 = int(sine900LookupTable[SampleIdx2_laser2++]*amplitude2+DC_offset2); // Going thorught the slow sine values
  // The frequency ratio of fast to slow sine functions is defined by the ratio of their number of points
  
  if(ramp_amp1 > 255){
    ramp_amp1 = 255;
  }else if (ramp_amp1 < 0){
    ramp_amp1 = 0;
  }
  if(SampleIdx1_laser2 == 30){
    SampleIdx1_laser2 = 0;
  }
  
  if(ramp_amp2 > 255){
    ramp_amp2 = 255;
  }else if (ramp_amp2 < 0){
    ramp_amp2 = 0;
  }
  
  if(SampleIdx2_laser2 == 900){
    SampleIdx2_laser2 = 0;
    amplitude2 = amplitude2 + amp_incr;
    amp_incr_cnt++;
    if (amp_incr_cnt == num_amp_steps){
      amplitude2 = amp_incr;
      amp_incr_cnt = 0;
      }
  }
  spiCommand(hspi, 0b00010000, ramp_amp2); // The slow relock waveform goes to port B  of the dig trimpot 1 (LD)
  spiCommand(vspi, 0b00010000, ramp_amp1); // The fast  relock waveform goes to port B  of the dig trimpot 2 (piezo)
  }



  if(ramp_gen_flag_laser2 == 1){ 
    ramp_amp = int(ramp10LookupTable[SampleIdx3_laser2++]*0.7+DC_offset2); // Going through the ramp values
    if(SampleIdx3_laser2 == 10){
      SampleIdx3_laser2 = 0;
      toogle_laser2 = !toogle_laser2;
      digitalWrite(osci_trigger_laser2, toogle_laser2); // Generating a sync trigger signal for the oscilloscope
    }
    if(ramp_amp > 255){
      ramp_amp = 255;
    }else if (ramp_amp < 0){
      ramp_amp = 0;
    }
    spiCommand(hspi, 0b00010000, ramp_amp); // The ramp waveform goes to port B of the dig trimpot 1 (LD)
 }
}

void timer1_setup(uint32_t interval) {
  // Initialize Timer 1
  
  // Attach the ISR function to Timer 1
  timerAttachInterrupt(timer, &timer1_ISR, true);

  // Set the timer to repeat every 'interval' microseconds
  timerAlarmWrite(timer, interval, true);

  
  timerAlarmEnable(timer);
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
String elp = "0";

int engage_relock_track_laser1 = 0; // This flag determines if the laser 1 lock status should be tracked. Does not imply outputing relock waveforms.
int engage_relock_track_laser2 = 0; // The same for laser 2
int engage_ramp_laser1 = 0; // Flag that determines if the ramp waveform should be outputed for laser 1 (if it has a value of 1) or not (if it has a value of 0). 
                            // Apart from this flag to be 1, for the ramp to be outputed on laser 1, the engage_relock_track_laser1 flag should be 0.
int engage_ramp_laser2 = 0; // The same for laser 2







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
    
    if (message.indexOf("1REL") >= 0){
      elp = message.substring(4);
      engage_relock_track_laser1 = elp.toInt();
      //Serial.println(switchStatus);  
      }

    if (message.indexOf("2REL") >= 0){
      elp = message.substring(4);
      engage_relock_track_laser2 = elp.toInt();
      //Serial.println(switchStatus);  
      }

    if (message.indexOf("1RAM") >= 0){
      elp = message.substring(4);
      engage_ramp_laser1 = elp.toInt();
      //Serial.println(switchStatus);  
      }

    if (message.indexOf("2RAM") >= 0){
      elp = message.substring(4);
      engage_ramp_laser2 = elp.toInt();
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
      //threshold_engage2 = map(sliderValue3.toInt(), 0, 3300, 0, 3.3);
      threshold_engage2 = sliderValue3.toInt();
      threshold_engage2 = threshold_engage2*Volt_to_number_10/1000;
      //Serial.println(threshold_engage2);
      //Serial.print(getSliderValues());
      notifyClients(getSliderValues());
    }
    if (message.indexOf("4s") >= 0) {
      sliderValue4 = message.substring(2);
      //threshold_disengage2 = map(sliderValue4.toInt(), 0, 3300, 0, 3.3);
      threshold_disengage2 = sliderValue4.toInt();
      threshold_disengage2 = threshold_disengage2*Volt_to_number_10/1000;
      //Serial.println(threshold_disengage2);
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

  pinMode(relay21, OUTPUT);
  pinMode(relay22, OUTPUT);
  pinMode(relay23, OUTPUT);
  pinMode(relay24, OUTPUT);

  pinMode(osci_trigger_laser2, OUTPUT);
  pinMode(osci_trigger_laser1, OUTPUT);
  //Initially all realys and outputs should be in the OFF state
  digitalWrite(relay11, HIGH);
  digitalWrite(relay12, HIGH);
  digitalWrite(relay13, HIGH);
  digitalWrite(relay14, HIGH);
  digitalWrite(relay21, HIGH);
  digitalWrite(relay22, HIGH);
  digitalWrite(relay23, HIGH);
  digitalWrite(relay24, HIGH);

  timer1_setup(10000); // 10 is 2ms interval; like this sine with 30 points is around 21Hz and the one with 900 points is around 0.5 Hz

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
  Value_laser2 = analogRead(ADC1_CH6);
  Value_laser2 = analogRead(ADC1_CH6);
  Value_laser2 = analogRead(ADC1_CH6);
  Value_laser1 = analogRead(ADC1_CH5);
  Value_laser1 = analogRead(ADC1_CH5);
  Value_laser1 = analogRead(ADC1_CH5);


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

if (engage_relock_track_laser1 == 1){ //Go to lock tracking only if the "Engage lock tracking laser 1" is ticked
  ramp_gen_flag_laser1 = 0;  // If the ramp was being generated for laser 1, stop it now
   
  if (Value_laser1 > threshold_engage1){
    timer_off_lock_laser1 = 0;
    
    re_lock_gen_flag_laser1 = 0; // Indicate to the timer interrupt routine to stop generating relock waveforms on laser 1
    // Order of engaging integrators matters!
    digitalWrite(relay12, LOW);  //laser 1, integrator piezo 2 (IP2)
    delay(20);
    digitalWrite(relay11, LOW);  //laser 1, integrator piezo 1 (IP1)
    delay(20);
    digitalWrite(relay13, LOW);  //laser 1, integrator LD 1 (IL1)
    delay(20);
    digitalWrite(relay14, LOW);  //laser 1, integrator LD 2 (IL2)
    delay(20);
    }
    
   if (Value_laser1 < threshold_disengage1){
    if (timer_off_lock_laser1 > 5000) { 
    digitalWrite(relay11, HIGH);
    digitalWrite(relay12, HIGH);
    digitalWrite(relay13, HIGH);
    digitalWrite(relay14, HIGH);
    re_lock_gen_flag_laser1 = 1; // Indicate to the timer interrupt routine to start generating relock waveforms on laser 1
    }
    timer_off_lock_laser1 = timer_off_lock_laser1 + millis();
    }
}else{
  // If "Engage lock tracking laser 1" is unticked, disengage all the relays
  digitalWrite(relay11, HIGH);
  digitalWrite(relay12, HIGH);
  digitalWrite(relay13, HIGH);
  digitalWrite(relay14, HIGH);
  re_lock_gen_flag_laser1 = 0; // Indicate to the timer interrupt routine to stop generating relock waveforms on laser 1
  if (engage_ramp_laser1 == 1){ // If the Engage ramp laser 1 is ticked, start generating the ramp on laser 1, now when the relock track laser 1 is off
    ramp_gen_flag_laser1 = 1; 
    }else{ // Otherwise, stop the ramp on laser 1
     ramp_gen_flag_laser1 = 0; }
  }

if (engage_relock_track_laser2 == 1){ //Go to lock tracking only if the "Engage lock tracking laser 2" is ticked
   ramp_gen_flag_laser2 = 0;  // If the ramp was being generated for laser 2, stop it now
   if (Value_laser2 > threshold_engage2){
    timer_off_lock_laser2 = 0;
    re_lock_gen_flag_laser2 = 0; // Stop generating relock waveforms
    // Order of engaging integrators matters!
    digitalWrite(relay22, LOW); //GPIO32, laser 2, integrator piezo 2 (IP2)
    delay(20);
    digitalWrite(relay21, LOW); //GPIO31, laser 2, integrator piezo 1 (IP1)
    delay(20);
    digitalWrite(relay23, LOW); //GPIO33, laser 2, integrator LD 1 (IL1)
    delay(20);
    digitalWrite(relay24, LOW); //GPIO34, laser 2, integrator LD 2 (IL2)
    delay(20);
    }
    
   if (Value_laser2 < threshold_disengage2){
    if (timer_off_lock_laser2 > 5000) { 
    digitalWrite(relay21, HIGH);
    digitalWrite(relay22, HIGH);
    digitalWrite(relay23, HIGH);
    digitalWrite(relay24, HIGH);
    re_lock_gen_flag_laser2 = 1; // Start generating relock waveforms
    }
    timer_off_lock_laser2 = timer_off_lock_laser2 + millis();
    }
}else{
  // If "Engage lock tracking laser 2" is unticked, disengage all the relays
  digitalWrite(relay21, HIGH);
  digitalWrite(relay22, HIGH);
  digitalWrite(relay23, HIGH);
  digitalWrite(relay24, HIGH);
  re_lock_gen_flag_laser2 = 0; // Stop generating relock waveforms
  if (engage_ramp_laser2 == 1){ // If the Engage ramp laser 2 is ticked, engage the ramp on laser 2, now when the relock track laser 2 is off
    ramp_gen_flag_laser2 = 1;
    }else{
      ramp_gen_flag_laser2 = 0; // Otherwise, switch off the ramp on laser 2
      }
  }
  ws.cleanupClients();
  //delay(200);
}
