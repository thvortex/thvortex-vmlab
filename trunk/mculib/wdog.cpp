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

#include <windows.h>
#include <commctrl.h>
#pragma hdrstop

#define IS_PERIPHERAL       // To distinguish from a normal user component
#include "blackbox.h"
#include "useravr.h"
#include "wdog.h"

int WINAPI DllEntryPoint(HINSTANCE, unsigned long, void*) {return 1;} // is DLL
// =============================================================================

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

// Action codes to be used with REMIND_ME2() -> On_remind_me(),
enum { RMD_AUTOCLEAR_WDCE, RMD_TIMEOUT };

// =============================================================================
// Declare variables here to allow multiple instances to be placed
//
DECLARE_VAR
   int Tick_signature;    // For REMIND_ME2, to validate ticks
   
   bool Wdton;            // True if the WDTON fuse if programmed (set to 0)
   bool Wdrf;             // True if the WDRF flag is set to 1 in MCUSR
   int Mask;              // Register mask for known/unknown bits
   
   bool Log;              // True if the "Log" checkbox button is checked
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
   return ((pData & 0x5) | ((pData & 0x20) >> 2)).get_field(3, 0);
}

//*************************
// Extract the Watchdog Timer Prescaler (WDP) bits and return as single field
#if 0
int Wdp(const WORD8 &pData)
{
   int low = pData.get_field(2, 0);
   int high = pData[5];
   return (low == -1 | high == -1) ? (-1) : ((high << 3) | low);
}
#endif

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
{
   // Save the state of the WDTON fuse which locks the WDE and WDIE bits
   // to respectively 1 and 0, forcing the watchdog into system reset mode.
   VAR(Wdton) = GET_FUSE("WDTON") == 0;
}

void On_simulation_end()
//**********************
{
   REG(WDTCSR) = WORD8(0, 0);
   // TODO: Set Last_WDTCSR equal to the same value
}

void On_register_write(REGISTER_ID pId, WORD8 pData)
//**************************************************
// The micro is writing pData into the pId register This notification
// allows to perform all the derived operations.
// Register selection macros include the "case", X bits check and logging
{
   switch(pId) {
      case WDTCSR:
         Log_register_write(WDTCSR, pData, VAR(Mask));
         
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
         
         // Writing WDE=X silently ignored if WDTON programmed
         else if(!VAR(Wdton)) { 
            REG(WDTCSR).set_bit(3, pData[3]);
         }
         
         // Bit 4 - WDCE: Watchdog Change Enable
         // ----------------------------------------
         // Writing WDCE=1 only allowed if also writing WDE=1
         if(pData[4] == 1 && REG(WDTCSR)[4] != 1) {
            if(pData[3] == 1) {
               REG(WDTCSR).set_bit(4, 1);
               REMIND_ME2(4, RMD_AUTOCLEAR_WDCE);
            } else {
               Warn("Must write both WDCE=1 and WDE=1 to set WDCE");          
            }         
         }
         
         // Writing WDCE=0/X is always allowed
         else {
            REG(WDTCSR).set_bit(4, pData[4]);
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
         
         // TODO: Start timer if WDE=1
         
         break;
   }
}   

void On_reset(int pCause)
//***********************
// Initialize registers to the desired value. Note that the watchdog timer
// is not automatically disabled nor is the watchdog counter reset.
{
   // If WDTON fuse is programmed, then WDE and WDIE forced to 1 and 0
   if(VAR(Wdton)) {
      REG(WDTCSR) = 0x08;
      VAR(Mask) = 0xB7;
   } else {
      REG(WDTCSR) = 0;
      VAR(Mask) = 0xFF;
   }

   VAR(Tick_signature) = 0;
   VAR(Wdrf) = false;

   // The initial state of the WDE bit depends on the reset. If the MCU was
   // reset due to a previous watchdog timeout, then the WDE pin is set to 1
   // and becomes read-only until the WDRF flag is cleared in MCUSR
   if(pCause == RESET_WATCHDOG) {
      VAR(Wdrf) = true;
      REG(WDTCSR).set_bit(3, 1);
   }
}

void On_remind_me(double pTime, int pAux)
//**************************************
// Response to REMIND_ME2() used implement the watchdog timeout and the
// autoclearing of the WDCE bit 4 system clock cycles after it was set
{
   switch(pAux) {
      case RMD_AUTOCLEAR_WDCE:
         if(REG(WDTCSR)[4] != 0) {
            Warn("WDCE cleared by hardware; previously set 4 cycles ago");
         }
         REG(WDTCSR).set_bit(4, 0);
         break;
         
      case RMD_TIMEOUT:
         break;
   }
}

void On_notify(int pWhat)
//**********************
// Notification coming from some other DLL instance. Used here to handle
// the WDR instruction and to handle the clearing of the WDRF bit in MCUSR
// which allows clearing of the WDE bit.
{
   switch(pWhat) {
   
      // The WDR instruction was executed; reset watchdog counter
      case NTF_WDR:
         break;

      // The WDRF bit in MCUSR was cleared; WDE is read-write again
      case NTF_WDRF0:
         VAR(Wdrf) = false;
         break;
         
      // The WDRF bit in MCUSR was set; WDE is set and becomes read-only
      case NTF_WDRF1:
         VAR(Wdrf) = true;
         REG(WDTCSR).set_bit(3, 1);
         // TODO: Start timer
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
//************************************************
// Called periodically to refresh static controls showing the status, etc.
// If the watchdog timer is enabled, the "Timeout" field is updated to show
// remaining time based on the current "pTime". The rest of the GUI is only
// updated if the WDTCSR register has changed since the last On_update_tick().
{
}