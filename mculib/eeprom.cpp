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
#include "hexfile.h"

// Saved HINSTANCE argument from DllEntryPoint() function. Needed to register
// window classes and load resources from the DLL.
HINSTANCE DLL_instance;

int WINAPI DllEntryPoint(HINSTANCE pInstance, unsigned long, void*)
//*************************
// Entry point called when loading and unloading this DLL. Save the HINSTANCE
// so it can be later passed to Hexfile::init() from On_window_init().
{
   DLL_instance = pInstance;
   return 1;
}

// =============================================================================

// EEPROM programming mode in EEPMx bits
// NOTE: WORD8::get_field() returns -1 for unknown bits
enum { MODE_UNKNOWN = -1, MODE_ATOMIC, MODE_ERASE, MODE_WRITE, MODE_RESERVED };
const char *Mode_text[] = {
   "?", "Erase / Write", "Erase", "Write", "Reserved"
};

// EEPROM write/erase times corresponding to each of the MODE_xxx constants
double Delay_time[] = { 0, 3.4e-3, 1.8e-3, 1.8e-3, 0 };

// Action codes used with REMIND_ME() -> On_remind_me()
enum { RMD_AUTOCLEAR_EEMPE, RMD_AUTOCLEAR_EEPE };

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
   bool Simtime;        // True if the "Simulate write/erase time" is checked
   bool Autosave;       // True if the "Auto save" checkbox button is checked
   
   Hexfile Hex;         // Helper classs for GUI View/Load/Save/Erase
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

int Decode_address()
//*************************
// Decode and return the EEPROM memory address stored in EEAR registers. If
// the address is invalid then return -1 and issue the appropriate warning
// messages.
{
   // Read EEARL/EAARH pair as little-endian 16bit value
   WORD16LE *addr = (WORD16LE *) &REG(EEARL);
   
   if(!addr->known()) {
      WARNING("Unknown (X) bits in EEAR registers",
         CAT_EEPROM, WARN_EEPROM_ADDRES_OUTSIDE);
      return -1;
   } else if(addr->d() > VAR(Size)) {
      WARNING("Address in EEAR registers out of range",
         CAT_EEPROM, WARN_EEPROM_ADDRES_OUTSIDE);
      return -1;
   }
   
   return addr->d();
}

