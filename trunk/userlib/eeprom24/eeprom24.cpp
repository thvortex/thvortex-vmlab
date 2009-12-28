// =============================================================================
// Component Name: I2C 24xxx series EEPROM
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
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#pragma hdrstop

#include <blackbox.h>  // File located in <VMLAB install dir>/bin
#include "eeprom24x.h"

int WINAPI DllEntryPoint(HINSTANCE, unsigned long, void*) {return 1;} // is DLL
//==============================================================================

//==============================================================================
// Declare pins here
//
DECLARE_PINS
   DIGITAL_BID(SDA, 1);
   DIGITAL_IN(SCL, 2);
   DIGITAL_IN(WP, 3);
END_PINS

// This is the delay time from a falling edge on the SCL line to a transition
// on the SDA line when the EEPROM is transmitting data. This delay is needed
// so that other devices on the I2C bus do not mistake the changing data line
// for a START or STOP condition.
#define SCL_TO_OUT 300e-9 // TODO: This should be longer?

// Size of temporary string buffer for generating filenames and error messages
#define MAXBUF 256

// Notify codes used with REMIND_ME(). NTF_TX shifts out the next data bit
// after a SCL falling edge. NTF_IDLE switches from the "Busy" state to the
// "Idle" state after an internal operation has finished. NTS_TXDONE is used
// on a SCL falling edge to signal the final bit in a transmission has been
// sent finished.
enum { NTF_TX, NTF_TXDONE, NTF_IDLE };

// Internal device states assigned to VAR(State) and displayed in the GUI
enum {
   ST_IDLE,       // Initial state; awaiting START condition on I2C bus
   ST_START,      // START condition seen; awaiting slave address
   ST_ADDR,       // Begin write; awaiting single address byte
   ST_ADDR_MSB,   // Begin write; awaiting 1st address byte for big EEPROMS
   ST_ADDR_LSB,   // Begin write; awaiting 2nd address byte for big EEPROMS
   ST_WRITE,      // Write operation; awaiting next data byte
   ST_READ,       // Read operation; ready to shift out next data byte
   ST_READ_NAK,   // Received NAK on read operation; awaiting STOP condition
   ST_BUSY,       // Busy after write operation; I2C bus ignored
};

// String descriptions for each ST_XXX enum constant (must be in same order)
const char *State_text[] = {
   "Idle", "Start", "Write (Address)", "Write (Address MSB)",
   "Write (Address LSB)", "Write (Data)", "Read", "Read (Finished)", "Busy"
};

// =============================================================================
// Declare module global variables here.
// The reason for this is to keep a set of variables by instance
// To use a variable, do it through the the macro VAR(...)
//
DECLARE_VAR
   int Write_Count;    // Number of bytes received during sequential write
   int Pointer;        // Memory pointer within EEPROM for read/write
   char *Memory;       // Pointer to malloc()ed memory holdinf EEPROM contents

   int State;          // One of the ST_XXX enum constants
   bool Dirty;         // True if the GUI needs to be refreshed
   
   int Pointer_mask;   // Pointer bitmask based on total EEPROM memory size
   int Page_mask;      // Pointer bitmask based on sequential write page size
   int Protect_size;   // Size of EEPROM memory write-protected by WP pin
   int Slave_addr;     // I2C slave address assigned to this EEPROM
   int Slave_mask;     // Bitmask indicating which Slave_addr bits are used
   int Slave_ptr_mask; // Part of slave address used as memory address MSbs
   double Delay;       // Delay in seconds for write operations to complete
   
   UCHAR RX_Byte;      // Receive data shifted in from SDA pin
   UCHAR TX_Byte;      // Transmit data waiting to shift out on SDA pin
   char RX_Count;      // Number of bits left to receive in current word
   char TX_Count;      // Number of bits left to transmit in current word
   bool TX_Active;     // True if a TX operation begun and still in progress
   
   bool Log;           // True if the "Log" checkbox button checked
   bool Break;         // True if the "Break on error" checkbox button checked
      
   LOGIC SDA_State;    // TODO: DELETE ME ONCE ON_DIGITAL_EDGE WORKS
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

