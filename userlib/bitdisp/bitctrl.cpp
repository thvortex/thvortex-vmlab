// =============================================================================
// Component name: bitctrl v1.0
//
// This component provides a GUI interface to control the state of 8 digital
// output pins. The user can either click on the buttons corresponding to
// individual bits, or can type a hexadecimal value for all 8 bits into an edit
// control. The output drive of each pin can also be individually turned on and
// off (i.e. tri-stated).
// 
// To use this component, use the following component definition:
//
// X _bitctrl <D7> <D6> <D5> <D4> <D3> <D2> <D1> <D0>
//
// The component will provide a logic output on the bi-directional <D7> through
// <D0> pins, with <D7> being the most signifinact bit.
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
#include <ctype.h>
#pragma hdrstop
#include <blackbox.h>
int WINAPI DllEntryPoint(HINSTANCE, unsigned long, void*) {return 1;} // is DLL
//==============================================================================

//==============================================================================
// Declare pins here
//
DECLARE_PINS
   DIGITAL_BID(D7, 1);
   DIGITAL_BID(D6, 2);
   DIGITAL_BID(D5, 3);
   DIGITAL_BID(D4, 4);
   DIGITAL_BID(D3, 5);
   DIGITAL_BID(D2, 6);
   DIGITAL_BID(D1, 7);
   DIGITAL_BID(D0, 8);
END_PINS

// =============================================================================
// Declare module global variables here.
// The reason for this is to keep a set of variables by instance
// To use a variable, do it through the the macro VAR(...)
//
DECLARE_VAR
   int Data_value;     // Integer encoding of "Data" button state
   int Output_value;   // Integer encoding of "Output" button state 
END_VAR

// You can delare also globals variable outside DECLARE_VAR / END_VAR, but if
// multiple instances of this cell are placed, all these instances will
// share the same variable.

bool Started;          // True if simulation started and interface functions work
WNDPROC Edit_proc;     // Original window procedure for edit controls

// ============================================================================
// Say here if your component has an associated window or not. Pass as parameter
// the dialog resource ID (from .RC file) or 0 if no window
//
USE_WINDOW(WINDOW_USER_1);   // If window USE_WINDOW(WINDOW_USER_1) (for example)

// =============================================================================
// Callback functions. These functions are called by VMLAB at the proper time

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
// Custom window procedure for the edit controls. Controls what characters
// can be entered into the control.
{
   switch(msg) {

      // Only allow control characters (like backspace) or characters that
      // form valid hex digits.
      case WM_CHAR:
         if(!isprint(wParam))
            break;
         if(wParam >= '0' && wParam <= '9')
            break;
         if(wParam >= 'a' && wParam <= 'f')
            break;
         if(wParam >= 'A' && wParam <= 'F')
            break;

         return 0;

#if 0
      // Right mouse button open a custom pop-up menu only if the edit
      // control already had focus before the right-click
      case WM_RBUTTONDOWN:
         if(GetFocus() == hwnd) {
            PRINT("WM_RBUTTONDOWN");
         }
         return 0;

      // Only allow a paste operation if the clipboard contains valid
      // hexadecimal digits
      case WM_PASTE:
         return 0;
#endif

      // All other messages are processed by existing edit control procedure
      default:
         break;
   }

   return CallWindowProc(Edit_proc, hwnd, msg, wParam, lParam);
}

void On_window_init(HWND pHandle)
//*******************************
// Window initialization. Fill only if the component has an associated window
// -USE_WINDOW(..) not zero-. The Parameter pHandle brings the main component
// window handle. Typical tasks: fill controls with data, intitialize gadgets,
// hook an own Windows structure (VCL, MFC,...)
{
   // Subclass edit controls by installing a custom window procedure for both.
   // Save the previous procedure so the new custom procedure can call the old
   // the old one for unprocessed messages. Since both controls are of the same
   // window class, they will also have the same old window procedure which
   // can be saved in the same global variable.
   Edit_proc = (WNDPROC) GetWindowLongPtr(GET_HANDLE(GADGET22), GWL_WNDPROC);
   SetWindowLongPtr(GET_HANDLE(GADGET22), GWL_WNDPROC, (LONG_PTR) WndProc);
   SetWindowLongPtr(GET_HANDLE(GADGET25), GWL_WNDPROC, (LONG_PTR) WndProc);

   // Set max string length to 2 for hexadecimal mode
   SendMessage(GET_HANDLE(GADGET22), EM_LIMITTEXT, 2, 0);
   SendMessage(GET_HANDLE(GADGET25), EM_LIMITTEXT, 2, 0);
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

   // Set the initial drive/output state of all pins to match the current
   // state for the "Data" and "Output" buttons since the user may have
   // modified then before starting the simulation.
   for(int pin = 1; pin <= 8; pin++) {
      bool logic = VAR(Data_value) & (1 << (8 - pin));
      bool output = VAR(Output_value) & (1 << (8 - pin));

      SET_DRIVE(pin, output);
      if(output) {
         SET_LOGIC(pin, logic);
      }
   }
}

