/* Hall_tester_MusselGapeTracker_RevC
 *  Luke Miller 2019
 *  Meant to work with a MusselGapeTracker Rev C board. Connect
 *  to a KD Scientific Model 200 syringe dispenser to move the 
 *  dispenser back and forth known distances, causing the 
 *  magnetic field of a Hall effect sensor to change in response.
 *  If you don't have a Model 200 syringe dispenser sitting around,
 *  don't bother running this program.
 *  
 *  Connect Syringe Trigger pin to MusselTracker SCL line (A5)
 *  Connect Syringe GND to MusselTracker GND
 *  
 *  
 *  
 *  Make sure the syringe dispenser is set to use Air-Tite All Pls
 *  syringe, 20cc volume, 20.0mm diameter. When you dispense
 *  0.2mL in this setting, you get 1 full rotation of the drive screw
 *  The drive screw is 40 teeth per inch, so one full rotation moves
 *  the carriage 1/40 of an inch, which is 0.025 inches = 0.635mm. 
 *  Thus, if set up as above, 
 *  one actuation of the Trigger pin will move the carriage 0.635mm. 
 *  Direction of travel needs to be set as Withdraw to move the carriage
 *  away from the Hall effect sensor. 
 *  *******NOTE*******************
 *  For real measurements, set the dispenser up to move 0.1mL (half rotation)
 *  which will move 0.3175mm linearly. This needs to be set on the 
 *  syringe dispenser itself in the Volume menu. The Rate value on the 
 *  syringe pump can be set to 10ml/m to move at a reasonable pace.
 *  
 *  Usage:
 *  Place Hall sensor on syringe pump fixed block. Place magnet on syringe
 *  pump traveling block. Fire up Serial Monitor to show Hall reading
 *  Release traveling block feed, slide magnet near Hall sensor. Engage
 *  traveling block feed. 
 *  Make sure Hall reading is more than ~ 12, as this is the saturated 
 *  reading. If Hall reading is > 500, turn the magnet over (or make 
 *  sure the bottom of the sensor board is oriented towards the magnet).
 *  For consistency's sake we try to set these up so that when the magnet
 *  is near the board the reading is near zero, and as the magnet moves
 *  further from the board the signal goes up towards 500.
 *  Press Button1 on the MusselTracker board to start the syringe pump 
 *  moving. A set of 4 Hall readings will be taken at each distance,
 *  starting from a distance = 0.0 for the initial reading. Every 
 *  subsequent distance is relative to that initial starting point. 
 *  When the last movement is made, the Serial Monitor will return to 
 *  just outputing the current Hall reading.
 *  You may now release the traveling block and reposition the magnet
 *  near the Hall sensor to run another trial. I recommend starting 
 *  the magnet/Hall combination in several positions and angles to 
 *  make sure you span the range of possible starting positions and
 *  signal curves. 
 */


#include "SdFat.h" // https://github.com/greiman/SdFat
#include <Wire.h>  // built in library, for I2C communications
#include <SPI.h>  // built in library, for SPI communications
#include "RTClib.h" // https://github.com/millerlp/RTClib
#include <EEPROM.h> // built in library, for reading the serial number stored in EEPROM
#include "MusselTrackerlib.h"  // https://github.com/millerlp/MusselTrackerlib

RTC_Millis rtc; // Create real time clock object (not using onboard DS3231 I2C clock)


#define ERRLED 5    // Red error LED pin
#define GREENLED 6    // Green LED pin
#define BUTTON1 2     // BUTTON1 on INT0, pin PD2
#define TRIGGER A5  // Trigger on syringe dispenser

//*************************************
float moveDist = 0.3175; // units mm, if pump set to dispense 0.1mL

//*************
// Create sd card objects
SdFat sd;
SdFile logfile;  // for sd card, this is the file object to be written to
SdFile calibfile; // for sd card, this is the calibration file to write
const byte chipSelect = 10; // define the Chip Select pin for SD card
//*************
// Define a HallSensor object (from MusselTrackerlib.h library)
// to access the hall effect sensor functions available in that
// library. 
HallSensor hallsensor;
// Variables to hold hall effect readings
int hallVal1 = 0;
int hallVal2 = 0;
// The HallSensor functions will require an argument to 
// specify which sensor your want, HALL1 or HALL2, so you 
// must always specify that in the hall effect sensor
// function calls. 


