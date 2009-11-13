// ============================================================================
// This component simulates a graphic 128x64 dot-matrix LCD based on the
// KS0108 or HD61202 controller chips.
//
// Copyright (C) 2009 Mrkaras.
// see http://www.amctools.com/cgi-bin/yabb2/YaBB.pl?num=1250250728 for original posting
//
// Copyright (C) 2009 Wojciech Stryjewski <thvortex@gmail.com>
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 3 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General
// Public License along with this library; if not, write to the
// Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
// Boston, MA 02110-1301 USA

#include <windows.h>
#include <commctrl.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>

#pragma hdrstop
#include <blackbox.h>

// Size of temporary string buffer for messages
#define MAXBUF 256

// Scaling factor for LCD display image on the screen. LCD display window is
// automatically resized to match this scaling factor.
#define SCALE 2

// Format string passed to SetWindowTextf() in On_update_tick() for displaying
// the values of internal registers. If the simulation is not currently running,
// then all registers are displayed as "$??" which is the VMLAB convention.
#define NUMFMT (bStarted ? "$%02X" : "$??")

// Return the minimum of two values
#define MIN(a,b) ((a) < (b) ? (a) : (b))

// Add a given rectangle region to the total pending update region. Used to
// delay redrawing of the LCD bitmap until the next On_update_tick()
#define UPDATE_RECT(r) UnionRect(&VAR(UpdateRect), &VAR(UpdateRect), &(r));

void SetWindowTextf(HWND pHandle, const char *pFormat, ...);

// DIB (Device Independent Bitmap) header with two palette color entries
// (colors entries must immediately follow the BITMAPINFOHEADER in memory)
typedef struct
{
   BITMAPINFOHEADER header;
   RGBQUAD BLColour; // The brightness adjusted colour of the background
   RGBQUAD FGColour; // Individual pixel (foreground) color; always black
} DIBHeader_t;

// Constant initializer for the per-instance bitmap header in VAR block
const DIBHeader_t DIB_HEADER_INIT =
{
   {
      sizeof(BITMAPINFOHEADER),  // biSize (size of structure; required)
      128,    // biWidth (bitmap width in pixels)
      -64,    // biHeight (height in pixels; negative means a top-down bitmap)
      1,      // biPlanes (number of color planes; always 1)
      8,      // biBitCount (8 bits per pixel makes LCDData array easy to index)
      BI_RGB, // biCompression (no compression on LCDData)
      0,      // biSizeImage (not used with uncompressed image)
      0,      // biXPelsPerMeter (physical device resolution; not used)
      0,      // biYPelsPerMeter (physical device resolution; not used)
      2,      // biClrUsed (two color entries after this header)
      0,      // biClrImportant (all colors required for drawing bitmap)
   },
   { 0x4C, 0x1C, 0x0E }, // Initial background color is with backlight off
   { 0x00, 0x00, 0x00 }  // Individual pixel color is always black
};

// RGBQUAD constants for updating backlight colour in On_time_step()
const RGBQUAD BL_COLOUR[] =
{
   { 0x4C, 0x1C, 0x0E },
   { 0x6D, 0x2C, 0x14 },
   { 0x8E, 0x3B, 0x1B },
   { 0xAF, 0x4B, 0x23 },
   { 0xD1, 0x5C, 0x29 },
   { 0xFF, 0x6A, 0x30 }
};

// Rectangle constants encoding screen update regions for each controller
const RECT RECT1 = { 0, 0, 64 * SCALE, 64 * SCALE };
const RECT RECT2 = { 64 * SCALE, 0, 128 * SCALE, 64 * SCALE };

int WINAPI DllEntryPoint(HINSTANCE, unsigned long, void*) {return 1;} // is DLL
//==============================================================================

