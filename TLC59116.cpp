// FIXME: make things final

#include <TLC59116.h>
#include <Wire.h>

// The convenience method sometlc.delay(x) hides the normal delay. Use ::delay(...)

bool TLC59116::DEBUG = 0;
const char* TLC59116::Device = "TLC59116";


// NB. You must use the accessor address() to take care of lazy initialization of the address!
// (e.g., the "first" case)

inline void printf0(byte v) { // 3 digits
    Serial.print(v / 100);
    v = v % 100;
    Serial.print(v/10);
    v = v % 10;
    Serial.print(v);
    }

int TLC59116::reset() {
  Wire.beginTransmission(Reset_Addr);
    Wire.write(0xA5);
    Wire.write(0x5A);
  int rez = _end_trans();
  if (DEBUG) { 
    debug("Reset "); 
    if (rez)  { debug("Failed with "); debug(rez); } else { debug("worked!"); } 
    debug();
    }
  // does not "reset" the error_detect
  }

byte TLC59116::address() {
  // The plain constructor implies "first"
  // And we have to delay scanner() till after Wire.begin.
  if (this->_address == NULL) {
    debug("Lazy init for first");debug();
    this->_address = Scan::scanner().first_addr();
    }
  return this->_address;
  }

void TLC59116::describe_channels() {
    byte addr = address();
    
    // General/Group
    byte grppwm = control_register(0x12);
    byte mode1 = control_register(MODE1_Register);
    byte mode2 = control_register(MODE2_Register);
    bool is_blink = (mode2 && 0b10000); // DMBLNK

    // note: if MODE2[GRP/DMBLNK] then GRPPWM is PWM, GRPFREQ is blink 24Hz to .1hz
    // note: if !MODE2[GRP/DMBLNK] then GRPPWM is PWM, GRPFREQ ignored, and PWMx is * GRPPWM
    Serial.print("GRP[");
    Serial.print(is_blink ? "BlinkDuty" : "PWM"); // what does blinkduty do?
    Serial.print("]=");Serial.print(grppwm);Serial.print("/256 ");

    byte grpfreq = control_register(0x12);
    float blink_len = ((float)grpfreq + 1.0) / 24.0; // docs say max 10.73 secs, but math says 10.66 secs
    Serial.print("GRPFREQ=");
    // Aren't we nice:
    if (blink_len <= 1) {
      Serial.print(1.0/blink_len);Serial.print("Hz blink");
      }
    else {
      Serial.print(blink_len);Serial.print(" second blink");
      }
    Serial.print(is_blink ? "" : "(ignored):");

    Serial.println(" for outputs that are Gnnn");
    
    Serial.print("Outputs ");
    Serial.print( (mode1 & 0b10) ? "OFF" : "ON"); // nb. reverse, "OSC"
    Serial.println();
    Serial.print("        ");

    for(byte led_num=0; led_num < Channels; led_num++) {
        byte led_state = control_register(LEDx_Register(led_num));
        // The led state is groups of 4
        byte state_bits = (led_state >> ((led_num % 4) * 2)) & 0b11; // right shifted to lsb's location

        byte pwm_value;
        switch (state_bits) {

            // Digital
            case 0b00:
                Serial.print("D-  ");
                break;
            case 0b01:
                Serial.print("D+  ");
                break;

            // PWM
            case 0b10:
                pwm_value = control_register(PWM0_Register + led_num);
                printf0(pwm_value);
                Serial.print(" ");
                break;
            // GPWM
            case 0b11:
                Serial.print("G");
                printf0(pwm_value);
                break;
            }

        // Groups of 4 per line
        if ((led_num % 4) == 3) {
            Serial.println(); 
            if (led_num != 15) Serial.print("        ");
        } else {
            Serial.print(" ");
        }
       }
    }

