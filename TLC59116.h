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
    TLC59116& set_outputs(byte led_num_start, byte led_num_end, const byte brightness[] /*[start..end]*/); // A list of PWM values starting at start_i. Tolerates led_num_end>15 which wraps around
    TLC59116& pwm(byte led_num, byte brightness) { byte ba[1] = {brightness}; return set_outputs(led_num, led_num, ba); }
    TLC59116& pwm(byte led_num_start, byte led_num_end, const byte brightness[] /*[ct]*/) { return set_outputs(led_num_start, led_num_end, brightness); }
    TLC59116& pwm(byte led_num_start, byte led_num_end, byte pwm_value) { // set all to same value
      const byte register_count = led_num_end - led_num_start +1;
      // FIXME: warnings of range
      byte pwm[register_count];
      memset(pwm, pwm_value, register_count);
      return set_outputs(led_num_start, led_num_end, pwm); 
      }
    TLC59116& pwm(const byte brightness[16]) { return set_outputs(0,15, brightness); }
    // fixme: maybe brightness()?
    
    // "group" functions have a bug:
    // If you decrease the value (blink_time or brightness),
    // The chip may act like the value is max for one "cycle".
    // This causes a full-brightness result for group_pwm, and a 10 second blink-length for group_blink.
    // I think this is because:
    // * There is a continously running timer, counting up.
    // * The "value" is compared to the timer, and triggers the action and a reset of the timer.
    // * If you move the "value" down, you may skip over the current timer value,
    //    so it runs to the maximum value before wrapping around again.
    // "reset" is the only thing that I know of that will reset the timer.
    // But, more experimentation is needed.

    // Superpose group-pwm on current pwm setting (but, current should be 255 for best results)
    // FIXME: has a hardware bug
    TLC59116& group_pwm(word bit_pattern, byte brightness);

    // Blink the LEDs, at their current/last PWM setting. 
    // FIXME: has a hardware bug
    // Set PWM values first.
    // To turn off blink, set channels to PWM or digital-on/off
    // All:
    TLC59116& group_blink(byte blink_delay, byte on_ratio=128) { return group_blink(0xFFFF,blink_delay,on_ratio); }
    TLC59116& group_blink(double blink_length_secs, double on_percent=50.0) {
      return group_blink(0xFFFF,blink_length_secs,on_percent); 
      }
    // By pattern:
    TLC59116& group_blink(word bit_pattern, double blink_length_secs, double on_percent=50.0) { // convenience % & len
      // on_percent is 05..99.61%, blink_length is 0.041secs (24.00384hz) to 10.54secs (.09375hz) in steps of .04166secs 
      return group_blink(
        bit_pattern, 
        (byte) int(blink_length_secs/0.041666667 + .0001), 
        (byte) int(on_percent * 256.0/100.0)
        );
      }
    TLC59116& group_blink(unsigned int bit_pattern, int blink_length_secs, double on_percent=50.0) {
      return group_blink((word)bit_pattern, blink_length_secs, int(on_percent * 256.0/100.0));
      }
    TLC59116& group_blink(word bit_pattern, int blink_delay, int on_ratio=128); // blinks/sec = (blink_delay+1)/24

    // Address stuff
    // it is illegal to set any address to Reset_Addr (0x6B), will be ignored, with warning
    // Addresses should be specified as 0x60..0x6F

    // Can't change Reset
    byte Reset_address() { return shadow_registers[Reset_Addr] >> 1; } // convenience

    // allcall is enabled at reset
    TLC59116& allcall_address(byte address, bool enable=true);
    TLC59116& allcall_address(bool enable); // enable/disable
    byte allcall_address() { return shadow_registers[AllCall_Addr_Register] >> 1; }
    bool is_allcall_address() { return shadow_registers[AllCall_Addr_Register] & MODE1_ALLCALL_mask; }

    // SUBADR's are disabled at reset
    // SUBADR are 1,2,3 for "which"
    TLC59116& SUBADR_address(byte which, byte address, bool enable=true);
    TLC59116& SUBADR_address(byte which, bool enable); // enable/disable
    byte SUBADR_address(byte which) { return shadow_registers[SUBADRx_Register(which)] >> 1; }
    bool is_SUBADR_address(byte which) { return is_SUBADR_bit( shadow_registers[MODE1_Register], which); }
    
    // Software current control
    // We need a Rext value to calculate this, 156ohms gives 120ma at reset, 937ohms give 20ma
    // Note that this will tend to set the output 1milliamp low.
    // This will also clip the result to the minimum (for Rext) and 120ma.
    // Check it with milliamps().
    TLC59116& set_milliamps(byte ma, int Rext=Rext_Min); // Rext_Min ohms is 120ma at reset.
    int milliamps(int Rext=Rext_Min) {return i_out(shadow_registers[IREF_Register]); } // current setting

    // Error detect
    // FIXME: implement. we should set error enable on reset? then read them?

  public: // Debugging
    TLC59116& describe_shadow();
    TLC59116& resync_shadow_registers();

  private:
    // FIXME: move Power_Up_Register_Values to FLASH
    static const prog_uchar Power_Up_Register_Values[TLC59116_Unmanaged::Control_Register_Max + 1];
    TLC59116Manager &manager;
    // void (*on_reset)(byte /* manager[i] */); 

    // Manager has to track for reset, so factory
    TLC59116(TwoWire& bus, byte address, TLC59116Manager& m) : TLC59116_Unmanaged(bus, address), manager(m) {reset_shadow_registers();}  // factory control, must get from manager
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
    TLC59116& set_address(const byte address[/* sub1,sub2,sub3,all */], byte enable_mask /* MODE1_ALLCALL_mask | MODE1_SUB1_mask... */); // set all at once w/enable'ment



    // We have to shadow the state
    byte shadow_registers[Control_Register_Max+1];
    void reset_shadow_registers() {
      DEV(F("Reset shadow "));DEV(address(),HEX);DEV();
      memcpy_P(shadow_registers, Power_Up_Register_Values, Control_Register_Max);
      }
    void sync_shadow_registers() { /* fixme: read device, for ... shadow=; */ }

  public: // Special classes
    class Broadcast;

  private:
    friend class TLC59116Manager; // so it can construct TLC59116's
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
