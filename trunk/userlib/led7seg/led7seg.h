// =============================================================================
// Component name: led7seg v1.0
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

#define LED_NUM  8 // Number of LED segments in a single 7-segment display (+1 for decimal point)
#define DISP_NUM 8 // Number of 7-segment displays per control panel dialog window
#define ICON_NUM 6 // Number of distinct icon images used by the component

// These constants help define the resource IDs for all LED segment images. These are also used by the
// code to index an array containing the HANDLEs of all the loaded images.
#define ICON_BASE 200 // Base ID for all ICON resources
#define LED_H     0   // LED horizontal grayed-out off segment
#define LED_V     1   // LED vertical grayed-out off segment
#define LED_D     2   // LED decimal point grayed-out off segment
#define LED_H_ON  3   // LED horizontal red on segment
#define LED_V_ON  4   // LED vertical red on segment
#define LED_D_ON  5   // LED decimal point red on segment

// Dialog Base resource base IDs for the static text labels under each 7-segment display and for the
// segments themselves.
#define LABEL_BASE GADGET0
#define LED_BASE   (GADGET0 + 8)
