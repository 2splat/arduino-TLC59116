// So tired of typing "Serial.print"
template <typename T> void inline print(T msg) { Serial.print(msg); }
template <typename T> void inline print(T msg, int format) { Serial.print(msg,format); }

// convenience to print a value with base (hex/bin) & leading zeros according to size
template <typename T> void printw(T msg, int format) {
  if (format == BIN) {
    Serial.print(F("0b"));
    for(byte i=0; i < (sizeof(msg) * 8); i++) {
      Serial.print( (msg & ((T)1 << (sizeof(msg) * 8 - 1))) ? "1" : "0");
      msg <<= 1;
      }
    }
  else if (format == HEX) {
    Serial.print(F("0x"));
    for(byte i = (sizeof(msg) / 4) -1 ; i >= 0; i--) { // count down from msb digit
      Serial.print(
          msg >> (i*4) // digit to right
          & 0xf // pick off digit
        ,format
        );
      }
    }
  }

