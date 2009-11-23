// =============================================================================
// Component name: led7seg v1.0
//
// To use these components, use one of the following component definitions:
//
// X<Name> _led7cc <A> <B> <C> <D> <E> <F> <G> <DP> <CATHODE>
// X<Name> _led7ca <A> <B> <C> <D> <E> <F> <G> <DP> <ANODE>
//
// The instance <Name> must end with a decimal number. It is this number which
// associates an instance of the component with a 7-Segment display in the
// control panel window. Pins <A> through <G> control the individual segments of
// the display, and pin <DP> controls the decimal point. The common <CATHODE>
// or <ANODE> pin acts as a global enable for the entire display.
//
// The "led7cc" component implements a common cathode display. Its LED segments
// will illuminate when a logic 1 is applied to any of the individual segment
// pins and a logic 0 is applied to the common <CATHODE> pin. The "led7ca"
// component implements a common anode display, which requires a logic 0 on the
// individual pins and a logic 1 on the common <ANODE> pins to illuminate.
//
// Version History:
// v1.0 02/16/09 - Implemented as both common anode and cathode components
// v0.1 02/13/09 - Initial public release
//
// Copyright (C) 2009 Wojciech Stryjewski <thvortex@gmail.com>
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
#include <string>
#include <map>
#include <set>
#pragma hdrstop
#include "C:\VMLAB\bin\blackbox.h"
#include "led7seg.h"

using namespace std;

int WINAPI DllEntryPoint(HINSTANCE, unsigned long, void*) {return 1;} // is DLL
//==============================================================================

