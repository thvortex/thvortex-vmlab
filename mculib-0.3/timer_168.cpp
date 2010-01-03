// =============================================================================
// Common implementation code for the newest style AVR TIMER models. This file
// is included by a different .cpp file for each of the timer types.
//
// Copyright (C) 2009 Advanced MicroControllers Tools (http://www.amctools.com/)
// Copyright (C) 2009-2010 Wojciech Stryjewski <thvortex@gmail.com>
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
#ifdef TIMER_N
   MICRO_PORT(ICP, 4)     // Input Capture
#endif
END_PINS

// Coding of work modes, status, clock sources, etc
//
enum {CLK_STOP, CLK_INTERNAL, CLK_EXT_FALL, CLK_EXT_RISE, CLK_UNKNOWN, CLK_32K, CLK_EXT};
enum {ACT_NONE, ACT_TOGGLE, ACT_CLEAR, ACT_SET, ACT_RESERVED}; // Actions on compare
enum {DEBUG_LOG = 1, DEBUG_ASYNC_CORRUPT = 2}; // For Debug. To combine by ORing
enum {ASY_NONE, ASY_32K, ASY_EXT}; // Type of asynchronous mode operation

// Wsveform generation modes
enum {
   WAVE_NORMAL, WAVE_PWM_PC, WAVE_PWM_PFC, WAVE_CTC, WAVE_PWM_FAST,
   WAVE_RESERVED, WAVE_UNKNOWN
};

// Counter TOP/overflow value selected by WGM bits
enum {
   VAL_NONE, VAL_OCRA, VAL_ICR, VAL_00, VAL_FF, VAL_1FF, VAL_3FF, VAL_FFFF
};

// Mask values for OCR/TCNT registers based on the VAL_XXX used as counter TOP.
// When using a fixed counter TOP value with 16-bit timers, writes to the
// OCR buffers and TCNT updates are masked against unused bits
const int MASK[] = {
   0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFF, 0x1FF, 0x3FF, 0xFFFF
};

// Involved registers. Use same order as in .INI file "Register_map"
// These are IDs or indexes. The real data is stored in a hidden WORD8
// type array. (see "blackbox.h" for details). The WORD8 data is referred
// with the REG(xxx) macro. Note that for 16-bit timers, the low byte
// for a given register (e.g. TCNTn) must be immediately followed by the
// high byte (e.g. TCNTnH) in the DECLARE_REGISTERS block. The WORDHL()
// macro depends on this when accessing both registers as a single 16
// little-endian value.
//
#ifdef TIMER_N
DECLARE_REGISTERS
   TCCRnA, TCCRnB, TCCRnC, TCNTn, TCNTnH,
   OCRnA, OCRnAH, OCRnB, OCRnBH, ICRn, ICRnH
END_REGISTERS
#else

DECLARE_REGISTERS
   TCCRnA, TCCRnB, TCNTn, OCRnA, OCRnB
#ifdef TIMER_2
   ,ASSR
#endif
END_REGISTERS

#endif // #ifdef TIMER_N

// Return true if counter uses dual slope operation
#define IS_WAVE_DUAL_SLOPE() (Waveform == WAVE_PWM_PC || Waveform == WAVE_PWM_PFC)

// Involved interrupts. Use same order as in .INI file "Interrupt_map"
// Like in  registers, these are IDs; what is important is the order.
// It is "Interupt_map" who assigns the index into the actual interrupt name, 
// therefore, different instances can use a different interrupt set
//
DECLARE_INTERRUPTS
   CMPA, CMPB, OVF
#ifdef TIMER_N
   ,CAPT
#endif
END_INTERRUPTS 

// This type is used to keep track of elapsed CPU and I/O clock cycles. Using
// an unsigned type allows the cycle count to function correctly even when it
// wraps due to the rules of modulo arithmetic.
typedef unsigned int uint;

#ifdef TIMER_2
// Register IDs that need to be updated through a temporary register when
// TIMER2 is in asynchronous mode. Must be listed in the same order as the
// _2UB bits in the ASSR register.
const REGISTER_ID ASSR_UB[] = { TCCRnB, TCCRnA, OCRnB, OCRnA, TCNTn };

// Register names that correspond to ASSR UB bits in the same order
const char *Assr_text[] = { "TCCR2B", "TCCR2A", "OCR2B", "OCR2A", "TCNT2" };

// Structure used to track pending register updates while TIMER2 is running
// in asynchronous mode
typedef struct {
   WORD8 Value;   // Pending value to be assigned to register
   uint Ticks;    // Async_ticks value when register update was started
} Async_update_t;
#endif

// To insure that more than a timer instance can be placed, declare
// variables here. Some #defines below hide the VAR() use, making the
// code more readable
//
DECLARE_VAR
   BOOL _Dirty;            // True if the GUI needs refreshing
   int _Clock_source;      // Internal, external, ...
   bool _PRR;              // True if disabled by PRR
   int _Sleep_mode;        // One of the SLEEP_xxx constants
   int _Prescaler_index;   // The selected prescaler
   int _Timer_period;      // In clock cycles
   BOOL _Counting_up;      // For up-down management
   int _Tick_signature;    // For REMIND_ME2, to validate ticks
   int _Waveform;          // Says if PWM, normal, CTC, etc
   int _Top;               // Counter TOP value (0xFF, OCRA, etc)
   int _Overflow;          // Counter value that causes overflow interrupt
   int _Update_OCR;        // Counter value at which to update OCR double-buffer
   WORDSZ _OCRA_buffer;    // For OCR double buffering in PWM modes
   WORDSZ _OCRB_buffer;    //
   int _Action_comp_A;     // None/Toggle/Set/Clear actions performed
   int _Action_comp_B;     //    at OCR match. 
   int _Action_top_A;      // Set/Clear actions performs when counter reaches
   int _Action_top_B;      //    TOP in PWM mode
   uint _Last_PSR;         // I/O clock cycle when last PSRSYNC/PSRASY occurred
   uint _Last_disabled;    // I/O clock cycle when prescaler was disabled
   uint _Total_disabled;   // Total prescaler clock cycles lost due to SLEEP
   BOOL _Compare_blocked;  // Writing TCNTn blocks output compare for one tick
   BOOL _TSM;              // Counter paused while TSM bit set in GTCCR
   int _Debug;             // To store debugging options
   int _Async;             // Type of asynchronous mode operation (TIMER2 only)
   uint _Async_prescaler;  // Explicit prescaler count for asynchronous mode
   uint _Async_interrupt;  // Bitflags of any interrupts from last async tick
   BOOL _OCA_toggle_ok;    // True if toggle on OCA pin allowed by waveform
