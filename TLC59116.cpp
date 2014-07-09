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
  // It may appear that a reset looks like Auto_PWM ^ PWMx_Register(3) = 0x5a. But Reset_addr is a special mode.
  Wire.beginTransmission(Reset_Addr);
    Wire.write(0xA5);
    Wire.write(0x5A);
  int rez = _end_trans();

  if (DEBUG) { 
    debug(F("Reset ")); 
    if (rez)  { debug(F("Failed with ")); debug(rez); } else { debug(F("worked!")); } 
    debug();
    }
  // does not "reset" the error_detect
  return rez;
  }

byte TLC59116::address() {
  // The plain constructor implies "first"
  // And we have to delay scanner() till after Wire.begin.
  if (this->_address == NULL) {
    debug(F("Lazy init for first"));debug();
    this->_address = Scan::scanner().first_addr();
    }
  return this->_address;
  }

void TLC59116::describe_channels() {
    byte addr = address();
    
    // General/Group
    byte grppwm = control_register(GRPPWM_Register);
    byte mode1 = control_register(MODE1_Register);
    byte mode2 = control_register(MODE2_Register);
    bool is_blink = (mode2 && MODE2_DMBLNK);

    // note: if MODE2[GRP/DMBLNK] then GRPPWM is PWM, GRPFREQ is blink 24Hz to .1hz
    // note: if !MODE2[GRP/DMBLNK] then GRPPWM is PWM, GRPFREQ ignored, and PWMx is * GRPPWM
    Serial.print(F("GRP["));
    Serial.print(is_blink ? "Blink" : "PWM*");
    Serial.print(F("]="));Serial.print(grppwm);Serial.print(F("/256 on ratio "));

    byte grpfreq = control_register(GRPFREQ_Register);
    float blink_len = ((float)grpfreq + 1.0) / 24.0; // docs say max 10.73 secs, but math says 10.66 secs
    Serial.print(F("GRPFREQ="));
    // Aren't we nice:
    if (blink_len <= 1) {
      Serial.print(1.0/blink_len);Serial.print(F("Hz blink"));
      }
    else {
      Serial.print(blink_len);Serial.print(F(" second blink"));
      }
    Serial.print(is_blink ? "" : "(ignored):");

    Serial.print(F(" for outputs that are "));Serial.print(is_blink ? "B" : "G"); Serial.println(F("nnn"));
    
    Serial.print(F("Outputs "));
    Serial.print( (mode1 & 0b10) ? "OFF" : "ON"); // nb. reverse, "OSC"
    Serial.println();
    Serial.print(F("        "));

    for(byte led_num=0; led_num < Channels; led_num++) {
        byte led_state = control_register(LEDx_Register(led_num));
        // The led state is groups of 4
        byte state_bits = (led_state >> ((led_num % 4) * 2)) & 0b11; // right shifted to lsb's location

        byte pwm_value;
        switch (state_bits) {

            // Digital
            case LEDOUT_DigitalOff:
                Serial.print(F("D-  "));
                break;
            case LEDOUT_DigitalOn:
                Serial.print(F("D+  "));
                break;

            // PWM
            case LEDOUT_PWM:
                pwm_value = control_register(PWM0_Register + led_num);
                printf0(pwm_value);
                Serial.print(" ");
                break;
            // GPWM
            case LEDOUT_GRPPWM:
                if (is_blink) {
                  pwm_value = control_register(PWM0_Register + led_num);
                  Serial.print("B");printf0(pwm_value);
                  }
                else {
                  Serial.print("G???"); // FIXME: calculate the pwm
                  }
                break;
            }

        // Groups of 4 per line
        if ((led_num % 4) == 3) {
            Serial.print(F(" via "));Serial.print(led_state,BIN);Serial.println(); 
            if (led_num != 15) Serial.print(F("        "));
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
  Serial.print(F("AllCall(0x"));Serial.print(the_addr >> 1,HEX);Serial.print(") ");

  for(byte i = 0; i<3; i++) {
      the_addr = control_register(SUBADR1_Register + i);
      // mode bit is 2,4,8 for enable
      Serial.print(mode1 & (2<<1) ? "+" : "-");
      Serial.print(F("SUBADR")); Serial.print(i+1); Serial.print("(0x");
      Serial.print(the_addr >> 1, HEX); Serial.print(") ");
      }
  Serial.println();
  }

void TLC59116::describe_iref() {
    byte value = control_register(0x1C);
    word current_multiplier = value & 0x1F; // 5 bits

    // FIXME: do the math, and explain
    Serial.print(F("IREF: "));
    Serial.print(F("CM=0x"));Serial.print(current_multiplier,HEX);
    Serial.print(F(" HC="));Serial.print( (value & 0x40) ? "1" : "0");
    Serial.print( (value & 0x80) ? " Hi" : " Lo");
    Serial.println();
    }

unsigned int TLC59116::error_detect(bool do_overtemp) {
  // reset over-temp
  control_register(MODE2_Register, 
    set_with_mask(MODE2_EFCLR, MODE2_EFCLR, shadow_registers[MODE2_Register])
    );
  if (do_overtemp) {
    // enable temp detection
    control_register(MODE2_Register, 
      set_with_mask(0, MODE2_EFCLR, shadow_registers[MODE2_Register])
      );
    }
  pattern(0xFFFF);
  unsigned int eflags = control_register(EFLAG1_Register);
  eflags ^= control_register(EFLAG2_Register) << 8;
  return eflags;
  }


void TLC59116::describe_error_flag() {
    byte mode2 = control_register(MODE2_Register);
    byte value;
    // format same as channels

    Serial.print(F("Errors: "));
    Serial.print((mode2 & MODE2_EFCLR) ? "clear" : "enabled"); // EFCLR
    Serial.println();
    Serial.print(F("        "));

    // 8 channels per register
    for(byte i=0;i<=1;i++) {
        value = control_register(EFLAG1_Register + i);
        // Each bit is a channel
        for(byte bit_num=0; bit_num<=7; bit_num++) {
            Serial.print( (value & (1<<bit_num)) ? "1    " : "0    ");
            // format
            if (bit_num % 4 == 3 && !(bit_num==7 && i==1)) {Serial.println(); Serial.print(F("        "));}
            }
        }
    Serial.println();
    }

TLC59116& TLC59116::describe() {
  const byte addr = address(); // just less typing

  Serial.print(F("Describe "));
  Serial.print(Device);Serial.print(" 0x");Serial.print(addr,HEX);Serial.print("/");
  Serial.print(addr);Serial.print("(");Serial.print(addr-Base_Addr);Serial.print(")");
  Serial.println();

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
        Serial.print(F(" +AutoIncrement "));
        switch (value & 0xE0) {
            case 0x80:
                break;
            case 0xA0:
                Serial.print(F("(PWM 0x02..0x11)"));
                break;
            case 0x0C:
                Serial.print(F("(GRP-PWM/FREQ)"));
            case 0xE0:
                Serial.print(F("(PWM+GRP 0x02..0x13)"));
                break;
            }
    } else {
        Serial.print(F(" -AutoIncrement"));
        }

    Serial.println();
    }

void TLC59116::describe_mode2() {
    byte value = control_register(MODE2_Register);
    
    // Serial.print("MODE2");Serial.print(" 0x");Serial.print(value,HEX);Serial.print(" ");
    Serial.print(F("Latch on "));Serial.print((value & 0b0100) ? F("ACK") : F("Stop")); // OCH
    Serial.println();
    }

void TLC59116::control_register(byte register_num, byte data) {
  // set
  if (!is_control_register(register_num)) {
    if (DEBUG) {
      debug(F("Not a "));debug(Device);debug(F(" control register #:0x"));debug(register_num,HEX);
      debug(F(". Set to 0x"));debug(data,HEX);debug(F("is ignored."));
      }
    return;
    }
    // debug(F("set R "));debug(register_num,HEX);debug("=");debug(data,BIN);debug();
    _begin_trans(this->address(), register_num); 
      Wire.write(data);
    _end_trans();
  }

byte TLC59116::control_register(byte register_num) {
  // get
  // Does "work" for allcall: for 1 device, you get the value you expect
  if (!is_control_register(register_num)) {
    if (DEBUG) {
      debug(F("Not a "));debug(Device);debug(F(" control register #:"));debug(register_num,HEX);
      debug(F(". Get is ignored."));
      }
    return 0; // is 0xFF better?
    }
    byte addr = address();

    _begin_trans(addr, register_num); 
    int stat = _end_trans();

    int data = 0; // is 0xFF to signal fail better?
    if (stat == 0) {
      int avail = Wire.requestFrom(addr,(byte)1); // not checking avail
      if (DEBUG) {
        if (avail > 1 || avail==0) { debug(F("Expected 1 byte, saw "));debug(avail);debug(); }
        }
      data = Wire.read();
      shadow_registers[addr] = data;
      }
    return data;
  }

void TLC59116::modify_control_register(byte register_num, byte value) {
  shadow_registers[register_num] = value;
  // debug("CR...");debug();
  control_register(register_num, value);
  }

void TLC59116::modify_control_register(byte register_num, byte mask, byte bits) {
  byte new_value = set_with_mask(bits, mask, shadow_registers[register_num]);
  /* if (register_num < PWM0_Register || register_num > 0x17) {
    debug(F("Modify R"));debug(register_num,HEX);debug();
    debug(F("       ="));debug(shadow_registers[register_num],BIN);debug();
    debug(F("       M"));debug(mask,BIN);debug();
    debug(F("       V"));debug(bits,BIN);debug();
    debug(F("       ="));debug(new_value,BIN);debug();
    } */
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

TLC59116& TLC59116::pattern(word bit_pattern, word which_mask) {
  byte addr = this->address(); // force lazy

  byte new_ledx[4];
  // need all for later comparison
  memcpy(new_ledx, &(this->shadow_registers[LEDOUT0_Register]), 4);

  // count through LED nums, starting from max

  for(byte ledx_r=15; ; ledx_r--) {
    if (0x8000 & which_mask) {
      new_ledx[ledx_r / 4] = set_with_mask(
        LEDx_mode_bits(ledx_r, (bit_pattern & 0x8000) ? LEDOUT_DigitalOn : LEDOUT_DigitalOff),
        LEDx_Register_mask(ledx_r),
        new_ledx[ledx_r / 4]
        );
      }
    bit_pattern <<= 1;
    which_mask <<= 1;

    if (ledx_r==0) break;
    }

  update_ledx_registers(addr, new_ledx, 0, 15);
  }

void TLC59116::update_ledx_registers(byte addr, byte to_what, word bit_pattern) {

  byte new_ledx[4];
  // need all for later comparison
  memcpy(new_ledx, &(this->shadow_registers[LEDOUT0_Register]), 4);
  // debug(F("Change from ")); for(byte i=0;i<4;i++){ debug(new_ledx[i],BIN); debug(" ");} debug();

  // count through LED nums, starting from max

  for(byte ledx_r=15; ; ledx_r--) {
    if (0x8000 & bit_pattern) {
      new_ledx[ledx_r / 4] = set_with_mask(
        LEDx_mode_bits(ledx_r, to_what),
        LEDx_Register_mask(ledx_r),
        new_ledx[ledx_r / 4]
        );
      }
    bit_pattern <<= 1;

    if (ledx_r==0) break;
    }
  // debug(F("Change to   ")); for(byte i=0;i<4;i++){ debug(new_ledx[i],BIN); debug(" ");} debug();

  update_ledx_registers(addr, new_ledx, 0, 15);
  }

TLC59116& TLC59116::on(byte led_num, bool yes) {
  // we don't try to optimize "don't send data if shadow is already in that state"

  if (is_valid_led(led_num)) {
    if (DEBUG == 2) {
      debug(F("LED set "));debug(_address,HEX);debug("[");debug(led_num);debug("] ");debug(yes ? "on" : "off");debug();
      debug("  ");
        debug("R ");debug(LEDx_Register(led_num),HEX);
        debug(" M ");debug(LEDx_Register_mask(led_num),HEX);
        debug(" V ");debug(yes ? LEDx_digital_on_bits(led_num) : LEDx_digital_off_bits(led_num));
        debug();
      }

    modify_control_register(
      LEDx_Register(led_num),
      LEDx_Register_mask(led_num),
      yes ? LEDx_digital_on_bits(led_num) : LEDx_digital_off_bits(led_num)
      );
    }
  else {
    debug(F("Bad led_num "));debug(led_num);debug(F(" on device "));debug(_address,HEX);debug();
    }
  return *this;
  }

TLC59116& TLC59116::pwm(byte led_num, byte value) {
  // put us into pwm mode if we aren't
  byte led_register = LEDx_Register(led_num);
  byte mode_bits = LEDx_bits(led_num, shadow_registers[led_register]);

  if (! (mode_bits == LEDx_pwm_bits(led_num) || mode_bits == LEDx_gpwm_bits(led_num))) {
    // debug(F("Switch led to pwm: "));debug(led_num); debug(" register ");debug(led_register, HEX);debug();
    modify_control_register(
      led_register,
      LEDx_Register_mask(led_num),
      LEDx_pwm_bits(led_num)
      );
    }

  // we don't try to short-circuit the write based on shadow-registers[]
  // debug("Set ");debug(led_num);debug(" to pwm ");debug(value);debug();
  modify_control_register(PWMx_Register(led_num), value);
  return *this;
  }

TLC59116& TLC59116::group_blink(word bit_pattern, byte ratio, byte blink_time) {
  // this blinks the current pwm mode
  // debug(F("DMBLNK mode"));
  // FIXME: more efficient? Auto_GRP
  modify_control_register(MODE2_Register, MODE2_DMBLNK, MODE2_DMBLNK);
  // debug(F("Set some to grpblnk ratio "));debug(ratio);debug(F(" len "));debug(blink_time);debug();
  modify_control_register(GRPPWM_Register, ratio);
  modify_control_register(GRPFREQ_Register, blink_time);
  update_ledx_registers(address(), LEDOUT_GRPPWM /* aka blink too */, bit_pattern);
  return *this;
  }

TLC59116& TLC59116::group_pwm(word bit_pattern, byte brightness) {
  modify_control_register(MODE2_Register, 0, MODE2_DMBLNK);
  modify_control_register(GRPPWM_Register, brightness);
  // GRPFREQ not actually "don't care", effectively an anti-range for brightness
  // range = (256/(n+1))
  modify_control_register(GRPFREQ_Register, 0x00);
  update_ledx_registers(address(), LEDOUT_GRPPWM /* aka blink too */, bit_pattern);
  }

void TLC59116::update_ledx_registers(byte addr, byte to_what, byte led_start_i, byte led_end_i) {
  // Update the LEDx registers to PWM or GRPPWM, or even ON or OFF
  // But, all the same
  // For setting a pattern use the ...(addr, want_ledx[4],...) variant below
  
  byte new_ledx[4];
  // need all for later comparison
  memcpy(new_ledx, &(this->shadow_registers[LEDOUT0_Register]), 4);

  byte ledx_i;
  // new settings, ignoring < start > end
  for( byte doing_led_i = led_start_i; doing_led_i <= led_end_i; doing_led_i++) {
    // ledx_i to track led#
    ledx_i = LEDx_Register(doing_led_i) - LEDOUT0_Register;

    // set the LEDOUTx bits for the led
    new_ledx[ledx_i] = set_with_mask(
      LEDx_mode_bits(doing_led_i, to_what),
      LEDx_Register_mask(doing_led_i),
      new_ledx[ledx_i]
      );
    }

  update_ledx_registers(addr, new_ledx, led_start_i, led_end_i);
  }

void TLC59116::update_ledx_registers(byte addr, const byte* new_ledx /* [4] */, byte led_start_i, byte led_end_i) {
  // Update only the LEDx registers that need it
  // new_ledx is shadow_registers & new settings
  // Does wire.begin, write,...,end
  // (_i = index of a led 0..15, _r is register_number)
  const byte ledx_first_r = LEDx_Register(led_start_i);
  const byte ledx_last_r = LEDx_Register(led_end_i);
  //debug(F("Update control from #"));
    //debug(led_start_i);debug(F("/C"));debug(ledx_first_r,HEX);
    //debug(F(" to "));debug(led_end_i);debug(F("/C"));debug(ledx_last_r,HEX);
    //debug();

  //debug(F("Change to ")); for(byte i=0;i<4;i++){ debug(new_ledx[i],BIN); debug(" ");} debug();

  // "new" is updated, now find the changed ones
  // (all the following work to write only changed bytes instead of 4. 
  //  best case is: no writes
  //  2nd best case is: begin, device addr, register, 1 byte, end
  //  instead of  : begin, device addr, register, byte byte byte byte, end
  //  which is a _possible_ savings of 3-bytes/24-bits out of 56-bits = 42%
  //  so, pretty good
  // )
  byte change_first_r; // an LEDOUTx register num
  bool has_change_first = false;

  for (byte r = ledx_first_r; r <= ledx_last_r; r++) {
    if (new_ledx[r - LEDOUT0_Register] != shadow_registers[r]) {
      change_first_r = r;
      has_change_first = true;
      break; // done if we found one
      }
    }
  // if (!has_change_first) {debug(F("No Diff"));debug();}

  // Write the data if any changed

  if (has_change_first) { // might be "none changed"
    //// DEBUG
    //debug("Diff F... ");
    //for(byte r = LEDOUT0_Register; r < change_first_r; r++) debug("           ");
    //for(byte r = change_first_r; r < 4 + LEDOUT0_Register; r++) { debug(new_ledx[r - LEDOUT0_Register],BIN);debug(" "); }
    //debug();
    //// DEBUG
  

    byte change_last_r;

    // Find last
    for(byte r = ledx_last_r; ; r--) { // we know we'll hit change_first_r
      if (new_ledx[r - LEDOUT0_Register] != shadow_registers[r]) {
        change_last_r = r;
        break; // done if we found one. it might be change_first_r
        }
      }
    //// DEBUG
    //debug("Diff      ");
    //for(byte r = LEDOUT0_Register; r < change_first_r; r++) debug("           ");
    //for(byte r = change_first_r; r <= change_last_r; r++) { debug(new_ledx[r - LEDOUT0_Register],BIN);debug(" "); }
    //debug();
    //// DEBUG

    // We have a first..last, so write them
    // debug(F("Write @ register ")); debug(change_first_r,HEX); debug(" "); debug(change_last_r - change_first_r +1); debug(F(" bytes of data")); debug();
    _begin_trans(addr, Auto_All, change_first_r);
      // debug("  ");
      Wire.write(&new_ledx[ change_first_r - LEDOUT0_Register ], change_last_r-change_first_r+1);
      for(; change_first_r <= change_last_r; change_first_r++) {
        byte new_val = new_ledx[ change_first_r - LEDOUT0_Register ];
        shadow_registers[ change_first_r ] = new_val;
        // debug(new_val,BIN);debug(" ");
        }
      //debug();
    _end_trans();
    }
  }

TLC59116& TLC59116::pwm(byte led_num_start, byte ct, const byte values[]) {
  // deal with start+ct wrapping past 15
  if (led_num_start + ct > LEDx_Max + 1) { // so start 15, ct 1 is ok
    byte first_ct = LEDx_Max - led_num_start + 1; // from start to max, +1 for a count
    pwm(led_num_start, first_ct, values);
    pwm(0, ct-first_ct, &values[ first_ct ]); // ct is index+1
    }

  // Normal
  else {
    byte start_register = PWMx_Register(led_num_start);

    byte addr = this->address(); // force lazy

    //debug(F("For PWM @")); debug(led_num_start); debug(" ");for(byte i=0;i<ct;i++){ debug(values[0]); debug(" ");} debug();

    //debug(F("Start update registers"));debug();
    update_ledx_registers(addr, LEDOUT_PWM, led_num_start, led_num_start + ct -1);
    //debug(F("Done update registers"));debug();

    _begin_trans(addr, Auto_PWM, start_register); 
        //debug("  ");
        Wire.write(values, ct); // maybe slightly better than individual writes 
        for(byte i=0; i < ct; i++) {
          byte new_val = values[i];
          //debug(new_val); debug(" ");
          shadow_registers[ start_register + i ] = new_val;
          }
    _end_trans();
    //debug(F("Done pwm values"));debug();
    }
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

  debug(F("I2C scanner. Scanning ..."));debug();

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
      debug(F(" got "));debug(addr,HEX);

      ::delay(10);  // maybe unneeded?
      } // end of good response
    else if (stat != 2) {
      debug(F(" Unexpected stat(")); debug(stat); debug(F(") at address ")); debug(addr,HEX);
      }

    debug();

  } // end of for loop
  debug(F("Checked "));debug(debug_tried);
  debug(F(" out of "));debug(Base_Addr,HEX); debug(".."); debug(Max_Addr,HEX);
  debug();
  
  if (this->found) {
      debug(F("Found "));
      debug(this->found);
      debug(" ");debug(Device);debug(F(" address(es) that responded."));
      debug();
    }
  else {
    debug(F("None found!"));
    }

  return *this;
}

TLC59116::Scan& TLC59116::Scan::print() {
  bool has_reset = 0;

  Serial.print(F("Scanned 0x"));
  Serial.print(Base_Addr,HEX); Serial.print(F("(0)..0x")); Serial.print(Max_Addr,HEX);
  Serial.print("(");Serial.print(Max_Addr-Base_Addr);Serial.print("):");
  Serial.println();

  for (byte i=0; i<this->found; i++) {
    byte addr = addresses[i];
    Serial.print(Device);Serial.print(F(" at 0x"));
    Serial.print(addr,HEX);
    Serial.print("/");Serial.print(addr);
    Serial.print("(");Serial.print(addr-Base_Addr);Serial.print(")");
    if (addr == Reset_Addr) { Serial.print(F(" Reset_Addr")); }
    if (addr == AllCall_Addr) { Serial.print(F(" AllCall_Addr")); }
    if (addr == SUBADR1) { Serial.print(F(" SUBADR1")); }
    if (addr == SUBADR2) { Serial.print(F(" SUBADR2")); }
    if (addr == SUBADR3) { Serial.print(F(" SUBADR3")); }

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