// This macro checks that the assertion condition is true. If it's false then
// the Error() function gets called to breakpoint the simulation with the error.
#define ASSERT(cond) \
   if(!(cond)) { Error(__FILE__, __LINE__, #cond); }

// The CC() macro makes it possible to compile both common cathode and anode
// components from the same source. Anytime an input pin value is checked, this
// macro is used to reverse the logic condition if compiling as common anode, or
// to leave the condition unchanged if compiling as common cathode. This ifdef
// block also defines some cathode/anode specific strings used elsewhere.
#if defined(LED7CC)
#define CC(x) (x)
#define DLL_NAME "led7cc.dll"
#define LABEL_NAME "Common Cathode"

#elif defined(LED7CA)
#define CC(x) (!(x))
#define DLL_NAME "led7ca.dll"
#define LABEL_NAME "Common Anode"

#else
#error "Component must be compiled with either -DLED7CC or -DLEC7CA"
#endif

// Size of temporary string buffer for generating filenames and error messages
#define MAXBUF 256

// Array indices and resource ID offsets (relative to ICON_BASE) for all of the
// LED segment images.
const int ICON_ID[ICON_NUM] = {
   LED_H, LED_V, LED_D, LED_H_ON, LED_V_ON, LED_D_ON
};

// This lookup table maps each LED segment in a 7-segment display to the
// corresponding icon for its on and off states. The entries in this table must
// be in the same A, B, ..., DP order as the static controls in the DIALOG
// resource.
const struct {
   int on;  // Index into Icon_handle[] for "on" state
   int off; // Index into Icon_handle[] for "off" state
} LED_ICON_ID[LED_NUM] = {
   { LED_H_ON, LED_H }, { LED_V_ON, LED_V }, { LED_V_ON, LED_V },
   { LED_H_ON, LED_H }, { LED_V_ON, LED_V }, { LED_V_ON, LED_V },
   { LED_H_ON, LED_H }, { LED_D_ON, LED_D }
};

//==============================================================================
// Declare pins here
// Note that the pin declarations here must be in the same order as the
// LED_ICON_ID[] array above and in the same numeric order as the static
// controls in the DIALOG resource which display each LED segment.
//
DECLARE_PINS
   DIGITAL_IN(A, 1);
   DIGITAL_IN(B, 2);
   DIGITAL_IN(C, 3);
   DIGITAL_IN(D, 4);
   DIGITAL_IN(E, 5);
   DIGITAL_IN(F, 6);
   DIGITAL_IN(G, 7);
   DIGITAL_IN(DP, 8);
   DIGITAL_IN(COMMON, 9);
END_PINS

// =============================================================================
// Declare module global variables here.
// The reason for this is to keep a set of variables by instance
// To use a variable, do it through the the macro VAR(...)
//
DECLARE_VAR
   int Panel_number;   // Index into Dialog_handle for this shared panel frame
   int Display_number; // Position of 7seg display within shared panel frame
END_VAR

// You can delare also globals variable outside DECLARE_VAR / END_VAR, but if
// multiple instances of this cell are placed, all these instances will
// share the same variable.

// Global array holading all loaded icon images for individual LED segments.
HANDLE Icon_handle[ICON_NUM];

// Each group of 7-segment displays is shown in a single control panel frame.
// The key to this map is always the zero based id number of the leftmost
// 7-segment display in the control panel frame, and the value this key maps
// to is a handle to the dialog frame itself. For example, if a project file
// contains segments 3, 7, and 11, then this map will contain an entry at 0
// (which contains the frame with displays 3 and 7) and an entry at 8
// (containing the frame with display 11).
map<int, HWND> Dialog_handle;

// This set tracks all of the control panel frames that still need to be created
// by New_window() via USE_WINDOW().
set<int> Dialog_tocreate;

// Keeps track of which dialogs have already been created by USE_WINDOW()
// but have not yet been assigned to the Dialog_handle map via a
// On_window_init() call.
set<int> Dialog_toassign;

// Total number of component instances. The very first instance is responsible
// for loading the icon resources and the very last instance is responsible for
// freeing them.
int Instance_count = 0;

// =============================================================================
// Helper Functions

void Error(char *pFileName, int pLineNo, char *pAssertCond)
//*************************
// Used by the ASSERT() macro to breakpoint the simulation with an error message
// if the assert condition if false. The error message contains the instance
// name, source file/line number, and the text of the failed condition.
{
   char strBuffer[MAXBUF];
   
   snprintf(strBuffer, MAXBUF, "%s(%d): Assert Failure: %s",
      pFileName, pLineNo, pAssertCond);  
 
   BREAK(strBuffer);
}

void Set_LED(int pPin, BOOL pState)
//********************
// Change the state of the LED segment number "pPin" (1 is A, 8 is DP) to either
// the "on" or "off" image depending on the "pState" variable and the logic
// value present on the common cathode or anode pin which acts like a global
// enable for the display.
{
   // Combine individual segment state with logic value on common pin
   pState = pState && CC(GET_LOGIC(COMMON) == 0);

   // Index into Icon_handle[] array for the icon that represents current state
   int iconIdx = pState ? LED_ICON_ID[pPin - 1].on : LED_ICON_ID[pPin - 1].off;
   
   // Identifier within dialog of the static image control that gets updated
   int itemIdx = LED_BASE + (VAR(Display_number) * LED_NUM) + pPin - 1;
   
   // Handle to the control panel frame containing the static image control
   HWND dialogHandle = Dialog_handle[VAR(Panel_number)];

   ASSERT(
      SendDlgItemMessage(
         dialogHandle,                 // hDlg (handle to dialog window)
         itemIdx,                      // nIDDlgItem (control identifier)
         STM_SETIMAGE,                 // Msg (control specific message to send)
         (WPARAM) IMAGE_ICON,          // wParam (image type is an icon)
         (LPARAM) Icon_handle[iconIdx] // lParam (handle to the icon itself)
      )
   );
}

void Set_All_LEDs(BOOL pGetLogic)
//********************
// If "pGetLogic" is TRUE, then change the state of all LED segments to match
// the logic values present at the corresponding input pins. When called from
// On_simulation_end(), "pGetLogic" is FALSE in which case all segments are
// simply turned off.
{
   for(int i =  1; i <= LED_NUM; i++) {
      Set_LED(i, CC(GET_LOGIC(i) == 1) && pGetLogic);
   }
}

int New_window()
//********************
// Invoked as an argument to the USE_WINDOW() VMLAB API.
// Return WINOW_USER_1 if any dialog panel frames still remain to be created,
// as determined by the Dialog_tocreate set initialized by On_create(). If no
// panels need to be created then return 0.
// WARNING: This function may be called before the instance specific VAR()
// block is allocated; using VAR() in this circuimstance will lead to a VMLAB
// crash due to a NULL pointer reference.
{
   set<int>::iterator iterator = Dialog_tocreate.begin();

   // If the global Dialog_tocreate has any entries, then those shared control
   // panel frames haven't been created yet. Request that a new panel be
   // created, and move it from the Dialog_tocreate to the Dialog_toassign set.
   // When the On_window_init() callback is called later, it will go through
   // entries in Dialog_toassign and will store the actual window handles in the
   // Dialog_handle map.
   if(iterator != Dialog_tocreate.end()) {
      int panelNumber = *iterator;
      
      Dialog_tocreate.erase(panelNumber);
      Dialog_toassign.insert(panelNumber);
      
      return WINDOW_USER_1;
   }

   // If all the panels waiting to be created have been processed, then return
   // a 0 so no more windows are created.
   return 0;
}

// ============================================================================
// Say here if your component has an associated window or not. Pass as parameter
// the dialog resource ID (from .RC file) or 0 if no window.
USE_WINDOW(New_window());

// =============================================================================
// Callback functions. These functions are called by VMLAB at the proper time

const char *On_create()
//********************
// Perform component creation. It must return NULL if the creation process is
// OK, or a message describing the error cause. The message will show in the
// Messages window. Typical tasks: check passed parameters, open files,
// allocate memory,...
{
   // The very first instance to have its On_create() called is responsible for
   // loading the icon resources needed to display all of the LED segments.
   // To avoid memory leaks, these have to be freed later by the last instnace
   // in On_destroy() because LoadImage() will by default create a new private
   // copy of the icon.
   Instance_count++;
   if(Instance_count == 1) {
   
      // Initialize the Icon_handle[] array to all NULLs. In case the icon
      // loading below fails, this will allow the On_destroy() callback to clean
      // up by only freeing those icons that were actually loaded.
      for(int i = 0; i < ICON_NUM; i++) {
         Icon_handle[i] = NULL;
      }   

      // Get handle to this loaded DLL module. Needed to load the icon
      // resources. Note that the compiled DLL file must be renamed or this
      // function will fail. The GetModuleHandleEx() function would be
      // preferred here since it can lookup a module based on a function
      // address, but it is only available on Windows 2003/XP or later.
      HMODULE dllModule = GetModuleHandle(DLL_NAME);
      if(!dllModule) {
         return "GetModuleHandle() API failed. Was the DLL file renamed?";
      }

      // Load all of the ICON resources from the DLL file
      for(int i = 0; i < ICON_NUM; i++) {
         int index = ICON_ID[i];

         Icon_handle[index] = LoadImage(
            dllModule,  // hinst (handle to DLL module containing resources)
            MAKEINTRESOURCE(ICON_BASE + index), // lpszName (ICON ordinal ID)
            IMAGE_ICON, // uType (image type is an icon)
            0,          // cxDesired (use actual icon width)
            0,          // cyDesired (use actual icon height)
            0           // fuLoad (default options)
         );

         // If any ICON resource cannot be loaded then return with an error.
         // Normally, this should never happen since the ICON resources are
         // embedded into the DLL file itself and therefore should always
         // be available.
         if(!Icon_handle[index]) {
            return "Cannot load ICON resources from DLL";
         }
      }
   }
   
   // Create a substring to the trailing part of the instance name which should
   // contain only decimal digits.
   string instName(GET_INSTANCE());
   UINT pos = instName.find_last_not_of("0123456789");
   string instNumber = instName.substr(pos == string::npos ? 0 : pos + 1);
   
   // Convert the substring to a decimal number. This number is the 1 based
   // numeric id of this 7-segment display, and it controls which set of LED
   // segments this component instance controls in the shared control panel
   // frame.
   int instanceNumber = atoi(instNumber.c_str());
   if(instanceNumber <= 0) {
      return "Instance name must be an integer greater than zero";
   }
   
   // Separate the instance ID into a 7-seg display number (within a shared
   // control panel frame) and a control panel index used to lookup the
   // shared frame that this 7-seg display resides in. Both numbers are zero
   // based. The user visible instance ID is 1 based but the internal variables
   // are 0 based.
   VAR(Display_number) = (instanceNumber - 1) % DISP_NUM;
   VAR(Panel_number) = (instanceNumber - 1) / DISP_NUM * DISP_NUM;

   // Store this panel number for later creation by New_window(). Because the
   // set doesn't allow duplicate entries, this also ensures that panels with
   // duplicate displays aren't created.
   Dialog_tocreate.insert(VAR(Panel_number));
      
   return NULL;
}

void On_window_init(HWND pHandle)
//*******************************
// Window initialization. Fill only if the component has an associated window
// -USE_WINDOW(..) not zero-. The Parameter pHandle brings the main component
// window handle. Typical tasks: fill controls with data, intitialize gadgets,
// hook an own Windows structure (VCL, MFC,...)
// WARNING: For some reason VMLAB mixes up the USE_WINDOW() and On_window_init()
// callbacks between multiple instances of the same component. This is the
// reason that a separate "Dialog_toassign" is needed instead of simply using
// the current value of VAR(Panel_number).
{
   char strBuffer[MAXBUF];

   // We retrieve the next tpanel number that needs to be associated with a
   // window handle from the Dialog_toassign set. A nice side effect of using a
   // sorted set is that the acontrol panel frames re always displayed in order
   // based on the display numbers they contain.
   ASSERT(!Dialog_toassign.empty());
   int panelNumber = *Dialog_toassign.begin();
      
   // Store the window handle in the global Dialog_handle map so that it can be
   // retrieved later by other instances sharing the same.
   Dialog_toassign.erase(panelNumber);
   Dialog_handle[panelNumber] = pHandle;

   // Replace the title of this control panel frame with a custom one. The extra
   // spaces at the beginning skip over the EXPAND_BUTTON that overlaps the
   // beginning of the title.
   snprintf(strBuffer, MAXBUF, "      LED 7-Segment Display (%s): %d-%d",
      LABEL_NAME, panelNumber + 1, panelNumber + 8   
   );   
   ASSERT(SetDlgItemText(pHandle, EXPAND_FRAME, strBuffer));
      
   // Initializes the text labels in the dialog window to show which 7-seg
   // displays are assigned to this control panel frame.
   for(int i = 0; i < DISP_NUM; i++) {
      ASSERT(
         SetDlgItemInt(pHandle, LABEL_BASE + i, panelNumber + i + 1, FALSE)
      );
   }
}

void On_destroy()
//***************
// Destroy component. Free here memory allocated at On_create; close files
// etc.
{
   // The last component to have its On_destroy() function called is responsible
   // for freeing any resources acquired by the first component to enter
   // On_create().
   Instance_count--;
   if(Instance_count == 0) {
      ASSERT(Dialog_tocreate.empty());
      ASSERT(Dialog_toassign.empty());

      // Re-initialize global data structures in anticipation of the next run
      Dialog_handle.clear();

      // Free all of the previously loaded ICON resources. For any resources
      // that could not be loaded, the Icon_handle[] entry is NULL and the
      // DestroyIcon() call will be skipped.
      for(int i = 0; i < ICON_NUM; i++) {
         if(Icon_handle[i]) {
            ASSERT(DestroyIcon((HICON) Icon_handle[i]));
         }
      }   
   }
}

void On_simulation_begin()
//************************
// VMLAB informs you that the simulation is starting. Initialize pin values
// here Open files; allocate memory, etc.
{
   // No Action
}

void On_simulation_end()
//**********************
// Undo here the operations done at On_simulation_begin: free memory, close
// files, etc.
{
   // If the simulation is shutting down, reset all of the LED segments to the
   // initial "grayed" out look they started with.
   Set_All_LEDs(FALSE);
}

void On_digital_in_edge(PIN pDigitalIn, EDGE pEdge, double pTime)
//**************************************************************
// Response to a digital input pin edge. The EDGE type parameter (pEdge) can
// be RISE or FALL. Use pin identifers as declared in DECLARE_PINS
{
   // If the common pin changed state, then enable/disable all LED segments
   if(pDigitalIn == COMMON) {
      Set_All_LEDs(TRUE);
   }
   
   // If a single pin changed, then only update that one corresponding segment
   else {
      Set_LED(pDigitalIn, CC(pEdge == RISE));
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
   // Since On_digital_in_edge() is never called to report the initial state
   // of the pins, the code below does it manually at the time 0 which is
   // the start of the simulation.
   if(pTime == 0) {
      Set_All_LEDs(TRUE);
   }
}

void On_remind_me(double pTime, int pData)
//***************************************
// VMLAB notifies about a previouly sent REMIND_ME() function.
{
   // No action
}

void On_gadget_notify(GADGET pGadgetId, int pCode)
//************************************************
// A window gadget (control) is sending a notification.
{
   // No action
}
