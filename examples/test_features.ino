#include <Wire.h>
#include <avr/pgmspace.h>

#define TLC59116_DEV 1
#define TLC59116_WARNINGS 1
#include "TLC59116.h"
#include "simple_sequence.h"
#include "tired_of_serial.h"

TLC59116Manager tlcmanager; // defaults

extern int __bss_end;
extern void *__brkval;

const byte Hump_Values[] = { 0,10,80,255,80,10,0 };
const unsigned long Hump_Speed = 50; // time between changing. actually "slowness"

int get_free_memory()
{
  int free_memory;

  if((int)__brkval == 0)
    free_memory = ((int)&free_memory) - ((int)&__bss_end);
  else
    free_memory = ((int)&free_memory) - ((int)__brkval);

  return free_memory;
}

void setup() {
  Serial.begin(115200);
  Serial.println("Top of setup");
  Serial.print(F("Free memory "));Serial.println(get_free_memory());
  tlcmanager.init();
  Serial.print(F("Free memory after init "));Serial.println(get_free_memory());
  }

void loop() {
  static TLC59116 *tlc = &tlcmanager[0];
  static char test_num = 'b'; // idle pattern

  switch (test_num) {

    case 'l' : // List devices: addresses & output enablement
      list_devices();
      test_num = 0xff;
      break;

    case '!' : // pick a device: hex, *=broadcast, |=each
      Serial.println();
      tlc = pick_device(*tlc); Serial.println();
      test_num = '?';
      break;

    case '=' : // set on/off, Broadcast Address, SubAddress (prompts)
      set_something(tlc);
      test_num = 0xFF;
      break;

    case 'r' : // Reset all
      tlcmanager.reset();
      test_num = 0xff;
      break;

    case 'd' : // Dump actual device
      Serial.println();
      tlc->describe_actual();
      test_num = 0xff;
      break;

    case 'D' : // Dump shadow registers
      Serial.println();
      tlc->describe_shadow();
      test_num = 0xff;
      break;

    case 'R' : // Resync shadow-registers (fetch), each
      resync();
      test_num = 0xff;
      break;

    case 'b' : // Blink with an alternating pattern
      do_sequence_till_input
        sequence(0, tlc->pattern(0x5555), 250)
        sequence(1, tlc->pattern(0xAAAA), 250)
      end_do_sequence
      break;

    case 'o' : // test On/Off bit pattern
      // Shows that on/off doesn't disturb other channels
      while (Serial.available() <= 0) {
        tlc->on(0xFFFF).delay(200);
        word offp = 0b1;
        word onp = 0b1;
        for(byte i=0; i<16; i++) {
          tlc->off(offp);
          delay(200);
          offp <<= 1;
          if (Serial.available()>0) break;
          }
        for(byte i=0; i<16; i++) {
          tlc->on(onp);
          delay(200);
          onp <<= 1;
          if (Serial.available()>0) break;
          }
        }
      test_num = '?';
      break;

    case 'O' : // test on off by led-num
      while (Serial.available() <= 0) {
        for(byte i=0; i<16; i++) tlc->set(i,HIGH);
        delay(200);

        for(byte i=0; i<16; i++) {
          tlc->set(i,LOW);
          delay(200);
          if (Serial.available()>0) break;
          }
        for(byte i=0; i<16; i++) {
          tlc->set(i,HIGH);
          delay(200);
          if (Serial.available()>0) break;
          }
        }
      test_num = '?';
      break;

    case 'w' : // Wave using pwm, individual .pwm(led_num, brightness)
      pwm_wave(*tlc);
      test_num = 0xff;
      break;

    case 'W' : // Wave using pwm, bulk
      {
      byte i;
      do_sequence_till_input
        // me so tricky
        sequence(0, tlc->pwm(i % 16, (i%16)+sizeof(Hump_Values)-1, Hump_Values), Hump_Speed)
        sequence(1, i++, 0);
      end_do_sequence
      }
      test_num = 0xff;
      break;

    case 'B' : // Blink using group_blink, will run on its own
      tlc->pwm(0, TLC59116::Channels-1, 128);
      {
      int i=0;
      do_sequence_till_input
        sequence(0, Serial.print("---> "), 0);
        sequence(1, Serial.println(13-(i % 13)), 0);
        sequence(2, tlc->group_blink(0xFFFF, 13-(i++ % 13), 10.0), 0);
        sequence(3, debug_actual_group_mode(*tlc), 400);
      end_do_sequence
      }
      break;
      do_sequence_till_input
        sequence(0, tlc->pwm(0, TLC59116::Channels-1, 128), 0); // FIXME: Channels-1 is not friendly
        sequence(1, tlc->group_blink(0xFFFF, 6, 128), 3000);
        sequence(2, tlc->on(0xAAAA), 1000);
        sequence(3, tlc->pwm(0, TLC59116::Channels-1, 100), 0);
        sequence(4, tlc->group_blink(0xFFFF, 1, 50), 4000);
      end_do_sequence
      break;

    case '?' : // display menu
      Serial.println();
      Serial.print(F("Free memory "));Serial.println(get_free_memory());
      Serial.print("Using 0x");Serial.print(tlc->address(),HEX);Serial.println();
      // menu made by: make (in examples/, then insert here)
Serial.println(F("l  List devices: addresses & output enablement"));
Serial.println(F("!  pick a device: hex, *=broadcast, |=each"));
Serial.println(F("=  set on/off, Broadcast Address, SubAddress (prompts)"));
Serial.println(F("r  Reset all"));
Serial.println(F("d  Dump actual device"));
Serial.println(F("D  Dump shadow registers"));
Serial.println(F("R  Resync shadow-registers (fetch), each"));
Serial.println(F("b  Blink with an alternating pattern"));
Serial.println(F("o  test On/Off bit pattern"));
Serial.println(F("O  test on off by led-num"));
Serial.println(F("w  Wave using pwm, individual .pwm(led_num, brightness)"));
Serial.println(F("W  Wave using pwm, bulk"));
Serial.println(F("B  Blink using group_blink, will run on its own"));
Serial.println(F("?  display menu"));
      // end menu
      // fallthrough

    case 0xff : // show prompt, get input
      Serial.print(F("Choose (? for help): "));
      while(Serial.available() <= 0) {}
      test_num = Serial.read();
      Serial.println(test_num);
      break;

    default : 
      test_num = '?';
      break;

    }
  if (Serial.available() > 0) test_num = Serial.read();

  }

