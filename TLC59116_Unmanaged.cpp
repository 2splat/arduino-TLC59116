// FIXME: make things final

#define TLC59116_LOWLEVEL 0
#define TLC59116_DEV 1
#define TLC59116_WARNINGS 1

#include "TLC59116_Unmanaged.h"
#include <Wire.h>

// The convenience method sometlc.delay(x) hides the normal delay. Use ::delay(...)

#define WARN TLC59116Warn
#define DEV TLC59116Dev
#define LOWD TLC59116LowLevel
#define DEBUG TLC59116Warn

const char* TLC59116_Unmanaged::Device = "TLC59116";

inline void printf0(byte v) { // 3 digits
    Serial.print(v / 100);
    v = v % 100;
    Serial.print(v/10);
    v = v % 10;
    Serial.print(v);
    }

void TLC59116_Unmanaged::LEDx_set_mode(byte registers[] /*[4]*/, byte to_what, word which) {
  // count through LED nums, starting from max (backwards is easier)

  for(byte ledx_i=15; ; ledx_i--) {
    if (0x8000 & which) {
      registers[ledx_i / 4] = LEDx_set_mode(
        registers[ledx_i / 4], 
        ledx_i, 
        to_what
        );
      }
    which <<= 1;

    if (ledx_i==0) break; // can't detect < 0 on an unsigned!
    }
  }

void TLC59116_Unmanaged::control_register(byte register_num, byte data) {
  // set
  if (!is_control_register(register_num)) {
    TLC59116Warn(F("Not a "));TLC59116Warn(Device);TLC59116Warn(F(" control register #:"));TLC59116Warn(register_num,HEX);
    TLC59116Warn(F(". Set to "));TLC59116Warn(data,HEX);TLC59116Warn(F("is ignored."));
    return;
    }
  LOWD(F("Set "));LOWD(register_num,HEX);LOWD(F("=>"));LOWD(data,HEX);LOWD();
  _begin_trans(register_num); 
    LOWD(F(" "));LOWD(data,BIN);LOWD();
    i2cbus.write(data);
  _end_trans();
  }

byte TLC59116_Unmanaged::fetch_registers(byte start_r, byte count, byte registers[]) {
  // FIXME: make some notes on this pattern
  _begin_trans(Auto_All, start_r);
  byte rez = _end_trans();
  if (rez != 0) {
     WARN(F("Read all failed"));WARN(); 
     return rez;
     }

  int avail = i2cbus.requestFrom(address(), count);
  LOWD(F("Read # registers: "));LOWD(avail);LOWD();
  if (avail != count) { 
    WARN(F("Expected to read "));WARN(count);
    WARN(F(" bytes for all registers, but got "));WARN(avail);WARN();
    }
  if (avail > count) avail = count;

  for(byte i=0; i<avail; i++) {
    registers[i] = i2cbus.read();
    LOWD(F("  Read "));LOWD(i,HEX);LOWD(F("="));LOWD(registers[i],HEX);LOWD(F("/"));LOWD(registers[i],BIN);LOWD();
    }

  return 0;
  }

TLC59116_Unmanaged& TLC59116_Unmanaged::describe_actual() {
  byte actual[Control_Register_Max]; 
  byte rez = fetch_registers(0, Control_Register_Max+1, actual);
  if (rez) return *this;

  Serial.print(Device);Serial.print(F(" 0x"));Serial.print(address(),HEX);
  Serial.print(F(" on bus "));Serial.print((unsigned long)&i2cbus, HEX);Serial.println();
  Serial.println(F("Actual Registers"));

  TLC59116_Unmanaged::describe_registers(actual);
return *this;
  }



void TLC59116_Unmanaged::describe_registers(byte* registers /*[Control_Register_Max]*/) {
  describe_iref(registers);
  // mode1 is folded into describe_channels()
  describe_addresses(registers);
  describe_mode2(registers);
  describe_group_mode(registers);
  describe_outputs(registers);
  describe_error_flag(registers);
  }

void TLC59116_Unmanaged::describe_iref(byte* registers) {
    const byte value = registers[IREF_Register];

    byte CC = value & IREF_CC_mask;
    byte D=reverse_cc(CC);

    Serial.print(F("Iout "));Serial.print(i_out(value));Serial.print(F("ma @ "));Serial.print(Rext_Min);Serial.print(F("ohms ("));
    byte CM=(value & IREF_CM_mask) ? 1 : 0;
    Serial.print(F("CM:"));Serial.print( (value & IREF_CM_mask) ? "1" : "0");
    byte HC=(value & IREF_HC_mask) ? 1 : 0;
    Serial.print(F(" HC:"));Serial.print(HC);
    Serial.print(F(" CC:0b"));Serial.print( (value & IREF_CC_mask), BIN); 
    Serial.print(F(" (D:"));Serial.print(D);Serial.print(F(") = "));
    Serial.print(((1.26 * ((1 + HC) * (1.0 + D/64.0) / 4.0)) * (CM ? 15 : 5)));Serial.print(F("/Rext amp"));
    Serial.println();
    }

