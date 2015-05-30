#ifndef TLC59116_h
#define TLC59116_h

/* Documentation assumes Doxygen */

// FIXME: insert version here


/** \todo
  - put in examples for pwm, etc.
  - I think something that accumulates changes, then does them as a "block"
*/

#include <Arduino.h>
#include <Wire.h> // Usage.Include+: You have to do this in your .ino also
#include <avr/pgmspace.h> // Usage.Include+: You have to do this in your .ino also
#include "TLC59116_Unmanaged.h"

extern TwoWire Wire;
class TLC59116Manager;

#define WARN TLC59116Warn // FIXME: how about a include? or 2 copies (sed'ed)?
#define DEV TLC59116Dev 
#define LOWD TLC59116LowLevel

/// High Level interface to a single TLC59116 using Wire (I2C) interface.
/** 

  (see TLC59116_Unmanaged for lower level library)

  ### Usage:

  \include basic_usage_single/basic_usage_single.ino

  There are examples in the Arduino IDE examples, and of course, the download site https://github.com/2splat/arduino-TLC59116 .

  This high-level interface tracks each device's state so that commands appear to be independant.
  For example, you can set the digital-on of a channel without worrying about the 
  other 4 mode settings in each LEDOUTx_Register.

  Some optimization of the data sent to the devices is also done.

  ### The TLC59116 Device
  See the TI spec sheet for much detail: http://www.ti.com/lit/ds/symlink/tlc59116.pdf

  - Output pins ("channels") are numbered 0..15.
  - Channels are "sinks", acting like Arduino pins that are LOW, even for PWM.
    - Connect the minus side of LEDs to the output pins.
    - Instead of HIGH/LOW like DigitalWrite(), we use on/off/set.
    - Channels are considered ON, OFF, , instead of HIGH/LOW (or, at some PWM value)
    - PWM is 8 bit.
  - I2C addresses start at 0x60. See TLC59116_Unmanaged for the available addresses (up to 14).
  - The device provides a constant-current control (external and software settable).
    This means you can set the current, usually leave out resistors on each LED/channel, and is more efficient.
    See also, TLC59116::milliamps().
  - This library discovers the devices on the I2C bus and keeps them in a list.
    - Use the _index_ in this library: e.g. "0" is the device with the lowest address, 
      "1" is the next highest, etc.
    - This library exposes I2C addresses as 7bit addresses.
  - Individual devices cannot be "reset", all devices on the I2C bus are affected by a reset.
  - By default, all devices respond to broadcast ("allcall") commands (a special address).
    - This can be changed, or turned off, for each device (see TLC59116::allcall_address())
    - Three additional "broadcast" addresses can be set. See TLC59116::SUBADR_address()
    - Thus, a device can participate in 4 different broadcast groups (or none):
      - allcall_address()
      - SUBADR_address(0)
      - SUBADR_address(1)
      - SUBADR_address(2)
  - See the various groupings of the functions (below) for functionality.
  
  ### Protocol:

  - Setting up
    - Declare a static TLC59116Manager (static scope, or "static" keyword if you are very clever)
      \snippet allfeatures/allfeatures.ino Need a manager
    - Initialize the serial-output e.g.
      - `Serial.begin(9600)`
      - This library will warn you about some mistakes or dubious usages.
      - You don't have to setup the serial-output, but then you won't see the warnings.
      - I like to set the speed to 115200, because at that setting it has little effect 
        on the speed of your program.
    - Execute \ref TLC59116Manager::init() ".init()" for the manager once, usually in setup()
      \snippet allfeatures/allfeatures.ino Init the manager
    - The devices will then be in a ready state (by default, outputs are enabled, but all channels are off)
  - Obtain a reference from yourmanager\[i],  for i from 0..device_count()-1
    - This library passes and returns references so that "." can always be used.
      \snippet allfeatures/allfeatures.ino Static referencing devices
      \snippet allfeatures/allfeatures.ino Index referencing devices
  - Get information about the device as needed from the \ref Info Functions
      \snippet allfeatures/allfeatures.ino device info
  - Set device \ref Global Functions, \ref Group/Broadcast Addresses, \ref Software Current Control, typically in setup()
      \code{.cpp}
      void setup() {
      \endcode
      \snippet allfeatures/allfeatures.ino device setup
  - Do initial \ref Digital Functions, \ref PWM Functions, typically in setup()
      \snippet allfeatures/allfeatures.ino initial channel settings
  - During loop, mess with \ref Global Functions, \ref Group/Broadcast Addresses, \ref Software Current Control, \ref Digital Functions, \ref PWM Functions
  - This library returns the device-reference for most calls, so you can chain functions, e.g.:
    \snippet allfeatures/allfeatures.ino chain functions together

*/
class TLC59116 : public TLC59116_Unmanaged {

