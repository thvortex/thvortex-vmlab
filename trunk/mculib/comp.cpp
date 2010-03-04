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
#include "comp.h"

int WINAPI DllEntryPoint(HINSTANCE, unsigned long, void*) {return 1;} // is DLL
// =============================================================================

// Maximum value allowed in WDP field
#define MAX_PRESCALER_INDEX 9

// Period of one watchdog timer tick of 128kHz clock
#define WDOG_PERIOD (1.0 / 128000)

// Watchdog mode as a combination of ACISx bits
const char *Mode_text[] = {
   "?", "Disabled", "Rising Edge", "Falling Edge", "Toggle"
};

// Involved ports. Keep same order as in .INI file "Port_map = ..." who
// does the actual assignment to micro ports PD0, etc. This allows multiple instances
// to be mapped into different port sets
//
DECLARE_PINS
   MICRO_PORT(AIN0, 1)
   MICRO_PORT(AIN1, 2)
END_PINS

// Involved registers. Use same order as in .INI file "Register_map"
// These are IDs or indexes. The real data is stored in a hidden WORD8
// type array. (see "blackbox.h" for details). The WORD8 data is referred
// with the REG(xxx) macro.
//
DECLARE_REGISTERS
   ACSR
END_REGISTERS

// Involved interrupts. Use same order as in .INI file "Interrupt_map"
// Like in  registers, these are IDs; what is important is the order.
// It is "Interupt_map" who assigns the index into the actual interrupt name, 
// therefore, different instances can use a different interrupt set
//
DECLARE_INTERRUPTS
   ADCC
END_INTERRUPTS 

// Wrapper macro to call WARNING() since watchdog has only one category
#define Warn(text) WARNING(text, CAT_WATCHDOG, WARN_WATCHDOG_SUSPICIOUS_USE)

// =============================================================================
// Declare variables here to allow multiple instances to be placed
//
DECLARE_VAR
   double Positive;       // Last voltage seen on AIN0 or positive input
   double Negative;       // Last voltage seen on AIN1 or negative input

   bool Log;              // True if the "Log" checkbox button is checked
   bool Dirty;            // True if "Mode" or voltage labels need update
   bool Dirty_voltage;    // True if voltage values need update
END_VAR

USE_WINDOW(WINDOW_USER_1); // Window to display registers, etc. See .RC file

REGISTERS_VIEW
//           ID     .RC ID    substitute "*" by bit names: bi7name, ..., b0
   DISPLAY(ACSR, GDT_ACSR, ACD, ACBG, ACO, ACI, ACIE, ACIC, ACIS1, ACIS0)
END_VIEW

// =============================================================================
// Helper Functions

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
//**********************
// Called at the beginning of simulation. The rest of initialization
// happens inside On_reset() since that will always get called immediately
// after On_simulation_begin().
{
}

void On_simulation_end()
//**********************
// If the simulation is ending, set all registers and text fieds to unknown.
{
}

void On_register_write(REGISTER_ID pId, WORD8 pData)
//**********************
// The micro is writing pData into the pId register This notification
// allows to perform all the derived operations.
// Register selection macros include the "case", X bits check and logging
{
   switch(pId) {
      case ACSR:
         Log_register_write(ACSR, pData, 0xdf); // All r/w except ACO (bit 5)

         // Bit 3 - ACIE: Analog Comparator Interrupt Enable
         // ----------------------------------------
         // TODO: Implement this with some NOTIFY() code
                  
         // Bit 4 - ACI: Analog Comparator Interrupt Flag
         // ----------------------------------------
         // Writing ACI=1 clears interrupt flag; writing ACI=0 has no effect
         if(pData[4] == 1) {
            SET_INTERRUPT_FLAG(ADCC, FLAG_CLEAR);
            REG(ACSR).set_bit(4, 0);
         }
         
         // Bit 5 - ACO: Analog Comparator Output
         // Bit 6 - ACBG: Analog Comparator Bandgap Select
         // ----------------------------------------
         // Nothing special to do here

         // Bit 7 - ACD: Analog Comparator Disable
         // ----------------------------------------
         // Should write ACIE=0 to avoid spurious interrupts when changing ACD
         if(pData[7] != REG(ACSR)[7] && pData[3] != 0) {
            WARNING("Should write ACIE=0 when changing ACD (avoid spurious interrupt)",
               CAT_COMP, WARN_MISC);
         }

         // TODO: Log mode changes
         // TODO: Log bandgap changes
         
         REG(ACSR) = pData;
         break;
   }
}   

void On_reset(int pCause)
//***********************
// Initialize registers to the desired value.
{   
}

void On_remind_me(double pTime, int pAux)
//***************************************
// Response to REMIND_ME()
{
}

void On_notify(int pWhat)
//***********************
// Notification coming from some other DLL instance. 
{
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
}

void On_interrupt_start(INTERRUPT_ID pId)
//***********************************
// Called when MCU begins to execute the ADCC interrupt.
{
   switch(pId) {
      case ADCC:
         
         break;
   }
}