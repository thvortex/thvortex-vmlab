#include <stdio.h>
#include <stdarg.h>

// Size of temporary string buffer for generating filenames and error messages
#define MAXBUF 256

// On_notify() codes; shared by all peripheral modules. Be careful about inserting/removing
// enums since the remaining ones are renumbered and this can cause compatibility issues
// when some DLL files are not recompiled.
//
enum {NTF_PRR0, NTF_PRR1, NTF_TSM, NTF_PSR, NTF_WDR};

char *hex(const WORD8 &pData)
//*******************
// Return a hex string representation of a WORD8 value. If the WORD8 contains any unknown
// bits then return "$??". Because this function uses a static buffer, the returned string
// is only valid until the next call to this function.
{
   static char strBuffer[8];

   if(pData.x == 0xFF)
      snprintf(strBuffer, 8, "$%02X", pData.d);
   else
      snprintf(strBuffer, 8, "$??");
   
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
