// Code, that compiles, to show all of the library features.
// I suppose it will "run", but not usefully! So, don't bother.
// See the documentation where all the snippets are shown.

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
TLC59116Manager tlc4kHz(Wire,400000); // default I2C/device initialization
//    OR
// Specify the I2C interface, I2C speed, and initialization
TLC59116Manager tlcWithOtherWire(Wire,400000, TLC59116Manager::Reset | TLC59116Manager::WireInit); // e.g. don't init Wire

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
    tlcmanager[4].allcall_address(false); // 5th \[sic] device ignores broadcast now.
    //! [device setup]

    //! [initial current control]
    // Assume we put a 100ohm resistor on the Rext pin
    // set it to 5mA before we play with any outputs
    tlcmanager[0].set_milliamps(5,100);
    Serial.print("Device 0 is actually ");
    Serial.print(tlcmanager[0].milliamps(100)); // might be slightly off due to rounding
    Serial.println("mA");
    //! [initial current control]

    //! [initial channel settings]
    tlcmanager.broadcast().set(0, HIGH); // every device, channel 0 on
    tlcmanager[0].pwm(13, 128); // set device[0], channel 13, PWM to half bright
    //! [initial channel settings]

    }

//! [Static referencing devices]
void loop() {
    // "static" "reference" acts like a global. note the "&"
    static TLC59116 &first_device = tlcmanager[0]; // the lowest address device
    //! [Static referencing devices]

    //! [temp referencing devices]
    // Name it for meaning (note "&", but absence of "static")
    TLC59116 &left_arm = tlcmanager[1];
    left_arm.on(4);
    left_arm.pwm(6,20);


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
    first_device.set_outputs(0xFFFF, random(0x10)); // randomly turn on the first 4 channels
    first_device.set_outputs(0x0000, random(0x10)); // randomly turn off the first 4 channels

    // _bit_pattern can be a variable, expression, function, etc.
    first_device.set_outputs(random(0xF), 0x000F); // randomly _change_ the first 4 channels


    // Leave "other" channels alone
    first_device.pwm(1,128); // channel 1 set to half bright
    first_device.set_outputs(0b101,0xb101); // Only change 0 & 2, 1 is still PWM=128
    //! [set_outputs by bit pattern]

    //! [on_pattern]
    // Only the on-bits have an effect:
    first_device.on_pattern(0b101); // channel 0 and 2 on
    first_device.on_pattern(random(0xF)); // randomly turn on the first 4 channels
    first_device.on_pattern(0); // no effect: no bits set
    first_device.on_pattern(0xFFFF); // All on
    //! [on_pattern]

    //! [using set]
    // Setting on/off for a pin
    first_device.set(0,true); // turn channel 0 on
    first_device.set(0,false); // turn channel 0 false
    first_device.set(1,random(1)); // random setting of channel 1
    first_device.set(1,random(2)); // On 2/3 of the time, randomly
    first_device.set(15,true); // turn the last channel on
    //! [using set]

    //! [off_pattern]
    // Only the bits that are set have an effect:
    first_device.off_pattern(0b101); // channel 0 and 2 off, all others left alone
    first_device.off_pattern(random(0xF)); // randomly turn off the first 4 channels
    first_device.off_pattern(0); // no effect: no bits set
    first_device.off_pattern(0xFFFF); // All off
    //! [off_pattern]


    //! [range of pwm]
    // Simple list of brightness for 0,1,2
    byte brightness[] = {10,20,30};
    first_device.set_outputs(0,2,brightness); // set first 3 to very dim values

    // The range gets "wrapped" back to 0..15
    byte wrapbrightness[] = {100,110,120,130}; // 4 brightnesses
    first_device.set_outputs(15,18, wrapbrightness); // 15=100, 0=110, 1=120, 2=130
    //! [range of pwm]

    //! [all pwm]
    // Set all 16 PWM values. Probably you would have calculated all of these.
    byte allbrightness[] = {101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116};
    first_device.set_outputs(allbrightness);
    //! [all pwm]

    //! [same pwm]
    first_device.pwm(4,15, 129); // 4..15 are just above half-bright
    //! [same pwm]

    //! [one pwm]
    first_device.pwm(4, 240); // just channel 4 to pretty-bright
    //! [one pwm]

    //! [Index referencing devices]
    // The manager knows how many there are:
    for (int i=0; i<tlcmanager.device_count(); i++) {
        // turn on channel _i_ of each device
        tlcmanager[i].on(i); // in the list in order of address
        }
    //! [Index referencing devices]

    // Since most functions return the device, we can:
    //! [chain functions together]
    tlcmanager[0].on(1).pwm(2,128).pwm(3,14, 50).delay(200).off(1).delay(200).set_outputs(0x0000);
    //! [chain functions together]

    //! [set allcall]
    // Everybody change broadcast to I2C address 0x13
    tlcmanager.broadcast().allcall_address(0x13); 
    // Put first 2 devices on I2C address 0x14 for broadcast
    tlcmanager[0].allcall_address(0x14); 
    tlcmanager[1].allcall_address(0x14); 
    //! [set allcall]

    //! [set subadr]
    // Each device is red, green, or blue. Put them in groups
    for (int i=0; i<tlcmanager.device_count(); i++) {
        // device[0,3,6,9,12,15] has SUBADR_0=0x10,
        // device[1,4,7,10,13] has SUBADR_0=0x11,
        // device[2,5,8,11,14] has SUBADR_0=0x12,
        tlcmanager[i].SUBADR_address(0, 0x10 + (i%3)); // modulus wraps i to 0..2
        }
    
    // Now, if we knew how, we could tell the 3 different groups to do things
    //! [set subadr]

    //! [current control]
    // Assume we put a 100ohm resistor on the Rext pin
    // Mess with the current
    tlcmanager[0].set_milliamps(random(100)+20,100); // probably cook our led!
    //! [current control]
    }



