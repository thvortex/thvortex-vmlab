// =============================================================================
// Component Name: AVR watchdog timer peripheral (Version 2 with WDT interrupt)
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

#define GDT_WDTCSR   GADGET1
#define GDT_TIME     GADGET2
#define GDT_MODE     GADGET3
#define GDT_CLOCK    GADGET4
#define GDT_LOG      GADGET5
