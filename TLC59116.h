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

    static bool DEBUG; // default to 0 I think
    template <typename T> static void debug(T msg) { if (TLC59116::DEBUG) Serial.print(msg);} // and inlined
    template <typename T> static void debug(T msg, int format) { // and inlined
      if (TLC59116::DEBUG) {
        if (format == HEX) Serial.print("0x"); // so tired
        Serial.print(msg, format); 
        }
      } 
    static void debug() { if (TLC59116::DEBUG) Serial.println();}

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

    class Scan {
      public:

        // We only want one
        static Scan& scanner() {static Scan instance; return instance;} // singleton. didn't bother with clone & =
        Scan& print() {
          bool has_reset = 0;
          for (byte i=0; i<this->found; i++) {
            byte addr = addreses[i];
            pp("Device at 0x");
            pp(addr,HEX);
            pp("/");pp(addr);
            pp("(");pp(addr-Base_Addr);pp(")");
            if (addr == Reset_Addr) {
              pp(" Reset_Addr");
              }
            
          // addresses[0..found] 0xaddr/dec +0x/+dec attributes Reset_Addr SUBADRx AllCall_Addr
          // note: reset is avail
          }
          }

        byte is_any() { return found > 0; }
        byte first_addr() { return found > 0 ? addresses[0] : -1; }
        bool is_reset_available() {
          // ...
          }
        bool is_AllCall_default() { // is something at the default AllCall_Addr?
          }

        Scan& rescan() {
          // Find all the TLC59116 addresses, including broadcast(s)
          // If group-addresses are turned on, we might get collisions and count them as bad.

          // You should call TLC59116.reset() first (as appropriate),
          // Otherwise, this reads the current state.

          // See the field list for interesting attributes.

          // this code lifted & adapted from Nick Gammon (written 20th April 2011)
          // http://www.gammon.com.au/forum/?id=10896&reply=6#reply6
          // Thanks Nick!

          debug("I2C scanner. Scanning ...");

          byte debug_tried = 0;

          for (byte addr = Base_Addr; addr <= Max_Addr; addr++)
          {
            debug_tried++;
            debug("Try ");debug(addr,HEX);debug();

            // yup, just "ping"
            Wire.beginTransmission(addr);
            int stat = Wire.endTransmission(); // the trick is: 0 means "something there"

            if (stat == 0) {
              this->addresses[ this->found++ ] = addr;
              debug("got ");debug(addr,HEX);debug();

              delay(10);  // maybe unneeded?
              } // end of good response
            else if (stat != 2) {
              debug("Unexpected stat("); debug(stat); debug(") at address "); debug(addr,HEX); debug();
              }

          } // end of for loop
          debug("Checked ");debug(debug_tried);
          debug(" out of ");debug(Base_Addr,HEX); debug(Max_Addr,HEX);
          debug();
          
          if (this->found) {
              debug("Found ");
              debug(this->found);
              debug(" address(es) that responded.");
              debug();
            }
          else {
            debug("None found!");
            }
        }

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

    class First;

    class All {
      byte addresses[14];
      };
      

    static boolean reset() {}
    static Scan& scan(void) { // convenince, same as TLC59116::Scan::scanner();
      return Scan::scanner();
      };
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

    TLC59116() {};
    TLC59116(byte address) {};

  private:
    byte address;
};

class TLC59116::First : public TLC59116 {
  public:
  First() {};
      };


#endif
