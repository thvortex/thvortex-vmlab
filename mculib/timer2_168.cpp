// =============================================================================
// Component Name: ATMega168 "TIMER2" (asynchronous) peripheral.
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
#include <windows.h>
#include <commctrl.h>
#pragma hdrstop
#include <stdio.h>

#define IS_PERIPHERAL       // To distinguish from a normal user component
#include "blackbox.h"
#include "useravr.h"

#include "timer2_168.h"
#define TIMER_2
#define WORDSZ WORD8

#include "timer_168.cpp"
