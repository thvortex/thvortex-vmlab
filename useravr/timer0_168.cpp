// =============================================================================
// ATMega168 "TIMER0" peripheral.  AMcTools, VMLAB 3.14E test release July-09
//
// This is just a preliminary code for the ATMega168 TIMER0 peripheral, to
// show the use of the new API release. This could be valid also for
// Mega48 and Mega88, with minor modifications
//
#include <windows.h>
#include <commctrl.h>
#pragma hdrstop
#include <stdio.h>

#define IS_PERIPHERAL       // To distinguish from a normal user component
#include "C:\VMLAB\bin\blackbox.h"
#include "useravr.h"

int WINAPI DllEntryPoint(HINSTANCE, unsigned long, void*) {return 1;} // is DLL
// =============================================================================

// Involved ports. Keep same order as in .INI file "Port_map = ..." who
// does the actual assignment to micro ports PD0, etc. This allows multiple instances
// to be mapped into different port sets
//
DECLARE_PINS            
   MICRO_PORT(OCA, 1)     // Output Compare A  
   MICRO_PORT(OCB, 2)     // Output Compare B
   MICRO_PORT(XCLK, 3)    // External clock 
END_PINS

// Coding of work modes, status, clock sources, etc
//
enum {CLK_STOP, CLK_INTERNAL, CLK_EXT_FALL, CLK_EXT_RISE, CLK_UNKNOWN};
enum {ACT_NONE, ACT_TOGGLE, ACT_CLEAR, ACT_SET, ACT_RESERVED}; // Actions on compare
enum {WAVE_NORMAL, WAVE_PWM_PC, WAVE_CTC, WAVE_PWM_FAST, WAVE_RESERVED, WAVE_UNKNOWN};
enum {DISBL_BY_SLEEP = 1, DISBL_BY_PRR = 2, DISBL_BY_TSM = 4}; // Disable flags. Combine by ORing
enum {DEBUG_LOG = 1, DEBUG_FUTURE1 = 2, DEBUG_FUTURE2 = 4}; // For Debug. To combine by ORing
enum {VAL_NONE, VAL_00, VAL_FF, VAL_OCRA}; // Counter TOP/overflow value selected by WGM bits

// Involved registers. Use same order as in .INI file "Register_map"
// These are IDs or indexes. The real data is stored in a hidden WORD8
// type array. (see "blackbox.h" for details). The WORD8 data is referred
// with the REG(xxx) macro.
//
DECLARE_REGISTERS
   TCCR0A, TCCR0B, TCNT0, OCR0A, OCR0B
END_REGISTERS

// Involved interrupts. Use same order as in .INI file "Interrupt_map"
// Like in  registers, these are IDs; what is important is the order.
// It is "Interupt_map" who assigns the index into the actual interrupt name, 
// therefore, different instances can use a different interrupt set
//
DECLARE_INTERRUPTS
   CMPA, CMPB, OVF
END_INTERRUPTS 

// To insure that more than a timer instance can be placed, declare
// variables here. Some #defines below hide the VAR() use, making the
// code more readable
//
DECLARE_VAR
   int _Clock_source;      // Internal, external, ...
   int _Disabled;          // A combination of DISBL_x constants, saying why
   int _Prescaler_index;   // The selected prescaler
   int _Timer_period;      // In clock cycles
   BOOL _Counting_up;      // For up-down management
   int _Tick_signature;    // For REMIND_ME2, to validate ticks
   int _Waveform;          // Says if PWM, normal, CTC, etc
   int _Top;               // Counter TOP value (0xFF, OCRA, etc)
   int _Overflow;          // Counter value that causes overflow interrupt
   int _Update_OCR;        // Counter value at which to update OCR double-buffer
   WORD8 _OCR0A_buffer;    // For OCR double buffering in PWM modes
   WORD8 _OCR0B_buffer;    //
   int _Action_comp_A;     // To code the actions to be performed at
   int _Action_comp_B;     // output compare
   int _Action_top_A;      //
   int _Action_top_B;
   int _Last_PSR;          // Clock cycle in which last PSRSYNC/PSRASY occurred
   BOOL _Compare_blocked;  // Writing TCNT0 blocks output compare for one tick
   BOOL _TSM;              // Counter paused while TSM bit set in GTCCR
   int _Debug;             // To store debugging options
