/*
  stepper.c - stepper motor driver: executes motion plans using stepper motors
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

/* The timer calculations of this module informed by the 'RepRap cartesian firmware' by Zack Smith
   and Philipp Tiefenbacher. */

#include "Marlin.h"
#include "stepper.h"
#include "planner.h"
#include "language.h"
#include "speed_lookuptable.h"

#include <SPI.h>

//===========================================================================
//=============================public variables  ============================
//===========================================================================
block_t *current_block;  // A pointer to the block currently being traced


//===========================================================================
//=============================private variables ============================
//===========================================================================
//static makes it inpossible to be called from outside of this file by extern.!

// Variables used by The Stepper Driver Interrupt
static unsigned char out_bits;        // The next stepping-bits to be output
static long counter_x,       // Counter variables for the bresenham line tracer
            counter_y, 
            counter_rz,       
            counter_lz;
volatile static unsigned long step_events_completed; // The number of step events executed in the current block
#ifdef ADVANCE
  static long advance_rate, advance, final_advance = 0;
  static long old_advance = 0;
#endif
static long lz_steps[3];
static long acceleration_time, deceleration_time;
//static unsigned long accelerate_until, decelerate_after, acceleration_rate, initial_rate, final_rate, nominal_rate;
static unsigned short acc_step_rate; // needed for deccelaration start point
static char step_loops;
static unsigned short OCR1A_nominal;

volatile unsigned long Galvo_WorldXPosition;
volatile unsigned long Galvo_WorldYPosition;
volatile long endstops_trigsteps[3]={0,0,0};
volatile long endstops_stepsTotal,endstops_stepsDone;
static volatile bool endstop_x_hit=false;
static volatile bool endstop_y_hit=false;
static volatile bool endstop_z_hit=false;

static bool old_x_min_endstop=false;
static bool old_x_max_endstop=false;
static bool old_y_min_endstop=false;
static bool old_y_max_endstop=false;
static bool old_z_min_endstop=false;
static bool old_z_max_endstop=false;

static bool check_endstops = true;

volatile long count_position[NUM_AXIS] = { 0, 0, 0, 0};
volatile char count_direction[NUM_AXIS] = { 1, 1, 1, 1};

//===========================================================================
//=============================functions         ============================
//===========================================================================

  #define CHECK_ENDSTOPS  if(check_endstops)

// intRes = intIn1 * intIn2 >> 16
// uses:
// r26 to store 0
// r27 to store the byte 1 of the 24 bit result
#define MultiU16X8toH16(intRes, charIn1, intIn2) \
asm volatile ( \
"clr r26 \n\t" \
"mul %A1, %B2 \n\t" \
"movw %A0, r0 \n\t" \
"mul %A1, %A2 \n\t" \
"add %A0, r1 \n\t" \
"adc %B0, r26 \n\t" \
"lsr r0 \n\t" \
"adc %A0, r26 \n\t" \
"adc %B0, r26 \n\t" \
"clr r1 \n\t" \
: \
"=&r" (intRes) \
: \
"d" (charIn1), \
"d" (intIn2) \
: \
"r26" \
)

// intRes = longIn1 * longIn2 >> 24
// uses:
// r26 to store 0
// r27 to store the byte 1 of the 48bit result
#define MultiU24X24toH16(intRes, longIn1, longIn2) \
asm volatile ( \
"clr r26 \n\t" \
"mul %A1, %B2 \n\t" \
"mov r27, r1 \n\t" \
"mul %B1, %C2 \n\t" \
"movw %A0, r0 \n\t" \
"mul %C1, %C2 \n\t" \
"add %B0, r0 \n\t" \
"mul %C1, %B2 \n\t" \
"add %A0, r0 \n\t" \
"adc %B0, r1 \n\t" \
"mul %A1, %C2 \n\t" \
"add r27, r0 \n\t" \
"adc %A0, r1 \n\t" \
"adc %B0, r26 \n\t" \
"mul %B1, %B2 \n\t" \
"add r27, r0 \n\t" \
"adc %A0, r1 \n\t" \
"adc %B0, r26 \n\t" \
"mul %C1, %A2 \n\t" \
"add r27, r0 \n\t" \
"adc %A0, r1 \n\t" \
"adc %B0, r26 \n\t" \
"mul %B1, %A2 \n\t" \
"add r27, r1 \n\t" \
"adc %A0, r26 \n\t" \
"adc %B0, r26 \n\t" \
"lsr r27 \n\t" \
"adc %A0, r26 \n\t" \
"adc %B0, r26 \n\t" \
"clr r1 \n\t" \
: \
"=&r" (intRes) \
: \
"d" (longIn1), \
"d" (longIn2) \
: \
"r26" , "r27" \
)

