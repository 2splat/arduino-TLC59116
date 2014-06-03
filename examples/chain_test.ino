/*

 Test n chained TLC59116 shift-registers.
 
 Set the test
 
 */

#define TEST_NUM 1

#include <TLC59116.h>

void setup() {
  Serial.begin(9600);

}

TLC59116::First tlc_first; 
TLC59116::Broadcast tlc_broadcast; 
TLC59116 tlcs; // all of them
TLC59116 tlc(0); // address pins set to 0

int first = 1; // first time in loop for initing

void loop() {
  switch (TEST_NUM) {

  case 1:
    // Find out that TLC59116's are hooked up and working (and the broadcast addresses).
    // TLC59116::scan.print();
    while(1) {delay(1000);}
    break;
/*
  // on/off simple
  case 2:
    tlc.all(HIGH)->delay(900)
      ->all(LOW)->delay(100)
        ;        
    break;

    // OE test - Should turn on/off & PWM dim
  case 3:

    if (first) { 
      tlc.all(HIGH); 
      tlc.off(); 
    }
    Serial.print(first);
    tlc.on()->delay(900)
      ->off()->delay(100)
        ;
    tlc.on()->delay(500)
      ->brightness(100)->delay(500)
        ->brightness(50)->delay(500)
          ->brightness(10)->delay(500)
            ;
    break;

    // LE test - Should have NO flicker
  case 4:
    tlc.all(LOW)->delay(500);
    for (int i=0; i<CHAIN_CT; i++) {
      tlc.shift(0xFFFF); // no flicker here
    }
    tlc.delay(400);
    tlc.latch_pulse();
    tlc.delay(500);
    break;

    // SDO test, only 1 shift-register -- Check the serial monitor to confirm
    // Blinks while reading the SDO
  case 5:
    {
      tlc.on();
      unsigned int pattern;
      pattern = 0b0011000111001101;
      tlc.send(pattern); // prime it, SDO is 0 (MSB)

      Serial.println("Want");
      Serial.println("| Saw");
      Serial.println("| | Good?");

      int fill = 1;
      for(int i=0; i<16; i++) {
        int want = bitRead(pattern, 15-i); // SDO shows MSB
        pattern >> 1;
        int val = tlc.read_sdo();
        Serial.print(want); 
        Serial.print(" ");
        Serial.print(val); 
        Serial.print(" ");
        Serial.println(val == want ? 1 : 0);

        // we'll fill w/ 1010..., slow enough to see
        tlc.send_bits(1,fill, 100);
        fill = fill ? 0 : 1;
      }
      tlc.all(HIGH)->delay(400)->flash()->delay(400);
    }
    break;

    // error-detect - short 1 pin, load 1 pin, leave 1 pin open
  case 6:
    {
      int status;
      if (first) tlc.all(HIGH)->on()->delay(300);
      status = tlc.error_detect();
      Serial.print("Error Status: ");
      Serial.println(status,BIN);
      tlc.all(HIGH)->on()->delay(10000);
    }
    break;

    // Use the current-gain-multiplier to set the range down
    // Does a bunch of blinking to confirm mode switch
  case 7:
    Serial.println("On Max");
    tlc.config(1,1,127);
    tlc.all(HIGH)->on()->delay(1000);

    Serial.println("On Min");
    tlc.config(0,0,0);
    tlc.all(HIGH)->on()->delay(1000);

    // This confirms that we are back in normal mode
    Serial.println("shifting");
    tlc.all(LOW)->delay(300);
    tlc.all(HIGH)->delay(300);
    tlc.all(LOW)->delay(300);
    tlc.all(HIGH)->delay(300);
    tlc.all(LOW)->delay(300);
    tlc.all(HIGH)->delay(300);
    tlc.all(LOW)->delay(300);

    break;

  // Assuming config() test above works,
  // This shows what it looks like to set config
  // without delays,
  // And the full-range effect
  // Set tlc.debug(0): should be smooth (no flash)
  case 8:
    if (first) {Serial.println("FIRST"); tlc.debug(0); tlc.all(HIGH)->on(); }
    Serial.println("Segment ends");
    tlc.config(1,1,127)->on()->all(HIGH)->delay(200);
    tlc.config(0,1,127)->on()->all(HIGH)->delay(200);
    tlc.config(1,0,127)->on()->all(HIGH)->delay(200);
    tlc.config(0,0,127)->on()->all(HIGH)->delay(200);
    tlc.flash();
    
    // Full range
    Serial.println("Full range");
    for (int cm=1; cm>=0; cm--) {
      for (int vb=1; vb>=0; vb--) {
        for (int vg=127; vg>=0; vg -=16) {
          tlc.config(cm,vb,vg)->on()->all(HIGH)->delay(200);
        }
      }
    }
    break;
  
  // Calibrate the trim-pot vs. current-gain
  // Does Max-mid-min so you can see what effect trimpot has
  case 9:
    if (first) tlc.all(HIGH)->on();
    tlc.config(1,1,127)->on()->all(HIGH)->delay(400);
    tlc.config(1,0,0)->on()->all(HIGH)->delay(400);
    tlc.config(0,0,0)->on()->all(HIGH)->delay(400);
    break;
    
  case 10:
     break;
*/
  }

  first = 0;
}          