#ifdef TIMER_2
   WORD8 _Tcnt_async;      // TCNT2 seens by MCU when TIMER2 in async mode
   uint _Async_ticks;      // Number of times Async_tick() has been called
   Async_update_t _Async_update[5]; // Pending register updates in async TIMER2
#endif
#ifdef TIMER_N
   int _TMP_regid;         // Register ID by which TMP was last accessed
   WORD8 _TMP_buffer;      // For high byte access in 16-bit timers
   WORD8 _READ_buffer;     // For high-byte access in On_register_read()   
#endif
END_VAR
#define Dirty VAR(_Dirty)                  // To simplify readability...
#define Clock_source VAR(_Clock_source)
#define PRR VAR(_PRR)
#define Sleep_mode VAR(_Sleep_mode)
#define Prescaler_index VAR(_Prescaler_index)
#define Timer_period VAR(_Timer_period)
#define Counting_up VAR(_Counting_up)
#define Tick_signature VAR(_Tick_signature)
#define Waveform VAR(_Waveform)
#define Top VAR(_Top)
#define Overflow VAR(_Overflow)
#define Update_OCR VAR(_Update_OCR)
#define TMP_regid VAR(_TMP_regid)
#define TMP_buffer VAR(_TMP_buffer)
#define READ_buffer VAR(_READ_buffer)
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
#define Async_interrupt VAR(_Async_interrupt)
#define Tcnt_async VAR(_Tcnt_async)
#define Async_ticks VAR(_Async_ticks)
#define Async_update VAR(_Async_update)
#define OCA_toggle_ok VAR(_OCA_toggle_ok)

// Constant WORD8 value with all bits unknown. Returned by On_register_read()
// if the timer registers are accessed while the timer is disabled due to
// PRR.
WORD8 UNKNOWN8;

// Some static tables for timer mode display
const char *Clock_text[] = {
   "Stop", "Internal", "External (Fall)", "External (Rise)", "?", "32768Hz", "External"
};
const char *Wave_text[] = {"Normal", "PWM PC", "PWM PFC", "CTC", "PWM Fast", "Reserved", "?"};
const char *Top_text[] = {"?", "OCRA", "ICR", "$00", "$FF", "$1FF", "$3FF", "$FFFF" };
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

#ifdef TIMER_N
REGISTERS_VIEW
//           ID     .RC ID       b7        ...   bit names   ...             b0
   DISPLAY(TCCRnA, GDT_TCCRnA, COMnA1, COMnA0, COMnB1, COMnB0, *, *, WGMn1, WGMn0)
   DISPLAY(TCCRnB, GDT_TCCRnB, ICNCn, ICESn, *, WGMn3, WGMn2, CSn2, CSn1, CSn0)
   DISPLAY(TCCRnC, GDT_TCCRnC, FOCnA, FOCnB, *, *, *, *, *, *)
   DISPLAY(TCNTnH, GDT_TCNTnH, *, *, *, *, *, *, *, *)
   DISPLAY(TCNTn,  GDT_TCNTnL, *, *, *, *, *, *, *, *)
   DISPLAY(OCRnAH, GDT_OCRnAH, *, *, *, *, *, *, *, *)
   DISPLAY(OCRnA,  GDT_OCRnAL, *, *, *, *, *, *, *, *)
   DISPLAY(OCRnBH, GDT_OCRnBH, *, *, *, *, *, *, *, *)
   DISPLAY(OCRnB,  GDT_OCRnBL, *, *, *, *, *, *, *, *)
   DISPLAY(ICRnH,  GDT_ICRnH,  *, *, *, *, *, *, *, *)
   DISPLAY(ICRn,   GDT_ICRnL,  *, *, *, *, *, *, *, *)
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
void Async_sleep_check(int pMode);
void Update_waveform();
void Update_compare_actions();
void Update_clock_source();
void Action_on_port(PORT, int, BOOL pMode = Counting_up);
void Log(const char *pFormat, ...);
void Log_register_write(int pId, WORD8 pData, unsigned char pMask);
int Value(int);
uint Get_io_cycles(void);
bool Is_disabled(BOOL pPRRWanted = TRUE);
void Update_register(REGISTER_ID, WORDSZ);
void On_XCLK_edge(EDGE pEdge);
void On_ICP_edge(EDGE pEdge);

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
#ifdef TIMER_N
   TMP_buffer.x(0);
#endif
   OCRA_buffer.x(0);
   OCRB_buffer.x(0);
   PRR = false;
   Sleep_mode = SLEEP_EXIT;
   Clock_source = CLK_UNKNOWN;
   Waveform = WAVE_UNKNOWN;
   Top = VAL_NONE;
   Prescaler_index = 0;
   Update_OCR = VAL_NONE;
   Async = ASY_NONE;
   Dirty = true;
}

WORD8 *On_register_read(REGISTER_ID pId)
//**************************************
// Notification about a read operation over the given register. This is only
// necessary if any kind of on-read action is needed: clear-on-read,
// temporary buffer read, etc. Otherwise it can be omitted (this case)
{
   // Registers cannot be read if the timer is disabled due to PRR
   if(PRR && !Async) {
      WARNING("Register read while disabled by PRR", CAT_TIMER, WARN_MISC);
      return &UNKNOWN8;
   }
   
   // If TIMER2 is in async mode, then reading TCNT2 returns the value from
   // a synchronization register. If TIMER2 is waking up from power-save mode
   // then reading TCNT2 immediately after wake up may return the stale value.
#ifdef TIMER_2
   if(Async && pId == TCNTn) {
      if(Tcnt_async != REG(TCNTn)) {
         WARNING("Reading stale TCNT2 after exiting SLEEP",
            CAT_TIMER, WARN_READ_BUSY);
      }
      return &Tcnt_async;
   }
#endif   

#ifdef TIMER_N
   // If reading the high byte of 16-bit TCNT and ICR registers, return the
   // TMP_buffer instead. If reading the TCNT/ICR low byte then also copy the
   // high byte into the TMP buffer.   
   switch(pId) {
      case TCNTnH:
      case ICRnH:
         // Verify that the TMP_buffer was previously written by performing
         // a read of the correct low byte register. Note that for register
         // reads, negative IDs are used with TMP_regid, while positive IDs
         // are used when writing registers.
         if(TMP_regid != -pId) {
            WARNING("Possibly incorrect read sequence from 16-bit register",
               CAT_TIMER, WARN_TIMERS_16BIT_READ);
         }            
         return &TMP_buffer;
      case TCNTn:
         TMP_regid = -TCNTnH;
         TMP_buffer = REG(TCNTnH);
         Dirty = true;
         break;
      case ICRn:
         TMP_regid = -ICRnH;
         TMP_buffer = REG(ICRnH);
         Dirty = true;
         break;
   }
   
   // If OCRx double buffering is in effect, the CPU always reads the buffer
   // instead of the real registers. Since the buffers are 16 bit, they have to
   // be separated into the high and low bytes depending on which register
   // is being accessed. Unlike TCNT and ICR, the OCR registers (or the buffers)
   // are read directly without going through the TMP_buffer.
   if(Update_OCR) {   
      switch(pId) {
         case OCRnAH:
            READ_buffer = OCRA_buffer >> 8;
            return &READ_buffer;
         case OCRnBH:
            READ_buffer = OCRB_buffer >> 8;
            return &READ_buffer;
         case OCRnA:
            READ_buffer = OCRA_buffer;
            return &READ_buffer;
         case OCRnB:
            READ_buffer = OCRB_buffer;
            return &READ_buffer;
      }
   }
#else 
   // If OCRx double buffering is in effect, the CPU always reads the buffer
   // instead of the real registers.
   if(Update_OCR) {
      switch(pId) {
         case OCRnA:
            return &OCRA_buffer;
         case OCRnB:
            return &OCRB_buffer;
      }
   }
#endif
   
   return NULL; // The normal register address will be read
}

