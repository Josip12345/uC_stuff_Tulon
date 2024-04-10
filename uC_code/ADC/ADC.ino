const int ADC1_CH2 = 8; // PD laser 1
const int ADC1_CH4 = 10; // PD laser 2

// Relay set pin
const int relay11 = 19; //GPIO15
const int relay12 = 21; //GPIO14
const int relay13 = 33; //GPIO29
const int relay14 = 34; //GPIO30

const int relay21 = 35; //GPIO31
const int relay22 = 36; //GPIO32
const int relay23 = 37; //GPIO33
const int relay24 = 38; //GPIO34

// variable for storing the potentiometer value
int Value_laser1 = 0;
int Value_laser2 = 0;
int Volt_to_number_8 = 2600.0;
int Volt_to_number_10 = 3100.0;

int threshold_engage1 = Volt_to_number_8*1.0; // Integrators engaged above PD value of 1 V
int threshold_disengage1 = Volt_to_number_8*0.8; // Integrators disengaged below a PD value of 0.8 V
                                               // Like this we have needed hysteresis


int threshold_engage2 = Volt_to_number_10*1.5; // Integrators engaged above PD value of 1.5 V
int threshold_disengage2 = Volt_to_number_10*1.2; // Integrators disengaged below a PD value of 1.2 V
                                               // Like this we have needed hysteres                                               

void setup() {
  Serial.begin(115200);
  pinMode(relay11, OUTPUT);
  pinMode(relay12, OUTPUT);
  pinMode(relay13, OUTPUT);
  pinMode(relay14, OUTPUT);

  pinMode(relay21, OUTPUT);
  pinMode(relay22, OUTPUT);
  pinMode(relay23, OUTPUT);
  pinMode(relay24, OUTPUT);
  
  delay(1000);
}



void loop() {
  // Reading potentiometer value
  Value_laser1 = analogRead(ADC1_CH2);
  Value_laser2 = analogRead(ADC1_CH4);
  Serial.println(Value_laser1);
  Serial.println(Value_laser2);
  if (Value_laser1 > threshold_engage1){
    digitalWrite(relay11, LOW);
    digitalWrite(relay12, LOW);
    digitalWrite(relay13, LOW);
    digitalWrite(relay14, LOW);
    }
   else if (Value_laser1 < threshold_disengage1){
    digitalWrite(relay11, HIGH);
    digitalWrite(relay12, HIGH);
    digitalWrite(relay13, HIGH);
    digitalWrite(relay14, HIGH);
    }
    
   if (Value_laser2 > threshold_engage2){
    digitalWrite(relay21, LOW);
    digitalWrite(relay22, LOW);
    digitalWrite(relay23, LOW);
    digitalWrite(relay24, LOW);
    }
   else if (Value_laser2 < threshold_disengage2){
    digitalWrite(relay21, HIGH);
    digitalWrite(relay22, HIGH);
    digitalWrite(relay23, HIGH);
    digitalWrite(relay24, HIGH);
    }
    

     
  delay(500);
}
