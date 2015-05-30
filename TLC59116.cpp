#include <Arduino.h>
#define TLC59116_LOWLEVEL 0
#define TLC59116_DEV 0
#define TLC59116_WARNINGS 1

#include "TLC59116.h"
extern "C" void atexit( void ) { } // so I can have statics in a method, i.e. singleton

// #define WARN Serial.print
#define WARN TLC59116Warn
#define DEV TLC59116Dev
#define LOWD TLC59116LowLevel
#define DEBUG TLC59116Warn

const unsigned char TLC59116::Power_Up_Register_Values[TLC59116_Unmanaged::Control_Register_Max + 1] PROGMEM = {
  TLC59116_Unmanaged::MODE1_OSC_mask | TLC59116_Unmanaged::MODE1_ALLCALL_mask,
  0, // mode2
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // pwm0..15
  0xff, // grppwm
  0, // grpfreq
  0,0,0,0, // ledout0..3
  (TLC59116_Unmanaged::SUBADR1 << 1), (TLC59116_Unmanaged::SUBADR2 << 1), (TLC59116_Unmanaged::SUBADR3 << 1),
  (TLC59116_Unmanaged::AllCall_Addr << 1),
  TLC59116_Unmanaged::IREF_CM_mask | TLC59116_Unmanaged::IREF_HC_mask | ( TLC59116_Unmanaged::IREF_CC_mask & 0xff),
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
  if (this->reset_actions & Already) { 
    WARN(F("Manager already Inited for i2c ")); WARN((unsigned long)&i2cbus, HEX); WARN(); 
    return; 
    }
  this->reset_actions |= Already;
  WARN(F("Init <i2cbus "));WARN((unsigned long)&i2cbus, HEX);WARN(F("> "));WARN(init_frequency);WARN(F("hz actions:"));WARN(reset_actions,BIN);WARN();

  if (reset_actions & WireInit) i2cbus.begin();
  // don't know how to set other WIRE interfaces
  if (&i2cbus == &Wire && init_frequency != 0) 
    {
    TWBR = ((F_CPU / init_frequency) - 16) / 2; // AFTER wire.begin
    }
  else {WARN(F("Don't know how to set i2c frequency for non Wire"));WARN();}

  // NB. desired-frequency and actual, may not match
  WARN(F("I2C bus init'd to ")); WARN(F_CPU / (16 + (2 * TWBR))); WARN(F("hz. F_CPU=")); WARN(F_CPU); WARN();

  scan();
  if (reset_actions & Reset) reset(); // does enable

  WARN(F("Inited manager for I2C "));WARN((unsigned long)&i2cbus, HEX); WARN();
  }

int TLC59116Manager::scan() {
  // this code lifted & adapted from Nick Gammon (written 20th April 2011)
  // http://www.gammon.com.au/forum/?id=10896&reply=6#reply6
  // Thanks Nick!

  // FIXME: no rescan supported, no add/remove supported

  TLC59116Dev(TLC59116::Device); TLC59116Dev(F(" scanner. Scanning ..."));TLC59116Dev();

  byte debug_tried = 0;

  this->device_ct = 0;
  for (byte addr = TLC59116_Unmanaged::Base_Addr; addr <= TLC59116_Unmanaged::Max_Addr; addr++) {
    debug_tried++;
    TLC59116Dev("Try ");TLC59116Dev(addr,HEX);

    // If we hang here, then you did not do a Wire.begin();, or the frequency is too high, or communications is borked

    // yup, just "ping"
    i2cbus.beginTransmission(addr);
    int stat = i2cbus.endTransmission(); // the trick is: 0 means "something there"

    if (stat == 0) {
      if (addr == TLC59116_Unmanaged::AllCall_Addr) {TLC59116Dev(F(" AllCall_Addr, skipped")); TLC59116Dev(); continue; }
      if (addr == TLC59116_Unmanaged::Reset_Addr) {TLC59116Dev(F(" Reset_Addr, skipped")); TLC59116Dev(); continue; }

      this->devices[ this->device_ct++ ] = new TLC59116(i2cbus,addr, *this);
      TLC59116Warn(F(" found "));TLC59116Warn(addr,HEX);
      TLC59116Warn();

      ::delay(10);  // maybe unneeded?
      } // end of good response
    else if (stat != 2) { // "2" means, "not there"
      TLC59116Warn(F(" Unexpected stat(")); TLC59116Warn(stat); TLC59116Warn(F(") at address ")); TLC59116Warn(addr,HEX);
      TLC59116Warn();
      }


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
    TLC59116Dev(F("Reset failed with ")); TLC59116Warn(rez); TLC59116Warn(); 
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
      new_ledx[ledx_i / 4] = LEDx_set_mode(
        new_ledx[ledx_i / 4], 
        ledx_i, 
        (pattern & 0x8000) ? LEDOUT_DigitalOn : LEDOUT_DigitalOff
        );
      }
    pattern <<= 1;
    which <<= 1;

    if (ledx_i==0) break; // can't detect < 0 on an unsigned!
    }

  update_registers(new_ledx, LEDOUT0_Register, LEDOUTx_Register(15));
  return *this;
  }

