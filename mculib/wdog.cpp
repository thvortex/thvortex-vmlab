// =============================================================================
// Component Name: AVR watchdog timer peripheral
//
// Copyright (C) 2010 Wojciech Stryjewski <thvortex@gmail.com>
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

// NOTE: Because the VMLAB API does not have a "pTime" argument in
// On_register_write(), it's necessary to keep track of the explicit prescaler
// counter value and to only use REMIND_ME() with a fixed 128kHz period.

#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#pragma hdrstop

#define IS_PERIPHERAL       // To distinguish from a normal user component
#include "blackbox.h"
#include "useravr.h"
#include "wdog.h"

int WINAPI DllEntryPoint(HINSTANCE, unsigned long, void*) {return 1;} // is DLL
// =============================================================================

// Maximum value allowed in WDP field
#define MAX_PRESCALER_INDEX 9

// Period of one watchdog timer tick of 128kHz clock
#define WDOG_PERIOD (1.0 / 128000)

// Watchdog mode as a combination of WDE and WDIE bits
const char *Mode_text[] = {
   "?", "Disabled", "Interrupt", "Reset", "Interrupt and Reset"
};
const char *Prescaler_text[] = {
   "?", "2K", "4K", "8K", "16K", "32K", "64K", "128K", "256K", "512K", "1024K",
   "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved"
};

// Involved ports. Keep same order as in .INI file "Port_map = ..." who
// does the actual assignment to micro ports PD0, etc. This allows multiple instances
// to be mapped into different port sets
//
DECLARE_PINS            
END_PINS

// Involved registers. Use same order as in .INI file "Register_map"
// These are IDs or indexes. The real data is stored in a hidden WORD8
// type array. (see "blackbox.h" for details). The WORD8 data is referred
// with the REG(xxx) macro.
//
DECLARE_REGISTERS
   WDTCSR
END_REGISTERS

// Involved interrupts. Use same order as in .INI file "Interrupt_map"
// Like in  registers, these are IDs; what is important is the order.
// It is "Interupt_map" who assigns the index into the actual interrupt name, 
// therefore, different instances can use a different interrupt set
//
DECLARE_INTERRUPTS
   WDT
END_INTERRUPTS 

// Wrapper macro to call WARNING() since watchdog has only one category
#define Warn(text) WARNING(text, CAT_WATCHDOG, WARN_WATCHDOG_SUSPICIOUS_USE)

// Action codes used with REMIND_ME() -> On_remind_me() (must tbe even ints)
enum { RMD_AUTOCLEAR_WDCE };

// =============================================================================
// Declare variables here to allow multiple instances to be placed
//
DECLARE_VAR   
   bool Wdton;            // True if the WDTON fuse if programmed (set to 0)
   bool Wdrf;             // True if the WDRF flag is set to 1 in MCUSR
   int Mask;              // Register mask for known/unknown bits

   UINT Count;            // Current prescaler counter value
   int Prescaler;         // Prescaler divisor selected by WDP bits
   int Tick_signature;    // For REMIND_ME, to validate ticks
   
   bool Log;              // True if the "Log" checkbox button is checked
   bool Dirty;            // True if "Mode" and "Clock" fields need update
   bool Dirty_time;       // True if "Time Left" field needs update
END_VAR

USE_WINDOW(WINDOW_USER_1); // Window to display registers, etc. See .RC file

REGISTERS_VIEW
//           ID     .RC ID    substitute "*" by bit names: bi7name, ..., b0
   DISPLAY(WDTCSR, GDT_WDTCSR, WDIF, WDIE, WDP3, WDCE, WDE, WDP2, WDP1, WDP0)
END_VIEW

// =============================================================================
// Helper Functions

//*************************
// Extract the Watchdog Timer Prescaler (WDP) bits and return as single field
int Wdp(const WORD8 &pData)
{
   return ((pData & 0x7) | ((pData & 0x20) >> 2)).get_field(3, 0);
}

