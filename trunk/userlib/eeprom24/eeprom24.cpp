// =============================================================================
// Component Name: eeprom24 v1.0
//
// Simulation of a generic 24xxx series I2C serial EEPROM
//
// X[<Name>] _eeprom24 <MemorySize> <PageSize> [<Delay> <Address> <Mask>]
// + <SDA> <SCL>
//
// The <SDA> and <SCL> pins are respectively the serial data and serial clock
// used by the I2C bus. The <MemorySize> parameter specifies the total EEPROM
// size in bytes, and the <PageSize> parameter specifies the maximum number of
// bytes that can be written to the EEPROM with a single I2C write command.
// Both <MemorySize> and <PageSize> must both be given as "log base 2" of the
// actual byte size. For example a <MemorySize> of "12" would actually specify
// 4096 bytes since 2 to the power of 12 equals 4096.
//
// After data is written to memory, the EEPROM will go into a busy state for
// the duration of time given by the optional <Delay> parameter. During this
// busy time, the EEPROM does not respond to any commands on the I2C bus.
// If <Delay> is omitted, then it defaults to 0 which results in no busy
// time; in other words, after a memory write operation, the EEPROM will be
// immediately ready to accept another command.
//
// The optional <Address> parameter specifies the 7-bit I2C slave address to
// which the EEPROM responds. The <Mask> parameter is a bitmask indicating
// which bits in <Address> are actually significant when performing the
// address comparison; a 0 bit in <Mask> means that the corresponding bit in
// <Address> is a "don't care". If present, both parameters must be used
// together and both are specified as decimal numbers. If these paremeters are
// omitted, the default I2C address is "1010xxx".
//
// If the optional instance <Name> is specified, then the EEPROM will preserve
// its memory contents across simulation runs by using an Intel HEX format
// file "<Name>.eep".
//
// Version History:
// v1.0 2010/7/16/ - Initial public release
//
// Copyright (C) 2009-2010 Wojciech Stryjewski <thvortex@gmail.com>
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
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#pragma hdrstop

#include <blackbox.h>  // File located in <VMLAB install dir>/bin
#include "eeprom24.h"

// Because usercomp.exe supports only one source file, by including hexfile.cpp
// here, we can still use usercomp.exe for compiling and don't require a
// separate makefile.
#include "../mculib/hexfile.h"
#include "../mculib/hexfile.cpp"

// Size of temporary string buffer for generating filenames and messages
#define MAXBUF 256

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
   
//==============================================================================
// Declare pins here
//
DECLARE_PINS
   DIGITAL_BID(SDA, 1);
   DIGITAL_IN(SCL, 2);
END_PINS

// This is the delay time from a falling edge on the SCL line to a transition
// on the SDA line when the EEPROM is transmitting data. This delay is needed
// so that other devices on the I2C bus do not mistake the changing data line
// for a START or STOP condition. For 5V operating voltage, both the Atmel
// and Microchip datasheets specify a max 900ns delay.
#define SCL_TO_OUT 900e-9

// Size of temporary string buffer for generating filenames and error messages
#define MAXBUF 256

// Notify codes used with REMIND_ME(). NTF_TX shifts out the next data bit
// after a SCL falling edge. NTF_IDLE switches from the "Busy" state to the
// "Idle" state after an internal operation has finished.
enum { NTF_IDLE, NTF_TX };

// Internal device states assigned to VAR(State) and displayed in the GUI
enum {
   ST_IDLE,       // Initial state; awaiting START condition on I2C bus
   ST_START,      // START condition seen; awaiting slave address
   ST_ADDR,       // Begin write; awaiting single address byte
   ST_ADDR_MSB,   // Begin write; awaiting 1st address byte for big EEPROMs
   ST_ADDR_LSB,   // Begin write; awaiting 2nd address byte for big EEPROMs
   ST_WRITE,      // Write operation; awaiting next data byte
   ST_READ,       // Read operation; ready to shift out next data byte
   ST_READ_NAK,   // Received NAK on read operation; awaiting STOP condition
   ST_BUSY,       // Busy after write operation; I2C bus ignored
};

// String descriptions for each ST_XXX enum constant (must be in same order)
const char *State_text[] = {
   "Idle", "START", "Write (Address)", "Write (Address MSB)",
   "Write (Address LSB)", "Write (Data)", "Read", "Read (Finished)", "Busy"
};