TLC59116& TLC59116::set_outputs(byte led_num_start, byte led_num_end, const byte brightness[] /*[ct]*/ ) {
  // We are going to start with current shadow values, mark changes against them, 
  //  then write the smallest range of registers.
  // We are going to write LEDOUTx and PWMx registers together to prevent flicker.
  // This becomes less efficient if the only changed PWMx is at a large 'x',
  // And if a LEDOUTx needs to be changed to PWM,
  // And/Or if the changed PWMx is sparse.
  // But, pretty efficient if the number of changes (from first change to last) is more than 1/2 the range.

  byte ct = led_num_end - led_num_start + 1;
  LOWD(F("Set PWMs from "));LOWD(led_num_start,HEX),LOWD(F(" for "));LOWD(ct);
    LOWD(F(": "));for(byte i=0;i<ct;i++) {LOWD(brightness[i],HEX);LOWD(F(" "));}
    LOWD();

  // need current values to mark changes against
  // FIXME: We could try to minimize the register_count: to start with the min(PWMx), but math
  byte register_count = /* r0... */ LEDOUTx_Register(Channels-1) + 1;
  LOWD(F("setup buff of 0.."));LOWD(register_count);LOWD(F("-1"));LOWD();
  byte want[register_count]; // I know that TLC59116 is: veryfew...PWMx..LEDOUTx. so waste a few at head
  memcpy(want, this->shadow_registers, register_count);

  // (for ledoutx, Might be able to do build it with <<bitmask, and then set_with_mask the whole thing)
  LOWD(F("  copying led# "));LOWD(led_num_start);LOWD(F(" to% "));LOWD((led_num_start + ct -1)%16);LOWD();
  for(byte ledi=led_num_start; ledi < led_num_start + ct; ledi++) {
    // ledout settings
    byte wrapped_i = ledi % 16; // %16 wraps
    byte out_r = LEDOUTx_Register(wrapped_i);
    want[out_r] = LEDx_set_mode(want[out_r], wrapped_i, LEDOUT_PWM);
    
    // PWM
    want[ PWMx_Register(wrapped_i) ] = brightness[ledi - led_num_start];
    LOWD(F("  setting LEDOUT @"));LOWD(out_r);LOWD(F("="));LOWD(want[out_r],HEX);
    LOWD(F(", PWM @"));LOWD(wrapped_i);LOWD(F("="));LOWD(want[ PWMx_Register(wrapped_i) ]);
    LOWD();
    }

  update_registers(&want[PWM0_Register], PWM0_Register, LEDOUTx_Register(Channels-1));
  return *this;
  }

TLC59116& TLC59116::group_pwm(word bit_pattern, byte brightness) {
  // Doing them together to minimize freq/mode being out of sync
  byte register_count = LEDOUTx_Register(Channels-1) + 1;
  byte want[register_count]; // 0..MODE2_Register...GRPPWM_Register,GRPFREQ_Register,LEDOUTx...; wasting 1

  // not touching PWM registers
  memcpy( &want[PWM0_Register], &shadow_registers[PWM0_Register],  GRPPWM_Register - PWM0_Register);
  // start with extant LEDOUTx values
  memcpy(&want[LEDOUT0_Register], &shadow_registers[LEDOUT0_Register], LEDOUTx_Register(Channels-1) - LEDOUT0_Register+1);

  want[MODE2_Register] = set_with_mask(shadow_registers[MODE2_Register], MODE2_DMBLNK, 0);
  want[GRPPWM_Register] = brightness;
  want[GRPFREQ_Register] = 0; // not actuall "don't care", it's ~dynamic_range
  LEDx_set_mode( &want[LEDOUT0_Register], LEDOUT_GRPPWM, bit_pattern);

  // do it
  update_registers(&want[MODE2_Register], MODE1_Register, register_count-1);
  return *this;
  }

