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
// Return a hex string representation of a WORD16 value. If the WORD16 contains any unknown
// bits then return "$????". Because this function uses a static buffer, the returned string
// is only valid until the next call to this function.
{
   static char strBuffer[8];

   if(pData.known())
      snprintf(strBuffer, 8, "$%04X", pData.d());
   else
      snprintf(strBuffer, 8, "$????");
   
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
