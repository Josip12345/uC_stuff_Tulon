

/* The ESP32 has four SPi buses, however as of right now only two of
 * them are available to use, HSPI and VSPI. Simply using the SPI API 
 * as illustrated in Arduino examples will use VSPI, leaving HSPI unused.
 * 
 * However if we simply intialise two instance of the SPI class for both
 * of these buses both can be used. However when just using these the Arduino
 * way only will actually be outputting at a time.
 * 
 * Logic analyser capture is in the same folder as this example as
 * "multiple_bus_output.png"
 * 
 * created 30/04/2018 by Alistair Symonds
 */
#include <SPI.h>
#include <Arduino.h>

// Define ALTERNATE_PINS to use non-standard GPIO pins for SPI bus


#define VSPI_MISO   21 //SDO
#define VSPI_MOSI   47 //SDI
#define VSPI_SCLK   48
#define VSPI_SS     45


const int sine30LookupTable[] = { // Lookup table for the fast sine function
   128, 155, 181, 205, 225, 241, 251, 255, 254, 246, 233, 216, 193,
       168, 141, 114,  87,  62,  39,  22,   9,   1,   0,   4,  14,  30,
        50,  74, 100, 127};

int i=0;

#if CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3
#define VSPI FSPI
#endif

static const int spiClk = 10000000; // 10 MHz

//uninitalised pointers to SPI objects
SPIClass * vspi = NULL;
SPIClass * hspi = NULL;

void setup() {
  //initialise two instances of the SPIClass attached to VSPI and HSPI respectively
  vspi = new SPIClass(VSPI);
  hspi = new SPIClass(HSPI);
  Serial.begin(115200);
  

// route through GPIO pins of your choice
vspi->begin(VSPI_SCLK, VSPI_MISO, VSPI_MOSI, VSPI_SS); //SCLK, MISO, MOSI, SS


  //set up slave select pins as outputs as the Arduino API
  //doesn't handle automatically pulling SS low
  pinMode(vspi->pinSS(), OUTPUT); //VSPI SS


}

// the loop function runs over and over again until power down or reset
void loop() {
  //while (i<10){
  //use the SPI buses
  //Serial.println(sine30LookupTable[i++]);
  spiCommand(vspi, 00, sine30LookupTable[i++]*0.1);
  if (i==30){
    i=0;
    }; 
  //spiCommand(hspi, 0b11001100);
  delay(5);
  //i++;
  //}
}

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
