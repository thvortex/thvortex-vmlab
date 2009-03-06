// =============================================================================
// C++ code template for a VMLAB user defined component
// Target file must be Windows .DLL (no .EXE !!)
//
// Component name: avrlog v1.1
//
// This component implements an 8-bit digital data logger that creates log files
// in the same "NNNNNNNNN:XX" format as the AVR Studio simulator where the Ns
// are the MCU cycle count in decimal and the XX is the logged value in hex.
//
// To use this component, use the following component definition:
//
// X<Name> _avrlog(<ClockFrequency>) <D7> <D6> <D5> <D4> <D3> <D2> <D1> <D0>
//
// The <Name> is used to form part of the output filename in the format
// "<Name>.log". The <ClockFrequency> is specified in Hz (.e.g. "1MEG") and
// should match the actual MCU clock frequency being simulated to ensure correct
// cycle counts in the output log. The <D7-D0> nodes are the digital inputs to
// be logged (D7 is the MSb).
//
// Version History:
// v1.1 12/21/08 - Improved error handling; break simulation on errors
// v1.0 11/16/08 - Initial public release
//
// Written by Wojciech Stryjewski <thvortex@gmail.com>, 2008
//
// This work is hereby released into the Public Domain. To view a copy of the
// public domain dedication, visit http://creativecommons.org/licenses/publicdomain/
// or send a letter to Creative Commons, 171 Second Street, Suite 300,
// San Francisco, California, 94105, USA.
//
// THERE IS NO WARRANTY FOR THE PROGRAM, TO THE EXTENT PERMITTED BY APPLICABLE
// LAW. EXCEPT WHEN OTHERWISE STATED IN WRITING THE COPYRIGHT HOLDERS AND/OR
// OTHER PARTIES PROVIDE THE PROGRAM ?AS IS? WITHOUT WARRANTY OF ANY KIND,
// EITHER EXPRESSED OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE
// ENTIRE RISK AS TO THE QUALITY AND PERFORMANCE OF THE PROGRAM IS WITH YOU.
// SHOULD THE PROGRAM PROVE DEFECTIVE, YOU ASSUME THE COST OF ALL NECESSARY
// SERVICING, REPAIR OR CORRECTION.
// 
// IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW OR AGREED TO IN WRITING WILL
// ANY COPYRIGHT HOLDER, OR ANY OTHER PARTY WHO MODIFIES AND/OR CONVEYS THE
// PROGRAM AS PERMITTED ABOVE, BE LIABLE TO YOU FOR DAMAGES, INCLUDING ANY
// GENERAL, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE
// OR INABILITY TO USE THE PROGRAM (INCLUDING BUT NOT LIMITED TO LOSS OF DATA OR
// DATA BEING RENDERED INACCURATE OR LOSSES SUSTAINED BY YOU OR THIRD PARTIES OR
// A FAILURE OF THE PROGRAM TO OPERATE WITH ANY OTHER PROGRAMS), EVEN IF SUCH
// HOLDER OR OTHER PARTY HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
//
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#pragma hdrstop
#include "C:\VMLAB\bin\blackbox.h"
int WINAPI DllEntryPoint(HINSTANCE, unsigned long, void*) {return 1;} // is DLL
//==============================================================================

// Size of temporary string buffer for generating filenames and error messages
#define MAXBUF 256

// If "pin" is a logic 1 then set "bit" position in "VAR(Log_data)". If the
// "pin" is a logic 0 then do nothing. This macro is used to combine all of the
// pin values into a single data byte to capture the initial state at the start
// of the simulation. Note that a pin value of UNKNOWN is treated the same as a
// logic 0, because the AVR Studio log file format doesn't support unknown bit
// values, but it starts the simulation with most ports initialized to zero.
#define INIT_LOG_DATA(pin, bit) \
   VAR(Log_data) |= (GET_LOGIC(pin) == 1 ? 1 << bit : 0)

//==============================================================================
// Declare pins here
//
DECLARE_PINS
   DIGITAL_IN(D7, 1);
   DIGITAL_IN(D6, 2);
   DIGITAL_IN(D5, 3);
   DIGITAL_IN(D4, 4);
   DIGITAL_IN(D3, 5);
   DIGITAL_IN(D2, 6);
   DIGITAL_IN(D1, 7);
   DIGITAL_IN(D0, 8);
END_PINS