byte is_hex_digit(char d) { 
  if (d >= '0' && d <= '9') return d - '0';
  else if (d >= 'a' && d <= 'f') return d - 'a';
  else if (d >= 'A'  && d <= 'F') return d - 'A' + 10;
  else return d;
  }

TLC59116 *pick_device(TLC59116 &was) {
  TLC59116 *tlc = &was;

  char x = 'z'; // --none--

  bool type_ahead = Serial.available() > 0;
  while (x >= tlcmanager.device_count() && x != '*' && x != '|' && x != '=') {
    if (!type_ahead) {
      // show the list, including broadcast, marking with "="
      // take 0..F from list, or '=' for no change, '*' for broadcast, '|' for .each
      Serial.println("* Broadcast");
      for(byte i=0; i < tlcmanager.device_count(); i++) {
        Serial.print( tlc == &tlcmanager[i] ? '=' : ' ');
        Serial.print(i,HEX);Serial.print(" Device at ");printw(tlcmanager[i].address(),HEX);Serial.print("/");Serial.print(tlcmanager[i].address() - TLC59116::Base_Addr);
        Serial.println();
        }
      Serial.print(tlc == &tlcmanager.broadcast() ? '=' : ' '); Serial.println("* Broadcast");
      Serial.print(tlc == NULL ? '=' : ' ');Serial.println("| Each individually");
      }

    Serial.print("Choose device: ");
    x = Serial.read(); Serial.println(x);

    x = is_hex_digit(x);
    if (x > 0xf || x < 0) x='z';
    }
  TLC59116Warn(F(" chose i "));TLC59116Warn((byte)x,HEX); TLC59116Warn();
  if (x < tlcmanager.device_count()) tlc = &tlcmanager[x];
  else if (x=='*') tlc = &tlcmanager.broadcast();
  else if (x=='=') tlc = &was;
  else if (x=='|') tlc = NULL; // FIXME
  else Serial.println("What?");

  return tlc;
  }