// Some useful constants

#define ENABLE_STEPPER_DRIVER_INTERRUPT()  TIMSK1 |= (1<<OCIE1A)
#define DISABLE_STEPPER_DRIVER_INTERRUPT() TIMSK1 &= ~(1<<OCIE1A)


void checkHitEndstops()
{
 if( endstop_x_hit || endstop_y_hit || endstop_z_hit) {
   SERIAL_ECHO_START;
   SERIAL_ECHOPGM(MSG_ENDSTOPS_HIT);
   if(endstop_x_hit) {
     SERIAL_ECHOPAIR(" X:",(float)endstops_trigsteps[X_AXIS]/axis_steps_per_unit[X_AXIS]);
   }
   if(endstop_y_hit) {
     SERIAL_ECHOPAIR(" Y:",(float)endstops_trigsteps[Y_AXIS]/axis_steps_per_unit[Y_AXIS]);
   }
   if(endstop_z_hit) {
     SERIAL_ECHOPAIR(" Z:",(float)endstops_trigsteps[RZ_AXIS]/axis_steps_per_unit[RZ_AXIS]);
   }
   SERIAL_ECHOLN("");
   endstop_x_hit=false;
   endstop_y_hit=false;
   endstop_z_hit=false;
 }
}

void endstops_hit_on_purpose()
{
  endstop_x_hit=false;
  endstop_y_hit=false;
  endstop_z_hit=false;
}

void enable_endstops(bool check)
{
  check_endstops = check;
}

//         __________________________
//        /|                        |\     _________________         ^
//       / |                        | \   /|               |\        |
//      /  |                        |  \ / |               | \       s
//     /   |                        |   |  |               |  \      p
//    /    |                        |   |  |               |   \     e
//   +-----+------------------------+---+--+---------------+----+    e
//   |               BLOCK 1            |      BLOCK 2          |    d
//
//                           time ----->
// 
//  The trapezoid is the shape the speed curve over time. It starts at block->initial_rate, accelerates 
//  first block->accelerate_until step_events_completed, then keeps going at constant speed until 
//  step_events_completed reaches block->decelerate_after after which it decelerates until the trapezoid generator is reset.
//  The slope of acceleration is calculated with the leib ramp alghorithm.

void st_wake_up() {
  //  TCNT1 = 0;
  ENABLE_STEPPER_DRIVER_INTERRUPT();  
}

void step_wait(){
    for(int8_t i=0; i < 6; i++){
    }
}
  

