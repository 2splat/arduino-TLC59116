#include <Wire.h>
#include <avr/pgmspace.h>

#define TLC59116_DEV 1
#define TLC59116_WARNINGS 1
#include "TLC59116.h"
#include "simple_sequence.h"

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

    case '=' : // pick a device
      tlc = pick_device(*tlc);
      test_num = '?';
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
        sequence(0, tlc->pwm(i++ % 16, sizeof(Hump_Values), Hump_Values), Hump_Speed)
      end_do_sequence
      }
      test_num = 0xff;
      break;

    case '?' : // display menu
      Serial.println();
      Serial.print(F("Free memory "));Serial.println(get_free_memory());
      Serial.print("Using 0x");Serial.print(tlc->address(),HEX);Serial.println();
      // menu made by: make (in examples/, then insert here)
Serial.println(F("=  pick a device"));
Serial.println(F("r  Reset all"));
Serial.println(F("d  Dump actual device"));
Serial.println(F("D  Dump shadow registers"));
Serial.println(F("b  Blink with an alternating pattern"));
Serial.println(F("o  test On/Off bit pattern"));
Serial.println(F("O  test on off by led-num"));
Serial.println(F("w  Wave using pwm, individual .pwm(led_num, brightness)"));
Serial.println(F("W  Wave using pwm, bulk"));
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

TLC59116 *pick_device(TLC59116 &was) {
  TLC59116 *tlc = &was;

  char x = 'z'; // --none--

  Serial.println();
  while (x >= tlcmanager.device_count() && x != '*' && x != '|') {
    // show the list, including broadcast, marking with "="
    // take 0..F from list, or '*' for broadcast, '|' for .each
    for(byte i=0; i < tlcmanager.device_count(); i++) {
      Serial.print( tlc == &tlcmanager[i] ? '=' : ' ');
      Serial.print(i,HEX);Serial.print(" Device at 0x");Serial.print(tlcmanager[i].address(),HEX);Serial.print("/");Serial.print(tlcmanager[i].address() - TLC59116::Base_Addr);
      Serial.println();
      }
    Serial.print(tlc == &tlcmanager.broadcast() ? '=' : ' '); Serial.println("* Broadcast");
    Serial.print(tlc == NULL ? '=' : ' ');Serial.println("| Each individually");

    Serial.print("Choose device: ");
    while (Serial.available() <=0) {};
    x = Serial.read();
    Serial.println(x);

    if (x<0) x='z';
    else if (x >= '0' && x <= '9') x=x-'0';
    else if (x>='A' && x<='F') x=x-'A'+10;
    }
  TLC59116Warn(F(" chose i "));TLC59116Warn((byte)x,HEX); TLC59116Warn();
  if (x < tlcmanager.device_count()) tlc = &tlcmanager[x];
  else if (x=='*') tlc = &tlcmanager.broadcast();
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
