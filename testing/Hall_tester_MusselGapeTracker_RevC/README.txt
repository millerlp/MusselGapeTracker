/* Hall_tester_MusselGapeTracker_RevC
 *  Luke Miller 2020
 *  
 *  Meant to work with a MusselGapeTracker Rev C board. Connect
 *  to a KD Scientific Model 200 syringe dispenser to move the 
 *  dispenser a known distances, causing the 
 *  magnetic field of a Hall effect sensor to change in response.
 *  If you don't have a Model 200 syringe dispenser sitting around,
 *  don't bother running this program.
 *  
 *  Connect Syringe Trigger pin to MusselTracker pin PD3 (Button2 line)
 *  Connect Syringe GND to MusselTracker GND
 *  
 *  
 *  
 *  Make sure the syringe pump is set as follows:
 *  Diam: 50.00 mm
 *  Vol: 0.623 ml
 *  Direction: Withdraw
 *  When you dispense 0.623mL in this setting, you get 1/2 of a full 
 *  rotation of the drive screw.
 *  The drive screw is 40 teeth per inch, so one full rotation moves
 *  the carriage 1/40 of an inch, which is 0.025 inches = 0.635mm, and 
 *  a half rotation = 0.3175 mm.
 *  Usage:
 *  1. Choose which Hall channel you want to calibrate (0-15). Press Button 1
 *  briefly to cycle through the channels. A live update of the current 
 *  channel's analog reading will be shown. 
 *  2. Place Hall sensor on syringe pump fixed block. Place magnet on syringe
 *  pump traveling block (carriage). Fire up Serial Monitor to show Hall reading
 *  3. Release carriage feed, slide magnet near Hall sensor. 
 *  4. Make sure Hall reading is more than ~ 12, as this is the saturated 
 *  reading. If Hall reading is > 500, turn the magnet over (or make 
 *  sure the bottom of the sensor board is oriented towards the magnet).
 *  For consistency's sake we try to set these up so that when the magnet
 *  is near the board the reading is near zero, and as the magnet moves
 *  further from the board the signal goes up towards 500.
 *  5. Engage carriage feed. 
 *  6. Press and hold Button 1 for at least 5 seconds, then release. This
 *  will immediately start the first trial.
 *  7.  A  Hall reading will be taken at each distance (actually the average
 *  of 4 readings), starting from a distance = 0.0 for the initial reading. Every 
 *  subsequent distance is relative to that initial starting point (and is 
 *  assumed to be happening in 0.3175mm steps if the syringe pump is
 *  correctly set to 0.623mL volume,  50mm diameter syringe. I recommend 40ml/min
 *  rate as a reasonable movement speed.
 *  **Any different settings on the 
 *  syringe pump will result in an incorrect distance being recorded.**
 *  8. When the last movement is made, the Serial Monitor or OLED screeen 
 *  will return to just outputing the current Hall reading. The filename
 *  that is being written to for this session will be displayed.
 *  9. You may now release the traveling block and reposition the magnet
 *  near the Hall sensor to run another trial. I recommend starting 
 *  the magnet/Hall combination in several positions and angles to 
 *  make sure you span the range of possible starting positions and
 *  signal curves. 
 *  10. Press Button 1 briefly to start the next trial, which will be
 *  saved to the same output file.
 *  11. Repeat steps 7-10 to carry out multiple trials. 
 *  12. When finished with trials for this Hall effect channel, press 
 *  the RESET button on the circuit board to restart the program and
 *  choose a new Hall channel. 
 */