FORCE_INLINE unsigned short calc_timer(unsigned short step_rate) {
  unsigned short timer;
  if(step_rate > MAX_STEP_FREQUENCY) step_rate = MAX_STEP_FREQUENCY;
  
  if(step_rate > 20000) { // If steprate > 20kHz >> step 4 times
    step_rate = (step_rate >> 2)&0x3fff;
    step_loops = 4;
  }
  else if(step_rate > 10000) { // If steprate > 10kHz >> step 2 times
    step_rate = (step_rate >> 1)&0x7fff;
    step_loops = 2;
  }
  else {
    step_loops = 1;
  } 
  
  if(step_rate < (F_CPU/500000)) step_rate = (F_CPU/500000);
  step_rate -= (F_CPU/500000); // Correct for minimal speed
  if(step_rate >= (8*256)){ // higher step rate 
    unsigned short table_address = (unsigned short)&speed_lookuptable_fast[(unsigned char)(step_rate>>8)][0];
    unsigned char tmp_step_rate = (step_rate & 0x00ff);
    unsigned short gain = (unsigned short)pgm_read_word_near(table_address+2);
    MultiU16X8toH16(timer, tmp_step_rate, gain);
    timer = (unsigned short)pgm_read_word_near(table_address) - timer;
  }
  else { // lower step rates
    unsigned short table_address = (unsigned short)&speed_lookuptable_slow[0][0];
    table_address += ((step_rate)>>1) & 0xfffc;
    timer = (unsigned short)pgm_read_word_near(table_address);
    timer -= (((unsigned short)pgm_read_word_near(table_address+2) * (unsigned char)(step_rate & 0x0007))>>3);
  }
  if(timer < 100) { timer = 100; MYSERIAL.print(MSG_STEPPER_TO_HIGH); MYSERIAL.println(step_rate); }//(20kHz this should never happen)
  return timer;
}

// Initializes the trapezoid generator from the current block. Called whenever a new 
// block begins.
FORCE_INLINE void trapezoid_generator_reset() {
  #ifdef ADVANCE
    advance = current_block->initial_advance;
    final_advance = current_block->final_advance;
    // Do E steps + advance steps
    lz_steps[current_block->active_extruder] += ((advance >>8) - old_advance);
    old_advance = advance >>8;  
  #endif
  deceleration_time = 0;
  // step_rate to timer interval
  OCR1A_nominal = calc_timer(current_block->nominal_rate);
  acc_step_rate = current_block->initial_rate;
  acceleration_time = calc_timer(acc_step_rate);
  OCR1A = acceleration_time;
  
//    SERIAL_ECHO_START;
//    SERIAL_ECHOPGM("advance :");
//    SERIAL_ECHO(current_block->advance/256.0);
//    SERIAL_ECHOPGM("advance rate :");
//    SERIAL_ECHO(current_block->advance_rate/256.0);
//    SERIAL_ECHOPGM("initial advance :");
//  SERIAL_ECHO(current_block->initial_advance/256.0);
//    SERIAL_ECHOPGM("final advance :");
//    SERIAL_ECHOLN(current_block->final_advance/256.0);
    
}

