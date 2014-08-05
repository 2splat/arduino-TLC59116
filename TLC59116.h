#ifndef TLC59116_h
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
  // Get one from TLC59116_Manager x[]

  friend class TLC59116Manager;

  public:
    // No constructor, get from a TLC59116Manager[i]

    virtual TLC59116& enable_outputs(bool yes = true, bool with_delay = true);

    // High level
    // Digital
    TLC59116& set_outputs(word pattern, word which=0xFFFF); // Only change bits marked in which: to bits in pattern
    TLC59116& pattern(word pattern, word which=0xFFFF) { return set_outputs(pattern, which); }
    TLC59116& on(word pattern) { return set_outputs(pattern, pattern); } // only set those indicated
    TLC59116& off(word pattern) { return set_outputs(~pattern, pattern); } // only turn-off those indicated
    TLC59116& set(int led_num, int offon) {  // only turn-off those indicated
      word bits = 1 << led_num;
      return set_outputs(offon ? bits : ~bits,bits); 
      }

    // PWM
    TLC59116& set_outputs(byte led_num_start, byte ct, const byte brightness[] /*[ct]*/); // A list of PWM values starting at start_i. Tolerates wrapping past i=15
    TLC59116& pwm(byte led_num, byte brightness) { byte ba[1] = {brightness}; return set_outputs(led_num, 1, ba); }
    

    TLC59116& describe_shadow(); 

    TLC59116Manager& manager; // our manager


  private:
    static const prog_uchar Power_Up_Register_Values[TLC59116_Unmanaged::Control_Register_Max];

    // Manager has to track for reset, so factory
    TLC59116(TwoWire& bus, byte address, TLC59116Manager& m) : manager(m), TLC59116_Unmanaged(bus, address) {reset_shadow_registers();}  // factory control, must get from manager
    TLC59116(); // none
    TLC59116(const TLC59116&); // none
    TLC59116& operator=(const TLC59116&); // none
    ~TLC59116() {} // managed destructor....

    
    // low-level
    void reset_happened(); // reset affects the state of the devices
    void modify_control_register(byte register_num, byte mask, byte bits);
    void modify_control_register(byte register_num, byte value);

    // mid-level
    void update_registers(const byte want[] /* [start..end] */, byte start_r, byte end_r); // want[0]=register[start_r]


    // We have to shadow the state
    byte shadow_registers[Control_Register_Max+1];
    void reset_shadow_registers() {
      DEV(F("Reset shadow "));DEV(address(),HEX);DEV();
      memcpy_P(shadow_registers, Power_Up_Register_Values, Control_Register_Max);
      }
    void sync_shadow_registers() { /* fixme: read device, for ... shadow=; */ }

  public: // Special classes
    class Broadcast;
  };

class TLC59116::Broadcast : public TLC59116 {
  public: //
    Broadcast(TwoWire &bus, TLC59116Manager& m) : TLC59116(bus, TLC59116::AllCall_Addr, m) {}

    Broadcast& enable_outputs(bool yes = true, bool with_delay = true);

  private:
    Broadcast(); // none
    Broadcast(const Broadcast&); // none
    Broadcast& operator=(const TLC59116&); // none
    friend class TLC59116Manager;
    ~Broadcast() {} // managed destructor....

    void propagate_register(byte register_num);

  };

class TLC59116Manager {
  // Manager
  // Holds all-devices (on one bus) because reset needs to affect them
  // Holds which "wire" interface

  // FIXME: this skips the device at AllCall_Addr, allow a remediation of that
  // FIXME: likewise, subadr is disabled at reset, allow remediation to exclude that on scan

  friend class TLC59116;

  public:
    static const byte MaxDevicesPerI2C = 15; // 16 addresses - reset (subadr1..subadr3 and allcall can be disabled)
  
    // manager init flags. Default setting should be 1
    static const byte WireInit = 0b1;
    static const byte EnableOutputs = 0b10;
    static const byte Reset = 0b100;
    static const byte Already = 0b1000; // internal

    static const long Default_Frequency = 100000L; // default to 100khz, it's the default for Wire

    // specific to one I2C bus, because reset affects only 1 bus
    TLC59116Manager(TwoWire &w, // Use the specified i2c bus (shouldn't allow 2 of these on same bus)
        long frequency = Default_Frequency, // Use 0 to leave frequency (TWBR) alone
        byte dothings = WireInit | EnableOutputs | Reset // do things by default. disable: 0xFF ^ (X::EnableOutputs | ...)
        ) : i2cbus(w), init_frequency(frequency) { 
      this->reset_actions = WireInit | EnableOutputs | Reset;
      }
    // convenience I2C bus using Wire, the standard I2C arduino pins
    TLC59116Manager() : i2cbus(Wire), init_frequency(Default_Frequency) { 
      this->reset_actions = WireInit | EnableOutputs | Reset; 
      }
    // You'll have to write an adaptor for other I2C interfaces

    // You have to call this (usually in setup)
    void init();
    bool is_inited() { return !!(this->reset_actions & Already); }

    // Get the 0th, 1st, 2nd, etc. device (index, not by i2c-address). In address order.
    // Can return null if index is out of range, or there are none!
    // Pattern: TLC59116& first = manager[0]; // Note the '&'
    // Pattern: manager[i].all_on(); // you can use "." notation
    // FIXME: can we do: BaseClass& x = manager[0];
    TLC59116& operator[](byte index) { 
      if (index >= device_ct) { 
        TLC59116Warn(F("Index "));TLC59116Warn(index);TLC59116Warn(F(" >= max device "));TLC59116Warn(device_ct);TLC59116Warn();
        return *(devices[0]); // Can't return null
        } 
      else { 
        return *(devices[index]);
        } 
      }
    byte device_count() { return device_ct; } // you can iterate

  public: // global things
    int reset(); // 0 is success, does enable_outputs if init(...EnableOutputs...)
    TLC59116::Broadcast& broadcast() { static TLC59116::Broadcast x(Wire, *this); return x; }

    // consider: is_SUBADR(). has to track all devices subaddr settings (value & enabled)

  private:
    TLC59116Manager(const TLC59116Manager&); // undefined
    TLC59116Manager& operator=(const TLC59116Manager&); // undefined
    int scan();

    // Specific bus
    TwoWire &i2cbus;
    byte reset_actions;
    long init_frequency;

    // Need to track extant
    TLC59116* devices[MaxDevicesPerI2C]; // that's 420 bytes of ram
    byte device_ct;

    static const prog_uchar Power_Up_Register_Values[TLC59116::Control_Register_Max+1] PROGMEM;

  };

#endif