//==============================================================================
// Declare pins here
//
DECLARE_PINS
   DIGITAL_IN(RS, 1);
   DIGITAL_IN(RW, 2);
   DIGITAL_IN(E, 3);
   DIGITAL_BID(D7, 4); //MSB
   DIGITAL_BID(D6, 5);
   DIGITAL_BID(D5, 6);
   DIGITAL_BID(D4, 7);
   DIGITAL_BID(D3, 8);
   DIGITAL_BID(D2, 9);
   DIGITAL_BID(D1, 10);
   DIGITAL_BID(D0, 11);//LSB
   DIGITAL_IN(CS1, 12);
   DIGITAL_IN(CS2, 13);
   DIGITAL_IN(Reset, 14);
   ANALOG_IN(LEDPos, 15);
   ANALOG_IN(LEDNeg, 16);
END_PINS

// =============================================================================
// Declare module global variables here.
// The reason for this is to keep a set of variables by instance
// To use a variable, do it through the the macro VAR(...)
//
DECLARE_VAR
	char LCDData[64][128]; // Raw bitmap; row-major order; must be DWORD aligned
   DIBHeader_t DIBHeader; // Copy of DIB_HEADER_INIT; BLColour can change
   HBRUSH BLBrush;        // For painting blank square when display is off
   bool bLeftActive;
   bool bRightActive;
   int Pos1;
   int Pos2;
   int Page1;
   int Page2;
   int iDispStart1; //start offset, for scrolling the whole display
   int iDispStart2;
   int OutRegister1; // Buffer registers used for reading out data
   int OutRegister2;
	bool BUSY1; //Busy flags for each controller
	bool BUSY2; //Busy flags for each controller
	bool bReset; //resering flag
   int VBacklight; // Current voltage level on backlight (rounded to nearest 1V)
   bool bUpdate;   // True if GUI should be refreshed in On_update_tick()
   RECT UpdateRect; // Portion of LCD display waiting to be redrawn
END_VAR
// You can delare also globals variable outside DECLARE_VAR / END_VAR, but if
// multiple instances of this cell are placed, all these instances will
// share the same variable.
//

bool bStarted;   // True if simulation started and interface functions work
WNDPROC StaticProc;     // Original window procedure for static controls


// Convenience wrapper around StretchDIBits()
void PaintDIB(HDC hdc, RECT &src, RECT &dst)
{
   StretchDIBits
   (
      hdc,                             // hdc (device context handle)
      dst.left,                        // XDest
      dst.top,                         // YDest
      dst.right - dst.left,            // nDestWidth
      dst.bottom - dst.top,            // nDestHeight
      src.left,                        // XSrc
      64 - src.bottom,                 // YSrc (upside-down coordinate system)
      src.right - src.left,            // nSrcWidth
      src.bottom - src.top,            // nSrcHeight
      VAR(LCDData),                    // lpBits (pointer to raw bitmap data)
      (BITMAPINFO *) &VAR(DIBHeader),  // lpBitsInfo (pointer to header)
      DIB_RGB_COLORS,                  // iUsage (bitmap palette has RGB triples)
      SRCCOPY                          // dwRop (raster mode: source copy)
   );
}

