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
// Is 1, 2, or 3 to specify the version of the EECR register. Version 3 is
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

// EEPROM programming mode in EEPMx bits and EEPE status bit. Note that
// WORD8::get_field() returns -1 for unknown bits while WORD8::get_bit()
// (or the [] operator) returns 2 for unknown bits.
enum { MODE_UNKNOWN = -1, MODE_ATOMIC, MODE_ERASE, MODE_WRITE, MODE_RESERVED };
const char *Mode_text[] = {
   "?", "Erase / Write", "Erase", "Write", "Reserved"
};
const char *Status_text[] = {
   "Idle", "Busy", "?"
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
   bool Dirty;          // True if mode bits changed or EEPROM memory updated
   bool Sleep;          // True if ERDY interrupt disabled by SLEEP mode
   
   Hexfile Hex;         // Helper classs for GUI View/Load/Save/Erase
END_VAR

USE_WINDOW(WINDOW_USER_1); // Window to display registers, etc. See .RC file

REGISTERS_VIEW
   // It may not be safe yet to use VAR(Version) so use local variable here
   int version = VERSION();

//           ID     .RC ID    substitute "*" by bit names: bi7name, ..., b0
   DISPLAY(EEARH, GDT_EEARH, *, *, *, *, *, *, *, *)
   DISPLAY(EEARL, GDT_EEARL, *, *, *, *, *, *, *, *)
   DISPLAY(EEDR, GDT_EEDR, *, *, *, *, *, *, *, *)
   if(version == 3) {
      DISPLAY(EECR, GDT_EECR, *, *, EEPM1, EEPM0, EERIE, EEMPE, EEPE, EERE)
   } else if(version == 2) {
      DISPLAY(EECR, GDT_EECR, *, *, *, *, EERIE, EEMWE, EEWE, EERE)
   } else if(version == 1) {
      DISPLAY(EECR, GDT_EECR, *, *, *, *, *, EEMWE, EEWE, EERE)
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
      // It should not be possible for this error to occur since assignments
      // to the EEARx registers are masked based on the EEPROM size.
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
            VAR(Dirty) = true;
            break;
         
         case MODE_ERASE:
            Log("Erase EEPROM[$%04X]", addr);
            VAR(Memory)[addr] = 0xFF;
            VAR(Dirty) = true;
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
            VAR(Dirty) = true;
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

   // Initialize bitmasks for valid register bits based on EEPROM size. This
   // assumes the EEPROM size is a power of 2.
   UINT mask = VAR(Size) - 1;
   VAR(EEARH_mask) = mask >> 8;
   VAR(EEARL_mask) = mask & 0xFF;
   
   // Bitmask for EECR corresponds to the bits defined in REGISTERS_VIEW
   VAR(Version) = VERSION();
   if(VAR(Version) == 3) {
      VAR(EECR_mask) = 0x3F; // EEPM1, EEPM0, EERIE, EEMPE, EEPE, EERE
   } else if(VAR(Version) == 2) {
      VAR(EECR_mask) = 0x0F; // EERIE, EEMWE, EEWE, EERE
   } else if(VAR(Version) == 1) {
      VAR(EECR_mask) = 0x07; // EEMWE, EEWE, EERE
   } else {
      return "Version in INI file must be 1, 2, or 3";
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
   for(int i = 0; i < VAR(Size); i++) {
      WORD8 *data = GET_MICRO_DATA(DATA_EEPROM, i);
      if(data == NULL) {
         BREAK("Internal error; GET_MICRO_DATA(DATA_EEPROM) returned NULL");
      }
      VAR(Memory)[i] = data->d();
   }
   
   // Force the GUI to display new EEPROM contents and mode bits
   VAR(Dirty) = true;

   // The hex editor can now be used to make changes to EEPROM data and the
   // Load/Save/Erase buttons are enabled.
   VAR(Hex).readonly(false);
   EnableWindow(GET_HANDLE(GDT_LOAD), true);
   EnableWindow(GET_HANDLE(GDT_SAVE), true);
   EnableWindow(GET_HANDLE(GDT_ERASE), true);
}

void On_simulation_end()
//**********************
// If the simulation is ending, set all registers to unknown.
{
   FOREACH_REGISTER(i){
      REG(i) = WORD8(0,0);
   }

   // If "Auto save" is checked, then copy the local memory buffer back to
   // VMLAB so that any modifications can be automatically saved back to the
   // project's .eep file.
   if(VAR(Autosave)) {
      for(int i = 0; i < VAR(Size); i++) {
         WORD8 *data = GET_MICRO_DATA(DATA_EEPROM, i);
         if(data == NULL) {
            BREAK("Internal error; GET_MICRO_DATA(DATA_EEPROM) returned NULL");
         }
         *data = VAR(Memory)[i];
      }
   }
   
   // Initialize memory back to $FF just as it was before simulation start
   memset(VAR(Memory), 0xFF, VAR(Size));

   // Force hex editor to show erased $FF EEPROM contents and mode to show "?"
   VAR(Dirty) = true;

   // The hex editor is read only since On_simulation_begin() will reload data
   // and any user changes are lost. Also disable Load/Save/Erase buttons for
   // the same reasons
   VAR(Hex).readonly(true);
   EnableWindow(GET_HANDLE(GDT_LOAD), false);
   EnableWindow(GET_HANDLE(GDT_SAVE), false);
   EnableWindow(GET_HANDLE(GDT_ERASE), false);
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
      case EEARH:
      case EEARL:
         // Allow changing only those bits defined by EEPROM size masks.   
         mask = (pId == EEARH) ? VAR(EEARH_mask) : VAR(EEARL_mask);      
         Log_register_write(pId, pData, mask);
         
         // If EECR{EEPE]=1 (EEPROM programming in progress) then don't allow
         // the address registers to be modified.
         if(REG(EECR)[1] == 1) {
            WARNING("Cannot write EEAR registers while EEPROM busy (EEPE=1)",
               CAT_EEPROM, WARN_WRITE_BUSY);
         } else {
            REG(pId) = pData & mask;
         }
         
         break;
      
      // EEPROM data register
      //---------------------------------------------------------
      case EEDR:
         Log_register_write(EEDR, pData, 0xFF);

         // If EECR{EEPE]=1 (EEPROM programming in progress) then don't allow
         // the data register to be modified.
         if(REG(EECR)[1] == 1) {
            WARNING("Cannot write EEDR register while EEPROM busy (EEPE=1)",
               CAT_EEPROM, WARN_WRITE_BUSY);
         } else {
            REG(pId) = pData;
         }

         break;
      
      // EEPROM control register
      //---------------------------------------------------------
      case EECR:
         mask = VAR(EECR_mask);
         Log_register_write(EECR, pData, mask);                  

         // Bits 5,4 - EEPMx: EEPROM Programming Mode Bits
         //---------------------------------------------------------
         int newMode = pData.get_field(5,4);
         if(newMode != REG(EECR).get_field(5,4)) {

            // Mode change only allowed if EEPROM programming not already in
            // progress (i.e. EEPE must be 0).
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
               
               // Refresh Mode display in GUI
               Log("Update mode: %s", Mode_text[newMode + 1]);
               VAR(Dirty) = true;
            }
         }

         // Bit 3 - EERIE: EEPROM Ready Interrupt Enable
         //---------------------------------------------------------
         REG(EECR).set_bit(3, pData[3]);
         SET_INTERRUPT_ENABLE(ERDY, pData[3] == 1);

         // Bit 1 - EEPE: EEPROM Write/Erase Enable
         //---------------------------------------------------------
         // Note that this EEPE code block must execute before EEMPE code block
         if(pData[1] == 1) {
            // If EEPE=1 and EERE=1 then neither read nor write occurs
            if(pData[0] == 1) {
               WARNING("Cannot read (EERE=1) and write (EEPE=1) at the same time",
                  CAT_EEPROM, WARN_EEPROM_SIMULTANEOUS_RW);
            }
            
            // If EEMPE already 0 then programming operation is ignored. If
            // EEPROM is busy then EEMPE will always be 0 and cannot be
            // changed to 1 until the programming operation is finished.
            else if(REG(EECR)[2] != 1) {
               WARNING("Cannot set EEPE=1 if EEMPE not already set",
                  CAT_EEPROM, WARN_PARAM_BUSY);
            }
            
            // If EEMPE already 1 then perform selected programming operation
            else {
               // TODO: Writing the EEPROM requires an additional 2 cycle
               // CPU delay. This cannot be emulated using the current
               // VMLAB API.
               Write_EEPROM();
            }
         }
                  
         // Bit 2 - EEMPE: EEPROM Write/Erase Enable
         //---------------------------------------------------------         
         // Writing EEPE=1 always forces EEMPE=0 even if the write/erase
         // cannot occur for some other reason; writing EEMPE=0 is always
         // allowed
         if(pData[1] == 1 || pData[2] == 0) {
            REG(EECR).set_bit(2, 0);
         }
         
         // Writing EEMPE=1/X is only allowed if EEPROM not busy (EEPE=0)
         // The EEMPE will be auto-cleared after 4 cycles
         // TODO: If the 4 cycle delay due to EERE is emulated one day, then
         // the EEMPE bit will reset 4 cycles *after* the EERE delay.
         else {
            if(pData[2] == 1 && REG(EECR)[1] == 1) {
               WARNING("Cannot set EEMPE=1 while EEPROM busy (EEPE=1)",
                  CAT_EEPROM, WARN_PARAM_BUSY);
            } else {
               REG(EECR).set_bit(2, pData[2]);
               REMIND_ME2(4, RMD_AUTOCLEAR_EEMPE);
            }
         }
         
         // Bit 0 - EERE: EEPROM Read Enable
         //---------------------------------------------------------
         if(pData[0] == 1) {            
            // If EEPROM busy (EEPE already 1) then issue warning but only if
            // the EEPE coce block has not issued a similar warning
            if(REG(EECR)[1] == 1 && pData[1] != 1) {
               WARNING("Cannot read (EERE=1) while EEPROM busy (EEPE=1)",
                  CAT_EEPROM, WARN_EEPROM_SIMULTANEOUS_RW);
            }
            
            // If writing EEPE=1 and EERE=1 at once, then do nothing here
            // since the EEPE code block already issued a warning. Otherwise
            // copy the byte from EEPROM memory buffer to EEDR.
            else if(pData[1] != 1) {
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
         // TODO: VMLAB ignores SET_INTERRUPT_FLAG() from On_reset()
         // and the "Reset_status" setting in the .ini file would not
         // allow the case where EEPROM continues programming across
         // a reset and therefore ERDY remains cleared. However since
         // the reset enable mask is always disabled after reset, and
         // the only way to enable the mask is by writing EECR[EERIE],
         // we can workaround this problem by always updating the ERDY
         // flag here based on the current EEPE bit setting. If disabled
         // by sleep deeper than ADC noise reduction, then force the
         // the flag to zero so that no interrupt is triggered.
         if(VAR(Sleep)) {
            SET_INTERRUPT_FLAG(ERDY, FLAG_UNLOCK);
         } else {
            SET_INTERRUPT_FLAG(ERDY, REG(EECR)[1] == 0 ? FLAG_LOCK : FLAG_UNLOCK);
         }
         
         break;
   }   
}

void On_reset(int pCause)
//***********************
// Initialize registers to the desired value.
{
   VAR(Sleep) = false;

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
   
   // TODO: VMLAB should allow SET_INTERRUPT_FLAG() here
   
   // Force the GUI to refresh the mode display
   VAR(Dirty) = true;
}

void On_remind_me(double pTime, int pAux)
//***************************************
// Response to REMIND_ME() used implement the autoclearing of EEMPE after
// 4 cycles and the autoclearing of EEPE after write/erase cycle finishes.
{
   switch(pAux) {
   
      // Autoclear EEPE after erase/write cycle finished and set level-
      // triggered ERDY interrupt (only if simulating programming delay).
      // If disabled by SLEEP then do not update ERDY flag
      case RMD_AUTOCLEAR_EEPE:
         if(!VAR(Sleep)) {
            SET_INTERRUPT_FLAG(ERDY, FLAG_LOCK);
         }
         REG(EECR).set_bit(1, 0);
         VAR(Dirty) = true;
         break;
      
      // Autoclear of the WDCE bit 4 system clock cycles after it was set
      // TODO: Add checks for multiple pending RMD_AUTOCLEAR_EEMPE; each
      // write of EEMPE=1 should reset the countdown. Due to some timing/
      // round-off bugs in VMLAB, we can't reliably use REMIND_ME2() and
      // GET_MICRO_INFO(INFO_CPU_CYCLES) together. The cycle count returned
      // by GET_MICRO_INFO() may be plus or minus 1 compared to the cycle
      // count at which we expect On_remind_me() to execute.
      case RMD_AUTOCLEAR_EEMPE:
         if(REG(EECR)[2] != 0) {
            WARNING("EEMPE cleared by hardware; previously set 4 cycles ago",
               CAT_EEPROM, WARN_MISC);
         }
         REG(EECR).set_bit(2, 0);
         break;
   }
}

void On_sleep(int pMode)
//*********************
// The micro has entered in SLEEP mode. SLEEP mode does not affect the
// completion of a write or erase EEPROM operation, but the ERDY interrupt
// will only function in Idle or ADC noise reduction mode. This was verified
// on real ATmega48 hardware while running with debugWIRE over an AVR DRagon.
{
   bool oldSleep = VAR(Sleep);
   VAR(Sleep) = pMode > SLEEP_NOISE_REDUCTION;
   
   if(VAR(Sleep) != oldSleep) {

      // If disabled by sleep, then mask ERDY interrupt
      if(VAR(Sleep)) {
         Log("Disabled by SLEEP");
         SET_INTERRUPT_FLAG(ERDY, FLAG_UNLOCK);
         
         // Warn if EEPROM busy (EEPE=1) and ERDY interrupt enabled (EERIE=1)
         if(REG(EECR)[1] == 1 && REG(EECR)[3] == 1) {
            WARNING("ERDY interrupt will not fire while disabled by SLEEP",
               CAT_EEPROM, WARN_PARAM_BUSY);
         }
      }
      
      // If enabled by sleep, update ERDY flag to reflect EEPE state
      else {
         Log("Exit from SLEEP");
         SET_INTERRUPT_FLAG(ERDY, REG(EECR)[1] == 0 ? FLAG_LOCK : FLAG_UNLOCK);
      }
      
      VAR(Dirty) = true;
   }   
}

void On_update_tick(double pTime)
//*******************************
// Called periodically to refresh Mode display and hex editor contents.
// Only update GUI is VAR(Dirty) is true indicating that changes were
// made since the last On_update_tick()
{
   if(VAR(Dirty)) {
      // Note that get_field() will return -1 if the mode bits are UNKNOWN,
      // hence the +1 here to get the correct array index.
      int mode = REG(EECR).get_field(5,4); // EEPMx (mode bits)
      SetWindowText(GET_HANDLE(GDT_MODE), Mode_text[mode + 1]);

      // If the EEPE bit is set, then the EEPROM is simulating write/erase
      // time and is currently busy with an operation. If in sleep mode, then
      // display "Disabled" regardless of EEPE bit state.
      if(VAR(Sleep)) {
         SetWindowText(GET_HANDLE(GDT_STATUS), "Disabled");
      } else {
         SetWindowText(GET_HANDLE(GDT_STATUS), Status_text[REG(EECR)[1]]);
      }

      // Refresh hex editor contents in case AVR has updated EEPROM memory
      VAR(Hex).refresh();

      VAR(Dirty) = false;
   }
}

void On_gadget_notify(GADGET pGadget, int pCode)
//*********************************************
// Response to Win32 notification coming from buttons, etc.
{
   if(pCode != BN_CLICKED) {
      return;
   }

   switch(pGadget) {
      case GDT_LOG:       VAR(Log) ^= 1; break;
      case GDT_SIMTIME:   VAR(Simtime) ^= 1; break;
      case GDT_AUTOSAVE:  VAR(Autosave) ^= 1; break;
      case GDT_VIEW:      VAR(Hex).show(); break;
      case GDT_LOAD:      VAR(Hex).load(); break;
      case GDT_SAVE:      VAR(Hex).save(); break;
      case GDT_ERASE:     VAR(Hex).erase(); break;
   }
}