// Shortcuts for common operations on the I2C bus
inline void Tx(char pData) { VAR(TX_Count) = 8; VAR(TX_Byte) = pData; }
inline void Tx_Ack()       { VAR(TX_Count) = 1; VAR(TX_Byte) = 0; }
inline void Tx_Nak()       { VAR(TX_Count) = 1; VAR(TX_Byte) = 0x80; }
inline void Rx()           { VAR(RX_Count) = 8; VAR(RX_Byte) = 0; }
inline void Rx_Ack()       { VAR(RX_Count) = 1; VAR(RX_Byte) = 0; }

void Log(const char *pFormat, ...)
//*************************
// Wrapper around the PRINT() function to provide printf() like functionality
// This function also automatically prepends the instance name to the formatted
// message. The message is only printed if the "Log" button is checked.
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

void Error(const char *pMessage)
//*************************
// If the "Break on error" button is checked, then breakpoint the simulation
// with the specified error message. If "Break on error" is not checked but
// the "Log" button is, then PRINT() the error message with an "ERROR: "
// prefix. If neither button is checked then do nothin.
{
   if(VAR(Break)) {
      BREAK(pMessage);
   } else if(VAR(Log)) {
      Log("ERROR: ", pMessage);
   }
}

void On_Start()
//**********************
// Called by On_digital_in_edge() in response to START condition on I2C bus
{
   // EEPROM is ready to receive I2C slave address word
   // TODO: Go into START state; START may complete WRITE operations
   
   // TODO: Signal an error if in the middle of certain operations
   
   Rx();
}

void On_Stop()
//**********************
// Called by On_digital_in_edge() in response to STOP condition on I2C bus
{
   // TODO: Stop condition only allowed after 0 or 1 bit received   
   // TODO: Go into IDLE state
   
   // Cancel any pending RX or TX operations
   VAR(TX_Active) = false;
   VAR(TX_Count) = 0;   
   VAR(RX_Count) = 0;
}

void On_Rx(UCHAR pData)
//**********************
// Called by On_digital_in_edge() when the requested number of bits in
// VAR(RX_Count) has been shifted in and the received data is stored in
// VAR(RX_Byte).
{
   char buf[64];
   snprintf(buf, 64, "RX: %02X", pData);
   PRINT(buf);
      
   // TODO: Transmit ACK if not busy and EEPROM in write mode
   Tx_Ack();
}

void On_Tx()
//**********************
// Called by On_remind_me() when the requested number of bits in VAR(TX_Count)
// has been shifted out from VAR(TX_Byte).
{
   // TODO (write mode): Setup to receive the next byte
   Rx();
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
   int paramCount = GET_PARAM(0);
   if(paramCount < 2) {
      return "<MemorySizeL2> and <PageSizeL2> parameters are required";
   }
      
   // <MemorySizeL2> is the total EEPROM memory byte size (in log base 2)
   int memorySize = GET_PARAM(1);
   if(memorySize < 0 || memorySize > 19) {
      return "<MemorySizeL2> parameter must be an integer 0 to 19";
   }
   VAR(Pointer_mask) = (1 << memorySize) - 1;
   
   // <PageSizeL2> is the sequential write page size (in log base 2)
   int pageSize = GET_PARAM(2);
   if(pageSize < 0 || pageSize > memorySize) {
      return "<PageSizeL2> parameter must be an integer 0 to <MemorySizeL2>";
   }
   VAR(Page_mask) = (1 << pageSize) - 1;

   // Specify default values for optional parameters
   VAR(Delay) = 0;         // No write delay; fastest possible simulation
   VAR(Slave_addr) = 0x50; // Default slave address: 1010xxx
   VAR(Slave_mask) = 0x78; // Default slave address mask: 1111000
   VAR(Protect_size) = VAR(Pointer_mask) + 1; // Protect entire EEPROM
   
   // <Delay> is the delay (in seconds) for write operations to complete
   if(paramCount >= 3) {
      VAR(Delay) = GET_PARAM(3);
   }

   // <SlaveAddr> and <SlaveMask> specify the I2C address used by EEPROM
   if(paramCount == 4) {
      return "<SlaveAddr> and <SlaveMask> parameters must be included together";
   }
   if(paramCount >= 5) {
      VAR(Slave_addr) = GET_PARAM(4);
      if(VAR(Slave_addr) < 0 || VAR(Slave_addr) > 127) {
         return "<SlaveAddr> must be an integer 0 to 127";
      }     
      VAR(Slave_mask) = GET_PARAM(5);
      if(VAR(Slave_mask) < 0 || VAR(Slave_mask) > 127) {
         return "<SlaveMask> must be an integer 0 to 127";
      }
   }
   
   // Adjust the 7-bit slave address/mask parameters to match the full 8-bit
   // byte received after the START condition where the LSb in the byte is
   // a read/write flag;
   VAR(Slave_addr) <<= 1;
   VAR(Slave_mask) <<= 1;

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
   
   // <ProtectSizeL2> is number of bytes protected by WP pin (in log base 2)
   if(paramCount >= 6) {
      int protectSize = GET_PARAM(6);
      if(protectSize < 0 || protectSize > memorySize) {
         return "<ProtectSizeL2> parameter must be an integer 0 to <MemorySizeL2>";
      }
      VAR(Protect_size) = (1 << protectSize);
   }
   
   // Allocate enough memory to store all of the EEPROM contents
   VAR(Memory) = (char *) malloc(VAR(Pointer_mask) + 1);
   if(VAR(Memory) == NULL) {
      return "Could not allocate memory";
   }
   
   return NULL;
}

