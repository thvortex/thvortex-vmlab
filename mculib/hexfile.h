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

#ifndef _HEXFILE_H
#define _HEXFILE_H

class Hexfile
//*********
// Primary interface class. A separate instance of this class should be
// created for every data buffer. Instances can be added directly to
// the DELCARE_VAR section.
//
// Each byte in the bitflag buffer registered with flags() has a one-to-one
// correspondence with the each byte in the data buffer registered with
// data(). The bits in each flag byte are defined as follows:
//
// Bit 0: True if read by microcontroller
// Bit 1: Reserved for 2nd port (i.e. device) read in dual-port memory
// Bit 2: True if written by microcontroller
// Bit 3: Reserved for 2nd port (i.e. device) write in dual-port memory
// Bit 4: Reserved for read breakpoint
// Bit 5: Reserved for write breakpoint
// Bit 6: Reserved for valid/invalid location (will always display hex as ..)
// Bit 7: Reserved for data type (I/O register vs normal memory)
// 
// At this time only bits 0 and 1 are currently implemented
{
private:
   HINSTANCE Instance; // Saved copy of pInstance passed to init()
   HICON Icon;       // Small icon resource used by child window
   HWND MDI_child;   // Child window of MDI_client containing HEX_child
   HWND HEX_child;   // The "shineinhex" class window inside MDI_child
   
   UCHAR *Pointer;   // Data buffer on which all operations will act on
   UINT Size;        // Total size of data buffer in bytes
   UINT Offset;      // Value added to the offset display in the hex editor

   UCHAR *Flags;     // bitflag buffer for customizing hex editor colors
   UINT Flags_size;  // Size of the bitflag buffer
   
   char Dummy;       // Dummy data for editor because HEXM_UNSETPOINTER is buggy

   void On_custom_colors(LPARAM lParam);
   void On_clear_rw();
   
   static LRESULT CALLBACK MDI_proc(HWND pWindow, UINT pMessage, WPARAM pW, LPARAM pL);
   
public:
   // File types which are passed as the 2nd argument to the load() and
   // save() functions. Note that these enum values must correspond to
   // the OPENFILENAME_FILTER string in hexfile.cpp.
   enum { FT_HEX = 1, FT_SREC, FT_GEN, FT_BIN };
   
   // bitflags used with the flags() registered buffer to indicate what type
   // of memory access (read or write) has been performed on an address range.
   // These presence of these flags customizes the display colors in the hex
   // editor. All other bit positions are reserved for future use and should
   // be zero.
   enum { FL_READ = 0x1, FL_WRITE = 0x4 };

   // Bitmasks used with the flags() registered buffer to select various bits.
   // The bits used for read/write coverage are considered ephermal; they can
   // be cleared by the user via the context menu and should be cleared by the
   // component on simulation end.
   enum { FLM_COVERAGE = 0x0f, FLM_BREAKPOINTS = 0x30 };
   
   Hexfile();
   ~Hexfile();

   void init(HINSTANCE pInstance, HWND pHandle, char *pTitle, int pIcon = 0);   
   void data(UCHAR *pPointer, UINT pSize, UINT pOffset = 0);   
   void destroy();

   void flags(UCHAR *pFlags, UINT pSize);
   
   void erase();
   void load();
   void save();
   void load(char *pFile, int pType);
   void save(char *pFile, int pType);

   void hide();
   void show();
   void readonly(bool pReadOnly);
   void refresh();
};

#endif // #ifndef _HEXFILE_H
