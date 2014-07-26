#ifndef TLC59116_Unmanaged_h
#define TLC59116_Unmanaged_h

/*
  Pins:
    TLC59116: 27:SDA, 26:SCL
    UNO: A4:SDA, A5:SCL
    mega: as marked
    teensy: ...

  In your setup():
    Wire.begin();
    Serial.init(nnnnn); // if you do TLC59116_WARNINGS=1, or use any of the describes
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
  TLC59108 is supposedly the same except only 8 channels.
    8 output channels
    64 addresses : quad-value pins A0,A1,A2 (see datasheet)
      16..47,80..103,112..119

  TLC59208F : 4channel, similar, pin changes, up to 64 devices, etc.
  * Change to group_blink(ratio, hz, pwm values)
    low level is blink-bit-mask
    highlevel is leds-are:x blink/on-off/pwm/grppwm
  !! erratic flashes seen when using GRPPWM and decreasing it over time
      claimed to happen at same point every time (speed dependant I think)
  !! I see flickering when: many LEDs (14) are pwm on fairly dim (20), and I on/off blink another LED.
      adjustng Vext seems to improve the situation (so, current?)
  !! This stuff appears to work in a timer-service-routine, but you must (re-)enable interrupts in the isr: sei();

*/


// Set this to 1/true to turn on Warnings & info
#ifndef TLC59116_WARNINGS
  #define TLC59116_WARNINGS 0
#endif

// Set this to 1/true to turn on lower-level development debugging
#ifndef TLC59116_DEV
  #define TLC59116_DEV 0
#endif

