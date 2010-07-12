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
#include "adc.h"

int WINAPI DllEntryPoint(HINSTANCE, unsigned long, void*) {return 1;} // is DLL
// =============================================================================

// Reference voltage applied to VREF pin if REFSx=0b11 in ADMUX
#define VREF_VOLTAGE 1.1

// Constant added to MUXx bit field to obtain ADC0 pin number
#define PIN_ADC0 2

// Converter input based on MUXx bits in ADMUX
// NOTE: WORD8::get_field() returns -1 for unknown bits
enum { IN_UNKNOWN = -1, IN_MIN = 0, IN_MAX = 7, IN_VREF = 14, IN_GND = 15 };
const char *Input_text[] = {
   "?", "ADC0", "ADC1", "ADC2", "ADC3", "ADC4", "ADC5", "ADC6", "ADC7",
   "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
   "VREF", "GND"
};

// Converter voltage reference based on REFSx value in ADMUX
// NOTE: WORD8::get_field() returns -1 for unknown bits
enum { REF_UNKNOWN = -1, MODE_TOGGLE, MODE_RESERVED, MODE_FALL, MODE_RISE };
const char *Ref_text[] = { "?", "AREF", "AVCC", "Reserved", "VREF" };

// Involved ports. Keep same order as in .INI file "Port_map = ..." who
// does the actual assignment to micro ports PD0, etc. This allows multiple instances
// to be mapped into different port sets
//
DECLARE_PINS
   MICRO_PORT(AREF, 1)
   MICRO_PORT(ADC0, 2)
   MICRO_PORT(ADC1, 3)
   MICRO_PORT(ADC2, 4)
   MICRO_PORT(ADC3, 5)
   MICRO_PORT(ADC4, 6)
   MICRO_PORT(ADC5, 7)
   MICRO_PORT(ADC6, 8)
   MICRO_PORT(ADC7, 9)
END_PINS

// Involved registers. Use same order as in .INI file "Register_map"
// These are IDs or indexes. The real data is stored in a hidden WORD8
// type array. (see "blackbox.h" for details). The WORD8 data is referred
// with the REG(xxx) macro.
//
DECLARE_REGISTERS
   ADCL,
   ADCH,
   ADCSRA,
   ADCSRB,
   ADMUX,
   DIDR
END_REGISTERS

// Involved interrupts. Use same order as in .INI file "Interrupt_map"
// Like in  registers, these are IDs; what is important is the order.
// It is "Interupt_map" who assigns the index into the actual interrupt name, 
// therefore, different instances can use a different interrupt set
//
DECLARE_INTERRUPTS
   ADC
END_INTERRUPTS 

// =============================================================================
// Declare variables here to allow multiple instances to be placed
//
DECLARE_VAR
   double Input;          // Last voltage seen on converter input
   double Reference;      // Last voltage seen on converter reference

   bool Log;              // True if the "Log" checkbox button is checked
   bool Dirty;            // True if clock/status or voltage labels need update
   bool Sleep;            // True if disabled by SLEEP mode deeper than ADC Noise
END_VAR

bool Started;             // True if simulation started and interface functions work

USE_WINDOW(WINDOW_USER_1); // Window to display registers, etc. See .RC file

REGISTERS_VIEW
//           ID     .RC ID    substitute "*" by bit names: bi7name, ..., b0
   DISPLAY(ADCL, GDT_ADCL, *, *, *, *, *, *, *, *)
   DISPLAY(ADCH, GDT_ADCH, *, *, *, *, *, *, *, *)
   DISPLAY(ADCSRA, GDT_ADCSRA, ADEN, ADSC, ADATE, ADIF, ADIE, ADPS2, ADPS1, ADPS0)
   DISPLAY(ADCSRB, GDT_ADCSRB, *, ACME, *, *, *, ADTS2, ADTS1, ADTS0)
   DISPLAY(ADMUX, GDT_ADMUX, REFS1, REFS0, ADLAR, *, MUX3, MUX2, MUX1, MUX0)
   DISPLAY(DIDR, GDT_DIDR, *, *, ADC5D, ADC4D, ADC3D, ADC2D, ADC1D, ADC0D)
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
{
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
{
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
   FOREACH_REGISTER(j){
      REG(j) = WORD8(0,0);      // All bits unknown (X)
   }
   VAR(Dirty) = true;
   
   // Force On_update_tick() to display "? V" for the voltage values
   Started = false;
}

void On_register_write(REGISTER_ID pId, WORD8 pData)
//**********************
// The micro is writing pData into the pId register This notification
// allows to perform all the derived operations.
{
   switch(pId) {
   }
}   

void On_time_step(double pTime)
//*******************************
{
   // If disabled due to SLEEP, then don't update ACO or interrupt
   if(VAR(Sleep)) {
      return;
   }
}

void On_reset(int pCause)
//***********************
// Initialize registers to the desired value.
{
}

void On_notify(int pWhat)
//***********************
// Notification coming from some other DLL instance. The ADC will send
// notifications when the ADC multiplexer is used for selecting the
// negative input of the comparator
{
}

void On_sleep(int pMode)
//*********************
// The micro has entered in SLEEP mode. Any SLEEP mode deeper than IDLE will
// disable the analog comparator. If not already disabled due to ACD=1 in ACSR
// then also Log() a message about entering/exisiting SLEEP and make sure the
// "Mode" display in the GUI gets updated.
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
   // while the simulation is paused) or in case the comparator is disabled
   // (if disabled, On_time_step() will not call Measure() for better
   // performance), and update the voltage display.
   if(Started) {
      Measure();
   }
}

void On_interrupt_start(INTERRUPT_ID pId)
//***********************************
// Called when MCU begins to execute the ACI interrupt. Clear ACI flag.
{
   switch(pId) {
   }
}