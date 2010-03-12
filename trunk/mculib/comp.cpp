// =============================================================================
// Component Name: AVR analog comparator peripheral
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

// Reference voltage applied to AIN0 pin if ACBG=1 in ACSR
#define VREF_VOLTAGE 1.1

// Comparator mode as a combination of ACISx bits overriden by ACD.
// NOTE: get_field() returns -1 for unknown bits
enum { 
   MODE_UNKNOWN = -1, MODE_TOGGLE, MODE_RESERVED, MODE_FALL,
   MODE_RISE, MODE_DISABLED, 
};
const char *Mode_text[] = {
   "?", "Toggle", "Reserved", "Falling Edge", "Rising Edge", "Disabled"
};

// Labels for positive voltage input based on ACBG bit value 
// NOTE: operator[] returns UNKNOWN (=2) if ACBG=X
const char *Plus_text[] = { "AIN0", "VREF", "???" };

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
   ACI
END_INTERRUPTS 

// =============================================================================
// Declare variables here to allow multiple instances to be placed
//
DECLARE_VAR
   int Mode;              // Current mode (ACD & ACISx bits)

   double Positive;       // Last voltage seen on AIN0 or positive input
   double Negative;       // Last voltage seen on AIN1 or negative input

   bool Log;              // True if the "Log" checkbox button is checked
   bool Dirty;            // True if "Mode" or voltage labels need update
END_VAR

bool Started;             // True if simulation started and interface functions work

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

void Interrupt()
//*************************
// Generate ACI interrupt and set ACI flag in ACSR
{
   SET_INTERRUPT_FLAG(ACI, FLAG_SET);
   REG(ACSR).set_bit(4, 1);
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
   // Set a sane default value until first On_time_step() call. The Started
   // variable is not set until the first On_time_step() so that the voltage
   // display does not update until the first
   //VAR(Positive) = 0;
   //VAR(Negative) = 0;
}

void On_simulation_end()
//**********************
// If the simulation is ending, set all registers and text fieds to unknown.
{
   // Set reigster to unknown and ensure that mode/labels update accordingly
   REG(ACSR) = WORD8(0, 0);
   VAR(Mode) = MODE_UNKNOWN;   
   VAR(Dirty) = true;
   
   // Force On_update_tick() to display "? V" for the voltage values
   Started = false;
}

