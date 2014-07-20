#ifndef TLC59116_h
#define TLC59116_h

/*
  Pins:
    TLC59116: 27:SDA, 26:SCL
    UNO: A4:SDA, A5:SCL
    mega: as marked
    teensy: ...

  In your setup():
    Wire.begin();
    Serial.init(nnnnn); // if you do DEBUG=1, or use any of the describes
    // Run the I2C faster (default is 100000L):
    const long want_freq = 340000L; // 100000L; .. 340000L for 1, not termination
    TWBR = ((F_CPU / want_freq) - 16) / 2; // aka want_freq = F_CPU/(2*TWBR +16). F_CPU is supplied by arduino-ide


  FIXME: rewrite whole thing:
    singleton TLC
      tracks all, maybe pointers? 14 bytes vs 56, but easier to do id?
    an each {:addr=>x} using the singleton
    so reset does right thing, and other global state
    layer
      tlc[1].dmblnk = 1, ...
  ?? What does a read on AllCall get?
  The TLC59108 is supposedly the same except only 8 channels.
  * Change to group_blink(ratio, hz, pwm values)
    low level is blink-bit-mask
    highlevel is leds-are:x blink/on-off/pwm/grppwm
  !! erratic flashes seen when using GRPPWM and decreasing it over time
      claimed to happen at same point every time (speed dependant I think)
  !! I see flickering when: many LEDs (14) are pwm on fairly dim (20), and I on/off blink another LED.
      adjustng Vext seems to improve the situation (so, current?)
  !! This stuff appears to work in a timer-service-routine, but you must (re-)enable interrupts in the isr: sei();

*/

#include <Arduino.h>
#include <Serial.h>

// for .c
#include <Wire.h> // does nothing inthe .h


class TLC59116 {

  public:
    class Scan;

    static bool DEBUG; // defaults to "off". Set to 1 to get debug output.

    template <typename T> static void debug(T msg) { if (TLC59116::DEBUG) Serial.print(msg);} // and inlined
    template <typename T> static void debug(T msg, int format) { // and inlined
      if (TLC59116::DEBUG) {
        if (format == BIN) {
          Serial.print(F("0b")); // so tired
          for(byte i=0; i < (sizeof(msg) * 8); i++) {
            Serial.print( (msg & ((T)1 << (sizeof(msg) * 8 - 1))) ? "1" : "0");
            msg <<= 1;
            }
          }
        else {
          if (format == HEX) Serial.print(F("0x")); // so tired
          Serial.print(msg, format); 
          }
        }
      } 
    static void debug() { if (TLC59116::DEBUG) Serial.println();}

    static const char* Device; // "TLC59116" for printing

    static const byte Channels = 16; // number of channels

    // Addresses (7 bits, as per Wire library)
    // 8 addresses are unassigned, +3 more if SUBADR's aren't used
    // these are defined by the device (datasheet)
    static const byte Base_Addr    = 0x60; // 0b110xxxx
    static const byte Max_Addr     = Base_Addr + 15;   // +0b1111 +0x0F. 14 of 'em (16- reset & allcall)
    //                                         + 0x00  // Unassigned at on/reset
    //                                     ... + 0x07  // Unassigned at on/reset
    static const byte AllCall_Addr = Base_Addr + 0x08; // +0b1000 Programmable
    static const byte SUBADR1      = Base_Addr + 0x09; // +0b1001 Programmable Disabled at on/reset
    static const byte SUBADR2      = Base_Addr + 0x0A; // +0b1010 Programmable Disabled at on/reset
    static const byte Reset_Addr   = Base_Addr + 0x0B; // +0b1011
    static const byte SUBADR3      = Base_Addr + 0x0C; // +0b1100 Programmable Disabled at on/reset
    //                                         + 0x0D .. 0x0F  // Unassigned at on/reset

    // Auto-increment mode bits
    // default is Auto_none, same register can be written to multiple times
    static const byte Auto_All = 0b10000000; // 0..Control_Register_Max
    static const byte Auto_PWM = 0b10100000; // PWM0_Register .. +(Channels-1)
    static const byte Auto_GRP = 0b11000000; // GRPPWM..GRPFREQ
    static const byte Auto_PWM_GRP = 0b11100000; // PWM0_Register..n, GRPPWM..GRPFREQ
    // It may appear that a reset looks like Auto_PWM ^ PWMx_Register(3) = 0x5a. But Reset_addr is a special mode.

    // Control Registers
    static const byte Control_Register_Min = 0;
    static const byte Control_Register_Max = 0x1E; // NB, no 0x1F !
    static const byte MODE1_Register = 0x00;
    static const byte MODE1_OSC_mask = 0b10000;
    static const byte MODE2_Register = 0x01;
    static const byte PWM0_Register = 0x02;
    static const byte GRPPWM_Register = 0x12; // aka blink-duty-cycle-register
    static const byte GRPFREQ_Register = 0x13; // aka blink-length 0=~24hz, 255=~10sec
    static const byte LEDOUT0_Register = 0x14;
    static const byte SUBADR1_Register = 0x18;
    // for LEDx_Register, see Register_Led_State
    static const byte EFLAG1_Register = 0x1d;
    static const byte EFLAG2_Register = EFLAG1_Register + 1;

