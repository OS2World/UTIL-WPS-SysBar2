!define BLANK ""

WATCOM = O:\WATCOM
TOOLKIT = O:\TOOLKIT

all: sysbar2.dll

clean:
 del *.dll *.lib *.obj *.res *.err *.imp *.lk1 *.mbr *.map *.sym

sysbar2.obj : sysbar2.cpp .AUTODEPEND
 *wpp386 sysbar2.cpp -i=$(WATCOM)\h;$(WATCOM)\h\os2 -w4 -e25 -zq -otexan -hw -bd -bm -4r -bt=os2 -mf -zp=1

sysbar2.res : sysbar2.rc .AUTODEPEND
 rc -i $(WATCOM)\h -i $(WATCOM)\h\os2 -r sysbar2.rc sysbar2.res

sysbar2.dll : sysbar2.obj sysbar2.res .AUTODEPEND
 @%write sysbar2.lk1 NAME sysbar2
 @%append sysbar2.lk1 FIL sysbar2.obj
 @%append sysbar2.lk1 
!ifneq BLANK ""
 *wlib -q -n -b sysbar2.imp 
 @%append sysbar2.lk1 LIBR sysbar2.imp
!endif
 *wlink SYS os2v2 dll op m op st=20384 op maxe=25 op q op symf op many @sysbar2.lk1
!ifneq BLANK "sysbar2.res"
 rc -i $(WATCOM)\h -i $(WATCOM)\h\os2 sysbar2.res sysbar2.dll
!endif
 wlib -n -b sysbar2.lib +sysbar2.dll