  public:
    // No constructor, get from a TLC59116Manager[i]

    /// \name Global Functions
    ///@{
    //! Controlling various global features of the device (see also, the "Software Current Control" section)

    /// Master power switch (the TLC59116Manager sets this on/true by default)
    /** It takes the device a moment to enable power, so typically leave _with_delay_ true **/
    virtual TLC59116& enable_outputs(
      bool yes = true,  //!< false to disable outputs
      bool with_delay = true //!< delay for 500msec
      );

    /// Is master power on?
    bool is_enable_outputs() { return !is_OSC_bit(shadow_registers[MODE1_Register]); };
    bool is_enabled() { return is_enable_outputs(); } ///< Alias for is_enable_outputs()
    ///@}

    // High level

    /** \name Digital Functions
      Analogous to DigitalWrite.

      Arduino DigitalWrite uses HIGH and LOW, but the device is a "sink", so "on" is actually a LOW. 
        Thus, this library uses "on" and "off" for terminology.

      There are several aliases, and overloaded functions here (and for the pwm functions below). 
      The intention was to provide natural
      overloads and aliases. I've shied away from using the Arduino DigitalWrite and AnalogWrite so far.

      These functions only change the specified channels, they'll leave other outputs, like a PWM channel alone.

      You can have some channels doing PWM, and some channels doing digital.
      
    */
    ///@{
    // FIXME: pattern_bits(bits, which). set_bits(pattern). reset_bits(pattern). on/off/set
    TLC59116& set_outputs(
        word pattern,  //!< on/off for each channel by bit
        word which //!< But, only change these bits
      ); //!< Only change the channels (that are marked in _bit_which_) to on/off as marked in _bit_pattern_
      /**< 
        (cf. on_pattern(word), on(), off() )

        Examples:
        \snippet allfeatures/allfeatures.ino set_outputs by bit pattern
      */
    /// Alias for set_outputs()
    TLC59116& pattern(word bit_pattern, word bit_which=0xFFFF) { return set_outputs(bit_pattern, bit_which); }

    /// Turn on by bit pattern
    /** See set_outputs(word,word) examples for hex/binary notation hints 

        Examples:
        \snippet allfeatures/allfeatures.ino on_pattern
    */
    TLC59116& on_pattern(word pattern) { return set_outputs(pattern, pattern); }

    /// Turn off by bit pattern
    /** See set_outputs(word,word) examples for hex/binary notation hints 

        Examples:
        \snippet allfeatures/allfeatures.ino off_pattern
    */
    TLC59116& off_pattern(word pattern) { return set_outputs(~pattern, pattern); }

    /// Turn one on/off, much like DigitalWrite.
    /** Examples:
        \snippet allfeatures/allfeatures.ino using set
    */
    TLC59116& set(int led_num, bool offon /**< _true_ for on */ ) {
      word bits = 1 << led_num;
      return set_outputs(offon ? bits : ~bits, bits); 
      }
    TLC59116& on(int led_num) { return set(led_num, true); } ///< Turn one on
    TLC59116& off(int led_num) { return set(led_num, false); } ///< Turn one off
    // \todo maybe a pattern(byte array)? maybe a pattern(a,b,c,...)?
    ///@}

    /** \name PWM Functions
      Analogous to AnalogWrite

      You can have some channels doing PWM, and some channels doing digital.

      Remember, the device is a "sink", even for PWM. Minus side of LED goes to the output pin.

      \bug Be careful with start/end arguments, if they are out of range (0..15) you will have mysterious problems.
      \todo Put in range checking
    */
    ///@{

