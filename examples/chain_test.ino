/*

 Test n chained TLC59116 shift-registers.
 
 */

#include <Wire.h>
#include <TLC59116.h>

// Addresses can be 0x60..0x6D (96..109), or the shorthand 0..13
// NB: 0x6B (11) is always the Reset_Addr
// NB: 0x68 is AllCall on power-up.
// NB: 0x69, 0x6A (10), and 0x6C (12) are the default group-addresses (disabled at power-up)
TLC59116 tlc_first;  // Lowest address found by a scan: auto!
TLC59116 tlc(0); // device whose address pins set to 0
TLC59116 tlc_allcall(TLC59116::AllCall_Addr); // all devices at once
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
  Serial.print(F("Serial initialized, cpu is hz "));Serial.println(F_CPU);

  // Must do this for TLC59116 stuff
  Serial.println(F("wire..."));
  Wire.begin();
  const long want_freq = 150000L; // 100000L; .. 340000L for 1, not termination
  TWBR = ((F_CPU / want_freq) - 16) / 2;
  // TWBR=20;
  Serial.print(F("Wire initialized at "));Serial.print(TWBR);Serial.println(F(" twbr, reset..."));

  // Reset to power-up defaults. Known state, fix weirdness.
  TLC59116::reset(); 

  // Outputs are off at power up
  tlc_first.enable_outputs();

  Serial.print(F("Free memory "));Serial.println(get_free_memory());
  Serial.print(F("Send '?' for menu of actions."));
}

extern int __bss_end;
extern void *__brkval;

int get_free_memory()
{
  int free_memory;

  if((int)__brkval == 0)
    free_memory = ((int)&free_memory) - ((int)&__bss_end);
  else
    free_memory = ((int)&free_memory) - ((int)__brkval);

  return free_memory;
}
const byte hump_values[] = { 0,10,80,255,80,10,0 };
const unsigned long hump_speed = 50; // time between changing. actually "slowness"