    static const byte LEDx_Max = 15; // 0..15


    static Scan& scan(void) { // convenince, same as TLC59116::Scan::scanner();
      return Scan::scanner();
      };

    // some tests
    static bool is_device_range_addr(byte address) { return address >= Base_Addr && address <= Max_Addr; } // inline
    static bool is_SUBADR(byte address) { // inline
      // does not take into account changing these programmable addresses, nor whether they are enabled
      return address == SUBADR1 || address == SUBADR2 || address == SUBADR3 ;
      }
    static bool is_single_device_addr(byte address) { // inline
      // Is a single device: not AllCall,Reset,or SUBADRx
      // does not take into account programmable addresses, nor whether they are enabled
      return is_device_range_addr(address) && address != Reset_Addr && address != AllCall_Addr && !is_SUBADR(address);
      }
    static bool is_control_register(byte register_num) { // inline
      return (register_num >= Control_Register_Min && register_num <= Control_Register_Max);
      }
    static byte is_valid_led(byte v) { return v <= LEDx_Max; };

    // utility
    static byte normalize_address(byte address) { 
      return 
        (address <= (Max_Addr-Base_Addr)) // shorthand: 0..15
        ? (Base_Addr+address) 
        : address
        ;
      }
    static byte set_with_mask(byte new_bits, byte mask, byte was) {
      byte to_change = mask & new_bits; // sanity
      byte unchanged = ~mask & was; // the bits of interest are 0 here
      byte new_value = unchanged ^ to_change;
      return new_value;
      }

    static byte LEDx_Register(byte led_num) {
      // 4 leds per register, 2 bits per led
      // See LEDx_pwm _gpwm _digital... below
      // registers are: {0x14, 0x15, 0x16, 0x17}; 
      return LEDOUT0_Register + (led_num >> 2); // int(led_num/4)
      }
    static byte PWMx_Register(byte led_num) { return PWM0_Register + led_num; }

    // LED register bits
    const static byte LEDOUT_PWM = 0b10;
    const static byte LEDOUT_GRPPWM = 0b11; // also "group blink" when mode2[dmblnk] = 1
    const static byte LEDOUT_DigitalOn = 0b01;
    const static byte LEDOUT_DigitalOff = 0b00;
    static byte LEDx_Register_mask(byte led_num) { return 0b11 << (2 * (led_num % 4)); }
    static byte LEDx_bits(byte led_num, byte register_value) {  return register_value & (0b11 << ((led_num % 4)* 2)); }
    static byte LEDx_mode_bits(byte led_num, byte out_mode) { return out_mode << (2 * (led_num % 4));} // 2 bits in a LEDx_Register
    static byte LEDx_pwm_bits(byte led_num) {return LEDx_mode_bits(led_num, LEDOUT_PWM);}
    static byte LEDx_gpwm_bits(byte led_num) {return LEDx_mode_bits(led_num, LEDOUT_GRPPWM);}
    static byte LEDx_digital_off_bits(byte led_num) { return 0; }; // no calc needed (cheating)
    static byte LEDx_digital_on_bits(byte led_num) {  return LEDx_mode_bits(led_num, LEDOUT_DigitalOn);}

    // Other register bits
    const static byte MODE2_DMBLNK = 0b100000;
    const static byte MODE2_EFCLR = 0x80; // '1' is clear.

    // Constructors (plus ::Each, ::Broadcast)
    // NB: The initial state is "outputs off"
    TLC59116() { reset_shadow_registers(); }; // Means first from scanner
    TLC59116(byte address) {
      this->_address = normalize_address(address);
      if (DEBUG) { 
        if (_address == Reset_Addr) {
          debug(F("You made a "));debug(Device);debug(F(" with the Reset address, "));
          debug(Reset_Addr,HEX);debug(F(": that's not going to work."));debug();
          }
        }
      reset_shadow_registers();
      }

    // reset all TLC59116's to power-up values. 
    // 0 means it worked, 2 means nobody there, others?
    // You probably want to .reset_shadow_registers() & .enable_outputs() on all objects!
    static int reset(); 

    TLC59116& describe();
    TLC59116& enable_outputs(bool yes = true); // enables w/o arg, use false for disable

    TLC59116& on(byte led_num, bool yes = true); // turns the led on, false turns it off
    TLC59116& pattern(word bit_pattern, word which_mask = 0xFFFF); // "bulk" on, by bitmask pattern, msb=15
    TLC59116& off(byte led_num) { return on(led_num, false); } // convenience
    TLC59116& pwm(byte led_num, byte value); 
    // timings give 1/2..2mitllis for bulk, vs. 1..6millis for one-at-time (proportional to ct)
    // Changing the LEDOUTx registers from digital to pwm seems to add 1/4millis for one-at-a-time
    //  and about 1/8 millis for bulk.
    // Seeing quite a bit of variance, first time is slowest (seems to be the "monitor" communications)
    TLC59116& pwm(byte led_num_start, byte ct, const byte* values); // bulk set
    TLC59116& pwm(const byte* values) { return pwm(0,16, values); }