void pwm_wave(TLC59116& tlc) {
  // 6 leds 80,160,240,160,80 that chase around
  // Writing individual leds
  unsigned long before_time = 0;
  byte chase_i = 0;

  while(Serial.available() <= 0) {
    // Serial.print("at ");Serial.print((chase_i+3) % 16);Serial.print(" ");
    before_time = millis();
    for (char i = -3; i <= 3; i++) {
      byte led_num = (chase_i + 3 + i) % 16;
      byte pwm =  Hump_Values[i + 3];
      // Serial.print(pwm);Serial.print(" ");
      tlc.pwm(led_num, pwm);
      }
    // Serial.println(F("Doit"));
    chase_i = (chase_i + 1) % 16; // 0..15

    unsigned long used = millis() - before_time;
    if (Hump_Speed > used) delay(Hump_Speed - used);
    else { Serial.print("!Took ");Serial.println(used); }
    }
  }

void debug_actual_group_mode(TLC59116& tlc) {
  // minimal fetch
  byte registers[31]; //[TLC59116::GRPFREQ_Register+1];
  // tlc.fetch_registers(TLC59116::MODE1_Register, 2, registers);
  // tlc.fetch_registers(TLC59116::GRPPWM_Register, 2, registers);
  tlc.fetch_registers(0, 31, registers);
  tlc.describe_group_mode(registers);
  }

void resync() {
  for(byte i=0; i < tlcmanager.device_count(); i++) {
    TLC59116& tlc = tlcmanager[i];
    tlc.resync_shadow_registers();
    Serial.print("Synced 0x");Serial.println(tlc.address(),HEX);
    }
  }

void list_devices() {
  Serial.println("Addr on Broadcast Sub1 Sub2 Sub3"); // header
  for(byte i=0; i < tlcmanager.device_count(); i++) {
    TLC59116& tlc = tlcmanager[i];
    printw(tlc.address(), HEX); print(" ");
    tlc.is_enabled() ? print("+ ") : print("- ");
    print("     ");printw(tlc.allcall_address(),HEX);
    printw(tlc.SUBADR_address(1),HEX);print(" ");
    printw(tlc.SUBADR_address(2),HEX);print(" ");
    printw(tlc.SUBADR_address(3),HEX);print(" ");
    Serial.println();
    }
  }

void set_something(TLC59116 *tlc) {
  // set something, prompting for the little-language

  // :: address = device# | = | * | '|' (specific, current, broadcast, each)
  tlc = pick_device(*tlc);
  // :: setall | setsub | setenabled

  print("set: a allcall-addr, s subaddr, +/- output enable: ");
  char what = Serial.read(); Serial.println(what);

  switch (what) {
    case 'a' :
      // handle setting allcall +/-, or to address
      {
      print("allcall address: hex_digit set as +0x60, +/- enable: ");
      char x = Serial.read(); Serial.println(x);

      if (x=is_hex_digit(x)) tlc->allcall_address(0x60 + x, true); // and enable
      else if (x == '+' || x == '-') tlc->allcall_address( x=='+' );
      else Serial.println("What? ");
      }
      break;

    case 's' :
      // handle setting nth subadr +/-, or to address
      {
      print("subcall #: 1, 2 or 3: ");
      char x = Serial.read(); print(x); Serial.println();

      if (x == '1' || x=='2' || x=='3') {
        byte subadr_i = x - '0';
        
        print("subadr #");print(subadr_i);print(" address: hex_digit set as +0x60, +/- enable: "); 
        x = Serial.read(); print(x); Serial.println();
       
        if (x=is_hex_digit(x)) tlc->SUBADR_address(subadr_i, 0x60 + x, true); // and enable
        else if (x == '+' || x == '-') tlc->SUBADR_address(subadr_i, x=='+' );
        else Serial.println("What? ");
        }
      else Serial.println("What? ");
      }
      break;

    // set output enable +/-
    case '+' :
      tlc->enable_outputs(true);
      break;
    case '-' :
      tlc->enable_outputs(false);
      break;

    default :
      Serial.println("What?");
    }
  while (Serial.available() > 0) Serial.read(); // exhaust, because dangerous if "syntax error"
  }
