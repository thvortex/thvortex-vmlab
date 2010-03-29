// =============================================================================
// Component Name: AVR EEPROM peripheral
//
// Copyright (C) 2010 Wojciech Stryjewski <thvortex@gmail.com>
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
// INI file "Version" usage:
//
// The version is a hexadecimal integer defined as: 0xhhll000v
//
// The "hh" and "ll" are bitmaks for valid bits in EEARH and EEARL. The "v"
// is 1, 2, or 3 to specify the version of the EECR register. Version 3 is
// ATmega168 like with split erase/write support, Version 2 is ATmega8 like,
// and Version 1 is ATtiny22 like (no ERDY interrupt). Version defaults to 0
// if omitted from INI file; this will cause an error due to missing DISPLAY()
// for EECR.

#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#pragma hdrstop

#define IS_PERIPHERAL       // To distinguish from a normal user component
#include "blackbox.h"
#include "useravr.h"
#include "eeprom.h"

int WINAPI DllEntryPoint(HINSTANCE, unsigned long, void*) {return 1;} // is DLL
// =============================================================================

// Involved ports. Keep same order as in .INI file "Port_map = ..." who
// does the actual assignment to micro ports PD0, etc. This allows multiple instances
// to be mapped into different port sets
//
DECLARE_PINS
END_PINS

// Involved registers. Use same order as in .INI file "Register_map"
// These are IDs or indexes. The real data is stored in a hidden WORD8
// type array. (see "blackbox.h" for details). The WORD8 data is referred
// with the REG(xxx) macro.
//
DECLARE_REGISTERS
   EEARL, EEARH, EEDR, EECR
END_REGISTERS

// Involved interrupts. Use same order as in .INI file "Interrupt_map"
// Like in  registers, these are IDs; what is important is the order.
// It is "Interupt_map" who assigns the index into the actual interrupt name, 
// therefore, different instances can use a different interrupt set
//
DECLARE_INTERRUPTS
   ERDY
END_INTERRUPTS 

// =============================================================================
// Declare variables here to allow multiple instances to be placed
//
DECLARE_VAR
   int Size;            // Total EEPROM memory size from ini file
   UCHAR *Memory;       // Local copy of EEPROM data

   UCHAR EEARH_mask;    // Valid bits in EEARH (Version high byte)
   UCHAR EEARL_mask;    // Valid bits in EEARL (Version 2nd highest byte)
   UCHAR EECR_mask;     // Valid bits mask in EECR (Version low byte)
   UINT Version;        // Lowest byte of VERSION() that defines EECR

   bool Log;            // True if the "Log" checkbox button is checked
   bool Delay;          // True if the "Simulate write/erase time" is checked
END_VAR

USE_WINDOW(WINDOW_USER_1); // Window to display registers, etc. See .RC file

REGISTERS_VIEW
   // It may not be safe yet to use VAR(Version) so use local variable here
   int version = VERSION() & 0xFF;

//           ID     .RC ID    substitute "*" by bit names: bi7name, ..., b0
   DISPLAY(EEARH, GDT_EEARH, *, *, *, *, *, *, *, *)
   DISPLAY(EEARL, GDT_EEARL, *, *, *, *, *, *, *, *)
   DISPLAY(EEDR, GDT_EEDR, *, *, *, *, *, *, *, *)
   if(version == 3) {
      DISPLAY(EECR, GDT_EECR, *, *, EEPM1, EEPM0, EERIE, EEMPE, EEPE, EERE)
   } else if(version == 2) {
      DISPLAY(EECR, GDT_EECR, *, *, *, *, EERIE, EEMWE, EEWE, EERE)
   } else if(version == 1) {
      DISPLAY(EECR, GDT_EECR, *, *, *, *, *, EEMWE, EEWE, EEWE)
   }
END_VIEW

// =============================================================================
// Helper Functions

void Log(const char *pFormat, ...)
//*************************
// Wrapper around the PRINT() function to provide printf() like functionality
// This function also automatically prepends the instance name to the formatted
// message. The message is only printed if logging is enabled in GUI.
{
   if(VAR(Log)) {   
      char strBuffer[MAXBUF];
      va_list argList;

      snprintf(strBuffer, MAXBUF, "%s: ", GET_INSTANCE());
      int len = strlen(strBuffer);

      va_start(argList, pFormat);
      vsnprintf(strBuffer + len, MAXBUF - len, pFormat, argList);
      va_end(argList);

      PRINT(strBuffer);
   }
}