END_VAR
#define Clock_source VAR(_Clock_source)     // To simplify readability...
#define Disabled VAR(_Disabled)
#define Prescaler_index VAR(_Prescaler_index)
#define Timer_period VAR(_Timer_period)
#define Counting_up VAR(_Counting_up)
#define Tick_signature VAR(_Tick_signature)
#define Waveform VAR(_Waveform)
#define Top VAR(_Top)
#define Overflow VAR(_Overflow)
#define Update_OCR VAR(_Update_OCR)
#define OCR0A_buffer VAR(_OCR0A_buffer)
#define OCR0B_buffer VAR(_OCR0B_buffer)
#define Action_comp_A VAR(_Action_comp_A)
#define Action_comp_B VAR(_Action_comp_B)
#define Action_top_A  VAR(_Action_top_A)
#define Action_top_B  VAR(_Action_top_B)
#define Last_PSR VAR(_Last_PSR)
#define Compare_blocked VAR(_Compare_blocked)
#define TSM VAR(_TSM)
#define Debug VAR(_Debug)

// Some static tables for clock prescaling and timer mode display
//
const int Prescaler_table[] =  {0, 1, 8, 64, 256, 1024};
const char *Prescaler_text[] = {"", "/ 1", "/ 8", "/ 64", "/ 256", "/ 1024" };
const char *Clock_text[] = { "Stop", "Internal", "External (Fall)", "External (Rise)", "?",};
const char *Wave_text[] = {"Normal", "PWM PC", "CTC", "PWM Fast", "Reserved", "?"};
const char *Top_text[] = {"?", "$00", "$FF", "OCRA" };
const char *Action_text[] = {"Disconnected", "Toggle", "Clear", "Set", "Reserved"};

USE_WINDOW(WINDOW_USER_1); // Window to display registers, etc. See .RC file

REGISTERS_VIEW
//           ID     .RC ID    b7        ...   bit names   ...             b0
   DISPLAY(TCCR0A, GADGET1, COM0A1, COM0A0, COM0B1, COM0B0, *, *, WGM01, WGM00)
   DISPLAY(TCCR0B, GADGET3, FOC0A, FOC0B, *, *, WGM02, CS02, CS01, CS00)
   DISPLAY(TCNT0,  GADGET2, *, *, *, *, *, *, *, *)
   DISPLAY(OCR0A,  GADGET4, *, *, *, *, *, *, *, *)
   DISPLAY(OCR0B,  GADGET5, *, *, *, *, *, *, *, *)  

END_VIEW

void Go();                      // Function prototypes
void Count();                   //
void Update_waveform();
void Update_compare_actions();
void Update_clock_source();
void Update_display();
void Action_on_port(PORT, int, BOOL pMode = Counting_up);
void Log(const char *pFormat, ...);
int Value(int);

//==============================================================================
// Callback functions, On_xxx(...)
//==============================================================================

const char *On_create()                   // Mandatory function
{
   Debug = DEBUG_LOG; // TODO: Replace with 0
   return NULL;
}
void On_destroy()                         //     "        "
{
}
void On_simulation_begin()                //     "        "
{
   //TRACE(true);  // Uncomment for tracing
}

void On_simulation_end()
//**********************
{
   FOREACH_REGISTER(j){
      REG(j) = WORD8(0,0);      // All bits unknown (X)
   }
   OCR0A_buffer.x = 0;
   OCR0B_buffer.x = 0;
   Disabled = 0;
   Clock_source = CLK_UNKNOWN;
   Waveform = WAVE_UNKNOWN;
   Top = VAL_NONE;
   Prescaler_index = 0;
   Update_OCR = VAL_NONE;
   Update_display();
}