void TLC59116::describe_addresses() {
  byte mode1 = control_register(MODE1_Register);
  byte the_addr;

  the_addr = control_register(0x1B);
  // Internal addresses are 7:1, with bit 0=r/w
  Serial.print(mode1 & 0b01 ? "+" : "-");
  Serial.print("AllCall(0x");Serial.print(the_addr >> 1,HEX);Serial.print(") ");

  for(byte i = 0; i<3; i++) {
      the_addr = control_register(SUBADR1_Register + i);
      // mode bit is 2,4,8 for enable
      Serial.print(mode1 & (2<<1) ? "+" : "-");
      Serial.print("SUBADR"); Serial.print(i+1); Serial.print("(0x");
      Serial.print(the_addr >> 1, HEX); Serial.print(") ");
      }
  Serial.println();
  }

void TLC59116::describe_iref() {
    byte value = control_register(0x1C);
    word current_multiplier = value & 0x1F; // 5 bits

    // FIXME: do the math, and explain
    Serial.print("IREF: ");
    Serial.print("CM=0x");Serial.print(current_multiplier,HEX);
    Serial.print(" HC=");Serial.print( (value & 0x40) ? "1" : "0");
    Serial.print( (value & 0x80) ? " Hi" : " Lo");
    Serial.println();
    }

void TLC59116::describe_error_flag() {
    byte mode2 = control_register(MODE2_Register);
    byte value;
    // format same as channels

    Serial.print("Errors: ");
    Serial.print((mode2 & 0x80) ? "clear" : "enabled"); // EFCLR
    Serial.println();
    Serial.print("        ");

    // 8 channels per register
    for(byte i=0;i<=1;i++) {
        value = control_register(EFLAG_Register + i);
        // Each bit is a channel
        for(byte bit_num=0; bit_num<=7; bit_num++) {
            Serial.print( (value & (1<<bit_num)) ? "1    " : "0    ");
            // format
            if (bit_num % 4 == 3 && !(bit_num==7 && i==1)) {Serial.println(); Serial.print("        ");}
            }
        }
    Serial.println();
    }

TLC59116& TLC59116::describe() {
  const byte addr = address(); // just less typing

  Serial.print("Describe ");
  Serial.print(Device);Serial.print(" 0x");Serial.print(addr,HEX);Serial.print("/");
  Serial.print(addr);Serial.print("(");Serial.print(addr-Base_Addr);Serial.print(")");
  Serial.println();

  byte *s = this->shadow_registers;
  byte *b = &this->bracket;
  Serial.print("Start   0x");Serial.println((unsigned long int)s,HEX);
  Serial.print("Bracket 0x");Serial.println((unsigned long int)b,HEX);

  // We could get all the registers at once, and pass each value into describe_x's
  // But, efficiency isn't exactly important here. (We Serial.print!)
  // Not in register # order, but...
  describe_iref();
  describe_mode1(); // FIXME: collapse into describe_addresses(), + osc=on/off & autoincrement mode
  describe_addresses();
  describe_mode2();
  describe_channels();
  describe_error_flag();

  return *this;
  }

void TLC59116::describe_mode1() {
    byte value = control_register(MODE1_Register);
    
    // Let describe_addresses() do the addr enable bits

    // Serial.print("MODE1");Serial.print(" 0x");Serial.print(value,HEX);Serial.print(" ");
    
    if (value & 0x80) {
        Serial.print(" +AutoIncrement ");
        switch (value & 0xE0) {
            case 0x80:
                break;
            case 0xA0:
                Serial.print("(PWM 0x02..0x11)");
                break;
            case 0x0C:
                Serial.print("(GRP-PWM/FREQ)");
            case 0xE0:
                Serial.print("(PWM+GRP 0x02..0x13)");
                break;
            }
    } else {
        Serial.print(" -AutoIncrement");
        }

    Serial.println();
    }

void TLC59116::describe_mode2() {
    byte value = control_register(MODE2_Register);
    
    // Serial.print("MODE2");Serial.print(" 0x");Serial.print(value,HEX);Serial.print(" ");
    Serial.print("Latch on ");Serial.print((value & 0b0100) ? "ACK" : "Stop"); // OCH
    Serial.println();
    }