void Write_EEPROM()
//*************************
// Called from On_register_write() to handle all the details of writing the
// EEPROM memory depending on the current EEPM mode bits.
{
   // Treat UNKNOWN bits in EEDR as zero; they can't be saved to a HEX file
   UCHAR data = REG(EEDR).d() & REG(EEDR).x();
   
   int addr = Decode_address();
   if(addr != -1) {
      
      // Perform different action depending on EEPM mode bits
      switch(REG(EECR).get_field(5,4)) {

         case MODE_UNKNOWN:
            WARNING("Unknown EEPM value in EECR; EEPROM write ignored",
               CAT_EEPROM, WARN_MEMORY_WRITE_X_IO);         
            break;

         case MODE_RESERVED:
            WARNING("Reserved EEPM value in EECR; EEPROM write ignored",
               CAT_EEPROM, WARN_PARAM_RESERVED);
            break;
            
         case MODE_ATOMIC:
            Log("Write EEPROM[$%04X]=$%02X", addr, data);
            VAR(Memory)[addr] = data;
            break;
         
         case MODE_ERASE:
            Log("Erase EEPROM[$%04X]", addr);
            VAR(Memory)[addr] = 0xFF;
            break;
                  
         case MODE_WRITE:
            // A write only operation can change a 1 bit to a 0 in EEPROM or
            // can leave the bit set to the current value, but only erase or
            // atomic write can change a 0 bit back to a 1. If this condition
            // is violated, issue a warning and mask new data so existing 0
            // bits don't change to 1.
            if(data & ~VAR(Memory)[addr]) {
               WARNING("Cannot change EEPROM bit from 0 to 1 in write-only mode (EEPMx=10)",
                  CAT_EEPROM, WARN_MISC);
            }            
            data &= VAR(Memory)[addr];
            Log("Write EEPROM[$%04X]=$%02X", addr, data);
            VAR(Memory)[addr] = data;            
            break;         
      }
      
      // If VAR(Simtime) true then simulate the write/erase programming delay
      // by setting EEPE=1 to indicate EEPROM is busy and scheduling
      // REMIND_ME() to set EEPE=0 when the programming is finished. 
      if(VAR(Simtime)) {

         // The +1 is needed in case get_field() returns -1 becaue unknown (X)
         // bits are present. Unknown/reserved modes have a zero delay.
         double delay = Delay_time[REG(EECR).get_field(5,4) + 1];
         if(delay > 0) {
            REG(EECR).set_bit(1, 1);
            REMIND_ME(delay, RMD_AUTOCLEAR_EEPE);
         }
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
   
   // Initialize memory to $FF for display purposes in hex editor before the
   // simulation is started and actual data from .eep file is loaded.
   memset(VAR(Memory), 0xFF, VAR(Size));

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

void On_window_init(HWND pHandle)
//*******************************
// Window initialization. Set initial check box state and setup Hexfile class.
{
   // The "Auto Save" checkbox is initially enabled
   SendMessage(GET_HANDLE(GDT_AUTOSAVE), BM_SETCHECK, BST_CHECKED, 0);
   VAR(Autosave) = true;
   
   // Initialize Hexfile support class. For some reason VMLAB adds a space
   // in front of the title string for most child windows, so this peripheral
   // maintains the same convention. Icon resource 13005 in VMLAB.EXE is the
   // "E^2" icon used for the main EEPROM window.
   VAR(Hex).init(DLL_instance, pHandle, " EEPROM Memory", 13005);
   VAR(Hex).data(VAR(Memory), VAR(Size));
   
   // Hex editor remains read-only until simulation is started
   VAR(Hex).readonly(true);
}

// TODO: Need On_window_init() to set initial checkboxes

void On_simulation_begin()
//**********************
// Called at the beginning of simulation.
{
   // Ensure that EEPE=0 and no EEPROM erase/write is in progress. On_reset()
   // will initialize other bits and registers, but will preserve EEPE.
   REG(EECR) = 0;

   // VLMAB internally loads data from project's .eep file at the beginning
   // of the simulation. Copy this data to the local memory buffer so that it
   // can be used with an external hex editor control that expects an array
   // of bytes rather than an array of WORD8.
   // TODO: Use Autosave option
   for(int i = 0; i < VAR(Size); i++) {
      WORD8 *data = GET_MICRO_DATA(DATA_EEPROM, i);
      if(data == NULL) {
         BREAK("Internal error; GET_MICRO_DATA(DATA_EEPROM) returned NULL");
      }
      VAR(Memory)[i] = data->d();
   }
   
   // TODO: Refresh should be done from On_update_tick()
   VAR(Hex).refresh();

   // The hex editor can now be used to make changes to EEPROM data
   VAR(Hex).readonly(false);
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
   
   // Initialize memory back to $FF just as it was before simulation start
   memset(VAR(Memory), 0xFF, VAR(Size));

   // TODO: Refresh should be done from On_update_tick()
   VAR(Hex).refresh();

   // The hex editor is read only since On_simulation_begin() will reload data
   VAR(Hex).readonly(true);  
}

void On_register_write(REGISTER_ID pId, WORD8 pData)
//**********************
// The micro is writing pData into the pId register. This notification
// allows to perform all the derived operations.
{   
   UCHAR mask;     // Bitmask applied to register assignment
   
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
            WARNING("Cannot write EEAR registers while EEPROM busy (EEPE=1)",
               CAT_EEPROM, WARN_WRITE_BUSY);
         } else {
            REG(pId) = pData & mask;
         }
         
         break;
      
      // EEPROM data register
      //---------------------------------------------------------
      // No special processing required.
      // TODO: Can EEDR be modified while write/erase in progress?
      case EEDR:
         Log_register_write(EEDR, pData, 0xFF);
         REG(EEDR) = pData;
         break;
      
      // EEPROM control register
      //---------------------------------------------------------
      case EECR:
         mask = VAR(EECR_mask);
         Log_register_write(EECR, pData, mask);                  

         // Bits 5,4 - EEPMx: EEPROM Programming Mode Bits
         //---------------------------------------------------------
         // Mode change only allowed if EEPROM programming not already in
         // progress (i.e. EEPE must be 0).
         int newMode = pData.get_field(5,4);
         if(newMode != REG(EECR).get_field(5,4)) {
            if(REG(EECR)[1] == 1) {
               WARNING("Cannot change EEPM while EEPROM busy (EEPE=1)",
                  CAT_EEPROM, WARN_PARAM_BUSY);
            } else {
               if(newMode == MODE_RESERVED) {
                  WARNING("Reserved EEPM value written to EECR",
                     CAT_EEPROM, WARN_PARAM_RESERVED);
               }               
               
               // Assign individual bits to preserve positions of unknowns (X)
               REG(EECR).set_bit(4, pData[4]);
               REG(EECR).set_bit(5, pData[5]);               
               
               Log("Update mode: %s", Mode_text[newMode + 1]);
            }
         }

         // Bit 3 - EERIE: EEPROM Ready Interrupt Enable
         //---------------------------------------------------------
         REG(EECR).set_bit(3, pData[3]);
         SET_INTERRUPT_ENABLE(ERDY, pData[3] == 1);

         // Bit 2 - EEMPE: EEPROM Write/Erase Enable
         //---------------------------------------------------------
         // EEMPE can only be set if EEPE=0; if EEPE=1 in the register write
         // then alwahys force EEMPE=0. Can write EEMPE=0 or EEMPE=X any time.
         // TODO: If EEPE slready set, can EEMPE be set again?
         // TODO: If the 4 cycle delay due to EERE is emulated one day, then
         // the EEMPE bit will reset 4 cycles *after* the EERE delay.
         if(pData[2] == 1) {
            if(pData[1] == 1) {
               REG(EECR).set_bit(2, 0);
            } else {
               REG(EECR).set_bit(2, 1);
               REMIND_ME2(4, RMD_AUTOCLEAR_EEMPE);
            }
         } else {
            REG(EECR).set_bit(2, pData[2]);
         }
                  
         // Bit 1 - EEPE: EEPROM Write/Erase Enable
         //---------------------------------------------------------
         // If EEMPE=1 and EEPROM is not busy then perform selected write mode
         if(pData[1] == 1) {
            if(pData[2] == 1) {
               WARNING("Cannot set EEMPE=1 and EEPE=1 at the same time",
                  CAT_EEPROM, WARN_PARAM_BUSY);
            } else if(REG(EECR)[2] != 1) {
               WARNING("Cannot set EEPE=1 if EEMPE is not already set",
                  CAT_EEPROM, WARN_PARAM_BUSY);
            } else {
               Write_EEPROM();
            }
         }
         
         // Bit 0 - EERE: EEPROM Read Enable
         //---------------------------------------------------------
         // If EEPROM is not busy then copy byte from memory buffer to EEDR.
         if(pData[0] == 1) {
            if(pData[1] == 1) {
               WARNING("Cannot read (EERE=1) and write (EEPE=1) EEPROM at the same time",
                  CAT_EEPROM, WARN_EEPROM_SIMULTANEOUS_RW);
            } else if(REG(EECR)[1] == 1) {
               WARNING("Cannot read (EERE=1) while EEPROM busy (EEPE=1)",
                  CAT_EEPROM, WARN_EEPROM_SIMULTANEOUS_RW);
            } else {
               int addr = Decode_address();
               if(addr != -1) {
                  // TODO: Reading the EEPROM requires an additional 4 cycle
                  // CPU delay. This cannot be emulated using the current
                  // VMLAB API.
                  UCHAR data = VAR(Memory)[addr];
                  REG(EEDR) = data;
                  Log("Read EEPROM[$%04X]=$%02X", addr, data);
               }
            }
         }
         
         // ERDY interrupt is level triggered and always set if EEPE=0
         SET_INTERRUPT_FLAG(ERDY, pData[1] == 0 ? FLAG_SET : FLAG_CLEAR);
         
         break;
   }   
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
// Response to REMIND_ME() used implement the autoclearing of EEMPE after
// 4 cycles and the autoclearing of EEPE after write/erase cycle finishes.
{
   switch(pAux) {
   
      // Autoclear EEPE after erase/write cycle finished and set level-
      // triggered ERDY interrupt (only if simulating programming delay)
      case RMD_AUTOCLEAR_EEPE:
         REG(EECR).set_bit(1, 0);
         SET_INTERRUPT_FLAG(ERDY, FLAG_SET);
         break;
      
      // TODO: Add checks for multiple pending RMD_AUTOCLEAR_EEMPE
      case RMD_AUTOCLEAR_EEMPE:
         if(REG(EECR)[2] != 0) {
            WARNING("EEMPE cleared by hardware; previously set 4 cycles ago",
               CAT_EEPROM, WARN_MISC);
         }
         REG(EECR).set_bit(2, 0);
         break;
   }
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

// TODO: Need On_update_tick() and a "Mode:" display in the GUI

void On_gadget_notify(GADGET pGadget, int pCode)
//*********************************************
// Response to Win32 notification coming from buttons, etc.
{
   if(pGadget == GDT_LOG && pCode == BN_CLICKED) {
      VAR(Log) ^= 1;
   }
   if(pGadget == GDT_SIMTIME && pCode == BN_CLICKED) {
      VAR(Simtime) ^= 1;
   }
   if(pGadget == GDT_AUTOSAVE && pCode == BN_CLICKED) {
      VAR(Autosave) ^= 1;
   }
   if(pGadget == GDT_VIEW && pCode == BN_CLICKED) {
      VAR(Hex).show();
   }
}
