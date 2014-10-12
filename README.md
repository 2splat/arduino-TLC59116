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