void TLC59116_Unmanaged::describe_addresses(byte* registers) {
  byte mode1 = registers[MODE1_Register];
  byte the_addr;

  // Internal addresses are 7:1, with bit 0=r/w

  the_addr = registers[AllCall_Addr_Register];
  Serial.print(mode1 & MODE1_ALLCALL_mask ? "+" : "-");
  Serial.print(F("AllCall(0x"));Serial.print(the_addr >> 1,HEX);Serial.print(") ");

  for(byte i = 0; i<3; i++) {
      the_addr = registers[SUBADR1_Register + i];
      // mode bit is 2,4,8 for enable
      Serial.print(mode1 & (MODE1_SUB1_mask >> i) ? "+" : "-");
      Serial.print(F("SUBADR")); Serial.print(i+1); Serial.print("(0x");
      Serial.print(the_addr >> 1, HEX); Serial.print(") ");
      }
  Serial.println();
  }

byte TLC59116_Unmanaged::i_out(byte CM, byte HC, byte CC, int Rext) { 
  // the Iout per channel 
  byte D=reverse_cc(CC);
  return i_out_d(CM, HC, D, Rext); 
  }

byte TLC59116_Unmanaged::i_out_d(byte CM, byte HC, byte D, int Rext) { 
  // the Iout per channel, with bit reversed CC
  return ((1.26 * ((1 + HC) * (1.0 + D/64.0) / 4.0)) / (double)Rext) * (CM ? 15 : 5) *1000;
  }

byte TLC59116_Unmanaged::best_iref(byte ma, int Rext) {
  byte HC; // [6]
  byte CM; // [7]
  byte D; // [5:0], but reverse bits for CC!

  // Iout = ((1.26v * ((1 + HC) * (1 + D/64) / 4)) / Rext) * 15 * 3^(CM-1)
  // (as per specsheet "SLDS157D – FEBRUARY 2008 – REVISED JULY 2011")
  // where D is CC bit-reversed
  // simplify for HC & CM and D=0|127 to get ranges below

  byte actual = 255; // anything is better.
  if (ma <= 5 || ma < 1575/Rext) {
    HC=0; CM=0; D=0;
    actual = i_out_d(CM, HC, D, Rext);
    // printf("Too small, actual %dma HC %d, CM %d, D %03o\n",actual, HC, CM, D);
    }
  else if (ma >= 120 || ma > 28202.3/Rext) {
    HC=1; CM=1; D=0b111111;
    actual = i_out_d(CM, HC, D, Rext);
    // printf("Too big, actual %dma HC %d, CM %d, D %03o\n",actual, HC, CM, D);
    }
  else {
    if (ma <= 4725/Rext) {
      HC=0; CM=0; D = (2.560*ma*Rext)/63-64;
      actual = i_out_d(CM, HC, D, Rext);
      // printf("segment 1, actual %dma HC %d, CM %d, D %03o\n",actual, HC, CM, D);
      }
    else if (ma <= 14101.2/Rext) {
      HC=0; CM=1; D = (2.560*ma*Rext)/189-64;
      actual = i_out_d(CM, HC, D, Rext);
      // printf("segment 2, actual %dma HC %d, CM %d, D %03o\n",actual, HC, CM, D);
      }
    if (ma >= 3150/Rext) {
      if (ma <= 9449.98/Rext) {
        HC=1; CM=0; D = (1.280*ma*Rext )/63-64;
        byte proposed = i_out_d(CM, HC, D, Rext);
        // The solutions overlap, so:
        if ( proposed >= actual) actual=proposed;
        else {
          // printf("  tried proposed %dma HC %d, CM %d, D %03o but, use previous segment\n", proposed, HC, CM, D);
          // revert
          D=D*2; // 1.28 -> 2.56
          HC=0;
          actual = i_out_d(CM, HC, D, Rext);
          }
        // printf("segment 3, actual %dma HC %d, CM %d, D %03o\n",actual, HC, CM, D);
        }
      else {
        HC=1; CM=1; D = (1.280*ma*Rext)/189.0-64;
        actual = i_out_d(CM, HC, D, Rext);
        // printf("segment 4, actual %dma HC %d, CM %d, D %03o\n",actual, HC, CM, D);
        }
      }
    }
  byte iref = 0;
  // CC (D reversed)
  iref = reverse_cc(D);
  iref |= HC << 6 | CM << 7;
  DEV(F("Iref wanted "));DEV(ma);DEV(F("ma closest "));DEV(actual);DEV(F("ma, iref="));DEV(iref,BIN);DEV();
  // printf("Iref wanted %dma closest %dma, iref=%03o\n", ma, actual, iref);
  if (abs(ma - actual) > 1) { WARN(F("You asked for "));WARN(ma);WARN(F(" but best is "));WARN(actual);WARN(); }
  return iref; // FIXME: and the flags
  }