TLC59116& TLC59116::group_blink(word bit_pattern, int blink_delay, int on_ratio) {
  // Doing them together to minimize freq/mode being out of sync
  byte register_count = LEDOUTx_Register(Channels-1) + 1;
  byte want[register_count]; // 0..MODE2_Register...GRPPWM_Register,GRPFREQ_Register,LEDOUTx...; wasting 1

  // not touching PWM registers
  memcpy( &want[PWM0_Register], &shadow_registers[PWM0_Register],  GRPPWM_Register - PWM0_Register);
  // start with extant LEDOUTx values
  memcpy(&want[LEDOUT0_Register], &shadow_registers[LEDOUT0_Register], LEDOUTx_Register(Channels-1) - LEDOUT0_Register+1);

  // Hypoth: DMBLNK resets the grpfreq timer. Nope.
  // want[MODE2_Register] = set_with_mask(shadow_registers[MODE2_Register], MODE2_DMBLNK, ~MODE2_DMBLNK);
  // want[GRPPWM_Register] = 0xFF;
  // want[GRPFREQ_Register] = 0;
  // LEDx_set_mode( &want[LEDOUT0_Register], LEDOUT_PWM, bit_pattern);
  // update_registers(&want[MODE1_Register], MODE1_Register, 1);

  // Hypoth: OSC resets the grpfreq timer. Nope.
  // want[MODE1_Register] = set_with_mask(shadow_registers[MODE1_Register], MODE1_OSC_mask, MODE1_OSC_mask);
  // update_registers(&want[MODE1_Register], MODE1_Register, 1);
  // delayMicroseconds(501);

  // Only Reset resetsthe grpfreq timer.

  want[MODE2_Register] = set_with_mask(shadow_registers[MODE2_Register], MODE2_DMBLNK, MODE2_DMBLNK);
  want[GRPPWM_Register] = on_ratio;
  want[GRPFREQ_Register] = blink_delay;
  LEDx_set_mode( &want[LEDOUT0_Register], LEDOUT_GRPPWM, bit_pattern);

  // do it
  update_registers(&want[MODE2_Register], MODE2_Register, register_count-1);
  return *this;
  }

void TLC59116::update_registers(const byte want[] /* [start..end] */, byte start_r, byte end_r) {
  // Update only the registers that need it
  // 'want' has to be (shadow_registers & new_value)
  // want[0] is register_value[start_r], i.e. a subset of the full register set
  // Does wire.begin, write,...,end
  // (_i = index of a led 0..15, _r is register_number)
  LOWD(F("Update registers ")); LOWD(start_r,HEX);LOWD(F(".."));LOWD(end_r,HEX);LOWD();

  LOWD(F("Change to ")); LOWD();
  for(byte i=0;i<end_r-start_r+1;i++){if (i>0 && i % 8 ==0) LOWD(); LOWD(want[i],BIN);LOWD(F("/"));LOWD(want[i],HEX); LOWD(" "); } 
  LOWD();

  // now find the changed ones
  //  best case is: no writes
  //  2nd best case is some subrange

  const byte *want_fullset = want - start_r; // now want[i] matches shadow_register[i]

  // First change...
  bool has_change_first = false;
  byte change_first_r; // a register num
  for (change_first_r = start_r; change_first_r <= end_r; change_first_r++) {
    if (want_fullset[change_first_r] != shadow_registers[change_first_r]) {
      has_change_first = true; // found it
      break;
      }
    }

  // Write the data if any changed

  if (!has_change_first) { // might be "none changed"
    LOWD(F("No Diff"));LOWD();
    }
  else {
    // Find last change
    byte change_last_r; // a register num
    for (change_last_r = end_r; change_last_r >= change_first_r; change_last_r--) {
      if (want_fullset[change_last_r] != shadow_registers[change_last_r]) {
        break; // found it
        }
      }
    LOWD(F("Diff "));LOWD(change_first_r,HEX);LOWD(F(".."));LOWD(change_last_r,HEX);
      LOWD(F(" ("));LOWD(change_last_r-change_first_r+1);LOWD(F(")"));
    LOWD();
    for(byte r=change_first_r; r<=change_last_r; r++) {LOWD(shadow_registers[r],HEX);LOWD(" ");} LOWD();
    for(byte r=change_first_r; r<=change_last_r; r++) {LOWD(want_fullset[r],HEX);LOWD(" ");} LOWD();

    // We have a first..last, so write them
    LOWD(F("Write "));LOWD(change_first_r,HEX);LOWD(F("+"));LOWD(change_last_r-change_first_r+1);LOWD(F(": "));
    _begin_trans(Auto_All, change_first_r);
      // TLC59116Warn("  ");
      i2cbus.write(&want_fullset[change_first_r], change_last_r-change_first_r+1);
    _end_trans();
    // update shadow
    LOWD();
    memcpy(&shadow_registers[change_first_r], &want_fullset[change_first_r], change_last_r-change_first_r+1);
    // FIXME: propagate shadow
    LOWD(F("Wrote bulk"));LOWD();
    LOWD(F("S="));for(byte i=0; i <=Control_Register_Max ; i++) { LOWD(shadow_registers[i],HEX);LOWD(F(" ")); } LOWD();
    }
  }

