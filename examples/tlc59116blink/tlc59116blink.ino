/* Simple test with TLC59116

  Test w/arduino-pin-7 first, to check your LED

  On the TLC59116, 
  test channel 0-3 for blink
    play with the trimpot for brightness
  then channel 4-7 for a triangle ramp for brightness
  then channel 8-11 for brightness

  These are at Arduino voltage, usually 3v!
  So, typical LED of 20ma means resistor of 125ohms
  We are using pins as SINK, so LED- to the arduino pins
  Arduino Pin 7 : short blink for "safe" test of LED
  Arduino Pin 13 : Follows 7 (on board LED) (source, so HIGH is on)

  These are the TLC59116:
  At BAT=5v, a LED of 20ma means resistor 250ohm
  Output 0,1,2,3: short blink for "safe" test of LED
  Output 4,5,6,7: triangle ramp
  Output 8,9,10,11: On
  Output 12,13,14,15: Off
*/

#include <Wire.h>
#include <avr/pgmspace.h>

#include "TLC59116.h"
TLC59116Manager tlcmanager; // (Wire, 50000); // defaults

void setup() {
    Serial.begin(9600);
    tlcmanager.init();
    Serial.println("STARTED");
    pinMode(7,OUTPUT);
    pinMode(5,OUTPUT);
    digitalWrite(5,HIGH);
    pinMode(13,OUTPUT);
    tlcmanager[0].set_pattern(0xF << 8); // 8..11 on
    // tlcmanager[0].set_milliamps(0, 150);
}

void loop() {
  static TLC59116 &t = tlcmanager[0];

  do_blinks(t);
  do_triangles(t);

  }

void do_blinks(TLC59116 &t) {
  static unsigned long blink_cycle_start = 0;
  const static int On_Time = 3; // elapsed from beginning of cycle. empirical to keep led on very short
  const static int Off_Time = On_Time + 150; // elapsed from beginning of cycle

  if (millis() - blink_cycle_start > Off_Time) {
    digitalWrite(7,LOW); // for sink, low is on
    digitalWrite(13,HIGH);
    t.set_pattern(0xF); // 0..3 blink
    blink_cycle_start = millis();
    }
  else if (millis() - blink_cycle_start > On_Time) {
    digitalWrite(7,HIGH);
    digitalWrite(13,LOW);
    t.reset_pattern(0xF);
    }
  }

void do_triangles(TLC59116 &t) {
  static long triangle_value = 1;
  triangle(triangle_value, 6, 0,255, 5,5, false);
  t.pwm(4,7,abs(triangle_value));
  }

#define debug(msg)
#define debugln(msg)

void triangle(long &state, int pin, int min, int max, int up_inc, int down_inc, boolean once) {
  // Set state to 1 to start running. We will set state to 0 when we want to stop!
  debug(min);debug('<');debug(state);debug('<');debug(max);debug(' ');
  if (state < 0) {
    debug("N<");debug(min);debug(' ');
    state += down_inc;
    if (state >= -min) {
      if (once) {
        debug("once'd\n");
        analogWrite(pin, 0);
        state = 0; // mark not running
        return;
        // return true; // done
        }
      else {
        state = min;
        }
      }
    }
  else {
    debug("N>");debug(min);debug(' ');
    if (state == max) {
      debug("peak'd ");
      state = -max; // was max last time, so wrap
      }
    else {
      state += up_inc;
      if (state > max) {
        state = max; // clip to max, and allow it for next time
        }
      }
    }
  if (state == 0) state = 1; // don't allow 0
  analogWrite(pin, abs(state));    
  debug("=");debug(state);debugln();
  // return false;
  }
