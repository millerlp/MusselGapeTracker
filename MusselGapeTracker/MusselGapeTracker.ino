/*
 * MusselGapeTracker.ino - RevC/D updated 2019-05-15
 * 
 * The bootloader installed on the board should be Optiboot v6.2, 7, 8 or later
 * which can be accessed by adding Optiboot to the Boards Manager.
 * https://github.com/Optiboot/optiboot 
 * See their instructions on that page "To install into the Arduino software"
 * Specifically, follow their link to https://github.com/Optiboot/optiboot/releases
 * and copy the link address to the .json file under the Optiboot v6.2 release called 
 * "package_optiboot_optiboot-additional_index.json"
 * After installation of Optiboot in Arduino, 
 * choose the entry Optiboot on 32-pin cpus. 
 * After choosing that, you will get new options in the Tools menu to 
 * choose Processor: ATmega328p, and CPU Speed: 8MHz (int).
 * Then run Burn Bootloader with a programmer attached to the 
 * board's ICSP 3x2 header. 
 * Use those same settings (Optiboot, 32-pin cpus, ATmega328p, 8MHz (int)
 * when compiling and uploading this program as well. 
 * 
 * Available user options:
 * Change the value of SAVE_INTERVAL below to set how many seconds between
 * values being written to the SD card. Common values would be 1, 5, or 10.
 * 
 * Change the value of SCREEN_TIMEOUT below to set how many seconds the 
 * onboard display screen will stay on before going blank to save power.
 * 
 * User interface:
 * During bootup, the software will check that the real time clock and 
 * SD card are both present and working. If either is not present, the
 * onboard display will show the user and let them press Button 1 to choose
 * to continue without fixing the issue (data may not be saved to SD card).
 * 
 * 4-channel display mode:
 * While running, the user can press Button1 for ~2 seconds to wake the 
 * onboard display screen if asleep. If the screen is already on, pressing
 * button 1 for ~2 seconds will cycle to the next set of 4 Hall effect sensor 
 * channels. Continue entering 2 sec presses to cycle through all channels, and
 * to display the current filename being written to on the SD card. 
 * 
 * Single-channel display mode (useful for setup purposes):
 * Pressing Button 1 for 5 seconds will enter single-channel display mode. Data
 * are not saved in this mode. The onboard screen will show "Press button 1 to
 * choose channel". Repeatedly pressing Button 1 lets the user choose which channel
 * (0 to 15) to display, or go back to normal data collection. After the user finds
 * the desired channel, wait for ~5 seconds, and the onboard display will prompt
 * the user "Press Button 1 to proceed". When the user presses Button 1, the display
 * will show a rapidly updating value from the desired channel (no data are saved
 * to the SD card during this mode). Pressing Button 1 
 * again will return to normal data collection mode, saving all 16 channels to the 
 * SD card. 
 * 
 * File close + new file open:
 * Pressing Button 1 for at least 10 seconds will force the current data file to be
 * closed on the SD card, and a new file will be opened. The old file name will be
 * displayed briefly on the onboard display.
 * 
 * Reset button:
 * The reset button will force a full reboot of the board, and a new datalogging
 * session will begin if the clock and SD card are both functional. In some cases, 
 * when attempting to upload new firmware from the Arduino program to the board,
 * it may be necessary to press Reset briefly while the upload is starting. 
 * 
 * 

 * 
 */

#include "SdFat.h" // https://github.com/greiman/SdFat
#include <Wire.h>  // built in library, for I2C communications
#include "SSD1306Ascii.h" // https://github.com/greiman/SSD1306Ascii
#include "SSD1306AsciiWire.h" // https://github.com/greiman/SSD1306Ascii
#include <SPI.h>  // built in library, for SPI communications
#include "RTClib.h" // https://github.com/millerlp/RTClib
#include <EEPROM.h> // built in library, for reading the serial number stored in EEPROM
#include "MusselGapeTrackerlib.h" // https://github.com/millerlp/MusselGapeTrackerlib

//*********************************************************************
#define SAVE_INTERVAL 5 // Seconds between saved samples (1, 5, 10)
//*********************************************************************
#define SCREEN_TIMEOUT 30 // Seconds before OLED display shuts off
//*********************************************************************
#define SPS 16 // Sleeps per second. Leave this set at 4

// ***** TYPE DEFINITIONS *****
typedef enum STATE
{
  STATE_DATA, // collecting data normally
  STATE_ENTER_CALIB, // user wants to calibrate
  STATE_CALIB_WAIT, // waiting for user to signal mussel is positioned
  STATE_CALIB_ACTIVE, // taking calibration data, button press ends this
  STATE_CLOSE_FILE, // close data file, start new file
} mainState_t;

typedef enum DEBOUNCE_STATE
{
  DEBOUNCE_STATE_IDLE,
  DEBOUNCE_STATE_CHECK,
  DEBOUNCE_STATE_TIME
} debounceState_t;

