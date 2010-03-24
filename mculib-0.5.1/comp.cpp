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
// NOTE: WORD8::get_field() returns -1 for unknown bits
enum { MODE_UNKNOWN = -1, MODE_TOGGLE, MODE_RESERVED, MODE_FALL, MODE_RISE };
const char *Mode_text[] = {
   "?", "Toggle", "Reserved", "Falling Edge", "Rising Edge",
};

// Labels for positive voltage input based on ACBG bit value 
// NOTE: WORD8::operator[] returns UNKNOWN (2) if ACBG=X
const char *Plus_text[] = { "AIN0", "VREF", "????" };

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
   ACSR,
   DIDR
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
   double Positive;       // Last voltage seen on AIN0 or positive input
   double Negative;       // Last voltage seen on AIN1 or negative input

   bool Log;              // True if the "Log" checkbox button is checked
   bool Dirty;            // True if "Mode" or voltage labels need update
   bool Sleep;            // True if disabled by SLEEP mode deeper than IDLE
END_VAR

bool Started;             // True if simulation started and interface functions work

USE_WINDOW(WINDOW_USER_1); // Window to display registers, etc. See .RC file

REGISTERS_VIEW
//           ID     .RC ID    substitute "*" by bit names: bi7name, ..., b0
   DISPLAY(ACSR, GDT_ACSR, ACD, ACBG, ACO, ACI, ACIE, ACIC, ACIS1, ACIS0)
   DISPLAY(DIDR, GDT_DIDR, *, *, *, *, *, *, AIN1D, AIN0D)
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
   // TODO: Workaround for VMLAB 3.15 which does not automatically disable
   // all interrupts after a MCU reset. Since SET_INTERRUPT_ENABLE() has no
   // effect from On_reset(), it's possible that a previously enabled ACI
   // will remain enabled after reset even though ACIE=0 in ACSR on reset.
   // However, since this is the only place where a flag can be set, we
   // can call SET_INTERRUPT_ENABLE() here to ensure that VMLAB's internal
   // interrupt enable matches the contents of the ACIC bit.
   SET_INTERRUPT_ENABLE(ACI, REG(ACSR)[3] == 1);   
   
   SET_INTERRUPT_FLAG(ACI, FLAG_SET);
   REG(ACSR).set_bit(4, 1);
}

void Disable_digital(PORT pPort, bool state)
//*************************
// If "state" is true then disable digital input on "pPort" pin. If "state" is
// false then re-enable the digital functionality. 
{
   BOOL rc;

   if(state) {
      rc = SET_PORT_ATTRI(pPort, ATTRI_DISABLE_DIGITAL, 0);
   } else {
      rc = SET_PORT_ATTRI(pPort, 0, ATTRI_DISABLE_DIGITAL);
   }
   
   if(!rc) {
      BREAK("Internal error: SET_PORT_ATTRI() returned false");
   }
}