// "The Stepper Driver Interrupt" - This timer interrupt is the workhorse.  
// It pops blocks from the block_buffer and executes them by pulsing the stepper pins appropriately. 
ISR(TIMER1_COMPA_vect)
{    
  // If there is no current block, attempt to pop one from the buffer
  if (current_block == NULL) {
    // Anything in the buffer?
    current_block = plan_get_current_block();
    if (current_block != NULL) {
      current_block->busy = true;
      trapezoid_generator_reset();
      counter_x = -(current_block->step_event_count >> 1);
      counter_y = counter_x;
      counter_rz = counter_x;
      counter_lz = counter_x;
      step_events_completed = 0; 
      
      #ifdef Z_LATE_ENABLE 
        if(current_block->steps_rz > 0) {
          enable_rz();
          OCR1A = 2000; //1ms wait
          return;
        }
      #endif
      
//      #ifdef ADVANCE
//      lz_steps[current_block->active_extruder] = 0;
//      #endif
    } 
    else {
        OCR1A=2000; // 1kHz.
    }    
  } 

  if (current_block != NULL) {
    // Set directions TO DO This should be done once during init of trapezoid. Endstops -> interrupt
    out_bits = current_block->direction_bits;

    // Set direction en check limit switches
    if ((out_bits & (1<<X_AXIS)) != 0) {   // stepping along -X axis
      count_direction[X_AXIS]=-1;
    }
    else { // +direction 
      count_direction[X_AXIS]=1;
    }

    if ((out_bits & (1<<Y_AXIS)) != 0) {   // -direction
      count_direction[Y_AXIS]=-1;
    }
    else { // +direction
      count_direction[Y_AXIS]=1;
    }

    if ((out_bits & (1<<RZ_AXIS)) != 0) {   // -direction
      WRITE(RZ_DIR_PIN,INVERT_RZ_DIR);
      
      count_direction[RZ_AXIS]=-1;
      CHECK_ENDSTOPS
      {
        #if Z_MIN_PIN > -1
          bool z_min_endstop=(READ(Z_MIN_PIN) != Z_ENDSTOPS_INVERTING);
          if(z_min_endstop && old_z_min_endstop && (current_block->steps_rz > 0)) {
            endstops_trigsteps[RZ_AXIS] = count_position[RZ_AXIS];
            endstop_z_hit=true;
            step_events_completed = current_block->step_event_count;
          }
          old_z_min_endstop = z_min_endstop;
        #endif
      }
    }
    else { // +direction
      WRITE(RZ_DIR_PIN,!INVERT_RZ_DIR);

      count_direction[RZ_AXIS]=1;
      CHECK_ENDSTOPS
      {
        #if Z_MAX_PIN > -1
          bool z_max_endstop=(READ(Z_MAX_PIN) != Z_ENDSTOPS_INVERTING);
          if(z_max_endstop && old_z_max_endstop && (current_block->steps_rz > 0)) {
            endstops_trigsteps[RZ_AXIS] = count_position[RZ_AXIS];
            endstop_z_hit=true;
            step_events_completed = current_block->step_event_count;
          }
          old_z_max_endstop = z_max_endstop;
        #endif
      }
    }

    #ifndef ADVANCE
      if ((out_bits & (1<<LZ_AXIS)) != 0) {  // -direction
        REV_LZ_DIR();
        count_direction[LZ_AXIS]=-1;
      }
      else { // +direction
        NORM_LZ_DIR();
        count_direction[LZ_AXIS]=1;
      }
    #endif //!ADVANCE
    

    
    for(int8_t i=0; i < step_loops; i++) { // Take multiple steps per interrupt (For high speed moves) 
      #if !defined(__AVR_AT90USB1286__) && !defined(__AVR_AT90USB1287__)
      MSerial.checkRx(); // Check for serial chars.
      #endif 
      
      #ifdef ADVANCE
      counter_lz += current_block->steps_lz;
      if (counter_lz > 0) {
        counter_lz -= current_block->step_event_count;
        if ((out_bits & (1<<LZ_AXIS)) != 0) { // - direction
          lz_steps[current_block->active_extruder]--;
        }
        else {
          lz_steps[current_block->active_extruder]++;
        }
      }    
      #endif //ADVANCE
      
   #if OPENSL_PRINT_MODE == 0
      
      
      #if !defined COREXY      
      counter_x += current_block->steps_x;
      if (counter_x > 0) {
        counter_x -= current_block->step_event_count;
        count_position[X_AXIS]+=count_direction[X_AXIS];   
        update_X_galvo(count_direction[X_AXIS]);
      }

      counter_y += current_block->steps_y;
      if (counter_y > 0) {
        counter_y -= current_block->step_event_count;
        count_position[Y_AXIS]+=count_direction[Y_AXIS]; 
        update_Y_galvo(count_direction[Y_AXIS]);
      }
      #endif
  
      #ifdef COREXY
        counter_x += current_block->steps_x;        
        counter_y += current_block->steps_y;
        
        if ((counter_x > 0)&&!(counter_y>0)){  //X step only
          counter_x -= current_block->step_event_count; 
          count_position[X_AXIS]+=count_direction[X_AXIS];   
          update_X_galvo(count_direction[X_AXIS]);
          
        }
        
        if (!(counter_x > 0)&&(counter_y>0)){  //Y step only
          counter_y -= current_block->step_event_count; 
          count_position[Y_AXIS]+=count_direction[Y_AXIS];
          update_Y_galvo(count_direction[Y_AXIS]);
        }        
        
        if ((counter_x > 0)&&(counter_y>0)){  //step in both axes
          if (((out_bits & (1<<X_AXIS)) == 0)^((out_bits & (1<<Y_AXIS)) == 0)){  //X and Y in different directions
            counter_x -= current_block->step_event_count;  
            //step_wait();
            count_position[X_AXIS]+=count_direction[X_AXIS];
            count_position[Y_AXIS]+=count_direction[Y_AXIS];
            update_X_galvo(count_direction[X_AXIS]);
            update_Y_galvo(count_direction[Y_AXIS]);
            counter_y -= current_block->step_event_count;
          }
          else{  //X and Y in same direction
            counter_x -= current_block->step_event_count;    
            //step_wait();
            count_position[X_AXIS]+=count_direction[X_AXIS];
            count_position[Y_AXIS]+=count_direction[Y_AXIS];
            update_X_galvo(count_direction[X_AXIS]);
            update_Y_galvo(count_direction[Y_AXIS]);
            counter_y -= current_block->step_event_count;    
          }
        }
      #endif //corexy

   #else
      //Scanning X&Y With Galvos!
        
      unsigned long old_x = Galvo_WorldXPosition;
      unsigned long old_y = Galvo_WorldYPosition;
      
      Galvo_WorldXPosition = Galvo_WorldXPosition + (count_direction[X_AXIS] * current_block->steps_x);
      Galvo_WorldYPosition = Galvo_WorldYPosition + (count_direction[Y_AXIS] * current_block->steps_y);
      
      scan_X_Y_galvo(old_x, old_y, Galvo_WorldXPosition, Galvo_WorldYPosition);
      
      return;
   #endif //OPENSL_PRINT_MODE
      
      counter_rz += current_block->steps_rz;
      if (counter_rz > 0) {
        WRITE(RZ_STEP_PIN, !INVERT_RZ_STEP_PIN);
        
        counter_rz -= current_block->step_event_count;
        count_position[RZ_AXIS]+=count_direction[RZ_AXIS];
        WRITE(RZ_STEP_PIN, INVERT_RZ_STEP_PIN);
        
      }

      #ifndef ADVANCE
        counter_lz += current_block->steps_lz;
        if (counter_lz > 0) {
          WRITE_LZ_STEP(!INVERT_LZ_STEP_PIN);
          counter_lz -= current_block->step_event_count;
          count_position[LZ_AXIS]+=count_direction[LZ_AXIS];
          WRITE_LZ_STEP(INVERT_LZ_STEP_PIN);
        }
      #endif //!ADVANCE
      step_events_completed += 1;  
      if(step_events_completed >= current_block->step_event_count) break;
    }
    // Calculare new timer value
    unsigned short timer;
    unsigned short step_rate;
    if (step_events_completed <= (unsigned long int)current_block->accelerate_until) {
      
      MultiU24X24toH16(acc_step_rate, acceleration_time, current_block->acceleration_rate);
      acc_step_rate += current_block->initial_rate;
      
      // upper limit
      if(acc_step_rate > current_block->nominal_rate)
        acc_step_rate = current_block->nominal_rate;

      // step_rate to timer interval
      timer = calc_timer(acc_step_rate);
      OCR1A = timer;
      acceleration_time += timer;
      #ifdef ADVANCE
        for(int8_t i=0; i < step_loops; i++) {
          advance += advance_rate;
        }
        //if(advance > current_block->advance) advance = current_block->advance;
        // Do E steps + advance steps
        lz_steps[current_block->active_extruder] += ((advance >>8) - old_advance);
        old_advance = advance >>8;  
        
      #endif
    } 
    else if (step_events_completed > (unsigned long int)current_block->decelerate_after) {   
      MultiU24X24toH16(step_rate, deceleration_time, current_block->acceleration_rate);
      
      if(step_rate > acc_step_rate) { // Check step_rate stays positive
        step_rate = current_block->final_rate;
      }
      else {
        step_rate = acc_step_rate - step_rate; // Decelerate from aceleration end point.
      }

      // lower limit
      if(step_rate < current_block->final_rate)
        step_rate = current_block->final_rate;

      // step_rate to timer interval
      timer = calc_timer(step_rate);
      OCR1A = timer;
      deceleration_time += timer;
      #ifdef ADVANCE
        for(int8_t i=0; i < step_loops; i++) {
          advance -= advance_rate;
        }
        if(advance < final_advance) advance = final_advance;
        // Do E steps + advance steps
        lz_steps[current_block->active_extruder] += ((advance >>8) - old_advance);
        old_advance = advance >>8;  
      #endif //ADVANCE
    }
    else {
      OCR1A = OCR1A_nominal;
    }

    // If current block is finished, reset pointer 
    if (step_events_completed >= current_block->step_event_count) {
      current_block = NULL;
      plan_discard_current_block();
    }   
  } 
}

