#ifndef TLC59116_Unmanaged_h
#define TLC59116_Unmanaged_h

/* \todo
  - group functions, and constants
*/

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


  ?? What does a read on AllCall get?
  TLC59108 is supposedly the same except only 8 channels.
    8 output channels
    64 addresses : quad-value pins A0,A1,A2 (see datasheet)
      16..47,80..103,112..119

  TLC59208F : 4channel, similar, pin changes, up to 64 devices, etc.

  Dandelion address jumpers:
    Don't use the "reserved", you can use "programmable reserved"
    A B C D
    0 0 0 0 = 0x60
    0 0 0 1 = 0x61
      ...
    0 1 1 1 = 0x67
    1 0 0 0 = 0x68 (AllCall, reserved)
    1 0 0 1 = 0x69 (SUBADR1, programmable reserved)
    1 0 1 0 = 0x6A (SUBADR2, programmable reserved)
    1 0 1 1 = 0x6B (RESET, reserved)
    1 1 0 0 = 0x6C (SUBADR3, programmable reserved)
    1 1 0 1 = 0x6D
    1 1 1 1 = 0x6E

  !! erratic flashes seen when using GRPPWM and decreasing it over time
      claimed to happen at same point every time (speed dependant I think)
  !! I see flickering when: many LEDs (14) are pwm on fairly dim (20), and I on/off blink another LED.
      adjustng Vext seems to improve the situation (so, current?)
  "As the value of GRPFREQ increases, the GRPPWM duty cycle dynamic range decreases" -- Karl M
  http://e2e.ti.com/support/power_management/led_driver/f/192/p/201430/1048227.aspx#1048227
  http://e2e.ti.com/support/power_management/led_driver/f/192/t/153172.aspx
  !! This stuff appears to work in a timer-service-routine, but you must (re-)enable interrupts in the isr: sei();

  Consider merging with https://github.com/jrowberg/i2cdevlib

  todo: post to arduino playground

*/

#include <Arduino.h>
#include <HardwareSerial.h>

// Set this to 1/true for lowlevel debugging messages
#ifndef TLC59116_LOWLEVEL
  #define TLC59116_LOWLEVEL 0
#endif

// Set this to 1/true to turn on lower-level development debugging
// Don't forget to Serial.begin(some-speed)
#ifndef TLC59116_DEV
  #if TLC59116_LOWLEVEL
    #define TLC59116_DEV 1
  #else
    #define TLC59116_DEV 0
  #endif
#endif

// Set this to 1/true to turn on Warnings & info
#ifndef TLC59116_WARNINGS
  #if TLC59116_DEV
    #define TLC59116_WARNINGS 1
  #else
    #define TLC59116_WARNINGS 0
  #endif
#endif

