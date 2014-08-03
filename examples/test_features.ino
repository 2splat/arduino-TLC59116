#include <Wire.h>
#include <avr/pgmspace.h>

#define TLC59116_DEV 1
#define TLC59116_WARNINGS 1
#include "TLC59116.h"
#include "simple_sequence.h"

TLC59116Manager tlcmanager; // defaults

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

    case '?' : // display menu
      Serial.println();
      Serial.print(F("Free memory "));Serial.println(get_free_memory());
      Serial.print("Using 0x");Serial.print(tlc->address(),HEX);Serial.println();
      // menu made by: make (in examples/, then insert here)
Serial.println(F("=  pick a device"));
Serial.println(F("b  Blink with an alternating pattern"));
Serial.println(F("?  display menu"));
      // end insert
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