void On_register_write(REGISTER_ID pId, WORD8 pData)
//**************************************************
// The micro is writing pData into the pId register This notification allows to
// perform all the derived operations. Forwards all register writes to
// Update_register() unless TIMER2 is in asynchronous mode. In that case this
// function sets up a pending register update after two asynchronous clock
// cycles.
{
   // Registers cannot be written if the timer is disabled due to PRR
   if(PRR && !Async) {
      WARNING("Register written while disabled by PRR", CAT_TIMER, WARN_MISC);
      return;
   }

#ifdef TIMER_2
   if(Async) {
      for(int i = 0; i < countof(ASSR_UB); i++) {
         if(pId == ASSR_UB[i]) {
            Log("Write temporary asynchronous %s register: %s",
               Assr_text[i], hex(pData));
         
            if(REG(ASSR)[i] == 1) {
               WARNING("Asynchronous register update already pending",
                  CAT_TIMER, WARN_WRITE_BUSY);
            }
            
            // The ATmega48/88/168 datasheet has an Errata for all revisions
            // that warns about possible lost interrupts if any register
            // is modified while in asynchronous mode and TCNT is 0xff. It's
            // not clear from the datasheet if this also applies to overflow
            // interrupts generated when TOP==OCRA or in PWM phase-correct
            // mode when overflow interrupts occur on BOTTOM.
            if(REG(TCNTn) == 0xff && pId != TCNTn) {
               WARNING("Errata: Asynchronous interrupts may be lost",
                  CAT_TIMER, WARN_WRITE_BUSY);
            }
            
            REG(ASSR).set_bit(i, 1);
            Async_update[i].Value = pData;
            Async_update[i].Ticks = Async_ticks;
            
            // Return so Update_register() is not called
            return;
         }
      }
   }
#endif
   
   Update_register(pId, pData);
}

