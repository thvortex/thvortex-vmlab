// =============================================================================
// Component name: comxch v1.0
//
// These component allows the exchange of data between a physical COM port (i.e.
// RS-232 serial port) and a virtual UART in the simulated VMLAB environment.
//
// To use this component, use the following component definition:
//
// X<Port> _comxch(<Baud> [<Data> <Parity> <EvenParity> <Stop>]) <TX> <RX>
//
// X<Port> _comxchx(<Baud> [<Data> <Parity> <EvenParity> <Stop>]) <TX> <RX>
// + <CTS> <DSR> <RI> <DCD> <RTS> <DTR> <OUT1> <OUT2>
//
// The instance name <Port> identifies which serial port to use. It would normally
// be of the form COM<N> such as "COM1" or "COM3", although it can also be a
// non-standard name like "CNCA0" if using the com0com null-modem emulator. Any
// data that arrives on the COM port will be transmitted via the <TX> pin of the
// comxch component, and any data received at the <RX> pin will be sent out through
// the same COM port. The arguments to the comxch or comxchx component are in the
// same order and format as VMLAB's builtin TTY component, and they specify the
// data format used by both the physical COM port and the <TX> and <RX> pair of
// pins:
//
// <Baud>       : Required argument; any positive integer is allowed although a
//                physical COM port will only support standard rates.
// <Data>       : Number of data bits; must be 7 or 8; default is 7
// <Parity>     : 0 for no parity, 1 to use parity; default is 0
// <EvenParity> : 1 for even parity, 0 for odd; default is 0; ignored if
//                <Parity> argument is 0
// <Stop>       : Number of stop bits; must be 1 or 2; default is 1
//
// The comxchx component provides additional pins to get and set all of the
// auxiliary modem control lines of the RS-232 port. A logic 1 on any of the
// auxiliary pins corresponds to an asserted signal on the COM port. (i.e. a
// SPACING condition or a positive voltage on a physical port).
//
// The <CTS>, <DSR>, <RI>, and <DCD> output pins respectively mirror the state of
// the Clear To Send, Data Set Ready, Ring Indicator, and Data Carrier Detect input
// signals on the COM port. The <RTS> and <DTR> input pins respectively control the
// state of the Request To Send and Data Terminal Ready output signals on the COM
// port. The <OUT1> and <OUT2> input pins are for use only with the com0com virtual
// loopback driver; they allow for controlling of the Ring Indicator and Data Carrier
// Detect signals at the opposite COM port in the loopback. See the "ReadMe.txt" for
// the com0com driver for instructions on how to configure the OUT1 and OUT2 signals.
//
// The comxch component uses automatic RTS hardware flow control for the COM port
// input, but no flow control for the COM port output. The comxchx component uses no
// flow control in either direction.
//
// Version History:
// v1.0 01/27/09 - Detect COM port receiver overrun; added comxchx component
// v0.3 01/22/09 - Pass break/parity/framing errors from COM port to TX
// v0.2 01/18/09 - Better error reporting; detect break and framing errors on RX
// v0.1 12/25/08 - First working version released
// v0.0 02/09/06 - Original source posted by AMTools in VMLAB user forum
//
// Copyright (C) 2005 Advanced MicroControllers Tools (http://www.amctools.com/)
// Copyright (C) 2008,2009 Wojciech Stryjewski <thvortex@gmail.com>
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

// Naming style used in variables:
// ==============================
// Global_variable: Starts in uppercase, use underscore
// myLocalVariable: starts in lowercase, do not use underscore
// pMyParameter: passed parameters, starts with p, no underscore
// Global_function: with underscore, to distiguish from Win32 API: CreateFile()
// On_xxx: VMLAB handlers
// REMIND_ME: (all uppercase) VMLAB Interface Functions

// For reference about configuring and using serial ports, see:
//
// http://msdn.microsoft.com/en-us/library/aa363195(VS.85).aspx
//

#include <windows.h>
#include <commctrl.h>
#include <winioctl.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#pragma hdrstop
#include "C:\VMLAB\bin\blackbox.h"
int WINAPI DllEntryPoint(HINSTANCE, unsigned long, void*) {return 1;} // is DLL
//==============================================================================