void TLC59116::control_register(byte register_num, byte data) {
  // set
  if (!is_control_register(register_num)) {
    if (DEBUG) {
      debug("Not a ");debug(Device);debug(" control register #:0x");debug(register_num,HEX);
      debug(". Set to 0x");debug(data,HEX);debug("is ignored.");
      }
    return;
    }
    Wire.beginTransmission(address());
      Wire.write(register_num); 
      Wire.write(data);
    _end_trans();
  }

byte TLC59116::control_register(byte register_num) {
  // get
  if (!is_control_register(register_num)) {
    if (DEBUG) {
      debug("Not a ");debug(Device);debug(" control register #:0x");debug(register_num,HEX);
      debug(". Get is ignored.");
      }
    return 0; // is 0xFF better?
    }
    byte addr = address();

    Wire.beginTransmission(addr);
      Wire.write(register_num); 
    int stat = _end_trans();

    int data = 0; // is 0xFF to signal fail better?
    if (stat == 0) {
      int avail = Wire.requestFrom(addr,(byte)1); // not checking avail
      data = Wire.read();
      shadow_registers[addr] = data;
      }
    return data;
  }

void TLC59116::modify_control_register(byte register_num, byte value) {
  shadow_registers[register_num] = value;
  control_register(register_num, value);
  }

void TLC59116::modify_control_register(byte register_num, byte mask, byte bits) {
  byte to_change = mask & bits; // sanity
  byte unchanged = ~mask & shadow_registers[register_num]; // the bits of interest are 0 here
  byte new_value = unchanged ^ to_change;
  /* if (register_num < PWM0_Register || register_num > 0x17) {
    debug("Modify R");debug(register_num,HEX);debug();
    debug("       =");debug(shadow_registers[register_num],BIN);debug();
    debug("       M");debug(mask,BIN);debug();
    debug("       V");debug(bits,BIN);debug();
    debug("       =");debug(new_value,BIN);debug();
    }
  */
  shadow_registers[register_num] = new_value;
  control_register(register_num, new_value);
  }

TLC59116& TLC59116::enable_outputs(bool yes) {
  if (yes) {
    modify_control_register(MODE1_Register,MODE1_OSC_mask, 0x00); // bits off is osc on
    delayMicroseconds(500); // would be nice to be smarter about when to do this
    }
  else {
    modify_control_register(MODE1_Register,MODE1_OSC_mask, 0xFF); // bits on is osc off
    }

  return *this;
  }

TLC59116& TLC59116::on(byte led_num, bool yes) {
  // we don't try to optimize "don't send data if shadow is alredy in that state"

  if (is_valid_led(led_num)) {
    if (DEBUG == 2) {
      debug("LED set ");debug(_address,HEX);debug("[");debug(led_num);debug("] ");debug(yes ? "on" : "off");debug();
      debug("  ");
        debug("R ");debug(LEDx_Register(led_num),HEX);
        debug(" M ");debug(LEDx_Register_mask(led_num),HEX);
        debug(" V ");debug(yes ? LEDx_digital_on(led_num) : (byte) 0x0);
        debug();
      }

    modify_control_register(
      LEDx_Register(led_num),
      LEDx_Register_mask(led_num),
      yes ? LEDx_digital_on(led_num) : (byte) 0x0
      );
    }
  else {
    debug("Bad led_num ");debug(led_num);debug(" on device ");debug(_address,HEX);debug();
    }
  return *this;
  }