// main state machine variable, this takes on the various
// values defined for the STATE typedef above. 
mainState_t mainState;

// debounce state machine variable, this takes on the various
// values defined for the DEBOUNCE_STATE typedef above.
volatile debounceState_t debounceState;

#define REDLED 4    // Red error LED pin
#define GRNLED A3   // Green LED pin
#define BUTTON1 2     // BUTTON1 on INT0, pin PD2
#define BUTTON2 3     // BUTTON2 on INT1, pin PD3
#define CS_SD 10    // Chip select for SD card, pin PB2
#define CS_SHIFT_REG A2 // Chip select for shift registers, pin PC2
#define SHIFT_CLEAR A1  // Clear (erase) line for shift registers, pin PC1
#define ANALOG_IN A7  // Hall effect analog input from multiplexer, pin ADC7
#define BATT_MONITOR A6  // analog input channel to sense battery voltage
#define BATT_MONITOR_EN A0 // digital output channel to turn on battery voltage check

#define MUX_S0  9   // Multiplexer channel select line, pin PB1
#define MUX_S1  5   // Multiplexer channel select line, pin PD5
#define MUX_S2  6   // Multiplexer channel select line, pin PD6
#define MUX_S3  7   // Multiplexer channel select line, pin PD7
#define MUX_EN  8   // Multiplexer enable line, pin PB0


unsigned int hallAverages[16]; // array to hold each second's sample averages
unsigned int prevAverages[16]; // array to hold previous second's sample averages

//******************************************
// 0X3C+SA0 - 0x3C or 0x3D for OLED screen on I2C bus
#define I2C_ADDRESS1 0x3C   // Typical default address
//#define I2C_ADDRESS2 0x3D // Alternate address, after moving resistor on OLED

SSD1306AsciiWire oled1; // create OLED display object, using I2C Wire

//*************
// Create sd card objects
SdFat sd;
SdFile logfile;  // for sd card, this is the file object to be written to
// Declare initial name for output files written to SD card
char filename[] = "YYYYMMDD_HHMM_00_SN00.csv";
// Placeholder serialNumber
char serialNumber[] = "SN00";
//************ Flags
bool sdErrorFlag = false;   // Flag to store SD card initialization error
bool rtcErrorFlag = false;  // Flag to store real time clock error
bool saveData = false;  // Flag to mark that saving to SD is possible
volatile bool buttonFlag = false; // Flag to mark when button was pressed
bool takeSamples = false;   // Flag to mark that samples should be taken on this loop
bool writeData = false; // Flag to mark when to write to SD card
bool serialValid = false; // Flag to show whether the serialNumber value is real or just zeros
bool screenUpdate = false; // Flag to trigger updating of OLED screen
bool screenChange = true; // Flag to trigger a full redraw of OLED screen, including channel labels
byte mcusr; // used to capture and store resetFlags

//******* Extract and store reset flags **************
uint8_t resetFlags __attribute__ ((section(".noinit"))); // Retrieve reset flags at startup
// Function to extract reset flag values when using Optiboot 6.2 or later
void resetFlagsInit(void) __attribute__ ((naked))
                          __attribute__ ((used))
                          __attribute__ ((section (".init0")));
void resetFlagsInit(void)
{
  /*
   * save the reset flags passed from the bootloader
   * This is a "simple" matter of storing (STS) r2 in the special variable
   * that we have created.  We use assembler to access the right variable.
   */
  __asm__ __volatile__ ("sts %0, r2\n" : "=m" (resetFlags) :);
}                          
//***********************************
// Create real time clock object
RTC_DS3231 rtc;
char buf[20]; // declare a string buffer to hold the time result
//**************** Time variables
DateTime newtime;
DateTime oldtime; // used to track time in main loop
byte oldday;     // used to keep track of midnight transition
DateTime buttonTime; // hold the time since the button was pressed
DateTime chooseTime; // hold the time stamp when a waiting period starts
DateTime calibEnterTime; // hold the time stamp when calibration mode is entered
volatile unsigned long buttonTime1; // hold the initial button press millis() value
byte debounceTime = 20; // milliseconds to wait for debounce
byte mediumPressTime = 1; // seconds to hold button1 to register a medium press
byte longPressTime = 5; // seconds to hold button1 to register a long press
byte pressCount = 0; // counter for number of button presses
unsigned long prevMillis;  // counter for faster operations
unsigned long newMillis;  // counter for faster operations
DateTime screenOnTime;  // Store the last time the OLED screen was switched on.
byte screenTimeout = SCREEN_TIMEOUT; // Seconds to wait before shutting off OLED screen
byte screenNum = 0; // keep track of which set of channels to show onscreen
bool screenOn = true; // turn OLEDs on or off
//******************
// Create ShiftReg and Mux objects
ShiftReg shiftReg;
Mux mux;
//*************Battery monitor********
float dividerRatio = 2.5; // Ratio of voltage divider (15k + 10k) / 10k = 2.5
// The refVoltage needs to be measured individually for each board (use a 
// voltmeter and measure at the AREF and GND pins on the board). Enter the 
// voltage value here. 
float refVoltage = 3.37; // Voltage at AREF pin on ATmega microcontroller
float batteryVolts = 0; // Estimated battery voltage returned from readBatteryVoltage function