// These defines are from ntddser.h of the Windows DDK. They are used with the
// com0com driver to control the auxiliary OUT1 and OUT2 serial port lines.
#define IOCTL_SERIAL_GET_MODEM_CONTROL  CTL_CODE(FILE_DEVICE_SERIAL_PORT,37,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_SERIAL_SET_MODEM_CONTROL  CTL_CODE(FILE_DEVICE_SERIAL_PORT,38,METHOD_BUFFERED,FILE_ANY_ACCESS)

// These defines are from cncext.h of the com0com driver sources.
#define C0CE_SIGNATURE              "c0c"
#define C0CE_SIGNATURE_SIZE         (sizeof(UCHAR)*4)

// If COMTRACE was defined at compile time, this macro will allow the statement
// in it's argument to execute. Otherwise, this contained statement is a no-op.
// Using this macro avoids having to put #ifdef/#endif statements throughtout
// the source file.
#ifdef COMTRACE
#define IF_COMTRACE(x) { x; }
#else
#define IF_COMTRACE(x)
#endif

// This macro checks the assertion that a Win32 API call returned succesfully.
// If the call failed, then this macro will print a detailed error message, it
// will close the COM port, it will break the simulation with the user specified
// error string, and then finally it will force the current function to return
// immediately.
#define WIN32_ASSERT(cond, error) \
   if(!(cond)) { Print_error(); Close_COM_port(); BREAK(error); return; }

// Size of temporary string buffer for generating filenames and error messages
#define MAXBUF 256

// Defines for REMIND_ME() fucntion 'pData' auxiliary parameter.
#define TX_END      1
#define RX_END      2
#define RX_DATA     3
#define RX_PARITY   4
#define RX_STOP     5

DECLARE_PINS
   DIGITAL_OUT(TX, 1);  
   DIGITAL_IN(RX, 2);   
#ifdef COMXCHX
   DIGITAL_OUT(CTS, 3);  
   DIGITAL_OUT(DSR, 4);  
   DIGITAL_OUT(RI, 5);  
   DIGITAL_OUT(DCD, 6);  
   DIGITAL_IN(RTS, 7);  
   DIGITAL_IN(DTR, 8);  
   DIGITAL_IN(OUT1, 9); 
   DIGITAL_IN(OUT2, 10);
#endif
END_PINS

DECLARE_VAR
   HANDLE Handle_port;    // Win32 handle to COMx port

   int Baud_rate;         // Baud rate as specified by user
   int Data_bits;         // Number of data bits from instance arguments
   int Stop_bits;         // Real number of stop bits as given by user and not DCB
   BOOL Parity;           // Parity flag from instance arguments
   BOOL Even_parity;      // Odd parity flag from instance arguments

   double Bit_time;       // Inverse of baudrate   
   BOOL COM_rx_overrun;   // Flag indicating a COM receive overrun occurred
   DWORD Prev_modem_stat; // Previous return from GetCommModemStatus()      

   BOOL TX_busy;          // Flag indicating transmission in progress
   BOOL TX_break;         // Flag indicating a break should be transmitted on TX
   BOOL TX_frame_error;   // Flag that a framing error should be generated on TX
   BOOL TX_parity_error;  // Flag that a framing error should be generated on TX

   BOOL RX_busy;          // Flag indicating reception in progress
   BYTE RX_byte;          // Where byte received is being stored
   LOGIC RX_parity;       // Where received parity bit is being stored
   int RX_stopbits;       // Where number of stop bits received is counted
   BOOL RX_break;         // Flag indicating if a break condition exists on RX
END_VAR

// Function prototypes, defined later
void Open_COM_port();     
void Close_COM_port();
           
BOOL Compute_parity(char pByte);
BOOL Get_RX();

void Set_com0com_control(ULONG pControl, ULONG pMask);

void Print_error();              
void Printf(char *pFormat, ...);
void Printf_byte(char *pHeader, BYTE pByte);

USE_WINDOW(0);