void TLC59116_Unmanaged::describe_mode2(byte *registers) {
    byte value = registers[MODE2_Register];

    // All other bits are folded into describe_channels
    Serial.print(F("Latch on "));Serial.print((value & MODE2_OCH) ? F("ACK") : F("Stop"));
    Serial.println();
    }

void TLC59116_Unmanaged::describe_group_mode(byte *registers) {
    // registers has to have valid: [MODE1_Register,MODE2_Register...GRPPWM_Register,GRPFREQ_Register]
    // e.g. [0,1,...,18,19]
    
    // General/Group
    byte grppwm = registers[GRPPWM_Register];
    byte mode1 = registers[MODE1_Register];
    byte mode2 = registers[MODE2_Register];
    bool is_blink = (mode2 & MODE2_DMBLNK);

    // note: if MODE2[GRP/DMBLNK] then GRPPWM is PWM, GRPFREQ is blink 24Hz to .1hz
    // note: if !MODE2[GRP/DMBLNK] then GRPPWM is PWM, GRPFREQ ignored, and PWMx is * GRPPWM
    if (is_blink) {
      Serial.print(F("GRP[Blink]="));
      Serial.print(grppwm);Serial.print(F("/256 on-ratio, "));
      }
    else {
      Serial.print(F("GRP[*PWM]="));
      Serial.print(grppwm);Serial.print(F("/256, "));
      }

    Serial.print(F("GRPFREQ="));
    byte grpfreq = registers[GRPFREQ_Register];

    if (!is_blink) {
      if (grpfreq !=0) {
        Serial.print(F("!"));Serial.print(grpfreq);Serial.print(F("/256, "));
        Serial.print(F("(group PWM dynamic range):"));
        }
      else {
        Serial.print(F("(ignored):"));
        }
      }
    else {
      float blink_len = ((float)grpfreq + 1.0) / 24.0; // docs say max 10.73 secs, but math says 10.66 secs
      // Aren't we nice:
      if (blink_len <= 1) {
        Serial.print(1.0/blink_len);Serial.print(F("Hz blink"));
        }
      else {
        Serial.print(blink_len);Serial.print(F(" second blink"));
        }
      }

    if (is_blink) { Serial.print(F(" for outputs that are ")); Serial.println(F("Bnnn")); }
    else { Serial.print(F(" on top of outputs that are ")); Serial.println(F("Gnnn")); }
    }
     
void TLC59116_Unmanaged::describe_outputs(byte *registers) {
    byte mode1 = registers[MODE1_Register];
    byte mode2 = registers[MODE2_Register];
    bool is_blink = (mode2 & MODE2_DMBLNK);

    Serial.print(F("Outputs "));
    Serial.print( (mode1 & MODE1_OSC_mask) ? F("OFF") : F("ON")); // nb. reverse, "OSC"
    Serial.println();
    Serial.print(F("        "));

    for(byte led_num=0; led_num < Channels; led_num++) {
        byte led_state = registers[LEDOUTx_Register(led_num)];
        // The led state is groups of 4
        byte state_bits = (led_state >> ((led_num % 4) * 2)) & LEDOUT_Mask; // right shifted to lsb's location

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
                pwm_value = registers[PWM0_Register + led_num];
                printf0(pwm_value);
                Serial.print(" ");
                break;
            // GPWM
            case LEDOUT_GRPPWM:
                if (is_blink) {
                  pwm_value = registers[PWM0_Register + led_num];
                  Serial.print(F("B"));printf0(pwm_value);
                  }
                else {
                  Serial.print(F("G"));
                  printf0(pwm_value);
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

void TLC59116_Unmanaged::describe_error_flag(byte* registers) {
    byte mode2 = registers[MODE2_Register];
    byte value;
    // format same as channels

    Serial.print(F("Errors: "));
    Serial.print((mode2 & MODE2_EFCLR) ? "clear" : "enabled"); // EFCLR
    Serial.println();
    Serial.print(F("        "));

    // 8 channels per register
    for(byte i=0;i<=1;i++) {
        value = registers[EFLAG1_Register + i];
        // Each bit is a channel
        for(byte bit_num=0; bit_num<=7; bit_num++) {
            Serial.print( (value & (1<<bit_num)) ? "1    " : "0    ");
            // format
            if (bit_num % 4 == 3 && !(bit_num==7 && i==1)) {Serial.println(); Serial.print(F("        "));}
            }
        }
    Serial.println();
    }