// =============================================================================
// Declare module global variables here.
// The reason for this is to keep a set of variables by instance
// To use a variable, do it through the the macro VAR(...)
//
DECLARE_VAR
   int Write_count;    // Number of bytes received during sequential write
   int Pointer;        // Memory pointer within EEPROM for read/write
   int Pointer_temp;   // New or previous pointer used by write operation
   UCHAR *Memory;      // Pointer to malloc()ed memory holding EEPROM contents

   int State;          // One of the ST_XXX enum constants
   bool Dirty;         // True if the GUI needs to be refreshed
   
   int Pointer_mask;   // Pointer bitmask based on total EEPROM memory size
   int Page_mask;      // Pointer bitmask based on sequential write page size
   int Slave_addr;     // I2C slave address assigned to this EEPROM
   int Slave_mask;     // Bitmask indicating which Slave_addr bits are used
   int Slave_ptr_mask; // Part of slave address used as memory address bits
   double Delay;       // Delay in seconds for write operations to complete
   
   UCHAR RX_byte;      // Receive data shifted in from SDA pin
   USHORT TX_byte;     // Inverse of transmit data to shift out on SDA pin
   char RX_count;      // Number of bits left to receive in current word
   char TX_count;      // Number of bits left to transmit in current word
   
   bool Log;           // True if the "Log" checkbox button is checked
   bool Break;         // True if the "Break on error" checkbox button checked

   Hexfile Hex;        // Helper class for GUI View/Load/Save/Erase   

   LOGIC SDA_state;    // TODO: DELETE ME ONCE ON_DIGITAL_IN_EDGE WORKS   
   LOGIC SCL_state;    // TODO: DELETE ME ONCE ON_DIGITAL_IN_EDGE WORKS   
END_VAR

// You can delare also globals variable outside DECLARE_VAR / END_VAR, but if
// multiple instances of this cell are placed, all these instances will
// share the same variable.

// ============================================================================
// Say here if your component has an associated window or not. Pass as parameter
// the dialog resource ID (from .RC file) or 0 if no window
//
USE_WINDOW(WINDOW_USER_1);

// =============================================================================
// Helper Functions

void Tx(UCHAR pData, bool pAck = false)
//*************************
// Transmit the 8-bit pData value and afterwards setup to receive a single
// acknowledgment bit from the master. If pAck is true then also transmit
// a single ACK bit (i.e. a 0 on the I2C bus) immediately before transmiting
// pData.
{
   // Transmit 8-bit pData with an optional ACK preceeding it. VAR(TX_byte)
   // is a USHORT and is shifted out on the SDA line in MSb first order,
   // therefore pData must be left aligned within the USHORT. Note that
   // VAR(TX_count) is always 1 more than the number of data bits to
   // transmit; after the entire value is transmitted, an additional 1 bit
   // is transmitted which serves to tri-state the I2C bus and allow an
   // acknowledgment or a START/STOP condition to be received. Also note that
   // VAR(TX_byte) actually holds the inverse of the transmit data; using
   // an inverse makes it a little simpler to combine pData along with the
   // trailing 1 bit (for tri-stating the bus) and when needed the preceeding
   // 0 ACK bit.
   if(pAck){
      VAR(TX_count) = 10;
      VAR(TX_byte) = ~pData << 7;
   } else {
      VAR(TX_count) = 9;
      VAR(TX_byte) = ~pData << 8;
   }   
   
   VAR(RX_count) = 1;
   VAR(RX_byte) = 0;
}

void Rx()
//*************************
// First transmit a single ACK bit to acknowledge a previously received byte,
// and then setup to receive another byte.
{
   // See comments in Tx() for how VAR(TX_count) and VAR(TX_byte) is used
   VAR(TX_count) = 2;
   VAR(TX_byte) = 0x8000;
   
   VAR(RX_count) = 8;
   VAR(RX_byte) = 0;
}

void Log(const char *pFormat, ...)
//*************************
// Wrapper around the PRINT() function to provide printf() like functionality
// This function also automatically prepends the instance name to the formatted
// message. The message is only printed if the "Log" button is checked.
{
   if(VAR(Log)) {   
      char strBuffer[MAXBUF];
      va_list argList;

      sprintf(strBuffer, "%s: ", GET_INSTANCE());
      int len = strlen(strBuffer);

      va_start(argList, pFormat);
      vsnprintf(strBuffer + len, MAXBUF - len, pFormat, argList);
      va_end(argList);

      PRINT(strBuffer);
   }
}