void Measure()
//*************************
// Sample the voltage levels at the positive and negative comparator inputs
// and record them into VAR(Positive) and VAR(Negative). Called from
// On_time_step() to detect a rising/falling edge on the comparator output
// and from On_reset() to assign an initial value to the ACO bit in ACSR.
{
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
// Called at the beginning of simulation. All initialization happens inside
// On_reset() which is always called immediately after On_simulation_begin().
{   
}

void On_simulation_end()
//**********************
// If the simulation is ending, set all registers and text fieds to unknown.
{
   // Set reigster to unknown and ensure that mode/labels update accordingly
   REG(ACSR) = WORD8(0, 0);
   REG(DIDR) = WORD8(0, 0);
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
      {
         Log_register_write(ACSR, pData, 0xff); // All bits valid

         // Bits 0,1 - ACISx: Analog Comparator Interrupt Mode Select
         // ----------------------------------------
         int newMode = pData.get_field(1, 0);

         // ACIS=01 is a reserved mode
         if(newMode == MODE_RESERVED) {
            WARNING("Reserved ACIS value written to ACSR",
               CAT_COMP, WARN_PARAM_RESERVED);
         }
         
         // Log changes to the ACIS field
         if(newMode != REG(ACSR).get_field(1, 0)) {
            Log("Updating mode: %s", Mode_text[newMode + 1]);
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
         pData.set_bit(5, REG(ACSR)[5]);
 
         // Bit 6 - ACBG: Analog Comparator Bandgap Select
         // ----------------------------------------
         // If bandgap reference was updated, then Log() and update GUI label
         if(pData[6] != REG(ACSR)[6]) {
            Log("Changing positive input: %s", Plus_text[pData[6]]);
         }

         // Bit 7 - ACD: Analog Comparator Disable
         // ----------------------------------------
         // The datasheet warns about writing ACIE=0 when changing ACD in
         // case an interrupt occurs. It's possible to write an application
         // that handles this interrupt correctly, so VMLAB will not issue a
         // warning here. Since VMLAB will simulate this interrupt, the user
         // should notice if their AVR code doesn't handle this situation
         // correctly.
         if(pData[7] != REG(ACSR)[7]) {
            
            // If not already disabled by SLEEP, then Log() enable/disable event
            if(!VAR(Sleep)) {
               Log("%s by ACD", pData[7] == 1 ? "Disabled" : "Enabled");
            }
         }
         
         // Bit 2 - ACIC: Analog Comparator Input Capture Enable
         // ----------------------------------------
         // Writing ACIC=0/X will disable comparator input capture. Sending
         // the current ACO bit when enabling allows simulating of the
         // spurious input capture interrupt that can occur on real hardware.
         //
         // NOTE: The NOTIFY() must be the last interface function called due to
         // a bug IN VMLAB 3.15. This bug is fixed in 3.15E and later but for now
         // this allows me to release the component before a new VMLAB 3.16 is
         // ready.
         if(pData[2] == 1 && REG(ACSR)[2] != 1) {
            Log("Updating input capture: enabled");
            NOTIFY("TIMER1", REG(ACSR)[5] ? NTF_ACIC_1 : NTF_ACIC_0);
         } else if(pData[2] != 1 && REG(ACSR)[2] == 1) {
            Log("Updating input capture: disabled");
            NOTIFY("TIMER1", NTF_ACIC_OFF);
         }
                  
         // All bits r/w except for ACO (read only) and ACI (clear only)
         REG(ACSR) = pData;
         VAR(Dirty) = true;
         break;
      }
      
      case DIDR:
      {
         Log_register_write(DIDR, pData, 0x03); // Only bits 0-1 valid
         
         Disable_digital(AIN0, pData[0] == 1);
         Disable_digital(AIN1, pData[1] == 1);
         
         REG(DIDR) = pData & 0x03;
         break;
      }
   }
}   

void On_time_step(double pTime)
//*******************************
// Called on every simulation time step. Read the voltages present on the
// input pins, update the ACO bit in the ACSR as necessary, generate an
// interrupt if ACIE=1, and sent input capture notifications to TIMER1
// if ACIC=1
{
   // If disabled due to SLEEP, then don't update ACO or interrupt
   if(VAR(Sleep)) {
      return;
   }

   // Update ACO bit with comparator output. If comparator is currently
   // disabled with ACD=1, then force ACO=0 which could possibly generate
   // an interrupt. This ACO behavior with ACD=1 was verified on real
   // ATmega48 hardware. For performance reasons, Measure() is only called
   // to sample new voltages if the comparator is not disabled.
   LOGIC oldOutput = REG(ACSR)[5];
   LOGIC newOutput = 0;
   if(REG(ACSR)[7] != 1) {
      Measure();
      newOutput = VAR(Positive) > VAR(Negative);
   }
   REG(ACSR).set_bit(5, newOutput);
      
   // Check if the logic output of comparator has changed on this time step.
   // Generate interrupt if ACIE=1. If ACIE=0 then no interrupt is generated
   // but the ACI interrupt flag will still be set and the interrupt will
   // remain pending. The ACIS mode bits determine the edge condition that
   // causes an interrupt to occur.
   if(newOutput != oldOutput) {
      switch(REG(ACSR).get_field(1, 0)) {
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
}

void On_reset(int pCause)
//***********************
// Initialize registers to the desired value.
{
   VAR(Sleep) = false;
   VAR(Dirty) = true;
   
   // The ACSR register (including the ACO bit) is initialized to zero on
   // reset. If the comparator output is 1, then the ACO bit will be set
   // inside the next On_time_step() and an interrupt will be generated
   // immediately after the reset. Verified on real ATmega48 hardware.
   REG(ACSR) = 0;

   // Because DIDR is initialized to 0 on reset, make sure that all pins
   // have their digital functionality enabled.
   REG(DIDR) = 0;
   Disable_digital(AIN0, false);
   Disable_digital(AIN1, false);
   
   // In case this is the first On_reset(), ensure that On_update_tick() will
   // begin displaying the measured voltage in the GUI window.
   Measure();
   Started = true;
}

void On_notify(int pWhat)
//***********************
// Notification coming from some other DLL instance. The ADC will send
// notifications when the ADC multiplexer is used for selecting the
// negative input of the comparator
{
   // TODO: Finish this once ADC is written
}

void On_sleep(int pMode)
//*********************
// The micro has entered in SLEEP mode. Any SLEEP mode deeper than IDLE will
// disable the analog comparator. If not already disabled due to ACD=1 in ACSR
// then also Log() a message about entering/exisiting SLEEP and make sure the
// "Mode" display in the GUI gets updated.
{
   bool oldSleep = VAR(Sleep);
   VAR(Sleep) = pMode > SLEEP_IDLE;

   if(REG(ACSR)[7] != 1) {

      if(VAR(Sleep) && !oldSleep) {
         Log("Disabled by SLEEP");
      } else if(!VAR(Sleep) && oldSleep) {
         Log("Exit from SLEEP");
      }
   
      VAR(Dirty) = true;
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
// Only the mode display, the voltage field labels, and the voltage fields
// themselves, but only if any changes occurred since the last update.
{
   // If the simulation has started, then re-measure the voltages in case
   // the sources changed (for example, if updating ACSR through the GUI
   // while the simulation is paused) or in case the comparator is disabled
   // (if disabled, On_time_step() will not call Measure() for better
   // performance), and update the voltage display.
   if(Started) {
      Measure();
      SetWindowTextf(GET_HANDLE(GDT_VPLUS), "%.3f V", VAR(Positive));
      SetWindowTextf(GET_HANDLE(GDT_VMINUS), "%.3f V", VAR(Negative));
   }
   
   // If simulation not running, then always display unknown voltage level
   else {
      SetWindowText(GET_HANDLE(GDT_VPLUS), "? V");
      SetWindowText(GET_HANDLE(GDT_VMINUS), "? V");
   }
   
   // If ACSR register or sleep state changed, then update mode displays. If
   // disabled through SLEEP or ACD=1 then always display current mode as
   // "Disabled". If not disabled, then display the decoded mode from ACIS
   // bits, and if ACIC=1 then append " / Input Capture" to the mode string.
   // Also update label on the positive voltage display in case ACBG changed.
   if(VAR(Dirty)) {
      SetWindowTextf(GET_HANDLE(GDT_LPLUS), "%s", Plus_text[REG(ACSR)[6]]);
      
      if(VAR(Sleep) || REG(ACSR)[7] == 1) {
         SetWindowText(GET_HANDLE(GDT_MODE), "Disabled");
      } else {
         int mode = REG(ACSR).get_field(1, 0);
         SetWindowTextf(GET_HANDLE(GDT_MODE), "%s%s", Mode_text[mode + 1],
            REG(ACSR)[2] == 1 ? " / Input Capture" : "");
      }
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