// =============================================================================
// C++ code template for a VMLAB user defined component
// Target file must be Windows .DLL (no .EXE !!)
//
// Component name: vcdlog v1.0
//
// This component implements a 1-bit digital data logger that creates a log
// file in the Verilog Value Change Dump format. This file can be viewed by
// an external application such as GTKWave.
//
// To use this component, use the following component definition:
//
// X<Name> _vcdlog <Data>
//
// The component always writes to a file named "vcdlog.vcd", and if multiple
// component instances are used in the same project file, then the logged data
// from each instance is interleaved within the file. The instance <Name> is
// used as the variable name in the VCD file. <Data> is the single input
// bit being logged. The VCD file always uses a 1ns timescale, which will
// be adequate for all clock speeds under 1Ghz. The maximum number of vcdlog
// instances allowed per project is 94 (due to the single character ASCII
// identifiers used in a VCD file).
//
// Version History:
// v1.0 11/25/08 - Initial public release
//
// Written by Wojciech Stryjewski, 2008
//
// This work is hereby released into the Public Domain. To view a copy of the
// public domain dedication, visit http://creativecommons.org/licenses/publicdomain/
// or send a letter to Creative Commons, 171 Second Street, Suite 300,
// San Francisco, California, 94105, USA.
//
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#pragma hdrstop
#include "C:\VMLAB\bin\blackbox.h"
int WINAPI DllEntryPoint(HINSTANCE, unsigned long, void*) {return 1;} // is DLL
//==============================================================================

// Size of temporary string buffer for generating error messages
#define MAXBUF 256

// Multipler for converting elapsed time (in seconds) to an integer number of
// nanoseconds for the VCD log file.
#define TIME_MULT 1E9
#define TIME_UNITS "ns"

// Output filename always created by this component
#define FILE_NAME "vcdlog.vcd"

// The lowest and highest ASCII characters that are allowed in a VCD file for
// identifying the value changes with the appropriate variable name from the
// header section.
#define MIN_ID '!'
#define MAX_ID '~'

//==============================================================================
// Declare pins here
// Unfortunately, DIGITAL_IN() does not reliably return UNKNOWN for input pins;
// it only seems to return it at the very beginning of the simulation. To
// actually detect UNKNOWN logic values, the input is declared as analog, in
// which case an UNKNOWN logic value is presented as a voltage level of
// "POWER() / 2".
DECLARE_PINS
   ANALOG_IN(DATA, 1);
END_PINS

// =============================================================================
// Declare module global variables here.
// The reason for this is to keep a set of variables by instance
// To use a variable, do it through the the macro VAR(...)
//
DECLARE_VAR
   LOGIC Log_data;         // Previous pin state already written to the log
   int Instance_number;    // Number of this component instance
END_VAR

// You can delare also globals variable outside DECLARE_VAR / END_VAR, but if
// multiple instances of this cell are placed, all these instances will
// share the same variable.

// Global log file shared by all vcdlog component instances. The first instance
// whose On_simulation_begin() is called is responsible for opening it, and the
// first instance whose On_simulation_end() is called is responsible for
// closing it. The file will also be closed if any I/O errors occur to disable
// any further logging and prevent the user from being flooded with error
// messages.
FILE *File = NULL;

// Total number of vcdlog component instances. Since the VCD file requires a
// different ASCII character identifier for each signal, this count is used
// to initialize VAR(Instance_number).
int Instance_count = 0;

// Time step at which any vcdlog instance last wrote to the log file.
double Log_time = -1;

// Total amount of time elapsed in the current simulation. When the simulation
// is finished and On_simulation_end() is called, it will record the total
// elapsed time as the last line in the VCD file.
double Total_time;

// ============================================================================
// Say here if your component has an associated window or not. Pass as parameter
// the dialog resource ID (from .RC file) or 0 if no window
//
USE_WINDOW(0);   // If window USE_WINDOW(WINDOW_USER_1) (for example)

// =============================================================================
// Helper Functions

void Close_file(void)
//********************
// Close the global log file, and check for any I/O errors that can occur
// when flushing the remaining output.
{
   char strBuffer[MAXBUF];

   // Do nothing if the log file is already closed
   if(!File) {
      return;
   }

   // Close the log file so it can be moved or deleted
   if(fclose(File)) {
      snprintf(strBuffer, MAXBUF, "Error closing/flushing \"%s\" file: %s",
         FILE_NAME, strerror(errno));
      BREAK(strBuffer);         
   }
   
   File = NULL;
}

