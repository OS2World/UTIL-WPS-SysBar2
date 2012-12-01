!define BLANK ""

WATCOM = O:\WATCOM

all: sb2_cd.exe

clean:
 del *.exe *.obj *.res *.err *.imp *.lk1 *.mbr *.map *.sym

cdengine.obj : cdengine.cpp .AUTODEPEND
 *wpp386 cdengine.cpp -i=$(WATCOM)\h;$(WATCOM)\h\os2 -w4 -e25 -zq -otexan -bm -4r -bt=os2 -mf -zp=1

sb2_cd.obj : sb2_cd.cpp .AUTODEPEND
 *wpp386 sb2_cd.cpp -i=$(WATCOM)\h;$(WATCOM)\h\os2 -w4 -e25 -zq -otexan -bm -4r -bt=os2 -mf -zp=1

sb2_cd.res : sb2_cd.rc .AUTODEPEND
 rc -i $(WATCOM)\h;$(WATCOM)\h\os2 -i $(WATCOM)\h -i $(WATCOM)\h\os2 -r sb2_cd.rc sb2_cd.res

sb2_cd.exe : cdengine.obj sb2_cd.obj ..\sysbar2.dll sb2_cd.res .AUTODEPEND
 @%write sb2_cd.lk1 NAME sb2_cd
 @%append sb2_cd.lk1 FIL cdengine.obj,sb2_cd.obj
 @%append sb2_cd.lk1 
!ifneq BLANK "..\sysbar2.dll"
 *wlib -q -n -b sb2_cd.imp ..\sysbar2.dll
 @%append sb2_cd.lk1 LIBR sb2_cd.imp
!endif
 *wlink d all SYS os2v2 pm op m op maxe=25 op q op symf @sb2_cd.lk1
!ifneq BLANK "sb2_cd.res"
 rc -i $(WATCOM)\h -i $(WATCOM)\h\os2 sb2_cd.res sb2_cd.exe
!endif

