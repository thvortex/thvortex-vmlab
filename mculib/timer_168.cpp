// =============================================================================
// Common implementation code for the newest style AVR TIMER models. This file
// is included by a different .cpp file for each of the timer types.
//
// Version History:
// v0.1 05/24/09 - Initial public release by AMcTools
// v1.0 09/15/09 - Fully implemented model
//
// Copyright (C) 2009 Advanced MicroControllers Tools (http://www.amctools.com/)
// Copyright (C) 2009 Wojciech Stryjewski <thvortex@gmail.com>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation; either version 2.1 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//

int WINAPI DllEntryPoint(HINSTANCE, unsigned long, void*) {return 1;} // is DLL
// =============================================================================

// Period (in seconds) of an asynchronous 32.768kHz real-time clock crystal.
// Used by TIMER2 for REMIND_ME() delays.
#define PERIOD_32K (1.0 / 32768)

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
enum {CLK_STOP, CLK_INTERNAL, CLK_EXT_FALL, CLK_EXT_RISE, CLK_UNKNOWN, CLK_32K, CLK_EXT};
enum {ACT_NONE, ACT_TOGGLE, ACT_CLEAR, ACT_SET, ACT_RESERVED}; // Actions on compare
enum {WAVE_NORMAL, WAVE_PWM_PC, WAVE_CTC, WAVE_PWM_FAST, WAVE_RESERVED, WAVE_UNKNOWN};
enum {DISBL_BY_SLEEP = 1, DISBL_BY_PRR = 2, DISBL_BY_ADC_SLEEP = 4}; // Disable bit flags
enum {DEBUG_LOG = 1, DEBUG_FUTURE1 = 2, DEBUG_FUTURE2 = 4}; // For Debug. To combine by ORing
enum {VAL_NONE, VAL_00, VAL_FF, VAL_OCRA}; // Counter TOP/overflow value selected by WGM bits
enum {ASY_NONE, ASY_32K, ASY_EXT}; // Type of asynchronous mode operation

// Involved registers. Use same order as in .INI file "Register_map"
// These are IDs or indexes. The real data is stored in a hidden WORD8
// type array. (see "blackbox.h" for details). The WORD8 data is referred
// with the REG(xxx) macro.
//
#ifdef TIMER_0
DECLARE_REGISTERS
   TCCRnA, TCCRnB, TCNTn, OCRnA, OCRnB
END_REGISTERS
#endif

#ifdef TIMER_2
DECLARE_REGISTERS
   TCCRnA, TCCRnB, TCNTn, OCRnA, OCRnB, ASSR
END_REGISTERS
#endif

// Involved interrupts. Use same order as in .INI file "Interrupt_map"
// Like in  registers, these are IDs; what is important is the order.
// It is "Interupt_map" who assigns the index into the actual interrupt name, 
// therefore, different instances can use a different interrupt set
//
DECLARE_INTERRUPTS
   CMPA, CMPB, OVF
END_INTERRUPTS 

// This type is used to keep track of elapsed CPU and I/O clock cycles. Using
// an unsigned type allows the cycle count to function correctly even when it
// wraps due to the rules of modulo arithmetic.
typedef unsigned int uint;

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
   WORD8 _OCRA_buffer;     // For OCR double buffering in PWM modes
   WORD8 _OCRB_buffer;     //
   int _Action_comp_A;     // None/Toggle/Set/Clear actions performed
   int _Action_comp_B;     //    at OCR match. 
   int _Action_top_A;      // Set/Clear actions performs when counter reaches
   int _Action_top_B;      //    TOP in PWM mode
   uint _Last_PSR;         // I/O clock cycle when last PSRSYNC/PSRASY occurred
   uint _Last_disabled;    // I/O clock cycle when _Disabled became non-zero
   uint _Total_disabled;   // Total I/O clock cycles lost due to SLEEP or PRR
   BOOL _Compare_blocked;  // Writing TCNTn blocks output compare for one tick
   BOOL _TSM;              // Counter paused while TSM bit set in GTCCR
   int _Debug;             // To store debugging options
   int _Async;             // Type of asynchronous mode operation (TIMER2 only)
   uint _Async_prescaler;  // Explicit prescaler count for asynchronous mode
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
#define OCRA_buffer VAR(_OCRA_buffer)
#define OCRB_buffer VAR(_OCRB_buffer)
#define Action_comp_A VAR(_Action_comp_A)
#define Action_comp_B VAR(_Action_comp_B)
#define Action_top_A  VAR(_Action_top_A)
#define Action_top_B  VAR(_Action_top_B)
#define Last_PSR VAR(_Last_PSR)
#define Last_disabled VAR(_Last_disabled)
#define Total_disabled VAR(_Total_disabled)
#define Compare_blocked VAR(_Compare_blocked)
#define TSM VAR(_TSM)
#define Debug VAR(_Debug)
#define Async VAR(_Async)
#define Async_prescaler VAR(_Async_prescaler)

