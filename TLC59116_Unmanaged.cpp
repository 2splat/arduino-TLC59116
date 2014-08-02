// FIXME: make things final

#define TLC59116_DEV 1
#define TLC59116_WARNINGS 1

#include "TLC59116_Unmanaged.h"
#include <Wire.h>

// The convenience method sometlc.delay(x) hides the normal delay. Use ::delay(...)

#define WARN TLC59116Warn
#define DEV TLC59116Dev

const char* TLC59116_Unmanaged::Device = "TLC59116";

inline void printf0(byte v) { // 3 digits
    Serial.print(v / 100);
    v = v % 100;
    Serial.print(v/10);
    v = v % 10;
    Serial.print(v);
    }

void TLC59116_Unmanaged::control_register(byte register_num, byte data) {
  // set
  if (!is_control_register(register_num)) {
    TLC59116Warn(F("Not a "));TLC59116Warn(Device);TLC59116Warn(F(" control register #:"));TLC59116Warn(register_num,HEX);
    TLC59116Warn(F(". Set to "));TLC59116Warn(data,HEX);TLC59116Warn(F("is ignored."));
    return;
    }
  TLC59116Dev(F("Set "));TLC59116Dev(register_num,HEX);TLC59116Dev(F("=>"));TLC59116Dev(data,HEX);TLC59116Dev();
  _begin_trans(register_num); 
    DEV(F(" "));DEV(data,BIN);DEV();
    i2cbus.write(data);
  _end_trans();
  }
