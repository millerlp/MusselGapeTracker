MusselGapeTracker
-------------------
Arduino-compatible code and designs for a multi-channel hall effect data logger. 

The RevC hardware consists of a Atmega 328P microcontroller hooked to two SN74HC595 shift registers
and a CD74HC4067 16-channel multiplexer set up to read 16 analog voltages coming from
Allegro A1393 or A1395 (recommended) Hall effect sensors. The SN74HC595 chips are used to control the SLEEP lines on each of the 16 Hall effect sensors, to wake them one at a time when a reading is requested. 

------
Bootloader information:
Because this board is built from scratch with a "bare" ATmega328p chip installed,
you need to use a programmer and the Arduino software to burn a bootloader
onto the ATmega328p before you can use a standard FTDI serial adapter to upload 
firmware.

* The bootloader installed on the MusselGapeTracker board should be Optiboot v6.2 or later (up to version 8.0 as of 5/2019), which can be accessed by adding Optiboot to the Arduino software Boards Manager.
* Go to https://github.com/Optiboot/optiboot 
* See their instructions on that page "To install into the Arduino software"
* Specifically, follow their link to https://github.com/Optiboot/optiboot/releases and copy the link address to the .json file under the Optiboot v6.2 (or 8.0) release called  "package_optiboot_optiboot-additional_index.json"
* After installation of Optiboot in Arduino, go to the menu Tools/Board: and choose the entry "Optiboot on 32-pin cpus". 
* After choosing that, you will get new options in the Tools menu to choose Processor: ATmega328p, and CPU Speed: 8MHz (int).
* Then run Burn Bootloader with a programmer attached to the MusselGapeTracker board's ICSP 3x2 header.