// Some static tables for timer mode display
const char *Clock_text[] = {
   "Stop", "Internal", "External (Fall)", "External (Rise)", "?", "32768Hz", "External"
};
const char *Wave_text[] = {"Normal", "PWM PC", "CTC", "PWM Fast", "Reserved", "?"};
const char *Top_text[] = {"?", "$00", "$FF", "OCRA" };
const char *Action_text[] = {"Disconnected", "Toggle", "Clear", "Set", "Reserved"};

USE_WINDOW(WINDOW_USER_1); // Window to display registers, etc. See .RC file

// Coding and static tables for clock prescaling
#ifdef TIMER_2
const int Prescaler_table[] =  {0, 1, 8, 32, 64, 128, 256, 1024};
const char *Prescaler_text[] = {"", "/ 1", "/ 8", "/ 32", "/ 64", "/ 128", "/ 256", "/ 1024" };

#else
const int Prescaler_table[] =  {0, 1, 8, 64, 256, 1024};
const char *Prescaler_text[] = {"", "/ 1", "/ 8", "/ 64", "/ 256", "/ 1024" };
#endif

#ifdef TIMER_0
REGISTERS_VIEW
//           ID     .RC ID       b7        ...   bit names   ...             b0
   DISPLAY(TCCRnA, GDT_TCCRnA, COM0A1, COM0A0, COM0B1, COM0B0, *, *, WGM01, WGM00)
   DISPLAY(TCCRnB, GDT_TCCRnB, FOC0A, FOC0B, *, *, WGM02, CS02, CS01, CS00)
   DISPLAY(TCNTn,  GDT_TCNTn,  *, *, *, *, *, *, *, *)
   DISPLAY(OCRnA,  GDT_OCRnA,  *, *, *, *, *, *, *, *)
   DISPLAY(OCRnB,  GDT_OCRnB,  *, *, *, *, *, *, *, *)
END_VIEW
#endif

#ifdef TIMER_2
REGISTERS_VIEW
//           ID     .RC ID       b7        ...   bit names   ...             b0
   DISPLAY(TCCRnA, GDT_TCCRnA, COM2A1, COM2A0, COM2B1, COM2B0, *, *, WGM21, WGM20)
   DISPLAY(TCCRnB, GDT_TCCRnB, FOC2A, FOC2B, *, *, WGM22, CS22, CS21, CS20)
   DISPLAY(TCNTn,  GDT_TCNTn,  *, *, *, *, *, *, *, *)
   DISPLAY(OCRnA,  GDT_OCRnA,  *, *, *, *, *, *, *, *)
   DISPLAY(OCRnB,  GDT_OCRnB,  *, *, *, *, *, *, *, *)
   DISPLAY(ASSR,   GDT_ASSR,   *, EXCLK, AS2, TCN2UB, OCR2AUB, OCR2BUB, TCR2AUB, TCR2BUB)
END_VIEW
#endif

void Go();                      // Function prototypes
void Count();                   //
void Async_tick();
void Async_change();
void Update_waveform();
void Update_compare_actions();
void Update_clock_source();
void Update_display();
void Action_on_port(PORT, int, BOOL pMode = Counting_up);
void Log(const char *pFormat, ...);
int Value(int);
uint Get_io_cycles(void);
bool Is_disabled(void);

