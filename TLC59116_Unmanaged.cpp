// FIXME: make things final

#include "TLC59116_Unmanaged.h"
#include <Wire.h>

// The convenience method sometlc.delay(x) hides the normal delay. Use ::delay(...)

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
    // TLC59116Warn(F("set R "));TLC59116Warn(register_num,HEX);TLC59116Warn("=");TLC59116Warn(data,BIN);TLC59116Warn();
    _begin_trans(this->address(), register_num); 
      Wire.write(data);
    _end_trans();
  }