WORD8 *On_register_read(REGISTER_ID pId)
//**************************************
// Notification about a read operation over the given register. This is only
// necessary if any kind of on-read action is needed: clear-on-read,
// temporary buffer read, etc. Otherwise it can be omitted (this case)
{
   // If OCRx double buffering is in effect, the CPU always reads the buffer
   // instead of the real registers.
   if(Update_OCR) {
      switch(pId) {
         case OCR0A:
            return &OCR0A_buffer;
         case OCR0B:
            return &OCR0B_buffer;
         default:
            break;
      }
   }

   return NULL; // The normal register address will be read
}

void On_register_write(REGISTER_ID pId, WORD8 pData)
//**************************************************
// The micro is writing pData into the pId register This notification
// allows to perform all the derived operations.
{
// Register selection macros include the "case", X bits check and logging.
#define case_register(r, m) \
   case r: {\
      if((pData.x & m) != m) { \
         WARNING("Unknown bits (X) written into "#r" register", CAT_MEMORY, WARN_MEMORY_WRITE_X_IO); \
         Log("Write register "#r": $??"); \
      } else { \
         Log("Write register "#r": $%02X", pData.d & m); \
      }

#define end_register } break;

   switch(pId) {

       case_register(TCCR0A, 0xF3)      // Timer Control Register A
       //---------------------------------------------------------
          REG(TCCR0A) = pData & 0xF3;   // $F3 = read-only bits mask
          Update_waveform();
          Update_compare_actions();
          Update_display();
       end_register

       case_register(TCCR0B, 0xCF)      // Timer Control Register B
       //---------------------------------------------------------

          REG(TCCR0B) = pData & 0x0F;   // FOC0A/FOC0B are write-only
          Update_waveform();
          Update_compare_actions();
          Update_clock_source();
          Update_display();

          // Handle FOC0A, FOC0B (Force Output Compare), only in non-PWM modes
          // Since writing TCCR0B could have changed the waveform mode and
          // compare actions, we do this check after the modes are updated.
          if(pData[7] == 1) {       // FOC0A = bit 7
             if(Waveform == WAVE_NORMAL || Waveform == WAVE_CTC)
                Log("OC0A force compare");
                Action_on_port(OCA, Action_comp_A);
          }
          if(pData[6] == 1) {       // FOC0B = bit 6
             if(Waveform == WAVE_NORMAL || Waveform == WAVE_CTC)
                Log("OC0B force compare");
                Action_on_port(OCB, Action_comp_B);
          }

       end_register

       case_register(TCNT0, 0xFF) // Timer Counter Register
       //--------------------------------------------------
          REG(TCNT0) = pData;
          Compare_blocked = true;
          if(Clock_source != CLK_STOP && Clock_source != CLK_UNKNOWN)
             WARNING("TCNT0 modified while running", CAT_TIMER, WARN_PARAM_BUSY);
       end_register

       case_register(OCR0A, 0xFF)  // Output Compare Register A
       //------------------------------------------------------
          OCR0A_buffer = pData;    // All bits r/w no mask necessary
          if(!Update_OCR) {
             REG(OCR0A) = pData;
          }
          Update_display(); // Buffer static control need update
       end_register

       case_register(OCR0B, 0xFF) // Output Compare Register B
       //------------------------------------------------------
          OCR0B_buffer = pData;
          if(!Update_OCR) {
             REG(OCR0B) = pData;
          }
          Update_display();
       end_register
   }
}

void On_remind_me(double pTime, int pAux)
//**************************************
// Response to REMIND_ME2() used implement the internal
// prescaled clock tick. The pAux parameter holds a signature value
// used to void pending ticks in case of prescaler reset
{
   if(Tick_signature != pAux)        // If need to void a pending tick
      return;
   if(Disabled)                      // If disabled by SLEEP, PPR, etc
      return;
   if(Clock_source != CLK_INTERNAL)  // Or clock source is no longer internal
      return;
   else if(TSM && Timer_period != 1) // TSM only holds prescaler not direct IO clock
      return;

   Count();        // Launch next tick (in clock cycles)
   Go();
}

void On_port_edge(PORT pPort, EDGE pEdge, double pTime)
//*****************************************************
// Handle external clock. Check the edge and call the
// general Count() procedure
{
   // TODO: Warn about transitions that are too fast for CPU clock? Datasheet recommends
   // max frequency clkIO/2.5. This is the Nyquist sampling frequency with some room for
   // clock variations.

   if(pPort != XCLK)  // External clock pin XCLK, defined in DECLARE_PINS
      return;
   if(Disabled)
      return;
   if(Clock_source == CLK_EXT_RISE && pEdge == RISE)
      Count();
   else if(Clock_source == CLK_EXT_FALL && pEdge == FALL)
      Count();
}

void On_reset(int pCause)
//***********************
// Initialize registers to the desired value. Use the FOREACH macro for
// a general initialization
{
   FOREACH_REGISTER(j) {
      REG(j)= 0;
   }
   OCR0A_buffer = 0;
   OCR0B_buffer = 0;
   Disabled = 0;
   Prescaler_index = 0;
   Counting_up = true;
   Tick_signature = 0;
   Timer_period = 1;
   Waveform = WAVE_NORMAL;
   Top = VAL_FF;
   Overflow = VAL_FF;
   Clock_source = CLK_STOP;
   Update_OCR = VAL_NONE;
   Action_comp_A = ACT_NONE;
   Action_comp_B = ACT_NONE;
   Action_top_A = ACT_NONE;
   Action_top_B = ACT_NONE;
   TAKEOVER_PORT(OCA, false);  // Release ports; a reset can happen
   TAKEOVER_PORT(OCB, false);  // at any time...
   Last_PSR = 0;
   Compare_blocked = false; // Prevent compare match immediately after reset 
   TSM = false;
   Update_display();
}

void On_notify(int pWhat)
//**********************
// Notification coming from some other DLL instance. Used here to
// handle the PRR register and prescaler reset (coming from dummy component)
{
   switch(pWhat) {
      case NTF_PRR0:                 // A 0 set in PPR register bit for TIMER 0
         Disabled &= ~DISBL_BY_PRR;  // Remove disable flag and go
         Log("Enabled by PRR");
         Update_display();
         Go();
         break;

      case NTF_PRR1:                 // A 1 set in PPR register bit for TIMER 0
         Log("Disabled by PRR");
         Disabled |= DISBL_BY_PRR;   // Add flag
         Update_display();
         break;

      case NTF_TSM:                  // Timer sync mode; prescaler reset continuously asserted
         Log("Started TSM");
         TSM = true;
         Update_display();
         break;

      case NTF_PSR:                                    // Prescaler reset. Record elapsed
         Last_PSR = GET_MICRO_INFO(INFO_CPU_CYCLES);   // CPU cycles to simulate the reset
         Log("Prescaler reset by PSRSYNC");            // and ensure timers stay synchronized
         
         if (TSM) {              // Restart counter if previously disabled by TSM
            Log("Finished TSM");         
            TSM = false;
            Update_display();
         }
         
         Go();
         break;
   }
}

void On_sleep(int pMode)
//*********************
// The micro has entered in SLEEP mode. Must disable the timer
// if the sleep mode applies
{
   switch(pMode) {
      case SLEEP_EXIT:                 // Micro wakes up. Remove the
         Disabled &= ~DISBL_BY_SLEEP;  // flag and go.
         Log("Exit from SLEEP");
         Update_display();
         Go();
         break;

      case SLEEP_IDLE:                 // This mode does not affect
         return;

      case SLEEP_STANDBY:              // Modes that disable this timer
      case SLEEP_POWERSAVE:
      case SLEEP_POWERDOWN:
      case SLEEP_NOISE_REDUCTION:
         Log("Disabled by SLEEP");
         Disabled |= DISBL_BY_SLEEP;   // Set the flag
         Update_display();
         break;
   }
   Update_display();
}

void On_gadget_notify(GADGET pGadget, int pCode)
//*********************************************
// Response to notification from "Log" checkbox and toggle flag
{
   Log("On_gadget_notify()");
#if 0
   if(pGadget == GADGET11 && pCode == BN_CLICKED) {
      Debug ^= DEBUG_LOG;
   }
#endif
}

//==============================================================================
// Internal functions (non-callback). Prototypes defined above
//==============================================================================

void Go()
//******
// Schedule the next timer tick if the counter is not disabled and using the
// internal clock source. Called on every timer tick, each time the counter
// is re-enabled, and each time the clock source changes. The Tick_signature
// is updated each item to cancel any already pending ticks which may no longer
// be valid.
{
   if(Disabled)
      return;      
   if(Clock_source == CLK_INTERNAL) {
      if(TSM && Timer_period != 1) // TSM only holds prescaler not direct IO clock
         return;
      unsigned int cycles = GET_MICRO_INFO(INFO_CPU_CYCLES) - Last_PSR;
      REMIND_ME2(Timer_period - (cycles % Timer_period), ++Tick_signature);
   }
}

void Action_on_port(PORT pPort, int pCode, BOOL pMode)
//*****************************************
// Do the action on the given port. Reverse it in PWM PC mode
// downcounting phase
{
   int rc = 0;

   switch(pCode) {
      case ACT_TOGGLE:
         Log("SET_PORT(%d, %d)", pPort, TOGGLE);
         rc = SET_PORT(pPort, TOGGLE); break;
      case ACT_SET:
         Log("SET_PORT(%d, %d)", pPort, pMode ? 1 : 0);
         rc = SET_PORT(pPort, pMode ? 1 : 0); break;
      case ACT_CLEAR:
         Log("SET_PORT(%d, %d)", pPort, pMode ? 0 : 1);
         rc = SET_PORT(pPort, pMode ? 0 : 1); break;
   }
   
   if(rc == PORT_NOT_OUTPUT) {
      if(pPort == OCA) {
         WARNING("OC0A enabled but pin not defined as output in DDR", CAT_TIMER,
            WARN_TIMERS_OUTPUT);
      } else if(pPort == OCB) {
         WARNING("OC0B enabled but pin not defined as output in DDR", CAT_TIMER,
            WARN_TIMERS_OUTPUT);
      }
   }   
}

void Count()
//*********
// Handles one timer count event. Performs a output compare operation,
// leading to the necessary actions depending on the timer configuration.
// All compare match outputs and compare/overflow flag registers are delayed
// by one timer cycle from when the match occured.
{
   // Do nothing on reserved/unknown waveform; we don't know the count sequence
   if(Waveform == WAVE_RESERVED || Waveform == WAVE_UNKNOWN) {
      return;
   }

   // Set overflow flag when count == TOP/MAX/BOTTOM depending on mode
   if(REG(TCNT0) == Value(Overflow)) {
      SET_INTERRUPT_FLAG(OVF, FLAG_SET);
   }

   // Reverse counter direction in PWM PC mode when it reaches either TOP or BOTTOM
   if(Waveform == WAVE_PWM_PC) {
      if(Counting_up && REG(TCNT0) == Value(Top)) {
         Counting_up = false;
      } else if(!Counting_up && REG(TCNT0) == 0) {
         Counting_up = true;
      }
   }

   // Update output pins if a compare output match occurs. A CPU write to TNCT0 blocks
   // any output compare in the next timer clock cycle even if the counter was stopped
   // at the time the TNCT0 write occurred.
   if(!Compare_blocked) {
      if(REG(TCNT0) == REG(OCR0A)) {
         Action_on_port(OCA, Action_comp_A);
         SET_INTERRUPT_FLAG(CMPA, FLAG_SET);
      }
      if(REG(TCNT0) == REG(OCR0B)) {
         Action_on_port(OCB, Action_comp_B);
         SET_INTERRUPT_FLAG(CMPB, FLAG_SET);
      }
   }
   Compare_blocked = false;

   // If in PWM PC mode and output compare values are outside of the counting range,
   // then force the output pin to a fixed value without generating compare match
   // interrupt
   // TODO: Double-check when REG(OCR0A) == TOP
   if(Waveform == WAVE_PWM_PC) {
      if(REG(OCR0A) == 0 || REG(OCR0A).d > Value(Top) || REG(OCR0B).x != 0xff) {
         Log("Special OCR0A CASE");
         Action_on_port(OCA, Action_comp_A, true);
      }
      if(REG(OCR0B) == 0 || REG(OCR0B).d > Value(Top) || REG(OCR0B).x != 0xff) {
         Log("Special OCR0B CASE");
         Action_on_port(OCB, Action_comp_B, true);
      }
   }

   // Set/Clear ports in Fast PWM mode if reached TOP value while up counting
   if(Counting_up && REG(TCNT0) == Value(Top)) {
      Action_on_port(OCA, Action_top_A);     
      Action_on_port(OCB, Action_top_B);     
   }
   
   // If OCR double buffering in effect, then update real OCR registers when needed
   if(Update_OCR) {                         
      if(REG(TCNT0) == Value(Update_OCR)) {
         if(!(REG(OCR0A) == OCR0A_buffer)) {
            Log("Updating double buffered register OCR0A: %s", hex(OCR0A_buffer));
         }
         if(!(REG(OCR0B) == OCR0B_buffer)) {
            Log("Updating double buffered register OCR0B: %s", hex(OCR0B_buffer));
         }
         REG(OCR0A) = OCR0A_buffer;
         REG(OCR0B) = OCR0B_buffer;
      }
   }

   // TODO: Need special case for PWM_PC: If REG(OCR0A) == MAX ?? or ?? REG(OCR0A) > TOP && TCNT==TOP
   
   // Increment counter and wrap at TOP value or decrement counter if downcounting in PWM PC mode
   REG(TCNT0).d += Counting_up ? 1 : -1;
   REG(TCNT0).d %= Value(Top) + 1;
}

void Update_waveform()
//********************
// Determine the waveform mode according to bits WFMxx
// located in TCCR0B TCCR0A
{
   int waveCode1 = REG(TCCR0A).get_field(1, 0);  // Bits 1, 0 of TCCR0A
   int waveCode2 = REG(TCCR0B).get_field(3, 3);  // Bit 3 of TCCR0B
   int fullWaveCode = -1;                        // Initialize to unknown
   if(waveCode1 >= 0 && waveCode2 >= 0)
      fullWaveCode = waveCode2 * 4 + waveCode1;  // Combine both register's bits

   int newWaveform = WAVE_UNKNOWN;
   Top = VAL_NONE;
   
   switch(fullWaveCode) {       // Apply AVR manual (table #51)

      case 0: // Normal timer mode (TOP=0xFF)
         newWaveform = WAVE_NORMAL;
         Top = VAL_FF; Update_OCR = VAL_NONE; Overflow = VAL_FF;
         break;

      case 1: // PWM phase correct (TOP=0xFF)
         newWaveform = WAVE_PWM_PC;
         Top = VAL_FF; Update_OCR = VAL_FF; Overflow = VAL_00;
         break;

      case 2: // CTC (TOP=OCRA)
         newWaveform = WAVE_CTC;
         Top = VAL_OCRA; Update_OCR = VAL_NONE; Overflow = VAL_FF;
         break;

      case 3: // Fast PWM (TOP=0xFF)
         newWaveform = WAVE_PWM_FAST;
         Top = VAL_FF; Update_OCR = VAL_00; Overflow = VAL_FF;
         break;

      case 5: // PWM phase correct (TOP=OCRA)
         newWaveform = WAVE_PWM_PC;
         Top = VAL_OCRA; Update_OCR = VAL_OCRA; Overflow = VAL_00;
         break;

      case 7: // Fast PWM (TOP=OCRA)
         newWaveform = WAVE_PWM_FAST;
         Top = VAL_OCRA; Update_OCR = VAL_00; Overflow = VAL_OCRA;
         break;

      case 4: case 6:   // Reserved
         newWaveform = WAVE_RESERVED;
         Top = VAL_NONE; Update_OCR = VAL_NONE; Overflow = VAL_NONE;
         break;
   }

   if(newWaveform != Waveform) {
      Waveform = newWaveform;
      Log("Updating waveform: %s (TOP=%s)", Wave_text[Waveform], Top_text[Top]);
      Counting_up = true; // TODO: Should we leave this unset if switching PWM phase correct modes?
      if(Clock_source != CLK_STOP)
      	WARNING("Changing waveform while the timer is running", CAT_TIMER, WARN_PARAM_BUSY);
      if(Waveform == WAVE_RESERVED)
        WARNING("Reserved waveform code", CAT_TIMER, WARN_PARAM_RESERVED);
   }
}

void Update_compare_actions()
// **************************
// Setup the actions to be performed on comparison of A and B
// they are coded in Action_xx  global variables
{
   int oldAction_comp_A = Action_comp_A;
   int oldAction_comp_B = Action_comp_B;

   Action_comp_A = ACT_NONE;  // Compare A
   Action_top_A = ACT_NONE;
   int compCode = REG(TCCR0A).get_field(7, 6); // Bits 7, 6  COM0A1, COM0A0
   switch(compCode) {
      case 1:                               // Toggle
         Action_comp_A = ACT_TOGGLE; break;
      case 2:                               // Clear port
         Action_comp_A = ACT_CLEAR; break;
      case 3:                               // Set port
         Action_comp_A = ACT_SET; break;
   }
   if(Waveform == WAVE_PWM_FAST || Waveform == WAVE_PWM_PC) {
      switch(Action_comp_A) {
         case ACT_TOGGLE:                 // Must check WGM02 bit, since
            if(REG(TCCR0B)[3] == 0)       // disables toggle (TCCR0B, bit 3)
               Action_comp_A = ACT_NONE;
            break;
         case ACT_CLEAR:
            if(Waveform == WAVE_PWM_FAST)
            	Action_top_A = ACT_SET;
            break;
         case ACT_SET:
            if(Waveform == WAVE_PWM_FAST)
            	Action_top_A = ACT_CLEAR;
            break;
      }
   }

   Action_comp_B = ACT_NONE;  // Compare B; some small different behaviour
   Action_top_B = ACT_NONE;
   compCode = REG(TCCR0A).get_field(5, 4); // Bits 5, 4  COM0B1, COM0B0
   switch(compCode) {
      case 1:                               // Toggle
         Action_comp_B = ACT_TOGGLE; break;
      case 2:                               // Clear port
         Action_comp_B = ACT_CLEAR; break;
      case 3:                               // Set port
         Action_comp_B = ACT_SET; break;
   }
   if(Waveform == WAVE_PWM_FAST || Waveform == WAVE_PWM_PC) {
      switch(Action_comp_B) {
         case ACT_TOGGLE:
            Action_comp_B = ACT_RESERVED;  // Tables #49/50 manual
            break;
         case ACT_CLEAR:
            if(Waveform == WAVE_PWM_FAST)
            	Action_top_B = ACT_SET;
            break;
         case ACT_SET:
            if(Waveform == WAVE_PWM_FAST)
            	Action_top_B = ACT_CLEAR;
            break;
      }
   }
   if(Action_comp_B == ACT_RESERVED) {
      WARNING("Reserved combination of COM0Bx bits", CAT_TIMER, WARN_PARAM_RESERVED);
   }

   // If any action on output compare pins, I need to be the owner
   // TODO: I don't think we need to check for PORT_NOT_OUTPUT here
   int rc;
   rc = TAKEOVER_PORT(OCA, Action_comp_A != ACT_NONE);
   Log("TAKEOVER_PORT(%d, %d) == %d", OCA, Action_comp_A != ACT_NONE, rc); // TODO: Delete me
   if(rc == PORT_NOT_OUTPUT) {
      WARNING("OC0A enabled but pin not defined as output in DDR", CAT_TIMER,
         WARN_TIMERS_OUTPUT);
   }
   rc = TAKEOVER_PORT(OCB, Action_comp_B != ACT_NONE && Action_comp_B != ACT_RESERVED);
   Log("TAKEOVER_PORT(%d, %d) == %d", OCB, Action_comp_B != ACT_NONE && Action_comp_B != ACT_RESERVED, rc); // TODO: Delete me
   if(rc == PORT_NOT_OUTPUT) {
      WARNING("OC0B enabled but pin not defined as output in DDR", CAT_TIMER,
         WARN_TIMERS_OUTPUT);
   }
   
   // Log any changes to the compare output modes
   if(oldAction_comp_A != Action_comp_A) {
      Log("Updating OC0A mode: %s", Action_text[Action_comp_A]);
   }
   if(oldAction_comp_B != Action_comp_B) {
      Log("Updating OC0B mode: %s", Action_text[Action_comp_B]);
   }
}

void Update_clock_source()
//************************
{                                               
   int newClockSource = CLK_UNKNOWN;        // Init to unknown values
   int newPrescIndex = 0;

   int clkBits = REG(TCCR0B).get_field(2, 0);   // CSx bits field 2 - 0
   switch(clkBits) {
   	case 0:                            // Stopped
      	newClockSource = CLK_STOP;
         break;

      case 1: case 2:
      case 3: case 4: case 5:            // Internal clock
         newPrescIndex = clkBits;
         newClockSource = CLK_INTERNAL;
         break;

      case 6:                            // External clock, falling edge
      	newClockSource = CLK_EXT_FALL;
         break;

      case 7:                            // External clock rising edge
      	newClockSource = CLK_EXT_RISE;
         break;
   }
   
   // Warn about a clock hot switching and log changes
   if(newPrescIndex != Prescaler_index) {
      Prescaler_index = newPrescIndex;
      if(Prescaler_index > 0)       
         Log("Updating prescaler: %s", Prescaler_text[Prescaler_index]);
      Timer_period = Prescaler_table[Prescaler_index];
   }
   if(newClockSource != Clock_source) {
      if(Clock_source != CLK_STOP && newClockSource != CLK_STOP)
         WARNING("Changed clock source while running", CAT_TIMER, WARN_PARAM_BUSY);
      Clock_source = newClockSource;
      Log("Updating clock source: %s", Clock_text[Clock_source]);
   }

   // Re-schedule pending timer tick to reflect new prescaler value
   Go();
}

void Update_display()
//*******************
// Refresh static controls showing the status etc.
// WORD_8_VIEW_c controls are refreshed automatically. No action is needed
{
   //TODO: Make use of On_update_tick()

   SetWindowTextf(GET_HANDLE(GADGET6), Disabled ? "Disabled" : "%s %s",
      Clock_text[Clock_source], Prescaler_text[Prescaler_index]);

   SetWindowText(GET_HANDLE(GADGET7), Wave_text[Waveform]);
   SetWindowText(GET_HANDLE(GADGET12), Top_text[Top]);

   SetWindowText(GET_HANDLE(GADGET8), hex(OCR0A_buffer)); // Output compare buffers
   SetWindowText(GET_HANDLE(GADGET9), hex(OCR0B_buffer)); // Display in HEX
   
   // Disable (i.e. gray out) double-buffer displays if not in use
   EnableWindow(GET_HANDLE(GADGET8), Update_OCR);
   EnableWindow(GET_HANDLE(GADGET9), Update_OCR);
   EnableWindow(GET_HANDLE(GADGET10), Update_OCR);
}
/* >>> Improvement: a set of variables, Handle_xxx, can be declared at DECLARE_VAR, to
       keep the windows handles just once at the beginning, instead of calling
       every time GET_HANDLE() */

int Value(int pMode)
// Return the TOP value for the counter based on the settings of the WGM bits
{
   switch(pMode) {
      case VAL_00:
         return 0x00;
      case VAL_FF:
         return 0xFF;
      case VAL_OCRA:
         return REG(OCR0A).d;
      default:
         return -1;
   }
}

void Log(const char *pFormat, ...)
//*************************
// Wrapper around the PRINT() function to provide printf() like functionality
// This function also automatically prepends the instance name to the formatted
// message. The message is only printed if DEBUG_LOG is enabled.
{
   if(Debug & DEBUG_LOG) {   
      char strBuffer[MAXBUF];
      va_list argList;

      snprintf(strBuffer, MAXBUF, "%s: ", GET_INSTANCE());
      int len = strlen(strBuffer);

      va_start(argList, pFormat);
      vsnprintf(strBuffer + len, MAXBUF - len, pFormat, argList);
      va_end(argList);

      PRINT(strBuffer);
   }
}
