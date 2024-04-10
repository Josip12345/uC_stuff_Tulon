#include <Arduino.h>
int dacpin1 = DAC1; // pin 17 on ESP
int dacpin2 = DAC2; // pin 18 on ESP
int input_re_lock_flag = 39; // This (slave) uC listens to this pin and when it goes high it starts generating re-loking wavefroms. When the pin is low it does not generate re-lock wavefroms.

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
  
  if(digitalRead(input_re_lock_flag) == HIGH){ //If master uC sets the re-lock flag to high, only then output the re-lock waveforms
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
}


void timer1_setup(uint32_t interval) {
  // Initialize Timer 1
  
  // Attach the ISR function to Timer 1
  timerAttachInterrupt(timer, &timer1_ISR, true);

  // Set the timer to repeat every 'interval' microseconds
  timerAlarmWrite(timer, interval, true);

  // Enable the timer
  timerAlarmEnable(timer);
}






void setup() {
  Serial.begin(115200);
  // Initialize Timer 1 
  pinMode(input_re_lock_flag, INPUT);
  timer1_setup(2000);// 10 is 2ms interval; like this sine with 30 points is around 21Hz and the one with 900 points is around 0.5 Hz
}

void loop() {
}