void Log_register_write(int pId, WORD8 pData, unsigned char pMask)
//*************************
// Called from every 'case XXX:' statement when handling On_register_write().
// Parameter pMask is a bitmask of useful (i.e. not reserved) bits in the
// register. If the pData value being written to the register contains any
// unknown bits in the pMask position then issue a warning. In either case
// Log() a message indicating the register is being written with pData. The
// pId is used to look up the true register name (as given in the .ini file)
// for the warning and log messages.
{
   if((pData.x() & pMask) != pMask) {      
      char strBuffer[64];
      char *strName = reg(pId);      
      
      snprintf(strBuffer, 64, "Unknown bits (X) written into %s register",
         strName);
         
      WARNING(strBuffer, CAT_MEMORY, WARN_MEMORY_WRITE_X_IO);
      Log("Write register %s: $??", strName);      
   } else {
      if(VAR(Log)) { // Don't waste time in reg() if not logging
         char *strName = reg(pId);
         Log("Write register %s: $%02X", strName, pData.d() & pMask);
      }
   }
}

// =============================================================================
// Callback functions. These functions are called by VMLAB at the proper time

const char *On_create()
//**********************
// Called after building project file. Allocates 
{
   // Lookup the total EEPROM memory size as defined in the ini file
   VAR(Size) = GET_MICRO_INFO(INFO_EEPROM_SIZE);
   if(VAR(Size) < 0) {
      return "Internal error in GET_MICRO_INFO(INFO_EEPROM_SIZE)";
   }
   if(VAR(Size) == 0) {
      return "EEPROM peripheral cannot be used if EEPROM size is zero";
   }

   // Allocate enough memory to store all of the EEPROM contents
   VAR(Memory) = (UCHAR *) malloc(VAR(Size));
   if(VAR(Memory) == NULL) {
      return "Could not allocate memory";
   }

   // Initialize bitmasks for valid register bits based on decoded "Version"
   int version = VERSION();
   VAR(EEARH_mask) = (version >> 24) & 0xFF;
   VAR(EEARL_mask) = (version >> 16) & 0xFF;
   
   // Bitmask for EECR corresponds to the bits defined in REGISTERS_VIEW
   VAR(Version) = VERSION() & 0xFF;
   if(VAR(Version) == 3) {
      VAR(EECR_mask) = 0x3F; // EEPM1, EEPM0, EERIE, EEMPE, EEPE, EERE
   } else if(VAR(Version) == 2) {
      VAR(EECR_mask) = 0x0F; // EERIE, EEMWE, EEWE, EERE
   } else if(VAR(Version) == 1) {
      VAR(EECR_mask) = 0x07; // EEMWE, EEWE, EEWE
   } else {
      VAR(EECR_mask) = 0;
   }
   
   return NULL;
}

void On_destroy()
//**********************
// Called when closing project file and/or unloading components. Free memory
// buffer that was previously allocated inside On_create()
{
   // Deallocate EEPROM contents memory previously allocated in On_create()
   if(VAR(Memory)) {
      free(VAR(Memory));
   }
}

void On_simulation_begin()
//**********************
// Called at the beginning of simulation.
{
   // Ensure that EEPE=0 and no EEPROM erase/write is in progress. On_reset()
   // will initialize other bits and registers, but will preserve EEPE.
   REG(EECR) = 0;
   
   // VLMAB internally loads data from project's .eep file at the beginning
   // of the simulation. Copy this data to the local memory buffer so that it
   // can be used with an external hex editor control.
   for(int i = 0; i < VAR(Size); i++) {
      WORD8 *data = GET_MICRO_DATA(DATA_EEPROM, i);
      if(data == NULL) {
         BREAK("Internal error; GET_MICRO_DATA(DATA_EEPROM) returned NULL");
      }
      VAR(Memory)[i] = data->d();
   }
}

void On_simulation_end()
//**********************
// If the simulation is ending, set all registers to unknown.
{
   FOREACH_REGISTER(i){
      REG(i) = WORD8(0,0);
   }

   // Copy the local memory buffer back to VMLAB so that any modifications can
   // be automatically saved back to the project's .eep file.
   // TODO: This does not appear to be supported by VMLAB at the moment.
   // TODO: Check "Auto save" option before doing this
   for(int i = 0; i < VAR(Size); i++) {
      WORD8 *data = GET_MICRO_DATA(DATA_EEPROM, i);
      if(data == NULL) {
         BREAK("Internal error; GET_MICRO_DATA(DATA_EEPROM) returned NULL");
      }
      *data = VAR(Memory)[i];
   }
}

