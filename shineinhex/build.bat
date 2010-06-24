\masm32\bin\rc /V rsrc.rc
\masm32\bin\ml /c /coff /Cp ShineInHex.Asm
\masm32\bin\link /DLL /SUBSYSTEM:WINDOWS /RELEASE /DEF:ShineInHex.def ShineInHex.obj rsrc.res
