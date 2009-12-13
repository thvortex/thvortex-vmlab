// =============================================================================
// Common include file for all AVR peripherals
//
// Version History:
// v0.1 09/15/09 - Initial release as part of TIMER0 model
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

#include <stdio.h>
#include <stdarg.h>

// Size of temporary string buffer for generating filenames and error messages
#define MAXBUF 256

// Return the number of elements in a statically defined array
#define countof(x) (sizeof(x) / sizeof(*x))

// On_notify() codes; shared by all peripheral modules. Be careful about inserting/removing
// enums since the remaining ones are renumbered and this can cause compatibility issues
// when some DLL files are not recompiled.
enum {NTF_PRR0, NTF_PRR1, NTF_TSM, NTF_PSR, NTF_WDR};

char *hex(const WORD8 &pData)
//*******************
// Return a hex string representation of a WORD8 value. If the WORD8 contains any unknown
// bits then return "$??". Because this function uses a static buffer, the returned string
// is only valid until the next call to this function.
{
   static char strBuffer[8];

   if(pData.known())
      snprintf(strBuffer, 8, "$%02X", pData.d());
   else
      snprintf(strBuffer, 8, "$??");
   
   return strBuffer;
}

char *hex(const WORD16 &pData)
//*******************
// Return a hex string representation of a word16 value. if the word16 contains any unknown
// bits then return "$????". because this function uses a static buffer, the returned string
// is only valid until the next call to this function.
{
   static char strBuffer[8];

   if(pData.known())
      snprintf(strBuffer, 8, "$%04X", pData.d());
   else
      snprintf(strBuffer, 8, "$????");
   
   return strBuffer;
}

char *reg(int pId)
//*************************
// Given a register ID, return the "true" register name that was specified in
// the .ini file. Because this function uses a static buffer, the returned
// string is only valid until the next call to this function.
{
   static char strBuffer[16]; // Register name returned by this function

   int gadget;    // The register's gadget ID
   WORD8 *w;      // Dummy argument needed for GetRegisterInfo() call 
   const char *s; // Dummy argument needed for GetRegisterInfo() call
   
   // Call the hidden GetRegisterInfo() function that was created by the
   // REGISTERS_VIEW block to retrieve the register's gadget ID.
   gadget = GetRegisterInfo(pId, &w, &s, &s, &s, &s, &s, &s, &s, &s);   
   if(gadget == -1) {
      return "?";
   }
   
   // True name was assigned to the static label associated with register.
   // Since GET_HANDLE() only allows GADGET0 through GADGET31, we have to
   // use a more indirect way to get static label handle.
   HWND parentHandle = GetParent(GET_HANDLE(gadget));
   HWND labelHandle = GetDlgItem(parentHandle, gadget + 100);
   GetWindowText(labelHandle, strBuffer, 16);
   
   return strBuffer;
}

void SetWindowTextf(HWND pHandle, const char *pFormat, ...)
//*************************
// Wrapper around SetWindowText() that provides printf() like functionality
{
   char strBuffer[MAXBUF];
   va_list argList;

   va_start(argList, pFormat);
   vsnprintf(strBuffer, MAXBUF, pFormat, argList);
   va_end(argList);

   SetWindowText(pHandle, strBuffer);
}