void On_register_write(REGISTER_ID pId, WORD8 pData)
//**********************
// The micro is writing pData into the pId register. This notification
// allows to perform all the derived operations.
{
   // Bitmask applied to register assignment (all bits R/W by default)
   UCHAR mask;
   
   switch(pId) {
         
      // EEPROM high/low address byte
      //---------------------------------------------------------
      // Allow changing only those bits defined by VERSION() masks. If
      // EECR{EEPE]=1 (EEPROM programming in progress) then don't allow
      // the address registers to be modified.
      case EEARH:
      case EEARL:
         mask = (pId == EEARH) ? VAR(EEARH_mask) : VAR(EEARL_mask);      
         Log_register_write(pId, pData, mask);
         
         if(REG(EECR)[1] == 1) {
            WARNING("Cannot write EEAR registers while EEPE=1 (EEPROM busy)",
               CAT_EEPROM, WARN_WRITE_BUSY);
            mask = 0; // Inhibit register assignment
         }
         
         break;
      
      // EEPROM data register
      //---------------------------------------------------------
      // No special processing required.
      // TODO: Can EEDR be modified while write/erase in progress?
      case EEDR:
         mask = 0xFF;
         Log_register_write(EEDR, pData, mask);
         break;
      
      // EEPROM control register
      //---------------------------------------------------------
      case EECR:
         mask = VAR(EECR_mask);
         Log_register_write(EECR, pData, mask);
         mask &= 0xFE; // EERE is a strobe bit and always reads 0 in register
         
         // Bit 0 - EERE: EEPROM Read Enable
         //---------------------------------------------------------
         // If EEPROM is not busy then copy byte from memory buffer to EEDR.
         // TODO: What happens if EERE=1 and EEPE=1 written at the same time?
         // Does one take precedence over the other?         
         if(pData[0] == 1) {
            if(pData[1] == 1) {
               WARNING("Cannot read and write/erase EEPROM at the same time",
                  CAT_EEPROM, WARN_EEPROM_SIMULTANEOUS_RW);
            } else if(REG(EECR)[1] == 1) {
               WARNING("Cannot read EEPROM if write/erase already in progress",
                  CAT_EEPROM, WARN_EEPROM_SIMULTANEOUS_RW);
            } else {

               // Read EEARL/EAARH pair as little-endian 16bit value
               WORD16LE *addr = (WORD16LE *) &REG(EEARL);
               if(!addr->known()) {
                  WARNING("Unknown bits in EEAR registers",
                     CAT_EEPROM, WARN_EEPROM_ADDRES_OUTSIDE);
               } else if(addr->d() > VAR(Size)) {
                  WARNING("Address in EEAR registers out of range",
                     CAT_EEPROM, WARN_EEPROM_ADDRES_OUTSIDE);
               } else {
                  UCHAR data = VAR(Memory)[addr->d()];
                  REG(EEDR) = data;
                  Log("Read EEPROM[$%04X]=$%02X", addr->d(), data);
               }
            }
         }
         
         break;
   }
   
   REG(pId) = pData & mask;
}

void On_reset(int pCause)
//***********************
// Initialize registers to the desired value.
{
   // Initialize all registers to known zero state. If the EECR[EEPE]
   // bit is set (EEPROM write/erase still in progress) then preserve the
   // contents of the EEPE, EEPM1, and EEPM0 bits across reset.
   WORD8 eecr = REG(EECR);
   FOREACH_REGISTER(i){
      REG(i) = 0;
   }
   if(eecr[1] == 1) {
      REG(EECR) = eecr & 0x32;
   }

   // Other than for Version 1 (ATtiny22 like), the implemented bits in the
   // EEARH/EEARL registers are set to unknown state after reset.
   if(VAR(Version) != 1) {
      REG(EEARH) = WORD8(~VAR(EEARH_mask), 0);
      REG(EEARL) = WORD8(~VAR(EEARL_mask), 0);
   }
}

void On_remind_me(double pTime, int pAux)
//***************************************
{
}

void On_notify(int pWhat)
//***********************
// Notification coming from some other DLL instance.
{
}

void On_sleep(int pMode)
//*********************
// The micro has entered in SLEEP mode.
{
   // TODO: Verify how EEPROM is affected by sleep modes? What if SLEEP
   // entered while EEPROM is actively programming
}

void On_gadget_notify(GADGET pGadget, int pCode)
//*********************************************
// Response to Win32 notification coming from buttons, etc.
{
   if(pGadget == GDT_LOG && pCode == BN_CLICKED) {
      VAR(Log) ^= 1;
   }
   if(pGadget == GDT_SIMTIME && pCode == BN_CLICKED) {
      VAR(Delay) ^= 1;
   }
}

void On_interrupt_start(INTERRUPT_ID pId)
//***********************************
// Called when MCU begins to execute ERDY interrupt
{
}