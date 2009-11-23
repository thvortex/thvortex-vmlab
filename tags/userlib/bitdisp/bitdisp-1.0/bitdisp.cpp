// =============================================================================
// Component name: bitdisp v1.0
//
// This component provides a GUI interface to monitor the state of 8 digital
// input pins. The component shows the logic value applied to each individual
// pin, in addition to displaying the entire 8-bit value in hexadecimal.
// 
// To use this component, use the following component definition:
//
// X _bitdisp <D7> <D6> <D5> <D4> <D3> <D2> <D1> <D0>
//
// The component will monitor and display the logic values present at the
// <D7> through <D0> input pins, with <D7> being the most significant bit.
//
// Version History:
// v1.0 09/28/09 - Initial public release
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
#pragma hdrstop
#include <blackbox.h>
int WINAPI DllEntryPoint(HINSTANCE, unsigned long, void*) {return 1;} // is DLL
//==============================================================================

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
   LOGIC Data[8];  // Last LOGIC value read for each input pin
END_VAR

// You can delare also globals variable outside DECLARE_VAR / END_VAR, but if
// multiple instances of this cell are placed, all these instances will
// share the same variable.

bool Started;          // True if simulation started and interface functions work
WNDPROC Button_proc;   // Original window procedure for button controls

// ============================================================================
// Say here if your component has an associated window or not. Pass as parameter
// the dialog resource ID (from .RC file) or 0 if no window
//
USE_WINDOW(WINDOW_USER_1);   // If window USE_WINDOW(WINDOW_USER_1) (for example)

// =============================================================================

LOGIC Read_pin(int pPin)
//********************
// Read the voltage on an analog input pin and convert to a 0, 1, or UNKNOWN
// logic value. Because the digital input pins to a user component behave like
// Schmitt triggers, using an analog input is the only way to detect an UNKNOWN
// value.
{
   // Since we're expecting a digital input, the input voltage should be either
   // 0, POWER(), or POWER()/2 to designate logic 0, 1, and UNKNOWN. To allow
   // for round off-errors or perhaps input passed though an analog RC filter,
   // the 0 to POWER() voltage range is divided into 3 equal sections. Input
   // voltages in the lower third are considered to be logic 0, the upper third
   // to be logic 1, and the middle third to be UNKNOWN.
   double voltage = GET_VOLTAGE(pPin);
   if(voltage < (1.0/3.0) * POWER()) {
      return 0;
   } else if(voltage > (2.0/3.0) * POWER()) {
      return 1;
   } else {
      return UNKNOWN;
   }
}

char Hex_nibble(LOGIC pData[])
//********************
// Convert 4 consecutive logic value (MSb first) into a hexadecimal character
// or into '?' if any of the values are UNKNOWN. Used by Update_text() to
// display the current pin state as a hexadecimal value in the edit control.
{
   char buf[2];
   unsigned char value = 0;

   for(int i = 0; i < 4; i++) {
      if(pData[i] == UNKNOWN) {
         return '?';
      }
      if(pData[i]) {
         value |= (1 << (7 - i));
      }
   }

   snprintf(buf, 2, "%X", value);
   return buf[0];
}

void Update_text()
{
   char buf[3] = {0};

   buf[0] = Hex_nibble(&VAR(Data)[0]);
   buf[1] = Hex_nibble(&VAR(Data)[4]);

   SetWindowText(GET_HANDLE(GADGET22), buf);
}

void Update_button(HWND pHandle, LOGIC pData)
//********************
// Uptate the state of "pHandle" button to match the logic value in "pData".
// Called once for every button at the start of the simulation to set the
// initial state, and then periodically called from On_update_tick() when an
// input pin changes state.
{
   char *text;
   int state, enable;

   switch(pData) {
      case 0:       text = "0"; state = BST_UNCHECKED; enable = true; break;
      case 1:       text = "1"; state = BST_CHECKED;   enable = true; break;
      case UNKNOWN: text = "X"; state = BST_UNCHECKED; enable = false; break;
      default:      text = "?"; state = BST_UNCHECKED; enable = false; break;
   }

   SendMessage(pHandle, BM_SETCHECK, state, 0);
   SetWindowText(pHandle, text);
   EnableWindow(pHandle, enable);
}

const char *On_create()
//********************
// Perform component creation. It must return NULL if the creation process is
// OK, or a message describing the error cause. The message will show in the
// Messages window. Typical tasks: check passed parameters, open files,
// allocate memory,...
{
   return NULL;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
//*******************************
// Custom window procedure for the button controls. Intercepts and stops mouse
// click messages to make the buttons read-only.
{
   switch(msg) {
      case WM_LBUTTONDBLCLK:
      case WM_LBUTTONDOWN:
         return 0;

      // All other messages are processed by existing edit control procedure
      default:
         break;
   }

   return CallWindowProc(Button_proc, hwnd, msg, wParam, lParam);
}

void On_window_init(HWND pHandle)
//*******************************
// Window initialization. Fill only if the component has an associated window
// -USE_WINDOW(..) not zero-. The Parameter pHandle brings the main component
// window handle. Typical tasks: fill controls with data, intitialize gadgets,
// hook an own Windows structure (VCL, MFC,...)
{
   // Subclass button controls by installing custom window procedures for them.
   // Save the previous procedure so the new custom procedure can call the old
   // the old one for unprocessed messages. Since all controls are of the same
   // window class, they will also have the same old window procedure which
   // can be saved in the same global variable.
   Button_proc = (WNDPROC) GetWindowLongPtr(GET_HANDLE(GADGET1), GWL_WNDPROC);
   for(int i = 1; i <= 8; i++) {
      SetWindowLongPtr(GET_HANDLE(GADGET0 + i), GWL_WNDPROC, (LONG_PTR) WndProc);
   }
}

void On_update_tick(double pTime)
{
   bool changed = false; // Set to true if text box needs to be updated

   // GET_LOGIC() can only be called if the simulation is active
   if(!Started) {
      return;
   }

   // Read logic value at each input pin. If the value has changed since the
   // last update tick then update the button control to match logic state.
   for(int i = 1; i <= 8; i++) {
      LOGIC data = Read_pin(i);
      if(data != VAR(Data)[i - 1]) {
         Update_button(GET_HANDLE(GADGET0 + i), data);
         VAR(Data)[i - 1] = data;
         changed = true;
      }
   }

   // Update text box with current logic state of the input pins
   if(changed) {
      Update_text();
   }
}

void On_destroy()
//***************
// Destroy component. Free here memory allocated at On_create; close files
// etc.
{
}

void On_simulation_begin()
//************************
// VMLAB informs you that the simulation is starting. Initialize pin values
// here Open files; allocate memory, etc.
{
   Started = true;

   // Initialize VAR(Data) array to an unused value forcing the first
   // On_refresh_tick() to initialize all buttons so they match the input
   // pin state.
   for(int i = 0; i < 8; i++) {
      VAR(Data)[i] = -1;
   }
}

void On_simulation_end()
//**********************
// Undo here the operations done at On_simulation_begin: free memory, close
// files, etc.
{
   Started = false;

   // Set all button controls to unknown (?) state on simulation end
   for(int i = 1; i <= 8; i++) {
      HWND handle = GET_HANDLE(GADGET0 + i);
      SendMessage(handle, BM_SETCHECK, BST_UNCHECKED, 0);
      SetWindowText(handle, "?");
      EnableWindow(handle, false);
   }

   // Set text field to unknown value
   SetWindowText(GET_HANDLE(GADGET22), "??");
}