void Update_register(REGISTER_ID pId, WORDSZ pData)
//**************************************************
// This function performs the real work of updating the register values. The
// actual On_register_write() function acts as a frontend which calls
// Update_register() unless TIMER2 is in asynchronous mode.
{
// Register selection macros include the "case", X bits check and logging.
#define case_register(r, m) \
   case r: {\
      Log_register_write(r, pData, m);

#define end_register } break;

#ifdef TIMER_N
   // If writing the low byte of 16-bit timer register, then combine this low
   // byte with the high byte from TMP_buffer into a single 16-bit value.
   switch(pId) {
      case TCNTn:
      case OCRnA:
      case OCRnB:
      case ICRn:
         pData = (TMP_buffer << 8) | pData;

         // Verify that the TMP_buffer was previously written to using the
         // high byte register ID that corresponds to this low byte register.
         if(TMP_regid != pId + 1) {
            WARNING("Possibly incorrect write sequence to 16-bit register",
               CAT_TIMER, WARN_TIMERS_16BIT_WRITE);
         }
   }      
#endif

   switch(pId) {

       case_register(TCCRnA, 0xF3)      // Timer Control Register A
       //---------------------------------------------------------
          REG(TCCRnA) = pData & 0xF3;   // $F3 = read-only bits mask
          Update_waveform();
          Update_compare_actions();
          Dirty = true;
       end_register

       case_register(TCCRnB, TCCRnB_MASK) // Timer Control Register B
       //---------------------------------------------------------
          REG(TCCRnB) = pData & TCCRnB_RW_MASK;  // FOCnA/FOCnB are write-only
          Update_waveform();
          Update_compare_actions();
          Update_clock_source();
          Dirty = true;

#ifdef TIMER_N
       end_register
       
       case_register(TCCRnC, 0xC0)      // Timer Control Register C
       //---------------------------------------------------------
#endif          
          // Handle FOC0A, FOC0B (Force Output Compare), only in non-PWM modes
          // Since writing TCCRnB could have changed the waveform mode and
          // compare actions, we do this check after the modes are updated.
          if(pData[7] == 1) {       // FOC0A = bit 7
             if(Waveform == WAVE_NORMAL || Waveform == WAVE_CTC)
                Log("OCnA force compare");
                Action_on_port(OCA, Action_comp_A);
          }
          if(pData[6] == 1) {       // FOC0B = bit 6
             if(Waveform == WAVE_NORMAL || Waveform == WAVE_CTC)
                Log("OCnB force compare");
                Action_on_port(OCB, Action_comp_B);
          }
       end_register

#ifdef TIMER_N
       //--------------------------------------------------
       // Writing high byte of 16-bit register always updates TMP register
       case TCNTnH:
       case OCRnAH:
       case OCRnBH:
       case ICRnH:
          Log_register_write(pId, pData, 0xFF);
          TMP_regid = pId;
          TMP_buffer = pData;
          Dirty = true;       
          break;

       case_register(ICRn, 0xFF)  // Input Capture Register
       //------------------------------------------------------
          if(Top == VAL_ICR) {
             REGHL(ICRn) = pData;
          } else {
             WARNING("ICRn is read-only if not used as TOP",
                CAT_TIMER, WARN_PARAM_BUSY);
          }
       end_register
#endif
       
       case_register(TCNTn, 0xFF) // Timer Counter Register
       //--------------------------------------------------
          REGHL(TCNTn) = pData & MASK[Top];
          Compare_blocked = true;
          // TODO: If TSM is on and clock perios is not 1 then the clock is not
          // really running and this warning should not be printed
          if(Clock_source != CLK_STOP && Clock_source != CLK_UNKNOWN)
             WARNING("TCNTn modified while running", CAT_TIMER, WARN_PARAM_BUSY);
       end_register

       case_register(OCRnA, 0xFF)  // Output Compare Register A
       //------------------------------------------------------
          OCRA_buffer = pData & MASK[Top];
          if(!Update_OCR) {
             REGHL(OCRnA) = pData & MASK[Top];;
          }
          Dirty = true; // Buffer static control need update
       end_register

       case_register(OCRnB, 0xFF) // Output Compare Register B
       //------------------------------------------------------
          OCRB_buffer = pData & MASK[Top];
          if(!Update_OCR) {
             REGHL(OCRnB) = pData & MASK[Top];;
          }
          Dirty = true;
       end_register
       
#ifdef TIMER_2
       case_register(ASSR, 0x60) // Asynchronous Status Register
       //------------------------------------------------------
          // EXCLK should be changed before asynchronous mode enabled
          if(pData[6] != REG(ASSR)[6] && (pData[5] == 1 || REG(ASSR)[5] == 1)) {
             WARNING("EXCLK bit in ASSR changed while AS2 bit is 1", CAT_TIMER,
                WARN_PARAM_BUSY);
          }
          
          // Update register immediately because Async_change() depends on it.
          // Only upsate EXCLK and AS2 and Async_change() will clear the 2UB
          // if leaving async mode.
          REG(ASSR) = (pData & 0x60) | (REG(ASSR) & 0x1F);

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
                         
             // If switching from synchronous to async mode, then update the
             // TCNT2 view that the MCU sees when TIMER2 is in async mode. If
             // TCNT2 was corrupted by Async_change() as per the GUI check box
             // setting, then Tcnt_async will also be corrupted at this
             // point.
             if(Async) {
                Tcnt_async = REG(TCNTn);
             }
          }
          
          // Update everything since registers values may be unknown now
          Update_waveform();
          Update_compare_actions();
          Update_clock_source();
          Dirty = true;
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

   // If using internal clock source, increment counter and launch next tick
   if(Clock_source == CLK_INTERNAL) {
      if(TSM && Timer_period != 1)   // TSM holds prescaler not direct clock
         return;
      Count();
      Go();
   }
   
   // If using asynchronous 32kHz XTAL, always increment prescaler and schedule
   // next oscillator tick. Async_tick() takes care of incrementing the counter.
#ifdef TIMER_2
   else if(Async == ASY_32K) {
      REMIND_ME(PERIOD_32K, ++Tick_signature);
      Async_tick();
   }
#endif
}

void On_port_edge(PORT pPort, EDGE pEdge, double pTime)
//*****************************************************
// Response to a digital input port edge. The EDGE type parameter (pEdge) can
// be RISE or FALL. Use port identifers as declared in DECLARE_PINS
{
   if(Is_disabled())
      return;

   switch (pPort) {
      case XCLK:  On_XCLK_edge(pEdge); break;
#ifdef TIMER_N
      case ICP:   On_ICP_edge(pEdge); break;
#endif
   }
}

#ifdef TIMER_N
void On_ICP_edge(EDGE pEdge)
//*****************************************************
// Handle input capture
{   
   // Using ICR as TOP value disable input capture
   if(Top != VAL_ICR) {
   
      // Check edge on ICP pin against ICESn bit in TCCRnB
      if( (REG(TCCRnB)[6] == 0 && pEdge == FALL) || 
          (REG(TCCRnB)[6] == 1 && pEdge == RISE) ) {
                   
         // Generate CAPT interrupt and copy TCNTn register to ICRn
         SET_INTERRUPT_FLAG(CAPT, FLAG_SET);
         REG(ICRnH) = REG(TCNTnH);
         REG(ICRn) = REG(TCNTn);
      }
   }
}
#endif

void On_XCLK_edge(EDGE pEdge)
//*****************************************************
// Handle external clock. Check the edge and call the general Count() procedure
{
   // TODO: Warn about transitions that are too fast for CPU clock? Datasheet recommends
   // max frequency clkIO/2.5. This is the Nyquist sampling frequency with some room for
   // clock variations. For TIMER2 external clock it should be clkIO/4.

   if(Clock_source == CLK_EXT_RISE && pEdge == RISE)
      Count();
   else if(Clock_source == CLK_EXT_FALL && pEdge == FALL)
      Count();
      
#ifdef TIMER_2
   // Schedule a tick if TIMER2 asynchronous operation is in use. The "Async"
   // variable has to be checked here instead of "Clock_source" because the
   // latter is not updated immediately in asynchronous mode due to the UB
   // bits in ASSR.
   else if(Async == ASY_EXT && pEdge == RISE) {
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
   PRR = false;
   Sleep_mode = SLEEP_EXIT;
   Prescaler_index = 0;
   Counting_up = true;
   Tick_signature = 0;
   Timer_period = 0;
   Waveform = WAVE_NORMAL;
   OCA_toggle_ok = true; // OCA toggle allowed by WAVE_NORMAL
#ifdef TIMER_N
   TMP_regid = 0;
   TMP_buffer = 0;
   Top = VAL_FFFF;
   Overflow = VAL_FFFF;
#else
   Top = VAL_FF;
   Overflow = VAL_FF;
#endif
   Clock_source = CLK_STOP;
   Update_OCR = VAL_NONE;
   Action_comp_A = ACT_NONE;
   Action_comp_B = ACT_NONE;
   Action_top_A = ACT_NONE;
   Action_top_B = ACT_NONE;
   TAKEOVER_PORT(OCA, false);  // Release ports; a reset can happen
   TAKEOVER_PORT(OCB, false);  // at any time...
#ifdef TIMER_2
   TAKEOVER_PORT(XCLK, false);  // Release TOSC1 pins in case async mode used
#endif
   Last_PSR = 0;
   Last_disabled = 0;
   Total_disabled = 0;
   Compare_blocked = true; // Prevent compare match immediately after reset 
   TSM = false;
   Async = ASY_NONE;
   Async_prescaler = 0;
   Async_interrupt = 0;
   Dirty = true;
}

void On_notify(int pWhat)
//**********************
// Notification coming from some other DLL instance. Used here to
// handle the PRR register and prescaler reset (coming from dummy component)
{
   int wasDisabled;

   switch(pWhat) {
      case NTF_PRR0:                 // A 0 set in PPR register bit for TIMER 0
         wasDisabled = Is_disabled();
         PRR = false;
         if(wasDisabled & !Is_disabled()) {
            Log("Enabled by PRR");
            Dirty = true;
            Go();
         }
         break;

      case NTF_PRR1:                 // A 1 set in PPR register bit for TIMER 0
         wasDisabled = Is_disabled();
         PRR = true;
         if(!wasDisabled && Is_disabled()) {
            Log("Disabled by PRR");
            Dirty = true;
            Go();
         }
         break;

      case NTF_TSM:                  // Timer sync mode; prescaler reset continuously asserted
         Log("Started TSM");
         TSM = true;
         Dirty = true;
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
            Dirty = true;
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

#ifdef TIMER_2
   // Warn if any 2UB bits in ASSR are set (updates pending) and TIMER2 is
   // entering a non IDLE sleep. The updates are not lost but they will not
   // complete until the AVR comes out of SLEEP mode. This is the case even if
   // entering power-save or ADC noice reduction mode where TIMER2 continues
   // to run in async mode.
   if(Sleep_mode == SLEEP_EXIT && pMode != SLEEP_IDLE) {
      if(REG(ASSR).get_field(4, 0) > 0) {
         WARNING("Entering SLEEP with pending updates to asynchronous registers",
            CAT_TIMER, WARN_PARAM_BUSY);
      }
   }

   // Check for duplicate interrupts that can occur in TIMER2 async mode
   if(Async) {
      Async_sleep_check(pMode);
   }
#endif

   Sleep_mode = pMode;
   
   if(wasDisabled != Is_disabled()) {

      if(Is_disabled()) {
         Log("Disabled by SLEEP");
      } else {
         Log("Exit from SLEEP");
      }

#ifdef TIMER_2
      if(Async) {
         Async_change();
      }
#endif         
   
      Dirty = true;
      Go();
   }
}

void On_gadget_notify(GADGET pGadget, int pCode)
//*********************************************
// Response to notification from "Log" checkbox and toggle flag
{
   if(pGadget == GDT_LOG && pCode == BN_CLICKED) {
      Debug ^= DEBUG_LOG;
   }
#ifdef TIMER_2
   if(pGadget == GDT_CRPT && pCode == BN_CLICKED) {
      Debug ^= DEBUG_ASYNC_CORRUPT;
   }
#endif   
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
// external clock tick on TOSC1. Note that the prescaler continues to run
// even if the clock source is stopped (i.e. Timer_period == 0)
{
   // Count how many times Async_tick() is called for the purposes of completing
   // any pending asynchronous register updates. Register updates will complete
   // even when in TSM mode and the asynchronous prescaler is not running.
   Async_ticks++;
   
   // Allow pending TIMER2 asynchronous register updates to complete if the
   // CPU is not in any sleep mode deeper than IDLE and at least 2
   // asynchronous clock ticks have elapsed.
   if(Sleep_mode == SLEEP_EXIT || Sleep_mode == SLEEP_IDLE) {
      for(int i = 0; i < countof(ASSR_UB); i++) {
         if(REG(ASSR)[i] == 1) {         
            if(Async_ticks - Async_update[i].Ticks >= 2) {
               REG(ASSR).set_bit(i, 0);
               Update_register(ASSR_UB[i], Async_update[i].Value);
            }
         }
      }
   }
   
   // Only increment prescaler if the reset is not asserted due to TSM
   if(!TSM)
      Async_prescaler++;

   // TSM stops prescaled clock but a direct clock will still tick the timer
   if(TSM && Timer_period != 1)
      return;
   
   // If the timer clock source is not stopped then tick the timer when the
   // prescaler counter reaches the requested divisor.
   if(Timer_period) {
      if((Async_prescaler + 1) % Timer_period == 0)
         Count();
   }

   // If asynchronous TIMER2 is running in either IDLE, ADC Noise reduction, or
   // no sleep, then keep the TCNT2 register that the MCU sees updated with
   // the real TCNT2 register. However, if timer is running in power-save
   // mode, the synchronized TCNT2 seen by the MCU does not change. If the MCU
   // wakes up and immediately reads TCNT2 after wake up then it will see the
   // old stale value that TCNT2 had before going to sleep.
   if(Sleep_mode != SLEEP_POWERSAVE) {
      Tcnt_async = REG(TCNTn);
   }
}

void Async_change()
//*****************************************
// Called when TIMER2 is switching to/from asynchronous mode or when TIMER2
// is going to sleep or waking up from power-down or standby while in
// asynchronous mode.
{
   // Switching the AS2 bit in ASSE as well as waking up from power-down or
   // standy (and presumebly when entering either sleep mode) can corrupt
   // all TIMER2 registers except for ASSR. They can optionally be set to
   // UNKNOWN forcing the AVR software to re-initialize them.
   if(Debug & DEBUG_ASYNC_CORRUPT) {
      REG(TCCRnA).x(0);
      REG(TCCRnB).x(0);
      REG(TCNTn).x(0);
      REG(OCRnA).x(0);
      REG(OCRnB).x(0);
      OCRA_buffer.x(0);
      OCRB_buffer.x(0);
   }

   // If leaving async mode (AS2=0) then cancel any pending asynchronous
   // register updates and clear 2UB bits.
   if(Async == ASY_NONE && REG(ASSR).get_field(4, 0) > 0) {
      WARNING("Pending updates to asynchronous registers lost",
         CAT_TIMER, WARN_PARAM_BUSY);
      REG(ASSR) = REG(ASSR) & 0x60;
   }
                
   // If the 32.768kHz crystal oscillator was (re)enabled then schedule
   // the first oscillator tick with REMIND_ME()
   if(Async == ASY_32K && !Is_disabled()) {
      REMIND_ME(PERIOD_32K, ++Tick_signature);
      // TODO: Warn about waiting 1 sec for 32K oscillator to stabilize
   }

   // Otherwise, the oscillator may be getting disabled due to leaving
   // asynchronous mode (AS2=0), going to sleep, or switching to external
   // clock signal on TOSC1. Update Tick_signature to cancel any 32.768kHz
   // oscillator ticks that may be pending.
   else {
      ++Tick_signature;
   }

   // If external clock selected, then TOSC1/TOSC2 is disconnected from the
   // normal digital I/O circuitry. This is simulated by forcing both
   // pins to input.
   // TODO: The pins will still cause PCINT interrupts. 
   // TODO: Notify DUMMY component so it can block PCINT interrupts on
   // TOSC1/TOSC2 pins. DUMMY component can also issue warning if async
   // mode enabled while external system clock is in use.
   if (Async) {
      TAKEOVER_PORT(XCLK, true, FORCE_INPUT);
   } else {
      TAKEOVER_PORT(XCLK, false);
   }
   
}

void Async_sleep_check(int pMode)
//*****************************************
// Entering power-save or ADC noise reduction sleep while in TIMER2 asynchronous
// mode can potentially cause duplicate interrupts that immediately wake up
// the MCU again. If entering one of the above sleep modes on the same TOSC
// cycle that previously generated any interrupts, then immediately reschedule
// them in this function. This problem will not occur if the interrupt is
// masked, but in that case the function will still issue a warning since there
// is no way for the TIMER2 component to check the mask registers.
{
   if(pMode != SLEEP_POWERSAVE && pMode != SLEEP_NOISE_REDUCTION) {
      return;
   }   
   if(Async_interrupt) {
      WARNING("Possible duplicate asynchronous interrupts when re-entering SLEEP",
         CAT_TIMER, WARN_MISC);
   }
   if(Async_interrupt & (1 << OVF)) {
      SET_INTERRUPT_FLAG(OVF, FLAG_SET);
   }
   if(Async_interrupt & (1 << CMPA)) {
      SET_INTERRUPT_FLAG(CMPA, FLAG_SET);
   }
   if(Async_interrupt & (1 << CMPB)) {
      SET_INTERRUPT_FLAG(CMPB, FLAG_SET);
   }   
}
#endif // #ifdef TIMER_2

void Go()
//******
// Schedule the next timer tick if the counter is not disabled and using the
// internal clock source. Called on every timer tick, each time the counter
// is re-enabled, and each time the clock source changes. The Tick_signature
// is updated each item to cancel any already pending ticks which may no longer
// be valid. For TIMER2 in 32kHz asynchronous mode, this function never
// schedules timer ticks.
{
   if(Is_disabled(false)) {
      // Track when prescaler clock first became disabled due to sleep mode
      // but not due to PRR (which only stops timer clock and not prescaler)
      if(!Last_disabled)
         Last_disabled = Get_io_cycles();
      return;
   }
   
   // If the prescaler clock was re-enabled, compute how long it was disabled for
   if(Last_disabled) {
      Total_disabled = (uint) GET_MICRO_INFO(INFO_CPU_CYCLES) - Last_disabled;
      Last_disabled = 0;
   }

   if(Is_disabled()) {
      return; // Don't schedule ticks if timer itself is still disabled by PRR
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
         WARNING("OCnA enabled but pin not defined as output in DDR", CAT_TIMER,
            WARN_TIMERS_OUTPUT);
      } else if(pPort == OCB) {
         WARNING("OCnB enabled but pin not defined as output in DDR", CAT_TIMER,
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
   // Track which interrupts have occurred on this timer cycle. Used by TIMER2
   // Async_check_sleep() to simulate duplicate interrupts when entering sleep.
   Async_interrupt = 0;

   // Do nothing on reserved/unknown waveform; we don't know the count sequence
   if(Waveform == WAVE_RESERVED || Waveform == WAVE_UNKNOWN) {
      return;
   }

   // Set overflow flag when count == TOP/MAX/BOTTOM depending on mode
   if(REGHL(TCNTn) == Value(Overflow)) {
      SET_INTERRUPT_FLAG(OVF, FLAG_SET);
      Async_interrupt |= (1 << OVF);
   }

   // Reverse counter direction in PWM PC/PFC mode when it reaches TOP/BOTTOM/MAX
   if(IS_WAVE_DUAL_SLOPE()) {
      if(REGHL(TCNTn) == Value(Top)) {
         Counting_up = false;
      }
      else if(REGHL(TCNTn) == 0) {
         Counting_up = true;
      }
   }

   // If in PWM PC/PFC mode, when the counter reaches TOP, the output compare
   // values must equal the result of an up-counting compare match. This
   // takes care of the special cases when the OCR registers equal BOTTOM
   // or are greater than TOP. It also handles the case where the compare
   // output changes without an OCR match to ensure the outputs are symmetric
   // around BOTTOM. The only exception to this rule is when OCR equals top.
   // In that case, the next code block will overwrite the port assignment with
   // the correct values. If using ACT_TOGGLE, this code block does nothing.
   if(IS_WAVE_DUAL_SLOPE() && REGHL(TCNTn) == Value(Top)) {
      Action_on_port(OCA, Action_top_A, true);
      Action_on_port(OCB, Action_top_B, true);
   }

   // In TIMER2 async mode output compare match is disabled if TCNT/OCRx
   // updates are pending in ASSR
#ifdef TIMER_2
   if(REG(ASSR).get_field(4, 2) <= 0)
#endif
   {
      // Update output pins if a compare output match occurs. A CPU write to
      // TNCTn blocks any output compare in the next timer clock cycle even if
      // the counter was stopped at the time the TNCT0 write occurred.
      if(!Compare_blocked) {
         if(REGHL(TCNTn) == REGHL(OCRnA)) {
            Action_on_port(OCA, Action_comp_A);
            SET_INTERRUPT_FLAG(CMPA, FLAG_SET);
            Async_interrupt |= (1 << CMPA);
         }
         if(REGHL(TCNTn) == REGHL(OCRnB)) {
            Action_on_port(OCB, Action_comp_B);
            SET_INTERRUPT_FLAG(CMPB, FLAG_SET);
            Async_interrupt |= (1 << CMPB);
         }
#ifdef TIMER_N         
         // If using ICR as TOP, then generate interrupt on compare match
         if(Top == VAL_ICR && REGHL(TCNTn) == REGHL(ICRn)) {
            SET_INTERRUPT_FLAG(CAPT, FLAG_SET);
         }
#endif
      }
   }
   Compare_blocked = false;
   
   // In fast PWM mode, all PWM cycles begin when counter reaches TOP. This is
   // true even when OCR equals TOP in which case this code block overwrites
   // the previous port assignment.
   if(Waveform == WAVE_PWM_FAST && REGHL(TCNTn) == Value(Top)) {
      Action_on_port(OCA, Action_top_A, false);     
      Action_on_port(OCB, Action_top_B, false);     
   }
   
   // If OCR double buffering in effect, then update real OCR registers when needed
   if(Update_OCR) {                         
      if(REGHL(TCNTn) == Value(Update_OCR)) {
         if(REGHL(OCRnA) != (OCRA_buffer & MASK[Top])) {
            Log("Updating double buffered register OCRnA: %s", hex(OCRA_buffer));
         }
         if(REGHL(OCRnB) != (OCRB_buffer & MASK[Top])) {
            Log("Updating double buffered register OCRnB: %s", hex(OCRB_buffer));
         }
         REGHL(OCRnA) = OCRA_buffer & MASK[Top];
         REGHL(OCRnB) = OCRB_buffer & MASK[Top];
      }
   }

   // Increment/decrement counter and clear after TOP was reached if not
   // decrementing. Also if in decrement mode, do not decrement past zero
   if(Counting_up && REGHL(TCNTn) == Value(Top)) {
      REGHL(TCNTn) = 0;
   } else if(Counting_up || REGHL(TCNTn) != 0) {
      REGHL(TCNTn).d( (REGHL(TCNTn).d() + (Counting_up ? 1 : -1)) & MASK[Top] );
   }
}

#ifdef TIMER_N

void Update_waveform()
//********************
// 16-bit Timers (TIMER1, TIMER3, etc.):
// Determine the waveform mode according to bits WGMxx located in TCCRnB TCCRnA
{
   int waveCode1 = REG(TCCRnA).get_field(1, 0);  // Bits 1, 0 of TCCRnA
   int waveCode2 = REG(TCCRnB).get_field(4, 3);  // Bit 4, 3 of TCCRnB
   int fullWaveCode = -1;                        // Initialize to unknown
   if(waveCode1 >= 0 && waveCode2 >= 0)
      fullWaveCode = waveCode2 * 4 + waveCode1;  // Combine both register's bits

   int newWaveform = WAVE_UNKNOWN;
   Top = VAL_NONE;
   Update_OCR = VAL_NONE;
   OCA_toggle_ok = false;
   
   switch(fullWaveCode) {       // Apply AVR manual Table 15-4

      case 0: // Normal timer mode (TOP=0xFFFF / 16-bit) 
         newWaveform = WAVE_NORMAL; Counting_up = true; OCA_toggle_ok = true;
         Top = VAL_FFFF; Update_OCR = VAL_NONE; Overflow = VAL_FFFF;
         break;

      case 1: // PWM phase correct (TOP=0xFF / 8-bit)
         newWaveform = WAVE_PWM_PC;
         Top = VAL_FF; Update_OCR = VAL_FF; Overflow = VAL_00;
         break;

      case 2: // PWM phase correct (TOP=0x1FF / 9-bit)
         newWaveform = WAVE_PWM_PC;
         Top = VAL_1FF; Update_OCR = VAL_1FF; Overflow = VAL_00;
         break;

      case 3: // PWM phase correct (TOP=0x3FF / 10-bit)
         newWaveform = WAVE_PWM_PC;
         Top = VAL_3FF; Update_OCR = VAL_3FF; Overflow = VAL_00;
         break;

      case 4: // CTC (TOP=OCRA)
         newWaveform = WAVE_CTC; Counting_up = true; OCA_toggle_ok = true;
         Top = VAL_OCRA; Update_OCR = VAL_NONE; Overflow = VAL_FFFF;
         break;

      case 5: // Fast PWM (TOP=0xFF / 8-bit)
         newWaveform = WAVE_PWM_FAST; Counting_up = true;
         Top = VAL_FF; Update_OCR = VAL_00; Overflow = VAL_FF;
         break;

      case 6: // Fast PWM (TOP=0x1FF / 9-bit)
         newWaveform = WAVE_PWM_FAST; Counting_up = true;
         Top = VAL_1FF; Update_OCR = VAL_00; Overflow = VAL_1FF;
         break;

      case 7: // Fast PWM (TOP=0x3FF / 10-bit)
         newWaveform = WAVE_PWM_FAST; Counting_up = true;
         Top = VAL_3FF; Update_OCR = VAL_00; Overflow = VAL_3FF;
         break;

      case 8: // PWM phase and frequency correct (TOP=ICR)
         newWaveform = WAVE_PWM_PFC;
         Top = VAL_ICR; Update_OCR = VAL_00; Overflow = VAL_00;
         break;

      case 9: // PWM phase and frequency correct (TOP=OCRA)
         newWaveform = WAVE_PWM_PFC; OCA_toggle_ok = true;
         Top = VAL_OCRA; Update_OCR = VAL_00; Overflow = VAL_00;
         break;

      case 10: // PWM phase correct (TOP=ICR)
         newWaveform = WAVE_PWM_PC;
         Top = VAL_ICR; Update_OCR = VAL_ICR; Overflow = VAL_00;
         break;

      case 11: // PWM phase correct (TOP=OCRA)
         newWaveform = WAVE_PWM_PC; OCA_toggle_ok = true;
         Top = VAL_OCRA; Update_OCR = VAL_OCRA; Overflow = VAL_00;
         break;

      case 12: // CTC (TOP=ICR)
         newWaveform = WAVE_CTC; Counting_up = true; OCA_toggle_ok = true;
         Top = VAL_ICR; Update_OCR = VAL_NONE; Overflow = VAL_FFFF;
         break;

      case 13: // Reserved
         newWaveform = WAVE_RESERVED;
         Top = VAL_NONE; Update_OCR = VAL_NONE; Overflow = VAL_NONE;
         break;
         
      case 14: // Fast PWM (TOP=ICR)
         newWaveform = WAVE_PWM_FAST; Counting_up = true; OCA_toggle_ok = true;
         Top = VAL_ICR; Update_OCR = VAL_00; Overflow = VAL_ICR;
         break;

      case 15: // Fast PWM (TOP=OCRA)
         newWaveform = WAVE_PWM_FAST; Counting_up = true; OCA_toggle_ok = true;
         Top = VAL_OCRA; Update_OCR = VAL_00; Overflow = VAL_OCRA;
         break;
   }

   if(newWaveform != Waveform) {
      Waveform = newWaveform;
      Log("Updating waveform: %s (TOP=%s)", Wave_text[Waveform], Top_text[Top]);
      if(Clock_source != CLK_STOP) // TODO: Don't issue warning if in TSM mode
      	WARNING("Changing waveform while the timer is running", CAT_TIMER, WARN_PARAM_BUSY);
      if(Waveform == WAVE_RESERVED)
         WARNING("Reserved waveform mode", CAT_TIMER, WARN_PARAM_RESERVED);
   }
}

#else // #ifdef TIMER_N

void Update_waveform()
//********************
// 8-bit Timers (TIMER0, TIMER2):
// Determine the waveform mode according to bits WGMxx located in TCCRnB TCCRnA
{
   int waveCode1 = REG(TCCRnA).get_field(1, 0);  // Bits 1, 0 of TCCRnA
   int waveCode2 = REG(TCCRnB).get_field(3, 3);  // Bit 3 of TCCRnB
   int fullWaveCode = -1;                        // Initialize to unknown
   if(waveCode1 >= 0 && waveCode2 >= 0)
      fullWaveCode = waveCode2 * 4 + waveCode1;  // Combine both register's bits

   int newWaveform = WAVE_UNKNOWN;
   Top = VAL_NONE;
   Update_OCR = VAL_NONE;
   OCA_toggle_ok = false;
   
   switch(fullWaveCode) {       // Apply AVR manual (table #51)

      case 0: // Normal timer mode (TOP=0xFF)
         newWaveform = WAVE_NORMAL; Counting_up = true; OCA_toggle_ok = true;
         Top = VAL_FF; Update_OCR = VAL_NONE; Overflow = VAL_FF;
         break;

      case 1: // PWM phase correct (TOP=0xFF)
         newWaveform = WAVE_PWM_PC;
         Top = VAL_FF; Update_OCR = VAL_FF; Overflow = VAL_00;
         break;

      case 2: // CTC (TOP=OCRA)
         newWaveform = WAVE_CTC; Counting_up = true; OCA_toggle_ok = true;
         Top = VAL_OCRA; Update_OCR = VAL_NONE; Overflow = VAL_FF;
         break;

      case 3: // Fast PWM (TOP=0xFF)
         newWaveform = WAVE_PWM_FAST; Counting_up = true;
         Top = VAL_FF; Update_OCR = VAL_00; Overflow = VAL_FF;
         break;

      case 5: // PWM phase correct (TOP=OCRA)
         newWaveform = WAVE_PWM_PC; OCA_toggle_ok = true;
         Top = VAL_OCRA; Update_OCR = VAL_OCRA; Overflow = VAL_00;
         break;

      case 7: // Fast PWM (TOP=OCRA)
         newWaveform = WAVE_PWM_FAST; Counting_up = true; OCA_toggle_ok = true;
         Top = VAL_OCRA; Update_OCR = VAL_00; Overflow = VAL_OCRA;
         break;

      case 4: case 6:   // Reserved
         newWaveform = WAVE_RESERVED; OCA_toggle_ok = true;
         Top = VAL_NONE; Update_OCR = VAL_NONE; Overflow = VAL_NONE;
         break;
   }

   if(newWaveform != Waveform) {
      Waveform = newWaveform;
      Log("Updating waveform: %s (TOP=%s)", Wave_text[Waveform], Top_text[Top]);
      if(Clock_source != CLK_STOP) // TODO: Don't issue warning if in TSM mode
      	WARNING("Changing waveform while the timer is running", CAT_TIMER, WARN_PARAM_BUSY);
      if(Waveform == WAVE_RESERVED)
         WARNING("Reserved waveform mode", CAT_TIMER, WARN_PARAM_RESERVED);
   }
}
#endif // #ifdef TIMER_N

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
         if(OCA_toggle_ok) {
            Action_comp_A = ACT_TOGGLE;
         } else {
            Action_comp_A = ACT_NONE;
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
         if(Waveform == WAVE_PWM_FAST || IS_WAVE_DUAL_SLOPE()) {
#ifdef TIMER_N
            Action_comp_B = ACT_NONE;          // Tables 15-2, 15-3 manual         
#else
            Action_comp_B = ACT_RESERVED;      // Tables #49/50 manual
#endif
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
      WARNING("OCnA enabled but pin not defined as output in DDR", CAT_TIMER,
         WARN_TIMERS_OUTPUT);
   }
   rc = TAKEOVER_PORT(OCB, Action_comp_B != ACT_NONE && Action_comp_B != ACT_RESERVED);
   if(rc == PORT_NOT_OUTPUT) {
      WARNING("OCnB enabled but pin not defined as output in DDR", CAT_TIMER,
         WARN_TIMERS_OUTPUT);
   }
   
   // Log any changes to the compare output modes
   if(oldAction_comp_A != Action_comp_A) {
      Log("Updating OCnA mode: %s", Action_text[Action_comp_A]);
   }
   if(oldAction_comp_B != Action_comp_B) {
      Log("Updating OCnB mode: %s", Action_text[Action_comp_B]);
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
         newClockSource = CLK_INTERNAL;
         newPrescIndex = clkBits;
         
#ifdef TIMER_2
         // Asynchronous mode overrides clock selection
         switch(Async) {
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

void On_update_tick(double pTime)
//************************************************
// Called periodically to refresh static controls showing the status, etc.
// The GUI is only updated if VAR(Dirty) is true to indicate that internal
// state has changed. WORD_8_VIEW_c controls are refreshed automatically. 
{
   // Do nothing if state not changed since last On_update_tick()
   if (!Dirty) {
      return;
   }
   Dirty = false;

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

#ifdef TIMER_N
   // Output TMP (high byte for 16-bit registers) value in hex
   SetWindowText(GET_HANDLE(GDT_TMP), hex(TMP_buffer));
#endif
}
/* >>> Improvement: a set of variables, Handle_xxx, can be declared at DECLARE_VAR, to
       keep the windows handles just once at the beginning, instead of calling
       every time GET_HANDLE() */

int Value(int pMode)
//*************************
// Given a VAL_xxx constant, return the TOP value associated with it
{
   switch(pMode) {
      case VAL_00:
         return 0x00;
      case VAL_FF:
         return 0xFF;
      case VAL_1FF:
         return 0x1FF;
      case VAL_3FF:
         return 0x3FF;
      case VAL_FFFF:
         return 0xFFFF;
      case VAL_OCRA:
         return REGHL(OCRnA).d();
#ifdef TIMER_N
      case VAL_ICR:
         return REGHL(ICRn).d();
#endif
      default:
         BREAK("Internal error: Value(???)");
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

bool Is_disabled(BOOL pPRRWanted)
//*************************
// Return TRUE if the timer is really disabled based on the current sleep mode and
// the bit in PRR. Idle sleep never disabled any timer. Power-down and standby sleep
// disables all timers under all conditions. ADC noise reduction sleep disables all
// timers except TIMER2 when it's in asynchronous mode. Power-save disables any
// timer except TIMER2 in either synchronous or asynchronous mode. Although ADC
// and power-save sleep don't disable TIMER2 in asynchronous mode, they do put
// any asynchronous register updates on hold (see Async_tick). If pPRRWanted is
// FALSE then only consider the sleep mode and not PRR; this is used for prescaler
// synchronization since the PRR bit does not stop the prescaler.
{
   switch(Sleep_mode) {
      case SLEEP_NOISE_REDUCTION:
#ifdef TIMER_2
         return Async ? false : true;
#else
      case SLEEP_POWERSAVE:           
#endif   
      case SLEEP_STANDBY:
      case SLEEP_POWERDOWN:
         return true;

      // SLEEP_EXIT and SLEEP_IDLE (and for TIMER_2 also SLEEP_POWERSAVE)
      default:
         return Async ? false : (pPRRWanted ? PRR : false);      
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

void Log_register_write(int pId, WORD8 pData, unsigned char pMask)
//*************************
// Called from every 'case XXX:' statement when handling On_register_write().
// Parameter pMask is a bitmask of useful (i.e. not reserved) bits in the
// register. If the pData value being written to the register contains any
// unknown bits in the pMask position then issue a warning. In either case
// Log() a message indicating the register is being written with pData. The
// pId is used to look up the true register name (as given in the .ini file)
// for the warning and log messages.
{
   if((pData.x() & pMask) != pMask) {      
      char strBuffer[64];
      char *strName = reg(pId);      
      
      snprintf(strBuffer, 64, "Unknown bits (X) written into %s register",
         strName);
         
      WARNING(strBuffer, CAT_MEMORY, WARN_MEMORY_WRITE_X_IO);
      Log("Write register %s: $??", strName);      
   } else {
      if(Debug & DEBUG_LOG) { // Don't waste time in reg() if not logging
         char *strName = reg(pId);
         Log("Write register %s: $%02X", strName, pData.d() & pMask);
      }
   }
}
