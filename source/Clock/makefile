!define BLANK ""

WATCOM = O:\WATCOM

all: sb2_clock.exe

clean:
 del *.exe *.obj *.res *.err *.imp *.lk1 *.mbr *.map *.sym

sb2_clock.obj : sb2_clock.cpp .AUTODEPEND
 *wpp386 sb2_clock.cpp -i=$(WATCOM)\h;$(WATCOM)\h\os2 -w4 -e25 -zq -otexan -bm -4r -bt=os2 -mf -zp=1

sb2_clock.res : sb2_clock.rc .AUTODEPEND
 rc -i $(WATCOM)\h -i $(WATCOM)\h\os2 -r sb2_clock.rc sb2_clock.res

sb2_clock.exe : sb2_clock.obj ..\sysbar2.dll sb2_clock.res .AUTODEPEND
 @%write sb2_clock.lk1 NAME sb2_clock
 @%append sb2_clock.lk1 FIL sb2_clock.obj
 @%append sb2_clock.lk1 
!ifneq BLANK "..\sysbar2.dll"
 *wlib -q -n -b sb2_clock.imp ..\sysbar2.dll
 @%append sb2_clock.lk1 LIBR sb2_clock.imp
!endif
 *wlink SYS os2v2 pm op m op st=16384 op maxe=25 op q op symf @sb2_clock.lk1
!ifneq BLANK "sb2_clock.res"
 rc -i $(WATCOM)\h -i $(WATCOM)\h\os2 sb2_clock.res sb2_clock.exe
!endif
