LD	 = ilink32
INCLUDE	 = ${MAKEDIR}\..\include
LIBDIR	 = ${MAKEDIR}\..\lib
LIBS	 = ${LIBDIR}\c0d32.obj,$@, , ${LIBDIR}\import32.lib ${LIBDIR}\cw32.lib, ,
CPPFLAGS = -I"${INCLUDE}" -H -w-par -R -WM- -vi -WD -o$@ -c $**
LDFLAGS	 = -L"${LIBDIR}; ${LIBDIR}\psdk" -Tpd -aa -x -Gn
DELFILES = *.obj *.dll *.tds *.res



all: trace comxch.dll comxchx.dll trace\comxch.dll trace\comxchx.dll

clean:
	del /s ${DELFILES}
	IF EXIST trace rmdir trace

trace:
	@IF NOT EXIST trace mkdir trace



comxch.dll: comxch.obj comxch.res
	${LD} ${LDFLAGS} comxch.obj ${LIBS} comxch.res

comxchx.dll: comxchx.obj comxch.res
	${LD} ${LDFLAGS} comxchx.obj ${LIBS} comxch.res

trace\comxch.dll: trace\comxch.obj comxch.res
	${LD} ${LDFLAGS} trace\comxch.obj ${LIBS} comxch.res

trace\comxchx.dll: trace\comxchx.obj comxch.res
	${LD} ${LDFLAGS} trace\comxchx.obj ${LIBS} comxch.res



comxchx.obj: comxch.cpp
	${CC} -DCOMXCHX ${CPPFLAGS}

trace\comxch.obj: comxch.cpp
	${CC} -DCOMTRACE ${CPPFLAGS}

trace\comxchx.obj: comxch.cpp
	${CC} -DCOMTRACE -DCOMXCHX ${CPPFLAGS}