//*************************
// Extract the WDE and WDIE bits as a single field
int Mode(const WORD8 &pData)
{
   return (((pData & 0x8) >> 2) | ((pData & 0x40) >> 6)).get_field(1, 0);
}

void Log(const char *pFormat, ...)
//*************************
// Wrapper around the PRINT() function to provide printf() like functionality
// This function also automatically prepends the instance name to the formatted
// message. The message is only printed if logging is enabled in GUI.
{
   if(VAR(Log)) {   
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
      if(VAR(Log)) { // Don't waste time in reg() if not logging
         char *strName = reg(pId);
         Log("Write register %s: $%02X", strName, pData.d() & pMask);
      }
   }
}

void Go()
//*************************
// Schedule the next watchdog timer tick
{
   VAR(Tick_signature) += 2;            
   REMIND_ME(WDOG_PERIOD, VAR(Tick_signature));
}

void Count()
//*************************
// Called on every tick of the watchdog timer if the watchdog is enabled
// (i.e. WDE/WDIE are both known and at least one is true). Increments the
// prescaler counter, checks counter against the prescaler divisor for
// a watchdog timeout, and schedules a new watchdog timer tick.
{
   // Increment the prescaler counter. Real counter is only 20 bits but for
   // the modulo arithmetic it doesn't matter that a 32 bit integer is used.
   // The "Time_left" field will also need updating as a result.
   VAR(Count)++;
   VAR(Dirty_time) = true;

   // Schedule next timer tick to keep watchdog running. If watchdog becomes
   // disabled, then On_register_write() will invalide the Tick_signature.
   Go();

   // If the prescaler divisor is valid (no unknown bits and not a reserved
   // value), then compare it against the accumulated ticks and either shedule
   // an interrupt or do a system reset based on the current watchdog mode.
   if(VAR(Prescaler) && VAR(Count) % VAR(Prescaler) == 0) {
   
      // If WDIE=1 then using either Interrupt or Interrupt/Reset mode
      if(REG(WDTCSR)[6] == 1) {
         SET_INTERRUPT_FLAG(WDT, FLAG_SET);
         REG(WDTCSR).set_bit(7, 1);
      }
      
      // Otherwise WDE=1 should be set for Reset only mode
      else if(REG(WDTCSR)[3] == 1) {
         RESET(RESET_WATCHDOG);
      }
      
      // Should never happen; unknown/zero mode should have invalidated tick
      else {
         BREAK("Internal WDOG error in Count()");
      }
   }
}

// =============================================================================
// Callback functions. These functions are called by VMLAB at the proper time

const char *On_create()                   // Mandatory function
{
   return NULL;
}
void On_destroy()                         //     "        "
{
}

void On_simulation_begin()                //     "        "
//**********************
// Called at the beginning of simulation. The rest of initialization
// happens inside On_reset() since that will always get called immediately
// after On_simulation_begin().
{
   // Save the state of the WDTON fuse which locks the WDE and WDIE bits
   // to respectively 1 and 0, forcing the watchdog into system reset mode.
   VAR(Wdton) = GET_FUSE("WDTON") == 0;
   if(VAR(Wdton)) {
      PRINT("Watchdog always on (fuse WDTON=0)");
      VAR(Mask) = 0xB7; // All bits except WDE & WDIE writable
   } else {
      VAR(Mask) = 0xFF; // All bits writable
   }

   // The Tick_signature is always incremented by 2 and therefore it will
   // always be an odd number. This allows even values in pAux to specify
   // alternate functions. Note that the Tick_signature is not invalidated
   // after a reset because the watchdog timer runs independently.
   VAR(Tick_signature) = 1;      

   // The count is not zeroed on a reset because watchdog keeps runnning
   VAR(Count) = 0;   
}