void Error(const char *pMessage)
//*************************
// If the "Break on error" button is checked, then breakpoint the simulation
// with the specified error message. If "Break on error" is not checked but
// the "Log" button is, then simply PRINT() the error to the Messages window.
// If neither button is checked then do nothing.
{
   if(VAR(Break)) {
      BREAK(pMessage);
   } else if(VAR(Log)) {
      Log("%s", pMessage);
   }
}

bool On_Start_or_Stop()
//**********************
// Called by either On_Start() or On_Stop() to check for behavior that is
// common to both START and STOP conditions. If the function returns true
// then On_Start() and On_Stop() will return immediately without performing
// the usual START/STOP handling.
{
   switch(VAR(State)) { 

      // If busy after write operation then ignore all I2C bus activity
      case ST_BUSY:
         return true;
   
      // A START/STOP should not occur while still receiving the slave address
      // from a previous START or while still receiving the EEPROM memory
      // address at the beginning of a write operation.
      case ST_START:   
      case ST_ADDR:    
      case ST_ADDR_MSB:
      case ST_ADDR_LSB:
         Error("Unexpected START/STOP before command finished");
         break;

      // During read operation, the master should respond with a NAK
      // on the last byte before sending a START or STOP
      case ST_READ:
         Error("Unexpected START/STOP without receiving NAK");
         break;

      // During write mode, a START/STOP finishes the write operation and
      // begins a busy cycle. However, the START/STOP should only occur
      // after a complete byte was received.
      case ST_WRITE:
         Log("Total %d bytes written to EEPROM", VAR(Write_count));

         // Check for START/STOP in the middle of byte
         if(VAR(RX_count) != 7) {
            Error("Unexpected START/STOP during byte write");
         }
         
         // Schedule busy time if <Delay> is non-zero and data was written
         if(VAR(Delay) > 0 && VAR(Write_count)) {
            REMIND_ME(VAR(Delay), NTF_IDLE);
            VAR(State) = ST_BUSY;
            VAR(Dirty) = true;
            return true;
         }
         break;

      // Accept START/STOP; EEPROM not actively using I2C bus
      case ST_READ_NAK:
      case ST_IDLE:
         break;
   }
   
   // Allow On_Start() or On_Stop() to continue with regular processing
   return false;
}

void On_Start()
//**********************
// Called by On_digital_in_edge() in response to START condition on I2C bus.
{
   // Handle common START/STOP behavior; return if in/entering ST_BUSY state
   // If ST_BUSY was just entered, then cancel any RX/TX operations so the
   // EEPROM does not interfere with the I2C bus until ST_BUSY is over
   if(On_Start_or_Stop()) {
      VAR(TX_count) = 0;   
      VAR(RX_count) = 0;
      return;
   }
   
   // Setup to receive the I2C slave address if allowed by On_Start_or_Stop()
   VAR(RX_count) = 8;
   VAR(RX_byte) = 0;
   VAR(State) = ST_START;
   VAR(Dirty) = true;
}

void On_Stop()
//**********************
// Called by On_digital_in_edge() in response to STOP condition on I2C bus
{
   // Handle common START/STOP behavior; ignore == true if entering ST_BUSY
   bool ignore = On_Start_or_Stop();

   // Always cancel any pending RX or TX operations
   VAR(TX_count) = 0;   
   VAR(RX_count) = 0;
   
   // Await a new START condition if allowed by On_Start_or_Stop()
   if(!ignore) {
      VAR(State) = ST_IDLE;
      VAR(Dirty) = true;
   }
}

void On_read_byte(bool pAck)
//**********************
// Called from On_Rx() when a read mode has been requested. This function
// transmits the data byte at the current pointer address in EEPROM memory and
// then increments the pointer (if the pointer reaches the end of memory, it
// wraps back around to address zero). When reading the very first byte in a
// transfer, pAck is set to true so that an ACK bit is sent to acknowledge the
// slave address that was just received.
{
   UCHAR data = VAR(Memory)[VAR(Pointer)];

   Log("Read EEPROM[$%05X]=$%02X", VAR(Pointer), data);

   VAR(Pointer) = (VAR(Pointer) + 1) & VAR(Pointer_mask);   
   Tx(data, pAck);
}

