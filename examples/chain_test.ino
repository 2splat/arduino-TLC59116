/*

 Test n chained TLC59116 shift-registers.
 
 Set the test
 
 */

#include <Wire.h>
#include <TLC59116.h>

// Addresses can be 0x60..0x6D (96..109), or the shorthand 0..13
// NB: 0x6B (11) is always the Reset_Addr
// NB: 0x68 is AllCall on power-up.
// NB: 0x69, 0x6A (10), and 0x6C (12) are the default group-addresses (disabled at power-up)
TLC59116 tlc_first;  // Lowest address found by a scan: auto!
TLC59116 tlc(0); // device whose address pins set to 0
TLC59116 tlc4(4); // device whose address pins set to 4
TLC59116 tlc5(0x65); // device whose address pins set to 5
TLC59116::Each tlcs; // each from scanning, but not broadcast
TLC59116::Broadcast tlc_broadcast; // everybody-at-once (AllCall), no reading
TLC59116::Group1 tlc_group1; // everybody-at-once who is on SUBADR1

// a little sequential state machine
// start_sequence sequence(#, fn(), delay)... end_sequence
#define do_sequence_till_input while (Serial.available() == 0) { start_sequence
#define start_sequence { \
  static byte sequence_i = 0; \
  static unsigned long sequence_next = 0; \
  if (millis() >= sequence_next) { \
    /* Serial.print(F("Sequence "));Serial.println(sequence_i); */ \
    switch (sequence_i) { \
      case -1 : /* dumy */
#define sequence(sofar, fn, delay_millis) break; \
      case sofar : sequence_i++; sequence_next = millis() + delay_millis; fn;
#define end_sequence \
      default: sequence_i=0; /* wrap i */ \
      } \
    /* Serial.print(F("Next seq "); Serial.print(sequence_i); Serial.print(F(" in "));Serial.println(sequence_next - millis()); */ \
    } \
  }
#define end_do_sequence end_sequence }

void setup() {
  // For debugging output:
  TLC59116::DEBUG=1;   // change to 0 to skip warnings/etc.

  // This example has the arduino ask you what to do,
  // (open the serial monitor window).
  Serial.begin(115200);

  // Must do this for TLC59116 stuff
  Wire.begin();

  // Reset to power-up defaults. Known state, fix weirdness.
  TLC59116::reset(); 

  // Outputs are off at power up
  tlc_first.enable_outputs();

  Serial.print("Send '?' for menu of actions.");
}

const byte hump_values[] = { 0,10,80,255,80,10,0 };
const unsigned long hump_speed = 50; // time between changing. actually "slowness"

