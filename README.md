#define TLC59116_h

// Shadow the devices state, take them into account for high-level operations
// So you don't have to: less bugs, more convenience.
// Less confusion by allowing "." instead of "->"
// Useful "default" forms to get started easier.
// Warnings/Info available

// Set this to 1/true to turn on Warnings & info
#ifndef TLC59116Manager_WARNINGS
  #define TLC59116Manager_WARNINGS=0
#endif

// Set this to enable group_pwm
#ifndef TLC59116_UseGroupPWM
  #define TLC59116_UseGroupPWM 0
#endif

// Set this to 1/true to turn on lower-level development debugging
#ifndef TLC59116Manager_DEV
  #define TLC59116Manager_DEV=0
#endif

#include <Arduino.h>
#include <Wire.h>
#include <TLC59116_Unmanaged.h>

class TLC59116 {
  // High level operations,
  // Relieves you from having to track/manage the modes/state of each device.
  // Get one from TLC59116_Manager x[]

  public:
byte address() { return i2c_address; }

// disabled because of bug
#if TLC59116_UseGroupPWM
    TLC59116& group_pwm();
#endif

  private:
// Manager has to track for reset, so factory
TLC59116() :  address(byte i) {} // factory control, must get from manager
TLC59116(const &TLC59116); // singleton
TLC59116& operator=(const TLC59116&); // singleton
~TLC59116() {} // managed destructor....

// our identity is the bus address (w/in a bus)
byte i2c_address;
// No link to the bus (yet)

void reset_happened(); // reset affects the state of the devices

// We have to shadow the state
byte shadow_registers[TLC59116_Unmanaged::Control_Register_Max+1];
void reset_shadow_registers() { for ... shadow_register[i] = known[x] }
void sync_shadow_registers() { read device, for ... shadow=; }

  }

class TLC59116Manager {
  // Manager
  // Holds all-devices (on one bus) because reset needs to affect them

  public:
static const WireInit = 0b1;
static const EnableOutputs = 0b10;
static const Reset = 0b100;

// specific I2C bus, because reset affects only 1 bus
TLC59116Manager(TwoWire &w, // Use the specified i2c bus (shouldn't allow 2 of these on same bus)
  long frequency = 100000L // default to 100khz, it's the default for Wire
    // turn-off pattern: 0xFF ^ X::EnableOutputs
  byte dothings = WireInit | EnableOutputs | Reset, // do things by default
  ) : bus(w);
TLC59116Manager() : TLC59116Manager(Wire)  // convenience I2C bus using Wire, the standard I2C arduino pins
// You'll have to write an adaptor for other interfaces

// Get the 0th, 1st, 2nd, etc. device (index, not by i2c-address). In address order.
// Can return null if index is out of range, or there are none!
// Pattern: Blah::Blah& first = manager[0]; // Note the '&'
// Pattern: manager[i].all_on(); // you can use "." notation
// FIXME: can we do: BaseClass& x = manager[0];
TLC59116Manager& operator[](byte index) { if (index >= device_t) <debug>return null; return *(devices[index]); }
byte device_count() { return device_ct; } // you can iterate

reset(); // all devices on bus

  private:
TLC59116Manager(&TLC59116Manager); // undefined
TLC59116Manager& operator=(const TLC59116Manager&); // undefined

// Specific bus
TwoWire &bus;

// Need to track extant
TLC59116* devices[MaxDevicesPerI2C]; // that's 420 bytes of ram
byte device_ct;

  }

#endif

