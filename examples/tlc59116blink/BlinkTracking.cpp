#include "BlinkTracking.h"

BlinkTracking **BlinkTracking::tracked = NULL;

#define debug(msg)
#define debugln(msg)
// #define debug(msg) Serial.print(msg)
// #define debugln(msg) Serial.println(msg)

void BlinkTracking::init_tracking() {
   int ct = tlcmanager.device_count();
   BlinkTracking::tracked = (BlinkTracking**) malloc(ct * sizeof(BlinkTracking*));
   for(int i=0; i< ct; i++) {
    debug(F("Setup blink periods for "));debug(i);debugln(F("th tlc"));
    BlinkTracking::tracked[i] = new BlinkTracking(i);
    }
   }

void BlinkTracking::update(unsigned long now) {
    debug(now);debug(F(" "));
    for(int i=0; i< tlcmanager.device_count(); i++) {
      debug(i);debug(": ");
      BlinkTracking::tracked[i]->_update(now);
      debug(F(" | "));
      }
    debugln();
    }

void BlinkTracking::_update(unsigned long now) {
  // Is it time?
  debug(this->index);debug(" ");debug(this->at);debug(F(" -> "));debug(this->to);
  if (now > this->at) {
    // Between counting?
    if (this->to == 0) {
      debug(F(" <>"));
      this->at = now + Intercount;
      tlcmanager[this->index].off_pattern(Output_Bits);
      }
    // odd 'state' is on
    else if (this->to & 0x1) {
      debug(F(" +"));
      this->at = now + On_Time;
      tlcmanager[this->index].on_pattern(Output_Bits);
      }
    // even 'state' is off
    else {
      debug(F(" -"));
      this->at = now + Interblink;
      tlcmanager[this->index].off_pattern(Output_Bits);
      }

    // move state
    this->to ++;
    debug(F(" +"));debug(this->to);
    if (this->to > ((index+1) * 2)) { this->to = 0; }
    debug(F(" %"));debug(this->to);
    }

  // Detect clock wrap-around
  else if ( this->at - now > (Intercount+1) ) {
    this->at = 0;
    tlcmanager[this->index].off_pattern(Output_Bits);
    }
  }