#ifdef ADVANCE
  unsigned char old_OCR0A;
  // Timer interrupt for E. lz_steps is set in the main routine;
  // Timer 0 is shared with millies
  ISR(TIMER0_COMPA_vect)
  {
    old_OCR0A += 52; // ~10kHz interrupt (250000 / 26 = 9615kHz)
    OCR0A = old_OCR0A;
    // Set E direction (Depends on E direction + advance)
    for(unsigned char i=0; i<4;i++) {
      if (lz_steps[0] != 0) {
        WRITE(LZ_STEP_PIN, INVERT_LZ_STEP_PIN);
        if (lz_steps[0] < 0) {
          WRITE(LZ_DIR_PIN, INVERT_LZ_DIR);
          lz_steps[0]++;
          WRITE(LZ_STEP_PIN, !INVERT_LZ_STEP_PIN);
        } 
        else if (lz_steps[0] > 0) {
          WRITE(LZ_DIR_PIN, !INVERT_LZ_DIR);
          lz_steps[0]--;
          WRITE(LZ_STEP_PIN, !INVERT_LZ_STEP_PIN);
        }
      }
    }
  }
#endif // ADVANCE

void st_init()
{
  //Initialize Dir Pins
  #if RZ_DIR_PIN > -1 
    SET_OUTPUT(RZ_DIR_PIN);
  #endif
  #if LZ_DIR_PIN > -1 
    SET_OUTPUT(LZ_DIR_PIN);
  #endif

  //Initialize Enable Pins - steppers default to disabled.
  #if (RZ_ENABLE_PIN > -1)
    SET_OUTPUT(RZ_ENABLE_PIN);
    if(!RZ_ENABLE_ON) WRITE(RZ_ENABLE_PIN,HIGH);
  #endif
  #if (LZ_ENABLE_PIN > -1)
    SET_OUTPUT(LZ_ENABLE_PIN);
    if(!LZ_ENABLE_ON) WRITE(LZ_ENABLE_PIN,HIGH);
  #endif
  
  //endstops and pullups
  
    #if Z_MIN_PIN > -1
      SET_INPUT(Z_MIN_PIN); 
    #ifdef ENDSTOPPULLUP_ZMIN
      WRITE(Z_MIN_PIN,HIGH);
    #endif
    #endif
      
    #if Z_MAX_PIN > -1
      SET_INPUT(Z_MAX_PIN); 
    #ifdef ENDSTOPPULLUP_ZMAX
      WRITE(Z_MAX_PIN,HIGH);
    #endif
  #endif
 

  //Initialize Step Pins
  #if (RZ_STEP_PIN > -1) 
    SET_OUTPUT(RZ_STEP_PIN);
    WRITE(RZ_STEP_PIN,INVERT_RZ_STEP_PIN);
    if(!RZ_ENABLE_ON) WRITE(RZ_ENABLE_PIN,HIGH);
    
  #endif  
  #if (LZ_STEP_PIN > -1) 
    SET_OUTPUT(LZ_STEP_PIN);
    WRITE(LZ_STEP_PIN,INVERT_LZ_STEP_PIN);
    if(!LZ_ENABLE_ON) WRITE(LZ_ENABLE_PIN,HIGH);
  #endif  

  #ifdef CONTROLLERFAN_PIN
    SET_OUTPUT(CONTROLLERFAN_PIN); //Set pin used for driver cooling fan
  #endif
  
  // waveform generation = 0100 = CTC
  TCCR1B &= ~(1<<WGM13);
  TCCR1B |=  (1<<WGM12);
  TCCR1A &= ~(1<<WGM11); 
  TCCR1A &= ~(1<<WGM10);

  // output mode = 00 (disconnected)
  TCCR1A &= ~(3<<COM1A0); 
  TCCR1A &= ~(3<<COM1B0); 
  
  // Set the timer pre-scaler
  // Generally we use a divider of 8, resulting in a 2MHz timer
  // frequency on a 16MHz MCU. If you are going to change this, be
  // sure to regenerate speed_lookuptable.h with
  // create_speed_lookuptable.py
  TCCR1B = (TCCR1B & ~(0x07<<CS10)) | (2<<CS10);

  OCR1A = 0x4000;
  TCNT1 = 0;
  ENABLE_STEPPER_DRIVER_INTERRUPT();  

  #ifdef ADVANCE
  #if defined(TCCR0A) && defined(WGM01)
    TCCR0A &= ~(1<<WGM01);
    TCCR0A &= ~(1<<WGM00);
  #endif  
    lz_steps[0] = 0;
    lz_steps[1] = 0;
    lz_steps[2] = 0;
    TIMSK0 |= (1<<OCIE0A);
  #endif //ADVANCE
  
  enable_endstops(true); // Start with endstops active. After homing they can be disabled
  sei();
}