const char *On_create()
//********************
// Perform static checks on the instance like verifying some of the arguments,
// supplying default values for optional arguments, and collecting them into VAR()
// instance variables. We do not however try to open the serial port until the
// simulation has actually started, since that is a dynamic check which depends on
// factors outside of VMLAB (like the port being already opened).
{
   VAR(Baud_rate) = (int) GET_PARAM(1);
   if(VAR(Baud_rate) <= 0)  {
      return("<Baud> argument must be greater than zero");
   }
   VAR(Bit_time) = 1.0/(double)VAR(Baud_rate);

   VAR(Data_bits) = (int) GET_PARAM(2);
   if(VAR(Data_bits) == 0) VAR(Data_bits) = 7;   // Handle default
   if(VAR(Data_bits) != 7 && VAR(Data_bits) != 8) {
      return("Optional <Data> argument must be 7 or 8");
   }

   // Force the parity flag arguments to either 0 or 1
   VAR(Parity) = GET_PARAM(3) != 0;
   VAR(Even_parity) = GET_PARAM(4) != 0;

   VAR(Stop_bits) = (int) GET_PARAM(5);
   if(VAR(Stop_bits) == 0) VAR(Stop_bits) = 1;   // Handle default
   if(VAR(Stop_bits) != 1 && VAR(Stop_bits) != 2) {
      return("Optional <Stop> argument must be 1 or 2");
   }

   return NULL;
}

void On_window_init(HWND pHandle)
//*******************************
{
   // No Action
}

void On_destroy()
//***************
{
   // No Action
}

void On_simulation_begin()
//************************
// Open the port at start of simulation is cleaner than at component creation since
// it only uses the port when it's actually needed.
{
   // Initialize per instance variables
   VAR(COM_rx_overrun) = false;

   VAR(TX_busy) = false;
   VAR(TX_break) = false;
   VAR(TX_frame_error) = false;
   VAR(TX_parity_error) = false;
   
   VAR(RX_busy) = false;
   VAR(RX_byte) = 0;
   VAR(RX_break) = false;

   // Set TX pin to idle (mark) condition
   SET_LOGIC(TX, 1);

   Open_COM_port();
      
#ifdef COMXCHX
   // Read the initial values of the RTS, DTR, OUT1, OUT2 pins and set the modem
   // control lines since On_digital_in_edge() does not get called automatically
   // at the start of the simulation.
   On_digital_in_edge(RTS, GET_LOGIC(RTS) ? RISE : FALL, 0);
   On_digital_in_edge(DTR, GET_LOGIC(DTR) ? RISE : FALL, 0);
   On_digital_in_edge(OUT1, GET_LOGIC(OUT1) ? RISE : FALL, 0);
   On_digital_in_edge(OUT2, GET_LOGIC(OUT2) ? RISE : FALL, 0);
#endif
}

void On_simulation_end()
//**********************
{
   Close_COM_port();
}

void On_digital_in_edge(PIN pDigitalIn, EDGE pEdge, double pTime)
//**************************************************************
// Detect start bit and launch sampling events in the middle of the bit period
// At the end, depending on the nr. of bits and stop bits, launch a RX_END
// to indicate myself the end.
{
   DWORD commFunc;
   int rc;

   if(!VAR(Handle_port)) return; // No action if port failed to open

   switch(pDigitalIn) {

      case RX:

         // Rising edge on RX signals end of break condition
         if(VAR(RX_break) && pEdge == RISE) {
            IF_COMTRACE( Printf("RX <-- BREAK END") );
            VAR(RX_break) = false;

            rc = ClearCommBreak(VAR(Handle_port));
            WIN32_ASSERT(rc, "Error clearing break condition on COM port");
         }

         // Falling edge on RX is a start bit which initiates data reception
         else if(!VAR(RX_busy) && pEdge == FALL) {            
            int j;
            
            VAR(RX_busy) = true;
            VAR(RX_stopbits) = 0;

            // Schedule samples for every data bit
            for(j = 0; j < VAR(Data_bits); j++) {
               REMIND_ME(VAR(Bit_time) * (j + 1) + VAR(Bit_time) / 2, RX_DATA);
            }

            // If using parity, schedule a parity sampling
            if(VAR(Parity)) {
               REMIND_ME(VAR(Bit_time) * (j + 1) + VAR(Bit_time) / 2, RX_PARITY);
               j++;
            }

            // If using two stop bits, sample the first one separately
            if(VAR(Stop_bits) == 2) {
               REMIND_ME(VAR(Bit_time) * (j + 1) + VAR(Bit_time) / 2, RX_STOP);
               j++;
            }

            // Sample last (or only stop bit) and notify about end of reception
            REMIND_ME(VAR(Bit_time) * (j + 1) + VAR(Bit_time) / 2, RX_END);
         }
         
         break;
         
#ifdef COMXCHX         
      case RTS:
      
         IF_COMTRACE( Printf("RTS <-- %s", pEdge == RISE ? "ON" : "OFF") );

         commFunc = pEdge == RISE ? SETRTS : CLRRTS;                  
         rc = EscapeCommFunction(VAR(Handle_port), commFunc);
         WIN32_ASSERT(rc, "Error changing RTS control line on COM port");
         
         break;
         
      case DTR:
      
         IF_COMTRACE( Printf("DTR <-- %s", pEdge == RISE ? "ON" : "OFF") );
         
         commFunc = pEdge == RISE ? SETDTR : CLRDTR;
         rc = EscapeCommFunction(VAR(Handle_port), commFunc);
         WIN32_ASSERT(rc, "Error changing DTR control line on COM port");
         
         break;

      case OUT1:
      
         IF_COMTRACE( Printf("OUT1 <-- %s", pEdge == RISE ? "ON" : "OFF") );
         Set_com0com_control(pEdge == RISE ? -1 : 0, SERIAL_IOC_MCR_OUT1);
         break;
         
      case OUT2:
      
         IF_COMTRACE( Printf("OUT2 <-- %s", pEdge == RISE ? "ON" : "OFF") );         
         Set_com0com_control(pEdge == RISE ? -1 : 0, SERIAL_IOC_MCR_OUT2);
         break;

#endif
      default:
         break;
   }
}

