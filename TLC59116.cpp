#include <Arduino.h>
#define TLC59116_DEV 1
#define TLC59116_WARNINGS 1

#include "TLC59116.h"
extern "C" void atexit( void ) { } // so I can have statics in a method, i.e. singleton

// #define WARN Serial.print
#define WARN TLC59116Warn
#define DEV TLC59116Dev
#define LOWD TLC59116LowLevel

const prog_uchar TLC59116::Power_Up_Register_Values[TLC59116_Unmanaged::Control_Register_Max] PROGMEM = {
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

void TLC59116::reset_happened() {
  this->reset_shadow_registers();
  }

//
// TLC59116Manager
//

void TLC59116Manager::init() {
  // Only once. We are mean.
  if (this->reset_actions & Already) { WARN(F("Manager already Inited for i2c ")); WARN((unsigned long)&i2cbus, HEX); WARN(); return; }
  this->reset_actions |= Already;
  WARN(F("Init i2cbus "));WARN((unsigned long)&i2cbus, HEX);WARN(F(" "));WARN(init_frequency);WARN(F("hz "));WARN(reset_actions,BIN);WARN();

  if (reset_actions & WireInit) i2cbus.begin();
  // don't know how to set other WIRE interfaces
  if (&i2cbus == &Wire && init_frequency != 0) TWBR = ((F_CPU / init_frequency) - 16) / 2; // AFTER wire.begin
  else {WARN(F("Don't know how to set i2c frequency for non Wire"));WARN();}

  // NB. desired-frequency and actual, may not match
  WARN(F("I2C bus init'd to ")); WARN(F_CPU / (16 + (2 * TWBR))); WARN(F("hz")); WARN();

  scan();
  if (reset_actions & Reset) reset(); // does enable

  WARN(F("Inited manager for "));WARN((unsigned long)&i2cbus, HEX); WARN();
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
    i2cbus.beginTransmission(addr);
    int stat = i2cbus.endTransmission(); // the trick is: 0 means "something there"

    if (stat == 0) {
      if (addr == TLC59116_Unmanaged::AllCall_Addr) {TLC59116Dev(F(" AllCall_Addr, skipped")); TLC59116Dev(); continue; }
      if (addr == TLC59116_Unmanaged::Reset_Addr) {TLC59116Dev(F(" AllCall_Addr, skipped")); TLC59116Dev(); continue; }

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
      for(byte i=0; i<device_ct; i++) { WARN(" ");WARN((*this)[i].address(),HEX); }
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

  broadcast().reset_happened();
  for (byte i=0; i< device_ct; i++) { devices[i]->reset_happened(); }
  WARN(F("Reset signalled to all"));WARN();
  if (this->reset_actions & EnableOutputs) broadcast().enable_outputs();
  return 0;
  }

TLC59116& TLC59116::enable_outputs(bool yes, bool with_delay) {
  if (yes) {
    WARN(F("Enable outputs for "));WARN(address(),HEX);WARN();
    modify_control_register(MODE1_Register,MODE1_OSC_mask, 0x00); // bits off is osc on
    if (with_delay) delayMicroseconds(500); // would be nice to be smarter about when to do this
    }
  else {
    WARN(F("Disable outputs for "));WARN(address(),HEX);WARN();
    modify_control_register(MODE1_Register,MODE1_OSC_mask, MODE1_OSC_mask); // bits on is osc off
    }

  LOWD(F("Finished en/dis-able outputs"));LOWD();
  return *this;
  }

void TLC59116::modify_control_register(byte register_num, byte value) {
  if (shadow_registers[register_num] != value) {
      shadow_registers[register_num] = value;
      LOWD(F("Modify "));LOWD(register_num,HEX);LOWD(F("=>"));LOWD(value,HEX);LOWD();
      control_register(register_num, value);
      }
  }

void TLC59116::modify_control_register(byte register_num, byte mask, byte bits) {
  byte new_value = set_with_mask(shadow_registers[register_num], mask, bits);
  if (register_num < PWM0_Register || register_num > 0x17) {
    LOWD(F("Modify R"));LOWD(register_num,HEX);LOWD();
    LOWD(F("       ="));LOWD(shadow_registers[register_num],BIN);LOWD();
    LOWD(F("       M"));LOWD(mask,BIN);LOWD();
    LOWD(F("       V"));LOWD(bits,BIN);LOWD();
    LOWD(F("       ="));LOWD(new_value,BIN);LOWD();
    }
  modify_control_register(register_num, new_value);
  }

TLC59116& TLC59116::set_outputs(word pattern, word which) {
  // Only change bits marked in which: to bits in pattern

  // We'll make the desired ledoutx register set
  byte new_ledx[4];
  // need initial value for later comparison
  memcpy(new_ledx, &(this->shadow_registers[LEDOUT0_Register]), 4);

  // count through LED nums, starting from max (backwards is easier)

  for(byte ledx_i=15; ; ledx_i--) {
    if (0x8000 & which) {
      new_ledx[ledx_i / 4] = LEDX_set_mode(
        new_ledx[ledx_i / 4], 
        ledx_i, 
        (pattern & 0x8000) ? LEDOUT_DigitalOn : LEDOUT_DigitalOff
        );
      }
    pattern <<= 1;
    which <<= 1;

    if (ledx_i==0) break; // can't detect < 0 on an unsigned!
    }

  update_ledx_registers(new_ledx, 0, 15);
  return *this;
  }

void TLC59116::update_ledx_registers(const byte* new_ledx /* [4] */, byte led_start_i, byte led_end_i) {
  // Update only the LEDx registers that need it
  // new_ledx is shadow_registers & new settings
  // Does wire.begin, write,...,end
  // (_i = index of a led 0..15, _r is register_number)
  const byte ledx_first_r = LEDOUTx_Register(led_start_i);
  const byte ledx_last_r = LEDOUTx_Register(led_end_i);
  //TLC59116Warn(F("Update control from #"));
    //TLC59116Warn(led_start_i);TLC59116Warn(F("/C"));TLC59116Warn(ledx_first_r,HEX);
    //TLC59116Warn(F(" to "));TLC59116Warn(led_end_i);TLC59116Warn(F("/C"));TLC59116Warn(ledx_last_r,HEX);
    //TLC59116Warn();

  //TLC59116Warn(F("Change to ")); for(byte i=0;i<4;i++){ TLC59116Warn(new_ledx[i],BIN); TLC59116Warn(" ");} TLC59116Warn();

  // "new" is updated, now find the changed ones
  // (all the following work to write only changed bytes instead of 4. 
  //  best case is: no writes
  //  2nd best case is: begin, device addr, register, 1 byte, end
  //  instead of  : begin, device addr, register, byte byte byte byte, end
  //  which is a _possible_ savings of 3-bytes/24-bits out of 56-bits = 42%
  //  so, pretty good
  // )
  byte change_first_r; // an LEDOUTx register num
  bool has_change_first = false;

  for (byte r = ledx_first_r; r <= ledx_last_r; r++) {
    if (new_ledx[r - LEDOUT0_Register] != shadow_registers[r]) {
      change_first_r = r;
      has_change_first = true;
      break; // done if we found one
      }
    }
  // if (!has_change_first) {TLC59116Warn(F("No Diff"));TLC59116Warn();}

  // Write the data if any changed

  if (has_change_first) { // might be "none changed"
    //// DEBUG
    //TLC59116Warn("Diff F... ");
    //for(byte r = LEDOUT0_Register; r < change_first_r; r++) TLC59116Warn("           ");
    //for(byte r = change_first_r; r < 4 + LEDOUT0_Register; r++) { TLC59116Warn(new_ledx[r - LEDOUT0_Register],BIN);TLC59116Warn(" "); }
    //TLC59116Warn();
    //// DEBUG
  

    byte change_last_r;

    // Find last
    for(byte r = ledx_last_r; ; r--) { // we know we'll hit change_first_r
      if (new_ledx[r - LEDOUT0_Register] != shadow_registers[r]) {
        change_last_r = r;
        break; // done if we found one. it might be change_first_r
        }
      }
    //// DEBUG
    //TLC59116Warn("Diff      ");
    //for(byte r = LEDOUT0_Register; r < change_first_r; r++) TLC59116Warn("           ");
    //for(byte r = change_first_r; r <= change_last_r; r++) { TLC59116Warn(new_ledx[r - LEDOUT0_Register],BIN);TLC59116Warn(" "); }
    //TLC59116Warn();
    //// DEBUG

    // We have a first..last, so write them
    // TLC59116Warn(F("Write @ register ")); TLC59116Warn(change_first_r,HEX); TLC59116Warn(" "); TLC59116Warn(change_last_r - change_first_r +1); TLC59116Warn(F(" bytes of data")); TLC59116Warn();
    _begin_trans(Auto_All, change_first_r);
      // TLC59116Warn("  ");
      i2cbus.write(&new_ledx[ change_first_r - LEDOUT0_Register ], change_last_r-change_first_r+1);
      for(; change_first_r <= change_last_r; change_first_r++) {
        byte new_val = new_ledx[ change_first_r - LEDOUT0_Register ];
        shadow_registers[ change_first_r ] = new_val;
        // TLC59116Warn(new_val,BIN);TLC59116Warn(" ");
        }
      //TLC59116Warn();
    _end_trans();
    }
  }

TLC59116& TLC59116::describe_shadow() {
  Serial.print(Device);Serial.print(F(" 0x"));Serial.print(address(),HEX);
  Serial.print(F(" on bus "));Serial.print((unsigned long)&i2cbus, HEX);Serial.println();
  Serial.println(F("Shadow Registers"));
  TLC59116_Unmanaged::describe_registers(shadow_registers); 
  return *this; 
  }

//
// Broadcast
//

TLC59116::Broadcast& TLC59116::Broadcast::enable_outputs(bool yes, bool with_delay) {
  WARN(yes ? F("Enable") : F("Disable")); WARN(F(" outputs for ALL")); WARN();
  TLC59116::enable_outputs(yes, with_delay);
  propagate_register(MODE1_Register);
  return *this;
  }

void TLC59116::Broadcast::propagate_register(byte register_num) {
  byte my_value = shadow_registers[register_num];
  for (byte i=0; i<= manager.device_ct; i++) { manager.devices[i]->shadow_registers[register_num]=my_value; }
  }


