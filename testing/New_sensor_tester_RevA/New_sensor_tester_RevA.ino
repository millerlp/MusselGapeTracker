/* New_sensor_tester_RevA
 *  Luke Miller 2023
 *  A test program for newly assembled Allegro A1395 Hall effect
 *  sensors, using an old MusselGapeTracker_RevA board. Plug the 
 *  sensor into the Hall0 port on the board. 
 *  
 *  The shift registers used to activate sleep lines don't appear
 *  to work on this early revision, so I am bypassing them and
 *  just wiring the board so that the sleep line is always pulled
 *  high (awake) on Hall 0. 

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

#define ERRLED 4    // Red error LED pin
#define GREENLED A3    // Green LED pin
#define BUTTON1 2     // BUTTON1 on INT0, pin PD2
//#define TRIGGER 3  // Trigger on syringe dispenser, pin D3 on RevC
#define ANALOG_IN A0  // Hall effect analog input from multiplexer, pin ADC0 on Rev A (A7 on newer Revs)
#define CS_SD 10    // Chip select for SD card, pin PB2
#define CS_SHIFT_REG A2 // Chip select for shift registers, pin PC2
#define SHIFT_CLEAR A1  // Clear (erase) line for shift registers, pin PC1

#define MUX_S0  9   // Multiplexer channel select line, pin PB1
#define MUX_S1  5   // Multiplexer channel select line, pin PD5
#define MUX_S2  6   // Multiplexer channel select line, pin PD6
#define MUX_S3  7   // Multiplexer channel select line, pin PD7
#define MUX_EN  8   // Multiplexer enable line, pin PB0
//*************************************



//******************************************
// 0X3C+SA0 - 0x3C or 0x3D for OLED screen on I2C bus
#define I2C_ADDRESS1 0x3C   // Typical default address
SSD1306AsciiWire oled1; // create OLED display object, using I2C Wire
//******************************************


unsigned long prevMillis;  // counter for faster operations
unsigned long newMillis;  // counter for faster operations

byte channel = 0; // keep track of current hall channel

//******************
// Create ShiftReg and Mux objects
ShiftReg shiftReg;
Mux mux;
unsigned int newReading = 0; // Reading from Hall effect sensor analog input


void setup() {
    // Set button1 as an input
  pinMode(BUTTON1, INPUT_PULLUP);

  // Set up the LEDs as output
  pinMode(ERRLED,OUTPUT);
  digitalWrite(ERRLED, LOW);
  pinMode(GREENLED,OUTPUT);
  digitalWrite(GREENLED, LOW);

  // Set up pins for Hall effect sensor
  pinMode(A2, OUTPUT); // sleep line for hall effect 1
  digitalWrite(A2, LOW);
  pinMode(ANALOG_IN, INPUT); // Read the Hall effect sensor on this input
  // Initialize the shift register object
//  shiftReg.begin(CS_SHIFT_REG, SHIFT_CLEAR);
  // Initialize the multiplexer object
  mux.begin(MUX_EN,MUX_S0,MUX_S1,MUX_S2,MUX_S3);

  Serial.begin(57600);
  Serial.println("Hello");


  //----------------------------------
  // Start up the oled display
  oled1.begin(&Adafruit128x64, I2C_ADDRESS1);
  Wire.setClock(400000L); //  oled1.set400kHz();  
  oled1.setFont(Adafruit5x7);    
  oled1.clear(); 
  oled1.home();
  oled1.set2X();
  oled1.print(F("Hello"));
  // Initialize the real time clock DS3231M
//  Wire.begin(); // Start the I2C library with default options
//  rtc.begin();  // Start the rtc object with default options
//  newtime = rtc.now(); // read a time from the real time clock
//  newtime.toString(buf, 20); 
  // Now extract the time by making another character pointer that
  // is advanced 10 places into buf to skip over the date. 
//  char *timebuf = buf + 10;
//  for (int i = 0; i<11; i++){
//    oled1.print(buf[i]);
//  }
//  oled1.println();
//  oled1.println(timebuf);


//  delay(2500);
  //***********************************************
  // Check that real time clock has a reasonable time value
  bool stallFlag = true; // Used in error handling below
  

//******************************************
  // Have the user choose which channel to calibrate
  stallFlag = true;

  // Update oled
//  oled1.home();
//  oled1.clear();
//  oled1.set1X();
//  oled1.println(F("Press button 1"));
//  oled1.println(F("to choose channel"));
//  oled1.println(F("Long press = start"));
//  oled1.set2X();
//  Serial.println(F("Press button 1 to choose channel"));
//  Serial.println(F("Long press = start"));

  long cycleMillis = millis();
  Serial.println(cycleMillis);
  delay(250);
  Serial.println(millis());

  while (stallFlag){
    // The program will just cycle here endlessly
    // Take Hall reading and update oled very 200 ms
    if (millis()-cycleMillis > 200){
      cycleMillis = millis(); // update
      // Choose which channel to sample based on channel value
      // and take a reading to display
      digitalWrite(MUX_EN, LOW); // enable multiplexer
      delayMicroseconds(2);
      
//      shiftReg.shiftChannelSet(channel);
      
      mux.muxChannelSet(channel);
      
      newReading = readHall(ANALOG_IN);
      digitalWrite(MUX_EN, HIGH); // disable multiplexer

      if ( (newReading > 25 ) & (newReading < 800) ) {
        digitalWrite(ERRLED, LOW); // Make sure error led is off
        digitalWrite(GREENLED, !digitalRead(GREENLED));
      } else {
        digitalWrite(GREENLED, LOW); // Turn off green
        digitalWrite(ERRLED, !digitalRead(ERRLED));
      }
  
      oled1.setCursor(0,3); // Column 0, row 3 (8-pixel rows)
      oled1.clear(0,128,3,4);
      oled1.print(F("Ch"));
      oled1.print(channel);
      oled1.print(F(": "));
      oled1.print(newReading); // current Hall value
      Serial.print(F("Ch"));
      Serial.print(channel);
      Serial.print(F(": "));
      Serial.println(newReading);
    }
  } // end of while(stallFlag) loop

} // end of setup loop ************************************

void loop() {

  // Nothing happens here, we never arrive here
}
