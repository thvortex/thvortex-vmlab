// =============================================================================
// Component name: perfmon v1.0
//
// This component measures overall VMLAB performance. It computes and displays
// a ratio of simulated time vs real (or wall) time, and it displays the
// effective clock speed (number of instructions executed per real time).
//
// To use this component, use the following component definition:
//
// X _perfmon NC
//
// This component has one dummy input pin which should be connected to the NC
// (Not Connected) node. This input is not used by the perfmon component, but
// it's needed to satisfy the VMLAB requirement that each user component have
// at least one input or output pin.
//
// Version History:
// v1.0 09/13/09 - Initial public release
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
#include "C:\VMLAB\bin\blackbox.h"
int WINAPI DllEntryPoint(HINSTANCE, unsigned long, void*) {return 1;} // is DLL
//==============================================================================

//==============================================================================
// Declare pins here
//
DECLARE_PINS
   DIGITAL_IN(NC, 1);
END_PINS

// =============================================================================
// Declare module global variables here.
// The reason for this is to keep a set of variables by instance
// To use a variable, do it through the the macro VAR(...)
//
DECLARE_VAR
   BOOL Break;
   double Prev_time;
   ULONGLONG Prev_ticks;
   ULONGLONG Freq_ticks;
END_VAR

// You can delare also globals variable outside DECLARE_VAR / END_VAR, but if
// multiple instances of this cell are placed, all these instances will
// share the same variable.
//

// ============================================================================
// Say here if your component has an associated window or not. Pass as parameter
// the dialog resource ID (from .RC file) or 0 if no window
//
USE_WINDOW(WINDOW_USER_1);

// =============================================================================
// Callback functions. These functions are called by VMLAB at the proper time

const char *On_create()
//********************
// Perform component creation. It must return NULL if the creation process is
// OK, or a message describing the error cause. The message will show in the
// Messages window. Typical tasks: check passed parameters, open files,
// allocate memory,...
{
   BOOL rc = QueryPerformanceFrequency((LARGE_INTEGER *) &VAR(Freq_ticks));
   return rc ? NULL : "Processor does not support high-resolution performance counter";
}

void On_window_init(HWND pHandle)
//*******************************
// Window initialization. Fill only if the component has an associated window
// -USE_WINDOW(..) not zero-. The Parameter pHandle brings the main component
// window handle. Typical tasks: fill controls with data, intitialize gadgets,
// hook an own Windows structure (VCL, MFC,...)
{

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
   VAR(Break) = FALSE;
   VAR(Prev_time) = 0;
   QueryPerformanceCounter((LARGE_INTEGER *) &VAR(Prev_ticks));
}

void On_simulation_end()
//**********************
// Undo here the operations done at On_simulation_begin: free memory, close
// files, etc.
{
   SetWindowText(GET_HANDLE(GADGET1), "? : ?");
   SetWindowText(GET_HANDLE(GADGET2), "? Mhz");
}

void On_update_tick(double pTime)
//**********************
// Called periodically by VMLAB to let the user component update it's GUI
{
   // When On_update_tick() is called immediately after On_break(TRUE), this will reset
   // the accumulated simulated time. Once VMLAB is resumed again, the accumulated
   // ticks will reset by On_break(FALSE). This way, the total real time spent while
   // paused is ignored and doesn't throw off the calculations.
   if(VAR(Break)) {
      VAR(Prev_time) = pTime;
   }

   // Avoid divisions by zero if simulated time has not advanced since last update tick
   // due to a user break and avoid updating the display after On_simulation_end() where
   // pTime == 0 again.
   if(pTime - VAR(Prev_time) <= 0) {
      return;
   }

   ULONGLONG ticks;
   QueryPerformanceCounter((LARGE_INTEGER *) &ticks);

   // Wait until at least one second of real time has elapsed. This helps average out
   // small variations in the simulation speed.
   if(ticks - VAR(Prev_ticks) < VAR(Freq_ticks)) {
      return;
   }

   double realTime = 1.0 * (ticks - VAR(Prev_ticks)) / VAR(Freq_ticks);
   double ratio = (pTime - VAR(Prev_time)) / realTime;
   double clock = ratio * GET_CLOCK();

   char buf[64];
   if(ratio < 1) {
      snprintf(buf, 64, "1 : %.1lf", 1.0 / ratio);
   } else {
      snprintf(buf, 64, "%.1lf : 1", ratio);
   }
   SetWindowText(GET_HANDLE(GADGET1), buf);

   if(clock >= 1e6) {
      snprintf(buf, 64, "%.1lf Mhz", clock / 1e6);
   } else if(clock >= 1e3) {
      snprintf(buf, 64, "%.1lf kHz", clock / 1e3);
   } else {
      snprintf(buf, 64, "%.1lf Hz", clock);
   }
   SetWindowText(GET_HANDLE(GADGET2), buf);

   VAR(Prev_time) = pTime;
   VAR(Prev_ticks) = ticks;
}

void On_break(BOOL pState)
//***************************************
// Called when the user breaks and resumes the simulation
{
   VAR(Break) = pState;

   if(!pState) {
      QueryPerformanceCounter((LARGE_INTEGER *) &VAR(Prev_ticks));
   }
}