void On_write_byte(UCHAR pData)
//**********************
// Called from On_Rx() when a new data byte is received for writing to EEPROM
// memory. The address pointer is incremented after the write (if the pointer
// reached the end of the current page, it wraps back around to the start
// of the same page). This function also issues errors when the address pointer
// wraps and when the entire page has been filled up
{
   Log("Write EEPROM[$%05X]=$%02X", VAR(Pointer), pData);
   VAR(Memory)[VAR(Pointer)] = pData;
   
   // Check if this write has wrapped around to beginning of page
   if(VAR(Pointer) < VAR(Pointer_temp)) {
      Error("EEPROM address wrapped to start of page");
   }

   // Increment pointer and wrap around within the same page. The current
   // pointer value is saved in VAR(Pointer_temp) so that a wrap around can
   // be detected by the next call to On_write_byte().
   VAR(Pointer_temp) = VAR(Pointer);
   VAR(Pointer) = VAR(Pointer) & (VAR(Pointer_mask) ^ VAR(Page_mask));
   VAR(Pointer) |= (VAR(Pointer_temp) + 1) & VAR(Page_mask);
   
   // Cannot write more than one page of data to EEPROM memory.
   if(VAR(Write_count) < VAR(Page_mask) + 1) {
      VAR(Write_count)++;
   } else {
      Error("Page buffer full; previous byte lost");
   }
}

