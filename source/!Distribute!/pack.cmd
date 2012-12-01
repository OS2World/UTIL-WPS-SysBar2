@echo off
copy ..\sysbar2.dll files
lxlite files\sysbar2.dll
lxlite ..\cd_player\sb2_cd.exe
lxlite ..\clock\sb2_clock.exe
lxlite ..\piper\sb2_pipe.exe
lxlite ..\task_switcher\sb2_switcher.exe
copy ..\cd_player\sb2_cd.exe files
copy ..\clock\sb2_clock.exe files\sb2_clck.exe
copy ..\piper\sb2_pipe.exe files
copy ..\task_switcher\sb2_switcher.exe files\sb2_tswt.exe
