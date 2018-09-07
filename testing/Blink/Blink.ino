/*
 *  Blink.ino
 *  
 *  Simple blink test for RevC hardware
 */


#define REDLED 4  // on arduino digital pin 4
#define GRNLED A3

void setup() {
  pinMode(REDLED, OUTPUT);
  digitalWrite(REDLED, LOW);
  pinMode(GRNLED, OUTPUT);
  digitalWrite(GRNLED, LOW);
  

}

void loop() {
  digitalWrite(REDLED, !(digitalRead(REDLED)));
  delay(100);
  digitalWrite(GRNLED, !(digitalRead(GRNLED)));
  delay(100);
  

}