void Log_printf(char *fmt, ...)
//********************
// Wrapper around vfprintf that checks for any file I/O errors. Since the
// printf() style functions will occasionally flush the stdio buffers, they
// can return a system level error (such as disk full). Using a wrapper
// function that checks for errors immediately after each printf() guarantees
// that errno will still be valid and that strerror() wlll produce a useful
// message to the user.
{
   char strBuffer[MAXBUF];
   va_list args;

   // Do nothing if the log file could not be opened or was already closed
   // due to a previous error.
   if(!File) {
      return;
   }

   // Call the real fvprintf function to do the actual printing
   va_start(args, fmt);
   vfprintf(File, fmt, args);

   // If any I/O error occurred, break with error message and close log file
   if(ferror(File)) {
      snprintf(strBuffer, MAXBUF, "Could not write \"%s\" file: %s",
         FILE_NAME, strerror(errno));
      BREAK(strBuffer);
      Close_file();
   }

   va_end(args);
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
// The first instance to enter this function is responsible for opening the
// single log file and initializing any global data. Although the log filename
// doesn't change with each simulation, it's better to open it here and close it
// in On_simulation_end(). This allows the user to delete or move the log file
// without having to close or rebuild the project.
{
   char strBuffer[MAXBUF];

   // Force the initial value of data input to be logged at time step 0
   VAR(Log_data) = -1;

   // Keep track of how many instances have already been created
   VAR(Instance_number) = Instance_count;
   Instance_count++;
   
   // Because the VCD file format uses a single ASCII character to identify each
   // signal, it limits the number of vcdlog instances that can be used in a
   // project
   if(VAR(Instance_number) + MIN_ID > MAX_ID) {
      snprintf(strBuffer, MAXBUF, "Too many instances (max %d)",
         MAX_ID - MIN_ID + 1);
      BREAK(strBuffer);
      Close_file();
   }

   // The first instance to have its On_simulation_begin() called is responsible
   // for opening the log file and initializing global variables.
   if(Instance_count == 1) {
      Total_time = 0;

      // Setting Log_time to -1 forces the first instance entering
      // On_time_step(), to finish writing the VCD header section.
      Log_time = -1;

      // Create or overwrite log file in current directory
      File = fopen(FILE_NAME, "w");

      // We can still run if the file won't open; we just can't log anything.
      if(!File) {
         snprintf(strBuffer, MAXBUF, "Could not create \"%s\" file: %s",
            FILE_NAME, strerror(errno));
         BREAK(strBuffer);
      }
      
      // Write out the global VCD file header
      Log_printf("$version VMLAB vcdlog component $end\n");
      Log_printf("$timescale 1 %s $end\n", TIME_UNITS);
      Log_printf("$scope module vmlab $end\n");
   }

   // Write out per instance part of the VCD header that contains the variable
   // name and the ASCII identifier.
   Log_printf("$var wire 1 %c %s $end\n",
      VAR(Instance_number) + MIN_ID, GET_INSTANCE());
}

void On_simulation_end()
//**********************
// Undo here the operations done at On_simulation_begin: free memory, close
// files, etc.
{
   // Write the total elapsed simulation time at the end of the VCD file,
   // making sure to convert it to nanoseconds. Without this final "delay"
   // in the VCD file, any bit value changes in the last time step might
   // not be visible in some waveform viewers like GTKWave.
   Log_printf("#%.0lf\n", Total_time * TIME_MULT);

   // Since there is nothing else left to do here, except for closing the
   // log file, we just let the first instance to enter On_simulation_end()
   // close the file and reset the global Instance_count in preparation for
   // another simulation.
   Close_file();
   Instance_count = 0;
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
   LOGIC newData;

   // Do nothing if the log file could not be opened by the first instance
   if(!File) {
      return;
   }

   // Record the total elapsed simulation time for use in On_simulation_end()
   Total_time = pTime;
   
   // Since we're expecting a digital input, the input voltage should be either
   // 0, POWER(), or POWER()/2 to designate logic 0, 1, and UNKNOWN. To allow
   // for round off-errors or perhaps input passed though an analog RC filter,
   // the 0 to POWER() voltage range is divided into 3 equal sections. Input
   // voltages in the lower third are considered to be logic 0, the upper third
   // to be logic 1, and the middle third to be UNKNOWN.
   double voltage = GET_VOLTAGE(DATA);
   if(voltage < (1.0/3.0) * POWER()) {
      newData = 0;
   } else if(voltage > (2.0/3.0) * POWER()) {
      newData = 1;
   } else {
      newData = UNKNOWN;
   }

   // The first component to enter On_time_step(0) at the beginning of the
   // simulation is responsible for finishing the VCD header section.
   if(Log_time == -1 && pTime == 0) {
      Log_printf("$upscope $end\n");
      Log_printf("$enddefinitions $end\n");
      Log_time = 0;
   }
   
   // If the current state of the input pin is different from the previous
   // state in the last time step, then the new state needs to be logged
   if(newData != VAR(Log_data)) {
      char id = VAR(Instance_number) + MIN_ID;

      // If this component instance is the first to log something at the
      // current "pTime" then also write the current elapsed time to the file
      if(pTime > Log_time) {
         // The elapsed time (in seconds) is logged as an integer number of
         // nanoseconds. To avoid any floating point round off errors, the
         // fprintf() is used to round up the result to the closest integer
         // instead of using an integer cast.
         Log_printf("#%.0lf\n", pTime * TIME_MULT);
         Log_time = pTime;
      }

      // Write the new changed pin state to the log file
      if(newData == 0) {
         Log_printf("0%c\n", id);
      } else if(newData == 1) {
         Log_printf("1%c\n", id);
      } else {
         Log_printf("x%c\n", id);
      }
      
      VAR(Log_data) = newData;
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
