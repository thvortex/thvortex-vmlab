// =============================================================================
// C++ code template for a VMLAB user defined component
// Target file must be Windows .DLL (no .EXE !!)
//
// Component name: avrstim v1.1
//
// This component implements an 8-bit digital data output that uses stimulus
// files in the same "NNNNNNNNN:XX" format as the AVR Studio simulator where the
// Ns are the MCU cycle count in decimal and the XX is the stimulus value in
// hex.
//
// To use this component, use the following component definition:
//
// X<Name> _avrstim(<ClockFrequency>) <D7> <D6> <D5> <D4> <D3> <D2> <D1> <D0>
//
// The <Name> is used to form part of the input filename in the format
// "<Name>.sti". The <ClockFrequency> is specified in Hz (.e.g. "1MEG") and
// should match the actual MCU clock frequency being simulated to ensure correct
// timing of the generated output. The <D7-D0> nodes are the digital outputs
// driven by the stimulus file (D7 is the MSb).
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

//==============================================================================
// Declare pins here
//
DECLARE_PINS
   DIGITAL_OUT(D7, 1);
   DIGITAL_OUT(D6, 2);
   DIGITAL_OUT(D5, 3);
   DIGITAL_OUT(D4, 4);
   DIGITAL_OUT(D3, 5);
   DIGITAL_OUT(D2, 6);
   DIGITAL_OUT(D1, 7);
   DIGITAL_OUT(D0, 8);
END_PINS

// =============================================================================
// Declare module global variables here.
// The reason for this is to keep a set of variables by instance
// To use a variable, do it through the the macro VAR(...)
//
DECLARE_VAR
   FILE *File;          // Stimulus file open for reading
   double Clock_period; // MCU clock period; convert cycle counts to time delays
   double Clock_delay;  // Time offset from simulation start to first inst
   int Line_count;      // Current line in stimulus file; for error reporting
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

// ============================================================================
// Helper functions

void Close_file(void)
//********************
// Close the input file, and check for any I/O errors that can occur when
// closing the file.
{
   char strBuffer[MAXBUF];

   // Do nothing if the wav file is already closed
   if(!VAR(File)) {
      return;
   }

   // Close the wav file so it can be moved or deleted
   if(fclose(VAR(File))) {
      snprintf(strBuffer, MAXBUF, "Error closing \"%s.sti\" file: %s",
         GET_INSTANCE(), strerror(errno));
      BREAK(strBuffer);         
   }

   VAR(File) = NULL;
}

BOOL Read_stimulus(int *pCycle, int *pData)
//********************
// Read the next line of input from the stimulus file, filling in "pCycle" and
// "pData". Return TRUE if the data was read successfully or FALSE if the file
// is already closed or the data is malformed.
{
   char strBuffer[MAXBUF];

   // Do nothing if the stimulus file could not be opened
   if(!VAR(File)) {
      return FALSE;
   }

   // Increment line/entry counter used to produce helpful error messages
   VAR(Line_count)++;

   // Read the next "cycle:value" pair from the stimulus file. 
   if(fscanf(VAR(File), " %d:%X", pCycle, pData) != 2) {
      // If a system level read error occurred, then break with error message
      if(ferror(VAR(File))) {
         snprintf(strBuffer, MAXBUF, "Could not read \"%s.sti\" file: %s",
            GET_INSTANCE(), strerror(errno));
         BREAK(strBuffer);         
      }

      // If a malformed entry is read, then break with an error message.
      else if(!feof(VAR(File))) {
         snprintf(strBuffer, MAXBUF, "Malformed entry on line %d in \"%s.sti\""
            " file", VAR(Line_count), GET_INSTANCE());
         BREAK(strBuffer);         
      }

      // Stop further processing of the stimulus file by closing it if either a
      // malformed entry was read, a system level read error occurred, or an
      // end-of-file was reached.
      Close_file();
      return FALSE;
   }

   return TRUE;
}

void Schedule_output(double pTime, int pCycle, int pData)
//********************
// Given the current elapsed time in the simulation as well as the next cycle
// count and data value from the stimulus file (as returned by Read_stimulus()),
// schedule the next update of the output pins using REMIND_ME().
{
   char strBuffer[MAXBUF];
   double outputDelay;

   // Convert the stimulus file cycle count to elapsed time (in seconds) based
   // on the MCU's clock rate (specified as a parameter). Break the simulation
   // with an error if the delay (i.e. future update time - current time) is
   // too small. This would indicate a duplicate or decreasing cycle count. To
   // avoid problems with floating point round off, the comparison is done
   // against half the clock period.
   outputDelay = pCycle * VAR(Clock_period) + VAR(Clock_delay) - pTime;
   if (outputDelay <= VAR(Clock_period) * 0.5) {
      snprintf(strBuffer, MAXBUF, "Invalid cycle number %09d on line %d "
         "in \"%s.sti\" file", pCycle, VAR(Line_count), GET_INSTANCE());
      BREAK(strBuffer);

      // Close the stimulus file to prevent further processing
      Close_file();
      return;
   }

   // Schedule an output pin update using the new stimulus values
   REMIND_ME(outputDelay, pData);
}

