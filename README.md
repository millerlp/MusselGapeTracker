MusselGapeTracker
-------------------
Arduino-compatible code and designs for a multi-channel hall effect data logger. 

The RevA hardware consists of a Atmega 328P microcontroller hooked to two SN74HC595 shift registers
and a CD74HC4067 16-channel multiplexer set up to read 16 analog voltages coming from
Allegro A1393 Hall effect sensors. The SN74HC595 chips are used to control the SLEEP lines on each of the 16 Hall effect sensors, to wake them one at a time when a reading is requested. 