void On_Rx(UCHAR pData)
//**********************
// Called by On_digital_in_edge() when the requested number of bits in
// VAR(RX_count) has been shifted in and the received data is stored in
// VAR(RX_byte).
{
   switch(VAR(State)) {
 
      // Slave address and read/write flag received after START condition
      case ST_START:

         // If slave address match then decode the read/write flag
         if((pData & VAR(Slave_mask)) == VAR(Slave_addr)) {

            // If read mode requested, no address bytes have to be received
            if(pData & 1) {
               VAR(State) = ST_READ;
               On_read_byte(true);
               
            // If write mode requsted, EEPROMS with 4096 bytes or more will
            // require 2 address bytes to be received, while smaller EEPROMS
            // require only 1 address byte. Also extract the LSbs of the slave
            // address and use them as the MSbs of the address pointer if
            // required by the EEPROM memory size.
            } else {
               if(VAR(Pointer_mask) >= 0xFFF) {
                  VAR(State) = ST_ADDR_MSB;
                  VAR(Pointer_temp) = (pData & VAR(Slave_ptr_mask)) << 15;
               } else {
                  VAR(State) = ST_ADDR;
                  VAR(Pointer_temp) = (pData & VAR(Slave_ptr_mask)) << 7;
               }
               Rx();
            }

         // If no address match, return to ST_IDLE and await next START
         } else {
            VAR(State) = ST_IDLE;
         }
         
         break;

      // Most significant address byte read; setup to read address LSB
      case ST_ADDR_MSB:
         VAR(Pointer_temp) |= (pData << 8);

         VAR(State) = ST_ADDR_LSB;
         Rx();
         break;

      // 2nd (or only) byte of write address received. Set the EEPROM address
      // pointer and prepare to receive data for writing
      case ST_ADDR:
      case ST_ADDR_LSB:
         VAR(Pointer_temp) = (VAR(Pointer_temp) | pData) & VAR(Pointer_mask);
         VAR(Pointer) = VAR(Pointer_temp);
         VAR(Write_count) = 0;
         Log("Set EEPROM address = $%05X", VAR(Pointer));

         VAR(State) = ST_WRITE;
         Rx();
         break;
         
      // New data byte received during EEPROM write operation
      case ST_WRITE:
         On_write_byte(pData);
         Rx();
         break;

      // Received acknowledgment after transmitting byte during read operation
      case ST_READ:
      
         // If ACK (i.e. a 0 bit) received, transmit the next byte. Otherwise
         // stop further transmission and await a START/STOP condition.
         if(pData == 0) {
            On_read_byte(false);                 
         } else {
            VAR(State) = ST_READ_NAK;
         }
         break;
         
      default:
         BREAK("Unexpected internal state in On_Rx()");
         break;
   }
   
   VAR(Dirty) = true;
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
   // Specify default values for optional parameters
   VAR(Delay) = 0;              // No write delay; fastest possible simulation
   VAR(Slave_addr) = 0x50 << 1; // Standard EEPROM address: 1010xxx
   VAR(Slave_mask) = 0x78 << 1; // EEPROM with A2-A0 pins not used: 1111000

   // TODO: Uncomment once VMLAB 3.16 is released and supports GET_PARAM(0)
   //int paramCount = (int) GET_PARAM(0);
   int paramCount = 5;
   if(paramCount < 2) {
      return "<MemorySize> and <PageSize> parameters are required";
   }
      
   // <MemorySize> is the total EEPROM memory byte size (in log base 2)
   int memorySize = (int) GET_PARAM(1);
   if(memorySize < 4 || memorySize > 19) {
      return "<MemorySize> parameter must be an integer 4 to 19";
   }
   VAR(Pointer_mask) = (1 << memorySize) - 1;
   
   // <PageSize> is the sequential write page size (in log base 2)
   int pageSize = (int) GET_PARAM(2);
   if(pageSize < 0 || pageSize > memorySize) {
      return "<PageSize> parameter must be an integer 0 to <MemorySize>";
   }
   VAR(Page_mask) = (1 << pageSize) - 1;
   
   // <Delay> is the delay (in seconds) for write operations to complete
   if(paramCount >= 3) {
      VAR(Delay) = GET_PARAM(3);
   }

   // <SlaveAddr> and <SlaveMask> specify the I2C address used by EEPROM
   if(paramCount == 4) {
      return "<SlaveAddr> and <SlaveMask> parameters must be used together";
   }
   if(paramCount >= 5) {
      int slaveAddr = (int) GET_PARAM(4);
      if(slaveAddr < 0 || slaveAddr > 127) {
         return "<SlaveAddr> must be an integer 0 to 127";
      }     
      int slaveMask = (int) GET_PARAM(5);
      if(slaveMask < 0 || slaveMask > 127) {
         return "<SlaveMask> must be an integer 0 to 127";
      }
      
      // Adjust the 7-bit slave address/mask parameters to match the full 8-bit
      // byte received after the START condition where the LSb in the byte is
      // a read/write flag;
      VAR(Slave_addr) = slaveAddr << 1;
      VAR(Slave_mask) = slaveMask << 1;
   }
   
   // Certain larger memory sizes require using some of the bits in the I2C
   // slave address as most significant bits in the EEPROM address pointer.
   // VAR(Slave_ptr_mask) is the bitmask applied against the slave address
   // to extract those bits if they are needed. If needed, these bits also
   // overide VAR(Slave_mask) and are always treated as "don't care" bits
   // for slave address matching purposes.
   switch(memorySize) {
      case 9:  case 17: VAR(Slave_ptr_mask) = 0x2; break;
      case 10: case 18: VAR(Slave_ptr_mask) = 0x6; break;
      case 11: case 19: VAR(Slave_ptr_mask) = 0xE; break;
      default: VAR(Slave_ptr_mask) = 0x0; break;
   }
   VAR(Slave_mask) &= ~VAR(Slave_ptr_mask);
   VAR(Slave_addr) &= VAR(Slave_mask);
   
   // Allocate enough memory to store all of the EEPROM contents
   VAR(Memory) = (UCHAR *) malloc(VAR(Pointer_mask) + 1);
   if(VAR(Memory) == NULL) {
      return "Could not allocate memory";
   }
   
   // Initialize EEPROM contents to fully erased (0xFF) state
   memset(VAR(Memory), 0xFF, VAR(Pointer_mask) + 1);

   return NULL;
}