// =============================================================================
// Declare module global variables here.
// The reason for this is to keep a set of variables by instance
// To use a variable, do it through the the macro VAR(...)
//
DECLARE_VAR
   FILE *File;      // Log file open for writing
   BYTE Log_data;	  // Previous pin state still to be written to the log
   double Log_time;	// Time step at which Log_data last changed
   double Clock_period; // MCU clock period; converts elapsed time to cycles
   double Clock_delay;  // Time offset from start of simulation to first inst
END_VAR

// You can delare also globals variable outside DECLARE_VAR / END_VAR, but if
// multiple instances of this cell are placed, all these instances will
// share the same variable.
//

// ============================================================================
// Say here if your component has an associated window or not. Pass as parameter
// the dialog resource ID (from .RC file) or 0 if no window
//
USE_WINDOW(0);   // If window USE_WINDOW(WINDOW_USER_1) (for example)

// =============================================================================
// Helper Functions

void Close_file(void)
//********************
// Close the output file, and check for any I/O errors that can occur when
// closing the file.
{
   char strBuffer[MAXBUF];

   // Do nothing if the wav file is already closed
   if(!VAR(File)) {
      return;
   }

   // Close the wav file so it can be moved or deleted
   if(fclose(VAR(File))) {
      snprintf(strBuffer, MAXBUF, "Error closing \"%s.log\" file: %s",
         GET_INSTANCE(), strerror(errno));
      BREAK(strBuffer);         
   }

   VAR(File) = NULL;
}

void Update_bit(int pBit, EDGE pEdge)
//********************
// Either set or clear the bit position "pBit" in "VAR(Log_data)" if "pEdge" is
// respectivelly RISE or FALL. This function is called from the switch statement
// in On_digital_in_edge() to accumulate all of the bit state changes into a
// single byte that holds the current pin state.
{
   if(pEdge == RISE) {
      VAR(Log_data) |= (1 << pBit);
   } else {
      VAR(Log_data) &= ~(1 << pBit);
   }
}

void Write_log(double pTime)
//********************
// If the elapsed time "pTime" of the current time step is different from the
// previous time step at "VAR(Log_time)" then write a log entry for the previous
// time step. Because On_digital_in_edge() may be called multiple times for the
// same time step, we want to delay writing any log entry until we know the time
// step has been fully simulated.
{
   char strBuffer[MAXBUF];
   double logCycle;

   // Do nothing if log file could not be opened or if the next time step has
   // not been reached yet.
   if(!VAR(File) || pTime == VAR(Log_time)) {
      return;
   }

   // The elapsed time (in seconds) is converted to a cycle count based on the
   // MCU's clock rate (specified as a parameter). The "- VAR(offset)" subtracts
   // out the initial power on delay from the cycle count. Also, to avoid any
   // floating point round off errors, the fprintf() is used to round up the
   // result to the closest integer instead of using an integer cast.
   logCycle = (VAR(Log_time) - VAR(Clock_delay)) / VAR(Clock_period);      
   fprintf(VAR(File), "%09.0lf:%02X\n", logCycle, VAR(Log_data));

   // If any errors occur with writing the file, then break with an error
   // message and close the file to prevent any further logging.
   if(ferror(VAR(File))) {
      snprintf(strBuffer, MAXBUF, "Could not write \"%s.log\" file: %s",
         GET_INSTANCE(), strerror(errno));
      BREAK(strBuffer);
      
      Close_file();
   }

   // Record current time so next log entry isn't written until next time step
   VAR(Log_time) = pTime;
}

// =============================================================================
// Callback functions. These functions are called by VMLAB at the proper time

const char *On_create()
//********************
// Perform component creation. It must return NULL if the creation process is
// OK, or a message describing the error cause. The message will show in the
// Messages window. Typical tasks: check passed parameters, open files,
// allocate memory,...
{
   // The clock rate of the simulated MCU should be passed as the only argument
   // to this component. This clock rate is needed to convert the elapsed time
   // that VMLAB reports (in seconds) into the cycle counts used by the
   // AVR Studio log file format.
   double clockFrequency = GET_PARAM(1);
   if(clockFrequency <= 0) {
      return "Missing/invalid MCU clock frequency parameter (in Hz)"; 
   }

   // Convert clock frequency to clock period
   VAR(Clock_period) = 1.0 / clockFrequency;

   return NULL;
}

void On_window_init(HWND pHandle)
//*******************************
// Window initialization. Fill only if the component has an associated window
// -USE_WINDOW(..) not zero-. The Parameter pHandle brings the main component
// window handle. Typical tasks: fill controls with data, intitialize gadgets,
// hook an own Windows structure (VCL, MFC,...)
{
   // No Action
}