// Block until all buffered steps are executed
void st_synchronize()
{
    while( blocks_queued()) {
    manage_inactivity();
  }
}

void digitalPotWrite(int channel, int value) {
  // take the SS pin low to select the chip:
  digitalWrite(GALVO_SS_PIN,LOW);
  //  send in the address and value via SPI:
  SPI.transfer(channel);
  SPI.transfer(value);
  // take the SS pin high to de-select the chip:
  digitalWrite(GALVO_SS_PIN,HIGH);
}

void scan_X_Y_galvo(unsigned long x1, unsigned long y1, unsigned long x2, unsigned long y2)
{
 //  unsigned long x_dist_sq_mm = ((x2-x1)*(x2-x1) * XY_GALVO_SCALAR) / axis_steps_per_unit[X_AXIS]; 
 //  unsigned long y_dist_sq_mm = ((y2-y1)*(y2-y1) * XY_GALVO_SCALAR) / axis_steps_per_unit[Y_AXIS];
 //  double scan_dist_mm = sqrt(x_dist_sq_mm + y_dist_sq_mm);
   
   unsigned long scan_time_ms = 1000; //ceil(scan_dist_mm * OPENSL_SCAN_TIME_MS_PER_MM);
   if(scan_time_ms == 0)
   {
     scan_time_ms = 1;
   }
   
   unsigned long startTime = millis();
   
   boolean first_point = true;
   while(millis() < startTime + 1000)
   {
     if(first_point)
     {
        coordinate_XY_move(x1, y1);
        first_point = false;
     }
     else
     {
        coordinate_XY_move(x2, y2);
        first_point = true;
     }
   }
}
void update_X_galvo(int step_dir)
{
   Galvo_WorldXPosition+=step_dir;  
   unsigned short s = (unsigned short)Galvo_WorldXPosition;
   if(Galvo_WorldXPosition > 0x0000FEFF)
   {
      s = 0x0000FEFF;
   }
   move_X_galvo(s);
}