    // Error detect bits, msb=15
    unsigned int error_detect(bool do_overtemp = false);
    unsigned int open_detect() { return error_detect(); } // convenience, 0=open, 1=loaded
    unsigned int overtemp_detect() { return error_detect(true); } // convenience. bitvalues: 0=overtemp, 1=normal
      // overtemp is not likely to last long

    // Blink the LEDs, at their current PWM setting
    TLC59116& group_blink(word bit_pattern, byte ratio, byte blink_time); // 0%..99.61%, 0=10sec..255=24hz
    TLC59116& group_blink(word bit_pattern, float on_percent, float hz) { // convenience % & hz
      // on_percent is 05..99.61%, hz is 24.00384hz to .09375 (10sec long) in steps of .04166hz (blink time 0..255)
      return group_blink((word)bit_pattern, (byte) int(on_percent * 256.0), 
      (byte) int( 24.0 / hz) - 1); 
      }
    TLC59116& group_blink(unsigned int bit_pattern, double on_percent, double hz) { // convenience: % & hz in default literal datatype
      return group_blink((word) bit_pattern, (float) on_percent, (float) hz); 
      }

    // Superpose group-pwm on current pwm setting (but, current should be 255 for best results)
    TLC59116& group_pwm(word bit_pattern, byte brightness);

    TLC59116& delay(int msec) { ::delay(msec); return *this;} // convenience

    TLC59116& reset_shadow_registers(); // reset to the power up values
    byte address(); // does a lazy init thing for "first"

    // Low level interface: Modifies only those bits of interest
    // Keeps a "shadow" of the control registers.
    // Assumes power-up-reset to start with, or call .update_shadows(). # FIXME
    // Each read via control_register(x) will update the shadow.
    void modify_control_register(byte register_num, byte mask, byte bits);
    void modify_control_register(byte register_num, byte value); // whole register

    // Low Level interface: writes bash the whole register
    byte control_register(byte register_num); // get. Failure returns 0, set DEBUG=1 and check monitor
    void control_register(byte register_num, byte data); // set
    void control_register(byte count, const byte *register_num, const byte data[]); // set a bunch FIXME not using...
    void get_control_registers(byte count, const byte *register_num, byte data[]); // get a bunch (more efficient)

    static void _begin_trans(byte addr, byte register_num) {
      Wire.beginTransmission(addr);
      Wire.write(register_num);
      }
    static void _begin_trans(byte addr, byte auto_mode, byte register_num) { // with auto-mode
      // debug(F("Trans starting at ")); debug(register_num,HEX); debug(F(" mode ")); debug(auto_mode,BIN); debug();
      _begin_trans(addr, auto_mode ^ register_num); 
      }

    static int _end_trans() {
      int stat = Wire.endTransmission();

      if (DEBUG) {
        if (stat != 0) {
          debug(F("endTransmission error, = "));debug(stat);debug();
          }
        }
      return stat;
      }

    byte shadow_registers[Control_Register_Max + 1];  // FIXME: dumping them, maybe a method?

  private:
    byte _address; // use the accessor (cf. lazy first)

    void describe_mode1();
    void describe_addresses();
    void describe_mode2();
    void describe_channels();
    void describe_iref();
    void describe_error_flag();

    void update_ledx_registers(byte addr, byte to_what, byte led_start_i, byte led_end_i);
    void update_ledx_registers(byte addr, const byte* want_ledx /* [4] */, byte led_start_i, byte led_end_i);
    void update_ledx_registers(byte addr, byte to_what, word bit_pattern);

  public:
    class Scan {
      public:

        // We only want one scanner
        static Scan& scanner() {static Scan instance; return instance;} // singleton. didn't bother with clone & =
        Scan& print(); 

        bool is_any() { return found > 0; }
        byte count() { return found; }
        const byte* device_addresses() { return addresses; } // FIXME: I want the contents to be const
        byte first_addr() { // debug message and 0xff if none 
          // return a good address or fall through
          for(byte i=0; i<found; i++) {
            if (is_single_device_addr(addresses[i])) return addresses[i]; 
            }

          if (DEBUG) {
            debug(F("Error: There is no first address for a "));debug(Device);debug();
            }
          return 0xff;
          }
        bool is_AllCall_default(); // is something at the default AllCall_Addr?

        // Find all the TLC59116 addresses, including broadcast(s)
        // If group-addresses are turned on, we might get collisions and count them as bad.
        // You should call TLC59116.reset() first (as appropriate),
        // Otherwise, this reads the current state.
        // Scan::scanner.print() is useful with this.
        Scan& rescan(); 

      private:
        Scan() {
          this->rescan();
          }

        byte found; // number of addresses found
        byte addresses[Max_Addr - Base_Addr - 2]; // reset/all are excluded, so only 14 possible
    };

    class Broadcast {
      byte dumy;
      };
    
    class Group1 {byte dumy1;};
    class Group2 {byte dumy1;};
    class Group3 {byte dumy1;};

    class Each { // Broadcast doesn't work for read?
      Scan *scanner; // just use the scanner list
      };
};


#endif
