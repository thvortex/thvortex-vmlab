# =============================================================================
# Makefile for building AVR peripherals (use with Borland BCC 5.5 in path)
#
# Copyright (C) 2010 Wojciech Stryjewski <thvortex@gmail.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation; either version 2.1 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
#


# All the DLL files that need to be included in "mculib"
all:	dummy168.dll timer0_168.dll timer2_168.dll timerN_168.dll \
      wdog.dll comp.dll

# Resource files need explicit dependencies (not handled by .autodepend)
dummy168.dll:     dummy168.res
timer0_168.dll:   timer0_168.res
timer2_168.dll:   timer2_168.res
timerN_168.dll:   timerN_168.res
wdog.dll:         wdog.res
comp.dll:         comp.res

dummy168.res:     version.h
timer0_168.res:   version.h timer0_168.h
timer2_168.res:   version.h timer2_168.h
timerN_168.res:   version.h timerN_168.h
wdog.res:         version.h wdog.h
comp.res:         version.h comp.h
         
# Suffixes directive needed to make implicit rules work properly
# Automatically track include files in .obj; doesn't work for includes in .rc
.suffixes: .cpp .rc .obj
.autodepend

# Directories, libraries, and startup .obj for linker and other tools
INCLUDE   = ${MAKEDIR}\..\include
LIBDIR    = ${MAKEDIR}\..\lib
STARTUP   = ${LIBDIR}\c0d32.obj
LIBS      = ${LIBDIR}\import32.lib ${LIBDIR}\cw32.lib

# Flags passed to C++ compiler, linker, and resource compiler
OPTFLAGS  = -O -O2 -Oc -Oi -OS -Ov
CPPFLAGS  = -I"${INCLUDE}" -H -w-par -WM- -vi -WD ${OPTFLAGS}
LDFLAGS   = -L"${LIBDIR}; ${LIBDIR}\psdk" -Tpd -aa -x
RFLAGS    = -I"${INCLUDE}"


# Delete all temporary (and final DLLs) files created while compiling
clean:
   del *.il? *.obj *.res *.tds
distclean: clean
   del *.dll
   
# Implicit rule for linking .obj and .res into .dll
.obj.dll:
   ilink32 ${LDFLAGS} ${STARTUP} $**,$@, ,${LIBS}, ,$&.res
