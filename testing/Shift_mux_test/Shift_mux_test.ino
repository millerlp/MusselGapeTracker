/*
 * Shift_mux_test.ino
 * 
 * Basic test sketch for interfacing with the 
 * SN74HC595D shift registers and 
 * CD74HC4067M96 16-channel multiplexer
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
#define CS_SD 10    // Chip select for SD card (if used)
#define CS_SHIFT_REG A2 // Chip select for shift registers
#define SHIFT_CLEAR A1  // Clear (erase) line for shift registers
#define ANALOG_IN A0  // Hall effect analog input from multiplexer

#define MUX_S0  9   // Multiplexer channel select line
#define MUX_S1  5   // Multiplexer channel select line
#define MUX_S2  6   // Multiplexer channel select line
#define MUX_S3  7   // Multiplexer channel select line
#define MUX_EN  8   // Multiplexer enable line

//******************************************
// 0X3C+SA0 - 0x3C or 0x3D for OLED screen on I2C bus
#define I2C_ADDRESS1 0x3C   // Typical default address
//#define I2C_ADDRESS2 0x3D // Alternate address, after moving resistor on OLED

SSD1306AsciiWire oled1; // create OLED display object, using I2C Wire

//*************
// Create sd card objects
SdFat sd;
SdFile logfile;  // for sd card, this is the file object to be written to

bool sdErrorFlag = false;
bool rtcErrorFlag = false;
//*************
// Create real time clock object
RTC_DS3231 rtc;
DateTime newtime;
char buf[20]; // declare a string buffer to hold the time result

unsigned long oldMillis;
unsigned int loopInterval = 1000;
byte HallChannel = 0;
unsigned int shiftChannel = 0;

//********************************************************
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

  pinMode(CS_SHIFT_REG, OUTPUT);
  pinMode(SHIFT_CLEAR, OUTPUT);
  // To use SHIFT_CLEAR, set it low, then pull CS_SHIFT_REG high,
  // and then set SHIFT_CLEAR high. 
  // Initially clear the shift registers to put all hall 
  // effect chips to sleep (they sleep when their sleep pin
  // is pulled low). Do this by pulling SHIFT_CLEAR low
  // while CS_SHIFT_REG is low, then send CS_SHIFT_REG high
  // to trigger the clear. 
  digitalWrite(SHIFT_CLEAR, HIGH);
  digitalWrite(CS_SHIFT_REG, LOW);
  digitalWrite(SHIFT_CLEAR, LOW);
  digitalWrite(CS_SHIFT_REG, HIGH);
  digitalWrite(SHIFT_CLEAR, HIGH); // reset high
  
  pinMode(MUX_EN, OUTPUT);
  pinMode(MUX_S0, OUTPUT);
  pinMode(MUX_S1, OUTPUT);
  pinMode(MUX_S2, OUTPUT);
  pinMode(MUX_S3, OUTPUT);

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
  oled1.println(timebuf);

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
    bool stallFlag = true; // set true when there is an error
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

  
  delay(3000);
  oled1.clear();
  oldMillis = millis();
      oled1.setCursor(0,0);
    oled1.clearToEOL();
    oled1.print(oldMillis);

}

//*****************************************************
void loop() {
  // Do stuff every time loopInterval is exceeded
  if ( (millis() - oldMillis) > loopInterval){
    oldMillis = millis(); // update oldMillis

    if (HallChannel > 1){
      HallChannel = 0; // Reset to 0
    }
    //-------------------------------------------
    // Call function to set shift register bit to wake
    // appropriate sensor
    shiftChannelSet(HallChannel); 
    //----------------------------------------------
    muxChannelSet(HallChannel); // Call function to set address bits
    
    unsigned int rawAnalog = 0;
    analogRead(ANALOG_IN); // throw away 1st reading
    for (byte i = 0; i<4; i++){
        rawAnalog = rawAnalog + analogRead(ANALOG_IN);
        delay(2);
    }
    // Do a 2-bit right shift to divide rawAnalog
    // by 4 to get the average of the 4 readings
    rawAnalog = rawAnalog >> 2;    
    oled1.setCursor(0,0);
    oled1.clearToEOL();
    oled1.print(oldMillis);
    oled1.setCursor(0,4);
    oled1.clearToEOL();
    oled1.println(HallChannel);
    oled1.clearToEOL();
    oled1.print(F("Ch"));
    oled1.print(HallChannel);
    oled1.print(F(": "));
    oled1.print(rawAnalog);

    
    HallChannel++; // increment for next loop
  }
}


void muxChannelSet (byte channel) {
  // select correct MUX channel
  digitalWrite (MUX_S0, (channel & 1) ? HIGH : LOW);  // low-order bit
  digitalWrite (MUX_S1, (channel & 2) ? HIGH : LOW);
  digitalWrite (MUX_S2, (channel & 4) ? HIGH : LOW);  
  digitalWrite (MUX_S3, (channel & 8) ? HIGH : LOW);  // high-order bit
}  // end of muxChannelSet

void shiftChannelSet (byte channel) {
    // Send a signal to the appropriate shift register
    // channel to go high (set 1) to wake that hall sensor
    digitalWrite(CS_SHIFT_REG, LOW);
    // Calculate the appropriate hex value to put a 1 in 
    // the correct channel's location (bit 0-15)
    // Do this by taking a hex 1 and left-shifting it the 
    // appropriate number of bits. To turn on Hall 0, you 
    // need a 1 in bit 0, to turn on Hall 15, you need
    // a 1 in bit 15 position.
    byte hexChannel = 0x01 << channel;
    SPI.transfer(hexChannel); // activate 1st hall effect sensor
    digitalWrite(CS_SHIFT_REG, HIGH);  
}

