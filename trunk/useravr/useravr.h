#include <stdio.h>

// TODO: Need new warning categories for port conflicts

// Wrapper around TAKEOVER_PORT() function to provide common error handling. If an error occurs, the
// port name is embedded into the warning message.
#define TAKEOVER_PORT(pPort, pTakeIt, pForceOutput) { \
      int rc = TAKEOVER_PORT(pPort, pTakeIt, pForceOutput); \
      if (rc == PORT_NOT_OUTPUT) { \
         WARNING("Pin "#pPort" is not defined as output", CAT_TIMER, WARN_TIMERS_OUTPUT); \
      } else if (rc == PORT_NOT_OWNER) { \
         WARNING("Pin "#pPort" already in use by another peripheral", CAT_TIMER, WARN_TIMERS_OUTPUT); \
      } \
   }

// Wrapper around SET_PORT() function to provide common error handling. If an error occurs, the
// port name is embedded into the warning message.
#define SET_PORT(pPort, pValue) { \
      int rc = SET_PORT(pPort, pValue); \
      if (rc == PORT_NOT_OUTPUT) { \
         WARNING("Pin "#pPort" is not defined as output", CAT_TIMER, WARN_TIMERS_OUTPUT); \
      } else if (rc == PORT_NOT_OWNER) { \
         WARNING("Pin "#pPort" already in use by another peripheral", CAT_TIMER, WARN_TIMERS_OUTPUT); \
      } \
   }

// On_notify() codes; shared by all peripheral modules. Be careful about inserting/removing
// enums since the remaining ones are renumbered and this can cause compatibility issues
// when some DLL files are not recompiled.
enum {NTF_PRR0, NTF_PRR1, NTF_TSM, NTF_PSR, NTF_WDR};

// Return a hex string representation of a WORD8 value. If the WORD8 contains any unknown
// bits then return "$??". Because this function uses a static buffer, the returned string
// is only valid until the next call to this function.
char *hex(const WORD8 &pData)
{
   static char strBuffer[8];

   if(pData.x == 0xFF)
      snprintf(strBuffer, 8, "$%02X", pData.d);
   else
      snprintf(strBuffer, 8, "$??");
   
   return strBuffer;
}