//********************************************************
void setup() {
  mcusr = resetFlags;

  // Call the checkMCUSR function to check reason for this restart.
  // If only a brownout is detected, this function will put the board
  // into a permanent sleep that can only be reset with the reset 
  // button or a complete power-down.
//  checkMCUSR(mcusr, REDLED);  // In the MusselTrackerlib library
  // Set BUTTON1 as an input
  pinMode(BUTTON1, INPUT_PULLUP);
  // Set button2 as an input
  pinMode(BUTTON2, INPUT_PULLUP);
  digitalWrite(BUTTON2, LOW);
  pinMode(BATT_MONITOR, INPUT);
  pinMode(BATT_MONITOR_EN, OUTPUT); 
  digitalWrite(BATT_MONITOR_EN, LOW); // disable battery check initially
  // Set up the LEDs as output
  pinMode(REDLED,OUTPUT);
  digitalWrite(REDLED, LOW);
  pinMode(GRNLED,OUTPUT);
  digitalWrite(GRNLED, LOW);
  // Notify the user that a reboot just happened by flashing the 
  // red LED 5 times. 
  for (byte i = 0; i < 5; i++){
    digitalWrite(REDLED, HIGH);
    delay(250);
    digitalWrite(REDLED, LOW);
    delay(250);
  }
  Serial.begin(57600);
  Serial.println(F("Hello"));  
  // Initialize the shift register object
  shiftReg.begin(CS_SHIFT_REG, SHIFT_CLEAR);
  // Initialize the multiplexer object
  mux.begin(MUX_EN,MUX_S0,MUX_S1,MUX_S2,MUX_S3);
  // Grab the serial number from the EEPROM memory
  // This will be in the format "SNxx". The serial number
  // for a board can be permanently set with the separate
  // program 'serial_number_generator.ino' available in 
  // one of the subfolders of the MusselGapeTracker software
  EEPROM.get(0, serialNumber);
  if (serialNumber[0] == 'S') {
    serialValid = true; // set flag   
  }

  
  //----------------------------------
  // Start up the oled displays
  oled1.begin(&Adafruit128x64, I2C_ADDRESS1);
  oled1.set400kHz();  
  oled1.setFont(Adafruit5x7);    
  oled1.clear(); 
  oled1.home();
  oled1.set2X();
  // Initialize the real time clock DS3231M
  Wire.begin(); // Start the I2C library with default options
  rtc.begin();  // Start the rtc object with default options
  newtime = rtc.now(); // read a time from the real time clock
  newtime.toString(buf, 20); 
  // Now extract the time by making another character pointer that
  // is advanced 10 places into buf to skip over the date. 
  char *timebuf = buf + 10;
  for (int i = 0; i<11; i++){
    oled1.print(buf[i]);
  }
  oled1.println();
  oled1.println(timebuf);
  
  if (serialValid){
    Serial.print(F("Read serial number: "));
    Serial.println(serialNumber);
    for (byte i = 0; i<4; i++){
      oled1.print(serialNumber[i]);
    }
    oled1.println();
  } else {
    Serial.print(F("No valid serial number: "));
    serialValid = false;    
  }
  Serial.println();
  printBits(mcusr); // Print MCUSR contents to serial port

  // Show cause of reboot
  if (mcusr & _BV(WDRF)) {
    oled1.print(F("WDT RESET"));
    Serial.println(F("WDT RESET"));
  } else if (mcusr & _BV(EXTRF)){
    oled1.print(F("RESET BTN"));
    Serial.println(F("RESET BUTTON"));
  } else if (mcusr & _BV(PORF)){
    oled1.print(F("PWR RESET"));
    Serial.println(F("Power-on reset"));
  }
  delay(3000);

  //***********************************************
  // Check that real time clock has a reasonable time value
  bool stallFlag = true; // Used in error handling below
  if ( (newtime.year() < 2019) | (newtime.year() > 2035) ) {
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
  delay(1000);
  // If the clock and sd card are both working, we can save data
  if (!sdErrorFlag && !rtcErrorFlag){ 
    saveData = true;
  } else {
    saveData = false;
  }

  if (saveData){
    // If both error flags were false, continue with file generation
    newtime = rtc.now(); // grab the current time
    initFileName(sd, logfile, newtime, filename, serialValid, serialNumber); // generate a file name
    oled1.home();
    oled1.clear();
    oled1.set1X();
    oled1.clearToEOL();
    oled1.println(F("Writing to: "));
    oled1.println(filename);
    oled1.println(F("Save interval (sec):"));
    oled1.println(SAVE_INTERVAL);
    oled1.print(F("Battery: "));
    oled1.print(readBatteryVoltage(BATT_MONITOR_EN,BATT_MONITOR,dividerRatio,refVoltage),2);
    oled1.println(F("V"));
    delay(1000);
  } else {
    // if saveData is false
    // If saveData if false
    oled1.home();
    oled1.clear();
    oled1.set2X();
    oled1.println(F("Data will"));
    oled1.println(F("not be"));
    oled1.println(F("saved!"));   
  }
  // Start 32.768kHz clock signal on TIMER2. 
  // Supply the current time value as the argument, returns 
  // an updated time
  newtime = startTIMER2(rtc.now(), rtc, SPS);

  attachInterrupt(0, buttonFunc, LOW);  // BUTTON1 interrupt
  buttonFlag = false;
  screenOn = true; // set flag true to show screen is on
  // Take an initial set of readings for display
  read16Hall(ANALOG_IN, hallAverages, shiftReg, mux, MUX_EN);

  // Cycle briefly until we reach an even 10 sec value, so that the
  // data collection loop will start on a nice even time stamp. 
  while (!( (newtime.second() % 10) == 0)){
    delay(50);
    newtime = rtc.now();
  }

  wdt_enable(WDTO_8S); // Enable 8 second watchdog timer timeout
  oldtime = newtime; // store the current time value
  screenOnTime = newtime; // store when the screen was turned on
  screenNum = 0; // start on first set of channels
  oldday = oldtime.day(); // store the current day value
  mainState = STATE_DATA; // Start the main loop in data-taking state
}

//*****************************************************
void loop() {
  // Always start the loop by checking the time
  newtime = rtc.now(); // Grab the current time
  // Also reset the watchdog timer every time the loop loops
  wdt_reset(); 

  //-------------------------------------------------------------
  // Begin loop by checking the debounceState to 
  // handle any button presses
  switch (debounceState) {
    // debounceState should normally start off as 
    // DEBOUNCE_STATE_IDLE until button1 is pressed,
    // which causes the state to be set to 
    // DEBOUNCE_STATE_CHECK
    //************************************
    case DEBOUNCE_STATE_IDLE:
      // Do nothing in this case
    break;
    //************************************
    case DEBOUNCE_STATE_CHECK:
      // If the debounce state has been set to 
      // DEBOUNCE_STATE_CHECK by the buttonFunc interrupt,
      // check if the button is still pressed
      if (digitalRead(BUTTON1) == LOW) {
        if (millis() > (buttonTime1 + debounceTime) ) {
          // If the button has been held long enough to 
          // be a legit button press, switch to 
          // DEBOUNCE_STATE_TIME to keep track of how long 
          // the button is held
          debounceState = DEBOUNCE_STATE_TIME;
          buttonTime = rtc.now();
        } else {
          // If button is still pressed, but the debounce 
          // time hasn't elapsed, remain in this state
          debounceState = DEBOUNCE_STATE_CHECK;
        }
      } else {
        // If button1 is high again when we hit this
        // case in DEBOUNCE_STATE_CHECK, it was a false trigger
        // Reset the debounceState
        debounceState = DEBOUNCE_STATE_IDLE;
        buttonFlag = false;
        // Restart the button1 interrupt
        attachInterrupt(0, buttonFunc, LOW);
      }
    break; // end of case DEBOUNCE_STATE_CHECK
    //*************************************
    case DEBOUNCE_STATE_TIME:
      if (digitalRead(BUTTON1) == HIGH) {
        // If the user released the button, now check how
        // long the button was depressed. This will determine
        // which state the user wants to enter. 

        DateTime checkTime = rtc.now(); // get the time
        
        if (checkTime.unixtime() < (buttonTime.unixtime() + mediumPressTime)) {
          Serial.println(F("Short press registered"));
          delay(5);
          // User held button briefly, treat as a normal
          // button press, which will be handled differently
          // depending on which mainState the program is in.
          buttonFlag = true;
          
        } else if ( (checkTime.unixtime() > (buttonTime.unixtime() + mediumPressTime)) &
          (checkTime.unixtime() < (buttonTime.unixtime() + longPressTime)) ) {
          // User held button1 long enough to enter calibration
          // mode, but not long enough to enter close file mode
          // First close the current logfile
          logfile.close(); // Make sure the data file is closed and saved.
          // Set state to STATE_ENTER_CALIB
          mainState = STATE_ENTER_CALIB;
          // Update OLEDs to prompt user
          oled1.home();
          oled1.clear();
          oled1.println(F("Press"));
          oled1.println(F("BUTTON 1"));
          oled1.println(F("to choose"));
          oled1.println(F("Channel"));
          
          // Flash both LEDs 5 times to let user know we've entered
          // calibration mode
          for (byte i = 0; i < 5; i++){
            digitalWrite(REDLED, HIGH);
            digitalWrite(GRNLED, HIGH);
            delay(100);
            digitalWrite(REDLED, LOW);
            digitalWrite(GRNLED, LOW);
            delay(100);
          }
          // Set pressCount to 16 to start
          pressCount = 16;
          // Start a timer for entering Calib mode, to be used to give
          // the user time to enter 1 button press or 2 to choose 
          // which unit to calibrate
          chooseTime = rtc.now();
  
        } else if (checkTime.unixtime() > (buttonTime.unixtime() + longPressTime)){
          // User held button1 long enough to enter close file mode
          mainState = STATE_CLOSE_FILE;
        }
        
        // Now that the button press has been handled, return
        // to DEBOUNCE_STATE_IDLE and await the next button press
        debounceState = DEBOUNCE_STATE_IDLE;
        // Restart the button1 interrupt now that button1
        // has been released
        attachInterrupt(0, buttonFunc, LOW);
      } else {
        // If button is still low (depressed), remain in 
        // this DEBOUNCE_STATE_TIME
        debounceState = DEBOUNCE_STATE_TIME;
      }
      
    break; // end of case DEBOUNCE_STATE_TIME 
  } // end switch(debounceState)
  
//----------------------------------------------------------
//----------------------------------------------------------
  switch (mainState) {
    //*****************************************************
    case STATE_DATA:
    
      // Check to see if the current seconds value
      // is equal to oldtime.second(). If so, we
      // are still in the same second. If not,
      //  oldtime updates to equal newtime.
      if (oldtime.second() != newtime.second()) {
        oldtime = newtime; // update oldtime
        // Set these flags true once per second
        takeSamples = true; // set flag to take samples
        screenUpdate = true; // set flag to update oled screen 
        // If it's a save interval, set writeData = true
        if ( (newtime.second() % SAVE_INTERVAL) == 0){
          if (saveData){
            writeData = true; // set flag to write samples to SD card     
          }          
        }
      } else { // if a new second hasn't rolled over yet
        takeSamples = false; // it is not time to take a new sample
        writeData = false; // it is not time to write data to card
        screenUpdate = false; // it is not time to update the oled screen
      }

      if (takeSamples){
         if (screenOn | writeData){
          // If the onboard screen is currently on, or it's time to
          // write samples to the SD card, then
          // take a set of samples from the 16 hall effect sensors
          read16Hall(ANALOG_IN, hallAverages, shiftReg, mux, MUX_EN);
          batteryVolts = readBatteryVoltage(BATT_MONITOR_EN,BATT_MONITOR,dividerRatio,refVoltage);
         }
      }
      if (saveData && writeData){
        // If saveData is true, and it's time to writeData, then do this:
        // Check to see if a new day has started. If so, open a new file
        // with the initFileName() function
        if (oldtime.day() != oldday) {
          // Close existing file
          logfile.close();
          // Generate a new output filename based on the new date
          initFileName(sd, logfile, oldtime, filename, serialValid, serialNumber); // generate a file name
          // Update oldday value to match the new day
          oldday = oldtime.day();
        }
        // Call the writeToSD function to output the data array contents
        // to the SD card
//          bitSet(PIND, 3);  // toggle on
        writeToSD(newtime);
//          bitSet(PIND, 3);  // toggle off
        writeData = false; // reset flag
        printTimeSerial(newtime);
        digitalWrite(GRNLED, HIGH);
        Serial.println();
        delay(10); 
        digitalWrite(GRNLED, LOW);
        // The delay above is necessary to give the shift register time
        // to reset and clear all channels
        shiftReg.clear();          
      }       
      //-------------------------------------------------------------
      // OLED screen updating
      // Only update if screenUpdate is true and the following conditions allow it
      if (screenUpdate){
        // Now check to see whether the button has been pressed in this
        // state (buttonFlag). If it has, update OLED displays
        if (!buttonFlag) {
          // If the buttonFlag is false, button has not been pressed.
          // So check whether the screen is on, and shut it off if it 
          // has timed out. Otherwise update the hall data. 
          if (!screenOn) {
            // If screen is off, do nothing here
          } else {
            // If screen is on, check how long it's been on
            if ( (newtime.unixtime() - screenOnTime.unixtime()) > screenTimeout){
              // Screen has been on too long, turn off
              oled1.home();
              oled1.clear();
              // Manual shut down of SSD1306 oled display driver
              Wire.beginTransmission(0x3C); // oled1 display address
              Wire.write(0x80); // oled set to Command mode (0x80) instead of data mode (0x40)
              Wire.write(0xAE); // oled command to power down (0xAF should power back up)
              Wire.endTransmission(); // stop transmitting
              screenOn = false; // set flag to show screen is off
            } else {
              if (screenNum <= 3){
                // Screen should stay on
                OLEDscreenUpdate(screenNum, hallAverages, prevAverages, oled1, I2C_ADDRESS1, screenChange);
                screenUpdate = false;
                screenChange = false; 
                for (byte i = 0; i < 16; i++){
                  prevAverages[i] = hallAverages[i]; // update prevAverages
                }
              } else if (screenNum == 4){
                // Do nothing here, the filename is already displayed and doesn't 
                // need to be updated
              }
            }
          }
        } else if (buttonFlag) {
          // The buttonFlag is true, the user has registered a short press
          // to either wake the screen or switch to the next set of channels
          
          // Update screenOnTime so that screen will remain on for a 
          // length of screenTimeout from this point
          screenOnTime = newtime;
          screenChange = true; // Force a full redraw of the screen to show new channel labels
          buttonFlag = false; // buttonFlag has now been handled, reset it
          
          if (!screenOn){         // If screen is not currently on...
               // Manual startup of SSD1306 oled display driver
              Wire.beginTransmission(0x3C); // oled1 display address
              Wire.write(0x80); // oled set to Command mode (0x80) instead of data mode (0x40)
              Wire.write(0xAF); // oled command 0xAF should power back up
              Wire.endTransmission(); // stop transmitting            
              delay(50); // give display time to fire back up to avoid damaging it.
            if (screenNum <= 3){
              // Call the oled screen update function (in MusselGapeTrackerlib.h)
              OLEDscreenUpdate(screenNum, hallAverages, prevAverages, oled1, I2C_ADDRESS1, screenChange);              
              screenOn = true; // Set flag to true since oled screen is now on
              screenUpdate = false; // Set false just for good measure
              screenChange = false; // Set false now that screen has been updated
              for (byte i = 0; i < 16; i++){
                  prevAverages[i] = hallAverages[i]; // update prevAverages
              }
            } else if (screenNum == 4){
              oled1.home();
              oled1.clear();
              oled1.set1X();
              oled1.println(F("Writing to:"));
              oled1.println(filename); // show current filename
              oled1.println(F("Save interval (sec)"));
              oled1.println(SAVE_INTERVAL);
              // Show current battery voltage
              oled1.print(F("Battery: "));
              // Calls readBatteryVoltage function in MusselGapeTrackerlib library
              oled1.print(readBatteryVoltage(BATT_MONITOR_EN,BATT_MONITOR,dividerRatio,refVoltage),2);
              oled1.println(F("V"));
              newtime.toString(buf, 20); 
              // Now extract the time by making another character pointer that
              // is advanced 10 places into buf to skip over the date. 
              char *timebuf = buf + 10;
              for (int i = 0; i<11; i++){
                oled1.print(buf[i]);
              }
              oled1.println();
              oled1.println(timebuf);
              oled1.set2X();
              screenOn = true;
              screenUpdate = false;
              screenChange = false; 
            }
          } else if (screenOn){
            // Screen is already on, so user must want to increment to 
            // next screen full of channels (or show filename on screen 4)
            screenNum++;
            if (screenNum > 4){
              screenNum = 0; // reset at 0
            }
            if (screenNum == 4) {
              oled1.home();
              oled1.clear();
              oled1.set1X();
              oled1.println(F("Writing to:"));
              oled1.println(filename);  // show current filename
              oled1.println(F("Save interval (sec)"));
              oled1.println(SAVE_INTERVAL); // show save interval, seconds
              // Show current battery voltage
              oled1.print(F("Battery: "));
              // Calls readBatteryVoltage function in MusselGapeTrackerlib library
              oled1.print(readBatteryVoltage(BATT_MONITOR_EN,BATT_MONITOR,dividerRatio,refVoltage),2);
              oled1.println(F("V"));
              newtime.toString(buf, 20); 
              // Now extract the time by making another character pointer that
              // is advanced 10 places into buf to skip over the date. 
              char *timebuf = buf + 10;
              for (int i = 0; i<11; i++){
                oled1.print(buf[i]);
              }
              oled1.println();
              oled1.println(timebuf);
              oled1.set2X();
              screenUpdate = false;
              screenChange = false;              
            } else if (screenNum <= 3){
              // Now call the oled screen update function (in MusselGapeTrackerlib.h)
              OLEDscreenUpdate(screenNum, hallAverages, prevAverages, oled1, I2C_ADDRESS1, screenChange);
              screenUpdate = false; // Set flag false now
              screenChange = false; // Set false for next cycle
              for (byte i = 0; i < 16; i++){
                prevAverages[i] = hallAverages[i]; // update prevAverages
              }
            } 
          } // end of if (!screenOn)
        } // end of if (!buttonFlag)  OLED updating
      } // end of if (screenUpdate)

//      bitSet(PIND, 3); // toggle on for monitoring
      goToSleep(); // function in MusselGapeTrackerlib.h  
//      bitSet(PIND, 3); // toggle off for monitoring

      // After waking, this case should end and the main loop
      // should start again. 
      mainState = STATE_DATA;
    break; // end of case STATE_DATA  
    
    //*****************************************************
    case STATE_CLOSE_FILE:
      // If user held button 1 down for at least 6 seconds, they want 
      // to close the current data file and open a new one. 
      logfile.close(); // Make sure the data file is closed and saved.
      oled1.home();
      oled1.clear();
      oled1.set2X();
      oled1.println(F("File"));
      oled1.println(F("Closed"));
      oled1.set1X();
      oled1.println(filename);
      oled1.println(F("Save interval (sec)"));
      oled1.println(SAVE_INTERVAL);      
      // Briefly flash the green led to show that program 
      // has closed the data file and started a new one. 
      for (byte i = 0; i < 10; i++){
        digitalWrite(GRNLED, HIGH);
        delay(60);
        digitalWrite(GRNLED, LOW);
        delay(60);
      }
      // Open a new output file
      initFileName(sd, logfile, rtc.now(), filename, serialValid, serialNumber ); 
      Serial.print(F("Writing to "));
      Serial.println(filename);
      printTimeSerial(newtime);
      Serial.println(); 
      // Set some flags so the OLED screen updates properly
      screenOn = true;
      screenUpdate = true;
      screenChange = true;
      screenOnTime = oldtime = newtime; 
      mainState = STATE_DATA; // Return to normal data collection
    break; // end of case STATE_FILE_CLOSE    
    //*****************************************************
    case STATE_ENTER_CALIB:
      // We have arrived at this state because the user held button1 down
      // for the specified amount of time (see the debounce cases earlier)
      // so we now allow the user to enter additional button presses to 
      // choose which Hall effect sensor they want to calibrate. 
      
      // Read a time value
      calibEnterTime = rtc.now();
  
      if (calibEnterTime.unixtime() < (chooseTime.unixtime() + longPressTime)){
        // If longPressTime has not elapsed, add any new button presses to the
        // the pressCount value
        if (buttonFlag) {
          pressCount++;
          // If the user pressed the button, reset chooseTime to
          // give them extra time to press the button again. 
          chooseTime = calibEnterTime;
          buttonFlag = false; // reset buttonFlag
          if (pressCount == 16){
              oled1.home();
              oled1.clear();
              oled1.println(F("Return"));
              oled1.println(F("to data"));
              oled1.print(F("collection")); 
          } else if (pressCount > 16) {
            pressCount = 0;
            oled1.home();
            oled1.clear();
            oled1.println(F("Choose"));
            oled1.println(F("Channel:"));
            oled1.print(pressCount);
          } else {
            // For any number of presses between 0 and 15, 
            // show the pressCount
            Serial.print(F("Channel "));
            Serial.println(pressCount);
            oled1.setCursor(0,4);
            oled1.clearToEOL();
            oled1.print(pressCount);
          }
        } 
        mainState = STATE_ENTER_CALIB; // remain in this state
      } else if (calibEnterTime.unixtime() >= (chooseTime.unixtime() + longPressTime)){
        // The wait time for button presses has elapsed, now deal with 
        // the user's choice based on the value in pressCount
        switch (pressCount) {
          case 16:
            // If the user didn't press the button again, return
            // to normal data taking mode
            mainState = STATE_DATA; 
            // Set some flags so the OLED screen updates properly
            screenOn = true;
            screenUpdate = true;
            screenChange = true;
          break;
          default:
            // If the user pressed at least one time, we'll show that channel
            mainState = STATE_CALIB_WAIT;
          break;
        } 
      }
    break; // end of STATE_CALIB_ENTER    
    //*****************************************************
     case STATE_CALIB_WAIT:
      // If the user entered 1 or more button presses in 
      // STATE_CALIB_ENTER, then we should arrive here. 
      // Now we wait for the user to get the mussel situated and 
      // press the button one more time to begin taking data.
      // The green led will pulse on and off at 1Hz while waiting
      if (newtime.second() != calibEnterTime.second() ) {
        Serial.println(F("Waiting..."));
        oled1.home();
        oled1.clear();
        oled1.println(F("Press"));
        oled1.println(F("BUTTON 1"));
        oled1.println(F("to proceed"));
        calibEnterTime = newtime; // reset calibEnterTime
        digitalWrite(GRNLED, !digitalRead(GRNLED));
      }
      
      // If the user finally presses the button, enter the active
      // calibration state
      if (buttonFlag) {
        mainState = STATE_CALIB_ACTIVE;
        buttonFlag = false; // reset buttonFlag
        prevMillis = millis();
        oled1.home();
        oled1.clear();
        oled1.print(F("Ch"));
        oled1.print(pressCount);
        oled1.print(F(": "));
      }
      
    break;
    //*****************************************************
    case STATE_CALIB_ACTIVE:
      // Show selected channel Hall effect values on screen
      newMillis = millis(); // get current millis value
      // If 200 or more milliseconds have elapsed, take a new
      // reading from the Hall effect sensor
      if (newMillis >= prevMillis + 200) {
        prevMillis = newMillis; // update millis
        
        // Choose which channel to sample based on pressCount value
        digitalWrite(MUX_EN, LOW); // enable multiplexer
        delayMicroseconds(2);
        shiftReg.shiftChannelSet(pressCount);
        mux.muxChannelSet(pressCount);
        unsigned int newReading = readHall(ANALOG_IN);
        digitalWrite(MUX_EN, HIGH); // disable multiplexer
        oled1.setCursor(60,0);
        oled1.clear(60,128,0,1); // Only clear the portion of the line with the value
        oled1.print(newReading);
        Serial.println(newReading); // echo to serial monitor
      } 
      
      
      // The user can press button1 again to end calibration mode
      // This would set buttonFlag true, and cause the if statement
      // below to execute
      if (buttonFlag) {
        buttonFlag = false;
       
        // Open a new output file
        initFileName(sd, logfile, rtc.now(), filename, serialValid, serialNumber );
        oled1.home();
        oled1.clear();
        oled1.set1X();
        oled1.println(F("New file: "));
        oled1.println(filename);

        mainState = STATE_DATA; // return to STATE_DATA
        // Flash both LEDs 5 times to let user know we've exited
        // calibration mode
        for (byte i = 0; i < 5; i++){
          digitalWrite(REDLED, HIGH);
          digitalWrite(GRNLED, HIGH);
          delay(100);
          digitalWrite(REDLED, LOW);
          digitalWrite(GRNLED, LOW);
          delay(100);
        }
        delay(800); 
        screenOnTime = rtc.now();
        // Set some flags so the OLED screen updates properly
        screenOn = true;
        screenUpdate = true;
        screenChange = true;
      } // end of if(buttonFlag) statement
      
    break; 
  } // end of main state
} // end of main loop

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// This Interrupt Service Routine (ISR) is called every time the
// TIMER2_OVF_vect goes high (==1), which happens when TIMER2
// overflows. The ISR doesn't care if the AVR is awake or
// in SLEEP_MODE_PWR_SAVE, it will still roll over and run this
// routine. If the AVR is in SLEEP_MODE_PWR_SAVE, the TIMER2
// interrupt will also reawaken it. This is used for the goToSleep() function
ISR(TIMER2_OVF_vect) {
  // nothing needs to happen here, this interrupt firing should 
  // just awaken the AVR
}

//---------watchdog interrupt------------
//ISR(WDT_vect){
//  // Do nothing here, only fires when watchdog timer expires
//  // which should force a complete reset/reboot
//}; 



//--------------- buttonFunc --------------------------------------------------
// buttonFunc
void buttonFunc(void){
  detachInterrupt(0); // Turn off the interrupt
  buttonTime1 = millis(); // Grab the current elapsed time
  debounceState = DEBOUNCE_STATE_CHECK; // Switch to new debounce state
  // Execution will now return to wherever it was interrupted, and this
  // interrupt will still be disabled. 
}

//------------- writeToSD -----------------------------------------------
// writeToSD function. This formats the available data in the
// data arrays and writes them to the SD card file in a
// comma-separated value format.
void writeToSD (DateTime timestamp) {
  // Reopen logfile. If opening fails, notify the user
  if (!logfile.isOpen()) {
    if (!logfile.open(filename, O_RDWR | O_CREAT | O_AT_END)) {
      digitalWrite(REDLED, HIGH); // turn on error LED
    }
  }
  // Write the unixtime
  logfile.print(timestamp.unixtime(), DEC); // POSIX time value
  logfile.print(F(","));
  printTimeToSD(logfile, timestamp); // human-readable 
  // Write the 16 Hall effect sensor values in a loop
  for (byte i = 0; i < 16; i++){
    logfile.print(F(","));
    logfile.print(hallAverages[i]); 
  }
  logfile.print(F(","));
  logfile.print(batteryVolts,2);
  logfile.println();
  // logfile.close(); // force the buffer to empty

  if (timestamp.second() % 30 == 0){
      logfile.timestamp(T_WRITE, timestamp.year(),timestamp.month(), \
      timestamp.day(),timestamp.hour(),timestamp.minute(),timestamp.second());
  }
}





//---------freeRam-----------
// A function to estimate the remaining free dynamic memory. On a 328P (Uno), 
// the dynamic memory is 2048 bytes.
int freeRam () {
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}