    /// PWM, start..end, list-of-brightness
    /** Examples:
        \snippet allfeatures/allfeatures.ino range of pwm
    */
    TLC59116& set_outputs(
      byte led_num_start, ///< first channel
      byte led_num_end, ///< last channel
      const byte brightness[] ///< A list of PWM values. Tolerates led_num_end>15 which wraps around
      );

    TLC59116& pwm(byte led_num_start, byte led_num_end, const byte brightness[] /*[ct]*/) { return set_outputs(led_num_start, led_num_end, brightness); } ///< Alias of set_outputs() above

    /// PWM, start..end same brightness
    /** Examples:
        \snippet allfeatures/allfeatures.ino same pwm
    */
    TLC59116& pwm(byte led_num_start, byte led_num_end, byte pwm_value) {
      const byte register_count = led_num_end - led_num_start +1;
      // FIXME: warnings of range
      byte pwm_buff[register_count];
      memset(pwm_buff, pwm_value, register_count);
      return set_outputs(led_num_start, led_num_end, pwm_buff); 
      }

    /// Set all 16 PWM according to the brightness list
    /** Examples:
        \snippet allfeatures/allfeatures.ino all pwm
    */
    TLC59116& set_outputs(const byte brightness[16] ) { return set_outputs(0,15, brightness); }
    TLC59116& pwm(const byte (&brightness)[16]) { return set_outputs(0,15, brightness); } ///< Set all 16 PWM according to the brightness list, same as above.

    /// PWM for one channel
    /** Examples:
        \snippet allfeatures/allfeatures.ino one pwm
    */
    TLC59116& pwm(byte led_num, byte brightness) { byte ba[1] = {brightness}; return set_outputs(led_num, led_num, ba); }
    // fixme: maybe brightness()?
    ///@}
    
    /** \name Group Functions
    \warning The TLC59116 has a hardware bug:

    If you decrease the value (blink_time or brightness) in these functions,
    the chip may act like the value is max for one "cycle".
    This causes a full-brightness result for group_pwm, and a 10 second blink-length for group_blink.
    I think this is because:
    - There is a continously running timer, counting up.
    - The "value" is compared to the timer, and triggers the action and a reset of the timer.
    - If you move the "value" down, you may skip over the current timer value,
       so it runs to the maximum value before wrapping around again.
    "reset" is the only thing that I know of that will reset the timer.
    But, more experimentation is needed.
    
    I recommend you only set group functions once (or only increase values).

    Of course, you could TLC59116Manager::reset() everytime, too.
    */
    ///@{
    // Warning, "group" functions have a bug in the device hardware

    /// Superpose group-pwm on current pwm setting (but, current should be 255 for best results)
    /// \warning Hardware Bug, see \ref Group Functions 
    TLC59116& group_pwm(
      word bit_pattern, ///< channels by bit 
      byte brightness ///< superposed PWM
      );

    /// Blink all the LEDs, at their current/last PWM setting. 
    /** Set PWM values first.
      To turn off blink, set channels to PWM or digital-on/off

      See TLC59116& group_blink(double, double) for a version in seconds/percent.

      \warning Hardware Bug, see \ref Group Functions 
    */
    TLC59116& group_blink(
      byte blink_period, ///< blink-length secs = (blink_period + 1)/24
      byte on_ratio=128  ///< ratio that the blink is on: on_ratio/256
      ) { return group_blink(0xFFFF,blink_period,on_ratio); }

    /// As TLC59116& group_blink(byte,byte), but you can specify the channels by bit-pattern
    //! \warning Hardware Bug, see \ref Group Functions 
    TLC59116& group_blink(word bit_pattern, int blink_delay, int on_ratio=128);

    /// Blink all the LEDs, at their current/last PWM setting by len/%.
    //! \warning Hardware Bug, see \ref Group Functions 
    TLC59116& group_blink(
      double blink_length_secs, ///< 0.041secs (24.00384hz) to 10.54secs (.09375hz) in steps of .04166secs
      double on_percent=50.0 ///< % of blink-cycle that is on
      ) {
      return group_blink(0xFFFF,blink_length_secs,on_percent); 
      }

