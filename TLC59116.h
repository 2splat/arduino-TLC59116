#ifndef TLC59116_h
#define TLC59116_h

/*
  In your setup():
    Wire.begin();
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
        if (format == HEX) Serial.print("0x"); // so tired
        Serial.print(msg, format); 
        }
      } 
    static void debug() { if (TLC59116::DEBUG) Serial.println();}

    static const char* Device; // "TLC59116" for printing

    // Addresses (7 bits, as per Wire library)
    // 8 addresses are unassigned, +3 more if SUBADR's aren't used
    // these are defined by the device (datasheet)
    static const byte Base_Addr    = 0x60; // 0b110xxxx
    static const byte Max_Addr     = Base_Addr + 13;   // +0b1101 +0x0D. 14 of 'em
    //                                         + 0x00  // Unassigned at on/reset
    //                                     ... + 0x07  // Unassigned at on/reset
    static const byte AllCall_Addr = Base_Addr + 0x08; // +0b1000 Programmable
    static const byte SUBADR1      = Base_Addr + 0x09; // +0b1001 Programmable Disabled at on/reset
    static const byte SUBADR2      = Base_Addr + 0x0A; // +0b1010 Programmable Disabled at on/reset
    static const byte Reset_Addr   = Base_Addr + 0x0B; // +0b1011
    static const byte SUBADR3      = Base_Addr + 0x0C; // +0b1100 Programmable Disabled at on/reset
    //                                         + 0x0D  // Unassigned at on/reset

    static const byte Control_Register_Min = 0;
    static const byte Control_Register_Max = 0x1E; // NB, no 0x1F !

    static boolean reset() {} // Resets all
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
      // Is a single device.
      // does not take into account programmable addresses, nor whether they are enabled
      return is_device_range_addr(address) && address != Reset_Addr && address != AllCall_Addr && !is_SUBADR(address);
      }
    static bool is_control_register(byte register_num) { // inline
      return (register_num >= Control_Register_Min && register_num <= Control_Register_Max);
      }
    static byte normalize_address(byte address) { return (address <= (Max_Addr-Base_Addr)) ? (Base_Addr+address) : address; }

    // Constructors (plus ::Each, ::Broadcast)
    // NB: The initial state is "outputs off"
    TLC59116() {}; // Means first from scanner
    TLC59116(byte address) {this->_address = normalize_address(address);}

    TLC59116& describe();

    byte address(); // does a lazy init thing for "first"

    // Low Level interface
    byte control_register(byte register_num); // get. Failure returns 0, set DEBUG=1 and check monitor
    void control_register(byte register_num, byte data); // set
    void get_control_registers(byte count, const byte *register_num, byte *data); // get a bunch (more efficient)
    void set_control_registers(byte count, const byte *register_num, const byte *data); // set a bunch

  private:
    byte _address; // use the accessor (cf. lazy first)
    void describe_mode1();
    void describe_mode2();
  
  public:
    class Scan {
      public:

        // We only want one scanner
        static Scan& scanner() {static Scan instance; return instance;} // singleton. didn't bother with clone & =
        Scan& print(); 

        bool is_any() { return found > 0; }
        byte first_addr() { // debug message and -1 if none 
          if (DEBUG) {
            if ( !is_any() ) { debug("Error: There is no first address for a ");debug(Device);debug(); }
            }
          return is_any() ? addresses[0] : -1; 
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
        byte addresses[14];
    };

    class Broadcast {
      byte dumy;
      };

    class Each { // Broadcast doesn't work for read?
      Scan *scanner; // just use the scanner list
      };
      
};


#endif
