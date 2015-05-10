// This is full of Doxygen markup, especially intrusive is the "block markers" like [Some Comment in brackets]. bummer.

#include <Wire.h> // TLC59116 uses Wire
// If you #define TLC59116_LOWLEVEL 1
//     or #define TLC59116_DEV 1
// then you'll need this include too:
#include <avr/pgmspace.h>
#include "TLC59116.h"

//! [Need a manager]
// ONLY 1 manager
// Use defaults, see TLC59116Manager:TLC59116Manager()
TLC59116Manager tlcmanager; 
//    OR
// Specify the I2C interface, I2C speed
// see TLC59116Manager:TLC59116Manager(TwoWire, long, byte)
TLC59116Manager tlcmanager(Wire,400000); // default I2C/device initialization
//    OR
// Specify the I2C interface, I2C speed, and initialization
TLC59116Manager tlcmanager(Wire,400000, TLC59116Manager::Reset | TLC59116::WireInit); // e.g. don't init Wire

//! [Need a manager]

//! [Init the manager]
void setup() {
    Serial.begin(9600);

    // inits Wire, devices, and collects them
    tlcmanager.init(); // uses the options from the definition of tlcmanager 
    //! [Init the manager]

    //! [device info]
    Serial.print("First device is at ");
    Serial.println(tlcmanager[0].address());
    //! [device info]

    //! [device setup]
    tlcmanager.broadcast().set_milliamps(5); // everybody
    tlcmanager[4].allcall_address(FALSE); // 5th \[sic] device ignores broadcast now.
    //! [device setup]

    //! [initial channel settings]
    tlcmanager.broadcast().set(0, HIGH); // every device, channel 0 on
    tlcmanager[0].pwm(13, 128); // set device[0], channel 13, PWM to half bright
    //! [initial channel settings]
    }

//! [Static referencing devices]
void loop() {
    // "static" "reference" acts like a global. note the "&"
    TLC59116 &first_device = tlcmanager[0]; // the lowest address device
    //! [Static referencing devices]

    //! [set_outputs by bit pattern]
    // Bit patterns are conveniently written using hex or binary:
    // Each "digit" of a hex number is 4 bits, so 0xF is binary "1111"
    // e.g. 0xFFFF is 16 bits on
    // Binary is written out like: 0b10111. High bits are 0.

    // _change_ first channel to on
    first_device.set_outputs(0xFFFF, 0b1); // bits 1..15 of _bit_which_ are 0, so "no change"
    // _change_ first channel to off
    first_device.set_outputs(0x0000, 0b1); // bits 1..15 of _bit_which_ are 0, so "no change"

    // _change_ to ON, for all 16 channels
    first_device.set_outputs(0xFFFF, 0xFFFF); 
    // change odd channels to on, leave even unaffected
    first_device.set_outputs(0xFFFF, 0b0101010101010101);

    // _bit_which_ can be a variable, expression, function, etc.
    first_device.set_outputs(0xFFFF, rand(0x10)); // randomly turn on the first 4 channels
    first_device.set_outputs(0x0000, rand(0x10)); // randomly turn off the first 4 channels

    // _bit_pattern can be a variable, expression, function, etc.
    first_device.set_outputs(rand(0xF) 0x000F); // randomly _change_ the first 4 channels
    //! [set_outputs by bit pattern]

    //! [Index referencing devices]
    // The manager knows how many there are:
    for (int i=0; i<tlcmanager.device_count(); i++) {
        // turn on channel _i_ of each device
        tlcmanager[i].set(i); // in the list in order of address
        }
    //! [Index referencing devices]

    // Since most functions return the device, we can:
    //! [chain functions together]
    tlcmanager[0].on(1).pwm(2,128).pwm(3,14, 50).delay(200).off(1).delay(200).set_outputs(0x0000);
    //! [chain functions together]

    }