void On_register_write(REGISTER_ID pId, WORD8 pData)
//**********************
// The micro is writing pData into the pId register This notification
// allows to perform all the derived operations.
// Register selection macros include the "case", X bits check and logging
{
   switch(pId) {
      case ACSR:
         Log_register_write(ACSR, pData, 0xff); // All bits valid

         // Bits 0,1 - ACISx: Analog Comparator Interrupt Mode Select
         // ----------------------------------------
         int newMode = pData.get_field(1, 0);
         if(newMode == MODE_RESERVED) {
            WARNING("Reserved ACIS value written to ACSR",
               CAT_COMP, WARN_PARAM_RESERVED);
         }
                  
         // Bit 3 - ACIE: Analog Comparator Interrupt Enable
         // ----------------------------------------
         // Writing ACIE=0/X will disable the interrupt
         SET_INTERRUPT_ENABLE(ACI, pData[3] == 1);
                  
         // Bit 4 - ACI: Analog Comparator Interrupt Flag
         // ----------------------------------------
         // Writing ACI=1 clears interrupt flag; writing ACI=0 or ACI=X has
         // no effect and ACI bit retains current value (by copying it from
         // ACSR to pData).
         if(pData[4] == 1) {
            SET_INTERRUPT_FLAG(ACI, FLAG_CLEAR);
            pData.set_bit(4, 0);
         } else {
            pData.set_bit(4, REG(ACSR)[4]);
         }
         
         // Bit 5 - ACO: Analog Comparator Output
         // ----------------------------------------
         // Copy read only ACO bit from ACSR to pData to preserve value
         // TODO: What happens to ACO when ACD=1?
         pData.set_bit(5, REG(ACSR)[5]);
 
         // Bit 6 - ACBG: Analog Comparator Bandgap Select
         // ----------------------------------------
         // If bandgap reference was updated, then Log() and update GUI label
         if(pData[6] != REG(ACSR)[6]) {
            Log("Changing positive input: %s", Plus_text[pData[6]]);
         }

         // Bit 7 - ACD: Analog Comparator Disable
         // ----------------------------------------
         // Should write ACIE=0 to avoid spurious interrupts when changing ACD
         // ACD=1 or ACD=X also overrides operating mode set by ACIS bits
         if(pData[7] != REG(ACSR)[7] && pData[3] != 0) {
            WARNING("Should write ACIE=0 when changing ACD (avoids spurious interrupt)",
               CAT_COMP, WARN_MISC);
         }
         
         // The ACD bit overrides the ACIS mode selection
         // If the comparator is disabled, then set ACO=X
         if(pData[7] == 1) {
            newMode = MODE_DISABLED;
            pData.set_bit(5, UNKNOWN);
         }
         // TODO: Use Is_disabled(); separate Log() message
                  
         // If operating mode changed then log it and redraw the GUI
         if(newMode != VAR(Mode)) {
            Log("Updating mode: %s", Mode_text[newMode + 1]);
            VAR(Mode) = newMode;
         }

         // Bit 2 - ACIC: Analog Comparator Input Capture Enable
         // ----------------------------------------
         // Writing ACIC=0/X will disable comparator input capture. Sending
         // the current comparator state when enabling allows simulating
         // of the spurious interrupt that can occur on real hardware
         //
         // NOTE: The NOTIFY() must be the last interface function called due to
         // a bug IN VMLAB 3.15. This bug is fixed in 3.15E and later but for now
         // this allows me to release the component before a new VMLAB 3.16 is
         // ready.
         if(pData[2] == 1 && REG(ACSR)[2] != 1) {
            bool state = VAR(Positive) > VAR(Negative);
            Log("Analog comparator input capture: enabled");
            NOTIFY("TIMER1", state ? NTF_ACIC_1 : NTF_ACIC_0);
         } else if(pData[2] != 1 && REG(ACSR)[2] == 1) {
            Log("Analog comparator input capture: disabled");
            NOTIFY("TIMER1", NTF_ACIC_OFF);
         }
                  
         // All bits r/w except for ACO (read only) and ACI (only clear)
         REG(ACSR) = pData;
         VAR(Dirty) = true;
         break;
   }
}   

// TODO: Need SLEEP mode handling

void On_time_step(double pTime)
//*******************************
// Called on every simulation time step. Read the voltages present on the
// input pins, update the ACO bit in the ACSR as necessary, generate an
// interrupt if ACIE=1, and sent input capture notifications to TIMER1
// if ACIC=1
{
   bool oldOutput = VAR(Positive) > VAR(Negative);

   // Measure the positive input voltage from either AIN0 or VREF as
   // determined by the ACBG pin in ACSR. If ACBG is UNKNOWN then
   // the measured voltage doesn't change.
   if(REG(ACSR)[6] == 0) {
      VAR(Positive) = GET_VOLTAGE(AIN0);
   } else if(REG(ACSR)[6] == 1) {
      VAR(Positive) = VREF_VOLTAGE;
   }
   
   // Measure the negative input voltage from either AIN1 or from one
   // of the ADCx pins based on the ACME/ADEN bits in ADCSRB/ADCSRBA
   // and the ADMUX register. ADC will NOTIFY() to inform comparator
   // of changes to the negative input source.
   // TODO: Finish this once ADC component is written
   VAR(Negative) = GET_VOLTAGE(AIN1);

   // If comparator is disabled then don't update ACO bit or interrupt
   if(VAR(Mode) == MODE_DISABLED) {
      return;
   }

   // Always update ACO bit with comparator output. If comparator was
   // previously disabled and ACO=X, it will not get updated to the
   // current output of the comparator.
   bool newOutput = VAR(Positive) > VAR(Negative);
   REG(ACSR).set_bit(5, newOutput);
      
   // Check if the logic output of comparator has changed on this time step.
   // Generate interrupt if ACIE=1. If ACIE=0 then no interrupt is generated
   // but the ACI interrupt flag will still be set and the interrupt will
   // remain pending.
   if(newOutput != oldOutput) {
      switch(VAR(Mode)) {
         case MODE_RISE:
            if(newOutput) {
               Interrupt();
            }
            break;
            
         case MODE_FALL:
            if(!newOutput) {
               Interrupt();
            }
            break;
            
         case MODE_TOGGLE:
            Interrupt();
            break;
            
         default: // MODE_RESERVED or MODE_UNKNOWN
            break;
      }

      // Notify TIMER1 of output change if ACIC=1.
      // NOTE: The NOTIFY() must be the last interface function called due to
      // a bug IN VMLAB 3.15. This bug is fixed in 3.15E and later but for now
      // this allows me to release the component before a new VMLAB 3.16 is
      // ready.
      if(REG(ACSR)[2] == 1) {
         NOTIFY("TIMER1", newOutput ? NTF_ACIC_1 : NTF_ACIC_0);
      }   
   }
   
   // If this is the first On_time_step(), ensure that On_update_tick() will
   // begin displaying the measured voltage in the GUI window.
   Started = true;
}