void loop() {
  static char test_num = 'w'; // idle pattern
  switch (test_num) {

    case 0xff: // prompt
      Serial.print(F("Choose (? for help): "));
      test_num = NULL;
      break;

    case NULL: // means "done with last, do nothing"
      break; // do it

    case 'm': // free Memory
      Serial.print(F("Free memory "));Serial.println(get_free_memory());
      test_num = 0xff;
      break;

    case 's': // Scan (cached) for addresses
      // Find out that TLC59116's are hooked up and working (and the broadcast addresses).
      TLC59116::scan().print();
      test_num = 0xff; // prompt;
      break;

    case 'S': // Scan (no cache) for addresses
      // Find out that TLC59116's are hooked up and working (and the broadcast addresses).
      TLC59116::Scan::scanner().rescan().print();
      test_num = 0xff; // prompt;
      break;


    case 'C': // Do all min/max for trimpot calibration
      TLC59116::reset();
      tlc_first.reset_shadow_registers();
      tlc_first.enable_outputs(); // before reset_shadow!

      do_sequence_till_input
        sequence(0, tlc_first.pattern(0xffff), 400)
        // sequence(1, tlc_first.pattern(0x0), 400)
        sequence(1, tlc_first.pwm((byte[]) {0x1,0x1,0x1,0x1,0x1,0x1,0x1,0x1,0x1,0x1,0x1,0x1,0x1,0x1,0x1,0x1}), 600)
      end_do_sequence
      test_num = 0xff;
      break;

    case 'd': // Describe first TLC59116
      tlc_first.describe();
      test_num = 0xff;
      break;

    case 'r': // Reset all TLC59116 to power up
      TLC59116::reset();
      Serial.println(F("Reset'd"));

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

      Serial.print(F("Blinked LED 0 of device # "));Serial.println(tlc_first.address() - TLC59116::Base_Addr);
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
      {
      tlc_first.enable_outputs(); // before reset_shadow!
      byte i;
      do_sequence_till_input
        sequence(0, tlc_first.pwm(i++ % 16, sizeof(hump_values), hump_values), hump_speed)
      end_do_sequence
      }
      break;

    case 'D': // dump Shadow registers of first
      for (byte i=0; i <= TLC59116::Control_Register_Max; i++) {
        Serial.print("@");Serial.print(i);Serial.print(" 0x");Serial.println(tlc_first.shadow_registers[i],HEX);
        }
      test_num = 0xff;
      break;

    case 'o': // all On (dim)
      all_on_dim();
      test_num = 0xff;
      break;

    case 'f' : // Flicker test (current test)
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

    case 'B' : // Global blink (osc) bogus brightness sometimes
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
      // TLC59116::reset();
      // tlc_first.reset_shadow_registers();
      tlc_first.enable_outputs(); // before reset_shadow!
      do_sequence_till_input
        sequence(0, tlc_first.pwm(0,4,(byte[]) {100,100,100,100}), 250);
        sequence(1, tlc_first.pwm(0,4,(byte[]) {25,25,25,25}), 250);
        sequence(2, tlc_first.pwm(8,4,(byte[]) {100,100,100,100}), 250);
        sequence(3, tlc_first.pwm(8,4,(byte[]) {25,25,25,25}), 250);
        sequence(4, tlc_first.pwm(4,4,(byte[]) {100,100,100,100}), 250);
        sequence(5, tlc_first.pwm(4,4,(byte[]) {25,25,25,25}), 250);
        sequence(6, tlc_first.pwm(12,4,(byte[]) {100,100,100,100}), 250);
        sequence(7, tlc_first.pwm(12,4,(byte[]) {25,25,25,25}), 250);
        sequence(8, tlc_first.pwm(0,4,(byte[]) {0,0,0,0}), 250);
        sequence(9, tlc_first.pwm(8,4,(byte[]) {0,0,0,0}), 250);
        sequence(10, tlc_first.pwm(4,4,(byte[]) {0,0,0,0}), 250);
        sequence(11, tlc_first.pwm(12,4,(byte[]) {0,0,0,0}), 250);
      end_do_sequence
      test_num = 0xff;
      break;

    case 'a' : // pwm full range using Allcall
      TLC59116::reset();
      tlc_first.enable_outputs(); // before reset_shadow!
      tlc_allcall.enable_outputs(); // before reset_shadow!
      pwm_full_range(tlc_allcall);
      TLC59116::reset();
      tlc_first.enable_outputs(); // before reset_shadow!
      tlc_first.reset_shadow_registers();
      test_num = 0xff;
      break;

    case 'p' : // Pwm full range
      pwm_full_range(tlc_first);
      test_num = 0xff;
      break;

    case 't' : // Time one-at-a-time vs. bulk, pwm
      time_each_vs_bulk_pwm();
      test_num = 0xff;
      break;
    
    case 'O' : // On/off, 4 at a time (bulk)
      tlc_first.enable_outputs(); // before reset_shadow!
      do_sequence_till_input
        sequence(0, tlc_first.pattern(0x000f,0x000f), 500);
        sequence(1, tlc_first.pattern(0x0f00,0x0f00), 500);
        sequence(2, tlc_first.pattern(0x00f0,0x00f0), 500);
        sequence(3, tlc_first.pattern(0xf000,0xf000), 500);
        sequence(4, tlc_first.pattern(0,0x000f), 500);
        sequence(5, tlc_first.pattern(0,0x0f00), 500);
        sequence(6, tlc_first.pattern(0,0x00f0), 500);
        sequence(7, tlc_first.pattern(0,0xf000), 500);
      end_do_sequence
      test_num = 0xff;
      break;

  case 'g': // group blink test
    tlc_first.pwm((byte[]){0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff});
    tlc_first.group_blink(0xffff, .25, 2.0);
    // tlc_first.group_pwm(0xffff, 129);
    while (Serial.available() <= 0) {}
    tlc_first.pattern(0x0);
    test_num = 0xff;
    break;

  case 'G': // group pwm test (cycle) flicker bug sometimes
    {
      // at 340000hz TWBR, with no delay(), up-down takes about 81msec. that's about 6 updates/msec!
      unsigned long elapsed;
      unsigned const slowness = 1;
      tlc_first.pwm((byte[]){0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff});
      tlc_first.group_pwm(0xffff, 0xff);
      while (Serial.available() <= 0) {
        // Serial.print("Down... ");
        elapsed = millis();
        for(byte i=255; ;i--) {
          // Serial.print(i);
          tlc_first.modify_control_register(TLC59116::GRPPWM_Register, i);
          if (slowness) delay(slowness);
          if (i==0) break;
          }
        // Serial.println("Up...");
        if (Serial.available() > 0) break;
        for(byte i=0; ;i++) {
          tlc_first.modify_control_register(TLC59116::GRPPWM_Register, i);
          if (slowness) delay(slowness);
          if (i==255) break;
          }
        Serial.print(F("Elapsed "));Serial.println(millis()-elapsed);
        }
    }
    // tlc_first.pattern(0x0);
    test_num = 0xff;
    break;

  case 'A' : // try an Allcall read
    // Works if there is a single TLC59116, you get the value you expect
    { 
    TLC59116::reset();
    tlc_allcall.enable_outputs();
    Serial.println(F("reset, on in .5"));
    delay(500);
    Serial.println(F("Alt pattern to prove allcall can write"));
    for(int i=0; i< 4;i++) {
      Serial.print(F("10 "));
      tlc_allcall.pattern(0xAAAA).delay(500);
      Serial.print(F("01 "));
      tlc_allcall.pattern(0x5555).delay(500);
      }

    Serial.println();
    Serial.print(F("Try a read of LEDOUT0, which should be 010001: "));
    Serial.println(tlc_allcall.control_register(TLC59116::LEDOUT0_Register),BIN);
    }
    Serial.println();

    Serial.println(F("Describe"));
    tlc_first.describe();

    TLC59116::reset();
    tlc_first.enable_outputs();
    tlc_first.reset_shadow_registers();
    Serial.println(F("Done"));
    test_num = 0xff;
    break;

    case 'e': // try to detect changes in the error_detect feature
      display_error_detect(true);
      break;

    default:
      Serial.println();
      // menu made by: make examples/.chain_test.ino.menu
Serial.println(F("m  free Memory"));
Serial.println(F("s  Scan (cached) for addresses"));
Serial.println(F("S  Scan (no cache) for addresses"));
Serial.println(F("C  Do all min/max for trimpot calibration"));
Serial.println(F("d  Describe first TLC59116"));
Serial.println(F("r  Reset all TLC59116 to power up"));
Serial.println(F("b  Blink led 0 of first TLC59116 a few times"));
Serial.println(F("c  Chase pattern (not pwm)"));
Serial.println(F("w  Wave chase pattern (pwm)"));
Serial.println(F("W  Same using bulk write"));
Serial.println(F("D  dump Shadow registers of first"));
Serial.println(F("o  all On (dim)"));
Serial.println(F("f  Flicker test (current test)"));
Serial.println(F("B  Global blink (osc) bogus brightness sometimes"));
Serial.println(F("P  Pwm, 4 at a time using bulk"));
Serial.println(F("a  pwm full range using Allcall"));
Serial.println(F("p  Pwm full range"));
Serial.println(F("t  Time one-at-a-time vs. bulk, pwm"));
Serial.println(F("O  On/off, 4 at a time (bulk)"));
Serial.println(F("g  group blink test"));
Serial.println(F("G  group pwm test (cycle) flicker bug sometimes"));
Serial.println(F("A  try an Allcall read"));
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

void on1() { tlc_first.on(0); }
void off1() { tlc_first.off(0); }
  
void next_idle_state() {
  // progress through the idle blinky pattern
  const unsigned long speed = 50; // time between changing. actually "slowness"
  static unsigned long last_time = 0;
  static byte chase_i = 0;

  if (last_time == 0) Serial.println(F("Idling..."));

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
    // Serial.println(F("Doit"));
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

      Serial.println(F("10 times each"));
      Serial.print(F("             "));
      for(byte ct = 1; ct<=16; ct++) { Serial.print(ct); Serial.print(ct<10 ? "  " : " " ); } Serial.println();

      for(byte digital=0; digital<2; digital++) {
        if(digital) { for(byte i=0; i<16; i++) tlc_first.off(i); }
        Serial.print(F("Singular pwm "));
        for(byte ct = 1; ct<=16; ct++) {
          from = millis();
          for(byte avg=0; avg<10; avg++) {
            for(byte i=0;i<ct;i++) {
              tlc_first.pwm(i,80);
              }
            }
          elapsed = millis() - from;
          if (elapsed<10) Serial.print(" "); Serial.print(elapsed);Serial.print(" ");
          }
        Serial.print(F("millis"));
        if (digital) Serial.print(F(" from digital"));
        Serial.println();

        if(digital) { for(byte i=0; i<16; i++) tlc_first.off(i); }
        Serial.print(F("Bulk     pwm "));
        for(byte ct = 1; ct<=16; ct++) {
          from = millis();
          for(byte avg=0; avg<10; avg++) {
            tlc_first.pwm(0,ct,(byte[]) {20,30,40,50,60,70,80,90,20,30,40,50,60,70,80,90});
            }
          elapsed = millis() - from;
          if (elapsed<10) Serial.print(" "); Serial.print(elapsed);Serial.print(" ");
          }
        Serial.print(F("millis"));
        if (digital) Serial.print(F(" from digital"));
        Serial.println();

        }
      }

void pwm_full_range(TLC59116 &tlc) {
  // while 0..7 goes up, 8..15 goes down & vice versa
  const byte size1 = 8;
  byte direction;
  byte values[16];
  unsigned long timer = millis();

  // init
  for(byte i=0; i < size1; i++) { values[i] = 0; }
  for(byte i=size1; i<=15; i++) { values[i] = 255; }

  // vary
  while (Serial.available() == 0) {
    // Serial.print(F("Set 0..15 "));for(byte i=0; i<16; i++) {Serial.print(values[i]);Serial.print(" ");}
    tlc.pwm(0,16, values);
    // Serial.print(F(" next "));Serial.print(direction);Serial.println();

    if (values[0] >= 255) { direction = -1; }
    else if (values[0] <= 0) { 
      Serial.print(F("Elapsed "));Serial.print(millis()-timer);Serial.println();
      timer = millis();
      direction = +1; 
      }

    for(byte i=0; i < size1; i++) { values[i] += direction; }
    for(byte i=size1; i<=15; i++) { values[i] -= direction; }
    
    }
  }

void display_error_detect(bool doovertemp) {
  unsigned long last_error_bits = 0xffff0000; // for first time
  byte out_ct = 10;

  while (Serial.available() <= 0) {
    unsigned int error_bits = tlc_first.error_detect(doovertemp);
    // error_bits |= random(2);

    if (last_error_bits != error_bits) {
      if (!(out_ct % 10)) {
        Serial.println(F("Channels  0  1  2  3  4  5  6  7  8  9  10 11 12 13 14 15"));
        }
      out_ct++;
      last_error_bits = error_bits;

      if (doovertemp)
        Serial.print(F(  "Over Temp "));
      else
        Serial.print(F(  "Open      "));

      for(byte i=0; i < (sizeof(error_bits) * 8); i++) {
        if(doovertemp)
          Serial.print( (error_bits & (1U << (sizeof(error_bits) * 8 - 1))) ? ">  " : "-  ");
        else
          Serial.print( (error_bits & (1U << (sizeof(error_bits) * 8 - 1))) ? "O  " : "-  ");
        error_bits <<= 1;
        }
      Serial.println();
      }

    }
  }

