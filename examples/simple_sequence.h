// a little sequential state machine.
// This will execute steps with delays between, without blocking. e.g.:
/* 
  void loop() {
    Serial.print(millis()); // Will repeatedly print

    // This does not block:
    start_sequence
      // On/Off 250msec each
      sequence( digitalWrite(11,HIGH), 250)  // does this, then waits for 250msec to pass, to proceed
      sequence( digitalWrite(11,LOW), 250)
    end_sequence

    Serial.println(" END");
    }
*/
// Two versions
//    // A simple table of steps:
//    start_sequence
//      sequence(...)
//      ...
//    end_sequence
//
//    // A loop around it of: while (Serial.available() == 0)
//    // I.e. Do this table till there is input
//    do_sequence_till_input
//      sequence(...)
//      ...
//    end_do_sequence


// start_sequence sequence(#, fn(), delay)... end_sequence
#define do_sequence_till_input while (Serial.available() == 0) { start_sequence
#define start_sequence { \
  static byte sequence_i = 0; \
  static unsigned long sequence_next = 0; \
  if (millis() >= sequence_next) { \
    /* Serial.print(F("Sequence "));Serial.println(sequence_i); */ \
    switch (sequence_i) { \
      case -1 : /* dumy */
#define sequence(sofar, fn, delay_millis) break; \
      case sofar : sequence_i++; sequence_next = millis() + delay_millis; fn;
#define end_sequence \
      default: sequence_i=0; /* wrap i */ \
      } \
    /* Serial.print(F("Next seq "); Serial.print(sequence_i); Serial.print(F(" in "));Serial.println(sequence_next - millis()); */ \
    } \
  }
#define end_do_sequence end_sequence }