void On_window_init(HWND pHandle)
//*******************************
// Window initialization. Fill only if the component has an associated window
// -USE_WINDOW(..) not zero-. The Parameter pHandle brings the main component
// window handle. Typical tasks: fill controls with data, intitialize gadgets,
// hook an own Windows structure (VCL, MFC,...)
{
   char strBuffer[8];

   // Initialize the "I2C Slave Address" display in the GUI based on what the
   // user specified in the <SlaveAddress> and <SlaveMask> parameters
   for(int i = 0; i < 7; i++) {
      int bit = 1 << (7 - i);

      if(VAR(Slave_ptr_mask) & bit) {
         strBuffer[i] = 'a';
      } else if(!(VAR(Slave_mask) & bit)) {
         strBuffer[i] = 'x';
      } else {
         strBuffer[i] = (VAR(Slave_addr) & bit) ? '1' : '0';
      }
   }
   
   strBuffer[7] = '\000';   
   SetWindowText(GET_HANDLE(GDT_SLAVE), strBuffer);
}

void On_destroy()
//***************
// Destroy component. Free here memory allocated at On_create; close files
// etc.
{
   // Deallocate EEPROM contents memory previously allocated in On_create()
   free(VAR(Memory));
}

void On_simulation_begin()
//************************
// VMLAB informs you that the simulation is starting. Initialize pin values
// here Open files; allocate memory, etc.
{
   // TODO: DELETE THIS ONCE ON_DIGITAL_EDGE WORKS FOR INPUTS
   SET_DRIVE(SDA, false);
   VAR(SDA_State) = GET_LOGIC(SDA);
   
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

void On_digital_in_edge(PIN pDigitalIn, EDGE pEdge, double pTime)
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
      
         // If both RX and TX data pending, then it's a bug in the component
         if(VAR(RX_Count) && VAR(TX_Count)) {
            BREAK("Internal error (RX and TX at the same time)");
            return;
         }
      
         // Data is shifted in MSb first. Don't try to receive if the last TX
         // bit has just been shifted out, because this rising edge will be
         // used by the master to sample the last bit.
         if(pEdge == RISE && VAR(RX_Count) && !VAR(TX_Active)) {
            VAR(RX_Byte) <<= 1;
            VAR(RX_Byte) |= (GET_LOGIC(SDA) == 1);
            VAR(RX_Count)--;
                        
            // If all expected bits eceived, call On_Rx() to handle the data
            if(VAR(RX_Count) == 0) {
               On_Rx(VAR(RX_Byte));
            }
         }
               
         if(pEdge == FALL) {     
         
            // Pending TX data is shifted out 300ns after falling SCL edge
            if(VAR(TX_Count)) {
               VAR(TX_Active) = true;
               REMIND_ME(SCL_TO_OUT, NTF_TX);
               
            // Otherwise, if the last TX bit has been shifted out on the previous
            // falling edge, then switch the SDA line back to input on this edge
            // (after 300ns) so that an acknowledgment or a START/STOP condition
            // can be detected.
            } else if(VAR(TX_Active)) {
               REMIND_ME(SCL_TO_OUT, NTF_TXDONE);
            }
         }
         
         break;
   }
}

