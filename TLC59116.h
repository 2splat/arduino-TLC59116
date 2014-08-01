#ifndef TLC59116_h
#define TLC59116_h

// Shadow the devices state, take them into account for high-level operations
// So you don't have to: less bugs, more convenience.
// Less confusion by allowing "." instead of "->"
// Useful "default" forms to get started easier.
// Warnings/Info available

// Set this to 1/true to turn on lower-level development debugging
#ifndef TLC59116_DEV
  #define TLC59116_DEV 0
#endif

#include <Arduino.h>
#include <avr/pgmspace.h>
#include <Wire.h>
#include "TLC59116_Unmanaged.h"

extern TwoWire Wire;
class TLC59116Manager;

class TLC59116 : TLC59116_Unmanaged {
  // High level operations,
  // Relieves you from having to track/manage the modes/state of each device.
  // Get one from TLC59116_Manager x[]

  friend class TLC59116Manager;

  public:


  private:
    // Manager has to track for reset, so factory
    TLC59116(TwoWire& bus, byte address, TLC59116Manager& m) : manager(m), TLC59116_Unmanaged(bus, address) {reset_shadow_registers();}  // factory control, must get from manager
    TLC59116(); // none
    TLC59116(const TLC59116&); // none
    TLC59116& operator=(const TLC59116&); // none
    ~TLC59116() {} // managed destructor....

    
    void reset_happened(); // reset affects the state of the devices
    void modify_control_register(byte register_num, byte mask, byte bits);
    void modify_control_register(byte register_num, byte value);

    // We have to shadow the state
    byte shadow_registers[Control_Register_Max+1];
    void inline reset_shadow_registers(); 
    void sync_shadow_registers() { /* fixme: read device, for ... shadow=; */ }

  public:
    virtual TLC59116& enable_outputs(bool yes = true, bool with_delay = true);

    TLC59116Manager& manager;

  public: // Special classes
    class Broadcast;
  };

class TLC59116::Broadcast : TLC59116 {
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
  
    // manager init flags
    static const byte WireInit = 0b1;
    static const byte EnableOutputs = 0b10;
    static const byte Reset = 0b100;

    // convenience I2C bus using Wire, the standard I2C arduino pins
    TLC59116Manager() : i2cbus(Wire) { init(100000L,  WireInit | EnableOutputs | Reset); }  
    // specific I2C bus, because reset affects only 1 bus
    TLC59116Manager(TwoWire &w, // Use the specified i2c bus (shouldn't allow 2 of these on same bus)
      long frequency = 100000L, // default to 100khz, it's the default for Wire
        // turn-off pattern: 0xFF ^ X::EnableOutputs
      byte dothings = WireInit | EnableOutputs | Reset // do things by default
      ) : i2cbus(w) { init(frequency, dothings); }
    // You'll have to write an adaptor for other interfaces

    // Get the 0th, 1st, 2nd, etc. device (index, not by i2c-address). In address order.
    // Can return null if index is out of range, or there are none!
    // Pattern: TLC59116& first = manager[0]; // Note the '&'
    // Pattern: manager[i].all_on(); // you can use "." notation
    // FIXME: can we do: BaseClass& x = manager[0];
    TLC59116& operator[](byte index) { 
      if (index >= device_ct) { 
        TLC59116Warn(F("Index "));TLC59116Warn(index);TLC59116Warn(F(" > max device "));TLC59116Warn(device_ct);TLC59116Warn();
        /// return NULL;
        } 
      else { 
        return *(devices[index]);
        } 
      }
    byte device_count() { return device_ct; } // you can iterate

  public: // global things
    int reset(); // 0 is success
    TLC59116::Broadcast& broadcast() { static TLC59116::Broadcast x(Wire, *this); return x; }

    // consider: is_SUBADR(). has to track all devices subaddr settings (value & enabled)

  private:
    TLC59116Manager(const TLC59116Manager&); // undefined
    TLC59116Manager& operator=(const TLC59116Manager&); // undefined
    void init(long frequency,
      byte dothings // turn-off pattern: 0xFF ^ X::EnableOutputs
      );
    int scan();

    // Specific bus
    TwoWire &i2cbus;
    byte reset_actions;

    // Need to track extant
    TLC59116* devices[MaxDevicesPerI2C]; // that's 420 bytes of ram
    byte device_ct;

    static const prog_uchar Power_Up_Register_Values[TLC59116::Control_Register_Max] PROGMEM;

  };

#endif
