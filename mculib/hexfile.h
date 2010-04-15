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

// These macros check the assertion that a Win32 API call returned succesfully.
// If the call failed, then this macro will display a message box with detailed
// error information. This macro also returns from the current function if the
// assertion has failed.
#define W32_ASSERT(cond) if((cond) == 0) \
   { w32_error("W32_ASSERT( "#cond" );", __FILE__, __LINE__); return; }

class Hexfile
//*********
// Primary interface class. A separate instance of this class should be
// created for every data buffer. Instances can be added directly to
// the DELCARE_VAR section.
{
private:
   static HWND VMLAB_window;  // Top level window that owns all dialog boxes
   static HWND MDI_client;    // MDI client window inside VMLAB window
   static HMODULE Library;    // Loaded ShineInHex.dll library
   static ATOM MDI_class;     // Window class of MDI_child window

   HWND MDI_child;  // Child window of MDI_client containing HEX_child
   HWND HEX_child;  // The "shineinhex" class window inside MDI_child
   
   // Arguments saved from init() function
   void *Pointer;   // Data buffer on which all operations will act on
   int Size;        // Total size of data buffer in bytes
   int Offset;      // Value added to the offset display in the hex editor
   
   // Default window size for newly created MDI_child
   enum { MDI_WIDTH = 251, MDI_HEIGHT = 251 };
   
   // Window messages supported by hex editor control
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
   
   static void w32_error(char *pCode, char *pFile, int pLine);

   static LRESULT CALLBACK MDI_proc(HWND, UINT, WPARAM, LPARAM);

public:
   Hexfile();
   ~Hexfile();

   void init(HINSTANCE pInstance, HWND pWindow, char *pTitle = "", int pIcon = 0);
   
   void data(void *pPointer, int pSize, int pOffset = 0);
   
   void erase();
   void load(char *pFile = NULL);
   void save(char *pFile = NULL);
   void hide();

   void show();      
   void redraw();
};

// Definitions for all static class varialbes in Hexfile
HWND Hexfile::VMLAB_window = NULL;
HWND Hexfile::MDI_client = NULL;
ATOM Hexfile::MDI_class = NULL;
HMODULE Hexfile::Library = NULL;

Hexfile::Hexfile()
//******************************
// Default constructor allows Hexfile objects to be placed directly in the
// DECLAVE_VAR block. Also responsible for loading the ShineInHex DLL which
// makes the hex editor available as a new window class.
{
   // Windows does reference counting on LoadLibrary() and FreeLibrary()
   // calls, so after the initial LoadLibrary() that does the real work,
   // all subsequent LoadLibrary() calls will return the same HMODULE.
   W32_ASSERT( Library = LoadLibrary("ShineInHex.dll") );
}

Hexfile::~Hexfile()
//******************************
// Destructor releases resource previously acquired in init(). The last the
// Hexfile instance to be destroyed also releases global resources.
{
   // TODO: Call HEXM_UNSETPOINTER before destroying window

   // Calling hide() will actually destroy MDI child if it exists
   hide();
   
   // When the last Hexfile instance is destroyed, then the library's
   // reference count will drop to zero and it will be unloaded.
   if(Library) {
      W32_ASSERT( FreeLibrary(Library) );
   }
   
   // TODO: Need to unregister class!!
   // TODO: Use our own reference counter!!
}

void Hexfile::init(HINSTANCE pInstance, HWND pWindow, char *pTitle, int pIcon)
//******************************
// Must be called at least once on each Hexfile object before calling any
// other methods.
{
   // TODO: Verify args are not NULL; 
   // TODO: Verity pInstance is same as before
   // TODO: Verify not already called on same window
   // TODO: Save pInstance somewhere
   
   // First init() finds top-level VMLAB window so it can later be passed as
   // hwndOwner to GetOpenFileName(), GetSaveFileName(), and MessageBox().
   // FindWindow() cannot be used here because there could be two VMLAB
   // windows open if running in multiprocess mode.
   if(VMLAB_window == NULL) {
      VMLAB_window = pWindow;
      while(HWND parent = GetParent(VMLAB_window)) {
         VMLAB_window = parent;
      }
   }
   
   // First init() finds the MDI client window which is a child of the
   // top-level VMLAB window. The MDI client window acts as a parent for the
   // MDI child window that will is created to display the hex editor.
   if(MDI_client == NULL) {
      W32_ASSERT( MDI_client = FindWindowEx(VMLAB_window, NULL,
         "MDIClient", NULL) );
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
         NULL,                // hIcon (use icon() to set per window icon)
         NULL,                // hCursor (use default cursor) // TODO: use constant
         //NULL,                // hbrBackground (hex editor fills entire window)
         (HBRUSH)(COLOR_WINDOW+1), // hbrBackground (TODO: hack to show window)
         NULL,                // lpszMenuName (no default class menu needed)
         "hexfile_c",         // lpszClassName (class name used only in Hexfile)
         NULL,                // hIconSm (use icon() to set per window icon)
      };
      W32_ASSERT( MDI_class = RegisterClassEx(&mdiClass) );
   }

   // Create initially hidden MDI child window containing the hex editor
   // control window as a child. Calling show() will make the window visible
   // and clicking the window's close box (SC_CLOSE) will hide it again.
   
   // TODO: Need to re-create window on each show()
   // TODO: Use SetWindowPlacement() in show() to restore position of previous
   // window within the same VMLAB debug session
   // TODO: Use WM_MDICREATE here instead; no longer need WM_MDIREFRESHMENU
   // TODO: SetWindowLong() to chnage extended styles to match those in VMLAB
   if(MDI_child == NULL) {
      MDI_child = CreateMDIWindow(
         "hexfile_c",     // lpClassName (previously used in RegisterClassEx)
         pTitle,          // lpWindowName (text displayed in title bar)
         WS_CHILD |       // dwStyle
         WS_CLIPSIBLINGS |
         WS_CLIPCHILDREN |
         WS_OVERLAPPEDWINDOW |
         WS_VISIBLE,
         0,               // X (upper left corner of VMLAB window)
         0,               // Y (upper left corner of VMLAB window)
         MDI_WIDTH,       // nWidth (default based on Control Panel size)
         MDI_HEIGHT,      // nHeight (default based on Control Panel size)
         MDI_client,      // hWndParent (parent MDI client window for child)
         pInstance,       // hInstance (window was created by this DLL)
         NULL             // lParam (no special value to pass to window proc)
      );
      W32_ASSERT( MDI_child );  

      // Request that the MDI client refreshes the "Windows" menu in the
      // top-most VMALB window to include the newly created MDI child.
      SendMessage(MDI_client, WM_MDIREFRESHMENU, 0, 0);
      W32_ASSERT( DrawMenuBar(VMLAB_window) );
   }   
}

void Hexfile::data(void *pPointer, int pSize, int pOffset)
//******************************
{
   // Update local variables used by load() and save()
   Pointer = pPointer;
   Size = pSize;
   Offset = pOffset;
   
   // If hex editor window exists, then update its internal variables
   // TODO: HEX_child should always exist
   if(HEX_child) {
      SendMessage(HEX_child, HEXM_SETOFFSET, (WPARAM) Offset, 0);
      if(Pointer && Size) {
         SendMessage(HEX_child, HEXM_SETPOINTER, (WPARAM) Pointer, Size);
      } else {
         SendMessage(HEX_child, HEXM_UNSETPOINTER, 0, 0);
      }
      // TODO: Possibly invalidate window so it redraws
   }
}
void Hexfile::hide()
//******************************
// Hide the window from view. Although this function is available publicly,
// calling should not be necessary since closing the window will automatically
// perform the same action.
{
   // Destroying the MDI_child will also destroy the contained HEX_child.
   if(MDI_child) {
      SendMessage(MDI_client, WM_MDIDESTROY, (WPARAM) MDI_child, 0);
   }
}

void Hexfile::show()
//******************************
// Called to handle "View" button in the GUI. the MDI child window that was
// previously created inside init() is made visible and is activated (i.e. is
// brought to the front and obtains keyboard focus).
{   
   // TODO: Assert MDI_child window already exists; init() was called
   
   // TODO: Hiding the window is not good enough for 
   
   ShowWindow(MDI_child, SW_SHOW);
   SendMessage(MDI_client, WM_MDIACTIVATE, (WPARAM) MDI_child, 0);
}

LRESULT CALLBACK Hexfile::MDI_proc(HWND pWindow, UINT pMessage, WPARAM pW, LPARAM pL)
//******************************
{
   switch(pMessage) {
   
      // System menu (close/minimize/maximise) command. Closing the window
      // (SC_CLOSE) will merely hide it instead of destryoing it. All other
      // system commands are passed through.
      case WM_SYSCOMMAND:
         if(pW == SC_CLOSE) {
            ShowWindow(pWindow, SW_HIDE);
            return true;
         }
         break;

      // All other messages are processed by DefMDIChildProc()
      default:
         break;
   };

   // All other messages must be passed to default MDI child window procedure
   return DefMDIChildProc(pWindow, pMessage, pW, pL);
}

void Hexfile::w32_error(char *pCode, char *pFile, int pLine)
//*************************
// This function is called if any Windows API call has failed. It uses the
// GetLastError() API to retrieve an extended error code value, calls
// FormatMessage() to produce a human readable error message, and then calls
// MessageBox() to display the error message returned by FormatMessage().
// The PRINT() interface function cannot be used here because this may be
// called from the DLL static constructor when VMLAB is not ready yet to handle
// any interface function calls from the components.
// TODO: Use second FormatMessage() to display pFile and pLine
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

// Undefine any macros that were only used in toolbar.h to avoid potential
// conflicts with other headers and the user component .cpp file.
#undef WIN32_ASSERT

#endif // #ifndef _HEXFILE_H