// inline the Warn() function, when disabled, no code
#if TLC59116_WARNINGS
  template <typename T> void TLC59116Warn(T msg) {Serial.print(msg);}
  template <typename T> void TLC59116Warn(T msg, int format) { // and inlined
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
  void TLC59116Warn() { Serial.println();}
#else
  template <typename T> void TLC59116Warn(T msg, int format=0) {}
  void TLC59116Warn() {}
#endif

#include <Arduino.h>
#include <Serial.h>

// for .c
#include <Wire.h> // does nothing inthe .h
extern TwoWire Wire;

class TLC59116_Unmanaged {

  public:
    class Scan;

  public: // constants
    static const byte Channels = 16; // number of channels

    // Addresses (7 bits, as per Wire library)
    // 8 addresses are unassigned, +3 more if SUBADR's aren't used
    // these are defined by the device (datasheet)
    static const byte Base_Addr    = 0x60; // 0b110xxxx
    //                                         + 0x00  // Unassigned at on/reset
    //                                     ... + 0x07  // Unassigned at on/reset
    static const byte AllCall_Addr = Base_Addr + 0x08; // +0b1000 Programmable
    static const byte SUBADR1      = Base_Addr + 0x09; // +0b1001 Programmable Disabled at on/reset
    static const byte SUBADR2      = Base_Addr + 0x0A; // +0b1010 Programmable Disabled at on/reset
    static const byte Reset_Addr   = Base_Addr + 0x0B; // +0b1011
    static const byte SUBADR3      = Base_Addr + 0x0C; // +0b1100 Programmable Disabled at on/reset
    //                                         + 0x0D  // Unassigned at on/reset
    static const byte Max_Addr     = Base_Addr + 0xF;   // +0b1111, 14 typically available

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
    static const byte MODE1_SUB1_mask = 0b1000;
    static const byte MODE1_SUB2_mask = 0b100;
    static const byte MODE1_SUB3_mask = 0b10;
    static const byte MODE1_ALLCALL_mask = 0b1;
    static const byte MODE2_Register = 0x01;
    static const byte PWM0_Register = 0x02;
    static const byte GRPPWM_Register = 0x12; // aka blink-duty-cycle-register
    static const byte GRPFREQ_Register = 0x13; // aka blink-length 0=~24hz, 255=~10sec
    static const byte LEDOUT0_Register = 0x14;
    static const byte SUBADR1_Register = 0x18;
    static const byte IREF_Register = 0x1d;
    static const byte IREF_CM_mask = 1<<7;
    static const byte IREF_HC_mask = 1<<6;
    static const byte IREF_CC_mask = 0x1F; // 5 bits
    // for LEDx_Register, see Register_Led_State
    static const byte EFLAG1_Register = 0x1d;
    static const byte EFLAG2_Register = EFLAG1_Register + 1;
    static const byte LEDx_Max = 15; // 0..15

  public: // class
    static const char* Device;

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
        (address <= (Max_Addr-Base_Addr))  // 0..13 shorthand for 0x60..0x6D
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

    // fixme: move up
    // Constructors (plus ::Each, ::Broadcast)
    // NB: The initial state is "outputs off"
    TLC59116_Unmanaged(byte address) : i2c_address(address) {}
    TLC59116_Unmanaged(const TLC59116_Unmanaged&); // none
    TLC59116_Unmanaged& operator=(const TLC59116_Unmanaged&); // none
    ~TLC59116_Unmanaged() {} // managed destructor....


    TLC59116_Unmanaged& describe();
    TLC59116_Unmanaged& enable_outputs(bool yes = true); // enables w/o arg, use false for disable

    TLC59116_Unmanaged& on(byte led_num, bool yes = true); // turns the led on, false turns it off
    TLC59116_Unmanaged& pattern(word bit_pattern, word which_mask = 0xFFFF); // "bulk" on, by bitmask pattern, msb=15
    TLC59116_Unmanaged& off(byte led_num) { return on(led_num, false); } // convenience
    TLC59116_Unmanaged& pwm(byte led_num, byte value); 
    // timings give 1/2..2mitllis for bulk, vs. 1..6millis for one-at-time (proportional to ct)
    // Changing the LEDOUTx registers from digital to pwm seems to add 1/4millis for one-at-a-time
    //  and about 1/8 millis for bulk.
    // Seeing quite a bit of variance, first time is slowest (seems to be the "monitor" communications)
    TLC59116_Unmanaged& pwm(byte led_num_start, byte ct, const byte* values); // bulk set
    TLC59116_Unmanaged& pwm(const byte* values) { return pwm(0,16, values); }

    // Error detect bits, msb=15
    unsigned int error_detect(bool do_overtemp = false);
    unsigned int open_detect() { return error_detect(); } // convenience, 0=open, 1=loaded
    unsigned int overtemp_detect() { return error_detect(true); } // convenience. bitvalues: 0=overtemp, 1=normal
      // overtemp is not likely to last long

    // Blink the LEDs, at their current PWM setting
    TLC59116_Unmanaged& group_blink(word bit_pattern, byte ratio, byte blink_time); // 0%..99.61%, 0=10sec..255=24hz
    TLC59116_Unmanaged& group_blink(word bit_pattern, float on_percent, float hz) { // convenience % & hz
      // on_percent is 05..99.61%, hz is 24.00384hz to .09375 (10sec long) in steps of .04166hz (blink time 0..255)
      return group_blink((word)bit_pattern, (byte) int(on_percent * 256.0), 
      (byte) int( 24.0 / hz) - 1); 
      }
    TLC59116_Unmanaged& group_blink(unsigned int bit_pattern, double on_percent, double hz) { // convenience: % & hz in default literal datatype
      return group_blink((word) bit_pattern, (float) on_percent, (float) hz); 
      }

    // Superpose group-pwm on current pwm setting (but, current should be 255 for best results)
    TLC59116_Unmanaged& group_pwm(word bit_pattern, byte brightness);

    TLC59116_Unmanaged& delay(int msec) { ::delay(msec); return *this;} // convenience

    byte address() { return i2c_address; } 

    // Low level interface: Modifies only those bits of interest
    // Keeps a "shadow" of the control registers.
    // Assumes power-up-reset to start with, or call .update_shadows(). # FIXME
    // Each read via control_register(x) will update the shadow.
    void modify_control_register(byte register_num, byte mask, byte bits);
    void modify_control_register(byte register_num, byte value); // whole register

    // Low Level interface: writes bash the whole register
    byte control_register(byte register_num); // get. Failure returns 0, set TLC59116_WARNINGS=1 and check monitor
    void control_register(byte register_num, byte data); // set
    void control_register(byte count, const byte *register_num, const byte data[]); // set a bunch FIXME not using...
    void get_control_registers(byte count, const byte *register_num, byte data[]); // get a bunch (more efficient)

    static void _begin_trans(byte addr, byte register_num) {
      Wire.beginTransmission(addr);
      Wire.write(register_num);
      }
    static void _begin_trans(byte addr, byte auto_mode, byte register_num) { // with auto-mode
      // dev(F("Trans starting at ")); dev(register_num,HEX); dev(F(" mode ")); dev(auto_mode,BIN); dev();
      _begin_trans(addr, auto_mode ^ register_num); 
      }

    static int _end_trans() {
      int stat = Wire.endTransmission();

      if (stat != 0) {
        TLC59116Warn(F("endTransmission error, = "));TLC59116Warn(stat);TLC59116Warn();
        }
      return stat;
      }

    byte shadow_registers[Control_Register_Max + 1];  // FIXME: dumping them, maybe a method?

  private:
    TLC59116_Unmanaged(); // not allowed


    // our identity is the bus address (w/in a bus)
    // Addresses are 7 bits (lsb missing)
    byte i2c_address;

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

          TLC59116Warn(F("Error: There is no first address for a "));TLC59116Warn(Device);TLC59116Warn();

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
        byte addresses[14]; // reset/all are excluded, so only 16 possible
    };

    static Scan& scan(void) { // convenince, same as TLC59116::Scan::scanner();
      return Scan::scanner();
      };

    class Broadcast {
      byte dumy;
      };
    
    class Group1 {byte dumy1;};
    class Group2 {byte dumy1;};
    class Group3 {byte dumy1;};

};


#endif