void On_simulation_end()
//**********************
// If the simulation is ending, set all registers and text fieds to unknown.
{
   REG(WDTCSR) = WORD8(0, 0);
   SetWindowText(GET_HANDLE(GDT_MODE), "?");
   SetWindowText(GET_HANDLE(GDT_CLOCK), "?");
   
   // Prevent On_update_tick() from changing mode/clock text field but
   // ensure the "Time Left" field is changed to "? ms".
   VAR(Prescaler) = 0;
   VAR(Dirty) = false;
   VAR(Dirty_time) = true;
   
   // WDRF bit in MCUSR is always 0 after a power-on reset
   VAR(Wdrf) = false;
}

void On_register_write(REGISTER_ID pId, WORD8 pData)
//**********************
// The micro is writing pData into the pId register This notification
// allows to perform all the derived operations.
// Register selection macros include the "case", X bits check and logging
{
   switch(pId) {
      case WDTCSR:
         Log_register_write(WDTCSR, pData, VAR(Mask));

         int oldMode = Mode(REG(WDTCSR));

         // Bits 0-2,5 - WDP: Watchdog Timer Prescaler
         // ----------------------------------------         
         int oldWdp = Wdp(REG(WDTCSR));
         int newWdp = Wdp(pData);

         // Changing WDP bits only allowed if WDCE already set
         if(newWdp != oldWdp) {
            if(REG(WDTCSR)[4] != 1) {
               Warn("Cannot change WDP bits if WDCE is not already set");
            } else {
               REG(WDTCSR) = (REG(WDTCSR) & ~0x27) | pData & 0x27;
               Log("Update prescaler: %s", Prescaler_text[newWdp + 1]);

               if(newWdp > MAX_PRESCALER_INDEX) {
                  Warn("Reserved WDP value written to WDTCSR");
               }
               
               // Decode WDP field to prescaler divisor
               if(newWdp >= 0 && newWdp <= MAX_PRESCALER_INDEX) {
                  VAR(Prescaler) = 0x800 << newWdp;
               } else {
                  VAR(Prescaler) = 0;
               }
               
               // Both "Time Left" field and "Clock" field need update
               VAR(Dirty_time) = true;
               VAR(Dirty) = true;
            }
         }
         
         // Bit 3 - WDE: Watchdog System Reset Enable
         // ----------------------------------------
         // Writing WDE=0 only allowed under special circuimstances
         if(pData[3] == 0 && REG(WDTCSR)[3] != 0) {
            if(VAR(Wdton)) {
               Warn("Cannot set WDE=0 if fuse WDTON=0");
            } else if(VAR(Wdrf)) {
               Warn("Cannot set WDE=0 if WDRF=1 in MCUSR");
            } else if(REG(WDTCSR)[4] != 1) {
               Warn("Cannot set WDE=0 if WDCE is not already set");
            } else {
               REG(WDTCSR).set_bit(3, 0);
            }
         }
         
         // If WDTON=0 (programmed) then WDE is read only and always 1
         // If WDTON=1 then writing WDE=X or WDE=1 always allowed
         else if(!VAR(Wdton)) { 
            REG(WDTCSR).set_bit(3, pData[3]);
         }
         
         // Bit 4 - WDCE: Watchdog Change Enable
         // ----------------------------------------
         // Writing WDCE=1 only allowed if also writing WDE=1 or if WDCE
         // was already set to 1 in WDTCSR.
         if(pData[4] == 1 && REG(WDTCSR)[4] != 1) {
            if(pData[3] == 1) {
               REG(WDTCSR).set_bit(4, 1);
               REMIND_ME2(4, RMD_AUTOCLEAR_WDCE);
            } else {
               Warn("Must write both WDCE=1 and WDE=1 to set WDCE");          
            }         
         }
         
         // Writing WDCE=X only allowed if WDE=1 or WDE=X
         else if(pData[4] == UNKNOWN && REG(WDTCSR)[3] != 0) {
            REG(WDTCSR).set_bit(4, UNKNOWN);
         }

         // Writing WDCE=0 is always allowed
         else if(pData[4] == 0) {
            REG(WDTCSR).set_bit(4, 0);
         }
         
         // Bit 6 - WDIE: Watchdog Interrupt Enable
         // ----------------------------------------
         // Changes to WDIE bit only allowed if WDTON not programmed
         
         // WDIE locked to 0 if WDTON programmed; warning if writing WDIE=1
         if(pData[6] == 1 && VAR(Wdton)) {
            Warn("Cannot set WDIE=1 if fuse WDTON=0");         
         }
         
         // Writing WDIE=X silently ignored if WDTON programmed
         else if(!VAR(Wdton)) { 
            SET_INTERRUPT_ENABLE(WDT, pData[6] == 1);
            REG(WDTCSR).set_bit(6, pData[6]);
         }
         
         // Bit 7 - WDIF: Watchdog Interrupt Flag
         // ----------------------------------------
         // Writing WDIF=1 clears interrupt flag; writing WDIF=0 has no effect
         if(pData[7] == 1) {
            SET_INTERRUPT_FLAG(WDT, FLAG_CLEAR);
            REG(WDTCSR).set_bit(7, 0);
         }

         // Log changes made to the timer's mode (WDIE and WDE fields)
         int newMode = Mode(REG(WDTCSR));
         if(oldMode != newMode) {
            Log("Updating mode: %s", Mode_text[newMode + 1]);
            VAR(Dirty) = true;
         }

         // If watchdog just became enabled, then schedule new tick
         if(oldMode <= 0 && newMode > 0) {
            Go();
         }
         
         // If watchdog just became disabled, then cancel pending tick
         if(oldMode > 0 && newMode <= 0) {
            VAR(Tick_signature) += 2;
         }
         
         break;
   }
}   