    /// As TLC59116& group_blink(double,double), but you can specify the channels by bit-pattern
    //! \warning Hardware Bug, see \ref Group Functions 
    TLC59116& group_blink(word bit_pattern, double blink_length_secs, double on_percent=50.0) { // convenience % & len
      // on_percent is 05..99.61%, blink_length is 0.041secs (24.00384hz) to 10.54secs (.09375hz) in steps of .04166secs 
      return group_blink(
        bit_pattern, 
        (byte) int(blink_length_secs/0.041666667 + .0001), 
        (byte) int(on_percent * 256.0/100.0)
        );
      }

    /// Same as TLC59116& group_blink(word, double, double), but blink_length is an integer (1..10 secs)
    //! \warning Hardware Bug, see \ref Group Functions 
    TLC59116& group_blink(unsigned int bit_pattern, int blink_length_secs, double on_percent=50.0) {
      return group_blink((word)bit_pattern, blink_length_secs, int(on_percent * 256.0/100.0));
      }
    ///@}

    /** \name Group/Broadcast Addresses
      Addresses for broadcast (_allcall_address_) and group-broadcase (_SUBADR_x_).

      Addresses should be specified as 0x60..0x6F.

      It is illegal to set any address to Reset_Addr (0x6B), it will be ignored with warning.
    */
    ///@{

    /// The reset-address, unchangeable.
    /** Not much you should do with this. See TLC59116Manager::reset() */
    byte Reset_address() { return Reset_Addr >> 1; }

    /// Get broadcast address, see below to set/enable.
    byte allcall_address() { return shadow_registers[AllCall_Addr_Register] >> 1; }

    /// Set and enable/disable the broadcast address (_allcall_addr_).
    /** Each device can have a different allcall_address, and have it disabled/enabled.

      By default, the TLC59116Manager arranges for the broadcast address to be enabled.
    
      \bug However, the TLC59116Manager.broadcast() object _only_ assumes the default allcall_address 0x68.
      So, it won't work if you change the allcall_address.

      Examples:
        \snippet allfeatures/allfeatures.ino set allcall
    */
    TLC59116& allcall_address(byte address, bool enable=true);

    TLC59116& allcall_address_enable(); ///< Enable broadcast
    TLC59116& allcall_address_disable(); ///< Disable broadcast
    bool is_allcall_address() { return shadow_registers[AllCall_Addr_Register] & MODE1_ALLCALL_mask; } ///< Is broadcast enabled?

    // SUBADR's are disabled at reset
    // SUBADR are 1,2,3 for "which"
    /// Get the SUBADR_n address.
    /** Each device can have a 3 more broadcast addresses. Thus you can form various groups of the devices.

      By default, the SUBADR's are disabled. You must enable them if you want to use them (see SUBADR_address(byte, byte, bool), SUBADR_address_enable(byte), SUBADR_address_disable(byte) ).

      The default addresses are: are 0x69, 0x6a, 0x6b

    */
    byte SUBADR_address(byte which) { return shadow_registers[SUBADRx_Register(which)] >> 1; }
    bool is_SUBADR_address(byte which) { return is_SUBADR_bit( shadow_registers[MODE1_Register], which); } ///< Is SUBADR_n enabled?

    /// Set the SUBADR_n address.
    /** 
      Examples:
        \snippet allfeatures/allfeatures.ino set subadr
    
      \bug We don't track commands on SUBADR_n's, so this library will get very confused 
      about the state of the devices and do confusing things if you mix broadcasts to 
      SUBADR_n devices, and individual devices in that SUBADR_n group.

      \bug Also, this library provides no easy access to broadcast to a SUBADR_n group.

      \todo Provide SUBADR_n broadcast functionality.
    */
    TLC59116& SUBADR_address(byte which, byte address, bool enable=true);
    TLC59116& SUBADR_address_enable(byte which); ///< Enable SUBADR_n
    TLC59116& SUBADR_address_disable(byte which); ///< Disable SUBADR_n
    ///@}
    
    /// \name Software Current Control
    ///@{

