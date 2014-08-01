#include <Arduino.h>
#include "TLC59116.h"
extern "C" void atexit( void ) { } // so I can have statics in a method, i.e. singleton

static const prog_uchar Power_Up_Register_Values[TLC59116_Unmanaged::Control_Register_Max] PROGMEM = {
  TLC59116_Unmanaged::MODE1_OSC_mask | TLC59116_Unmanaged::MODE1_ALLCALL_mask,
  0, // mode2
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // pwm0..15
  0xff, // grppwm
  0, // grpfreq
  0,0,0,0, // ledout0..3
  (TLC59116_Unmanaged::SUBADR1 << 1), (TLC59116_Unmanaged::SUBADR2 << 1), (TLC59116_Unmanaged::SUBADR2 << 1),
  TLC59116_Unmanaged::AllCall_Addr,
  TLC59116_Unmanaged::IREF_CM_mask | TLC59116_Unmanaged::IREF_HC_mask | ( TLC59116_Unmanaged::IREF_CC_mask && 0xff),
  0,0 // eflag1..eflag2
  };

void TLC59116::reset_shadow_registers() {
  memcpy_P(shadow_registers,
    (byte*) Power_Up_Register_Values,
    Control_Register_Max
    );
  }

void TLC59116::reset_happened() {
  this->reset_shadow_registers();
  }

//
// TLC59116Manager
//

void TLC59116Manager::init(long frequency, byte dothings) {
  this->reset_actions = dothings;
  if (dothings & WireInit) i2cbus.begin();
  // don't know how to set other WIRE interfaces
  if (&i2cbus == &Wire) TWBR = ((F_CPU / frequency) - 16) / 2; // AFTER wire.begin
  else {TLC59116Warn(F("Don't know how to set i2c frequency for non Wire"));TLC59116Warn();}
  scan();
  if (dothings & Reset) reset();
  // if (dothings & EnableOutputs) broadcast().enable_outputs();
  }

int TLC59116Manager::scan() {
  // this code lifted & adapted from Nick Gammon (written 20th April 2011)
  // http://www.gammon.com.au/forum/?id=10896&reply=6#reply6
  // Thanks Nick!

  // FIXME: no rescan supported, no add/remove supported

  TLC59116Dev(TLC59116::Device); TLC59116Dev(F(" scanner. Scanning ..."));TLC59116Dev();

  byte debug_tried = 0;

  for (byte addr = TLC59116_Unmanaged::Base_Addr; addr <= TLC59116_Unmanaged::Max_Addr; addr++) {
    debug_tried++;
    TLC59116Dev("Try ");TLC59116Dev(addr,HEX);

    // If we hang here, then you did not do a Wire.begin();, or the frequency is too high, or communications is borked

    // yup, just "ping"
    Wire.beginTransmission(addr);
    int stat = Wire.endTransmission(); // the trick is: 0 means "something there"

    if (stat == 0) {
      if (addr == TLC59116_Unmanaged::AllCall_Addr) {TLC59116Dev(F(" AllCall_Addr, skipped")); TLC59116Dev(); continue; }

      this->devices[ this->device_ct++ ] = new TLC59116(i2cbus,addr, *this);
      TLC59116Warn(F(" found "));TLC59116Warn(addr,HEX);

      ::delay(10);  // maybe unneeded?
      } // end of good response
    else if (stat != 2) { // "2" means, "not there"
      TLC59116Warn(F(" Unexpected stat(")); TLC59116Warn(stat); TLC59116Warn(F(") at address ")); TLC59116Warn(addr,HEX);
      }

    TLC59116Warn();

  } // end of for loop
  TLC59116Warn(F("Checked "));TLC59116Warn(debug_tried);
  TLC59116Warn(F(" out of "));TLC59116Warn(TLC59116_Unmanaged::Base_Addr,HEX); TLC59116Warn(".."); TLC59116Warn(TLC59116_Unmanaged::Max_Addr,HEX);
  TLC59116Warn();
  
  if (this->device_ct) {
      TLC59116Warn(F("Found "));
      TLC59116Warn(this->device_ct);
      TLC59116Warn(" ");TLC59116Warn(TLC59116_Unmanaged::Device);TLC59116Warn(F("'s that responded."));
      TLC59116Warn();
    }
  else {
    TLC59116Warn(F("None found!"));
    }

  return this->device_ct;
  }

int TLC59116Manager::reset() {
  i2cbus.beginTransmission(TLC59116_Unmanaged::Reset_Addr);
    // You might think you could:
    // i2cbus.write((const byte*)&TLC59116_Unmanaged::Reset_Bytes, size_t(TLC59116_Unmanaged::Reset_Bytes));
    i2cbus.write( TLC59116_Unmanaged::Reset_Bytes >> 8 ); 
    i2cbus.write( TLC59116_Unmanaged::Reset_Bytes & 0xFF); 
  int rez = TLC59116_Unmanaged::_end_trans(i2cbus);

  if (rez)  { 
    TLC59116Warn(F("Reset failed with ")); TLC59116Warn(rez); TLC59116Warn(); 
    return rez;
    }

  TLC59116Warn(F("Reset worked!")); TLC59116Warn();

  for (byte i=0; i<= device_ct; i++) { devices[i]->reset_happened(); }
  if (this->reset_actions & EnableOutputs) broadcast().enable_outputs();
  return 0;
  }

TLC59116& TLC59116::enable_outputs(bool yes, bool with_delay) {
  if (yes) {
    modify_control_register(MODE1_Register,MODE1_OSC_mask, 0x00); // bits off is osc on
    if (with_delay) delayMicroseconds(500); // would be nice to be smarter about when to do this
    }
  else {
    modify_control_register(MODE1_Register,MODE1_OSC_mask, MODE1_OSC_mask); // bits on is osc off
    }

  return *this;
  }

void TLC59116::modify_control_register(byte register_num, byte value) {
  if (shadow_registers[register_num] != value) {
      shadow_registers[register_num] = value;
      // TLC59116Dev("CR...");TLC59116Dev();
      control_register(register_num, value);
      }
      
  }

void TLC59116::modify_control_register(byte register_num, byte mask, byte bits) {
  byte new_value = set_with_mask(shadow_registers[register_num], mask, bits);
  /* if (register_num < PWM0_Register || register_num > 0x17) {
    TLC59116Warn(F("Modify R"));TLC59116Warn(register_num,HEX);TLC59116Warn();
    TLC59116Warn(F("       ="));TLC59116Warn(shadow_registers[register_num],BIN);TLC59116Warn();
    TLC59116Warn(F("       M"));TLC59116Warn(mask,BIN);TLC59116Warn();
    TLC59116Warn(F("       V"));TLC59116Warn(bits,BIN);TLC59116Warn();
    TLC59116Warn(F("       ="));TLC59116Warn(new_value,BIN);TLC59116Warn();
    } */
  modify_control_register(register_num, new_value);
  }

TLC59116::Broadcast& TLC59116::Broadcast::enable_outputs(bool yes, bool with_delay) {
  TLC59116::enable_outputs(yes, with_delay);
  propagate_register(MODE1_Register);
  return *this;
  }

void TLC59116::Broadcast::propagate_register(byte register_num) {
  byte my_value = shadow_registers[register_num];
  for (byte i=0; i<= manager.device_ct; i++) { manager.devices[i]->shadow_registers[register_num]=my_value; }
  }


