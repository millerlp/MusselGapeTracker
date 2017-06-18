/*
 *  SD_RTC_OLED_test
 *  
 *  
 *  Simple test for OLED screen on MusselGapeTracker RevA hardware,
 *  along with checking functionality of the Real Time Clock and
 *  SD card. 
 *  
 */

#include "SdFat.h" // https://github.com/greiman/SdFat
#include <Wire.h>  // built in library, for I2C communications
#include "SSD1306Ascii.h" // https://github.com/greiman/SSD1306Ascii
#include "SSD1306AsciiWire.h" // https://github.com/greiman/SSD1306Ascii
#include <SPI.h>  // built in library, for SPI communications
#include "RTClib.h" // https://github.com/millerlp/RTClib

#define REDLED 4    // Red error LED pin
#define GRNLED A3   // Green LED pin
#define BUTTON1 2     // BUTTON1 on INT0, pin PD2
#define BUTTON2 3     // BUTTON2 on INT1, pin PD3

//******************************************
// 0X3C+SA0 - 0x3C or 0x3D for OLED screen on I2C bus
#define I2C_ADDRESS1 0x3C
//#define I2C_ADDRESS2 0x3D

SSD1306AsciiWire oled1; // create OLED display object, using I2C Wire

bool sdErrorFlag = false;
bool rtcErrorFlag = false;
//*************
// Create real time clock object
RTC_DS3231 rtc;
DateTime newtime;
char buf[20]; // declare a string buffer to hold the time result

//*************
// Create sd card objects
SdFat sd;
SdFile logfile;  // for sd card, this is the file object to be written to
const byte CS_SD = 10; // define the Chip Select pin for SD card


void setup() {
    // Set button1 as an input
  pinMode(BUTTON1, INPUT_PULLUP);
  // Set button2 as an input
  pinMode(BUTTON2, INPUT_PULLUP);
    // Set up the LEDs as output
  pinMode(REDLED,OUTPUT);
  digitalWrite(REDLED, LOW);
  pinMode(GRNLED,OUTPUT);
  digitalWrite(GRNLED, LOW);
  Serial.begin(57600);
  Serial.println(F("Hello"));
  Serial.println();


  //----------------------------------
  // Start up the oled displays
  oled1.begin(&Adafruit128x64, I2C_ADDRESS1);
  oled1.set400kHz();  
  oled1.setFont(Adafruit5x7);    
  oled1.clear(); 
  oled1.print(F("Hello"));
  
  // Initialize the real time clock DS3231M
  Wire.begin(); // Start the I2C library with default options
  rtc.begin();  // Start the rtc object with default options
  newtime = rtc.now(); // read a time from the real time clock
  newtime.toString(buf, 20); 
  // Now extract the time by making another character pointer that
  // is advanced 10 places into buf to skip over the date. 
  char *timebuf = buf + 10;
  oled1.println();
  oled1.set2X();
  for (int i = 0; i<11; i++){
    oled1.print(buf[i]);
  }
  oled1.println();
  oled1.print(timebuf);
  delay(3000);
  //***********************************************
  // Check that real time clock has a reasonable time value
  bool stallFlag = true; // Used in error handling below
  if ( (newtime.year() < 2017) | (newtime.year() > 2035) ) {
    // There is an error with the clock, halt everything
    oled1.home();
    oled1.clear();
    oled1.set1X();
    oled1.println(F("RTC ERROR"));
    oled1.println(buf);
    oled1.set2X();
    oled1.println();
    oled1.println(F("Continue?"));
    oled1.println(F("Press 1"));
    Serial.println(F("Clock error, press BUTTON1 on board to continue"));

    rtcErrorFlag = true;
    // Consider removing this while loop and allowing user to plow
    // ahead without rtc (use button input?)
    while(stallFlag){
    // Flash the error led to notify the user
    // This permanently halts execution, no data will be collected
      digitalWrite(REDLED, !digitalRead(REDLED));
      delay(100);
      if (digitalRead(BUTTON1) == LOW){
        delay(40);  // debounce pause
        if (digitalRead(BUTTON1) == LOW){
          // If button is still low 40ms later, this is a real press
          // Now wait for button to go high again
          while(digitalRead(BUTTON1) == LOW) {;} // do nothing
          stallFlag = false; // break out of while(stallFlag) loop
          oled1.home();
          oled1.clear();
        } 
      }              
    } // end of while(stallFlag)
  } else {
    oled1.home();
    oled1.clear();
    oled1.set2X();
    oled1.println(F("RTC OKAY"));
    Serial.println(F("Clock okay"));
    Serial.println(buf);
  } // end of if ( (newtime.year() < 2017) | (newtime.year() > 2035) ) {

  //*************************************************************
  // SD card setup and read (assumes Serial output is functional already)
  bool sdErrorFlag = false;
  pinMode(CS_SD, OUTPUT);  // set chip select pin for SD card to output
  // Initialize the SD card object
  // Try SPI_FULL_SPEED, or SPI_HALF_SPEED if full speed produces
  // errors on a breadboard setup. 
  if (!sd.begin(CS_SD, SPI_FULL_SPEED)) {
  // If the above statement returns FALSE after trying to 
  // initialize the card, enter into this section and
  // hold in an infinite loop.
    // There is an error with the SD card, halt everything
    oled1.home();
    oled1.clear();
    oled1.println(F("SD ERROR"));
    oled1.println();
    oled1.println(F("Continue?"));
    oled1.println(F("Press 1"));
    Serial.println(F("SD error, press BUTTON1 on board to continue"));

    sdErrorFlag = true;
    stallFlag = true; // set true when there is an error
    while(stallFlag){ // loop due to SD card initialization error
      digitalWrite(REDLED, HIGH);
      delay(100);
      digitalWrite(REDLED, LOW);
      digitalWrite(GRNLED, HIGH);
      delay(100);
      digitalWrite(GRNLED, LOW);

      if (digitalRead(BUTTON1) == LOW){
        delay(40);  // debounce pause
        if (digitalRead(BUTTON1) == LOW){
          // If button is still low 40ms later, this is a real press
          // Now wait for button to go high again
          while(digitalRead(BUTTON1) == LOW) {;} // do nothing
          stallFlag = false; // break out of while(stallFlag) loop
        } 
      }              
    }
  }  else {
    oled1.println(F("SD OKAY"));
    Serial.println(F("SD OKAY"));
  }  // end of (!sd.begin(chipSelect, SPI_FULL_SPEED))

  // Give accounting of bootup successes and failures
  oled1.home();
  oled1.clear();
  oled1.set2X();
  oled1.print(F("RTC "));
  Serial.print(F("RTC "));
  if (rtcErrorFlag){
    oled1.println(F("ERROR"));
    Serial.println(F("ERROR"));
  } else {
    oled1.println(F("OKAY"));
    Serial.println(F("OKAY"));
  }
  oled1.print(F("SD "));
  Serial.print(F("SD "));
  if (sdErrorFlag){
    oled1.println(F("ERROR"));
    Serial.println(F("ERROR"));
  } else {
    oled1.println(F("OKAY"));
    Serial.println(F("OKAY"));
  }
  oled1.println(F("Finished"));
  oled1.println(F("boot"));
  Serial.println(F("Finished boot"));
}

void loop() {
// Nothing happens here

}
