#CC=icc
#CC=bcc -I. -v -y
#CC=gcc -Zsys -Zomf -I. -s
#CC=wcl386 -s -fm -d2
CC=wcl386 -oneatx -zm -"op eliminate"

#cpuload.EXE : cpuload.obj pscoll.obj collect.obj callpeek.obj dbgheap2.obj
#     $(CC) cpuload.obj pscoll.obj collect.obj callpeek.obj dbgheap2.obj si.lib
cpuload.EXE : cpuload.obj pscoll.obj collect.obj
     $(CC) cpuload.obj pscoll.obj collect.obj si.lib

#     lxlite cpuload.exe

.cpp.obj :
    $(CC) -c $<

.asm.obj :
    $(CC) -c $<

cpuload.obj : cpuload.cpp pscoll.h
pscoll.obj  : pscoll.cpp pscoll.h
collect.obj : collect.cpp collect.h
callpeek.obj: mem\callpeek.asm
dbgheap2.obj: mem\dbgheap2.cpp

