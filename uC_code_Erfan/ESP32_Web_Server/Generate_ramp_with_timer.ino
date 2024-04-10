#include <Arduino.h>
int dacpin1 = DAC1;

float amplitude = 0.1;
float DC_offset = 0.0;
hw_timer_t *timer = timerBegin(1, 80, true);  // Timer 1, prescaler 80, count up

// Sine LookUpTable & Index Variable
uint8_t SampleIdx = 0;
const uint8_t sineLookupTable[] = {
128, 136, 143, 151, 159, 167, 174, 182,
189, 196, 202, 209, 215, 220, 226, 231,
235, 239, 243, 246, 249, 251, 253, 254,
255, 255, 255, 254, 253, 251, 249, 246,
243, 239, 235, 231, 226, 220, 215, 209,
202, 196, 189, 182, 174, 167, 159, 151,
143, 136, 128, 119, 112, 104, 96, 88,
81, 73, 66, 59, 53, 46, 40, 35,
29, 24, 20, 16, 12, 9, 6, 4,
2, 1, 0, 0, 0, 1, 2, 4,
6, 9, 12, 16, 20, 24, 29, 35,
40, 46, 53, 59, 66, 73, 81, 88,
96, 104, 112, 119};

const uint8_t rampLookupTable[] = {
  127, 114, 102,  89,  76,  63,  50,  38,  25,  12,   0,   3,   6,
         9,  12,  15,  19,  22,  25,  28,  31,  35,  38,  41,  44,  47,
        51,  54,  57,  60,  63,  66,  70,  73,  76,  79,  82,  86,  89,
        92,  95,  98, 102, 105, 108, 111, 114, 117, 121, 124, 127, 130,
       133, 137, 140, 143, 146, 149, 153, 156, 159, 162, 165, 168, 172,
       175, 178, 181, 184, 188, 191, 194, 197, 200, 204, 207, 210, 213,
       216, 219, 223, 226, 229, 232, 235, 239, 242, 245, 248, 251, 255,
       242, 229, 216, 204, 191, 178, 165, 153, 140
};

// Function prototypes
void timer1_setup(uint32_t interval);
void update_amp(float new_amp);
void update_DC_offset(float new_DC_offset);
void update_period(int new_period);



void IRAM_ATTR timer1_ISR() {
// Send SineTable Values To DAC One By One
  //dacWrite(dacpin1, sineLookupTable[SampleIdx++]);
  int ramp_amp;
  ramp_amp = (rampLookupTable[SampleIdx++]*amplitude+DC_offset);
  if(ramp_amp >= 255)
  {
    ramp_amp = 255;
  }
  if(ramp_amp <= 0)
  {
    ramp_amp = 0;
  }
  dacWrite(dacpin1, ramp_amp);
  if(SampleIdx == 100)
  {
    SampleIdx = 0;
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


void update_amp(float new_amp){
  // Function to update the amplitude of the output wave
  // The new_amp goes from 0 to 255 so it is normalized to have values from 0 to 1, corresponding to 0 to 3.3 Volts DAC output
  amplitude = new_amp/255;
  }

void  update_DC_offset(float new_DC_offset){
  // Function to update the DC_offset of the output wave
  // The new_amp goes from 0 to 255 so it is normalized to have values from 0 to 1, corresponding to 0 to 3.3 Volts DAC output
  DC_offset = new_DC_offset;
  }
  

void update_period(int new_period){
  // Function to update the period of timer alarms
  timerAlarmDisable(timer);
  int interval = 50 + new_period; // 10 cycles stands for 1 ms period if one ramp period has 100 points
  timerAlarmWrite(timer, interval, true);
  timerAlarmEnable(timer);
  }
