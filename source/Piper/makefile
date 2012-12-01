!define BLANK ""

WATCOM = O:\WATCOM

all: sb2_pipe.exe

clean:
 del *.exe *.obj *.res *.err *.imp *.lk1 *.mbr *.map *.sym

collect.obj : cpuload\collect.cpp .AUTODEPEND
 *wpp386 cpuload\collect.cpp -i=$(WATCOM)\h;$(WATCOM)\h\os2;E:\mptn\include; -w4 -e25 -zq -otexan -d3 -bm -zm -4r -bt=os2 -mf -zp=1

pscoll.obj : cpuload\pscoll.cpp .AUTODEPEND
 *wpp386 cpuload\pscoll.cpp -i=$(WATCOM)\h;$(WATCOM)\h\os2;E:\mptn\include; -w4 -e25 -zq -otexan -d3 -bm -zm -4r -bt=os2 -mf -zp=1

sb2_pipe.obj : sb2_pipe.cpp .AUTODEPEND
 *wpp386 sb2_pipe.cpp -i=$(WATCOM)\h;$(WATCOM)\h\os2;E:\mptn\include; -w4 -e25 -zq -otexan -d3 -bm -4r -bt=os2 -mf -zp=1

sb2_pipe.res : sb2_pipe.rc .AUTODEPEND
 rc -i $(WATCOM)\h -i $(WATCOM)\h\os2 -r sb2_pipe.rc sb2_pipe.res

sb2_pipe.exe : collect.obj pscoll.obj sb2_pipe.obj ..\sysbar2.dll cpuload\si.lib sb2_pipe.res .AUTODEPEND
 @%write sb2_pipe.lk1 NAME sb2_pipe
 @%append sb2_pipe.lk1 FIL collect.obj,pscoll.obj,sb2_pipe.obj
 @%append sb2_pipe.lk1 LIBR cpuload\si.lib
!ifneq BLANK "..\sysbar2.dll"
 *wlib -q -n -b sb2_pipe.imp ..\sysbar2.dll
 @%append sb2_pipe.lk1 LIBR sb2_pipe.imp
!endif
 *wlink SYS os2v2 pm op maxe=25 op m op q @sb2_pipe.lk1
!ifneq BLANK "sb2_pipe.res"
 rc -i $(WATCOM)\h -i $(WATCOM)\h\os2 sb2_pipe.res sb2_pipe.exe
!endif