void On_reset(int pCause)
//***********************
// Initialize registers to the desired value.
{
   REG(ACSR) = 0;
   VAR(Mode) = 0;   
   VAR(Dirty) = true;
   
   // Call On_time_step() to measure the voltages again and schedule an
   // interrupt if they changed. It's possible that a reset will immediately
   // trigger an interrupt because the voltage sources for the positive
   // and negative inputs are changed back to AIN0 and AIN1 on reset.
   On_time_step(0); // pTime not used
   
   // TODO: It seems that the interrupt flags set with SET_INTERRUPT_FLAG()
   // are automatically cleared on reset, but the mask bits which are set
   // with SET_INTERRUPT_ENABLE() are NOT cleared on reset.
   // TODO: It also seems that calling SET_INTERRUPT_ENABLE(ACI, xxx)
   // or SET_INTERRUPT_FLAG(ACI, xxx) from On_reset() has no effect on the
   // simulator.
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
// Only the mode display, the voltage field labels, and the voltage fields
// themselves, but only if any changes occurred since the last update.
{
   // If the simulation has started, then re-measure the voltages in case
   // the sources changed (for example, if updating ACSR through the GUI
   // while the simulation is paused) and update the voltage display.
   if(Started) {
      On_time_step(0);
      SetWindowTextf(GET_HANDLE(GDT_VPLUS), "%.3f V", VAR(Positive));
      SetWindowTextf(GET_HANDLE(GDT_VMINUS), "%.3f V", VAR(Negative));
   }
   
   // If simulation not running, then always display unknown voltage level
   else {
      SetWindowText(GET_HANDLE(GDT_VPLUS), "? V");
      SetWindowText(GET_HANDLE(GDT_VMINUS), "? V");
   }
   
   // If ACSR register or sleep state changed, then update mode displays. If
   // ACIC=1 and current mode is not disabled then append " / Input Capture"
   // to the Mode display.
   if(VAR(Dirty)) {
      SetWindowTextf(GET_HANDLE(GDT_LPLUS), "V(%s)", Plus_text[REG(ACSR)[6]]);
      SetWindowTextf(GET_HANDLE(GDT_MODE), "%s%s", Mode_text[VAR(Mode) + 1],
         REG(ACSR)[2] == 1 && VAR(Mode) != MODE_DISABLED ? " / Input Capture" : "");
   }
}

void On_interrupt_start(INTERRUPT_ID pId)
//***********************************
// Called when MCU begins to execute the ACI interrupt. Clear ACI flag.
{
   switch(pId) {
      case ACI:
         REG(ACSR).set_bit(4, 0);
         break;
   }
}