void On_simulation_end()
//**********************
// Undo here the operations done at On_simulation_begin: free memory, close
// files, etc.
{
   Started = false;
}

void On_output(HWND pHandle, GADGET pGadgetId, bool pState)
//**********************
// Update one "Output" button title/check state and pin value to "pState"
{
   int pin = pGadgetId - GADGET10; // Pin number associated with pGadgetId

   // Update "Output" button text to match its state
   SetWindowText(pHandle, pState ? "On" : "Off");

   // Pin interface functions can only be used if simulation is running
   if(Started) {

      // Set the pin's drive direction to match that of the button
      SET_DRIVE(pin, pState);

      // If output pin was enabled, then immediately set it's logic value
      // to match the state of the corresponding "Data" button.
      // TODO: Change to use VAR(Data_value)
      if(pState) {
         bool state = VAR(Data_value) & (1 << (8 - pin));
         SET_LOGIC(pin, state);
      }
   }
}

void On_data(HWND pHandle, GADGET pGadgetId, bool pState)
//**********************
// Update one "Data" button title/check state and pin value to "pState"
{
   int pin = pGadgetId - GADGET0; // Pin number associated with pGadgetId

   // Update "Data" button text to match its state
   SetWindowText(pHandle, pState ? "1" : "0");

   // If the pin already configured as output, then set it's logic value
   // to match the state of the "Data" button. The pin interface functions
   // can only be used if the simulation has already started.
   if(Started && GET_DRIVE(pin) & 1) {
      SET_LOGIC(pin, pState);
   }
}

void On_edit_change(HWND pHandle, int &pValue, GADGET pStartId,
   void (*pFunc)(HWND, GADGET, bool))
//************************************************
// Called in response to a EN_CHANGE notification from either edit control.
// The text in the "pHandle" control is converted to a number and stored
// in "pValue". The eight buttons beginning with gadget ID "pStartId" are
// updated to match the new "pValue" and the "pFunc" is called to update
// the pin state.
{
   // Retrieve updated text from edit control and convert to a number. If the
   // control is blank or contains invalid characters then set number to 0.
   char buf[16];
   GetWindowText(pHandle, buf, 16);
   pValue = 0;
   sscanf(buf, " %x", &pValue);

   // Update button and pin state to correspond with new text field value
   for(int id = pStartId + 1; id <= pStartId + 8; id++) {
      bool state = pValue & (1 << (8 - id + pStartId));
      HWND handle = GET_HANDLE(id);

      SendMessage(handle, BM_SETCHECK, state ? BST_CHECKED : BST_UNCHECKED, 0);
      pFunc(handle, id, state);
   }
}

void Update_edit(int pValue, GADGET pGadgetId)
//************************************************
// Called in response to a button press to update the text in the edit control
// "pGadgetId" to match the "pValue" variable. Changing the edit box text causes
// a EN_CHANGE to be sent, which in turn is processed by On_gadget_notify() to
// update the button state.
{
   char buf[16];
   snprintf(buf, 16, "%02X", pValue);
   SetWindowText(GET_HANDLE(pGadgetId), buf);
}

void On_gadget_notify(GADGET pGadgetId, int pCode)
//************************************************
// A window gadget (control) is sending a notification.
{
   HWND handle = GET_HANDLE(pGadgetId);

   switch(pGadgetId) {
   
      // Set all "Data" buttons to "1"
      case GADGET20:
         VAR(Data_value) = 0xFF;
         Update_edit(VAR(Data_value), GADGET22);
         break;
         
      // Set all "Data" buttons to "0"
      case GADGET21:
         VAR(Data_value) = 0x00;
         Update_edit(VAR(Data_value), GADGET22);
         break;
         
      // Modified text in "Data" edit control
      case GADGET22:
         if(pCode == EN_CHANGE) {
            On_edit_change(handle, VAR(Data_value), GADGET0, On_data);
         }
         break;
         
      // Set all "Output" buttons to "On"
      case GADGET23:
         VAR(Output_value) = 0xFF;
         Update_edit(VAR(Output_value), GADGET25);
         break;
       
      // Set all "Output" buttons to "Off"
      case GADGET24:
         VAR(Output_value) = 0x00;
         Update_edit(VAR(Output_value), GADGET25);
         break;
         
      // Modified text in "Output" edit control
      case GADGET25:
         if(pCode == EN_CHANGE) {
            On_edit_change(handle, VAR(Output_value), GADGET10, On_output);
         }
         break;

      // Individual "Data" or "Output" button was pressed
      default:

         // If "Data" button pressed
         if(pGadgetId >= GADGET0 + 1 && pGadgetId <= GADGET0 + 8) {
            VAR(Data_value) ^= (1 << (8 - pGadgetId + GADGET0));
            Update_edit(VAR(Data_value), GADGET22);
         }

         // If "Output" button pressed
         else if(pGadgetId >= GADGET10 + 1 && pGadgetId <= GADGET10 + 8) {
            VAR(Output_value) ^= (1 << (8 - pGadgetId + GADGET10));
            Update_edit(VAR(Output_value), GADGET25);
         }
         
         break;
   }   
}