// Declare initial name for output files written to SD card
char filename[] = "Hall_CALIB_000_SN00.csv";
// Placeholder serialNumber
char serialNumber[] = "SN00";
// Define a flag to show whether the serialNumber value is real or just zeros
bool serialValid = false;
byte debounceTime = 20; // milliseconds to wait for debounce
volatile bool buttonFlag = false; // Flag to mark when button was pressed
unsigned long prevMillis;  // counter for faster operations
unsigned long newMillis;  // counter for faster operations
unsigned int trialNum = 0; // counter for trial number
float distanceVal = 0; // keep track of distance moved
DateTime newtime; // keep track of time

//--------------- buttonFunc --------------------------------------------------
// buttonFunc
void buttonFunc(void){
  detachInterrupt(0); // Turn off the interrupt
  delay(debounceTime);
  if (!digitalRead(BUTTON1)) {
    buttonFlag = true; // Set flag to true
  } else {
    buttonFlag = false; // False alarm
  }
  // Execution will now return to wherever it was interrupted, and this
  // interrupt will still be disabled. 
}
//--------------------------------------------------------------------------


void setup() {
    // Set button1 as an input
  pinMode(BUTTON1, INPUT_PULLUP);
  // Register an interrupt on INT0, attached to button1
  // which will call buttonFunc when the button is pressed.
  attachInterrupt(0, buttonFunc, LOW);
  // Set up the LEDs as output
  pinMode(ERRLED,OUTPUT);
  digitalWrite(ERRLED, LOW);
  pinMode(GREENLED,OUTPUT);
  digitalWrite(GREENLED, LOW);

  // Set up pins for Hall effect sensor
  pinMode(A2, OUTPUT); // sleep line for hall effect 1
  digitalWrite(A2, LOW);
  pinMode(A6, INPUT); // Read the Hall effect sensor on this input
  // Set up pin for syringe dispenser drive
  pinMode(TRIGGER, OUTPUT); // Same as SCL pin on Arduino
  digitalWrite(TRIGGER, HIGH);

  Serial.begin(57600);
  Serial.println("Hello");

    // Grab the serial number from the EEPROM memory
  // The character array serialNumber was defined in the preamble
  EEPROM.get(0, serialNumber);
  if (serialNumber[0] == 'S') {
    serialValid = true; // set flag
  } else {
    Serial.print(F("No valid serial number: "));
    Serial.println(serialNumber);
    serialValid = false;
  }
    // Initialize the Allegro A1393 hall effect sensors
  // There is only one hallsensor object, but you feed it
  // HALL1 or HALL2 as an argument to specify which channel
  // you want to activate and read. 
  hallsensor.begin(HALL1);
  //*************************************************************
// SD card setup and read (assumes Serial output is functional already)
  pinMode(chipSelect, OUTPUT);  // set chip select pin for SD card to output
  // Initialize the SD card object
  // Try SPI_FULL_SPEED, or SPI_HALF_SPEED if full speed produces
  // errors on a breadboard setup. 
  if (!sd.begin(chipSelect, SPI_FULL_SPEED)) {
  // If the above statement returns FALSE after trying to 
  // initialize the card, enter into this section and
  // hold in an infinite loop.
    while(1){ // infinite loop due to SD card initialization error
                        digitalWrite(ERRLED, HIGH);
                        delay(100);
                        digitalWrite(ERRLED, LOW);
                        digitalWrite(GREENLED, HIGH);
                        delay(100);
                        digitalWrite(GREENLED, LOW);
    }
  }

   // Following line sets the RTC to the date & time this sketch was compiled
   rtc.begin(DateTime(__DATE__, __TIME__));
   newtime = rtc.now(); // Read time
  
  initFileName(newtime); // generate a file name

}

