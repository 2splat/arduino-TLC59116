// So each TLC59116 can blink at it's own rate
// Each TLC blinks n times, then pause. repeat.

#include "TLC59116.h"
extern TLC59116Manager tlcmanager; // see .ino

class BlinkTracking;
class BlinkTracking { 
  public:
  // At time=at, set the output-channels to on/off depending on 'to'.
  // Then set the next 'at' (based in 'index') and 'to'

  static const int On_Time = 3; // elapsed from beginning of cycle. empirical to keep led on very short
  static const int Interblink = 300; // off time
  static const int Intercount = 500; // the pause
  static const long Output_Bits = 0xF; // just the first 4 (0..3). several, so it's easy to alligator clip.

  unsigned long at;
  byte to;   // 0 is intercount, odd is on, even is off
  byte index;

  static BlinkTracking **tracked; // [0..tlmanager.device_count], the blink-state of each tlc

  // Protocol:
  static void init_tracking(); // we assume a global 'tlcmannager'
  static void update(unsigned long now);

  // internal
  BlinkTracking(byte index) : index(index), at(0), to(1) {} // tlcmanager[index], start on

  void _update(unsigned long now);
    
};
