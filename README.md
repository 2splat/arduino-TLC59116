#define TLC59116_h

// demo commentz
// FIXME: insert version here

/* Description: High-level Arduino library for TI TLC59116 LED Driver over I2C

  See: TLC59116_Unmanaged for lower level library.

  This interface provides a high level interface:
  * So it's easier to get started (provides useful defaults, etc.).
  * With supposedly friendly functions.
  * And allowing the friendlier ".", instead of remembering when to use "->".
  
  This interface keeps track of the device(s) state (i.e. output-channel 4 is already "on"):
  * So you don't have to, which minimizes bugs 
(i.e.: only update the bits to turn channel 5 on, without stomping the other channels).
  * And to optimize communication with the device(s).

  See: README.*, doc/index.html