void update_Y_galvo(int step_dir)
{
   Galvo_WorldYPosition+=step_dir;
   unsigned short s = (unsigned short)Galvo_WorldYPosition;
   if(Galvo_WorldYPosition > 0x0000FEFF)
   {
      s = 0x0000FEFF;
   }
   
   move_Y_galvo(s);
}

void set_galvo_pos(unsigned long X, unsigned long Y)
{
   Galvo_WorldYPosition = X;
   Galvo_WorldYPosition = Y;
}

void move_galvos(unsigned long X, unsigned long Y)
{
  
   unsigned short sX = (unsigned short)X;
   if(X > 0x0000FEFF)
   {
      sX = 0x0000FEFF;
   }
   
   unsigned short sY = (unsigned short)Y;
   if(Y > 0x0000FEFF)
   {
      sY = 0x0000FEFF;
   }
  move_X_galvo(sX);
  move_Y_galvo(sY);
}

void coordinate_XY_move(unsigned long X, unsigned long Y)
{
  
   unsigned short sX = (unsigned short)X;
   if(X > 0x0000FEFF)
   {
      sX = 0x0000FEFF;
   }
   
   unsigned short sY = (unsigned short)Y;
   if(Y > 0x0000FEFF)
   {
      sY = 0x0000FEFF;
   }
   
  unsigned char xHigh = (((sX*XY_GALVO_SCALAR) & 0xFF00) >> 8);
  unsigned char xLow  = (sX*XY_GALVO_SCALAR) & 0x00FF;
  
  if(xHigh == 0xFF)
  {
     xHigh = 0xFE;
     xLow = 0xFF;
  }
     
  
  unsigned char yHigh = ((sY*XY_GALVO_SCALAR)  & 0xFF00) >> 8;
  unsigned char yLow  = (sY*XY_GALVO_SCALAR)  & 0x00FF;
  
  if(yHigh == 0xFF)
  {
     yHigh = 0xFE;
     yLow = 0xFF;
  }  
  digitalPotWrite(0, xHigh);
  digitalPotWrite(1, yHigh);
  digitalPotWrite(2, xHigh+1);
  digitalPotWrite(3, yHigh+1);
  digitalPotWrite(4, xLow);
  digitalPotWrite(5, yLow);
}