void On_time_step(double pTime)
//*****************************
// Check here if a character is available from the reception queue. 
// This is invoked frequently from VMLAB, therefore, a timer-based
// hook function should't be necessary. The port is configured 
// not to wait on ReadFile.
{
   int rc;

   if(!VAR(Handle_port)) return; // No action if port failed to open

#ifdef COMXCHX
   // Check the status of the modem control lines.
   DWORD modemStatus;
   rc = GetCommModemStatus(VAR(Handle_port), &modemStatus);
   WIN32_ASSERT(rc, "Error querying COM port control lines");

   // If any control lines changed since the last time step, then change the state
   // of the corresponding output pin. Also check status at time step 0 to setup
   // the output pins with their initial values.
   if(pTime == 0 || (VAR(Prev_modem_stat) ^ modemStatus) & MS_CTS_ON) {
      IF_COMTRACE( Printf("CTS --> %s", modemStatus & MS_CTS_ON ? "ON" : "OFF") );
      SET_LOGIC(CTS, modemStatus & MS_CTS_ON ? 1 : 0);      
   }
   
   if(pTime == 0 || (VAR(Prev_modem_stat) ^ modemStatus) & MS_DSR_ON) {
      IF_COMTRACE( Printf("DSR --> %s", modemStatus & MS_DSR_ON ? "ON" : "OFF") );
      SET_LOGIC(DSR, modemStatus & MS_DSR_ON ? 1 : 0);      
   }
   
   if(pTime == 0 || (VAR(Prev_modem_stat) ^ modemStatus) & MS_RING_ON) {
      IF_COMTRACE( Printf("RI --> %s", modemStatus & MS_RING_ON ? "ON" : "OFF") );
      SET_LOGIC(RI, modemStatus & MS_RING_ON ? 1 : 0);      
   }
   
   if(pTime == 0 || (VAR(Prev_modem_stat) ^ modemStatus) & MS_RLSD_ON) {
      IF_COMTRACE( Printf("DCD --> %s", modemStatus & MS_RLSD_ON ? "ON" : "OFF") );
      SET_LOGIC(DCD, modemStatus & MS_RLSD_ON ? 1 : 0);      
   }
   
   VAR(Prev_modem_stat) = modemStatus;
#endif

   // No further action if already transmitting a character
   if(VAR(TX_busy)) return;

   // Check for break condition, buffer overrun, or parity/framing errors
   DWORD commErrors;
   rc = ClearCommError(VAR(Handle_port), &commErrors, NULL);
   WIN32_ASSERT(rc, "Error querying COM port for receiver errors");
   
   if(commErrors & CE_BREAK) {
      IF_COMTRACE( Printf("TX --> BREAK") );
      VAR(TX_break) = true;
   }

   if(commErrors & CE_FRAME) {
      IF_COMTRACE( Printf("TX --> FRAMING ERROR") );
      VAR(TX_frame_error) = true;
   }

   if(commErrors & CE_RXPARITY) {
      IF_COMTRACE( Printf("TX --> PARITY ERROR") );
      VAR(TX_parity_error) = true;
   }

   // If a receive buffer overrun has occurred on the COM port, it will continue
   // to occur until the other side stops transmitting. To prevent the user from
   // being flooded with warnings, we only print the warning once and then set a
   // flag. This flag is only cleared once all characters have been read out
   // from the COM port receive buffer and port is idle.
   if((commErrors & CE_OVERRUN) || (commErrors & CE_RXOVER)) {
      if(!VAR(COM_rx_overrun)) {
         Printf("COM port receive buffer overrun");
         VAR(COM_rx_overrun) = true;
      }
   }
   
   // Read the next byte from the port
   BYTE myByte;
   DWORD bytesTransferred;
   rc = ReadFile(              // Win32 API function
         VAR(Handle_port),    // Port handle got by CreateFile
         &myByte,             // Pointer to data to read
         1,                   // Number of bytes to read
         &bytesTransferred,   // Pointer to number of bytes actually read
         NULL                 // Must be NULL
   );
   WIN32_ASSERT(rc, "Error reading data from the COM port");

   // Launch the serial logic events to build the character, if received. When a
   // break condition occurs, the port will also read a zero (NULL) byte value as
   // data which we ignore. Unfortunately, there is no way under Win32 to detect
   // when the break condition ends, so the generated break signal will last only
   // for the duration of the start, data, parity, and stop bits.
   if(bytesTransferred == 1) {
   
      int j; // Bit index

      VAR(TX_busy) = true;

      // Transmit start bit immediately
      SET_LOGIC(TX, 0);

      // If generating a break condition, ignore the received zero byte. Set j
      // equal to the number of data bits + parity bit because this will determine
      // the length of the break.
      if(VAR(TX_break)) {
         j = VAR(Data_bits);       // Count data bits
         j += VAR(Parity) ? 1 : 0; // Count parity bit if using
      }

      // If not a break, then transmit a normal byte, parity, stop bits
      else {
         int myParity = Compute_parity(myByte);

         IF_COMTRACE( Printf_byte("TX -->", myByte) );

         // Transmit all of the data bits. Afterwards, j will equal to the number
         // of data bits transmitted.
         for(j = 0; j < VAR(Data_bits); j++) {
            SET_LOGIC(TX, myByte & 0x01, VAR(Bit_time) * (1 + j));
            myByte >>= 1;
         }

         // Transmit parity bit if using even or odd parity. If a parity error was
         // detected on the COM port, re-transmit the error by inverting what the
         // parity bit should be.
         if(VAR(Parity)) {
            myParity = VAR(TX_parity_error) ? !myParity : myParity;
            SET_LOGIC(TX, myParity, VAR(Bit_time) * (1 + j));    
            j++;
         }

         // Raise the Stop Bit, but only if not transmitting a framing error
         if(!VAR(TX_frame_error)) {
            SET_LOGIC(TX, 1, VAR(Bit_time) * (1 + j)); // Raise stop bit
         }
      }
      
      // notify to myself the end of the transmission
      REMIND_ME(VAR(Bit_time) * (1 + j + VAR(Stop_bits)), TX_END);
      
      // Ensure the line is idle (high) after stop bits. This is necessary in case
      // a break or a framing error is being transmitted, since in both cases
      // the stop bits would remain low.
      SET_LOGIC(TX, 1, VAR(Bit_time) * (1 + j + VAR(Stop_bits)));
   }
   
   // If ReadFile() returned no bytes, then the COM port receive buffer must be
   // empty. Reset the flag so that a future receive buffer overrun can be reported
   // again.
   else {   
      VAR(COM_rx_overrun) = false;      
   }
}