void On_remind_me(double pTime, int pData)
//***************************************
// VMLAB notifies about a previouly sent REMIND_ME() function.
{
   switch(pData) {
   
      // An internal operation has finished. EEPROM is once again ready to
      // receive a new slave address on the I2C bus.
      case NTF_IDLE:
         VAR(RX_Count) = 8; // TODO: Only change state; START handles this     
         break;

      // Shift out the next data bit
      case NTF_TX:

         // Verify SCL is still low, otherwise a transition on SDA would
         // be interpreted as a START or STOP condition.
         if(GET_LOGIC(SCL) == 1) {
            Error("Clock on SCL pin changing too fast");
            return;
         }
         
         // If no data left to transmit, then it's a bug in the component
         if(!VAR(TX_Count)) {
            BREAK("Internal error: (No data to TX)");
            return;
         }

         // Data is shifted out MSb first. Because SDA is open-collector, a 0
         // bit is actively driven on SDA and a 1 bit tri-states SDA and lets
         // the pull-up resistor generate the high logic value.
         if(VAR(TX_Byte) & 0x80) {
            SET_DRIVE(SDA, false);            
         } else {
            SET_DRIVE(SDA, true);
            SET_LOGIC(SDA, 0);
         }
         VAR(TX_Byte) <<= 1;
         VAR(TX_Count)--;
         
         break;
         
      // Transmission finished; set SDA line back to tri-state for input
      case NTF_TXDONE:

         // Verify SCL is still low, otherwise a transition on SDA would
         // be interpreted as a START or STOP condition.
         if(GET_LOGIC(SCL) == 1) {
            Error("Clock on SCL pin changing too fast");
            return;
         }

         // If not all data transmitted, then it's a bug in the component
         if(VAR(TX_Count)) {
            BREAK("Internal error: (Pending data to TX)");
            return;
         }
         
         SET_DRIVE(SDA, false);
         VAR(TX_Active) = false;
         On_Tx();
         
         break;

   }
}

void On_gadget_notify(GADGET pGadgetId, int pCode)
//************************************************
// A window gadget (control) is sending a notification.
// Handles button/checkbox clicks in the GUI.
{
   switch(pGadgetId) {
      case GDT_BREAK:   VAR(Break) ^= 1; break;         
      case GDT_LOG:     VAR(Log) ^= 1; break;
      
      // TODO: Not implemented yet
      case GDT_VIEW:
      case GDT_LOAD:
      case GDT_SAVE:
      case GDT_ERASE:
         break;
   }
}

void On_update_tick(double pTime)
//************************************************
// Called periodically to re-draw the GUI. The GUI is only updated if
// VAR(Dirty) is true to indicate that internal state has changed
{
   if(VAR(Dirty)) {
      char strBuffer[16];      
      snprintf(strBuffer, 16, "$%05X", VAR(Pointer));
      SetWindowText(GET_HANDLE(GDT_ADDR), strBuffer);

      SetWindowText(GET_HANDLE(GDT_STATUS), State_text[VAR(State)]);
      VAR(Dirty) = false;
   }
}

void On_time_step(double pTime)
// TODO: DELETE THIS ONCE ON_DIGITAL_EDGE WORKS FOR SDA
{
   if(!GET_DRIVE(SDA) && GET_LOGIC(SDA) != VAR(SDA_State)) {
      VAR(SDA_State) = GET_LOGIC(SDA);
      On_digital_in_edge(SDA, VAR(SDA_State) ? RISE : FALL, pTime);
   }
}