void On_reset(int pCause)
//***********************
// Initialize registers to the desired value. Note that the watchdog timer
// is not automatically disabled here nor is the watchdog counter reset
// since the watchdog is independent of the MCU and can continue a
{   

   // The initial state of the WDE bit depends on the reset. If the MCU was
   // reset due to a previous watchdog timeout, then the WDE pin is set to 1
   // and becomes read-only until the WDRF flag is cleared in MCUSR.
   if(pCause == RESET_WATCHDOG) {
      VAR(Wdrf) = true;
   }   
   
   // If fuse WDTON=0 (programmed) or WRDF=1 in MCUSR (either from this
   // RESET_WATCHDOG, from a previous RESET_WATCHDOG, or because AVR code
   // previously set WRDF=1 manually) then WDE=1 after reset.
   int oldMode = Mode(REG(WDTCSR));
   if(VAR(Wdton) || VAR(Wdrf)) {
      REG(WDTCSR) = 0x08;
   } else {
      REG(WDTCSR) = 0;
   }
   int newMode = Mode(REG(WDTCSR));
   
   // Initial WDP value in MCUSR corresponds to 2K prescaler
   VAR(Prescaler) = 2048;
      
   // If the watchdog not already running (either WDTON=0 and this is the
   // power on reset or because user initiated watchdog reset from GUI)
   // then schedule the first watchdog timer tick.
   if(oldMode <= 0 && newMode > 0) {
      Go();
   }
   
   // If watchdog was already running and became disabled after reset
   // (e.g. WDIE was 1 before an external reset) then invalidate pending
   // timer tick.
   if(oldMode > 0 && newMode <= 0) {
      VAR(Tick_signature) += 2;
   }

   // Force the GUI to display new values
   VAR(Dirty) = true;
   VAR(Dirty_time) = true;
}

void On_remind_me(double pTime, int pAux)
//***************************************
// Response to REMIND_ME() used implement the watchdog timeer ticks and the
// autoclearing of the WDCE bit 4 system clock cycles after it was set.
{
   switch(pAux) {
      case RMD_AUTOCLEAR_WDCE:
         if(REG(WDTCSR)[4] != 0) {
            Warn("WDCE cleared by hardware; previously set 4 cycles ago");
         }
         REG(WDTCSR).set_bit(4, 0);
         break;
         
      default:
         if(pAux == VAR(Tick_signature)) {
            Count();
         }
         break;
   }
}

