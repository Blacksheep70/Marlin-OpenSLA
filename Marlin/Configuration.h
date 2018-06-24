#ifndef CONFIGURATION_H
#define CONFIGURATION_H

// This configurtion file contains the basic settings.
// Advanced settings can be found in Configuration_adv.h 
// BASIC SETTINGS: select your board type, axis scaling, and endstop configuration

//User specified version info of THIS file to display in [Pronterface, etc] terminal window during startup.
//Implementation of an idea by Prof Braino to inform user that any changes made
//to THIS file by the user have been successfully uploaded into firmware.
#define STRING_VERSION_CONFIG_H __DATE__ " " __TIME__ // build date and time
#define STRING_CONFIG_H_AUTHOR "Justin @ Pryntech" //Who made the changes.

//OpenSL Print Mode
// 0 = Steps (Slower, but consistent curing speed)
// 1 = Scanning Segments (Faster, but untested)
#define OPENSL_PRINT_MODE 0
#define OPENSL_SCAN_TIME_MS_PER_MM 1

// This determines the communication speed of the printer
#define BAUDRATE 250000
//#define BAUDRATE 115200

//// The following define selects which electronics board you have. Please choose the one that matches your setup
// Gen7 custom (Alfons3 Version) = 10 "https://github.com/Alfons3/Generation_7_Electronics"
// Gen7 v1.1, v1.2 = 11
// Gen7 v1.3 = 12
// Gen7 v1.4 = 13
// MEGA/RAMPS up to 1.2 = 3
// RAMPS 1.3 = 33 (Power outputs: Extruder, Bed, Fan)
// RAMPS 1.3 = 34 (Power outputs: Extruder0, Extruder1, Bed)
// Gen6 = 5
// Gen6 deluxe = 51
// Sanguinololu 1.2 and above = 62
// Melzi = 63
// Ultimaker = 7
// Teensylu = 8
// OpenSL = 81
// Gen3+ =9
#define MOTHERBOARD 81

#if (MOTHERBOARD == 81) //OpenSL
#define SDSUPPORT
#endif

//===========================================================================
//=============================Mechanical Settings===========================
//===========================================================================

// Uncomment the following line to enable CoreXY kinematics
// #define COREXY

// corse Endstop Settings
#define ENDSTOPPULLUPS // Comment this out (using // at the start of the line) to disable the endstop pullup resistors

#ifndef ENDSTOPPULLUPS
// fine Enstop settings: Individual Pullups. will be ignord if ENDSTOPPULLUPS is defined
#define ENDSTOPPULLUP_XMAX
#define ENDSTOPPULLUP_YMAX
#define ENDSTOPPULLUP_ZMAX
#define ENDSTOPPULLUP_XMIN
#define ENDSTOPPULLUP_YMIN
//#define ENDSTOPPULLUP_ZMIN
#endif

#ifdef ENDSTOPPULLUPS
#define ENDSTOPPULLUP_XMAX
#define ENDSTOPPULLUP_YMAX
#define ENDSTOPPULLUP_ZMAX
#define ENDSTOPPULLUP_XMIN
#define ENDSTOPPULLUP_YMIN
#define ENDSTOPPULLUP_ZMIN
#endif

// The pullups are needed if you directly connect a mechanical endswitch between the signal and ground pins.
const bool X_ENDSTOPS_INVERTING = false; // set to true to invert the logic of the endstops. 
const bool Y_ENDSTOPS_INVERTING = false; // set to true to invert the logic of the endstops. 
const bool Z_ENDSTOPS_INVERTING = true; // set to true to invert the logic of the endstops. 
#define DISABLE_MAX_ENDSTOPS

// For Inverting Stepper Enable Pins (Active Low) use 0, Non Inverting (Active High) use 1
#define X_ENABLE_ON 0
#define Y_ENABLE_ON 0
#define RZ_ENABLE_ON 0
#define LZ_ENABLE_ON 0 // For all extruders

// Disables axis when it's not being used.
#define DISABLE_X false
#define DISABLE_Y false
#define DISABLE_RZ false
#define DISABLE_LZ false // For all extruders

#define INVERT_X_DIR false    // for Mendel set to false, for Orca set to true
#define INVERT_Y_DIR true  // for Mendel set to true, for Orca set to false
#define INVERT_RZ_DIR false     // for Mendel set to false, for Orca set to true
#define INVERT_LZ_DIR false  // for direct drive extruder v9 set to true, for geared extruder set to false

// ENDSTOP SETTINGS:
// Sets direction of endstops when homing; 1=MAX, -1=MIN
#define X_HOME_DIR -1
#define Y_HOME_DIR -1
#define Z_HOME_DIR -1