void On_destroy()
//***************
// Destroy component. Free here memory allocated at On_create; close files
// etc.
{
   // No Action
}

void On_simulation_begin()
//************************
// VMLAB informs you that the simulation is starting. Initialize pin values
// here Open files; allocate memory, etc.
// Although the log filename doesn't change with each simulation, it's better to
// open it here and close it in On_simulation_end(). This allows the user to
// delete or move the log file without having to close or rebuild the project.
{
   char strBuffer[MAXBUF];

   // Initialize per instance simulation variables.
   VAR(Clock_delay) = 0;
   VAR(Log_time) = 0;
   VAR(Log_data) = 0;

   // Create or overwrite log file in current directory
   snprintf(strBuffer, MAXBUF, "%s.log", GET_INSTANCE());
   VAR(File) = fopen(strBuffer, "w");

   // We can still run if the file won't open; we just can't log anything
   if(!VAR(File)) {
      snprintf(strBuffer, MAXBUF, "Could not create \"%s.log\" file: %s",
         GET_INSTANCE(), strerror(errno));
      BREAK(strBuffer);
   }
}

void On_simulation_end()
//**********************
// Undo here the operations done at On_simulation_begin: free memory, close
// files, etc.
{
   // Do nothing if the log file could not be opened
   if(!VAR(File)) {
      return;
   }

   // Write the last pending entry to the log
   Write_log(0);

   // Close the log file so it can be moved or deleted
   Close_file();
}

void On_digital_in_edge(PIN pDigitalIn, EDGE pEdge, double pTime)
//**************************************************************
// Response to a digital input pin edge. The EDGE type parameter (pEdge) can
// be RISE or FALL. Use pin identifers as declared in DECLARE_PINS
{
   // If needed, write out log entry for previous time step
   Write_log(pTime);

   // Update the current log entry value based on which pin is changing
   switch(pDigitalIn) {
      case D7: Update_bit(7, pEdge); break;
      case D6: Update_bit(6, pEdge); break;
      case D5: Update_bit(5, pEdge); break;
      case D4: Update_bit(4, pEdge); break;
      case D3: Update_bit(3, pEdge); break;
      case D2: Update_bit(2, pEdge); break;
      case D1: Update_bit(1, pEdge); break;
      case D0: Update_bit(0, pEdge); break;
   }
}

double On_voltage_ask(PIN pAnalogOut, double pTime)
//**************************************************
// Return voltage as a function of Time for analog outputs that must behave
// as a continuous time wave.
// SET_VOLTAGE(), SET_LOGIC()etc. not allowed here. Return KEEP_VOLTAGE if
// no changes. Use pin identifers as declared in DECLARE_PINS
{
   return KEEP_VOLTAGE;
}

void On_time_step(double pTime)
//*****************************
// The analysis at the given time has finished. DO NOT place further actions
// on pins (unless they are delayed). Pins values are stable at this point.
{
   // Record the initial input pin state at time 0. These macros below relie on
   // VAR(Log_data) being previously initialized to 0 in On_simulation_begin().
   if(pTime == 0) {
      INIT_LOG_DATA(D7, 7);
      INIT_LOG_DATA(D6, 6);
      INIT_LOG_DATA(D5, 5);
      INIT_LOG_DATA(D4, 4);
      INIT_LOG_DATA(D3, 3);
      INIT_LOG_DATA(D2, 2);
      INIT_LOG_DATA(D1, 1);
      INIT_LOG_DATA(D0, 0);
   }
   
   // VMLAB simulates a power on delay in the MCU: there is a large gap of time
   // between the initial On_time_step(0) call and the second call that
   // signifies the start of execution at the reset vector (address 0). Since
   // AVR Studio does not simulate this delay, we subtract it out from the
   // logged cycle time. We also update VAR(Log_time) to match so that the very
   // first log file entry begins with cycle number 0.
   if(!VAR(Clock_delay) && pTime) {
      VAR(Clock_delay) = pTime;
      VAR(Log_time) = pTime;
   }
}

void On_remind_me(double pTime, int pData)
//***************************************
// VMLAB notifies about a previouly sent REMIND_ME() function.
{
   // No Action
}

void On_gadget_notify(GADGET pGadgetId, int pCode)
//************************************************
// A window gadget (control) is sending a notification.
{
   // No action
}
