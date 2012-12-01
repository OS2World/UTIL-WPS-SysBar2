/*
**  An information source example for SysBar/2 Pipe monitor
**
**  Just change some entries for your own needs, add appropriate pipes
**    in the pipe monitor and enjoy... :)
*/
'@echo off'
call RxFuncAdd 'SysDriveInfo', 'RexxUtil', 'SysDriveInfo';
call RxFuncAdd 'SysFileTree', 'RexxUtil', 'SysFileTree';
call RxFuncAdd 'SysSleep', 'RexxUtil', 'SysSleep';
do forever
  call ReportDriveSpace '\pipe\sb2_drive_c' SysDriveInfo( 'C:' )
  call ReportDriveSpace '\pipe\sb2_drive_d' SysDriveInfo( 'D:' )
  call ReportFileSize '\pipe\sb2_swap' 'd:\os2\system\swapper.dat' 'Sw'
  call ReportFileCount '\pipe\sb2_fido' 'd:\comm\fido\xenia\inbound\*' 'Inbound'
  call ReportFileCount '\pipe\sb2_mail' 'd:\comm\southsde\pmmail\dip_l321.act\inbox.fld\*.msg' 'Mail'
  call SysSleep 5
end
exit;

/*
**  Drive space reporter
**  Usage: ReportDriveSpace( _pipename_,  _driveinfo_ )
**    the _driveinfo_ parameter is the result of SysDriveInfo call
*/
ReportDriveSpace:
  parse arg sPipeName sDrive sFree sRest
  iSpace = sFree % 1048576
  'echo 'sDrive''iSpace' > 'sPipeName
return;

/*
**  File count reporter
**  Usage: ReportFileCount( _pipename_,  _filemask_, _comment_ )
*/
ReportFileCount:
  parse arg sPipeName sDir sName
  call SysFileTree sDir, aRes, 'F'
  if aRes.0 > 0 then 'echo 'sName': 'aRes.0' > 'sPipeName;
  else 'echo.> 'sPipeName;
return;

/*
**  File size reporter
**  Usage: ReportFileSize( _pipename_,  _filename_, _comment_ )
*/
ReportFileSize:
  parse arg sPipeName sFile sName
  call SysFileTree sFile, aRes, 'F'
  parse var aRes.1 sTime sDate sSize sRest
  iSize = sSize % 1048576
  'echo 'sName': 'iSize' > 'sPipeName;
return;
  