//==============================================================================
// Callback functions, On_xxx(...)
//==============================================================================

const char *On_create()                   // Mandatory function
{
   Debug = 0;
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
   OCRA_buffer.x = 0;
   OCRB_buffer.x = 0;
   Disabled = 0;
   Clock_source = CLK_UNKNOWN;
   Waveform = WAVE_UNKNOWN;
   Top = VAL_NONE;
   Prescaler_index = 0;
   Update_OCR = VAL_NONE;
   Async = ASY_NONE;
   Update_display();
}

WORD8 *On_register_read(REGISTER_ID pId)
//**************************************
// Notification about a read operation over the given register. This is only
// necessary if any kind of on-read action is needed: clear-on-read,
// temporary buffer read, etc. Otherwise it can be omitted (this case)
{
   // TODO: Cannot read any registers if disabled (no clocks)
   
   // TODO: TIMER@ async: Check for reading TCNT2 after immediate wakeup

   // If OCRx double buffering is in effect, the CPU always reads the buffer
   // instead of the real registers.
   if(Update_OCR) {
      switch(pId) {
         case OCRnA:
            return &OCRA_buffer;
         case OCRnB:
            return &OCRB_buffer;
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
   PRINT("On_register_write()");


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

   // TODO: Cannot write any registers if disabled (no clocks)

   // TODO: TIMER2 check for update-busy bits in async mode

   switch(pId) {

       case_register(TCCRnA, 0xF3)      // Timer Control Register A
       //---------------------------------------------------------
          REG(TCCRnA) = pData & 0xF3;   // $F3 = read-only bits mask
          Update_waveform();
          Update_compare_actions();
          Update_display();
       end_register

       case_register(TCCRnB, 0xCF)      // Timer Control Register B
       //---------------------------------------------------------

          REG(TCCRnB) = pData & 0x0F;   // FOCnA/FOCnB are write-only
          Update_waveform();
          Update_compare_actions();
          Update_clock_source();
          Update_display();

          // Handle FOC0A, FOC0B (Force Output Compare), only in non-PWM modes
          // Since writing TCCRnB could have changed the waveform mode and
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

       case_register(TCNTn, 0xFF) // Timer Counter Register
       //--------------------------------------------------
          REG(TCNTn) = pData;
          Compare_blocked = true;
          if(Clock_source != CLK_STOP && Clock_source != CLK_UNKNOWN)
             WARNING("TCNTn modified while running", CAT_TIMER, WARN_PARAM_BUSY);
       end_register

       case_register(OCRnA, 0xFF)  // Output Compare Register A
       //------------------------------------------------------
          OCRA_buffer = pData;    // All bits r/w no mask necessary
          if(!Update_OCR) {
             REG(OCRnA) = pData;
          }
          Update_display(); // Buffer static control need update
       end_register

       case_register(OCRnB, 0xFF) // Output Compare Register B
       //------------------------------------------------------
          OCRB_buffer = pData;
          if(!Update_OCR) {
             REG(OCRnB) = pData;
          }
          Update_display();
       end_register
       
#ifdef TIMER_2
       case_register(ASSR, 0x60) // Asynchronous Status Register
       //------------------------------------------------------
          // EXCLK should be changed before asynchronous mode enabled
          if(pData[6] != REG(ASSR)[6] && (pData[5] == 1 || REG(ASSR)[5] == 1)) {
             WARNING("EXCLK bit in ASSR changed while AS2 bit is 1", CAT_TIMER,
                WARN_PARAM_BUSY);
          }
          
          // TODO: Warn about pending update-busy bits and clear them if
          // changing async mode
          REG(ASSR) = pData & 0x60;

          // Check if asynchronous mode is enabled or disabled in ASSR
          int oldAsync = Async;
          switch(REG(ASSR).get_field(6, 5)) {
             case 1: Async = ASY_32K; break;   // EXCLK=0 AS2=1
             case 3: Async = ASY_EXT; break;   // EXCLK=1 AS2=2
             default: Async = ASY_NONE; break; // AS2=0 or EXCLK/AS2 unknown
          }
          
          // Changing the asynchronous mode can corrupt all TIMER2 registers
          // except for ASSR, so they should be set to UNKNOWN. Also, if the
          // 32.768kHz oscillator was enabled, then schedule a REMIND_ME()
          if(oldAsync != Async) {
             Async_change();
          }
   
          // Update everything since registers values may be unknown now
          Update_waveform();
          Update_compare_actions();
          Update_clock_source();
          Update_display();
       end_register
#endif
   }
}

void On_remind_me(double pTime, int pAux)
//**************************************
// Response to REMIND_ME2() used implement the internal
// prescaled clock tick or an asynchronous 32.768kHz clock
// crystal (before prescaling). The pAux parameter holds a
// signature value used to void pending ticks in case of
// prescaler reset or clock source changes.
{
   if(Tick_signature != pAux)        // If need to void a pending tick
      return;
   if(Is_disabled())                 // If disabled by SLEEP, PPR, etc
      return;
   if(Clock_source != CLK_INTERNAL)  // Or clock source is no longer internal
      return;
   else 

   // If using internal clock source, increment counter and launch next tick
   if(Clock_source == CLK_INTERNAL) {
      if(TSM && Timer_period != 1)   // TSM holds prescaler not direct clock
         return;
         
      Count();
      Go();
   }
   
   // If using asynchronous 32kHz XTAL, always increment prescaler and schedule
   // next oscillator tick. Increment counter only on prescaled ticks of the
   // oscillator
#ifdef TIMER_2
   else if(Clock_source == CLK_32K) {
      REMIND_ME(PERIOD_32K, ++Tick_signature);
      Async_tick();
   }
#endif
}

void On_port_edge(PORT pPort, EDGE pEdge, double pTime)
//*****************************************************
// Handle external clock. Check the edge and call the
// general Count() procedure
{
   // TODO: Warn about transitions that are too fast for CPU clock? Datasheet recommends
   // max frequency clkIO/2.5. This is the Nyquist sampling frequency with some room for
   // clock variations. For TIMER2 external clock it should be clkIO/4.

   if(pPort != XCLK)  // External clock pin XCLK, defined in DECLARE_PINS
      return;
   if(Is_disabled())
      return;
   if(Clock_source == CLK_EXT_RISE && pEdge == RISE)
      Count();
   else if(Clock_source == CLK_EXT_FALL && pEdge == FALL)
      Count();
      
   // CLK_EXT is only used with asynchronous TIMER2 operation
#ifdef TIMER_2
   else if(Clock_source == CLK_EXT && pEdge == RISE) {
      Async_tick();
   }
#endif
}

void On_reset(int pCause)
//***********************
// Initialize registers to the desired value. Use the FOREACH macro for
// a general initialization
{
   FOREACH_REGISTER(j) {
      REG(j)= 0;
   }
   OCRA_buffer = 0;
   OCRB_buffer = 0;
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
   Last_disabled = 0;
   Total_disabled = 0;
   Compare_blocked = true; // Prevent compare match immediately after reset 
   TSM = false;
   Async = ASY_NONE;
   Async_prescaler = 0;
   Update_display();
}

void On_update_tick(double)
{
   PRINT("On_update_tick()");
}

void On_notify(int pWhat)
//**********************
// Notification coming from some other DLL instance. Used here to
// handle the PRR register and prescaler reset (coming from dummy component)
{
   int wasDisabled;

   // TODO: PRR only disables synchronous mode
   switch(pWhat) {
      case NTF_PRR0:                 // A 0 set in PPR register bit for TIMER 0
         wasDisabled = Is_disabled();
         Disabled &= ~DISBL_BY_PRR;
         if(wasDisabled & !Is_disabled()) {
            Log("Enabled by PRR");
            Update_display();
            Go();
         }
         break;

      case NTF_PRR1:                 // A 1 set in PPR register bit for TIMER 0
         wasDisabled = Is_disabled();
         Disabled |= DISBL_BY_PRR;
         if(!wasDisabled && Is_disabled()) {
            Log("Disabled by PRR");
            Update_display();
            Go();
         }
         break;

      case NTF_TSM:                  // Timer sync mode; prescaler reset continuously asserted
         Log("Started TSM");
         TSM = true;
         Update_display();
         break;

      case NTF_PSR:                  // Prescaler reset.
         // Record elapsed I/O clock cycles to simulate the reset and ensure all
         // timers sharing the same prescaler stay synchronized
         Last_PSR = Get_io_cycles();
         Async_prescaler = 0;
#ifdef TIMER_2
         Log("Prescaler reset by PSRASY");
#else
         Log("Prescaler reset by PSRSYNC");
#endif
         
         // Restart counter if previously disabled by TSM
         if (TSM) {              
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
   int wasDisabled = Is_disabled();

   switch(pMode) {
      case SLEEP_EXIT:                    // Micro wakes up.
         Disabled &= ~(DISBL_BY_SLEEP | DISBL_BY_ADC_SLEEP);
         break;

      case SLEEP_IDLE:                 // This mode does not affect any timer
         return;

      case SLEEP_NOISE_REDUCTION:      // TIMER2 disabled only in sync mode
         Disabled |= DISBL_BY_ADC_SLEEP;
         break;
                                       
      case SLEEP_POWERSAVE:            // TIMER2 never disabled by power-save
#ifdef TIMER_2
         return;                       
#endif                                 

      case SLEEP_STANDBY:              // Modes that always disable any timer
      case SLEEP_POWERDOWN:
         Disabled |= DISBL_BY_SLEEP;
         break;
   }

   if(wasDisabled != Is_disabled()) {
      // TODO: TIMER2 async: Set registers to UNKNOWN on deep wake up

      if(Is_disabled()) {
         Log("Disabled by SLEEP");
      } else {
         Log("Exit from SLEEP");
      }

#ifdef TIMER_2
      // Waking up from power-down or standy (and presumebly when entering
      // either sleep mode) can corrupt all TIMER2 registers except for
      // ASSR, so they should be set to UNKNOWN. Also, if the 32.768kHz
      // oscillator was re-enabled, then schedule a new REMIND_ME()
      if(Async) {
         Async_change();
      }
#endif         
   
      Update_display();
      Go();
   }
   
   // TODO: TIMER2 checks:
   // * check for pending reigster updates if going to ADC sleep/powersave
   // * check for interrupts on this TOSC clock cycle if going to ADC/powersave
}

void On_gadget_notify(GADGET pGadget, int pCode)
//*********************************************
// Response to notification from "Log" checkbox and toggle flag
{
   if(pGadget == GDT_LOG && pCode == BN_CLICKED) {
      Debug ^= DEBUG_LOG;
   }
}

//==============================================================================
// Internal functions (non-callback). Prototypes defined above
//==============================================================================

#ifdef TIMER_2
void Async_tick()
//*****************************************
// Increment the explicit asynchronous prescaler and increment the timer
// if the prescaler period has been reached. Called from On_remind_me() to
// handle the 32.768kHz clock tick and from On_port_edge() to handle an
// external clock tick on TOSC1.
{
   PRINT("Async_tick()");

   // TODO: Implement update-busy bits here to write real registers/buffers
   // only update the registers if not in ADC noise or powersave sleep.

   if(TSM && Timer_period != 1) // TSM only holds prescaler not direct clock
      return;
   Async_prescaler++;
   if((Async_prescaler + 1) % Timer_period == 0)
      Count();
}

void Async_change()
//*****************************************
// Called when TIMER2 is switching to/from asynchronous mode or when TIMER2
// is waking up from power-down or standby while in asynchronous mode.
{
   // All TIMER2 registers except ASSE are set to UNKNOWN since they can
   // become corrupt on real AVR hardware.
   //REG(TCCRnA).x = 0;
   //REG(TCCRnB).x = 0;
   //REG(TCNTn).x = 0;
   //REG(OCRnA).x = 0;
   //REG(OCRnB).x = 0;
   //OCRA_buffer.x = 0;
   //OCRB_buffer.x = 0;

   // If the 32.768kHz crystal oscillator was (re)enabled then schedule
   // the first oscillator tick with REMIND_ME()
   if(Async == ASY_32K && !Is_disabled()) {
      REMIND_ME(PERIOD_32K, ++Tick_signature);
      // TODO: Warn about waiting 1 sec for 32K oscillator to stabilize
   }

   // Otherwise, the oscillator may be getting disabled due to leaving
   // asynchronous mode (AS2=0). Update Tick_signature to cancel any 32.768kHz
   // oscillator ticks that may be pending.
   else {
      ++Tick_signature;
   }
}
#endif

void Go()
//******
// Schedule the next timer tick if the counter is not disabled and using the
// internal clock source. Called on every timer tick, each time the counter
// is re-enabled, and each time the clock source changes. The Tick_signature
// is updated each item to cancel any already pending ticks which may no longer
// be valid. For TIMER2 in 32kHz asynchronous mode, this function never
// schedules timer ticks.
{
   if(Disabled) {
      // Track when I/O clock first became disabled
      if(!Last_disabled)
         Last_disabled = Get_io_cycles();
      return;
   }
   
   // If the I/O clock was re-enabled, compute how long it was disabled for
   if(Last_disabled) {
      Total_disabled = (uint) GET_MICRO_INFO(INFO_CPU_CYCLES) - Last_disabled;
      Last_disabled = 0;
   }
  
   if(Clock_source == CLK_INTERNAL) {
      if(TSM && Timer_period != 1) // TSM only holds prescaler not direct clock
         return;
      uint cycles = Get_io_cycles() - Last_PSR;
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
         rc = SET_PORT(pPort, TOGGLE); break;
      case ACT_SET:
         rc = SET_PORT(pPort, pMode ? 1 : 0); break;
      case ACT_CLEAR:
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
   if(REG(TCNTn) == Value(Overflow)) {
      SET_INTERRUPT_FLAG(OVF, FLAG_SET);
   }

   // Reverse counter direction in PWM PC mode when it reaches TOP/BOTTOM/MAX
   if(Waveform == WAVE_PWM_PC) {
      if(Counting_up) {
         if(REG(TCNTn) == Value(Top)) {
            Counting_up = false;
         }
      } else {
         if(REG(TCNTn) == 0) {
            Counting_up = true;
         }
      }
   }

   // If in PWM PC mode, when the counter reaches TOP, the output compare
   // values must equal the result of an up-counting compare match. This
   // takes care of the special cases when the OCR registers equal BOTTOM
   // or are greater than TOP. It also handles the case where the compare
   // output changes without an OCR match to ensure the outputs are symmetric
   // around BOTTOM. The only exception to this rule is when OCR equals top.
   // In that case, the next code block will overwrite the port assignment with
   // the correct values. If using ACT_TOGGLE, this code block does nothing.
   if(Waveform == WAVE_PWM_PC && REG(TCNTn) == Value(Top)) {
      Action_on_port(OCA, Action_top_A, true);
      Action_on_port(OCB, Action_top_B, true);
   }

   // Update output pins if a compare output match occurs. A CPU write to TNCT0
   // blocks any output compare in the next timer clock cycle even if the
   // counter was stopped at the time the TNCT0 write occurred.
   if(!Compare_blocked) {
      if(REG(TCNTn) == REG(OCRnA)) {
         Action_on_port(OCA, Action_comp_A);
         SET_INTERRUPT_FLAG(CMPA, FLAG_SET);
      }
      if(REG(TCNTn) == REG(OCRnB)) {
         Action_on_port(OCB, Action_comp_B);
         SET_INTERRUPT_FLAG(CMPB, FLAG_SET);
      }
   }
   Compare_blocked = false;

   // In fast PWM mode, all PWM cycles begin when counter reaches TOP. This is
   // true even when OCR equals TOP in which case this code block overwrites
   // the previous port assignment.
   if(Waveform == WAVE_PWM_FAST && REG(TCNTn) == Value(Top)) {
      Action_on_port(OCA, Action_top_A, false);     
      Action_on_port(OCB, Action_top_B, false);     
   }
   
   // If OCR double buffering in effect, then update real OCR registers when needed
   if(Update_OCR) {                         
      if(REG(TCNTn) == Value(Update_OCR)) {
         if(!(REG(OCRnA) == OCRA_buffer)) {
            Log("Updating double buffered register OCRnA: %s", hex(OCRA_buffer));
         }
         if(!(REG(OCRnB) == OCRB_buffer)) {
            Log("Updating double buffered register OCRnB: %s", hex(OCRB_buffer));
         }
         REG(OCRnA) = OCRA_buffer;
         REG(OCRnB) = OCRB_buffer;
      }
   }

   // Increment/decrement counter and clear after TOP was reached
   if(Counting_up && REG(TCNTn) == Value(Top)) {
      REG(TCNTn) = 0;
   } else {
      REG(TCNTn).d += Counting_up ? 1 : -1;
   }
}

void Update_waveform()
//********************
// Determine the waveform mode according to bits WFMxx
// located in TCCRnB TCCRnA
{
   int waveCode1 = REG(TCCRnA).get_field(1, 0);  // Bits 1, 0 of TCCRnA
   int waveCode2 = REG(TCCRnB).get_field(3, 3);  // Bit 3 of TCCRnB
   int fullWaveCode = -1;                        // Initialize to unknown
   if(waveCode1 >= 0 && waveCode2 >= 0)
      fullWaveCode = waveCode2 * 4 + waveCode1;  // Combine both register's bits

   int newWaveform = WAVE_UNKNOWN;
   Top = VAL_NONE;
   Update_OCR = VAL_NONE;
   
   switch(fullWaveCode) {       // Apply AVR manual (table #51)

      case 0: // Normal timer mode (TOP=0xFF)
         newWaveform = WAVE_NORMAL; Counting_up = true;
         Top = VAL_FF; Update_OCR = VAL_NONE; Overflow = VAL_FF;
         break;

      case 1: // PWM phase correct (TOP=0xFF)
         newWaveform = WAVE_PWM_PC;
         Top = VAL_FF; Update_OCR = VAL_FF; Overflow = VAL_00;
         break;

      case 2: // CTC (TOP=OCRA)
         newWaveform = WAVE_CTC; Counting_up = true;
         Top = VAL_OCRA; Update_OCR = VAL_NONE; Overflow = VAL_FF;
         break;

      case 3: // Fast PWM (TOP=0xFF)
         newWaveform = WAVE_PWM_FAST; Counting_up = true;
         Top = VAL_FF; Update_OCR = VAL_00; Overflow = VAL_FF;
         break;

      case 5: // PWM phase correct (TOP=OCRA)
         newWaveform = WAVE_PWM_PC;
         Top = VAL_OCRA; Update_OCR = VAL_OCRA; Overflow = VAL_00;
         break;

      case 7: // Fast PWM (TOP=OCRA)
         newWaveform = WAVE_PWM_FAST; Counting_up = true;
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
      if(Clock_source != CLK_STOP)
      	WARNING("Changing waveform while the timer is running", CAT_TIMER, WARN_PARAM_BUSY);
      if(Waveform == WAVE_RESERVED)
         WARNING("Reserved waveform mode", CAT_TIMER, WARN_PARAM_RESERVED);
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
   int compCode = REG(TCCRnA).get_field(7, 6); // Bits 7, 6  COM0A1, COM0A0
   switch(compCode) {
      case 1:                               // Toggle
         Action_comp_A = ACT_TOGGLE;
         if(Waveform == WAVE_PWM_FAST || Waveform == WAVE_PWM_PC) {
            if(REG(TCCRnB)[3] == 0) {      // disables toggle (TCCRnB, bit 3)
               Action_comp_A = ACT_NONE;
            }
         }
         break;
         
      case 2:                               // Clear port
         Action_comp_A = ACT_CLEAR;
         Action_top_A = ACT_CLEAR;
         break;
         
      case 3:                               // Set port
         Action_comp_A = ACT_SET;
         Action_top_A = ACT_SET;
         break;
   }

   Action_comp_B = ACT_NONE;  // Compare B; some small different behaviour
   Action_top_B = ACT_NONE;
   compCode = REG(TCCRnA).get_field(5, 4); // Bits 5, 4  COM0B1, COM0B0
   switch(compCode) {
      case 1:                               // Toggle
         Action_comp_B = ACT_TOGGLE;
         if(Waveform == WAVE_PWM_FAST || Waveform == WAVE_PWM_PC) {
            Action_comp_B = ACT_RESERVED;  // Tables #49/50 manual
         }
         break;
         
      case 2:                               // Clear port
         Action_comp_B = ACT_CLEAR;
         Action_top_B = ACT_CLEAR;
         break;
         
      case 3:                               // Set port
         Action_comp_B = ACT_SET;
         Action_top_B = ACT_SET;
         break;
   }
   if(Action_comp_B == ACT_RESERVED) {
      WARNING("Reserved combination of COM0Bx bits", CAT_TIMER, WARN_PARAM_RESERVED);
   }

   // If any action on output compare pins, I need to be the owner
   int rc;
   rc = TAKEOVER_PORT(OCA, Action_comp_A != ACT_NONE);
   if(rc == PORT_NOT_OUTPUT) {
      WARNING("OC0A enabled but pin not defined as output in DDR", CAT_TIMER,
         WARN_TIMERS_OUTPUT);
   }
   rc = TAKEOVER_PORT(OCB, Action_comp_B != ACT_NONE && Action_comp_B != ACT_RESERVED);
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
   int newClockSource;
   int newPrescIndex = 0;

   int clkBits = REG(TCCRnB).get_field(2, 0);   // CSx bits field 2 - 0
   switch(clkBits) {
   	case 0:                            // Stopped
      	newClockSource = CLK_STOP;
         break;

#ifndef TIMER_2
      case 6:                            // External clock, falling edge
      	newClockSource = CLK_EXT_FALL;
         break;

      case 7:                            // External clock rising edge
      	newClockSource = CLK_EXT_RISE;
         break;
#endif

      case -1:
         newClockSource = CLK_UNKNOWN;   // If CS bits in TCCRnB are unknown
         break;
         
      default:                           // Internal or asynchronous clock
         newPrescIndex = clkBits;
         
#ifdef TIMER_2
         // Asynchronous mode overrides clock selection except STOP or UNKNOWN
         switch(Async) {
            case ASY_NONE: newClockSource = CLK_INTERNAL; break;
            case ASY_32K: newClockSource = CLK_32K; break; 
            case ASY_EXT: newClockSource = CLK_EXT; break; 
         }
#endif
         
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

   SetWindowTextf(GET_HANDLE(GDT_CLOCK), Is_disabled() ? "Disabled" : "%s %s",
      Clock_text[Clock_source], Prescaler_text[Prescaler_index]);

   SetWindowText(GET_HANDLE(GDT_MODE), Wave_text[Waveform]);
   SetWindowText(GET_HANDLE(GDT_TOP), Top_text[Top]);

   // Output compare buffers in hex
   SetWindowText(GET_HANDLE(GDT_BUFA), hex(OCRA_buffer));
   SetWindowText(GET_HANDLE(GDT_BUFB), hex(OCRB_buffer));
   
   // Disable (i.e. gray out) double-buffer displays if not in use
   EnableWindow(GET_HANDLE(GDT_BUFA), Update_OCR);
   EnableWindow(GET_HANDLE(GDT_BUFB), Update_OCR);
   EnableWindow(GET_HANDLE(GDT_BUF), Update_OCR);
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
         return REG(OCRnA).d;
      default:
         return -1;
   }
}

uint Get_io_cycles(void)
//*************************
// Return the total number of I/O clock cycles seen by the timer. The I/O clock
// runs at the same speed as the CPU clock but it can be disabled by either
// SLEEP mode or by the PRR (Power Reduction Register)
{
   if(Last_disabled) {
      return Last_disabled;
   } else {
      return (uint) GET_MICRO_INFO(INFO_CPU_CYCLES) - Total_disabled;
   }
}

bool Is_disabled(void)
//*************************
// Return TRUE if the timer is really disabled based on whether it's using
// asynchronous mode. In asynchronous mode, PRR and ADC Noise Reduction sleep
// mode have no effect on timer. Only power-down or standby sleep mode disables
// the timer in asynchronous mode.
{
   return Async ? (Disabled & DISBL_BY_SLEEP) != 0 : Disabled != 0;
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