void Set_output(int pData)
//********************
// Set the state of the D7-D0 output pins to match the byte value in "pData".
// Pin D0 is the LSb.
{
   SET_LOGIC(D7, (pData & 0x80) == 0x80);
   SET_LOGIC(D6, (pData & 0x40) == 0x40);
   SET_LOGIC(D5, (pData & 0x20) == 0x20);
   SET_LOGIC(D4, (pData & 0x10) == 0x10);
   SET_LOGIC(D3, (pData & 0x08) == 0x08);
   SET_LOGIC(D2, (pData & 0x04) == 0x04);
   SET_LOGIC(D1, (pData & 0x02) == 0x02);
   SET_LOGIC(D0, (pData & 0x01) == 0x01);
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
   // to this component. This clock rate is needed to convert the cycle counts
   // used by the AVR Studio stimulus file format into time delays (in seconds)
   // for use in the REMIND_ME() call.
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
// Although the stimulus filename doesn't change with each simulation, it's
// better to open it here and close it in On_simulation_end(). This allows the
// user to delete or move the stimulus file without having to close or rebuild
// the project.
{
   char strBuffer[MAXBUF];
   int firstCycle, firstData;

   // Initialize per instance simulation variables.
   VAR(Clock_delay) = 0;
   VAR(Line_count) = 0;

   // Open existing stimulus file in current directory
   snprintf(strBuffer, MAXBUF, "%s.sti", GET_INSTANCE());
   VAR(File) = fopen(strBuffer, "r");

   // We can still run if the file won't open; the output pins stay UNKNOWN
   if(!VAR(File)) {
      snprintf(strBuffer, MAXBUF, "Could not open \"%s.sti\" file: %s",
         GET_INSTANCE(), strerror(errno));
      BREAK(strBuffer);
      return;
   }

   // Read the first entry in the stimulus file. If it contains a cycle count of
   // 0, then immediately set the initial output pin state. If the first entry
   // has a non-zero cycle count, then reposition the file pointer back to the
   // beginning; once we know the power on delay from the second call to
   // On_time_step() then we can re-read the first entry and schedule an output
   // pin update.
   if(Read_stimulus(&firstCycle, &firstData) && firstCycle == 0) {
      Set_output(firstData);
   } else {
      rewind(VAR(File));
   }
}

void On_simulation_end()
//**********************
// Undo here the operations done at On_simulation_begin: free memory, close
// files, etc.
{
   Close_file();
}

void On_digital_in_edge(PIN pDigitalIn, EDGE pEdge, double pTime)
//**************************************************************
// Response to a digital input pin edge. The EDGE type parameter (pEdge) can
// be RISE or FALL. Use pin identifers as declared in DECLARE_PINS
{
   /* No Action */
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
   // VMLAB simulates a power on delay in the MCU: there is a large gap of time
   // between the initial On_time_step(0) call and the second call that
   // signifies the start of execution at the reset vector (address 0). Since
   // AVR Studio does not simulate this delay, we subtract it out from the
   // logged cycle time. However, AVR Studio does simulate a 1.5 clock cycle
   // debouncing delay on inputs, hence the "VAR(Clock_period) * 1.5" to get
   // identical stimulus behavior.
   if(!VAR(Clock_delay) && pTime) {
      int nextCycle, nextData;

      VAR(Clock_delay) = pTime + VAR(Clock_period) * 1.5;

      // Once the power on delay is known, regular stimulus scheduling can
      // begin. Read next stimulus file entry and if the read was succesfull,
      // then schedule a new update to the output pins.
      if(Read_stimulus(&nextCycle, &nextData)) {
         Schedule_output(pTime, nextCycle, nextData);
      }
   }
}

void On_remind_me(double pTime, int pData)
//***************************************
// VMLAB notifies about a previouly sent REMIND_ME() function.
// Each time Schedule_output() is called, it reads the next line of the stimulus
// file and schedules a pin state update based on the cycle count in the file.
// The value passed as "pParam" to REMIND_ME() and passed as "pData" to this
// function is the new state of the output pins.
{
   int nextCycle, nextData;

   // Set the new output state from the previous Schedule_output() call.
   Set_output(pData);

   // Read next stimulus file entry and if the read was succesfull, then
   // schedule a new update to the output pins.
   if(Read_stimulus(&nextCycle, &nextData)) {
      Schedule_output(pTime, nextCycle, nextData);
   }
}

void On_gadget_notify(GADGET pGadgetId, int pCode)
//************************************************
// A window gadget (control) is sending a notification.
{
   // No Action
}
