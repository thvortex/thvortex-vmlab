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
// error information.
#define W32_ASSERT(cond) if((cond) == 0) \
   { w32_error("W32_ASSERT( "#cond" );", __FILE__, __LINE__); }

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

   static const char CLASS_NAME[]; // Window class name used only internally
   
   HWND MDI_child;  // Child window of MDI_client containing HEX_child
   HWND HEX_child;  // The "shineinhex" class window inside MDI_child
   
   void *Pointer;   // Data buffer on which all operations will act on
   int Size;        // Total size of data buffer in bytes
   int Offset;      // Value added to the offset display in the hex editor
   
   char Dummy;      // Dummy data for editor because HEXM_UNSETPOINTER is buggy
      
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
   
   static void w32_error(char *pCode, char *pFile, int pLine);

   static LRESULT CALLBACK MDI_proc(HWND, UINT, WPARAM, LPARAM);   
   static void hide(HWND pWindow);
   
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
   void refresh();
};

// Definitions for all static class varialbes in Hexfile
HWND Hexfile::VMLAB_window = NULL;
HWND Hexfile::MDI_client = NULL;
ATOM Hexfile::MDI_class = NULL;
HMODULE Hexfile::Library = NULL;
const char Hexfile::CLASS_NAME[] = "VMLAB Hexfile Editor";

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
   // TODO: Call HEXM_UNSETPOINTER before destroying windows?

   // Destroying the MDI_child will also destroy the contained HEX_child.
   if(MDI_child) {
      SendMessage(MDI_client, WM_MDIDESTROY, (WPARAM) MDI_child, 0);
   }
   
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
         NULL,                // hbrBackground (hex editor fills entire window)
         NULL,                // lpszMenuName (no default class menu needed)
         CLASS_NAME,          // lpszClassName (class name used only in Hexfile)
         NULL,                // hIconSm (use icon() to set per window icon)
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
   
   show(); // TODO: DELETE LATER
}

void Hexfile::data(void *pPointer, int pSize, int pOffset)
//******************************
{
   // TODO: assert that HEX_child should always exist

   // Update local variables used by load() and save()
   Pointer = pPointer;
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
   
   // Force a redraw of the hex editor window
   InvalidateRect(HEX_child, NULL, true);
}
void Hexfile::hide()
//******************************
// Hide the window from view. Although this function is available publicly,
// calling should not be necessary since closing the window will automatically
// perform the same action.
{
   // TODO: Assert MDI_child window already exists; init() was called

   hide(MDI_child);
}

void Hexfile::refresh()
//******************************
// Update displayed hex data because simulation has changed memory contents
{
   // Hex editor seems to use double buffering to prevent flicker. Sending
   // a HEXM_SETPOINTER forced the editor to re-read the data memory..
   data(Pointer, Size, Offset);
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

void Hexfile::hide(HWND pWindow)
//******************************
// This function is called from hide(void) and from the MDI window procedure
// (on WM_SYSCOMMAND/SC_CLOSE) to perform the real work of hiding the MDI
// child window.
{
   // Hide this MDI child window, active the next MDI child, and request that
   // the MDI client refreshes the "Windows" menu in the top-most VMALB window
   // to remove the hidden MDI child.
   ShowWindow(pWindow, SW_HIDE);
   SendMessage(MDI_client, WM_MDINEXT, 0, 0);
   SendMessage(MDI_client, WM_MDIREFRESHMENU, 0, 0);
   W32_ASSERT( DrawMenuBar(VMLAB_window) );   
}

LRESULT CALLBACK Hexfile::MDI_proc(HWND pWindow, UINT pMessage, WPARAM pW, LPARAM pL)
//******************************
{
   HWND child;

   switch(pMessage) {
      // System menu (close/minimize/maximise) command. Closing the window
      // (SC_CLOSE) will merely hide it instead of destryoing it. All other
      // system commands are passed through.
      case WM_SYSCOMMAND:
      {
         if(pW == SC_CLOSE) {
            hide(pWindow);
            return true;
         }
         break;
      }

      // The user is currently resizing window by dragging its corners. Since
      // the hex editor control doesn't draw properly at small sizes, a minimum
      // width has to be enforced here. For consistency, a minimum height is
      // also enforced which guarantees at least one row of data is visible.
      case WM_SIZING:
      {
         RECT *rect = (RECT *) pL;
         
         // Enfore minimum width otherwise
         if(rect->right - rect->left < MIN_WIDTH) {
            if(pW == WMSZ_LEFT || pW == WMSZ_TOPLEFT || pW == WMSZ_BOTTOMLEFT) {
               rect->left = rect->right - MIN_WIDTH;
            }
            if(pW == WMSZ_RIGHT || pW == WMSZ_TOPRIGHT || pW == WMSZ_BOTTOMRIGHT) {
               rect->right = rect->left + MIN_WIDTH;
            }
         }

         // Enfore minimum width otherwise
         if(rect->bottom - rect->top < MIN_HEIGHT) {
            if(pW == WMSZ_TOP || pW == WMSZ_TOPLEFT || pW == WMSZ_TOPRIGHT) {
               rect->top = rect->bottom - MIN_HEIGHT;
            }
            if(pW == WMSZ_BOTTOM || pW == WMSZ_BOTTOMLEFT || pW == WMSZ_BOTTOMRIGHT) {
               rect->bottom = rect->top + MIN_HEIGHT;
            }
         }
         
         break;
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
      
      // If MDI client is activated, make sure hex editor has keyboard focus
      case WM_MDIACTIVATE:
      {
         // Only process if this is an activate and not a de-activate event
         if(pWindow == (HWND) pL) {
            HWND child = GetWindow(pWindow, GW_CHILD);
            if(child) {
               SendMessage(child, WM_SETFOCUS, 0, 0);
            }
         }
         break;
      }

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
