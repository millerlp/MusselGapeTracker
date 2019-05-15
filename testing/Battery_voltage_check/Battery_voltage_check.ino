/*
 * Battery_voltage_check.ino
 * 
 * 
 * For MusselGapeTracker RevC hardware
 * 
 * Pin ADC6 is the battery voltage sense pin (analog input)
 * Pin ADC0 is used as a digital output to control the battery voltage
 * check circuitry. (BATT_MONITOR_EN on the Eagle schematic)
 * 
 * The RevA hardware is initially spec'd with a 10k and 15k resistor
 * divider, so that the input voltage will be divided by 2.5. The system
 * voltage should be 3.3V from the regulator. 
 * 
 */
#define BATT_MONITOR A6  // analog input channel to sense battery voltage
#define BATT_MONITOR_EN A0 // digital output channel to turn on battery voltage check


#include <Wire.h>  // built in library, for I2C communications
#include "SSD1306Ascii.h" // https://github.com/greiman/SSD1306Ascii
#include "SSD1306AsciiWire.h" // https://github.com/greiman/SSD1306Ascii


//******************************************
// 0X3C+SA0 - 0x3C or 0x3D for OLED screen on I2C bus
#define I2C_ADDRESS1 0x3C
//#define I2C_ADDRESS2 0x3D

SSD1306AsciiWire oled1; // create OLED display object, using I2C Wire

unsigned long oldMillis;
int loopTime = 1000; // time between voltage readings

float dividerRatio = 2.5; // Ratio of voltage divider (15k + 10k) / 10k = 2.5
float refVoltage = 3.205; // Voltage from voltage regulator running ATmega


void setup() {
  pinMode(BATT_MONITOR, INPUT);
  pinMode(BATT_MONITOR_EN, OUTPUT); 
  digitalWrite(BATT_MONITOR_EN, LOW); // disable battery check initially

  //----------------------------------
  // Start up the oled display
  oled1.begin(&Adafruit128x64, I2C_ADDRESS1);
  oled1.set400kHz();  
  oled1.setFont(Adafruit5x7);    
  oled1.clear(); 
  oled1.set2X();
  oled1.println(F("Voltage:"));
  oldMillis = millis();
}

void loop() {
  if ( (millis() - oldMillis) > loopTime) {
    oldMillis = millis();
    // Check battery voltage
    // 1. Enable battery check circuit
    digitalWrite(BATT_MONITOR_EN, HIGH);
    delay(1);
    // 2. Read the analog input pin
    unsigned int rawAnalog = 0;
    analogRead(BATT_MONITOR);
    delay(3); // Give the ADC time to stablize
    for (byte i = 0; i<4; i++){
        rawAnalog = rawAnalog + analogRead(BATT_MONITOR);
        delay(2);
      }
      // Do a 2-bit right shift to divide rawAnalog
      // by 4 to get the average of the 4 readings
      rawAnalog = rawAnalog >> 2;

    // 3. Shut off the battery voltage sense circuit
//    digitalWrite(BATT_MONITOR_EN, LOW);
    // 4. Convert the rawAnalog count value (0-1024) into a voltage
    float reading = rawAnalog * dividerRatio * refVoltage / 1024;
    // 5. Update OLED screen
    oled1.setCursor(0,2); // (column, row)
    oled1.clearToEOL();
    oled1.println(reading);
    oled1.setCursor(0,4);
    oled1.clearToEOL();
    oled1.println(oldMillis);
    oled1.setCursor(0,6); // (column, row)
    oled1.clearToEOL();
    oled1.print(F("Raw:"));
    oled1.println(rawAnalog);
    
  } // end of if ( (millis() - oldMillis) > loopTime) {

}