// inline the Warn() function, when disabled, no code
#if TLC59116_WARNINGS
  template <typename T> void inline TLC59116Warn(const T msg) {Serial.print(msg);}
  template <typename T> void inline TLC59116Warn(T msg, int format) { // and inlined
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
  void inline TLC59116Warn() { Serial.println();}
#else
  template <typename T> void inline TLC59116Warn(const T msg, int format=0) {}
  void inline TLC59116Warn() {}
#endif

#if TLC59116_DEV
  template <typename T> void inline TLC59116Dev(const T msg) {TLC59116Warn(msg);}
  template <typename T> void inline TLC59116Dev(T msg, int format) {TLC59116Warn(msg, format);}
  void inline TLC59116Dev() {TLC59116Warn();}
#else
  template <typename T> void inline TLC59116Dev(const T msg) {}
  template <typename T> void inline TLC59116Dev(T msg, int format) {}
  void inline TLC59116Dev() {}
#endif

#if TLC59116_LOWLEVEL
  template <typename T> void inline TLC59116LowLevel(T msg) {TLC59116Warn(msg);}
  template <typename T> void inline TLC59116LowLevel(T msg, int format) {TLC59116Warn(msg, format);}
  void inline TLC59116LowLevel() {TLC59116Warn();}
#else
  template <typename T> void inline TLC59116LowLevel(T msg) {}
  template <typename T> void inline TLC59116LowLevel(T msg, int format) {}
  void inline TLC59116LowLevel() {}
#endif

// for .c
#include <Wire.h> // does nothing in the .h
extern TwoWire Wire;

class TLC59116_Unmanaged {

  public:
    class Scan;

  public: // constants
    static const byte Channels = 16; // number of channels

    // Addresses (7 bits, as per Wire library)
    // these are defined by the device (datasheet)
    // 11 are Unassigned, 14 available, at power up; 1 more if allcall is disabled.
    static const byte Base_Addr    = 0x60; // 0b110xxxx
    //                                         + 0x00 .. 0x07 // Unassigned at on/reset
    static const byte AllCall_Addr = Base_Addr + 0x08; // +0b1000 Programmable Enabled at on/reset
    static const byte SUBADR1      = Base_Addr + 0x09; // +0b1001 Programmable Disabled at on/reset
    static const byte SUBADR2      = Base_Addr + 0x0A; // +0b1010 Programmable Disabled at on/reset
    static const byte Reset_Addr   = Base_Addr + 0x0B; // +0b1011
      // The byte(s) sent for Reset_Addr are not increment+address, it's a special mode
      static const word Reset_Bytes =  0xA55A;
    static const byte SUBADR3      = Base_Addr + 0x0C; // +0b1100 Programmable Disabled at on/reset
    //                                         + 0x0D .. 0x0F // Unassigned at on/reset
    static const byte Max_Addr     = Base_Addr + 0xF;   // 14 typically available
      static bool is_device_range_addr(byte address) { return address >= Base_Addr && address <= Max_Addr; } // inline

      // Does not take into account changed/enabled
      static bool is_default_SUBADR(byte address) { return address == SUBADR1 || address == SUBADR2 || address == SUBADR3; }
      // Is a single device: not AllCall,Reset,or SUBADRx
      // does not take into account programmable addresses, nor whether they are enabled
      static bool is_default_single_addr(byte address) {
        return is_device_range_addr(address) && address != Reset_Addr && address != AllCall_Addr && !is_default_SUBADR(address);
        }

  public: // Low-Level constants
    // Auto-increment mode bits
    // default is Auto_none, same register can be written to multiple times
    static const byte Auto_All = 0b10000000; // 0..Control_Register_Max
    static const byte Auto_PWM = 0b10100000; // PWM0_Register .. (Channels-1)
    static const byte Auto_GRP = 0b11000000; // GRPPWM..GRPFREQ
    static const byte Auto_PWM_GRP = 0b11100000; // PWM0_Register..n, GRPPWM..GRPFREQ

    // Control Registers FIXME: move to "low level"
    static const byte Control_Register_Min = 0;
    static const byte MODE1_Register = 0x00;
      static const byte MODE1_OSC_mask = 0b10000;
      bool is_OSC_bit(byte mode1_value) { return mode1_value & MODE1_OSC_mask; } // OSC is inverted for enable!
      static const byte MODE1_SUB1_mask = 0b1000;
      static const byte MODE1_SUB2_mask = 0b100;
      static const byte MODE1_SUB3_mask = 0b10;
      bool is_SUBADR_bit(byte mode1_value, byte which) { return mode1_value & (MODE1_SUB1_mask >> (which-1)); }
      static const byte MODE1_ALLCALL_mask = 0b1;
    static const byte MODE2_Register = 0x01;
      static const byte MODE2_DMBLNK = 1 << 5; // 0 = group dimming, 1 = group blinking
      static const byte MODE2_EFCLR = 1 << 7; // 0 to enable, 1 to clear
      static const byte MODE2_OCH = 1 << 3; // 0 = "latch" on Stop, 1 = latch on ACK
    static const byte PWM0_Register = 0x02;
      static byte PWMx_Register(byte led_num) { LEDOUTx_CHECK(__LINE__,led_num); return PWM0_Register + led_num; }
    static const byte GRPPWM_Register = 0x12; // aka blink-duty-cycle-register
    static const byte GRPFREQ_Register = 0x13; // aka blink-length 0=~24hz, 255=~10sec
    static const byte LEDOUT0_Register = 0x14; // len_num -> LEDOUTx_Register number. Registers are: {0x14, 0x15, 0x16, 0x17}
      static const byte LEDOUTx_Max = Channels-1; // 0..15
      const static byte LEDOUT_Mask = 0b11; // 2 bits per led 
      const static byte LEDOUT_PWM = 0b10;
      const static byte LEDOUT_GRPPWM = 0b11; // also "group blink" when mode2[dmblnk] = 1
      const static byte LEDOUT_DigitalOn = 0b01;
      const static byte LEDOUT_DigitalOff = 0b00;
      static byte LEDOUTx_Register(byte led_num) { return LEDOUT0_Register + (led_num >> 2); } // 4 leds per register
      static byte is_valid_led(byte v) { return v <= LEDOUTx_Max; };
      static void LEDOUTx_CHECK(int line, byte led_num) { 
        if (led_num > LEDOUTx_Max) {
          TLC59116Warn(F("led_num out of range "));TLC59116Warn(led_num); 
          TLC59116Warn(F(" @")); TLC59116Warn(line); TLC59116Warn();
          }
        }
      // FIXME: check for usage of these
      static byte LEDx_Register_mask(byte led_num) { // bit mask for led mode bits
        LEDOUTx_CHECK(__LINE__, led_num); 
        return LEDOUT_Mask << (2 * (led_num % 4)); 
        }
      static byte LEDx_Register_bits(byte led_num, byte register_value) { // extract bits for led
        LEDOUTx_CHECK(__LINE__, led_num);
        return register_value & LEDx_Register_mask(led_num); 
        }
      static byte LEDx_to_Register_bits(byte led_num, byte out_mode) { return (LEDOUT_Mask & out_mode) << (2 * (led_num % 4));} // 2 bits in a LEDOUTx_Register
      static byte LEDx_set_mode(byte was, byte led_num, byte mode) {
        LEDOUTx_CHECK(__LINE__, led_num);
        return set_with_mask(was, LEDx_Register_mask(led_num), LEDx_to_Register_bits(led_num, mode));
        }
      static void LEDx_set_mode(byte registers[] /*[4]*/, byte to_what, word which);

      static byte LEDx_pwm_bits(byte led_num) {return LEDx_to_Register_bits(led_num, LEDOUT_PWM);}
      static byte LEDx_gpwm_bits(byte led_num) {return LEDx_to_Register_bits(led_num, LEDOUT_GRPPWM);}
      static byte LEDx_digital_off_bits(byte led_num) { return 0; }; // no calc needed (cheating)
      static byte LEDx_digital_on_bits(byte led_num) {  return LEDx_to_Register_bits(led_num, LEDOUT_DigitalOn);}
    //                LEDOUT3_Register = 0x17; // cf. LEDOUTx_max. FIXME: maybe the list, maybe a max?
    static const byte SUBADR1_Register = 0x18;
      static byte SUBADRx_Register(byte i) { 
        if (i>3) {TLC59116Dev(F("SUBADRx out of range")); TLC59116Dev();} 
        return SUBADR1_Register - 1 + i; 
        }
    //                SUBADR3_Register = 0x1A; // FIXME: a _max, a list?
    static const byte AllCall_Addr_Register = 0x1B;
    static const byte IREF_Register = 0x1C;
      static const byte IREF_CM_mask = 1<<7;
      static const byte IREF_HC_mask = 1<<6;
      static const byte IREF_CC_mask = 0b111111; // 6 bits
      static const int Rext_Min = 156; // in ohms, gives 120ma at reset
      static byte best_iref(byte ma, int Rext=Rext_Min); // from milliamps. 937 for 20ma.
      static byte i_out(byte CM, byte HC, byte CC, int Rext=Rext_Min); // milliamps for register setting
      static byte i_out(byte iref_value, int Rext=Rext_Min) { 
        return i_out( iref_value >> 7 & 1, iref_value >> 6 & 1, iref_value & IREF_CC_mask, Rext ); 
        }
      static byte i_out_d(byte CM, byte HC, byte D, int Rext=Rext_Min); // D=CC with bits reversed
    static const byte EFLAG1_Register = 0x1D;
    static const byte EFLAG2_Register = EFLAG1_Register + 1;
    static const byte Control_Register_Max = 0x1E; // NB, no 0x1F !
      static bool is_control_register(byte register_num) {
        return (register_num >= Control_Register_Min && register_num <= Control_Register_Max);
        }

    static const char* Device; // holds "TLC59116" for convenience for debug messages

  public: // class

    // utility
    static byte normalize_address(byte address) { // return a Wire i2c address (7bit)
      if (address <= (Max_Addr - Base_Addr)) // 0..15
        return Base_Addr+address;
      else if (address >= Base_Addr && address <= Max_Addr) // Wire's I2C address: 0x60...
        return address;
      else if (address >= (Base_Addr<<1) && address <= (Max_Addr<<1)) // I2C protocol address: 0x60<<1
        return address >> 1;
      else
        return 0; // Not
      }
    static byte set_with_mask(byte was, byte mask, byte new_bits) { // only set the bits marked by mask
      byte to_change = mask & new_bits; // sanity
      byte unchanged = ~mask & was; // the bits of interest are 0 here
      byte new_value = unchanged | to_change;
      return new_value;
      }
    static void set_with_mask(byte *was, byte mask, byte new_bits) { *was = set_with_mask(*was, mask, new_bits); }

    static void describe_registers(byte* registers /*[Control_Register_Max]*/);

    static byte inline reverse_cc(byte CC) { // CC<->D (reverse 6 bits)
      // CC<->D (reverse 6 bits)
      byte D=0;
      for(byte i=0;i<6;i++) D |= ((CC>>i) & 0b1)<<(5-i);
      return D;
      }

  public: // instance
    TwoWire& i2cbus; // r/o

    // fixme: move up
    // Constructors (plus ::Each, ::Broadcast)
    // NB: The initial state is "outputs off"
    TLC59116_Unmanaged(TwoWire &bus, byte address) : i2cbus(bus), i2c_address(address) {}
    TLC59116_Unmanaged(const TLC59116_Unmanaged&); // none
    TLC59116_Unmanaged& operator=(const TLC59116_Unmanaged&); // none
    ~TLC59116_Unmanaged() {} // managed destructor....

    byte fetch_registers(byte start_r, byte count, byte registers[]); // returns 0 for success, I2C status otherwise
    TLC59116_Unmanaged& describe_actual();
    static void describe_group_mode(byte* registers); // for debugging

    // FIXME: implement error bits (reset, read).

    /// \name Info Functions
    ///@{
    byte address() { return i2c_address; } 
    ///@}

    // Low Level interface: writes bash the whole register
    byte control_register(byte register_num); // get. Failure returns 0, set TLC59116_WARNINGS=1 and check monitor
    void control_register(byte register_num, byte data); // set
    void control_register(byte count, const byte *register_num, const byte data[]); // set a bunch FIXME not using...
    void get_control_registers(byte count, const byte *register_num, byte data[]); // get a bunch (more efficient)

    void _begin_trans(byte register_num) { TLC59116_Unmanaged::_begin_trans(i2cbus,address(),register_num); }
    void _begin_trans(byte auto_mode, byte register_num) {
      TLC59116_Unmanaged::_begin_trans(i2cbus, address(), auto_mode, register_num);
      }

    static void _begin_trans(TwoWire& bus, byte addr, byte register_num) {
      TLC59116LowLevel(F("send R "));TLC59116LowLevel(register_num & 0x1f,HEX);TLC59116LowLevel(F("/"));TLC59116LowLevel((register_num & (byte)0xe0 >> 5),BIN);TLC59116LowLevel(F(" to "));TLC59116LowLevel(addr,HEX);TLC59116LowLevel(F(" on bus "));TLC59116LowLevel((unsigned long)&bus, HEX);TLC59116LowLevel();
      bus.beginTransmission(addr);
      bus.write(register_num);
      TLC59116LowLevel(F("  begin data..."));TLC59116LowLevel();
      }
    static void _begin_trans(TwoWire& bus, byte addr, byte auto_mode, byte register_num) { // with auto-mode
      _begin_trans(bus, addr, auto_mode ^ register_num); 
      }

    int _end_trans() { return TLC59116_Unmanaged::_end_trans(i2cbus); }

    static int _end_trans(TwoWire &bus) {
      TLC59116LowLevel(F("end_transmission..."));TLC59116LowLevel();
      int stat = bus.endTransmission();

      if (stat != 0) {
        TLC59116Warn(F("endTransmission error, = "));TLC59116Warn(stat);TLC59116Warn();
        }
      TLC59116LowLevel(F("  ="));TLC59116LowLevel(stat);TLC59116LowLevel();
      return stat;
      }

    byte shadow_registers[Control_Register_Max + 1];  // FIXME: dumping them, maybe a method?

  private:
    TLC59116_Unmanaged(); // not allowed


    // our identity is the bus address (w/in a bus)
    // Addresses are 7 bits (lsb missing)
    byte i2c_address;

    static void describe_iref(byte* registers);
    static void describe_addresses(byte* registers);
    static void describe_mode2(byte *registers);
    static void describe_outputs(byte* registers);
    static void describe_error_flag(byte* registers);

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
            if (is_default_single_addr(addresses[i])) return addresses[i]; 
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
        Scan& rescan();  // FIXME: not defined, steal back from TLC59116?

      private:
        Scan() {
          this->rescan();
          }

        byte found; // number of addresses found
        byte addresses[Max_Addr - Base_Addr - 2]; // reset/all are excluded, so only 16 possible
    };

    static Scan& scan(void) { // convenience, same as TLC59116_Unmanaged::Scan::scanner();
      return Scan::scanner();
      };

    class Broadcast { // FIXME: unused?
      byte dumy;
      };
    
};


#endif
