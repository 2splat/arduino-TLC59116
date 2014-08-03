// FIXME: make things final

#define TLC59116_DEV 1
#define TLC59116_WARNINGS 1

#include "TLC59116_Unmanaged.h"
#include <Wire.h>

// The convenience method sometlc.delay(x) hides the normal delay. Use ::delay(...)

#define WARN TLC59116Warn
#define DEV TLC59116Dev
#define LOWD TLC59116LowLevel

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
  LOWD(F("Set "));LOWD(register_num,HEX);LOWD(F("=>"));LOWD(data,HEX);LOWD();
  _begin_trans(register_num); 
    LOWD(F(" "));LOWD(data,BIN);LOWD();
    i2cbus.write(data);
  _end_trans();
  }

TLC59116_Unmanaged& TLC59116_Unmanaged::describe_actual() {
  byte actual[Control_Register_Max + 1]; 
  
  _begin_trans(address(), Auto_All | 0x0);
  if (_end_trans() == 0) {

    int avail = i2cbus.requestFrom(address(), Control_Register_Max);
    if (avail != Control_Register_Max) { 
      WARN(F("Expected to read "));WARN(Control_Register_Max);
      WARN(F(" bytes for all registers, but got "));WARN(avail);WARN();
      }
    if (avail > Control_Register_Max) avail = Control_Register_Max;

    for(byte i=0; i<avail; i++) actual[i] = i2cbus.read();

    Serial.print(Device);Serial.print(F(" 0x"));Serial.print(address(),HEX);
    Serial.print(F(" on bus "));Serial.print((unsigned long)&i2cbus, HEX);Serial.println();
    Serial.println(F("Actual Registers"));

    TLC59116_Unmanaged::describe_registers(actual);
    }

  else { WARN(F("Read all failed"));WARN(); }

  return *this;
  }

void TLC59116_Unmanaged::describe_registers(byte* registers /*[4]*/) {
  describe_iref(registers);
  // mode1 is folded into describe_channels()
  describe_addresses(registers);
  }

void TLC59116_Unmanaged::describe_iref(byte* registers) {
    const byte value = registers[IREF_Register];

    // FIXME: do the math, and explain
    Serial.print(F("IREF: "));
    Serial.print(F("CM=0x"));Serial.print(value & IREF_CC_mask,HEX);
    Serial.print(F(" HC="));Serial.print( (value & IREF_HC_mask) ? "1" : "0");
    Serial.print( (value & IREF_CM_mask) ? " Hi" : " Lo");
    Serial.println();
    }

void TLC59116_Unmanaged::describe_addresses(byte* registers) {
  byte mode1 = registers[MODE1_Register];
  byte the_addr;

  // Internal addresses are 7:1, with bit 0=r/w

  the_addr = registers[AllCall_Addr_Register];
  Serial.print(mode1 & MODE1_ALLCALL_mask ? "+" : "-");
  Serial.print(F("AllCall(0x"));Serial.print(the_addr >> 1,HEX);Serial.print(") ");

  for(byte i = 0; i<3; i++) {
      the_addr = registers[SUBADR1_Register + i];
      // mode bit is 2,4,8 for enable
      Serial.print(mode1 & (MODE1_SUB1_mask >> i) ? "+" : "-");
      Serial.print(F("SUBADR")); Serial.print(i+1); Serial.print("(0x");
      Serial.print(the_addr >> 1, HEX); Serial.print(") ");
      }
  Serial.println();
  }


