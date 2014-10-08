#define TLC59116_h

// Shadow the devices state, take them into account for high-level operations
// So you don't have to: less bugs, more convenience.
// Less confusion by allowing "." instead of "->"
// Useful "default" forms to get started easier.
// Warnings/Info available

#include <Arduino.h>
#include <avr/pgmspace.h>
#include <Wire.h>
#include "TLC59116_Unmanaged.h"

extern TwoWire Wire;
class TLC59116Manager;

#define WARN TLC59116Warn
#define DEV TLC59116Dev
#define LOWD TLC59116LowLevel

class TLC59116 : public TLC59116_Unmanaged {
  // High level operations,
  // Relieves you from having to track/manage the modes/state of each device.
  // Get one from TLC59116Manager x[]

  public:
// No constructor, get from a TLC59116Manager[i]

virtual TLC59116& enable_outputs(bool yes = true, bool with_delay = true); // delay if doing something immediately
bool is_enable_outputs() { return !is_OSC_bit(shadow_registers[MODE1_Register]); };
bool is_enabled() { return is_enable_outputs(); }

// High level

// Digital
TLC59116& set_outputs(word pattern, word which=0xFFFF); // Only change bits marked in which: to bits in pattern
TLC59116& pattern(word pattern, word which=0xFFFF) { return set_outputs(pattern, which); }
TLC59116& set_pattern(word pattern) { return set_outputs(pattern, pattern); } // only set those indicated
TLC59116& reset_pattern(word pattern) { return set_outputs(~pattern, pattern); } // only turn-off those indicated
TLC59116& set(int led_num, bool offon) {  // only turn-off one
  word bits = 1 << led_num;
  return set_outputs(offon ? bits : ~bits, bits); 
  }
TLC59116& on(int led_num) { return set(led_num, true); } // turn one on
TLC59116& off(int led_num) { return set(led_num, false); } // turn one off

// PWM
TLC59116& set_outputs(byte led_num_start, byte led_num_end, const byte brightness[] /*[start..end]*/); // A list of PWM values. Tolerates led_num_end>15 which wraps around

