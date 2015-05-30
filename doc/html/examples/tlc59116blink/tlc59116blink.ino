/* Simple test with TLC59116

  Test w/arduino-pin-7 first, to check your LED

  Then, on each TLC59116, 
  test channel 0-3 for blink
    The blink rate is different for each TLC (longer as the address increases)
    play with the trimpot for brightness
  then channel 4-7 for a triangle ramp for brightness
  then channel 8-11 for brightness

  These are at Arduino voltage, usually 3v:
  So, typical LED of 20ma means resistor of 125ohms
  Arduino Pin 7 : short blink for "safe" test of LED
    as SINK, LOW is on, and LED- to the arduino pin
  Arduino Pin 13 : Follows 7 (on board LED) (source, so HIGH is on)
    So, 7 should blink along with 13
  Arduino Pin 6 is doing the triangle ramp, but it's HIGH on, so SOURCE not SINK

  These are the TLC59116:
  At BAT=5v, a LED of 20ma means resistor 250ohm
  Output 0,1,2,3: short blink for "safe" test of LED
    Blinks n-times, then pause.
    'n' is the cardinal position of the TLC in the tlcmanager[]
    Thus, the tlcmanager[0] device has an 'n' of 1.
    This is set per TLC (individual addressed).
  Output 4,5,6,7: triangle ramp
    Increments brightness up/down by 1 on each "loop".
    All TLC at same rate.
    This is done by broadcast, not individually.
    This reflects the Wire speed, 50khz is noticeably slower than 500khz.
  Output 8,9,10,11: On
  Output 12,13,14,15: Off

  You can slow down the I2C speed (100khz default), look for the
    TLC59116Manager tlcmanager;
  below. Add arguments for speed setting, e.g.
    TLC59116Manager tlcmanager(Wire, 50000); // you have to say 'Wire', and then 50khz
  Check the console for the actual speed (only certain values possible).
*/

#include <Wire.h>
#include <avr/pgmspace.h>

#include "TLC59116.h"
TLC59116Manager tlcmanager(Wire, 100000); // see the I2C_Speed.xls spread sheet for workable speeds

#include "BlinkTracking.h"

void setup() {
    Serial.begin(9600);
    pinMode(7,OUTPUT);
    pinMode(5,OUTPUT);
    digitalWrite(5,HIGH);
    pinMode(13,OUTPUT);
    Serial.println("setup().arduino done");

    tlcmanager.init();
    tlcmanager.broadcast().on_pattern(0xF << 8); // 8..11 on
    // tlcmanager[0].set_milliamps(0, 150);
    Serial.println("setup().tlc done");

    BlinkTracking::init_tracking();

    Serial.println("setup() done");
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

  unsigned long now = millis();
  if (now - blink_cycle_start > Off_Time) {
    digitalWrite(7,LOW); // for sink, low is on
    digitalWrite(13,HIGH);
    // do individual addressing to prove it works (not just broadcast)
    blink_cycle_start = now;
    }
  else if (now - blink_cycle_start > On_Time) {
    digitalWrite(7,HIGH);
    digitalWrite(13,LOW);
    }

  BlinkTracking::update(now);
  }

void do_triangles(TLC59116 &t) {
  static long triangle_value = 1;
  triangle(triangle_value, 0,255, 1,1, false);
  // not doing individual addressing for triangle.
  tlcmanager.broadcast().pwm(4,7,abs(triangle_value));
  // For pin 6 triangle (source not sink)
  analogWrite(6, abs(triangle_value));
  }

#define debug(msg)
#define debugln(msg)

void triangle(long &state, int min, int max, int up_inc, int down_inc, boolean once) {
  // Set state to 1 to start running. We will set state to 0 when we want to stop!
  // debug(min);debug('<');debug(state);debug('<');debug(max);debug(' ');
  if (state < 0) {
    // debug("N<");debug(min);debug(' ');
    state += down_inc;
    if (state >= -min) {
      if (once) {
        /// debug("once'd\n");debugln();
        state = 0; // mark not running
        return;
        // return true; // done
        }
      else {
        /// debug("~peak'd ");debugln();
        state = min;
        }
      }
    }
  else {
    // debug("N>");debug(min);debug(' ');
    if (state == max) {
      /// debug("peak'd ");debugln();
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
  // debug("=");debug(state);debugln();
  // return false;
  }