void loop() {
  // Don't collect a round of data until buttonFlag goes true
  newMillis = millis();
  if (newMillis >= prevMillis + 500){
    prevMillis = newMillis; // Update prevMillis to new value
    Serial.print(F("Ready, Hall1: "));
    Serial.println(hallsensor.readHall(HALL1));
  }

  if (buttonFlag){
    buttonFlag = false; // reset buttonFlag
    distanceVal = 0.0; // initialize distance
    ++trialNum; // increment trial number
    
    while (distanceVal < 12.5){ 
      // Initialize the array to hold hall effect sensor values
      unsigned int hallArray[4] = {0,0,0,0};
      Serial.print(F("Trial: "));
      Serial.print(trialNum);
      Serial.print(F("\t Distance "));
      Serial.print(distanceVal,4);
      Serial.print(F(" mm"));
      // Read hall effect sensor
      for (int j = 0; j < 4; j++){
        // Wake and read the Hall effect sensor
        hallArray[j] = hallsensor.readHall(HALL1); // This takes an avg of 4 readings
        Serial.print(F("\t"));
        Serial.print(hallArray[j]);
        delay(150);
      }
      Serial.println();
      // Write the readings to the output file
        // Reopen logfile. If opening fails, notify the user
      if (!logfile.isOpen()) {
        if (!logfile.open(filename, O_RDWR | O_CREAT | O_AT_END)) {
          digitalWrite(ERRLED, HIGH); // turn on error LED
        }
      }
      for (int j = 0; j < 4; j++){
        logfile.print(trialNum);
        logfile.print(F(","));
        logfile.print(distanceVal,5);
        logfile.print(F(","));
        logfile.println(hallArray[j]);  
      }
      logfile.close();

      // Actuate the Trigger pin to make the syringe dispenser
      // run
      digitalWrite(TRIGGER, LOW); // Transition High to Low to trigger movement
      delay(2000); // Give dispenser time to move
      digitalWrite(TRIGGER, HIGH); // Reset Trigger line
      // Increment the distanceVal
      distanceVal = distanceVal + moveDist;  // units mm
    
    } // end of while loop 
    Serial.println(F("Finished"));
    // Reattach the interrupt to allow another trial on this same data file
    attachInterrupt(0, buttonFunc, LOW);
  } 
}

//------------------------------------------------------------------------------

//-------------- initFileName --------------------------------------------------
// initFileName - a function to create a filename for the SD card based
// on the 4-digit year, month, day, hour, minutes and a 2-digit counter. 
// The character array 'filename' was defined as a global array 
// at the top of the sketch in the form "Hall_CALIB_000_SN00.csv"
void initFileName(DateTime time1) {
  
  // If there is a valid serialnumber, insert it into 
  // the file name in positions 15-18. 
  if (serialValid) {
    byte serCount = 0;
    for (byte i = 15; i < 19; i++){
      filename[i] = serialNumber[serCount];
      serCount++;
    }
  }
  // Change the counter on the end of the filename
  // (digits 11,12,13) to increment count for files generated on
  // the same day. This shouldn't come into play
  // during a normal data run, but can be useful when 
  // troubleshooting.
  for (uint16_t i = 0; i < 1000; i++) {
    filename[11] = i / 100 + '0'; // 100's digit
    filename[12] = (i % 100)/10 + '0'; // 10's digit
    filename[13] = i % 10 + '0'; // 1's digit
    // Check and see if this filename is already on the card
    // and if so, repeat the for loop with a value 1 digit higher
    // until you find a non-existent filename.
    if (!sd.exists(filename)) {
      // when sd.exists() returns false, this block
      // of code will be executed to open the file
      if (!logfile.open(filename, O_RDWR | O_CREAT | O_AT_END)) {
        // If there is an error opening the file, notify the
        // user. Otherwise, the file is open and ready for writing
        // Turn both indicator LEDs on to indicate a failure
        // to create the log file
        digitalWrite(ERRLED, !digitalRead(ERRLED)); // Toggle error led 
        digitalWrite(GREENLED, !digitalRead(GREENLED)); // Toggle indicator led 
        delay(5);
      }
      break; // Break out of the for loop when the
      // statement if(!logfile.exists())
      // is finally false (i.e. you found a new file name to use).
    } // end of if(!sd.exists())

  } // end of file-naming for loop
  //------------------------------------------------------------

  // write a 2nd header line to the SD file
  logfile.println(F("Trial,Distance.mm,Hall1"));
  // Update the file's creation date, modify date, and access date.
  logfile.timestamp(T_CREATE, time1.year(), time1.month(), time1.day(), 
      time1.hour(), time1.minute(), time1.second());
  logfile.timestamp(T_WRITE, time1.year(), time1.month(), time1.day(), 
      time1.hour(), time1.minute(), time1.second());
  logfile.timestamp(T_ACCESS, time1.year(), time1.month(), time1.day(), 
      time1.hour(), time1.minute(), time1.second());
  logfile.close(); // force the data to be written to the file by closing it
  Serial.print(F("Ouput file: "));
  Serial.println(filename);
} // end of initFileName function