TLC59116& TLC59116::describe_shadow() {
  Serial.print(Device);Serial.print(F(" 0x"));Serial.print(address(),HEX);
  Serial.print(F(" on bus "));Serial.print((unsigned long)&i2cbus, HEX);Serial.println();
  Serial.println(F("Shadow Registers"));
  TLC59116_Unmanaged::describe_registers(shadow_registers); 
  return *this; 
  }

TLC59116& TLC59116::set_address(const byte address[/* sub1,sub2,sub3,all */], byte enable_mask /* MODE1_ALLCALL_mask | MODE1_SUB1_mask... */) {
  // does much:
  // sets the adresses, if != 0
  // enables/disables if corresponding address !=0
  byte want_mode1 = shadow_registers[MODE1_Register];
  byte want_addresses[4];
  memcpy(want_addresses, &shadow_registers[SUBADR1_Register], 4);

  for(byte i=0; i<4; i++) {
    if (address[i] != 0) {
      // actual address
      if (address[i]==Reset_Addr) {
        WARN(F("Ignored attempt to use the Reset_Addr (0x60) for "));
        if (i==3) WARN(F("AllCall"));
        else {
          WARN(F("SUBADR"));WARN(i+1);
          }
        WARN();
        continue;
        }

      want_addresses[i] = address[i];
      if (i==3) {
        // enable'ment
        set_with_mask(&want_mode1, MODE1_ALLCALL_mask, enable_mask);
        }
      else {
        // enable'ment
        set_with_mask(&want_mode1, MODE1_SUB1_mask >> (i), enable_mask);
        }
      }
    }
  update_registers( &want_mode1, MODE1_Register, MODE1_Register);
  update_registers( want_addresses, SUBADR1_Register, AllCall_Addr_Register);
  return *this;
  }


TLC59116& TLC59116::SUBADR_address(byte which, byte address, bool enable) {
  byte want_address[4] = {0,0,0,0};
  if (which==0 || which > 3) { WARN(F("Expected 1..3 for 'which' in SUBADR_address(), saw "));WARN(which);WARN(); return *this;}
  want_address[which-1] = address << 1;
  return set_address(want_address, enable ? MODE1_SUB1_mask >> (which-1) : 0);
  }

TLC59116& TLC59116::allcall_address(byte address, bool enable) {
  byte want_address[4] = {0,0,0,address << 1};
  return set_address(want_address, enable ? MODE1_ALLCALL_mask : 0);
  }

TLC59116& TLC59116::allcall_address_enable() {
  byte want_mode1 = shadow_registers[MODE1_Register];
  set_with_mask(&want_mode1, MODE1_ALLCALL_mask, MODE1_ALLCALL_mask);
  update_registers( &want_mode1, MODE1_Register, MODE1_Register);
  }

TLC59116& TLC59116::allcall_address_disable() {
  byte want_mode1 = shadow_registers[MODE1_Register];
  set_with_mask(&want_mode1, MODE1_ALLCALL_mask, 0);
  update_registers( &want_mode1, MODE1_Register, MODE1_Register);
  }

TLC59116& TLC59116::resync_shadow_registers() {
  byte rez = fetch_registers(0, Control_Register_Max+1, shadow_registers);
  return *this;
  }

TLC59116& TLC59116::set_milliamps(byte ma, int Rext) {
  byte iref_value = best_iref(ma, Rext);
  modify_control_register(IREF_Register,iref_value);
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