#define min_software_endstops false //If true, axis won't move to coordinates less than HOME_POS.
#define max_software_endstops false  //If true, axis won't move to coordinates greater than the defined lengths below.
// Travel limits after homing
#define X_MAX_POS 100 //205
#define X_MIN_POS 0
#define Y_MAX_POS 100 //205
#define Y_MIN_POS 0
#define Z_MAX_POS 180
#define Z_MIN_POS 0

#define X_MAX_LENGTH (X_MAX_POS - X_MIN_POS)
#define Y_MAX_LENGTH (Y_MAX_POS - Y_MIN_POS)
#define Z_MAX_LENGTH (Z_MAX_POS - Z_MIN_POS)

// The position of the homing switches
#define MANUAL_HOME_POSITIONS  // If defined, manualy programed locations will be used
//#define BED_CENTER_AT_0_0  // If defined the center of the bed is defined as (0,0)

//Manual homing switch locations:
#define MANUAL_X_HOME_POS 0
#define MANUAL_Y_HOME_POS 0
#define MANUAL_Z_HOME_POS 0

//// MOVEMENT SETTINGS
#define NUM_AXIS 4 // The axis order in all axis related arrays is X, Y, Z, E
#define HOMING_FEEDRATE {50*60, 50*60, 180, 180}  // set the homing speeds (mm/min)

// default settings 
#define XY_GALVO_SCALAR 45
#define DEFAULT_AXIS_STEPS_PER_UNIT   {652.79/XY_GALVO_SCALAR, 652.79/XY_GALVO_SCALAR,4000,4000}  // default steps per unit for OpenSL
#define DEFAULT_MAX_FEEDRATE          {8000, 8000, 10, 10}    // (mm/sec)    
#define DEFAULT_MAX_ACCELERATION      {12000,12000,4,4}    // X, Y, RZ, LZ maximum start speed for accelerated moves. E default values are good for skeinforge 40+, for older versions raise them a lot.

#define DEFAULT_ACCELERATION          5000    // X, Y, RZ and LZ max acceleration in mm/s^2 for printing moves 3000 for steel bearings
#define DEFAULT_RETRACT_ACCELERATION  5000    // X, Y, RZ and LZ max acceleration in mm/s^2 for r retracts 3000 for steel bearings

// 
#define DEFAULT_XYJERK                300.0    // (mm/sec) 20 for steel bearings
#define DEFAULT_ZJERK                 0.4     // (mm/sec)

//===========================================================================
//=============================Additional Features===========================
//===========================================================================

// EEPROM
// the microcontroller can store settings in the EEPROM, e.g. max velocity...
// M500 - stores paramters in EEPROM
// M501 - reads parameters from EEPROM (if you need reset them after you changed them temporarily).  
// M502 - reverts to the default "factory settings".  You still need to store them in EEPROM afterwards if you want to.
//define this to enable eeprom support
//#define EEPROM_SETTINGS
//to disable EEPROM Serial responses and decrease program space by ~1700 byte: comment this out:
// please keep turned on if you can.
//#define EEPROM_CHITCHAT

//LCD and SD support
//#define ULTRA_LCD  //general lcd support, also 16x2
//#define SDSUPPORT // Enable SD Card Support in Hardware Console

//#define ULTIMAKERCONTROLLER //as available from the ultimaker online store.
//#define ULTIPANEL  //the ultipanel as on thingiverse
//#define PANUCATT_VIKI // Panucatt ViKi (formerly VersaPanel)

#ifdef PANUCATT_VIKI
#define ULTIPANEL
#define NEWPANEL
#define MCP23017_LCD // Adafruit RGB LCD compatibles
#endif


#ifdef ULTIMAKERCONTROLLER    //automatic expansion
#define ULTIPANEL
#define NEWPANEL
#endif 


#ifdef ULTIPANEL
//  #define NEWPANEL  //enable this if you have a click-encoder panel
#define SDSUPPORT
#define ULTRA_LCD
#define LCD_WIDTH 20
#define LCD_HEIGHT 4

#else //no panel but just lcd 
#ifdef ULTRA_LCD
#define LCD_WIDTH 16
#define LCD_HEIGHT 2
#endif
#endif

// Increase the FAN pwm frequency. Removes the PWM noise but increases heating in the FET/Arduino
//#define FAST_PWM_FAN

// M240  Triggers a camera by emulating a Canon RC-1 Remote
// Data from: http://www.doc-diy.net/photo/rc-1_hacked/
// #define PHOTOGRAPH_PIN     23

// SF send wrong arc g-codes when using Arc Point as fillet procedure
//#define SF_ARC_FIX

#include "Configuration_adv.h"

#endif //__CONFIGURATION_H


