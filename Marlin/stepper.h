/*
  stepper.h - stepper motor driver: executes motion plans of planner.c using the stepper motors
  Part of Grbl

  Copyright (c) 2009-2011 Simen Svale Skogsrud

  Grbl is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Grbl is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Grbl.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef stepper_h
#define stepper_h 

#include "planner.h"

#define WRITE_LZ_STEP(v) WRITE(LZ_STEP_PIN, v)
#define NORM_LZ_DIR() WRITE(LZ_DIR_PIN, !INVERT_LZ_DIR)
#define REV_LZ_DIR() WRITE(LZ_DIR_PIN, INVERT_LZ_DIR)


// Initialize and start the stepper motor subsystem
void st_init();

// Block until all buffered steps are executed
void st_synchronize();

// Set current position in steps
void st_set_position(const long &x, const long &y, const long &rz, const long &lz);
void st_set_e_position(const long &lz);

// Get current position in steps
long st_get_position(uint8_t axis);

// The stepper subsystem goes to sleep when it runs out of things to execute. Call this
// to notify the subsystem that it is time to go to work.
void st_wake_up();

//Galvo Control

void scan_X_Y_galvo(unsigned long x1, unsigned long y1, unsigned long x2, unsigned long y2);
void coordinate_XY_move(unsigned long X, unsigned long Y);

short World_to_Galvo(long value);
void update_X_galvo(int step_dir);
void update_Y_galvo(int step_dir);
void digitalPotWrite(int channel, int value);
void move_galvos(unsigned long X, unsigned long Y);
void set_galvo_pos(unsigned long X, unsigned long Y);
void move_X_galvo(unsigned short X);
void move_Y_galvo(unsigned short Y);

void timed_refresh_of_galvos(void);

void checkHitEndstops(); //call from somwhere to create an serial error message with the locations the endstops where hit, in case they were triggered
void endstops_hit_on_purpose(); //avoid creation of the message, i.e. after homeing and before a routine call of checkHitEndstops();

void enable_endstops(bool check); // Enable/disable endstop checking

void checkStepperErrors(); //Print errors detected by the stepper

void finishAndDisableSteppers();

extern block_t *current_block;  // A pointer to the block currently being traced

void quickStop();
#endif