    /** We need a Rext value (the resistor attached to the Rext pin) 
      to calculate this, e.g. 156ohms gives 120mA at reset (default), 937ohms gives 20mA.
      This function can use a default value of 156ohms to do the calculation.
    
      By default, this is set to maximum current (e.g. 120mA for 156ohm). 
      Remember that common LEDs are usually 20mA, though 10mA is becoming common
      (the tiny surface-mount LEDs are usually 5mA).

      You probably want to set the current before you turn on any LEDs!

      Note that this will tend to set the output 1milliamp low (due to rounding).
    
      This will also clip the calculated mA to the minimum..120mA range (for Rext).
      
      Check the extant/calculated value with milliamps().

      See the section "The TLC59116 Device" for some short notes on "constant current".

      Examples:
        \code{.cpp}
        void setup() {
        \endcode
        \snippet allfeatures/allfeatures.ino initial current control

        \code{.cpp}
        void loop() {
        \endcode
        \snippet allfeatures/allfeatures.ino current control
    */
    TLC59116& set_milliamps(byte ma, int Rext=Rext_Min); // Rext_Min ohms is 120ma at reset.

    int milliamps(int Rext=Rext_Min) {return i_out(shadow_registers[IREF_Register]); } ///< The calculated current setting
    ///@}

    // Error detect
    // FIXME: implement. we should set error enable on reset? then read them?

    /** \name Convenience
      Since we allow function-chaining, it seems like a few convenience functions might be nice.

      Note all the functions (not just these convenience functions) that look like
        
        TLC59116& xxx(...)

      They return a reference to the device, so you can chain functions.
    */
    ///@{
    TLC59116& delay(int msec) { ::delay(msec); return *this;} ///< Just like the Arduino delay() function
    ///@}

    /** \name Info Functions
      See a few other functions whose name starts with "is_".
    */
    ///@{
    // \todo checking a shadow_register value
    // (make doxygen include this fn, but mess up the brief doc, thanks)
    // \fn byte address()
    ///@}

    /** \name Debugging
      You can enable debug output by editing TLC59116.cpp, and TLC59116_Unmanaged.cpp. Add one or both:
        \code
        #define TLC59116_LOWLEVEL 1
        #define TLC59116_DEV 1
        \endcode
    */
    ///@{
    TLC59116& describe_shadow(); ///< Print the registers, as we've cached them. cf. describe_actual()
    TLC59116& resync_shadow_registers(); ///< Fetch the actual into our cached registers
    ///@}

  private:
    // FIXME: move Power_Up_Register_Values to FLASH
    /// The state of the device on power-up/reset.
    static const unsigned char Power_Up_Register_Values[TLC59116_Unmanaged::Control_Register_Max + 1] PROGMEM;

    TLC59116Manager &manager; /// so we can find the manager for a few things
    // void (*on_reset)(byte /* manager[i] */); 

    // Manager has to track for reset, so factory instead of public constructors
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
  public:
    Broadcast(TwoWire &bus, TLC59116Manager& m) : TLC59116(bus, TLC59116::AllCall_Addr, m) {}

    Broadcast& enable_outputs(bool yes = true, bool with_delay = true);

  private:
    // odd, the trailing comment next is not captured (cons/decons?)
    Broadcast(); // none
    Broadcast(const Broadcast&); // none
    Broadcast& operator=(const TLC59116&); // none
    friend class TLC59116Manager;
    ~Broadcast() {} // managed destructor....

    void propagate_register(byte register_num);

  };

/// Holds all of the devices.
/** 
  - Inits the I2C bus, and devices (see TLC59116Manager::TLC59116Manager())
  - Collects and holds the devices (cf TLC59116Manager::operator[]) 
  - Provides reset (TLC59116Manager::reset())
  - Provides a broadcast object (TLC59116Manager::broadcast())
  
  The reset command for TCL59116's is a broadcast command, so we manage that here.

  \bug 
  This collects all I2C devices between 0x60 and 0x6E. Non TLC59116 devices can appear in our list!
  The TLC59116 doesn't have a feature that identifies it, so we just assume.

*/
class TLC59116Manager {

  // FIXME: this skips the device at AllCall_Addr, allow a remediation of that
  // FIXME: likewise, subadr is disabled at reset, allow remediation to exclude that on scan

  friend class TLC59116;