// Repaint part of the LCD bitmap given update rectangle in screen coordinates
// taking the vertical start line offset into account
void Paint(HDC hdc, RECT &rect, int scroll)
{
   // Ensure that src completely encloses (in bitmap coordinates) all the
   // pixels that are included in the update region (in window coordinates).
   // The left/top edges are rounded down and the right/bottom edges rounded up
   RECT src =
   {
      rect.left / SCALE, rect.top / SCALE,
      (rect.right + SCALE - 1) / SCALE, (rect.bottom + SCALE - 1) / SCALE
   };

   // If the vertical scroll offset is non-zero, the source rectangle may have
   // to be split into two halfs to handle the wrap-around effect. Compute the
   // height (from src.top) where the rectangle will be split. If top == full
   // height, then no wrap around occurs, and if top == 0, then full
   // wrap-around occurs.
   int split = MIN(src.bottom + scroll, 64) - MIN(src.top + scroll, 64);

   // When computing destination rectangle (in window coordinates) ensure it is
   // exactly a multiple of the bitmap source. Otherwise, scaling artifacts can
   // appear when another window partially covers the scaled LCD pixels.

   // Draw the top half before wrap-around
   if(split)
   {
      RECT src1 =
      {
         src.left, src.top + scroll,
         src.right, src.top + scroll + split
      };
      RECT dst1 =
      {
         src.left * SCALE, src.top * SCALE,
         src.right * SCALE, (src.top + split) * SCALE
      };
      PaintDIB(hdc, src1, dst1);
   }

   // Draw the bottom wrapped-around half
   if(src.bottom - src.top - split)
   {
      RECT src2 =
      {
         src.left, src.top + scroll + split - 64,
         src.right, src.bottom + scroll - 64
      };
      RECT dst2 =
      {
         src.left * SCALE, (src.top + split) * SCALE,
         src.right * SCALE, src.bottom * SCALE
      };
      PaintDIB(hdc, src2, dst2);
   }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
   RECT rect;
   HDC hdc;
	int Offset, rc;

   // Retrieve the component instance that was previously saved in
   // On_window_winit, so that the VAR() macros can work properly
   PRIVATE::_Instance_index = (int) GetProp(hwnd, "vmlab.index");

   switch (msg)
   {
      case WM_PAINT:
			hdc = BeginPaint(hwnd, &ps);

         // Brush is used with FillRect() in case a display is off and
         // has to be repainted with the solid backlight colour. When
         // the backlight brightness changes, the existing brush is
         // dealloacted and BLBrush set to NULL in 
         if(!VAR(BLBrush))
         {
            RGBQUAD &q = VAR(DIBHeader).BLColour;
            VAR(BLBrush) = CreateSolidBrush(RGB(q.rgbRed, q.rgbGreen, q.rgbBlue));
         }

         // Repaint controller 1 if it's within update region
         if(IntersectRect(&rect, &ps.rcPaint, &RECT1))
         {
            if(VAR(bLeftActive))
            {
               Paint(hdc, rect, VAR(iDispStart1));
            }
            else
            {
               FillRect(hdc, &rect, VAR(BLBrush));
            }
         }

         // Repaint controller 2 if it's within update region
         if(IntersectRect(&rect, &ps.rcPaint, &RECT2))
         {
            if(VAR(bRightActive))
            {
               Paint(hdc, rect, VAR(iDispStart2));
            }
            else
            {
               FillRect(hdc, &rect, VAR(BLBrush));
            }
         }

			EndPaint(hwnd, &ps);
         break;

      // When destroying the window, delete the property that was added in
      // On_window_init(), and allow the original window procedure for the static
      // control to handle WM_DESTROY
      case WM_DESTROY:
         RemoveProp(hwnd, "vmlab.index");
         break;
   }

   // Calling the existing static control proc is safer than just calling the
   // default proc. Since the WM_INIT message was processed by the static
   // w
   return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ============================================================================
// Say here if your component has an associated window or not. Pass as parameter
// the dialog resource ID (from .RC file) or 0 if no window
//
USE_WINDOW(WINDOW_USER_1);   // USE_WINDOW(0) for no window

// =============================================================================
// Callback functions. These functions are called by VMLAB at the proper time

const char *On_create()
//********************
// Perform component creation. It must return NULL if the creation process is
// OK, or a message describing the error cause. The message will show in the
// Messages window. Typical tasks: check passed parameters, open files,
// allocate memory,...
{
   // The first parameter specifies the LCD oscillator frequency (in Hz). If omitted,
   // the component assumes the minimum frequency of 50kHz
   // TODO: Wait until next VMLAB release where GET_PARAM(0) will return the total
   // count of available arguments.
/*
   double clock = GET_PARAM(1) == 0 ? 50e3 : GET_PARAM(1);
   if(clock < 50e3)
   {
      return "Oscillator frequency too low (minimum 50kHz)";
   }
   if(clock > 400e3)
   {
      return "Oscillator frequency too high (maximum 400kHz)";
   }
*/
   return NULL;
}

void On_window_init(HWND pHandle)
//*******************************
// Window initialization. Fill only if the component has an associated window
// -USE_WINDOW(..) not zero-. The Parameter pHandle brings the main component
// window handle. Typical tasks: fill controls with data, intitialize gadgets,
// hook an own Windows structure (VCL, MFC,...)
{
   VAR(DIBHeader) = DIB_HEADER_INIT;
   VAR(VBacklight) = 0;

   // The _Instance_index is user internally by the VAR() macro. Since the
   // window procedure can be called at any time, the _Instance_index will not
   // be automatically set by VMLAB. By saving this component's index as a
   // property of the static control window, it can be accessed later inside
   // the window procedure.
   SetProp(GET_HANDLE(GADGET10), "vmlab.index", (HANDLE)
      PRIVATE::_Instance_index);

   // Install window procedure now to paint the LCD as soon as it appears
   SetWindowLong(GET_HANDLE(GADGET10),GWL_WNDPROC,(LONG) (WNDPROC) WndProc);

   // Desired size of the LCD display window client area
   RECT size = { 0, 0, 128 * SCALE, 64 * SCALE };

   // Calculate window size from client area and window's border sizes
   AdjustWindowRectEx
   (
      &size,
      WS_CHILDWINDOW,
      false,
      WS_EX_CLIENTEDGE | WS_EX_STATICEDGE
   );

   // Resize the LCD display window
   SetWindowPos
   (
      GET_HANDLE(GADGET10),
      NULL,
      0,
      0,
      size.right - size.left,
      size.bottom - size.top,
      SWP_NOZORDER | SWP_NOMOVE
   );
}

void On_destroy()
//***************
// Destroy component. Free here memory allocated at On_create; close files
// etc.
{
   if(VAR(BLBrush))
   {
      DeleteObject(VAR(BLBrush));
   }
}

void Set_back_colour(const RGBQUAD &colour)
//***************
// Change the LCD background color to the one indicated. Called from
// On_time_step() when the blacklight voltage changes and from DoReset() when
// the simulation is shutting down.
{
   // Update background pallete entry in bitmap header
   VAR(DIBHeader).BLColour = colour;

   // Old GDI brush is no longer valid since the colour changed
   if(VAR(BLBrush))
   {
      DeleteObject(VAR(BLBrush));
      VAR(BLBrush) = NULL;
   }

   // Force a redraw of the entire bitmap
   UPDATE_RECT(RECT1);
   UPDATE_RECT(RECT2);
   VAR(bUpdate) = true;
}

void Update_backlight()
//*****************************
// Called periodically from On_update_tick() to set the background color if
// the backlight voltage has changed. This is a little more efficient than
// calling it from every On_time_step().
{
   // Do nothing if simulation not running; GET_VOLTAGE() may not work
   if(!bStarted)
   {
      return;
   }

   int V = (GET_VOLTAGE(LEDPos)-GET_VOLTAGE(LEDNeg));

   if(V != VAR(VBacklight))
   {
      if(V >= 0 && V <= 5)
      {
         Set_back_colour(BL_COLOUR[V]);
      }
      else
      {
         Set_back_colour(BL_COLOUR[5]);
      }

      VAR(VBacklight) = V;
   }

   // TODO: Instead of a fixed array of colors, the color could be computed
   // by using an "alpha blending" euqation: C=C1*a + C2*(1.0 - a) where a
   // ranges from 0.0 to 1.0
}

void On_simulation_begin()
//************************
// VMLAB informs you that the simulation is starting. Initialize pin values
// here Open files; allocate memory, etc.
{
   // Everything in the VAR() block is initialized to zero when the component
   // is first created, so there is no need to manually initialize it.
   // On_simulation_end() will re-initialize everything instead.

   // TODO: Is the display RAM initialized to zero on powerup/reset in a real
   // LCD? If not, then we could initialize it with random garbage here.
   bStarted = true;

   // TODO: Is this necessary? I think On_digital_in_edge() will report an
   // initial low value on the reset.
	VAR(bReset) = GET_LOGIC(Reset) == 0; //active low

   // Redraw register displayes with initial "$00" values instead of "$??"
   VAR(bUpdate) = true;
}

void On_simulation_end()
//**********************
// The simulation is ending and the LCD display is "powering off". Reset
// internal registers, bitmap, and background color to initial values to
// prepare for the next simulation run.
{
   bStarted = false;

   // TODO: Replace with memset()
   for(int y=0;y<64;y++)
	   for(int x=0;x<128;x++)
		   VAR(LCDData[y][x]) = 0;

	VAR(bLeftActive) = 0;
	VAR(bRightActive) = 0;
	VAR(Pos1) = 0;
	VAR(Pos2) = 0;
	VAR(Page1) = 0;
	VAR(Page2) = 0;
	VAR(iDispStart1) = 0;
	VAR(iDispStart2) = 0;
	VAR(OutRegister1) = 0;
	VAR(OutRegister2) = 0;
	VAR(BUSY1) = 0;
	VAR(BUSY2) = 0;

   Set_back_colour(BL_COLOUR[0]);
   VAR(VBacklight) = 0;

   // Redraw register displays with "$??" (VMLAB convention when not simulating)
   VAR(bUpdate) = true;
}

void On_update_tick(double pTime)
//**********************
// Called periodically to refresh the GUI display. Using this function, instead
// of updating the GUI each time something changes, improves simulation performance.
{
   // Check for voltage changes on LEDplus and LEDminus pins
   Update_backlight();

   // Do not redraw the controls if there is no update pending
   if(!VAR(bUpdate))
   {
      return;
   }

   // Redraw any parts of the LCD bitmap which have pending changes
   InvalidateRect(GET_HANDLE(GADGET10), &VAR(UpdateRect), 0);
   SetRectEmpty(&VAR(UpdateRect));

   SetWindowTextf(GET_HANDLE(GADGET2), NUMFMT, VAR(Pos1));
   SetWindowTextf(GET_HANDLE(GADGET6), NUMFMT, VAR(Pos2));
   SetWindowTextf(GET_HANDLE(GADGET3), NUMFMT, VAR(Page1));
   SetWindowTextf(GET_HANDLE(GADGET7), NUMFMT, VAR(Page2));
   SetWindowTextf(GET_HANDLE(GADGET11), NUMFMT, VAR(iDispStart1));
   SetWindowTextf(GET_HANDLE(GADGET12), NUMFMT, VAR(iDispStart2));
   SetWindowTextf(GET_HANDLE(GADGET13), NUMFMT, VAR(OutRegister1));
   SetWindowTextf(GET_HANDLE(GADGET14), NUMFMT, VAR(OutRegister2));
   SetWindowText(GET_HANDLE(GADGET1), VAR(bLeftActive) ? "On" : "Off");
   SetWindowText(GET_HANDLE(GADGET5), VAR(bRightActive) ? "On" : "Off");

   if(!bStarted)
   {
      SetWindowText(GET_HANDLE(GADGET4), "?");
      SetWindowText(GET_HANDLE(GADGET8), "?");
      return;
   }

   char *stat = "Idle";
   if(GET_LOGIC(E) == 1)
   {
      if(GET_LOGIC(RW) == 1)
      {
        stat = GET_LOGIC(RS) == 1 ? "rDat" : "rStat";
      }
      else
      {
        stat = GET_LOGIC(RS) == 1 ? "wDat" : "wInst";
      }
   }

   char *left = GET_LOGIC(CS1) == 0 ? stat : "Idle";
   char *right = GET_LOGIC(CS2) == 0 ? stat : "Idle";

   left = VAR(BUSY1) ? "Busy" : left;
   right = VAR(BUSY2) ? "Busy" : right;

   if(VAR(bReset))
   {
      left = "Rst";
      right = "Rst";
   }

   SetWindowText(GET_HANDLE(GADGET4), left);
   SetWindowText(GET_HANDLE(GADGET8), right);
}

void SetDataDrive(bool state)
{
   SET_DRIVE(D0, state);
   SET_DRIVE(D1, state);
   SET_DRIVE(D2, state);
   SET_DRIVE(D3, state);
   SET_DRIVE(D4, state);
   SET_DRIVE(D5, state);
   SET_DRIVE(D6, state);
   SET_DRIVE(D7, state);
}

int ReadDatabits(LOGIC bit[8])
{
	bit[0] = GET_LOGIC(D0);
	bit[1] = GET_LOGIC(D1);
	bit[2] = GET_LOGIC(D2);
	bit[3] = GET_LOGIC(D3);
	bit[4] = GET_LOGIC(D4);
	bit[5] = GET_LOGIC(D5);
	bit[6] = GET_LOGIC(D6);
	bit[7] = GET_LOGIC(D7);

	int byteValue = 0;
	for(int i = 0; i < 8; i++)
		byteValue |= (bit[i]<<i);

	return byteValue;
}

void IncPos(int Controller)
{
	if(Controller == 1)
	{
		if(++VAR(Pos1)>63) VAR(Pos1) = 0;
	}
	else
	{
		if(++VAR(Pos2)>63) VAR(Pos2) = 0;
	}
}

void WriteByte(LOGIC bit[8],int Controller)
{
	int Pos;
	int Page;
   int DispStart;
   bool Active;

	if(Controller == 1)
	{//1nd side
		Page = VAR(Page1);
		Pos = VAR(Pos1);
      Active = VAR(bLeftActive);
      DispStart = VAR(iDispStart1);
	}
	else
	{//2nd side
		Page = VAR(Page2);
		Pos = VAR(Pos2)+64;
      Active = VAR(bRightActive);
      DispStart = VAR(iDispStart2);
	}

	for(int i = 0;i<8;i++)
		VAR(LCDData[i+(Page*8)][Pos]) = (bit[i]==1);

   // If display controller is on, then track the updated region
   if(Active)
   {
      // Compute screen update region taking display start Z into account
      RECT rect =
      {
         Pos * SCALE, (Page * 8 - DispStart) * SCALE,
         (Pos + 1) * SCALE, ((Page + 1) * 8 - DispStart) * SCALE
      };

      // Adjust top/bottom edges if both wrap around due to display start Z
      if(rect.bottom <= 0)
      {
         rect.top += 64 * SCALE;
         rect.bottom += 64 * SCALE;
      }
      
      // If only one edge wrapped around, then update entire column
      else if(rect.top < 0)
      {
         rect.top = 0;
         rect.bottom = 64 * SCALE;
      }
      
      UPDATE_RECT(rect);
   }

	IncPos(Controller);
}

void On_digital_in_edge(PIN pDigitalIn, EDGE pEdge, double pTime)
//**************************************************************
// Response to a digital input pin edge. The EDGE type parameter (pEdge) can
// be RISE or FALL. Use pin identifers as declared in DECLARE_PINS
{
	if(pEdge == FALL)
	{
      // TODO: Should this be under case E? What happens if RS switches while
      // E is still high? Can you switch directly from reading data to
      // reading status? May need to test on real LCD. What happens if
      // chip select turns off while E still high (does the address NOT
      // increment)? What happens if RW toggles while E is high? I think it
      // should toggle the data direction on and off. In other words RS and
      // RW may both be LEVEL triggered signals
		SetDataDrive(false);

		switch(pDigitalIn)
		{
			case E:
				{
               // TODO: Issue warning for anything except status read
					if(VAR(bReset))//don't do this if in reset
						break;

               // Reading the status register is the only operation that will not put the
               // controller into the busy state. Even reading the output register
               // requires a busy cycle since the address counter has to be updated and
               // a new value loaded into the output register from data display memory
					if(GET_LOGIC(RW)!=1 || GET_LOGIC(RS)!=0)
					{
                  if(GET_LOGIC(CS1)==0)
                  {
						   if(VAR(BUSY1))
						   {
							   PRINT("Attempt to read/write while busy (left side)");
							   break;
						   }
						   VAR(BUSY1) = true;
						   REMIND_ME(50e-6, 1); //TODO: Use LCD oscillator period * 2
                  }
                  if(GET_LOGIC(CS2)==0)
                  {
						   if(VAR(BUSY2))
						   {
							   PRINT("Attempt to read/write while busy (right side)");
							   break;
						   }
						   VAR(BUSY2) = true;
						   REMIND_ME(50e-6, 2); //TODO: Use LCD oscillator period * 2
                  }
					}

               // Schedule a GUI refresh since registers/bitmap may change
               VAR(bUpdate) = true;

					LOGIC ReadByte[8];
					int DataRead = ReadDatabits(ReadByte);

					if(GET_LOGIC(RS)==1) //data mode
					{
						if(GET_LOGIC(RW) ==0) //write
						{
							if(GET_LOGIC(CS1)==0) //only if CS is low
							{
								WriteByte(ReadByte,1);
							}

							if(GET_LOGIC(CS2)==0)
							{
								WriteByte(ReadByte,2);
							}
						}
						else //read
						{
                     int Pos, Page, Controller, *OutRegister;

						   if(GET_LOGIC(CS1)==0)
						   {
							   Pos = VAR(Pos1);
							   Page = VAR(Page1);
                        Controller = 1;
                        OutRegister = &VAR(OutRegister1);
						   }
						   if(GET_LOGIC(CS2)==0)
						   {
							   Pos = VAR(Pos2)+64;
							   Page = VAR(Page2);
                        Controller = 2;
                        OutRegister = &VAR(OutRegister2);
						   }
                     if(GET_LOGIC(CS1)==0 || GET_LOGIC(CS2)==0)
                     {
                        *OutRegister = 0;
                        for(int i = 0;i<8;i++)
                           *OutRegister |= VAR(LCDData[Page*8+i][Pos]) << i;

                        IncPos(Controller);
                     }
						}

					}
					else //command
					{
					//PRINT("command mode");
						if(GET_LOGIC(RW) == 0) //writing
						{
							if((DataRead & 0xFE) == 0x3E)
							{
								//display on/off
								if(GET_LOGIC(CS1)==0)
								{
									VAR(bLeftActive) = DataRead & 0x01;
                           UPDATE_RECT(RECT1);
								}
								if(GET_LOGIC(CS2)==0)
								{
									VAR(bRightActive) = DataRead & 0x01;
                           UPDATE_RECT(RECT2);
								}
							}

							if((DataRead & 0xC0) == 0x40)
							{
                        // Set Y address
								if(GET_LOGIC(CS1)==0)
								{
									VAR(Pos1) = DataRead & 0x3F;
								}
								if(GET_LOGIC(CS2)==0)
								{
									VAR(Pos2) = DataRead & 0x3F;
								}

							}

							if((DataRead & 0xF8) == 0xB8)
							{
                        // Set X page
								if(GET_LOGIC(CS1)==0)
								{
									VAR(Page1) = DataRead & 0x07;
								}
								if(GET_LOGIC(CS2)==0)
								{
									VAR(Page2) = DataRead & 0x07;
								}

							};

							if((DataRead & 0xC0) == 0xC0)
							{
								// Set display start line Z
								if(GET_LOGIC(CS1)==0)
								{
									VAR(iDispStart1) = DataRead & 0x3F;
                           UPDATE_RECT(RECT1);
								}
								if(GET_LOGIC(CS2)==0)
								{
									VAR(iDispStart2) = DataRead & 0x3F;
                           UPDATE_RECT(RECT2);
								}
							}

						}
						else //read
						{
							//nothing to do here
						}
					}
				}
				break;

			case Reset:
				{
					VAR(bLeftActive) = 0;
               VAR(bRightActive) = 0;
               VAR(iDispStart1) = 0;
               VAR(iDispStart2) = 0;
	      		VAR(bReset) = true;
               UPDATE_RECT(RECT1);
               UPDATE_RECT(RECT2);
					break;
				}
			default: break;
		}
	}
	else //rise
	{
		switch(pDigitalIn)
		{
		case Reset:
			{
				VAR(bReset) = false;
				break;
			}
		case E:
			{
				if(GET_LOGIC(RW)==1)
					if(GET_LOGIC(RS)==0)
				{
				   // Read status
					SetDataDrive(true);

					SET_LOGIC(D0,0);
					SET_LOGIC(D1,0);
					SET_LOGIC(D2,0);
					SET_LOGIC(D3,0);
					SET_LOGIC(D4,VAR(bReset));
					if(GET_LOGIC(CS1)==0)
					{
						SET_LOGIC(D5,VAR(bLeftActive));
   					SET_LOGIC(D7,VAR(BUSY1));
					}
					else if(GET_LOGIC(CS2)==0)
					{
						SET_LOGIC(D5,VAR(bRightActive));
   					SET_LOGIC(D7,VAR(BUSY2));
					}
					SET_LOGIC(D6,0);

					if((GET_LOGIC(CS1)==0) && (GET_LOGIC(CS2)==0))
					{
						PRINT("Attempt to read with both CS1 and CS2 enabled!");
					}
				}
				else
				{
					// Read display data
               int *OutRegister;

					if(GET_LOGIC(CS1)==0)
					{
                  OutRegister = &VAR(OutRegister1);
					}
					if(GET_LOGIC(CS2)==0)
					{
                  OutRegister = &VAR(OutRegister2);
					}
					if((GET_LOGIC(CS1)==0) || (GET_LOGIC(CS2)==0))
					{
  						SetDataDrive(true);

						SET_LOGIC(D0, (*OutRegister & 0x01) == 0x01);
						SET_LOGIC(D1, (*OutRegister & 0x02) == 0x02);
						SET_LOGIC(D2, (*OutRegister & 0x04) == 0x04);
						SET_LOGIC(D3, (*OutRegister & 0x08) == 0x08);
						SET_LOGIC(D4, (*OutRegister & 0x10) == 0x10);
						SET_LOGIC(D5, (*OutRegister & 0x20) == 0x20);
						SET_LOGIC(D6, (*OutRegister & 0x40) == 0x40);
						SET_LOGIC(D7, (*OutRegister & 0x80) == 0x80);
					}
					if((GET_LOGIC(CS1)==0) && (GET_LOGIC(CS2)==0))
					{
						PRINT("Trying to read with both CS enabled - bad");
					}
				}
			}
		}
	}

}

void On_remind_me(double pTime, int pData)
//***************************************
// VMLAB notifies about a previouly sent REMIND_ME() function.
{
	if(pData == 1)
	{
		VAR(BUSY1) = 0;
	}
	if(pData == 2)
	{
		VAR(BUSY2) = 0;
	}

   // Redraw the status display to show idle
   VAR(bUpdate) = true;

   // TODO: If actively reading the status register then perhaps we should immediately
   // change the status output pin here. In other words, the status bits may be level
   // triggered.
}

void On_gadget_notify(GADGET pGadgetId, int pCode)
//************************************************
// A window gadget (control) is sending a notification.
{

}

void SetWindowTextf(HWND pHandle, const char *pFormat, ...)
//*************************
// Wrapper around SetWindowText() that provides printf() like functionality
{
   char strBuffer[MAXBUF];
   va_list argList;

   va_start(argList, pFormat);
   vsnprintf(strBuffer, MAXBUF, pFormat, argList);
   va_end(argList);

   SetWindowText(pHandle, strBuffer);
}
