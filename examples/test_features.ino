#include "TLC59116.h"
#include <Wire.h>
#include <avr/pgmspace.h>

#define TLC59116_WARNINGS 1

TLC59116Manager tlcmanager; // defaults

void setup() {
  }

void loop() {
  TLC59116& tlc_first = tlcmanager[0];
  }
