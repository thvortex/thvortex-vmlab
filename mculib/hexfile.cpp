// Utility class for displaying, loading, and saving bainary data in a char
// array. For saving and loading, this class supports the following file
// types: raw binary, Intel HEX, Motorola S-REC, and Atmel Generic. This
// class also makes use of the ShineInHex.dll to create a MDI child
// hex edit window to allow displaying and modifying of the binary data.
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
//
// The ShineInHex.dll control used by this class was written by Ramunas Cvirka
// <ramguru_zo@yahoo.com>. The author's original website is no longer available
// (http://www.stud.ktu.lt/~ramcvir/ShineInHex/) however I was able to get in
// touch within him by email to verify that the control can be used for any
// purpose. The original "SIH_final_14F.zip" file containing the dll, source,
// and documentation can be download from the MASM Forum at:
// http://www.masm32.com/board/index.php?topic=3671.msg27404
//

#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#pragma hdrstop
#include "hexfile.h"

// =============================================================================
// Global Constants and Macros
// =============================================================================

// Return the smaller of two values
#define MIN(a, b) ((a) < (b) ? (a) : (b))

// Size of temporary string buffer for generating filenames and error messages
#define MAXBUF 256

// Window class name used internally for the MDI child window which contains
// the SHINEINHEX child window class
#define CLASS_NAME "VMLAB Hexfile Editor"

// String assigned to the lpstrFilter field of the OPENFILENAME structure.
// Used by the "Open" and "Save" dialog boxes to display the pull-down list of
// supported file types. These strings must be in the same order as the public
// enum in hexfile.h so so that the return value from GetOpenFileName() and
// GetSaveFileName() functions directly corresponds to the enum.
#define OPENFILENAME_FILTER \
   "Intel HEX (*.eep; *.hex)\0*.eep;*.hex\0" \
   "Motorola S-Record (*.s19)\0*.s19\0" \
   "Atmel Generic 16/8 (*.gen)\0*.gen\0" \
   "Raw Binary (*.*)\0*.*\0"

// Error message displayed when loading memory image from larger EEPROM
#define FILEERROR_TOOBIG \
   "File uses higher addresses than supported by current EEPROM\n" \
   "memory size. Data beyond the end of memory was ignored."

// Error message displayed if any bad checksums were detected in HEX/SREC file
#define FILEERROR_CHECKSUM \
   "Checksum mismatches detected. Data in file may be corrupt."

// Warning displayed if writing more than 65536 bytes to Atmel generic file
#define FILEWARN_GENERICBIG \
   "Atmel Generic file type only supports 16-bit addresses.\n" \
   "Data beyond $FFFF memory address was not written to file."
   
// Confirmation question displayed when the erase() function is called
#define CONFIRM_ERASE \
   "Are you sure you want to erase entire EEPROM memory to $FF?"
   
// Initial size of MDI child window (not client area)
enum { INIT_WIDTH = 629, INIT_HEIGHT = 305 };

// Minimum allowed size of MDI child window (not client area)
enum { MIN_WIDTH = 250, MIN_HEIGHT = 64 };

// Window messages supported by hex editor control (no .h file available)
enum {
   HEXM_SETFONT          = WM_USER+100,
   HEXM_SETOFFSETGRADCOL = WM_USER+101,
   HEXM_SETHEADERGRADCOL = WM_USER+102,
   HEXM_SETVIEW1TEXTCOL  = WM_USER+103,
   HEXM_SETHEADERTEXTCOL = WM_USER+104,
   HEXM_SETVIEW2COL      = WM_USER+105,
   HEXM_SETVIEW3COL      = WM_USER+106,
   HEXM_SETVIEW2SELCOL   = WM_USER+107,
   HEXM_SETVIEW3SELCOL   = WM_USER+108,
   HEXM_SETACTIVECHARCOL = WM_USER+109,
   HEXM_SETMODBYTES1COL  = WM_USER+110,
   HEXM_SETMODBYTES2COL  = WM_USER+111,
   HEXM_SETMODBYTES3COL  = WM_USER+112,
   HEXM_SETPOINTER       = WM_USER+113,
   HEXM_UNSETPOINTER     = WM_USER+114,
   HEXM_SETOFFSET        = WM_USER+115,
   HEXM_SETSEL           = WM_USER+116,
   HEXM_UNDO             = WM_USER+117,
   HEXM_REDO             = WM_USER+118,
   HEXM_CANUNDO		    = WM_USER+119,
   HEXM_CANREDO          = WM_USER+120,
   HEXM_SETREADONLY      = WM_USER+121,
};

// =============================================================================
// Global Variables
// =============================================================================

// Global variables shared by all Hexfile objects; initialized by first init()
HMODULE VMLAB_module = NULL; // Handle to VMLAB.EXE for icon loading
HMODULE Library = NULL;      // Loaded ShineInHex.dll library
HWND VMLAB_window = NULL;    // Top level window that owns all dialog boxes
HWND MDI_client = NULL;      // MDI client window inside VMLAB window
ATOM MDI_class = NULL;       // Window class of MDI_child window
int Ref_count = 0;           // Reference count for library/class unloading

// =============================================================================
// Error Handling and Reporting Functions
// =============================================================================

