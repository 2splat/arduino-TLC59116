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
TLC59116(byte address) : TLC59116_Unmanaged(address) {}  // factory control, must get from manager
TLC59116(); // none
TLC59116(const TLC59116&); // none
TLC59116& operator=(const TLC59116&); // none
~TLC59116() {} // managed destructor....

// No link to the bus (yet)

void reset_happened(); // reset affects the state of the devices

// We have to shadow the state
byte shadow_registers[Control_Register_Max+1];
void inline reset_shadow_registers(); 
void sync_shadow_registers() { /* fixme: read device, for ... shadow=; */ }

