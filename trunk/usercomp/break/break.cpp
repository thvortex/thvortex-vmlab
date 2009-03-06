// =============================================================================
// C++ code template for a VMLAB user defined component
// Target file must be Windows .DLL (no .EXE !!)
//
// Component name: break v1.0
//
// This component is intended as a debugging and testing aid, as it will
// breakpoint the simulation on every rising edge of the <TRIGGER> pin. By
// combining this component with others, such as the Digital Signal Delay or the
// builtin NAND gate ("ND2"), complex breakpoint conditions may be defined
// without having to write a new application specific user component.
//
// To use this component, use the following component definition:
//
// X _break(<Delay>) <TRIGGER> <CANCEL>
//
// A breakpoint will occur on every rising edge of the <TRIGGER> pin. If the
// <Delay> argument is zero, then the breakpoint occurs immediately during the
// same time step as the rising edge. If the <Delay> argument is non zero, then
// a rising <TRIGGER> edge schedules a future breakpoint to occur <Delay>
// seconds after the rising edge was detected. Once a future breakpoint is
// scheduled in this manner, any further rising edges on <TRIGGER> are ignored
// and will not schedule additional breakpoints, until the original breakpoint
// is hit. Once the simulation is resumed, the <TRIGGER> pin will once again be
// monitored for rising edges.
//
// For normal operation as described above, the <CANCEL> pin must remain low.
// However, if the <CANCEL> pin becomes high, then any pending breakpoints (in
// case <Delay> is non zero) are cancelled. In addition, as long as <CANCEL>
// remains high, any further rising edges on <TRIGGER> will be ignored until the
// <CANCEL> pin goes low again.
//
// Version History:
// v1.0 12/31/08 - Initial public release
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
#include <stdio.h>
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
   DIGITAL_IN(TRIGGER, 1);
   DIGITAL_IN(CANCEL, 2);
END_PINS

// =============================================================================
// Declare module global variables here.
// The reason for this is to keep a set of variables by instance
// To use a variable, do it through the the macro VAR(...)
//
DECLARE_VAR
   double Break_delay; // Delay between detection of rising edge and BREAK()
   double Edge_time;   // Time at which a rising edge was detected on TRIGGER
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

void Check_trigger(double pTime)
//********************
// Called from On_digital_in_edge().and On_time_step() to check the state
// of the TRIGGER pin. If the pin became high, set VAR(Edge_time) to schedule
// a pending breakpoint.
{
   // If a break condition is already pending, do not schedule any further
   // breakpoints and ignore the TRIGGER input until the currently pending
   // BREAK() occurs.
   if(GET_LOGIC(TRIGGER) == 1) {
      if(VAR(Edge_time) == -1) {
         VAR(Edge_time) = pTime;
      }
   }
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
   // Check for valid delay argument
   VAR(Break_delay) = GET_PARAM(1);
   if(VAR(Break_delay) < 0) {
       return "Delay argument must be not be negative";
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
   VAR(Edge_time) = -1;
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
   if(pDigitalIn == TRIGGER) {
      Check_trigger(pTime);
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
   // Check the initial pin state (at time 0) here since On_digial_in_edge()
   // doesn't get called at the beginning of the simulation.
   if(pTime == 0) {
      Check_trigger(0);
   }

   // If the CANCEL pin is asserted, clear any pending BREAK() */
   if(GET_LOGIC(CANCEL) == 1) {
      VAR(Edge_time) = -1;
   }
   
   // Trigger any previously scheduled breakpoints if their time has arrived
   if(VAR(Edge_time) != -1 && pTime >= VAR(Edge_time) + VAR(Break_delay)) {
      char strBuffer[MAXBUF];
      
      snprintf(strBuffer, MAXBUF, "Triggered at %.2f ms",
         VAR(Edge_time) * 1000);
      BREAK(strBuffer);
      
      // Clear pending breakpoint so Check_trigger() can schedule new ones
      VAR(Edge_time) = -1;
   }
}

void On_remind_me(double pTime, int pData)
//***************************************
// VMLAB notifies about a previously sent REMIND_ME() function.
{
   // No Action
}

void On_gadget_notify(GADGET pGadgetId, int pCode)
//************************************************
// A window gadget (control) is sending a notification.
{
   // No Action
}

