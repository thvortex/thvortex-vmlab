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
{
private:
   HINSTANCE Instance; // Saved copy of pInstance passed to init()
   HICON Icon;      // Small icon resource used by child window
   HWND MDI_child;  // Child window of MDI_client containing HEX_child
   HWND HEX_child;  // The "shineinhex" class window inside MDI_child
   
   UCHAR *Pointer;  // Data buffer on which all operations will act on
   int Size;        // Total size of data buffer in bytes
   int Offset;      // Value added to the offset display in the hex editor
   
   char Dummy;      // Dummy data for editor because HEXM_UNSETPOINTER is buggy

public:
   Hexfile();
   ~Hexfile();

   void init(HINSTANCE pInstance, HWND pHandle, char *pTitle, int pIcon = 0);   
   void data(void *pPointer, int pSize, int pOffset = 0);
   void destroy();
   
   void erase();
   void load();
   void save();

   void hide();
   void show();
   void readonly(bool pReadOnly);
   void refresh();
};

#endif // #ifndef _HEXFILE_H
