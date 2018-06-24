#ifndef PINS_H
#define PINS_H

/****************************************************************************************
* OpenSL pin assingments (ATMEGA90USB1286)
* Requires the Teensyduino software with Teensy2.0++ selected in arduino IDE!
* See http://pryntech.com/wiki for more info
****************************************************************************************/
#if MOTHERBOARD == 81
#define MOTHERBOARD 81
#define KNOWN_BOARD 1

//Open SL Pins
#define GALVO_SS_PIN 42
#define LASER_PIN    44
#define R_LED 18
#define G_LED 19
#define B_LED 20

#define X_STEP_PIN         -1
#define X_DIR_PIN          -1
#define X_ENABLE_PIN       -1
#define X_MIN_PIN          -1
#define X_MAX_PIN          -1

#define Y_STEP_PIN         -1
#define Y_DIR_PIN          -1
#define Y_ENABLE_PIN       -1
#define Y_MIN_PIN          -1
#define Y_MAX_PIN          -1

#define RZ_STEP_PIN         34
#define RZ_DIR_PIN          32
#define RZ_ENABLE_PIN       33
#define Z_MIN_PIN          13
#define Z_MAX_PIN          -1

#define LZ_STEP_PIN         16
#define LZ_DIR_PIN          35
#define LZ_ENABLE_PIN       17

#define FAN_PIN            15  

#define SDPOWER            -1
#define SDSS               -1
#define LED_PIN            -1
#define PS_ON_PIN          -1
#define KILL_PIN           -1

#ifndef SDSUPPORT
// these pins are defined in the SD library if building with SD support
  #define SCK_PIN          52
  #define MISO_PIN         50
  #define MOSI_PIN         51
#endif

#endif

#ifndef KNOWN_BOARD
#error Unknown MOTHERBOARD value in configuration.h
#endif

//List of pins which to ignore when asked to change by gcode, 0 and 1 are RX and TX, do not mess with those!
#define _LZ_PINS LZ_STEP_PIN, LZ_DIR_PIN, LZ_ENABLE_PIN

#ifdef DISABLE_MAX_ENDSTOPS
#define X_MAX_PIN          -1
#define Y_MAX_PIN          -1
#define Z_MAX_PIN          -1
#endif


#define SENSITIVE_PINS {0, 1, RZ_STEP_PIN, RZ_DIR_PIN, RZ_ENABLE_PIN, LZ_STEP_PIN, LZ_DIR_PIN, LZ_ENABLE_PIN, Z_MIN_PIN, Z_MAX_PIN, LED_PIN, PS_ON_PIN, \
                        FAN_PIN, LASER_PIN, \
                        _LZ_PINS }
#endif