// This macro checks the assertion that a Win32 API call returned succesfully.
// If the call failed, then this macro will display a message box with detailed
// error information.
#define W32_ASSERT(cond) if((cond) == 0) \
   { W32_Error("W32_ASSERT( "#cond" );", __FILE__, __LINE__); }

void W32_Error(char *pCode, char *pFile, int pLine)
//*************************
// This function is called if any Windows API call has failed. It uses the
// GetLastError() API to retrieve an extended error code value, calls
// FormatMessage() to produce a human readable error message, and then calls
// MessageBox() to display the error message returned by FormatMessage().
// The PRINT() interface function cannot be used here because this may be
// called from the DLL static constructor when VMLAB is not ready yet to handle
// any interface function calls from the components.
// TODO: Use second FormatMessage() pr sprintf() to display pFile and pLine
{
   DWORD errorValue, bytesWritten;
   char *msgPtr = NULL;

   errorValue = GetLastError();   
   bytesWritten = FormatMessage(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | // dwFlags (allocate string buffer)
      FORMAT_MESSAGE_FROM_SYSTEM |     // dwFlags (use system message table)
      FORMAT_MESSAGE_IGNORE_INSERTS,   // dwFlags (no va_list passed in)
      NULL,            // lpSource (ignored for system messages)
      errorValue,      // dwMessageId (return value from GetLastError)
      0,               // dwLanguageId (0 means system specified)
      (LPTSTR)&msgPtr, // lpBuffer (returns pointer to allocated buffer)
      0,               // nSize (no minimum size specified for allocated buffer)
      NULL             // Arguments (no va_list arguments needed)
   );
  
   // Display message box if the message was succesfully retrieved or print a
   // generic error if no message could be found. The VMLAB_window could be
   // NULL but that won't cause any major problems here.
   if(bytesWritten) {
      MessageBox(VMLAB_window, msgPtr, pCode, MB_OK | MB_ICONERROR);
      LocalFree(msgPtr);
   } else {
      MessageBox(VMLAB_window, "An unknown Win32 error has occurred.", pCode,
         MB_OK | MB_ICONERROR);
   }
}

void File_Error(const char *pFile, UINT pType, const char *pFormat, ...)
//*************************
// Display a message box window with a printf() like error message related
// to the EEPROM memory image file with path "pFile". Paramemter "pType" is
// passed to MessageBox() to select the type of icon displayed.
{
   char strBuffer[MAXBUF];
   va_list argList;

   // The first line in message box is the file path.
   int len = sprintf(strBuffer, "File: \"%s\"\n\n", pFile);
   
   // The second (and subsequent lines) are specified by user
   va_start(argList, pFormat);
   vsnprintf(strBuffer + len, MAXBUF - len, pFormat, argList);
   va_end(argList);
   
   // Display the message box
   MessageBox(
      VMLAB_window,        // hWnd (parent window for message box)
      strBuffer,           // lpText (text displayed inside message box)
      "EEPROM File Error", // lpCaption (title string for message box window)
      pType                // uType (type of icon displayed)
   );
}

// =============================================================================
// Internal Helper Functions
// =============================================================================

int Sum(int pValue)
//********************
// Return the sum obtained by adding the individual bytes of the pValue
// integer together. Used for checksum calculations when reading and writing
// memory image files.
{
   int sum = 0;   
   
   for(int i = 0; i < 4; i++) {
      sum += pValue & 0xFF;
      pValue >>= 8;
   }   

   return sum;
}

void Hide(HWND pWindow)
//******************************
// This function is called from Hexfile::hide(void) and from MDI_proc()
// (on WM_SYSCOMMAND/SC_CLOSE) to perform the real work of hiding the MDI
// child window.
// TODO: Merge into single Show(HWND, bool) and ::show(bool)
{
   // Hide this MDI child window, active the next MDI child, and request that
   // the MDI client refreshes the "Windows" menu in the top-most VMALB window
   // to remove the hidden MDI child.
   ShowWindow(pWindow, SW_HIDE);
   SendMessage(MDI_client, WM_MDINEXT, 0, 0);
   SendMessage(MDI_client, WM_MDIREFRESHMENU, 0, 0);
   W32_ASSERT( DrawMenuBar(VMLAB_window) );   
}

LRESULT CALLBACK MDI_proc(HWND pWindow, UINT pMessage, WPARAM pW, LPARAM pL)
//******************************
// Custom window procedure for the MDI child window. Because MDI child
// procedures have to call DefMDIChildProc() instead of DefWindowProc() for
// unhandled messages, the SHINEINHEX window class cannot be created directly
// as a child window of the global MDI frame. Instead, this custom MDI child
// window acts as a container for the SHININHEX window and takes care of any
// MDI specific message handling.
{
   HWND child;

   switch(pMessage) {
      // System menu (close/minimize/maximise) command. Closing the window
      // (SC_CLOSE) will merely hide it instead of destryoing it. All other
      // system commands are passed through.
      case WM_SYSCOMMAND:
      {
         if(pW == SC_CLOSE) {
            Hide(pWindow);
            return true;
         }
         break;
      }

      // Returns the minimum size that the user can drag the window to. Since
      // the hex editor control doesn't draw properly at small sizes, a minimum
      // width has to be enforced here. For consistency, a minimum height is
      // also enforced which guarantees at least one row of data is visible.
      case WM_GETMINMAXINFO:
      {
         MINMAXINFO *info = (MINMAXINFO *) pL;
         info->ptMinTrackSize.x = MIN_WIDTH;
         info->ptMinTrackSize.y = MIN_HEIGHT;
         return true;
      }
      
      // If the MDI window changed size then update hex editor size to
      // always fill entire client area of MDI window. Note that the first
      // WM_SIZE passed to MDI_proc is before the HEX_child window exists
      // yet.
      case WM_SIZE:
      {
         HWND child = GetWindow(pWindow, GW_CHILD);
         if(child) {
            W32_ASSERT( MoveWindow(child, 0, 0, LOWORD(pL), HIWORD(pL), true) );
         }
         break;
      }
      
      // If MDI client has gained keyboard focus, then pass the focus to the
      // child hex editor. Using WM_MDIACTIVATE is not good enough here
      // because the latter message is not sent if only the parent MDI frame
      // is activated, but the active MDI child within the MDI frame has not
      // changed.
      case WM_SETFOCUS:
      {
         HWND child = GetWindow(pWindow, GW_CHILD);
         if(child) {
            SendMessage(child, WM_SETFOCUS, 0, 0);
         }
         break;
      }

      default:
         break;
   };

   // All other messages must be passed to default MDI child window procedure
   return DefMDIChildProc(pWindow, pMessage, pW, pL);
}

// =============================================================================
// Helper Classes
// =============================================================================

class File
//*********
// Wrapper class around the stdio fopen(), fprintf(), fwrite(), etc. series of
// functions. All member functions take care of performing runtime checking for
// any possible I/O errors. If an error occurs, call File_Error() to notify the
// user and the File::Error exception is thrown. Using exceptions for error
// handling makes the mainline code simpler since it doesn't have to check
// the return code of each operation to determine success or failure. However,
// no exception is thrown when closing the file, because it's not safe to
// do so from the destructor. Based on the pEofWanted constructor argument,
// input routines like scanf() and read() will either return FALSE or throw
// an exception.
// TODO: The C++ iofstream could be used in place of this custom File class
// since it too can throw exceptions on I/O, parse, and EOF errors.
{
   private:
      const char *Name;  // File name passed to fopen(); for error msgs
      bool EOF_Wanted;   // True if EOF returns FALSE instead of throwing
      FILE *File;        // The open file pointer returned by fopen()

      void error(const char *pError, bool pThrow = true, UINT pType = MB_ICONSTOP);
      
   public:
      class Error {};    // Thrown if any I/O errors occur in File class

      File(const char *pName, const char *pMode, bool pEofWanted = false);      
      ~File();
      
      void printf(const char *pFormat, ...);
      bool scanf(int pExpectedCount, const char *pFormat, ...);
      bool read(char *pData, int pLength);
      void write(const char *pData, int pLength);
};

void File::error(const char *pError, bool pThrow, UINT pType)
//********************************
// Used by internal functions to display errors/warnings as a pop-up dialog
{
   // If error() is called due to end-of-file or a parsing error
   // then errno == 0 since no actual I/O error has occurred
   if(errno) {
      File_Error(Name, pType, "%s: %s", pError, strerror(errno));
   } else {
      File_Error(Name, pType, "%s", pError);
   }

   if(pThrow) {
      throw Error();
   }
}

File::File(const char *pName, const char *pMode, bool pEofWanted) :
   File(NULL), Name(pName), EOF_Wanted(pEofWanted)
//********************************
// Constructor to open "pName" file using "pMode" for access. If
// the "pEofWanted" argument is true, then the scanf() and read()
// functions return false when an EOF is reached instead of throwing
// an exception.
{
   File = ::fopen(pName, pMode);
   if(!File) {
      error("Cannot open file");
   }
}

File::~File()
//********************************
// Destructor to close the file. If an error occurs while closing (e.g.
// disk full while flushing the stdio buffers), an exception is NOT
// thrown since doing so while C++ is handling another exception and
// unwinding the stack is not supported in C++ (process terminates).
{
   if(File && fclose(File)) {
      error("Error while closing file", false);
   }
}

bool File::scanf(int pExpectedCount, const char *pFormat, ...)
//********************
// Wrapper around the fvscanf() function
{
   va_list argList;

   va_start(argList, pFormat);
   int actualCount = vfscanf(File, pFormat, argList);
   va_end(argList);
   
   if(actualCount != pExpectedCount) {
      if(ferror(File)) {
         error("Cannot read from file");
      } else if(feof(File)) {
         if(EOF_Wanted) {
            return false;
         } else {
            error("Unexpected end-of-file", true, MB_ICONWARNING);
         }
      } else {
         error("Unrecognized data in file; unknown file type");
      }            
   }
   
   return true;
}

void File::printf(const char *pFormat, ...)
//********************
// Wrapper around the vfprintf() function
{
   va_list argList;

   va_start(argList, pFormat);
   vfprintf(File, pFormat, argList);
   va_end(argList);

   if(ferror(File)) {
      error("Cannot write to file");
   }
}

bool File::read(char *pData, int pLength)
//********************
// Wrapper around the fread() function
{
   int actualCount = fread(pData, 1, pLength, File);

   if(actualCount != pLength) {
      if(ferror(File)) {
         error("Cannot read from file");
      } else if(feof(File)) {
         if(EOF_Wanted) {
            return false;
         } else {
            error("Unexpected end-of-file", true, MB_ICONWARNING);
         }
      } else {
         error("Internal error; unknown fread() error");
      }            
   }
   
   return true;
}

void File::write(const char *pData, int pLength)
//********************
// Wrapper around the fwrite() function
{
   fwrite(pData, 1, pLength, File);

   if(ferror(File)) {
      error("Cannot write to file");
   }
}

// =============================================================================
// Memory image saving functions
// =============================================================================

void Write_BIN(UCHAR *pBuffer, const int pSize, const char *pName)
//********************
// Write full EEPROM memory contents to a raw binary file. The file will be
// the same size as the EEPROM memory.
{
   // Open file for writing in binary mode. The "b" specifier is needed so the
   // stdio library does not try to translate "\n" characters written to the
   // file into the "\r\n" line endings used by Windows.
   File file(pName, "wb");
   file.write(pBuffer, pSize);
}

void Write_HEX(UCHAR *pBuffer, const int pSize, const char *pName)
//********************
// Write EEPROM memory contents to an Intel HEX file. Sections of memory set
// to $FF are omitted from the file since this is the default value. EEPROMs
// larger than 16-bits will use the "Extended Linear Address" record.
{
   // The upper 16 bits of the last EEPROM address written to file. Used to
   // detect when the "Extened Segment Address" should be written to file.
   int segment = 0;

   // Open file for writing in text mode. The "t" specifier is needed for the
   // stdio library to translate "\n" characters written to the file into the
   // "\r\n" used by Windows.
   File file(pName, "wt");

   // Write or skip over EEPROM contents one row (16 bytes) at a time.
   for(int addr = 0; addr < pSize; addr += 0x10) {
      int rowAddr = (addr + 0x10 < pSize) ? (addr + 0x10) : pSize;
      int count = rowAddr - addr;
      UCHAR checksum;
      int i;
   
      // If the entire row contains $FF bytes then don't write it to file
      for(i = addr; i < rowAddr; i++) {
         if(pBuffer[i] != 0xFF) break;
      }
      if(i == rowAddr) continue;

      // If the current "addr" has crossed into the next 16-bit boundry,
      // then write an "Extended Linear Address" record. The checksum
      // below is calculated as: 02 (for record byte count) + 04 (record
      // type) + the sum of the individual "segment" bytes. Note that
      // the two's complement used for the checksum is the same as negation.
      if(addr >> 0x10 != segment) {         
         segment = addr >> 0x10;
         checksum = -(Sum(segment) + 0x06);
         file.printf(":02000004%04X%02X\n", segment, checksum);
      }

      // Write beginning of "Data Record", count, and lower 16-bits of "addr"
      checksum = count + Sum(addr & 0xFFFF);
      file.printf(":%02X%04X00", count, addr & 0xFFFF);
      
      // Write all bytes in the row while also updating the checksum
      for(i = addr; i < rowAddr; i++) {
         checksum += pBuffer[i];
         file.printf("%02X", pBuffer[i]);
      }
      
      // Finish the record by printing the checksum at the end
      checksum = -checksum;
      file.printf("%02X\n", checksum);
   }
   
   // Write "End of File" record
   file.printf(":00000001FF\n");
}

void Write_SREC(UCHAR *pBuffer, const int pSize, const char *pName)
//********************
// Write EEPROM memory contents to a Motorola S-Record File. Sections of
// memory set to $FF are omitted from the file since this is the default
// value. EEPROMs larger than 16-bits will use the larger address "S2"
// record type.
{
   // Open file for writing in text mode. The "t" specifier is needed for the
   // stdio library to translate "\n" characters written to the file into the
   // "\r\n" used by Windows.
   File file(pName, "wt");

   // Write or skip over EEPROM contents one row (16 bytes) at a time.
   for(int addr = 0; addr < pSize; addr += 0x10) {
      int rowAddr = (addr + 0x10 < pSize) ? (addr + 0x10) : pSize;
      int count = rowAddr - addr;
      UCHAR checksum;
      int i;
   
      // If the entire row contains $FF bytes then don't write it to file
      for(i = addr; i < rowAddr; i++) {
         if(pBuffer[i] != 0xFF) break;
      }
      if(i == rowAddr) continue;

      // Based on the current "addr" size, write beginning of either an "S1"
      // or "S2" record type.
      if(addr <= 0xFFFF) {
         checksum = (count + 3) + Sum(addr);
         file.printf("S1%02X%04X", count + 3, addr);
      } else {
         checksum = (count + 4) + Sum(addr);
         file.printf("S2%02X%06X", count + 4, addr);
      }
      
      // Write all bytes in the row while also updating the checksum
      for(i = addr; i < rowAddr; i++) {
         checksum += pBuffer[i];
         file.printf("%02X", pBuffer[i]);
      }
      
      // Finish the record by printing the checksum at the end
      checksum = ~checksum;
      file.printf("%02X\n", checksum);
   }
      
   // Write "End of File" record
   file.printf("S9030000FC\n");   
}

void Write_GEN(UCHAR *pBuffer, const int pSize, const char *pName)
//********************
// Write EEPROM memory contents to an Atmel Generic file in 16/8 format. Only
// the first 65536 bytes of EEPROM memory can be saved when using this format.
{
   int addr;

   // Open file for writing in text mode. The "t" specifier is needed for the
   // stdio library to translate "\n" characters written to the file into the
   // "\r\n" used by Windows.
   File file(pName, "wt");

   // Use 16/8 format for smaller EEPROMs; only write non-$FF bytes
   int maxSize = MIN(pSize, 0x10000);
   for(addr = 0; addr < maxSize; addr++) {
      if(pBuffer[addr] != 0xFF) {
         file.printf("%04X:%02X\n", addr, pBuffer[addr]);
      }
   }

   // Check for non $FF data at higher addresses and issue warning
   for(; addr < pSize; addr++) {
      if(pBuffer[addr] != 0xFF) {
         File_Error(pName, MB_ICONWARNING, FILEWARN_GENERICBIG);
         break;
      }
   }
}

// =============================================================================
// Memory image loading functions
// =============================================================================

void Read_BIN(UCHAR *pBuffer, const int pSize, const char *pName)
//********************
// Read full EEPROM memory contents from a raw binary file. A warning if shown
// if the file is larger than the EEPROM memory size, and the additional data
// at the end of the file is ignored. Files smaller than the EEPROM memory
// simply initialize a smaller portion of memory
{
   // Open file for reading in binary mode. The "b" specifier is needed so the
   // stdio library does not try to translate "\r\n" line edings used by
   // Windows into the "\n" specified by the C standard. Also request that
   // end-of-file should be returned by file.read() instead of throwing an
   // exception.
   File file(pName, "rb", true);

   // Read up to the smaller size of available memory or file size
   file.read(pBuffer, pSize);
   
   // Try readning one more byte to check if file is larger than memory. If
   // read succeesds than issue a warning because additional data is ignored.
   char dummy;
   if(file.read(&dummy, 1)) {
      File_Error(pName, MB_ICONWARNING, FILEERROR_TOOBIG);
   }   
}

void Read_HEX(UCHAR *pBuffer, const int pSize, const char *pName)
//********************
// Read EEPROM memory contents from an Intel HEX file.
{
   bool warnAddress = false;  // File contain addresses too big for EEPROM?
   bool warnChecksum = false; // File contains checksum mismatches?
   bool done = false;         // True when "End of File" record read

   // The upper 16 bits of the last EEPROM address written to file. Updated
   // by "02" and "04" record types.
   int segment = 0;
   
   // According the official Intel HEX file standard, the memory address is
   // supposed to wrap around within a 64KB block when reading data. If
   // an Extended Linear Address Record is read, then the address will no
   // longer wrap; reading an Extended Segment Address Record will re-enable
   // the wrapping.
   int mask = 0xFFFF;
   
   // Open file for reading in text mode. The "t" specifier is needed for the
   // stdio library to translate the "\r\n" line edings used by Windows into the
   // "\n" specified by the C standard.
   File file(pName, "rt");

   // Keep reading records until "End of File" record terminates the loop
   while(!done) {
      int count, addr, type;  // Fixed size field in all records
      char data[256];         // Variable size data field in some records
      UCHAR checksum;         // Checksum computed on the fly
      
      // Read entire record into memory, including variable sized data, and
      // also compute a checksum on the fly which will be checked later
      file.scanf(3, " :%2X%4X%2X", &count, &addr, &type);
      checksum = count + Sum(addr) + type;
      for(int i = 0; i < count; i++) {
         int temp;
         file.scanf(1, "%2X", &temp);
         data[i] = temp;
         checksum += temp;
      }
      
      // Decode record type
      switch(type) {
      
         // Data record; copy data into EEPROM memory. Ignore data at higher
         // addresses than supported by EEPROM.
         case 00:
            for(int i = 0; i < count; i++, addr++) {
               int fullAddr = segment + (addr & mask);
               if(fullAddr < pSize) {
                  pBuffer[fullAddr] = data[i];
               } else {
                  warnAddress = true;
               }
            }
            break;
         
         // End of File record; finish reading file
         case 01:
            done = true;
            break;
            
         // Extended Segment Address Record; update segment address
         // Assume big-endian address format in the data
         case 02:
            segment = ((data[0] << 8) | data[1]) << 4;
            mask = 0xFFFF;
            break;

         // Extended Linear Address Record; update segment address
         // Assume big-endian address format in the data
         case 04:
            segment = ((data[0] << 8) | data[1]) << 16;
            mask = 0xFFFFFFFF;
            break;
         
         // Ignore all other record types
         default:
            break;
      }
      
      // Read checksum from file and compare with locally computed checksum
      UCHAR fileChecksum;
      file.scanf(1, "%2X", &fileChecksum);
      checksum = -checksum;
      if(fileChecksum != checksum) {
         warnChecksum = true;
      }
   }
   
   // Display any warnings about the file
   if(warnAddress) {
      File_Error(pName, MB_ICONWARNING, FILEERROR_TOOBIG);
   }
   if(warnChecksum) {
      File_Error(pName, MB_ICONWARNING, FILEERROR_CHECKSUM);
   }
}

void Read_SREC(UCHAR *pBuffer, const int pSize, const char *pName)
//********************
// Read EEPROM memory contents from an Intel HEX file.
{
   bool warnAddress = false;  // File contain addresses too big for EEPROM?
   bool warnChecksum = false; // File contains checksum mismatches?
   bool done = false;         // True when "End of File" record read

   // Open file for reading in text mode. The "t" specifier is needed for the
   // stdio library to translate the "\r\n" line edings used by Windows into the
   // "\n" specified by the C standard.
   File file(pName, "rt");

   // Keep reading records until "End of File" record terminates the loop
   while(!done) {
      int count, type; // Fixed size field in all records
      char data[256];  // Variable size data field in some records
      UCHAR checksum;  // Checksum computed on the fly
      
      // Read record type and length and compute checksum on the fly
      file.scanf(2, " S%1X%2X", &type, &count);
      checksum = count;
      
      // Read variable sized address field depending on data sequuence record
      // type. Other record types simply ignore the address.
      int addr = 0;
      switch(type) {
         case 1: file.scanf(1, "%4X", &addr); count -= 2; break;
         case 2: file.scanf(1, "%6X", &addr); count -= 3; break;
         case 3: file.scanf(1, "%8X", &addr); count -= 4; break;
      }
      checksum += Sum(addr);

      // Read variable sized data portion, excluding checksum byte
      for(int i = 0; i < count - 1; i++) {
         int temp;
         file.scanf(1, "%2X", &temp);
         data[i] = temp;
         checksum += temp;
      }
      
      // Decode record type
      switch(type) {
      
         // Data record; copy data into EEPROM memory. Ignore data at higher
         // addresses than supported by EEPROM.
         case 1: case 2: case 3:
            for(int i = 0; i < count - 1; i++, addr++) {
               if(addr < pSize) {
                  pBuffer[addr] = data[i];
               } else {
                  warnAddress = true;
               }
            }
            break;
         
         // End of File record; finish reading file
         case 7: case 8: case 9:
            done = true;
            break;
         
         // Ignore all other record types
         default:
            break;
      }
      
      // Read checksum from file and compare with locally computed checksum
      UCHAR fileChecksum;
      file.scanf(1, "%2X", &fileChecksum);
      checksum = ~checksum;
      if(fileChecksum != checksum) {
         warnChecksum = true;
      }
   }
   
   // Display any warnings about the file
   if(warnAddress) {
      File_Error(pName, MB_ICONWARNING, FILEERROR_TOOBIG);
   }
   if(warnChecksum) {
      File_Error(pName, MB_ICONWARNING, FILEERROR_CHECKSUM);
   }
}

void Read_GEN(UCHAR *pBuffer, const int pSize, const char *pName)
//********************
// Read EEPROM memory contents from an Atmel Generic file in 16/8 format.
{
   bool warnAddress = false;  // File contain addresses too big for EEPROM?
   
   // Open file for reading in text mode. The "t" specifier is needed for the
   // stdio library to translate the "\r\n" line edings used by Windows into the
   // "\n" specified by the C standard. Also request that end-of-file should be
   // returned by file.scanf() instead of throwing an exception.
   File file(pName, "rt", true);

   // Atmel generic has no explicit end of data marker so continue reading
   // until the end-of-file is reached. Ignore data at higher addresses than
   // supported by current EEPROM memory size.
   int addr, data;
   while(file.scanf(2, " %4X:%2X", &addr, &data)) {   
      if(addr < pSize) {
         pBuffer[addr] = data;
      } else {
         warnAddress = true;
      }
   }
   
   // Display any warning about the file
   if(warnAddress) {
      File_Error(pName, MB_ICONWARNING, FILEERROR_TOOBIG);
   }
}

// =============================================================================
// Hexfile class member functions
// =============================================================================

Hexfile::Hexfile()
//******************************
// The default constructor allows Hexfile objects to be placed directly in
// the DECLAVE_VAR block. The real initialization will hapen when the init()
// function is called.
{
   // Global reference count is used in destructor to release all class
   // resources acquired in the very first init() call.
   Ref_count++;
}

Hexfile::~Hexfile()
//******************************
// Destructor releases resource previously acquired in init(). Based on the
// static reference count, the last Hexfile instance to be destroyed also
// releases global resources acquired by the first init() call.
{
   // Destroy the HEX editor control and MDI child windows
   destroy();

   // The last instance of the Hexfile class must also unregister the Win32
   // window class and unload the DLL library from init(). All class variables
   // are set to NULL so the next init() can re-initialize them.
   Ref_count--;
   if(Ref_count == 0) {
      if(MDI_class) {
         W32_ASSERT( UnregisterClass( (LPCTSTR) MDI_class, Instance) );
         MDI_class = NULL;
      }
      if(Library) {
         W32_ASSERT( FreeLibrary(Library) );
         Library = NULL;
      }
      VMLAB_window = NULL;
      MDI_client = NULL;
   }
}

void Hexfile::init(HINSTANCE pInstance, HWND pHandle, char *pTitle, int pIcon)
//******************************
// Must be called at least once on each Hexfile object before calling any
// other methods. The "pInstance" must have been saved by the caller from
// the DllEntryPoint() or DllMain() function. The "pHandle" should be the
// same as passed to the On_window_init() callback. The "pTitle" specifies
// the window title of the child window. The "pIcon" is optional and specifies
// the resource ID for a small icon to display in the corner of the window;
// if the resource doesn't exist in the pInstance DLL then the main VMLAB
// executable is searched.
{
   // TODO: Verify args are not NULL; 
   // TODO: Verify not already called on same window
   
   // First init() finds top-level VMLAB window so it can later be passed as
   // hwndOwner to GetOpenFileName(), GetSaveFileName(), and MessageBox().
   // FindWindow() cannot be used here because there could be two VMLAB
   // windows open if running in multiprocess mode.
   if(VMLAB_window == NULL) {
      VMLAB_window = pHandle;
      while(HWND parent = GetParent(VMLAB_window)) {
         VMLAB_window = parent;
      }
   }
   
   // Lookup the module handle to VMLAB.EXE from the window class of the
   // main VMLAB window. This module handle is used in LoadIcon() later on
   // to allow using icons from the main executable if an icon resource
   // was not found in the user's component DLL.
   if(VMLAB_module == NULL) {      
      W32_ASSERT( VMLAB_module = (HINSTANCE) GetClassLong(VMLAB_window, GCL_HMODULE) );
   }
   
   // First init() finds the MDI client window which is a child of the
   // top-level VMLAB window. The MDI client window acts as a parent for the
   // MDI child window that will is created to display the hex editor.
   if(MDI_client == NULL) {
      W32_ASSERT( MDI_client = FindWindowEx(VMLAB_window, NULL,
         "MDIClient", NULL) );
   }
   
   // Load DLL to automatically register "SHINEINHEX" window class
   if(Library == NULL) {
      W32_ASSERT( Library = LoadLibrary("ShineInHex.dll") );
   }
   
   // The HINSTANCE must be saved so that it can be later passed to
   // UnregisterClass() in the destructor.
   if(Instance == NULL) {
      Instance = pInstance;
   }
   
   // Try to load specified icon resource first from user's DLL and failing
   // that, from the main VMLAB executable. Because LoadIcon() returns a
   // shared icon, it is not necessary to later unload the icon in the
   // destructor.
   if(Icon == NULL && pIcon) {
      Icon = LoadIcon(pInstance, MAKEINTRESOURCE(pIcon));
      if(Icon == NULL) {
         W32_ASSERT( Icon = LoadIcon(VMLAB_module, MAKEINTRESOURCE(pIcon)) );
      }
   }

   // Register custom window class to handle behavior of the MDI child window
   if(MDI_class == NULL) {
      WNDCLASSEX mdiClass = {
         sizeof(WNDCLASSEX),  // cbSize (size of entire structure)
         0,                   // style (for performance don't clip children) // TODO??
         MDI_proc,            // lpfnWndProc (pointer to window procedure)
         0,                   // cbClsExtra (no extra class memory needed)
         0,                   // cbWndExtra (no extra window memory needed)
         pInstance,           // hInstance (DLL instance from DllEntryPoint)
         NULL,                // hIcon (large icon not used for child windows)
         NULL,                // hCursor (use default cursor) // TODO: use constant
         NULL,                // hbrBackground (hex editor fills entire window)
         NULL,                // lpszMenuName (no default class menu needed)
         CLASS_NAME,          // lpszClassName (class name used only in Hexfile)
         Icon,                // hIconSm (small icon loaded from resource)
      };
      W32_ASSERT( MDI_class = RegisterClassEx(&mdiClass) );
   }

   // Create initially hidden MDI child window containing the hex editor
   // control window as a child. Calling show() will make the window visible
   // and clicking the window's close box (SC_CLOSE) will hide it again.   
   if(MDI_child == NULL) {
      MDI_child = CreateMDIWindow(
         CLASS_NAME,      // lpClassName (returned by RegisterClassEx)
         pTitle,          // lpWindowName (text displayed in title bar)
         WS_OVERLAPPEDWINDOW, // dwStyle (same style as other MDI children)
         0,               // X (upper left corner of VMLAB window)
         0,               // Y (upper left corner of VMLAB window)
         INIT_WIDTH,      // nWidth (default based on Control Panel size)
         INIT_HEIGHT,     // nHeight (default based on Control Panel size)
         MDI_client,      // hWndParent (parent MDI client window for child)
         pInstance,       // hInstance (window was created by this DLL)
         NULL             // lParam (no special value to pass to window proc)
      );
      W32_ASSERT( MDI_child );
      
      // TODO: Assert that HEX_child == NULL
      
      // Hex editor window alays fills entire client area of MDI child
      RECT rect;
      W32_ASSERT( GetClientRect(MDI_child, &rect) );      
      
      // Create a hex editor window embedded in MDI child window
      HEX_child = CreateWindowEx(
         WS_EX_CLIENTEDGE,// dwExStyle (extra inset border to match VMLAB)
         "SHINEINHEX",    // lpClassName (registered by loading ShineInHex.dll)
         "",              // lpWindowName (not dispplayed by hex editor)
         WS_VISIBLE |     // dwStyle (always visible in parent window)
         WS_CHILD |       // dwStyle (child window with no frame)
         WS_VSCROLL,      // dwStyle (show vertical scroll bar)
         0,               // x (fills entire client area of parent)
         0,               // y (fills entire client area of parent)
         rect.right,      // nWidth (fills entire client area of parent)
         rect.bottom,     // nHeight (fills entire client area of parent)
         MDI_child,       // hWndParent (control embeds in MDI child window)
         NULL,            // hMenu (used as id; not needed for one child)
         pInstance,       // hInstance (window was created by this DLL)
         NULL             // lParam (no special value to pass to window proc)
      );
      W32_ASSERT( HEX_child );
      
      // All text in hex edit is displayed in black and all backgrounds are
      // drawn in white to match the layout of other VMLAB windows. Current
      // cursor position is shown as white text on a black background, and
      // selections in the hex editor are white text on a blue background.
      COLORREF txtColFg = GetSysColor(COLOR_WINDOWTEXT);
      COLORREF txtColBg = GetSysColor(COLOR_WINDOW);
      COLORREF selColFg = GetSysColor(COLOR_HIGHLIGHTTEXT);
      COLORREF selColBg = GetSysColor(COLOR_HIGHLIGHT);
      COLORREF hdrColFg = GetSysColor(COLOR_BTNTEXT);
      COLORREF hdrColBg = GetSysColor(COLOR_BTNFACE);
      
      // Initialize display settings for ShineInHex control
      SendMessage(HEX_child, HEXM_SETFONT, 3, 0); // Courier New font
      SendMessage(HEX_child, HEXM_SETOFFSETGRADCOL, hdrColBg, hdrColBg);
      SendMessage(HEX_child, HEXM_SETHEADERGRADCOL, hdrColBg, hdrColBg);
      SendMessage(HEX_child, HEXM_SETVIEW1TEXTCOL, hdrColFg, hdrColFg);
      SendMessage(HEX_child, HEXM_SETHEADERTEXTCOL, hdrColFg, hdrColFg);
      SendMessage(HEX_child, HEXM_SETVIEW2COL, txtColBg, txtColFg);
      SendMessage(HEX_child, HEXM_SETVIEW3COL, txtColBg, txtColFg);
      SendMessage(HEX_child, HEXM_SETVIEW2SELCOL, selColBg, selColFg);
      SendMessage(HEX_child, HEXM_SETVIEW3SELCOL, selColBg, selColFg);
      SendMessage(HEX_child, HEXM_SETACTIVECHARCOL, txtColFg, txtColBg);
      SendMessage(HEX_child, HEXM_SETMODBYTES1COL, txtColBg, txtColFg);
      SendMessage(HEX_child, HEXM_SETMODBYTES2COL, txtColBg, txtColFg);
      SendMessage(HEX_child, HEXM_SETMODBYTES3COL, txtColBg, txtColFg);
      
      // The hex editor doesn't work very well without any data and
      // will in fact crash if you try typing in the edit window. If
      // show() is called before data(), this will ensure the editor
      // functions properly until data() is called.
      SendMessage(HEX_child, HEXM_SETPOINTER, (WPARAM) &Dummy, 1);      
   }
}

void Hexfile::destroy()
//******************************
// Undo the effects of the init() call by destroying the HEX editor
// control and MDI child windows. This method should be called by the
// component from On_destroy() to ensure no accidental memory references
// to freed memory.
{
   // Destroying HEX_child will also destroy contained ShineInHex control
   if(HEX_child) {
      W32_ASSERT( DestroyWindow(HEX_child) );
      HEX_child = NULL;
   }

   // Destroy MDI child window, activate the next MDI child, and request that
   // the MDI client refreshes the "Windows" menu in the top-most VMLAB window
   // to remove the destroyed MDI child.
   if(MDI_child) {
      SendMessage(MDI_client, WM_MDIDESTROY, (WPARAM) MDI_child, 0);
      SendMessage(MDI_client, WM_MDINEXT, 0, 0);
      SendMessage(MDI_client, WM_MDIREFRESHMENU, 0, 0);
      W32_ASSERT( DrawMenuBar(VMLAB_window) );
      MDI_child = NULL;
   }
}

void Hexfile::data(void *pPointer, int pSize, int pOffset)
//******************************
// Set or change the raw data (and its size in bytes) that is displayed
// in the hex editor. Any changes made interactively by the user using
// the editor will be immediately written back to the buffer specified
// by "pPointer". The "pOffset" is typically zero but can specify any
// arbitrary address to display in the editor for the first byte of the
// buffer.
{
   // TODO: assert that pSize is power of 2 and larger than 16 (needed by
   // the file read/write functions)

   // TODO: assert that HEX_child should always exist

   // Update local variables used by load() and save()
   Pointer = (UCHAR*) pPointer;
   Size = pSize;
   Offset = pOffset;
   
   // Update internal state in the editor. Because the editor is not stable
   // without any data, the Dummy member variable is used as a placeholder
   // when unsetting the internal data pointers.   
   SendMessage(HEX_child, HEXM_SETOFFSET, (WPARAM) Offset, 0);
   if(Pointer && Size) {
      SendMessage(HEX_child, HEXM_SETPOINTER, (WPARAM) Pointer, Size);
   } else {
      SendMessage(HEX_child, HEXM_SETPOINTER, (WPARAM) &Dummy, 1);
   }
   
   // The hex editor will invalidate and re-draw its own window in response
   // to the HEXM_SETPOINTER message.
}
void Hexfile::hide()
//******************************
// Hide the window from view. Although this function is available publicly,
// calling should not be necessary since closing the window will automatically
// perform the same action.
{
   // TODO: Assert MDI_child window already exists; init() was called

   Hide(MDI_child);
}

void Hexfile::refresh()
//******************************
// Update displayed hex data because simulation has changed memory contents
{
   // Hex editor seems to use double buffering to prevent flicker. Sending
   // a HEXM_SETPOINTER by calling data() will force the editor to re-read
   // the data memory..
   //data(Pointer, Size, Offset);
   W32_ASSERT( InvalidateRect(HEX_child, NULL, false) );
}

void Hexfile::show()
//******************************
// Called to handle "View" button in the GUI. the MDI child window that was
// previously created inside init() is made visible.
{   
   // TODO: Assert MDI_child window already exists; init() was called
      
   // Make window visible and activate (i.e. is brought to the front and
   // obtains keyboard focus). Request that the MDI client refreshes the
   // "Windows" menu in the top-most VMALB window to include the now visible
   // MDI child.
   ShowWindow(MDI_child, SW_SHOW);
   SendMessage(MDI_client, WM_MDIACTIVATE, (WPARAM) MDI_child, 0);   
   SendMessage(MDI_client, WM_MDIREFRESHMENU, 0, 0);
   W32_ASSERT( DrawMenuBar(VMLAB_window) );   
}

void Hexfile::readonly(bool pReadOnly)
//******************************
// Set the hex editor control to either read-only or read/write and update
// hex editor text color to provide visual feedback of current editing mode.
{
   // TODO: Assert HEX_child window already exists; init was called()

   // If the control is set to read-only, text in the editor is drawn in gray
   // over a white background. If set to read-write, text is draw in black.
   COLORREF txtColFg = GetSysColor(pReadOnly ? COLOR_GRAYTEXT : COLOR_WINDOWTEXT);
   COLORREF txtColBg = GetSysColor(COLOR_WINDOW);
   
   SendMessage(HEX_child, HEXM_SETVIEW2COL, txtColBg, txtColFg);
   SendMessage(HEX_child, HEXM_SETVIEW3COL, txtColBg, txtColFg);   
   SendMessage(HEX_child, HEXM_SETREADONLY, pReadOnly, 0);

   // Force a redraw of the hex editor window to show updated colors
   InvalidateRect(HEX_child, NULL, true);
}   

void Hexfile::load(char *pFile, int pType)
//******************************
// Given the filename in "pFile" and the file type (one of the FT_HEX,
// FT_SREC, etc. enums) in "pType", attempt to load the memory image file
// into memory and immediately refresh the hex editor to show the new data.
{
   // Initialize EEPROM contents to fully erased (0xFF) state
   // before loading data so that any unspecified "holes" in the
   // file's address space end up as $FF
   memset(Pointer, 0xFF, Size);

   // If an I/O error occurs, the File class will throw an exception
   // to abort the operation. The EEPROM memory may be partially
   // initialized if only part of the input file was read.
   try {
      switch(pType) {
         case FT_HEX:  Read_HEX(Pointer, Size, pFile); break;
         case FT_SREC: Read_SREC(Pointer, Size, pFile); break;
         case FT_GEN:  Read_GEN(Pointer, Size, pFile); break;
         case FT_BIN:  Read_BIN(Pointer, Size, pFile); break;
         default:
            File_Error(pFile, MB_ICONERROR,
               "Invalid file type in Hexfile::load()");
            break;
      }
      
      // Force a redraw of the hex editor window to show new data
      refresh();
   }
   catch (File::Error) {}
}

void Hexfile::load()
//******************************
// Display common "Open" dialog box so the user can select a memory image file
// and load memory contents from file. The hex editor is immediately refreshed
// to show the new data.
{
   // Structure and pathname buffer for use with common dialog routines.
   char pathBuffer[MAX_PATH];
   OPENFILENAME dlg;
   
   // Initialize OPENFILENAME structure for use with "Open" common dialog box.
   // All unused, reserved, and output fields are initialized to zero.
   memset(&dlg, 0, sizeof(OPENFILENAME));  // Initialize struct to zero
   pathBuffer[0] = '\0';                   // No initial filename in dialog
   dlg.lStructSize = sizeof(OPENFILENAME); // Structure size required
   dlg.hwndOwner = VMLAB_window;           // Owner window of Open/Save dialog
   dlg.lpstrFilter = OPENFILENAME_FILTER;  // Supported file types
   dlg.lpstrFile = pathBuffer;             // Buffer to receive user's filename
   dlg.nMaxFile = MAX_PATH;                // Total size of lpstrFile buffer
   dlg.lpstrDefExt = "eep";                // Default filename extension
   dlg.lpstrTitle = "Load EEPROM File";    // Dialog title
   dlg.Flags =                             // Option flags
      OFN_NOCHANGEDIR |              // Don't change VMLAB's working directory
      OFN_HIDEREADONLY |             // Hide "Read Only" checkbox in dialog
      OFN_FILEMUSTEXIST |            // File names must already exist
      OFN_PATHMUSTEXIST;             // Directory locations must already exist

   // If the user clicks "OK" button, attempt loading from the file
   if(GetOpenFileName(&dlg)) {
      load(pathBuffer, dlg.nFilterIndex);
   }
}

void Hexfile::save(char *pFile, int pType)
//******************************
// Given the filename in "pFile" and the file type (one of the FT_HEX,
// FT_SREC, etc. enums) in "pType", attempt to write the memory contents
// into a memory image file.
{
   // If an I/O error occurs, the File class will throw an exception
   // to abort the operation. The output file will have been partially
   // written at this point.
   try {
      switch(pType) {
         case FT_HEX:  Write_HEX(Pointer, Size, pFile); break;
         case FT_SREC: Write_SREC(Pointer, Size, pFile); break;
         case FT_GEN:  Write_GEN(Pointer, Size, pFile); break;
         case FT_BIN:  Write_BIN(Pointer, Size, pFile); break;
         default:
            File_Error(pFile, MB_ICONERROR,
               "Invalid file type in Hexfile::load()");
            break;
      }
   }
   catch (File::Error) {}
}

void Hexfile::save()
//******************************
// Display common "Save" dialog box so the user can select a memory image file
// and write memory contents to file.
{
   // Structure and pathname buffer for use with common dialog routines.
   char pathBuffer[MAX_PATH];
   OPENFILENAME dlg;
   
   // Initialize OPENFILENAME structure for use with "Open" common dialog box.
   // All unused, reserved, and output fields are initialized to zero.
   memset(&dlg, 0, sizeof(OPENFILENAME));  // Initialize struct to zero
   pathBuffer[0] = '\0';                   // No initial filename in dialog
   dlg.lStructSize = sizeof(OPENFILENAME); // Structure size required
   dlg.hwndOwner = VMLAB_window;           // Owner window of Open/Save dialog
   dlg.lpstrFilter = OPENFILENAME_FILTER;  // Supported file types
   dlg.lpstrFile = pathBuffer;             // Buffer to receive user's filename
   dlg.nMaxFile = MAX_PATH;                // Total size of lpstrFile buffer
   dlg.lpstrDefExt = "eep";                // Default filename extension
   dlg.lpstrTitle = "Save EEPROM File";    // Dialog title
   dlg.Flags =                             // Option flags
      OFN_NOCHANGEDIR |              // Don't change VMLAB's working directory
      OFN_OVERWRITEPROMPT |          // Ask before overwriting existing file
      OFN_PATHMUSTEXIST;             // Directory locations must already exist

   // If the user clicks "OK" button, attempt saving to the file      
   if(GetSaveFileName(&dlg)) {         
      save(pathBuffer, dlg.nFilterIndex);
   }      
}

void Hexfile::erase()
//******************************
// Display a message box to the user asking for confirmation. If user clicks
// "Yes" button, then erase entire memory contents to $FF and immediately
// refresh the hex editor to show the changed data.
{
   int rc = MessageBox(
      VMLAB_window,             // hWnd (owning window of message box)
      CONFIRM_ERASE,            // lpText (test shown in body of window)
      "Confirm EEPROM Erase",   // lpCaption (text shown in the title bar)
      MB_YESNO | MB_ICONWARNING // uType (Yes/No buttons and exclamation icon)
   );
   
   if(rc == IDYES) {
      memset(Pointer, 0xFF, Size);
      refresh();
   }
}