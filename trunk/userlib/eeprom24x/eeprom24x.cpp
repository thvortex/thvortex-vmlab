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
int WINAPI DllEntryPoint(HINSTANCE, unsigned long, void*) {return 1;} // is DLL
//==============================================================================

//==============================================================================
// Declare pins here
//
DECLARE_PINS
   DIGITAL_BID(SDA, 1);
   DIGITAL_IN(SCL, 2);
   DIGITAL_IN(WP, 3);
   ANALOG_IN(A2, 4);
   ANALOG_IN(A1, 5);
   ANALOG_IN(A0, 6);
END_PINS

// This is the delay time from a falling edge on the SCL line to a transition
// on the SDA line when the EEPROM is transmitting data. This delay is necessary
// so that other devices on the I2C bus do not mistake the changing data line for
// a START or STOP condition.
#define SCL_TO_OUT 300e-9

// Notify codes used with REMIND_ME(). NTF_TX shifts out the next data bit after
// a SCL falling edge. NTF_IDLE switches from the "Busy" state to the "Idle" state
// after an internal operation has finished. NTS_TXDONE is used on a SCL falling
// edge to signal the final bit in a transmission has been sent finished.
enum { NTF_TX, NTF_TXDONE, NTF_IDLE };

// Internal states. Need OTHER, BUSY_OTHER, BUSY

// =============================================================================
// Declare module global variables here.
// The reason for this is to keep a set of variables by instance
// To use a variable, do it through the the macro VAR(...)
//
DECLARE_VAR
   int Write_Count;    // Number of bytes received during sequential write
   char Page[128];     // Temporary buffer for EEPROM write data
   char Data[655346];  // Maximum data size allowed by 11 address bits

   bool TX_Active;     // True if a TX operation has begun and still in progress
   UCHAR RX_Byte;      // Receive data shifted in from SDA pin
   UCHAR TX_Byte;      // Transmit data waiting to shift out on SDA pin
   char RX_Count;      // Number of bits left to receive in current word
   char TX_Count;      // Number of bits left to transmit in current word
END_VAR

// You can delare also globals variable outside DECLARE_VAR / END_VAR, but if
// multiple instances of this cell are placed, all these instances will
// share the same variable.

// ============================================================================
// Say here if your component has an associated window or not. Pass as parameter
// the dialog resource ID (from .RC file) or 0 if no window
//
USE_WINDOW(0);   // If window USE_WINDOW(WINDOW_USER_1) (for example)

// =============================================================================
// Callback functions. These functions are called by VMLAB at the proper time

const char *On_create()
//********************
// Perform component creation. It must return NULL if the creation process is
// OK, or a message describing the error cause. The message will show in the
// Messages window. Typical tasks: check passed parameters, open files,
// allocate memory,...
{
   return NULL;
}

void On_window_init(HWND pHandle)
//*******************************
// Window initialization. Fill only if the component has an associated window
// -USE_WINDOW(..) not zero-. The Parameter pHandle brings the main component
// window handle. Typical tasks: fill controls with data, intitialize gadgets,
// hook an own Windows structure (VCL, MFC,...)
{

}

void On_destroy()
//***************
// Destroy component. Free here memory allocated at On_create; close files
// etc.
{

}

void On_simulation_begin()
//************************
// VMLAB informs you that the simulation is starting. Initialize pin values
// here Open files; allocate memory, etc.
{
   // EEPROM is ready to receive I2C slave address word upon power-up
   // TODO: The receive should be done after a START condition
   SET_DRIVE(SDA, false);
   VAR(TX_Active) = false;
   VAR(RX_Count) = 8;
   VAR(TX_Count) = 0;
}

void On_simulation_end()
//**********************
// Undo here the operations done at On_simulation_begin: free memory, close
// files, etc.
{

}

// Shortcuts for common operations on the I2C bus
inline void Tx(char pData) { VAR(TX_Count) = 8; VAR(TX_Byte) = pData; }
inline void Tx_Ack()       { VAR(TX_Count) = 1; VAR(TX_Byte) = 0; }
inline void Tx_Nak()       { VAR(TX_Count) = 1; VAR(TX_Byte) = 0x80; }
inline void Rx()           { VAR(RX_Count) = 8; VAR(RX_Byte) = 0; }
inline void Rx_Ack()       { VAR(RX_Count) = 1; VAR(RX_Byte) = 0; }
// TODO: Add bool IS_ACK(pData)

void On_Start()
//**********************
// Called by On_digital_in_edge() in response to START condition on I2C bus
{
   PRINT("START");
}

void On_Stop()
//**********************
// Called by On_digital_in_edge() in response to STOP condition on I2C bus
{
   PRINT("STOP");

   // TODO: Stop condition only allowed after 0 or 1 bit received   
   
   // EEPROM is ready to receive new I2C slave address word after STOP
   // TODO: STOP should halt all transmission; START sets up slave addr receive
   SET_DRIVE(SDA, false);
   VAR(TX_Active) = false;
   VAR(RX_Count) = 8;
   VAR(TX_Count) = 0;
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
      
   // TODO (write mode): Transmit ACK
   VAR(TX_Count) = 1;
   VAR(TX_Byte) = 0;
}

void On_Tx()
//**********************
// Called by On_remind_me() when the requested number of bits in VAR(TX_Count)
// has been shifted out from VAR(TX_Byte).
{
   // TODO (write mode): Setup to receive the next byte
   VAR(RX_Count) = 8;   

   PRINT("TX");
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
            
            // TODO: Temporary bit decoding
            char buf[64];
            snprintf(buf, 64, "RX[%d]=%d", VAR(RX_Count), GET_LOGIC(SDA));
            PRINT(buf);
            
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
            // TODO: Signal warning: timing violation
            BREAK("Timing violation on SCL");
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
            // TODO: Signal warning: timing violation
            BREAK("Timing violation on SCL");
            return;
         }

         // If not all data transmitted, then it's a bug in the component
         if(VAR(TX_Count)) {
            BREAK("Internal error: (Data left to TX)");
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
{

}
