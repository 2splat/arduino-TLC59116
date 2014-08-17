// So tired of typing "Serial.print"
template <typename T> void inline print(T msg) { Serial.print(msg); }
template <typename T> void inline print(T msg, int format) { Serial.print(msg,format); }

// convenience to print a value with base (hex/bin) & leading zeros according to size
template <typename T> void printw(T msg, int format) {
  if (format == BIN) {
    print(F("0b"));
    for(byte i=0; i < (sizeof(msg) * 8); i++) {
      print( (msg & ((T)1 << (sizeof(msg) * 8 - 1))) ? "1" : "0");
      msg <<= 1;
      }
    }
  else if (format == HEX) {
    print(F("0x"));
    // print(F("["));print(msg,HEX);print(F("]"));
    for(byte i = (sizeof(msg) * 2); i > 0; i--) { // count down from msb digit
      print(
          msg >> ((i-1)*4) // digit to right
          & 0xf // pick off digit
        ,format
        );
      }
    }
  }

