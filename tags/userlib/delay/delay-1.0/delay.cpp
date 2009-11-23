// =============================================================================
// C++ code template for a VMLAB user defined component
// Target file must be Windows .DLL (no .EXE !!)
//
// Component name: delay v1.0
//
// This component is a single bit digital buffer with user configurable
// propagation delays. When the input signal changes state, it must remain at
// the new logic level for the duration of the delay. If the input is changing
// faster than the delay, then these changes are ignored and do not affect the
// output. In this regard, the delay component also acts like a digital low-pass
// filter.
//
// To use this component, use the following component definition:
//
// X _delay(<RiseDelay> <FallDelay>) <DIN> <DOUT>
//
// The <RiseDelay> and <FallDelay> arguments specify two different delays for
// respectively propagating rising and falling edges of the input signal from
// the <DIN> pin to the <DOUT> pin. The <RiseDelay> is the minimum length of
// time that a logic 1 must be applied for on the input pin before showing up on
// the output. <FallDelay> serves the same purpose but for a logic 0 on input.
// The two delays need not be the same, and either or both can be zero. With a
// zero delay, the input signal will propagate on the next instruction cycle of
// the MCU (i.e. the minimum time resolution of the simulator). Note that for an
// UNKNOWN 'X' logic level on <DIN>, the propagation delay is always zero.
//
// Version History:
// v1.0 12/30/08 - Initial public release
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
// GENERAL, SPECIAL, INCIDENTAL OR CONSEQUENeTIAL DAMAGES ARISING OUT OF THE USE
// OR INABILITY TO USE THE PROGRAM (INCLUDING BUT NOT LIMITED TO LOSS OF DATA OR
// DATA BEING RENDERED INACCURATE OR LOSSES SUSTAINED BY YOU OR THIRD PARTIES OR
// A FAILURE OF THE PROGRAM TO OPERATE WITH ANY OTHER PROGRAMS), EVEN IF SUCH
// HOLDER OR OTHER PARTY HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
//
#include <windows.h>
#include <commctrl.h>
#pragma hdrstop
#include "C:\VMLAB\bin\blackbox.h"
int WINAPI DllEntryPoint(HINSTANCE, unsigned long, void*) {return 1;} // is DLL
//==============================================================================

//==============================================================================
// Declare pins here
// Unfortunately, DIGITAL_IN() does not reliably return UNKNOWN for input pins;
// it only seems to return it at the very beginning of the simulation. To
// actually detect UNKNOWN logic values, the input is declared as analog, in
// which case an UNKNOWN logic value is presented as a voltage level of
// "POWER() / 2".
//
DECLARE_PINS
   ANALOG_IN(DIN, 1);    // The input signal
   DIGITAL_OUT(DOUT, 2); // A delayed version of the input signal
END_PINS

// =============================================================================
// Declare module global variables here.
// The reason for this is to keep a set of variables by instance
// To use a variable, do it through the the macro VAR(...)
//
DECLARE_VAR
   double Rise_delay;   // Propagation delay for rising edge on input
   double Fall_delay;   // Propagation delay for falling edge on input
   double Output_time;  // Time at which the pending output change occurs
   LOGIC Output_value;  // Pending logic value that will be applied to output
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
// Callback functions. These functions are called by VMLAB at the proper time

const char *On_create()
//********************
// Perform component creation. It must return NULL if the creation process is
// OK, or a message describing the error cause. The message will show in the
// Messages window. Typical tasks: check passed parameters, open files,
// allocate memory,...
{
   VAR(Rise_delay) = GET_PARAM(1);
   VAR(Fall_delay) = GET_PARAM(2);

   // Check for valid delay arguments
   if(VAR(Rise_delay) < 0 || VAR(Fall_delay) < 0) {
       return "Delay arguments must not be negative";
   }
   
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
{
   VAR(Output_value) = UNKNOWN;
   VAR(Output_time) = -1;
}

void On_simulation_end()
//**********************
// Undo here the operations done at On_simulation_begin: free memory, close
// files, etc.
{
   // No Action
}

void On_digital_in_edge(PIN pDigitalIn, EDGE pEdge, double pTime)
//**************************************************************
// Response to a digital input pin edge. The EDGE type parameter (pEdge) can
// be RISE or FALL. Use pin identifers as declared in DECLARE_PINS
{
   // No Action
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
   // Since we're expecting a digital input, the input voltage should be either
   // 0, POWER(), or POWER()/2 to designate logic 0, 1, and UNKNOWN. To allow
   // for round off-errors or perhaps input passed though an analog RC filter,
   // the 0 to POWER() voltage range is divided into 3 equal sections. The
   // lower, middle, and upper voltage ranges are mapped to logic 0, UNKNOWN,
   // and 1.
   double voltage = GET_VOLTAGE(DIN);

   // TODO: SEPARATE THE VAR(Output_value) CHECK INTO NESTED IF
   if(voltage > (2.0/3.0) * POWER()) {
      if(VAR(Output_value) != 1) {
         VAR(Output_value) = 1;
         VAR(Output_time) = pTime + VAR(Rise_delay);
         //PRINT("IN: RISE");
      }
   }
   
   else if(voltage < (1.0/3.0) * POWER()) {
      if(VAR(Output_value) != 0) {
         VAR(Output_value) = 0;
         VAR(Output_time) = pTime + VAR(Fall_delay);
         //PRINT("IN: FALL");
      }
   }
   
   else {
      if(VAR(Output_value) != UNKNOWN) {
         VAR(Output_value) = UNKNOWN;
         VAR(Output_time) = pTime;
         //PRINT("IN: UNKNOWN");
      }
   }
   
   // Change the output pin to match input, if the input pin has been held for
   // long enough at its current logic value. If the input becomes UNKNOWN,
   // then the output pin will also change to the UNKNOWN state but with only a
   // single clock sycle delay.
   if(VAR(Output_time) != -1 && pTime >= VAR(Output_time)) {
      SET_LOGIC(DOUT, VAR(Output_value));
      VAR(Output_time) = -1;
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
   // No Action
}
