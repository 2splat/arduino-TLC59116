// Blink one TCL59116

// 2 includes!
#include <Wire.h>
#include "TLC59116.h"

// Need a manager
TLC59116Manager tlcmanager; 

void setup() {
    tlcmanager.init(); // inits Wire, devices, and collects them
    }

void loop() {
    // Name it.
    // Note the "static" and "&"
    static TLC59116 &left_arm = tlcmanager[0]; // the lowest address device

    // blink first channel
    left_arm.set(0, HIGH);
    delay(200);
    left_arm.set(0, LOW);
    delay(200);
    }



