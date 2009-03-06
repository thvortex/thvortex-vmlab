LD	 = ilink32
INCLUDE	 = ${MAKEDIR}\..\include
LIBDIR	 = ${MAKEDIR}\..\lib
LIBS	 = ${LIBDIR}\c0d32.obj,$@, , ${LIBDIR}\import32.lib ${LIBDIR}\cw32.lib, ,
CPPFLAGS = -I"${INCLUDE}" -H -w-par -R -WM- -vi -WD -o$@ -c $**
LDFLAGS	 = -L"${LIBDIR}; ${LIBDIR}\psdk" -Tpd -aa -x -Gn
DELFILES = *.obj *.dll *.tds *.res



all: led7cc.dll led7ca.dll

clean:
	del ${DELFILES}

led7cc.dll: led7cc.obj led7seg.res
	${LD} ${LDFLAGS} led7cc.obj ${LIBS} led7seg.res

led7ca.dll: led7ca.obj led7seg.res
	${LD} ${LDFLAGS} led7ca.obj ${LIBS} led7seg.res

led7cc.obj: led7seg.cpp
	${CC} -DLED7CC ${CPPFLAGS}

led7ca.obj: led7seg.cpp
	${CC} -DLED7CA ${CPPFLAGS}