void On_window_init(HWND pHandle)
//*******************************
// Window initialization. Fill only if the component has an associated window
// -USE_WINDOW(..) not zero-. The Parameter pHandle brings the main component
// window handle. Typical tasks: fill controls with data, intitialize gadgets,
// hook an own Windows structure (VCL, MFC,...)
{
   char strBuffer[MAXBUF];

   // Initialize the "I2C Slave Address" display in the GUI based on what the
   // user specified in the <SlaveAddress> and <SlaveMask> parameters, and
   // based on how many bits of the slave address are used to specify the
   // EEPROM page address.
   for(int i = 0; i < 7; i++) {
      int bit = 1 << (7 - i);

      if(VAR(Slave_ptr_mask) & bit) {
         strBuffer[i] = 'p';
      } else if(!(VAR(Slave_mask) & bit)) {
         strBuffer[i] = 'x';
      } else {
         strBuffer[i] = (VAR(Slave_addr) & bit) ? '1' : '0';
      }
   }   
   strBuffer[7] = '\000';   
   SetWindowText(GET_HANDLE(GDT_SLAVE), strBuffer);

   // Even if On_create() returns error, VMLAB will still create a window
   // and call On_window_init() so the "if(VAR(Memory))" statement is
   // necessary to avoid an exception.
   if(VAR(Memory)) {      
      // Initialize Hexfile support class. For some reason VMLAB adds a space
      // in front of the title string for most child windows, so this peripheral
      // maintains the same convention. Icon resource 13005 in VMLAB.EXE is the
      // "E^2" icon used for the main EEPROM window. The window name is set
      // to match the component name displayed in the "Perhipherals" window,
      // including the instance name in case of multiple eeprom24 instances.
      sprintf(strBuffer, " EEPROM 24xxx Memory (%s)", GET_INSTANCE());
      VAR(Hex).init(DLL_instance, pHandle, strBuffer, 13005);
      VAR(Hex).data(VAR(Memory), VAR(Pointer_mask) + 1);

      // If this instance has a user assigned name (i.e. not a "$NN" name auto
      // generated by VMLAB) then automatically load the EEPROM memory contents
      // from "<Name>.eep", but only if it already exists.
      sprintf(strBuffer, "%s.eep", GET_INSTANCE());
      if(strBuffer[0] != '$' && GetFileAttributes(strBuffer) != (unsigned) -1) {
         VAR(Hex).load(strBuffer, Hexfile::FT_HEX);
      }
   }
}

void On_destroy()
//***************
// Destroy component. Free here memory allocated at On_create; close files
// etc.
{
   // Even if On_create() returns error, VMLAB will still create a window
   // and call On_window_init() so the "if(VAR(Memory))" statement is
   // necessary to avoid an exception.
   if(VAR(Memory)) {
      // If this instance has a user assigned name (i.e. not a "$NN" name auto
      // generated by VMLAB) then automatically save the EEPROM memory contents
      // to "<Name>.eep".
      char strBuffer[MAX_PATH];
      sprintf(strBuffer, "%s.eep", GET_INSTANCE());
      if(strBuffer[0] != '$') {
         VAR(Hex).save(strBuffer, Hexfile::FT_HEX);
      }

      // Deallocate EEPROM contents memory previously allocated in On_create()
      // and destroy the hex editor window to ensure no accidental memory
      // references to the deallocated buffer.
      VAR(Hex).destroy();
      free(VAR(Memory));
   }
}

void On_simulation_begin()
//************************
// VMLAB informs you that the simulation is starting. Initialize pin values
// here Open files; allocate memory, etc.
{
   // Initialize internal variables to power-on state and update GUI
   VAR(State) = ST_IDLE;
   VAR(Pointer) = 0x0;
   VAR(Dirty) = true;
}

void On_simulation_end()
//**********************
// Undo here the operations done at On_simulation_begin: free memory, close
// files, etc.
{
   // Cancel any pending GUI update and set display to unknown power-off state
   SetWindowText(GET_HANDLE(GDT_ADDR), "$?????");
   SetWindowText(GET_HANDLE(GDT_STATUS), "?");
   VAR(Dirty) = false;
}

// TODO: Remove trailing _ once VMLAB 3.16 is released
void On_digital_in_edge_(PIN pDigitalIn, EDGE pEdge, double pTime)
//**************************************************************
// Response to a digital input pin edge. The EDGE type parameter (pEdge) can
// be RISE or FALL. Use pin identifers as declared in DECLARE_PINS
{
   switch(pDigitalIn) {
   
      // SDA transitions while clock is high are START/STOP conditions
      case SDA:
         if(GET_LOGIC(SCL) == 1) {
            if(pEdge == FALL) {
               On_Start();
            } else {
               On_Stop();
            }
         }      
         break;
         
      // RX data is sampled on the rising edge of the clock. TX data is
      // shifted on the falling edge after a short delay.
      case SCL:
      
         // Data is shifted in MSb first. Don't try to receive if the last TX
         // bit has just been shifted out, because this rising edge will be
         // used by the master to sample the last bit.
         if(pEdge == RISE && VAR(RX_count) && !VAR(TX_count)) {
            VAR(RX_byte) <<= 1;
            VAR(RX_byte) |= (GET_LOGIC(SDA) == 1);
            VAR(RX_count)--;
                        
            // If all expected bits eceived, call On_Rx() to handle the data
            if(VAR(RX_count) == 0) {
               On_Rx(VAR(RX_byte));
            }
         }
               
         // Pending TX data is shifted out 300ns after falling SCL edge
         if(pEdge == FALL && VAR(TX_count)) {     
            REMIND_ME(SCL_TO_OUT, NTF_TX);
         }
         
         break;
   }
}

