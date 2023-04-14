/* Sensor_display_python
 *  A program to stream the reading from 3 Hall sensors over
 *  serial to a Python plotting program
 *  Luke Miller 2023
 *  Designed for Rev C/E GapeTracker boards
 *  
 */


//#include "SdFat.h" // https://github.com/greiman/SdFat
#include <Wire.h>  // built in library, for I2C communications
#include "SSD1306Ascii.h" // https://github.com/greiman/SSD1306Ascii
#include "SSD1306AsciiWire.h" // https://github.com/greiman/SSD1306Ascii
#include <SPI.h>  // built in library, for SPI communications
//#include "RTClib.h" // https://github.com/millerlp/RTClib
//#include <EEPROM.h> // built in library, for reading the serial number stored in EEPROM
#include "MusselGapeTrackerlib.h" // https://github.com/millerlp/MusselGapeTrackerlib

//RTC_DS3231 rtc; // Create real time clock object 

#define REDLED 4    // Red error LED pin
#define GRNLED A3   // Green LED pin
#define BUTTON1 2     // BUTTON1 on INT0, pin PD2
#define BUTTON2 3     // BUTTON2 on INT1, pin PD3
#define CS_SD 10    // Chip select for SD card (if used)
#define CS_SHIFT_REG A2 // Chip select for shift registers
#define SHIFT_CLEAR A1  // Clear (erase) line for shift registers
#define ANALOG_IN A7  // Hall effect analog input from multiplexer

#define MUX_S0  9   // Multiplexer channel select line
#define MUX_S1  5   // Multiplexer channel select line
#define MUX_S2  6   // Multiplexer channel select line
#define MUX_S3  7   // Multiplexer channel select line
#define MUX_EN  8   // Multiplexer enable line
//*************************************

//******************************************
// 0X3C+SA0 - 0x3C or 0x3D for OLED screen on I2C bus
#define I2C_ADDRESS1 0x3C   // Typical default address
SSD1306AsciiWire oled1; // create OLED display object, using I2C Wire
//******************************************


unsigned long prevMillis;  // counter for faster operations
unsigned long newMillis;  // counter for faster operations

uint8_t channel = 0; // keep track of current hall channel

//******************
// Create ShiftReg and Mux objects
ShiftReg shiftReg;
Mux mux;
unsigned int newReading = 0; // Reading from Hall effect sensor analog input
unsigned int hallAverages[16]; // array to hold each second's sample averages

unsigned int val1 = 0;
unsigned int val2 = 0;
unsigned int val3 = 0;

//
//void sendToPC(unsigned int* data1) {
//  byte* byteData = (byte*)(data1);
//  Serial.write(byteData,2);
//}

void sendToPC(unsigned int* data1, unsigned int* data2, unsigned int* data3)
{
  byte* byteData1 = (byte*)(data1);
  byte* byteData2 = (byte*)(data2);
  byte* byteData3 = (byte*)(data3);
  byte buf[6] = {byteData1[0], byteData1[1],
                 byteData2[0], byteData2[1], 
                 byteData3[0], byteData3[1]};
  Serial.write(buf, 6);
}



void setup() {
    // Set button1 as an input
  pinMode(BUTTON1, INPUT_PULLUP);

  // Set up the LEDs as output
  pinMode(REDLED,OUTPUT);
  digitalWrite(REDLED, LOW);
  pinMode(GRNLED,OUTPUT);
  digitalWrite(GRNLED, LOW);

  Serial.begin(57600);
//  Serial.println("Hello");
  
  SPI.begin(); // Need this to make the shift register run
  // Initialize the shift register object
  shiftReg.begin(CS_SHIFT_REG, SHIFT_CLEAR);
  // Initialize the multiplexer object
  mux.begin(MUX_EN,MUX_S0,MUX_S1,MUX_S2,MUX_S3);
  //----------------------------------
  // Start up the oled display
  oled1.begin(&Adafruit128x64, I2C_ADDRESS1);
  Wire.setClock(400000L); //  oled1.set400kHz();  
  oled1.setFont(Adafruit5x7);    
  oled1.clear(); 
  oled1.home();
  oled1.set2X();
  oled1.print(F("Hello"));
  delay(1000);
  // Prep the display by printing channel numbers
  oled1.clear();
  oled1.home();
  oled1.setCursor(0,0);
  oled1.print(F("Ch0: "));
  oled1.setCursor(0,3);
  oled1.print(F("Ch1: "));
  oled1.setCursor(0,6);
  oled1.print(F("Ch2: "));
 
  //***********************************************
  bool stallFlag = true; // Used in error handling below
  long cycleMillis = millis();
  long LEDMillis = millis();
  
  while (stallFlag){
    // The program will just cycle here endlessly
    // Take Hall reading and update oled
    if (millis()-cycleMillis > 50){
      cycleMillis = millis(); // update

      read16Hall(ANALOG_IN, hallAverages, shiftReg, mux, MUX_EN);
      newReading = hallAverages[channel];

      oled1.clear(60,128,0,1);
      oled1.setCursor(60,0); // Column 60, row 0 (8-pixel rows)
      oled1.print(hallAverages[0]); // current Hall value
      
      oled1.clear(60,128,3,4);
      oled1.setCursor(60,3); // Column 60, row 3 (8-pixel rows)
      oled1.print(hallAverages[1]); // current Hall value

      oled1.clear(60,128,6,7);
      oled1.setCursor(60,6); // Column 60, row 6 (8-pixel rows)
      oled1.print(hallAverages[2]); // current Hall value

//      Serial.print(F("Ch"));
//      Serial.print(channel);
//      Serial.print(F(": "));
////      Serial.println(newReading);
//      Serial.println(hallAverages[channel]);
      val1 = hallAverages[0];
      val2 = hallAverages[1];
      val3 = hallAverages[2];
      sendToPC(&val1, &val2, &val3);
      
      digitalWrite(GRNLED, !digitalRead(GRNLED));
    } // end of if(millis()-cycleMillis statement
  } // end of while(stallFlag) loop
} // end of setup loop ************************************

void loop() {

  // Nothing happens here, we never arrive here
}
