// Testing on old RevA hardware

 
#include "SdFat.h" // https://github.com/greiman/SdFat
#include <Wire.h>  // built in library, for I2C communications
#include "SSD1306Ascii.h" // https://github.com/greiman/SSD1306Ascii
#include "SSD1306AsciiWire.h" // https://github.com/greiman/SSD1306Ascii
#include <SPI.h>  // built in library, for SPI communications
#include "RTClib.h" // https://github.com/millerlp/RTClib
#include <EEPROM.h> // built in library, for reading the serial number stored in EEPROM
#include "MusselGapeTrackerlib.h" // https://github.com/millerlp/MusselGapeTrackerlib

#define REDLED 4    // Red error LED pin
#define GRNLED A3   // Green LED pin
#define ANALOG_IN A6 // battery monitor on ADC6


void setup() {
    // Set up the LEDs as output
  pinMode(REDLED,OUTPUT);
  digitalWrite(REDLED, HIGH);
  pinMode(GRNLED,OUTPUT);
  digitalWrite(GRNLED, HIGH);
  delay(150);
  digitalWrite(REDLED, LOW);
  digitalWrite(GRNLED, LOW);

  Serial.begin(57600);
  Serial.println(F("Hello"));
  delay(10);

}

void loop() {
  unsigned int myread = analogRead(ANALOG_IN);
  Serial.println(myread);
  delay(250);
  
  

}