void On_remind_me(double pTime, int pData)
//***************************************
// VMLAB notifies about a previouly sent REMIND_ME() function.
// Used to pass delayed notifications.
{
   int rc;

   if(!VAR(Handle_port)) return; // No action if port failed to open

   if(pData == TX_END) {          // End of transmission 

      VAR(TX_busy) = false;
      VAR(TX_break) = false;
      VAR(TX_frame_error) = false;
      VAR(TX_parity_error) = false;

   } else if(pData == RX_END) {   // End of reception, passes character to real COM

      // Sample the last (or only) stop bit
      VAR(RX_stopbits) += Get_RX();

      // If data, parity, and stop bits are all zero then it's a break condition
      if(!VAR(RX_byte) && !VAR(RX_parity) && !VAR(RX_stopbits)) {
         IF_COMTRACE( Printf("RX <-- BREAK START") );         
         VAR(RX_break) = true;

         rc = SetCommBreak(VAR(Handle_port));
         WIN32_ASSERT(rc, "Error setting break condition on COM port");
      }

      // Otherwise, this must be normal data (at least one bit was high)
      else {

         IF_COMTRACE( Printf_byte("RX <--", VAR(RX_byte)) );

         // Check for the correct number of received stop bits
         if(VAR(RX_stopbits) != VAR(Stop_bits)) {
            BREAK("Framing error at RX pin (bad stop bits)");
         }      

         // Do an extra shift if using 7 bits mode
         if(VAR(Data_bits) == 7) 
            VAR(RX_byte) >>= 1;

         // If using parity, verify the received parity bit was correct
         if(VAR(Parity)) {
            LOGIC myParity = Compute_parity(VAR(RX_byte));
            if(myParity != VAR(RX_parity)) {
               BREAK("Bad parity bit at RX pin");
            }
         }

         DWORD numBytesWritten;
         WriteFile(           // Win32 function
            VAR(Handle_port), // Port Win32 handle
            &VAR(RX_byte),    // Pointer to the data to write 
            1,                // Number of bytes to write
            &numBytesWritten, // Pointer to the number of bytes written
            NULL              // Must be NULL
         );
         WIN32_ASSERT(numBytesWritten == 1, "Error writting data to the COM port");
      }
      
      VAR(RX_busy) = false;
      VAR(RX_byte) = 0;  

   } else if(pData == RX_DATA) {   // Sampling data bit, bits come first LSB.

      VAR(RX_byte) >>= 1;
      LOGIC bitSample = Get_RX();
      VAR(RX_byte) |= (bitSample << 7);
      
   } else if(pData == RX_PARITY) { // Sampling parity bit

      VAR(RX_parity) = Get_RX();

   } else if(pData == RX_STOP) {   // Sample first stop bit (if using 2)
   
      VAR(RX_stopbits) += Get_RX();

   }
}

