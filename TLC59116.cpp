static const prog_uchar Power_Up_Register_Values[TLC59116::Control_Register_Max] PROGMEM = {
  MODE1_OSC_mask | MODE1_ALLCALL_mask,
  0, // mode2
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // pwm0..15
  0xff, // grppwm
  0, // grpfreq
  0,0,0,0, // ledout0..3
  (SUBADR1 << 1), (SUBADR2 << 1), (SUBADR2 << 1),
  AllCall_Addr,
  IREF_CM_mask | IREF_HC_mask | ( IREF_CC_mask && 0xff),
  0,0 // eflag1..eflag2
  };

void TLC59116::reset_shadow_registers() {
  memcpy_P(*shadow_registers,
    (byte*) TLC59116Manager::Power_Up_Register_Values,
    Control_Register_Max
    );
  }

void TLC59116::reset_happened() {
  this->reset_shadow_registers();
  }

//
// TLC59116Manager
//

void TLC59116Manager::init(long frequency, byte dothings) {
  this->reset_actions = dothings;
  if (dothings & WireInit) bus.begin();
  // don't know how to set other WIRE interfaces
  if (bus == Wire) TWBR = ((F_CPU / frequency) - 16) / 2; // AFTER wire.begin
  scan(); // FIXME, supposed to "populate"
  if (dothings & Rest) reset();
  else reset_shadowregisters();
  if (dothings & EnableOutputs) broadcast.enable_outputs();
  }

void TLC59116Manager::reset() {
  doit, address based, so can't bulk
  for (byte i=0; i<= device_ct; i++) { devices[i]->reset_happened(); }
  if (this->reset_actions & EnableOutputs) broadcast.enable_outputs();
  }