void On_notify(int pWhat)
//***********************
// Notification coming from some other DLL instance. Used here to handle
// the WDR instruction and to handle the clearing of the WDRF bit in MCUSR
// which allows clearing of the WDE bit.
{
   switch(pWhat) {
   
      // The WDR instruction was executed; reset watchdog counter
      case NTF_WDR:
         VAR(Count) = 0;
         VAR(Dirty_time) = true;
         break;

      // The WDRF bit in MCUSR was cleared; WDE is read-write again
      case NTF_WDRF0:
         VAR(Wdrf) = false;
         break;
         
      // The WDRF bit in MCUSR was set; WDE is set and becomes read-only
      // If the watchdog was previously disabled, then schedule new tick
      case NTF_WDRF1:
         VAR(Wdrf) = true;
         if(Mode(REG(WDTCSR)) <= 0) {
            Go();
         }
         REG(WDTCSR).set_bit(3, 1);
         VAR(Dirty) = true;
         break;
   }
}

void On_gadget_notify(GADGET pGadget, int pCode)
//*********************************************
// Response to Win32 notification coming from buttons, etc.
{
   if(pGadget == GDT_LOG && pCode == BN_CLICKED) {
      VAR(Log) ^= 1;
   }
}

void On_update_tick(double pTime)
//*******************************
// Called periodically to refresh static controls showing the status, etc.
// If the watchdog timer is enabled, the "Timeout" field is updated to show
// remaining time based on the current "pTime". The rest of the GUI is only
// updated if the WDTCSR register has changed since the last On_update_tick().
{
   // Update "Time Left" If either prescaler divisor or count changed
   if(VAR(Dirty_time)) {
   
      // If the prescaler divisor is known, update the "Time Left" based on the
      // the current counter value and the selected divisor. If the divisor is
      // unknown then only display "? ms".
      if(VAR(Prescaler)) {
         int cycles = VAR(Prescaler) - VAR(Count) % VAR(Prescaler);
         SetWindowTextf(GET_HANDLE(GDT_TIME), "%.0f ms", cycles * WDOG_PERIOD * 1000);
      } else {
         SetWindowText(GET_HANDLE(GDT_TIME), "? ms");
      }
      
      VAR(Dirty_time) = false;
   }

   // Only upate Mode and Clock fields if WDTCSR changed since last update
   if(VAR(Dirty)) {
      int mode = Mode(REG(WDTCSR));
      
      // Update mode display as a function of WDE and WDIE
      SetWindowText(GET_HANDLE(GDT_MODE), Mode_text[mode + 1]);
      
      // Update clock display; watchdog timer disabled if WDE=0 and WDIE=0
      SetWindowTextf(GET_HANDLE(GDT_CLOCK),
         (mode == 0) ? "Disabled" : "128kHz / %s",
         Prescaler_text[Wdp(REG(WDTCSR)) + 1]);
         
      VAR(Dirty) = false;
   }
}

void On_interrupt_start(INTERRUPT_ID pId)
//***********************************
// Called when MCU begins to execute the watchdog interrupt. Clears the
// interrupt flag (WDIF) and the interrupt enable (WDIE) flag to switch the
// watchdog into reset only mode.
{
   switch(pId) {
      case WDT:
         // Acknowledge the interrupt by clearing the interrupt flag
         REG(WDTCSR).set_bit(7, 0);
         
         // If WDE=1 (Interrupt and Reset), then set WDIE=0 (Reset only)
         if(REG(WDTCSR)[3] == 1) {
            REG(WDTCSR).set_bit(6, 0);
            VAR(Dirty) = true;
         }
         
         break;
   }
}