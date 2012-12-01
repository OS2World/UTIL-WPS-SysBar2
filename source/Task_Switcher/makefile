!define BLANK ""

WATCOM = O:\WATCOM

all: sb2_switcher.exe

clean:
 del *.exe *.obj *.res *.err *.imp *.lk1 *.mbr *.map *.sym

sb2_switcher.obj : sb2_switcher.cpp .AUTODEPEND
 *wpp386 sb2_switcher.cpp -i=$(WATCOM)\h;$(WATCOM)\h\os2 -w4 -e25 -zq -otexan -hw -bm -4r -bt=os2 -mf -zp=1

sb2_switcher.res : sb2_switcher.rc .AUTODEPEND
 rc -i $(WATCOM)\h -i $(WATCOM)\h\os2 -r sb2_switcher.rc sb2_switcher.res

sb2_switcher.exe : sb2_switcher.obj ..\sysbar2.dll ..\piper\cpuload\si.lib sb2_switcher.res .AUTODEPEND
 @%write sb2_switcher.lk1 NAME sb2_switcher
 @%append sb2_switcher.lk1 FIL sb2_switcher.obj
 @%append sb2_switcher.lk1 LIBR ..\piper\cpuload\si.lib
!ifneq BLANK "..\sysbar2.dll"
 *wlib -q -n -b sb2_switcher.imp ..\sysbar2.dll
 @%append sb2_switcher.lk1 LIBR sb2_switcher.imp
!endif
 *wlink SYS os2v2 pm op m op st=20000 op maxe=25 op q op symf @sb2_switcher.lk1
!ifneq BLANK "sb2_switcher.res"
 rc -i $(WATCOM)\h -i $(WATCOM)\h\os2 sb2_switcher.res sb2_switcher.exe
!endif