  public:
    static const byte MaxDevicesPerI2C = 15; // 16 addresses - reset (subadr1..subadr3 and allcall can be disabled)
  
    // manager init flags. Default setting should be 1 FIXME: enum?
    static const byte WireInit = 0b1; // call .begin() on the i2cbus. Unset if you've already done that
    // After i2c init, and devices reset, turn outputs on (see TLC59116.enable_outputs)
    static const byte EnableOutputs = 0b10; // device default at on/reset is "outputs disabled"
    static const byte Reset = 0b100; // reset all devics after i2c init (see TLC59116Manager.reset)
    static const byte Already = 0b1000; // internal use, leave unset

    static const long Default_Frequency = 100000L; // default to 100khz, it's the default for Wire

    // Protocol: * Get a manager via a constructor
    // Simple constructor, uses Wire (standard I2C pins), 100khz bus speed, resets devices, and enables outputs
    // NB: The IDE will get confused if you do: TLC59116Manager myname(); in the global section, so leave off "()"
    TLC59116Manager() : i2cbus(Wire), init_frequency(Default_Frequency), reset_actions( WireInit | EnableOutputs | Reset) {}
    // Allow override of setup
    // E.g. TLC59116Manager tlcmanager(Wire, 50000, EnableOutputs | Reset); // 500khz, don't do Wire.init()
    // You'll have to write an adaptor for other (non-Wire) I2C interfaces
    TLC59116Manager(TwoWire &w, // Use the specified i2c bus (e.g. Wire) (shouldn't allow 2 of these on same bus)
        long frequency = Default_Frequency, // hz. (Screws With:) This screws with TWBR 
          // Only certain speeds are actually allowed, rounds to nearest
          // FIXME: table of speeds
        byte dothings = WireInit | EnableOutputs | Reset // Do things now and at reset()
        ) : i2cbus(w), init_frequency(frequency), reset_actions(dothings) { }

    // Protocol: * You have to call .init() at run-time (usually in setup)
    /* Does the things indicated by the constructor's "dothings":
      * inits the i2c bus (see Wire.init())
      * set i2c frequency (TWBR)
      * finds devices (See TLC59116Manager.scan)
      * resets all devices (See TLC59116Manager.reset)
      * enables outputs on all devices (See TLC59116.enable_outputs)
      NB: Only acts if !is_inited() (i.e. .init() is only usable once)
    */
    void init();
    bool is_inited() { return !!(this->reset_actions & Already); } // you can check if .init() was called already

    // Protocol: * Get a specific device with themanager[i]
    // Get the 0th, 1st, 2nd, etc. device (index, not by i2c-address). In address order.
    // Can return null if index is out of range, or there are none!
    // Pattern: TLC59116& first = manager[0]; // Note the '&'
    // Pattern: manager[i].all_on(); // you can use "." notation
    // \todo get by I2C address, which we might could do because 0x60 > 15...
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
    byte device_count() { return device_ct; } // you can iterate. FIXME: implement interator protocol

  public: // global things
    // Protocol: * OR get the .broadcast() object that sends the same command to all devices
    // \todo reconsider the subclass, instead propagate if self.address==thebroadcast address
    // \todo and, broadcast can change, and there are subadr's, so we should deal with those.
    TLC59116::Broadcast& broadcast() { static TLC59116::Broadcast x(Wire, *this); return x; }

    // Protocol: * Reset all devices with .reset(), when desired
    int reset(); // 0 is success, does enable_outputs if init(...EnableOutputs...)

    // consider: is_SUBADR(). has to track all devices subaddr settings (value & enabled)

  private:
    TLC59116Manager(const TLC59116Manager&); // undefined
    TLC59116Manager& operator=(const TLC59116Manager&); // undefined
    int scan(); // client code can't rescan

    // Specific bus
    TwoWire &i2cbus;
    byte reset_actions;
    long init_frequency;

    // Need to track extant
    TLC59116* devices[MaxDevicesPerI2C]; // that's 420 bytes of ram
    byte device_ct;

    // We have to store the power-up values, so we can set our shadows to them on reset
    static const unsigned char Power_Up_Register_Values[TLC59116::Control_Register_Max+1] PROGMEM;
  };

#endif
