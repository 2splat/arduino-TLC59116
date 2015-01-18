// So each TLC59116 can blink at it's own rate
// Each TLC blinks n times, then pause. repeat.

#include "TLC59116.h"
extern TLC59116Manager tlcmanager; // see .ino

class BlinkTracking;
class BlinkTracking { 
  public:
  // At time=at, set the output-channels to on/off depending on 'to'.
  // Then set the next 'at' (based in 'index') and 'to'

  const static int On_Time = 3; // elapsed from beginning of cycle. empirical to keep led on very short
  const static int Interblink = 300; // off time
  const static int Intercount = 500; // the pause
  const static long Output_Bits = 0xF; // just the first 4 (0..3). several, so it's easy to alligator clip.

  unsigned long at;
  byte to;   // 0 is intercount, odd is on, even is off
  byte index;

  static BlinkTracking **tracked; // [0..tlmanager.device_count]
  static int xx;

  // Protocol:
  static void init_tracking();
  static void update(unsigned long now);

  BlinkTracking(byte index) : at(0), to(1) {} // start on

  void _update(unsigned long now);
    
};