void On_gadget_notify(GADGET pGadgetId, int pCode)
//************************************************
// A window gadget (control) is sending a notification.
{
   // No Action
}

void Open_COM_port()
//*************************
// Win32 uses the standard CreateFile(..) function to get a handle
// to a serial port. It manages automatically buffering. Parameters
// as recommended my MS
{
   char strBuffer[MAXBUF];
   DCB dcb; // Win32 structure for configuring COM ports
   int rc;

   // To support the com0com (See: http://com0com.sourceforge.net) virtual loop
   // back drivers, an instance name such as "CNCA0" will open a file named
   // "\\.\CNCA0". This also works for opening physical ports (e.g. "\\.\COM1").
   snprintf(strBuffer, MAXBUF, "\\\\.\\%s", GET_INSTANCE());

   VAR(Handle_port) = CreateFile(   // Win32 function
      strBuffer,                    // The instance name must be the port name "COM1", etc
      GENERIC_READ | GENERIC_WRITE, // Access (read-write) mode
      0,                            // Share mode
      NULL,                         // Pointer to the security attribute
      OPEN_EXISTING,                // How to open the serial port
      0,                            // Port attributes
      NULL                          // Handle to port with attribute to copy
   );
   WIN32_ASSERT(VAR(Handle_port) != INVALID_HANDLE_VALUE,
      "Unable to open requested COM port");

   // Initialize serial port parameters structure and load default parameters
   dcb.DCBlength = sizeof(DCB);
   rc = GetCommState(VAR(Handle_port), &dcb);
   WIN32_ASSERT(rc, "Unable to read serial port configuration");

   int msParity = NOPARITY;
   if(VAR(Parity)) {
      msParity = VAR(Even_parity) ? EVENPARITY : ODDPARITY;
   }

   // The Win32 structure uses integers 0, 1, and 2 to respectively specify
   // 1, 1.5, and 2 stop bits. The <stopBits> argument of the TTY component
   // uses the values 1 and 2 to respectively specify 1 and 2 stop bits.
   int msStopBits = VAR(Stop_bits);
   if(msStopBits == 1) msStopBits = 0;

   // Change the Win32 DCB structure settings to user defined ones. Since VMLAB is
   // slower than the physical port, it's much more likely that we loose data on
   // reception so we use RTS flow control for input. If the remote device doesn't
   // use RTS, then it causes no harm other than a potential for lost characters
   // due to no flow control. We don't use CTS for output control just in case the
   // remote device doesn't support it and leaves it in the low state, which would
   // prevent VMLAB from sending any data. Since VMLAB is slower than the physical
   // port, it's unlikely that CTS flow control would be needed in the first place.
   // However, if compiled as comxchx, then RTS flow control is disabled since the
   // RTS line is controlled directly through an input pin.
   dcb.BaudRate = VAR(Baud_rate);  // Current baud                      
   dcb.fBinary = TRUE;             // Binary mode; no EOF check         
   dcb.fParity = VAR(Parity);      // Enable parity checking            
   dcb.fOutxCtsFlow = FALSE;       // No CTS output flow control        
   dcb.fOutxDsrFlow = FALSE;       // No DSR output flow control        
   dcb.fDsrSensitivity = FALSE;    // DSR sensitivity                   
   dcb.fTXContinueOnXoff = TRUE;   // XOFF continues Tx                 
   dcb.fOutX = FALSE;              // No XON/XOFF out flow control      
   dcb.fInX = FALSE;               // No XON/XOFF in flow control       
   dcb.fErrorChar = FALSE;         // Disable error replacement         
   dcb.fNull = FALSE;              // Disable null stripping            
   dcb.fAbortOnError = FALSE;      // Do not abort reads/writes on error
   dcb.ByteSize = VAR(Data_bits);  // Number of bits/byte, 4-8          
   dcb.Parity = msParity;          // 0-4=no,odd,even,mark,space        
   dcb.StopBits = msStopBits;      // 0,1,2 = 1, 1.5, 2                 
#ifdef COMXCHX
   dcb.fDtrControl = DTR_CONTROL_DISABLE;   // DTR initially off
   dcb.fRtsControl = RTS_CONTROL_DISABLE;   // RTS initially off
#else
   dcb.fDtrControl = DTR_CONTROL_ENABLE;    // DTR on during connection
   dcb.fRtsControl = RTS_CONTROL_HANDSHAKE; // RTS input flow control
#endif

   // Configure the port according to the specifications of the DCB structure
   rc = SetCommState(VAR(Handle_port), &dcb);
   WIN32_ASSERT(rc, "Unable to configure serial port");

   // Configure the port to read/write immediately, even if no  character is
   // received, otherwise VMLAB is hanged waiting for a character to come.
   COMMTIMEOUTS myTOut; // Win32 struct
   myTOut.ReadIntervalTimeout = MAXDWORD;
   myTOut.ReadTotalTimeoutMultiplier = 0;
   myTOut.ReadTotalTimeoutConstant = 0;
   myTOut.WriteTotalTimeoutMultiplier = 0;
   myTOut.WriteTotalTimeoutConstant = 0;

   rc = SetCommTimeouts(VAR(Handle_port), &myTOut);
   WIN32_ASSERT(rc, "Unable to configure serial port timeouts");
}