void timed_refresh_of_galvos(void)
{
  update_X_galvo(Galvo_WorldXPosition);
  update_Y_galvo(Galvo_WorldYPosition);
}

void move_X_galvo(unsigned short X)
{
  unsigned char xHigh = (((X*XY_GALVO_SCALAR) & 0xFF00) >> 8);
  unsigned char xLow  = (X*XY_GALVO_SCALAR) & 0x00FF;
  
  if(xHigh == 0xFF)
  {
     xHigh = 0xFE;
     xLow = 0xFF;
  }
     
  digitalPotWrite(0, xHigh);
  digitalPotWrite(2, xHigh+1);
  digitalPotWrite(4, xLow);
}

void move_Y_galvo(unsigned short Y)
{
  unsigned char yHigh = ((Y*XY_GALVO_SCALAR)  & 0xFF00) >> 8;
  unsigned char yLow  = (Y*XY_GALVO_SCALAR)  & 0x00FF;
  
  if(yHigh == 0xFF)
  {
     yHigh = 0xFE;
     yLow = 0xFF;
  }
  
  
  digitalPotWrite(1, yHigh);
  digitalPotWrite(3, yHigh+1);
  digitalPotWrite(5, yLow);
}

void st_set_position(const long &x, const long &y, const long &rz, const long &lz)
{
  CRITICAL_SECTION_START;
  count_position[X_AXIS] = x;
  count_position[Y_AXIS] = y;
  count_position[RZ_AXIS] = rz;
  count_position[LZ_AXIS] = lz;
  CRITICAL_SECTION_END;
}

void st_set_e_position(const long &lz)
{
  CRITICAL_SECTION_START;
  count_position[LZ_AXIS] = lz;
  CRITICAL_SECTION_END;
}

long st_get_position(uint8_t axis)
{
  long count_pos;
  CRITICAL_SECTION_START;
  count_pos = count_position[axis];
  CRITICAL_SECTION_END;
  return count_pos;
}

void finishAndDisableSteppers()
{
  st_synchronize(); 
  disable_x(); 
  disable_y(); 
  disable_rz(); 
  disable_lz(); 
}

void quickStop()
{
  DISABLE_STEPPER_DRIVER_INTERRUPT();
  while(blocks_queued())
    plan_discard_current_block();
  current_block = NULL;
  ENABLE_STEPPER_DRIVER_INTERRUPT();
}


