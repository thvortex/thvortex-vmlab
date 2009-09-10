// =============================================================================
// C++ code template for a VMLAB user defined component
// Target file must be Windows .DLL (no .EXE !!)
//
// Component name: wavlog v1.0
//
// To use this component, use the following component definition:
//
// X<Name> _wavlog(<SampleRate> <BitWidth>) <Data>
//
// The <Name> is used to form part of the output filename in the format
// "<Name>.wav". The <SampleRate> is specified in Hz (e.g. "48K") and has no
// upper limit, although not all audio programs may open WAV files with non
// standard rates. The <BitWidth> must be 8, 16, 24, or 32. Voltage levels of
// VSS, VDD, and (VDD-VSS)/2 on the <Data> pin correspond to the maximum
// negative, maximum positive, and zero sample values within the WAV file.
// Files created by the wavlog component will always be single channel.
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
   ANALOG_IN(DATA, 1);
END_PINS

// =============================================================================
// Declare module global variables here.
// The reason for this is to keep a set of variables by instance
// To use a variable, do it through the the macro VAR(...)
//
DECLARE_VAR
   SNDFILE *File;       // The wav file open for writing
   SF_INFO File_info;   // Sample reate/bit width/etc used by libsndfile
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
// flushing the remaining output.
{
   char strBuffer[MAXBUF];

   // Do nothing if the wav file is already closed
   if(!VAR(File)) {
      return;
   }

   // Close the wav file so it can be moved or deleted
   if(sf_close(VAR(File))) {
      snprintf(strBuffer, MAXBUF, "Error closing/flushing \"%s.wav\" file: %s",
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
   // The first required parameter is the sample rate in Hz. Note this is
   // rounded to the nearest integer since that's what libsndfile expects.
   double sampleRate = GET_PARAM(1);
   if (sampleRate <= 0) {
      return "Missing/invalid sample rate (in Hz) first parameter";
   }
   VAR(File_info).samplerate = sampleRate + 0.5;
   
   // The second required parameter is the bit width. This must be converted
   // into a set of bit flags defined in sndfile.h..
   int bitWidth = GET_PARAM(2);
   switch(bitWidth) {
      case 8:
         VAR(File_info.format) = SF_FORMAT_WAV | SF_FORMAT_PCM_U8;
         break;
         
      case 16:
         VAR(File_info.format) = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
         break;
         
      case 24:
         VAR(File_info.format) = SF_FORMAT_WAV | SF_FORMAT_PCM_24;
         break;
         
      case 32:
         VAR(File_info.format) = SF_FORMAT_WAV | SF_FORMAT_PCM_32;
         break;
         
      default:
         return "Missing/invalid bit width second parameter; "
            "only 8, 16, 24 and 32 supported";
   }
   
   // This component always creates mono wav files
   VAR(File_info.channels) = 1;
   
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

   // Create or overwrite wav file in current directory
   snprintf(strBuffer, MAXBUF, "%s.wav", GET_INSTANCE());
   VAR(File) = sf_open(strBuffer, SFM_WRITE, &VAR(File_info));

   // We can still run if the file won't open; we just can't log anything
   if(!VAR(File)) {
      snprintf(strBuffer, MAXBUF, "Could not create \"%s.wav\" file: %s",
         GET_INSTANCE(), sf_strerror(VAR(File)));
      BREAK(strBuffer);
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
   // Log the initial voltage level at time 0 and schedule recurring samples
   if(pTime == 0) {
      On_remind_me(0, 0);
   }
}

void On_remind_me(double pTime, int pData)
//***************************************
// VMLAB notifies about a previouly sent REMIND_ME() function.
// Sample the analog voltage on the input pin, write the voltage sample to
// the WAV file, and schedule another sampling time based on the sampling
// rate of this component.
{
   char strBuffer[MAXBUF];

   // Do nothing if the wav file is closed due to an error
   if(!VAR(File)) {
      return;
   }

   // The voltage in VMLAB ranges from 0 to POWER(), while the sample that
   // libsndfile expects ranges from -1 to +1. Conveniantly, UNKNOWN logic
   // values are reported as POWER()/2 which will be written as 0 in the
   // WAV file.
   double sample = GET_VOLTAGE(DATA) * 2 / POWER() - 1;
   
   // Write the sample to the output WAV file. If any I/O error occurs, break
   // with an error message and close the file so no further logging is
   // performed and the user isn't flooded with error messages.
   if(sf_write_double(VAR(File), &sample, 1) != 1) {
      snprintf(strBuffer, MAXBUF, "Could not write \"%s.wav\" file: %s",
         GET_INSTANCE(), sf_strerror(VAR(File)));
      BREAK(strBuffer);
      Close_file();
   }
   
   // Schedule the next On_remind_me() based on the sampling rate
   REMIND_ME(1.0 / VAR(File_info).samplerate);
}

void On_gadget_notify(GADGET pGadgetId, int pCode)
//************************************************
// A window gadget (control) is sending a notification.
{
   // No Action
}
