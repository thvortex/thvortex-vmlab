// =============================================================================
// Component Name: ATMega168 "TIMERN" (16 bit) peripheral.
//
// Copyright (C) 2009 Advanced MicroControllers Tools (http://www.amctools.com/)
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
// =============================================================================

#define GDT_TCNTnH   GADGET1
#define GDT_TCNTnL   GADGET2
#define GDT_OCRnAH   GADGET3
#define GDT_OCRnAL   GADGET4
#define GDT_OCRnBH   GADGET13
#define GDT_OCRnBL   GADGET5
#define GDT_ICRnH    GADGET16
#define GDT_ICRnL    GADGET17
#define GDT_TCCRnA   GADGET18
#define GDT_TCCRnB   GADGET19
#define GDT_TCCRnC   GADGET20

#define GDT_CLOCK    GADGET6
#define GDT_MODE     GADGET7
#define GDT_BUFA     GADGET8
#define GDT_BUFB     GADGET9
#define GDT_BUF      GADGET10
#define GDT_LOG      GADGET11
#define GDT_TOP      GADGET12
#define GDT_TMP      GADGET21