TLC59116& TLC59116::g_pwm(byte led_num, byte value) {

  // put us into pwm mode if we aren't
  byte led_register = LEDx_Register(led_num);
  byte mode_bits = LEDx_bits(led_num, shadow_registers[led_register]);

  if (! (mode_bits == LEDx_pwm(led_num) || mode_bits == LEDx_gpwm(led_num))) {
    // debug("Switch led to pwm: ");debug(led_num);debug();
    modify_control_register(
      led_register,
      LEDx_Register_mask(led_num),
      LEDx_pwm(led_num)
      );
    }

  // we don't try to short-circuit the write based on shadow-registers[]
  // debug("Set ");debug(led_num);debug(" to pwm ");debug(value);debug();
  modify_control_register(PWMx_Register(led_num), value);
  }

TLC59116& TLC59116::reset_shadow_registers() {
  static const byte default_values[] = {
    // as per datasheet 
    0b00010001, // mode1: osc-off & all_call on
    0b00000000, // mode2
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // PWM
    0xff, // GRPPWM
    0x00, // GRPFREQ
    0,0,0,0, // LEDx (digital off)
    (SUBADR1 << 1), (SUBADR2 << 1), (SUBADR2 << 1), // msb is r/w bit
    (AllCall_Addr << 1),
    0xFF // IREF
    };

  // NB: called by constructor, so Serial and Wire are not init'd yet!

  memcpy(this->shadow_registers, default_values, Control_Register_Max + 1);
  return *this;
  }
//
// Scan
//

TLC59116::Scan& TLC59116::Scan::rescan() {
  // this code lifted & adapted from Nick Gammon (written 20th April 2011)
  // http://www.gammon.com.au/forum/?id=10896&reply=6#reply6
  // Thanks Nick!

  debug("I2C scanner. Scanning ...");

  byte debug_tried = 0;

  for (byte addr = Base_Addr; addr <= Max_Addr; addr++)
  {
    debug_tried++;
    debug("Try ");debug(addr,HEX);

    // If we hang here, then you did not do a Wire.begin();

    // yup, just "ping"
    Wire.beginTransmission(addr);
    int stat = Wire.endTransmission(); // the trick is: 0 means "something there"

    if (stat == 0) {
      this->addresses[ this->found++ ] = addr;
      debug(" got ");debug(addr,HEX);

      ::delay(10);  // maybe unneeded?
      } // end of good response
    else if (stat != 2) {
      debug(" Unexpected stat("); debug(stat); debug(") at address "); debug(addr,HEX);
      }

    debug();

  } // end of for loop
  debug("Checked ");debug(debug_tried);
  debug(" out of ");debug(Base_Addr,HEX); debug(".."); debug(Max_Addr,HEX);
  debug();
  
  if (this->found) {
      debug("Found ");
      debug(this->found);
      debug(" ");debug(Device);debug(" address(es) that responded.");
      debug();
    }
  else {
    debug("None found!");
    }

  return *this;
}

TLC59116::Scan& TLC59116::Scan::print() {
  bool has_reset = 0;

  Serial.print("Scanned 0x");
  Serial.print(Base_Addr,HEX); Serial.print("(0)..0x"); Serial.print(Max_Addr,HEX);
  Serial.print("(");Serial.print(Max_Addr-Base_Addr);Serial.print("):");
  Serial.println();

  for (byte i=0; i<this->found; i++) {
    byte addr = addresses[i];
    Serial.print(Device);Serial.print(" at 0x");
    Serial.print(addr,HEX);
    Serial.print("/");Serial.print(addr);
    Serial.print("(");Serial.print(addr-Base_Addr);Serial.print(")");
    if (addr == Reset_Addr) { Serial.print(" Reset_Addr"); }
    if (addr == AllCall_Addr) { Serial.print(" AllCall_Addr"); }
    if (addr == SUBADR1) { Serial.print(" SUBADR1"); }
    if (addr == SUBADR2) { Serial.print(" SUBADR2"); }
    if (addr == SUBADR3) { Serial.print(" SUBADR3"); }

    Serial.println();

    }
  return *this;
  }

bool TLC59116::Scan::is_AllCall_default() {
  for(byte i=0; i<found; i++) {
    if (addresses[i] == AllCall_Addr) {return 1;}
    }
  return 0;
  }

