#include <Wire.h>
#include <avr/pgmspace.h>

#include "TLC59116.h"
TLC59116Manager tlcmanager; // defaults

void setup() {
    Serial.begin(9600);
    tlcmanager.init();
    Serial.println("STARTED");
    pinMode(13,OUTPUT);
}

void loop() {
  static TLC59116 &t = tlcmanager[0];
  t.on(0);digitalWrite(13,HIGH);
  delay(100);
  t.off(0);digitalWrite(13,LOW);
  delay(100);
  Serial.println("loop");
}