void Close_COM_port()
//*******************
{
  if(VAR(Handle_port) != INVALID_HANDLE_VALUE) {
      if(!CloseHandle(VAR(Handle_port))) {
         Print_error();
         BREAK("Unable to close COM port");
      }
      VAR(Handle_port) = INVALID_HANDLE_VALUE;
   }   
}

void Set_com0com_control(ULONG pControl, ULONG pMask)
//**************************************************
// When using a com0com loopback COM port, this function will change the state of
// either the OUT1 or OUT2 modem control line. The pMask is a bitmask to specify
// which line is changing and pControl is a bitmask that gives the new state.
// We don't check the DeviceIoControl() Win32 API for success since we expect it
// to fail on COM ports not created by com0com.
{
  DWORD bytesReturned; // Not used but DeviceIoControl() requires it
  BYTE inBufIoctl[sizeof(ULONG) * 2 + C0CE_SIGNATURE_SIZE];
  
  ((ULONG *)inBufIoctl)[0] = pControl;
  ((ULONG *)inBufIoctl)[1] = pMask;
  memcpy(&((ULONG *)inBufIoctl)[2], C0CE_SIGNATURE, C0CE_SIGNATURE_SIZE);

  DeviceIoControl(VAR(Handle_port),               // hDevice (open COM port handle)
                  IOCTL_SERIAL_SET_MODEM_CONTROL, // dwIoControlCode
                  inBufIoctl,                     // lpInBuffer
                  sizeof(inBufIoctl),             // nInBufferSize
                  NULL,                           // lpOutBuffer
                  0,                              // nOutBufferSize
                  &bytesReturned,                 // lpBytesReturned
                  NULL                            // lpOverlapped
  );
}

