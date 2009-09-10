// =============================================================================
// C++ code template for a VMLAB user defined component
// Target file must be Windows .DLL (no .EXE !!)
//
// Component name: wavstim v1.0
//
// To use this component, use the following component definition:
//
// X<Name> _wavstim <Data>
//
// The <Name> is used to form part of the input filename in the format
// "<Name>.wav". The sample rate of the input file determines the rate at which
// the analog output pin <Data> gets updated. Voltage levels of VSS, VDD, and
// (VDD-VSS)/2 on the <Data> pin correspond respectively to the maximum
// negative, maximum positive, and zero sample values within the WAV file. If
// the WAV file contains multiple channels, only the first (left) channel will
// be used.
//
// Version History:
// v1.0 11/29/08 - Initial public release
//
// Written by Wojciech Stryjewski, 2008
//
// This work is hereby released into the Public Domain. To view a copy of the
// public domain dedication, visit http://creativecommons.org/licenses/publicdomain/
// or send a letter to Creative Commons, 171 Second Street, Suite 300,
// San Francisco, California, 94105, USA.
//
// This component depends on http://www.mega-nerd.com/libsndfile/ which is
// licensed under the GNU LGPL either version 2.1 or 3.
//
#include <windows.h>
#include <commctrl.h>
#include <stdlib.h>
#pragma hdrstop
#include "C:\VMLAB\bin\blackbox.h"
#include "sndfile.h"
int WINAPI DllEntryPoint(HINSTANCE, unsigned long, void*) {return 1;} // is DLL
//==============================================================================

// Size of temporary string buffer for generating filenames and error messages
#define MAXBUF 256

//==============================================================================
// Declare pins here
//
DECLARE_PINS
   ANALOG_OUT(DATA, 1);
END_PINS

// =============================================================================
// Declare module global variables here.
// The reason for this is to keep a set of variables by instance
// To use a variable, do it through the the macro VAR(...)
//
DECLARE_VAR
   SNDFILE *File;          // The wav file open for reading
   SF_INFO File_info;      // Sample reate/bit width/etc used by libsndfile
   double *Sample_buffer;  // Passed to sf_readf_double() in libsndfile
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
// Close the wav file, and check for any I/O errors that can occur when
// closing the file.
{
   char strBuffer[MAXBUF];

   // Do nothing if the wav file is already closed
   if(!VAR(File)) {
      return;
   }

   // Close the wav file so it can be moved or deleted
   if(sf_close(VAR(File))) {
      snprintf(strBuffer, MAXBUF, "Error closing \"%s.wav\" file: %s",
         GET_INSTANCE(), sf_strerror(VAR(File)));
      BREAK(strBuffer);         
   }
   
   VAR(File) = NULL;
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
// Although the wav filename doesn't change with each simulation, it's better to
// open it here and close it in On_simulation_end(). This allows the user to
// delete or move the log file without having to close or rebuild the project.
{
   char strBuffer[MAXBUF];

   // Open existing wav file in current directory. The libsndfile API requires
   // the SF_INFO.format field set to 0 before opening a file for read access.
   VAR(File_info).format = 0;
   snprintf(strBuffer, MAXBUF, "%s.wav", GET_INSTANCE());
   VAR(File) = sf_open(strBuffer, SFM_READ, &VAR(File_info));

   // We can still run if the file won't open; output stays at POWER()/2.
   if(!VAR(File)) {
      snprintf(strBuffer, MAXBUF, "Could not open \"%s.wav\" file: %s",
         GET_INSTANCE(), sf_strerror(VAR(File)));
      BREAK(strBuffer);
      return;
   }
   
   // If the WAV file contains more than one channel, print an error mssage
   // that the additional channels will be ignored.
   if(VAR(File_info).channels != 1) {
      snprintf(strBuffer, MAXBUF, "File \"%s.wav\" has multiple channels; "
         "only first (left) channel used", GET_INSTANCE());
      PRINT(strBuffer);
   }
   
   // Allocate buffer large enough to hold a single sample across all the
   // channels. The libsndfile library requires that all the channels are read
   // at once, and it provides no way to ignore unwanted channels.
   VAR(Sample_buffer) = (double *)
      malloc(sizeof(double) * VAR(File_info).channels);
   if(!VAR(Sample_buffer)) {
      BREAK("Error allocating memory buffer");
      Close_file();
      return;
   }
}

void On_simulation_end()
//**********************
// Undo here the operations done at On_simulation_begin: free memory, close
// files, etc.
{
   free(VAR(Sample_buffer));
   Close_file();
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
// no changes. Use pin identifers as declared in DECLARE_PINS.
{
   // This component produces a step-like analog output similar to a real
   // unfiltered DAC, therefore we don't use the On_voltage_ask() function.
   return KEEP_VOLTAGE;
}

void On_time_step(double pTime)
//*****************************
// The analysis at the given time has finished. DO NOT place further actions
// on pins (unless they are delayed). Pins values are stable at this point.
{
   // Set the initial voltage level at time 0 and schedule recurring output
   if(pTime == 0) {
      On_remind_me(0, 0);
   }
}

void On_remind_me(double pTime, int pData)
//***************************************
// VMLAB notifies about a previouly sent REMIND_ME() function.
// Read the next voltage sample from the WAV file, set the analog voltage on the
// output pin, and schedule another output update based on the sampling rate of
// the input wav file.
{
   char strBuffer[MAXBUF];

   // Do nothing if the wav file is closed due to an error
   if(!VAR(File)) {
      return;
   }
   
   // Read the next voltage sample from the wav file. If an error occurs or
   // EOF is reached, then the wav file is closed, the output voltage remains
   // set to the previous value and no more output updates are scheduled.
   if(sf_readf_double(VAR(File), VAR(Sample_buffer), 1) != 1) {
      // Break with an error message if an actual I/O or decode error occurred
      if(sf_error(VAR(File))) {
         snprintf(strBuffer, MAXBUF, "Error reading \"%s.wav\" file: %s",
            GET_INSTANCE(), sf_strerror(VAR(File)));
         BREAK(strBuffer);         
      }
      
      // Close the file on either an error or a normal end-of-file
      Close_file();
      return;
   }

   // The voltage in VMLAB ranges from 0 to POWER(), while the sample that
   // libsndfile returns ranges from -1 to +1. Only the sample from the
   // first (left) channel is used here. Samples from additional channels
   // are simply discarded.
   SET_VOLTAGE(DATA, (*VAR(Sample_buffer) + 1) * 0.5 * POWER());

   // Schedule the next On_remind_me() based on the sampling rate
   REMIND_ME(1.0 / VAR(File_info).samplerate);
}

void On_gadget_notify(GADGET pGadgetId, int pCode)
//************************************************
// A window gadget (control) is sending a notification.
{
   // No Action
}