void loop() {
  static char test_num = 'w'; // idle pattern
  switch (test_num) {

    case 0xff: // prompt
      Serial.print("Choose (? for help): ");
      test_num = NULL;
      break;

    case NULL: // means "done with last, do nothing"
      break; // do it

    case 's': // Scan for addresses
      // Find out that TLC59116's are hooked up and working (and the broadcast addresses).
      TLC59116::scan().print();
      test_num = 0xff; // prompt;
      break;

    case 'd': // Describe first TLC59116
      tlc_first.describe();
      test_num = 0xff;
      break;

    case 'r': // Reset all TCL59116 to power up
      TLC59116::reset();
      Serial.println("Reset'd");

      tlc_first.reset_shadow_registers();
      tlc_first.enable_outputs(); // before reset_shadow!
      test_num = 0xff;
      break;

    case 'b': // Blink led 0 of first TLC59116 a few times
      for(int i =0; i < 4; i++) {
        tlc_first.on(0).delay(100)
        .off(0).delay(200)
        ;
        }

      Serial.print("Blinked LED 0 of device # ");Serial.println(TLC59116::Base_Addr - tlc_first.address());
      test_num = 0xff;
      break;

    case 'c': // Chase pattern (not pwm)
      next_idle_state();
      break;

    case 'w': // Wave chase pattern (pwm)
      next_hump_state();
      break;

    case 'W': // Same using bulk write
      // TLC59116::reset();
      // tlc_first.reset_shadow_registers();
      tlc_first.enable_outputs(); // before reset_shadow!
      {
      byte i;
      do_sequence_till_input
        sequence(0, tlc_first.pwm(i++ % 16, sizeof(hump_values), hump_values), hump_speed)
      end_do_sequence
      }
      break;

    case 'S': // dump Shadow registers of first
      for (byte i=0; i <= TLC59116::Control_Register_Max; i++) {
        Serial.print("@");Serial.print(i);Serial.print(" 0x");Serial.println(tlc_first.shadow_registers[i],HEX);
        }
      test_num = 0xff;
      break;

    case 'o': // all On (dim)
      all_on_dim();
      test_num = 0xff;
      break;

    case 'f' : // Flicker test (bug)
      start_sequence
        sequence(0, flicker_test_reset(), 0)
        sequence(1, on1(), 250)
        sequence(2, off1(), 250)
        sequence(3, on1(), 250)
        sequence(4, off1(), 250)
        sequence(5, on1(), 250)
        sequence(6, off1(), 250)
      end_sequence
      break;

    case 'B' : // Global blink (osc)
      TLC59116::reset();
      tlc_first.reset_shadow_registers();
      tlc_first.enable_outputs(); // before reset_shadow!
      all_on_dim();
      do_sequence_till_input
        sequence(0, tlc_first.enable_outputs(), 500)
        sequence(1, tlc_first.enable_outputs(false), 500)
      end_do_sequence
      test_num = 0xff;
      break;
    
    case 'P' : // Pwm, 4 at a time using bulk
      TLC59116::debug("Try bin ");TLC59116::debug(0b10101010,BIN);TLC59116::debug();
      // TLC59116::reset();
      // tlc_first.reset_shadow_registers();
      tlc_first.enable_outputs(); // before reset_shadow!
      do_sequence_till_input
        sequence(0, tlc_first.pwm(0,4,(byte[]) {100,100,100,100}), 500);
        sequence(1, tlc_first.pwm(8,4,(byte[]) {100,100,100,100}), 500);
        sequence(2, tlc_first.pwm(4,4,(byte[]) {100,100,100,100}), 500);
        sequence(3, tlc_first.pwm(12,4,(byte[]) {100,100,100,100}), 500);
        sequence(4, tlc_first.pwm(0,4,(byte[]) {0,0,0,0}), 500);
        sequence(5, tlc_first.pwm(8,4,(byte[]) {0,0,0,0}), 500);
        sequence(6, tlc_first.pwm(4,4,(byte[]) {0,0,0,0}), 500);
        sequence(7, tlc_first.pwm(12,4,(byte[]) {0,0,0,0}), 500);
      end_do_sequence
      test_num = 0xff;
      break;

    case 't' : // Time one-at-a-time vs. bulk, pwm
      time_each_vs_bulk_pwm();
      test_num = 0xff;
      break;
  /*

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

    default:
      Serial.println();
      // menu made by: make examples/chain_test.ino.menu
      Serial.println(F("s  Scan for addresses"));
      Serial.println(F("d  Describe first TLC59116"));
      Serial.println(F("r  Reset all TCL59116 to power up"));
      Serial.println(F("b  Blink led 0 of first TLC59116 a few times"));
      Serial.println(F("c  Chase pattern (not pwm)"));
      Serial.println(F("w  Wave chase pattern (pwm)"));
      Serial.println(F("W  Same using bulk write"));
      Serial.println(F("S  dump Shadow registers of first"));
      Serial.println(F("o  all On (dim)"));
      Serial.println(F("f  Flicker test (bug)"));
      Serial.println(F("B  Global blink (osc)"));
      Serial.println(F("P  Pwm, 4 at a time using bulk"));
      Serial.println(F("t  Time one-at-a-time vs. bulk, pwm"));
            // end-menu

      Serial.println(F("? Prompt again"));
      test_num = 0xFF; // prompt
    }

  // Change test_num?
  if (Serial.available() > 0) {
    test_num = Serial.read();
    Serial.println(test_num);
    }

}          

void flicker_test_reset() {
  const byte pwm_bug_level = 20;
  static byte pwm_number = 15;

  for (byte i=1; i <= 15; i++) { // leave #1 alone
    if (i <= pwm_number) tlc_first.pwm(i, pwm_bug_level);
    else tlc_first.off(i);
    }
  pwm_number = (pwm_number - 1 ) % 16;
  }

void on1() { tlc_first.on(1); }
void off1() { tlc_first.off(1); }
  
void next_idle_state() {
  // progress through the idle blinky pattern
  const unsigned long speed = 50; // time between changing. actually "slowness"
  static unsigned long last_time = 0;
  static byte chase_i = 0;

  if (last_time == 0) Serial.println("Idling...");

  // time for next ?
  unsigned long now = millis();
  if (now > last_time + speed) {
    // 2 calls isn't that inefficient...
    tlc_first.on(chase_i)
    .off((chase_i + 15) % 16); // turn off the last one
    chase_i = (chase_i + 1) % 16; // 0..15
    last_time = now;
    }
  }

void next_hump_state() {
  // 6 leds 80,160,240,160,80 that chase around
  static unsigned long last_time = 0;
  static byte chase_i = 0;

  // time for next ?
  unsigned long now = millis();
  if (now > last_time + hump_speed) {
    // Serial.print("at ");Serial.print((chase_i+3) % 16);Serial.print(" ");
    for (char i = -3; i <= 3; i++) {
      byte led_num = (chase_i + 3 + i) % 16;
      byte pwm =  hump_values[i + 3];
      // Serial.print(pwm);Serial.print(" ");
      tlc_first.pwm(led_num, pwm);
      }
    // Serial.println("Doit");
    chase_i = (chase_i + 1) % 16; // 0..15
    last_time = now;
    }
  }


void all_on_dim() {
  for (byte i=0; i< 16;i++) {
    tlc_first.pwm(i, 50);
    }
  }

void time_each_vs_bulk_pwm() {
      unsigned long from;
      unsigned long elapsed;

      Serial.print(F("             "));
      for(byte ct = 1; ct<=16; ct++) { Serial.print(ct); Serial.print(ct<10 ? " " : "" ); } Serial.println();

      for(byte digital=0; digital<2; digital++) {
        if(digital) { for(byte i=0; i<16; i++) tlc_first.off(i); }
        Serial.print(F("Singular pwm "));
        for(byte ct = 1; ct<=16; ct++) {
          from = millis();
          for(byte i=0;i<ct;i++) {
            tlc_first.pwm(i,80);
            }
          elapsed = millis() - from;
          Serial.print(elapsed);Serial.print(" ");
          }
        Serial.print(F("millis"));
        if (digital) Serial.print(F(" from digital"));
        Serial.println();

        if(digital) { for(byte i=0; i<16; i++) tlc_first.off(i); }
        Serial.print(F("Bulk     pwm "));
        for(byte ct = 1; ct<=16; ct++) {
          from = millis();
          tlc_first.pwm(0,ct,(byte[]) {20,30,40,50,60,70,80,90,20,30,40,50,60,70,80,90});
          elapsed = millis() - from;
          Serial.print(elapsed);Serial.print(" ");
          }
        Serial.print(F("millis"));
        if (digital) Serial.print(F(" from digital"));
        Serial.println();

        }
      }