double On_voltage_ask(PIN pAnalogOut, double pTime)
//**************************************************
// Return voltage as a function of Time for analog outputs that must behave
// as a continuous time wave.
// SET_VOLTAGE(), SET_LOGIC()etc. not allowed here. Return KEEP_VOLTAGE if
// no changes. Use pin identifers as declared in DECLARE_PINS
{
   // VMLAB 3.14A has a bug that requires the On_voltage_ask() function to
   // exist in a component that calls SET_LOGIC() with a non-zero delay.
   return KEEP_VOLTAGE;
}

void Print_error()
//*************************
// This function is called if any Windows API call has failed. It uses the
// GetLastError() API to retrieve an extended error code value, calls
// FormatMessage() to produce a human readable error message, and then calls
// PRINT() to display the error message returned by FormatMessage().
{
   DWORD errorValue, bytesWritten;
   char *msgPtr = NULL;

   errorValue = GetLastError();   
   bytesWritten = FormatMessage(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
      NULL,            // lpSource (ignored for system messages)
      errorValue,      // dwMessageId (return value from GetLastError)
      0,               // dwLanguageId (0 means system specified)
      (LPTSTR)&msgPtr, // lpBuffer (returns pointer to allocated buffer)
      0,               // nSize (no minimum size specified for allocated buffer)
      NULL             // Arguments (no va_list arguments needed)
   );
   
   // Print message if it was succesfully retrieved or print the numeric error
   // code if no message could be found.
   if(bytesWritten) {
      Printf(msgPtr);
      LocalFree(msgPtr);
   } else {
      Printf("Unknown system error: %d", errorValue);
   }
}

BOOL Compute_parity(char pByte)
//*************************
// Based on the current number of data bits and parity type (even or odd),
// compute the parity bit that corresponds to the number of data bits in
// "byte".
{
   int i, totalOnes = 0;
   
   for(i = 0; i < VAR(Data_bits); i++) {
      totalOnes += (pByte & 0x01);
      pByte >>= 1;
   }
   
   // When using parity, the total number of data & parity bits set to 1 must
   // be even or odd. Example: if using odd parity and the data byte already
   // has an odd number of ones, then the parity bit is transmitted as zero.
   if(VAR(Even_parity))
      return totalOnes & 0x01;
   else
      return !(totalOnes & 0x01);
}

BOOL Get_RX()
//*************************
// Read the logic value at the RX pin and BREAK() with an error if an UNKNOWN
// 'X' value is read at the RX pin.
// TODO: Need ANALOG RX pin to detect UNKNOWN values
{
   LOGIC bitSample = GET_LOGIC(RX);

   if(bitSample == UNKNOWN) {
      bitSample = 0;  // Put a known value but give a warning
      BREAK("Sampled logic 'X' at RX pin");
   }

   return bitSample;
}

void Printf(char *pFormat, ...)
//*************************
// Wrapper around the PRINT() function to provide printf() like functionality
// This function also automatically prepends the instance name to the formatted
// message.
{
   char strBuffer[MAXBUF];
   va_list argList;

   snprintf(strBuffer, MAXBUF, "%s: ", GET_INSTANCE());
   int len = strlen(strBuffer);
   
   va_start(argList, pFormat);
   vsnprintf(strBuffer + len, MAXBUF - len, pFormat, argList);
   va_end(argList);
   
   PRINT(strBuffer);
}

void Printf_byte(char *pHeader, BYTE pByte)
//*************************
// PRINT() a message showing what byte value was transmitted or received on the
// TX or RX pin. The function takes care of checking if the pByte is printable
// or not, and it also adds the pHeader string to the printed message.
{
   if(isprint(pByte)) {
      Printf("%s '%c', 0x%0X", pHeader, pByte, pByte);
   } else {
      Printf("%s 0x%0X", pHeader, pByte);
   }
}