void On_remind_me(double pTime, int pData)
//***************************************
// VMLAB notifies about a previouly sent REMIND_ME() function.
{
   switch(pData) {
   
      // An internal write operation has finished. EEPROM is once again
      // ready to respond to I2C bus activity
      case NTF_IDLE:
         VAR(State) = ST_IDLE;
         VAR(Dirty) = true;
         break;

      // Shift out the next data bit
      case NTF_TX:

         // Verify SCL is still low, otherwise a transition on SDA would
         // be interpreted as a START or STOP condition.
         if(GET_LOGIC(SCL) == 1) {
            Error("Clock on SCL pin changing too fast");
         }
         
         // If no data left to transmit, then it's a bug in the component
         if(!VAR(TX_count)) {
            BREAK("Internal error: (No data to TX)");
            return;
         }

         // Data is shifted out MSb first. Because SDA is open-collector, a 0
         // bit is actively driven on SDA and a 1 bit tri-states SDA and lets
         // the pull-up resistor generate the high logic value. Note that the
         // data in TX_byte is actually inverted so a 0 bit in the variable is
         // output as a 1 bit on SDA and vice-versa.
         if(VAR(TX_byte) & 0x8000) {
            SET_DRIVE(SDA, true);
            SET_LOGIC(SDA, 0);
         } else {
            SET_DRIVE(SDA, false);            
         }
         VAR(TX_byte) <<= 1;
         VAR(TX_count)--;                  
         break;
   }
}

void On_gadget_notify(GADGET pGadgetId, int pCode)
//************************************************
// A window gadget (control) is sending a notification.
// Handles button/checkbox clicks in the GUI.
{
   // Even if On_create() returns error, VMLAB will still create a window
   // and call On_window_init() so the "if(VAR(Memory))" statement is
   // necessary to avoid an exception.
   if(VAR(Memory)) {
      switch(pGadgetId) {
         case GDT_BREAK:      VAR(Break) ^= 1; break;         
         case GDT_LOG:        VAR(Log) ^= 1; break;
         case GDT_VIEW:       VAR(Hex).show(); break;
         case GDT_LOAD:       VAR(Hex).load(); break;
         case GDT_SAVE:       VAR(Hex).save(); break;
         case GDT_ERASE:      VAR(Hex).erase(); break;   
      }
   }
}

void On_update_tick(double pTime)
//************************************************
// Called periodically to re-draw the GUI. The GUI is only updated if
// VAR(Dirty) is true to indicate that internal state has changed
{
   if(VAR(Dirty)) {
      char strBuffer[16];      
      sprintf(strBuffer, "$%05X", VAR(Pointer));
      SetWindowText(GET_HANDLE(GDT_ADDR), strBuffer);

      SetWindowText(GET_HANDLE(GDT_STATUS), State_text[VAR(State)]);

      VAR(Hex).refresh(); // Refresh hex editor in case EEPROM memory changed
      VAR(Dirty) = false;
   }
}

void On_time_step(double pTime)
// TODO: DELETE THIS ONCE ON_DIGITAL_EDGE WORKS FOR SDA
{
   if(!GET_DRIVE(SDA) && GET_LOGIC(SDA) != VAR(SDA_state)) {
      VAR(SDA_state) = GET_LOGIC(SDA);
      On_digital_in_edge_(SDA, VAR(SDA_state) ? RISE : FALL, pTime);
   }
   if(GET_LOGIC(SCL) != VAR(SCL_state)) {
      VAR(SCL_state) = GET_LOGIC(SCL);
      On_digital_in_edge_(SCL, VAR(SCL_state) ? RISE : FALL, pTime);
   }
}
