/*

  SysBar/2 Utility Set  version 0.23

  Pipe Monitor module

  ..................................................................

  Copyright (c) 1995-99  Dmitry I. Platonoff <dip@platonoff.com>
  Copyright (c) 2002,04  Max Alekseyev       <relf@os2.ru>

                         All rights reserved

  ..................................................................

  LICENSE
  ~~~~~~~
  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above
     copyright notice, this list of conditions and the following
     disclaimer in the documentation and/or other materials provided
     with the distribution.

  3. Redistributions of any form whatsoever, as well as all
     advertising materials mentioning features or use of this
     software (if any), must include the following acknowledgment:
     "This product includes software developed by Dmitry I. Platonoff".

  4. The names "SysBar/2" and "Dmitry I. Platonoff" must not be
     used to endorse or promote products derived from this software
     without prior written permission. For such permission, please
     contact dplatonoff@canada.com.

  5. Products derived from this software may not be called
     "SysBar/2" nor may "Dmitry I. Platonoff" appear in their
     contributor lists without prior written permission.

  ..................................................................

  DISCLAIMER
  ~~~~~~~~~~
  THIS SOFTWARE IS PROVIDED BY THE AUTHOR OR CONTRIBUTORS "AS IS"
  AND ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
  AUTHOR OR THE CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
  ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#define __IBMCPP__

#define INCL_BASE
#define INCL_DOSDATETIME
#define INCL_DOSDEVICES
#define INCL_DOSDEVIOCTL
#define INCL_DOSERRORS
#define INCL_DOSEXCEPTIONS
#define INCL_DOSFILEMGR
#define INCL_DOSNMPIPES
#define INCL_DOSPROCESS
#define INCL_DOSPROFILE
#define INCL_DOSSEMAPHORES
#define INCL_ERRORS
#define INCL_GPI
#define INCL_MODULEMGR
#define INCL_DOSNLS
#define INCL_WIN

#include <os2.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "..\SysBar2.h"
#include "sb2_pipe_res.h"
#include "cpuload\pscoll.h"
#include "cpuload\dosqss.h"

#define TCPV40HDRS

extern "C" {
#include <netdb.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
}

typedef unsigned long long ULLONG;

extern "C" {
  APIRET16 APIENTRY16 Dos16MemAvail( PULONG pulAvailMem );
}

//DosPerfSysCall
typedef APIRET ( APIENTRY DOSPERFSYSCALL ) ( ULONG ulCommand, ULONG ulParm1,
  ULONG ulParm2, ULONG ulParm3 );
typedef struct _CPUUTIL
{
  ULONG ulTimeLow;     /* Low 32 bits of time stamp      */
  ULONG ulTimeHigh;    /* High 32 bits of time stamp     */
  ULONG ulIdleLow;     /* Low 32 bits of idle time       */
  ULONG ulIdleHigh;    /* High 32 bits of idle time      */
  ULONG ulBusyLow;     /* Low 32 bits of busy time       */
  ULONG ulBusyHigh;    /* High 32 bits of busy time      */
  ULONG ulIntrLow;     /* Low 32 bits of interrupt time  */
  ULONG ulIntrHigh;    /* High 32 bits of interrupt time */
} CPUUTIL;
HMODULE hmDosCall1 = NULLHANDLE;
DOSPERFSYSCALL* dpsc = NULL;
/* Convert 8-byte (low, high) time value to ULLONG */
#define LL2F(high, low) ((ULLONG(high)<<32)+ULLONG(low))
#define CMD_KI_RDCNT          (0x63)
#define CMD_KI_ENABLE         (0x60)
// Maximal number of CPUs
#define CPUS (4)

typedef unsigned long ( _System INET_ADDR ) ( char *pszAddr );
typedef struct hostent * ( _System GETHOSTBYNAME ) ( char *pszHostname );
typedef unsigned short ( _System BSWAP ) ( unsigned short usValue );
typedef int ( _System SOCKET ) ( int iDomain, int iType, int iProtocol );
typedef int ( _System SOCLOSE ) ( int iSocket );
typedef int ( _System SOIOCTL ) ( int iSocket, int iCommand, char *pBuf, int iBufSize );
typedef int ( _System CONNECT ) ( int iSocket, struct sockaddr *Name, int iNameSize );
typedef int ( _System RECV ) ( int iSocket, char *pBuf, int iBufSize, int iFlags );
typedef int ( _System SEND ) ( int iSocket, char *pBuf, int iBufSize, int iFlags );

INET_ADDR* InetAddr = NULL;
GETHOSTBYNAME* GetHostByName = NULL;
BSWAP* BSwap = NULL;
SOCKET* Socket = NULL;
SOCLOSE* SoClose = NULL;
SOIOCTL* SoIOCtl = NULL;
CONNECT* Connect = NULL;
RECV* Recv = NULL;
SEND* Send = NULL;

typedef struct _Pop3URL
{
  char* pszUsername;
  char* pszPassword;
  char* pszHost;
  int iPort;
  char pData[1];
} Pop3URL;

// arbitrary magic constant
#define TRAF_INIT 0x98765432u
typedef struct _TrafData
{
  ULONG uInitialized;
  ULLONG lMaxCPS;
  ULLONG lBytes;
  int iLastOctets;
  ULONG uInitialOctets;
  time_t tInitialTime;
  time_t tLastTime;
} TrafData;

HMODULE hmTCP32DLL = NULLHANDLE;
HMODULE hmSo32DLL = NULLHANDLE;
const int iTCPCallCount = 3;
const int iSocketCallCount = 9;
char *pszSocketCalls[iSocketCallCount] =
{
  "INET_ADDR", "GETHOSTBYNAME", "BSWAP",
  "SOCKET", "SOCLOSE", "IOCTL", "CONNECT", "RECV", "SEND"
};
PFN* pSocketFns[iSocketCallCount] =
{
  ( PFN* ) &InetAddr,
  ( PFN* ) &GetHostByName,
  ( PFN* ) &BSwap,
  ( PFN* ) &Socket,
  ( PFN* ) &SoClose,
  ( PFN* ) &SoIOCtl,
  ( PFN* ) &Connect,
  ( PFN* ) &Recv,
  ( PFN* ) &Send
};


// Config value processing routines ------------------------------------------
inline int strcmp2( char *s1, char *s2, int iDefault = -1 )
{
  if ( ! s1 || ! s2 ) return iDefault;
  else return strcmp( s1, s2 );
}

inline int atoi2( char *s, int iDefault = 0 )
{
  if ( s && *s ) return atoi( s );
  else return iDefault;
}



HAB hab;
HWND hwPipeFrame = NULLHANDLE;
HWND hwPipeClient = NULLHANDLE;
HWND hmPopupMenu = NULLHANDLE;
HMODULE hmSysBar2Dll = NULLHANDLE;
//HDC hMemDC = NULLHANDLE;
//HPS hMemPS = NULLHANDLE;
//HBITMAP hbMem = NULLHANDLE;

FATTRS fatDesc;
FATTRS fatCell;
FONTDLG fd;
char szFamilyname[FACESIZE] = "";
char szDescFontSection[] = "desc_font";
char szCellFontSection[] = "pipe_font";

static short int bLastVisible = 0;
short int iTopmost = 1;
short int iLarge = 0;
short int bClearIt = 0;
short int iBindToCorner = 0;
short int bLockPosition = 0;
int iOrientation = 1;
int iCellSize[4] = { 20, 27, 20, 20 };
DescWindow *pPipeDesc;
//int iPipeBackground = 0;
short int bAlienMenu = 0;
int iDefaultTextColor = 0;
int iDefaultBackColor = 0;

char szDefaultColor[] = "-default-";

void SB2d_FillColorList( HWND hwnd, ULONG ulID, int iSelected )
{
  WinSendDlgItemMsg( hwnd, ulID, LM_INSERTITEM, ( MPARAM )
    LIT_END, MPFROMP( szDefaultColor ) );
  SB2_FillColorList( hwnd, ulID, iSelected );
}

int SB2d_ColorValue( int iIndex, int iDefault )
{
  return SB2_ColorValue( iIndex ? iIndex - 1 : iDefault );
}

char* SB2d_ColorName( int iIndex )
{
  return ( iIndex ? SB2_ColorName( iIndex - 1 ) : szDefaultColor );
}

int SB2d_ColorA2I( char *pszName )
{
  return
    ( strcmp( pszName, szDefaultColor ) ? SB2_ColorA2I( pszName ) + 1 : 0 );
}




int iCellToSetup = -1;
HWND hwLogToUpdate = NULLHANDLE;

// Config values delimiter
char Delimiter = ',';

#define NO_UseOwn  0x10
#define NO_UseGb   0x08
#define NO_UseMb   0x04
#define NO_UseKb   0x02
#define NO_Useb    0x01
typedef struct
{
  int bUseOptions;
  int iGbLimit;
  int iMbLimit;
  int iKbLimit;
  char szGbName[8];
  char szMbName[8];
  char szKbName[8];
  char szbName[8];
} NumberOptions;
NumberOptions DefaultNumberOptions = { 0xF, 10, 10, 10, "Gb", "Mb", "Kb", "b" };
char szNumberSection[] = "numbers";
char szNumberDefault[] = "default";
char szMiscSection[] = "misc";
NumberOptions *pCurrentOptionsSet = &DefaultNumberOptions;

void SaveNumberOptions( FILE *f, NumberOptions *pOptions )
{
  fprintf( f, "%i%c%i%c%i%c%i%c%s%c%s%c%s%c%s", 
    pOptions->bUseOptions, Delimiter,
    pOptions->iGbLimit, Delimiter,
    pOptions->iMbLimit, Delimiter,
    pOptions->iKbLimit, Delimiter,
    pOptions->szGbName, Delimiter,
    pOptions->szMbName, Delimiter,
    pOptions->szKbName, Delimiter,
    pOptions->szbName );
}

void LoadNumberOptions( char *s, NumberOptions *pOptions )
{
  memset( pOptions, 0, sizeof ( NumberOptions ) );
  s = SB2_ParseValue( s, pOptions->bUseOptions, Delimiter );
  s = SB2_ParseValue( s, pOptions->iGbLimit, Delimiter );
  s = SB2_ParseValue( s, pOptions->iMbLimit, Delimiter );
  s = SB2_ParseValue( s, pOptions->iKbLimit, Delimiter );
  s = SB2_ParseValue( s, pOptions->szGbName, Delimiter );
  s = SB2_ParseValue( s, pOptions->szMbName, Delimiter );
  s = SB2_ParseValue( s, pOptions->szKbName, Delimiter );
  s = SB2_ParseValue( s, pOptions->szbName, Delimiter );
}

int iPipeCount = 0;
HEV hevPipeGuard;
HEV hevTimed;
HEV hevHeavyTimed;
const int iPipeBufSize = 128;
UCHAR PipeBuffer[iPipeBufSize];

#define Condition_Off      0
#define Condition_LessThan 1
#define Condition_EqualsTo 2
#define Condition_MoreThan 3
char* pszConditionTitles[] = { "off", "<", "=", ">" };
const int iColorConditionCount = 3;
typedef struct
{
  short int iCondition;
  char szText[32];
  int iTextColor;
  int iBackColor;
} ColorCondition;

typedef struct
{
  int iCellStatus;
  int iCellType;
  int iCellTextColor;
  int iCellBackColor;
  int iCellSeconds;
  int iCellLogBufferSize;
  char szCellDesc[64];
  char szCellPrefix[64];
  char szCellEmpty[64];
  char szCellClickApp[256];
  char szCellChangeApp[256];
  char szCellLogFile[256];
  char* pszCellLog;
  NumberOptions noCellOptions;
  char szCellData1[256];
  ColorCondition Conditions[iColorConditionCount];

  int iCellWidth;
  int iCellLazyCounter;
  int iCellCounter;
  int iCellIndex;
  LHANDLE hCellHandle;
  char szCellData[iPipeBufSize];
  char szCellData0[iPipeBufSize];
} CellData;
CellData *pCells = NULL;
int iCellCount = 0;
int iCellIndexes = 0;
char szCellsCfg[] = "cells";
//char szCellDelimiters[] = ",";
const int iLazySeconds = 10;

char *pszCellTypes1[] =
{
  "CPU load meter",
  "available physical memory",
  "drive/partition free space",
  "file monitor",
  "battery status",
  "clock",
  "calendar",
  "system uptime",
  "system processes",
  "POP3 mailbox",
  "IP traffic: incoming",
  "IP traffic: outgoing",
  "custom pipe listener",
  "AICQ pipe: ICQ status",
  "Keyboard Layer/2 pipe: current layout",
  "SETI@HOME pipe: completion percentage",
  "ThermoProtect pipe: fan #1 speed",
  "ThermoProtect pipe: fan #2 speed",
  "ThermoProtect pipe: fan #3 speed",
  "ThermoProtect pipe: temperature probe #1, C",
  "ThermoProtect pipe: temperature probe #1, F",
  "ThermoProtect pipe: temperature probe #2, C",
  "ThermoProtect pipe: temperature probe #2, F",
  "ThermoProtect pipe: temperature probe #3, C",
  "ThermoProtect pipe: temperature probe #3, F",
  "ThermoProtect pipe: CPU core voltage A",
  "ThermoProtect pipe: CPU core voltage B",
  "ThermoProtect pipe: +3.3 voltage",
  "ThermoProtect pipe: +5.0 voltage",
  "ThermoProtect pipe: -5.0 voltage",
  "ThermoProtect pipe: +12.0 voltage",
  "ThermoProtect pipe: -12.0 voltage",
  "ThermoProtect pipe: CMOS battery voltage",
  "ThermoProtect pipe: +5.0 standby voltage",
  "ThermoProtect pipe: case intrusion"
};
char *pszCellTypes2[] =
{
  "cpuload",
  "physmem",
  "freespace",
  "files",
  "power",
  "clock",
  "calendar",
  "uptime",
  "processes",
  "pop3peek",
  "trafin",
  "trafout",
  "pipe",
  "pipe4aicq",
  "pipe4layer",
  "pipe4seti",
  "pipe4TP-fan1",
  "pipe4TP-fan2",
  "pipe4TP-fan3",
  "pipe4TP-temp#1",
  "pipe4TP-temp#1F",
  "pipe4TP-temp#2",
  "pipe4TP-temp#2F",
  "pipe4TP-temp#3",
  "pipe4TP-temp#3F",
  "pipe4TP-VcoreA",
  "pipe4TP-VcoreB",
  "pipe4TP-3.3V",
  "pipe4TP-5V",
  "pipe4TP-m5V",
  "pipe4TP-12V",
  "pipe4TP-m12V",
  "pipe4TP-CMOS",
  "pipe4TP-VSB",
  "pipe4TP-case"
};
#define MCT_CPULoad        0
#define MCT_PhysMem        1
#define MCT_FreeSpace      2
#define MCT_FileSize       3
#define MCT_Power          4
#define MCT_Clock          5
#define MCT_Calendar       6
#define MCT_Uptime         7
#define MCT_Processes      8
#define MCT_TCPDLLs0       9
#define MCT_POP3           9
#define MCT_TrafIn        10
#define MCT_TrafOut       11
#define MCT_TCPDLLs1      11
#define MCT_Pipe          12
#define MCT_Pipe4AICQ     13
#define MCT_Pipe4Layer    14
#define MCT_Pipe4SETI     15
#define MCT_Pipe4TPfan1   16
#define MCT_Pipe4TPfan2   17
#define MCT_Pipe4TPfan3   18
#define MCT_Pipe4TPtemp1  19
#define MCT_Pipe4TPtemp1F 20
#define MCT_Pipe4TPtemp2  21
#define MCT_Pipe4TPtemp2F 22
#define MCT_Pipe4TPtemp3  23
#define MCT_Pipe4TPtemp3F 24
#define MCT_Pipe4TPVcoreA 25
#define MCT_Pipe4TPVcoreB 26
#define MCT_Pipe4TP3V     27
#define MCT_Pipe4TP5V     28
#define MCT_Pipe4TPm5V    39
#define MCT_Pipe4TP12V    30
#define MCT_Pipe4TPm12V   31
#define MCT_Pipe4TPCMOS   32
#define MCT_Pipe4TPVSB    33
#define MCT_Pipe4TPcase   34
const int iCellTypeCount = 35;

#define Cell_Log_On            0x1
#define Cell_Log_Timestamping  0x2
#define Cell_Log_Append        0x4
#define Cell_Enabled           0x8
#define Cell_Click_App        0x10
#define Cell_Change_App       0x20
#define Cell_Expire           0x40
#define Cell_Check0           0x80
#define Cell_Check1          0x100
#define Cell_Check2          0x200
#define Cell_Check3          0x400
#define Cell_LazyShrink      0x800

char *pszCellParamNames[] = { "", "", "Drive", "File",
  "", "", "", "", "", "URL", "Interf.", "Interf.", "Pipe" };

const int iHiddenCheckCount = 4;
char *pszCellCheckNames[][iHiddenCheckCount] =
{
  { "~Limit to 99%",                              //CPU load meter
    "~Use newer DosPerfSysCall method", "~Second CPU", NULL },
  { NULL, NULL, NULL, NULL },                     //physical memory
  { NULL, NULL, NULL, NULL },                     //disk free space
  { "Show ~count", "Show ~size", NULL, NULL },    //file monitor
  { NULL, NULL, NULL, NULL },                     //battery status
  { "Show ~seconds", "AM/PM ~mode", NULL, NULL }, //clock
  { "Show ~day of the week", "Show ~month",       //calendar
    "Show ~year", "Display month ~name" },
  { "Show ~seconds", "Show ~days", NULL, NULL },  //uptime
  { "Show ~active task count",                    //processes
    "Show ~thread count", "Show ~module count",
    /*"Show ~process count"*/ NULL  },
  { "Show message ~count", "Show total ~size",    //POP3
    "Show e~rrors", "Show server ~output" },
  { "Show ~current CPS", "Show ~average CPS",     //incoming traffic
    "Show ~peak CPS", "Show ~total byte amount" },
  { "Show ~current CPS", "Show ~average CPS",     //outgoing traffic
    "Show ~peak CPS", "Show ~total byte amount" },
  { NULL, NULL, NULL, NULL }                      //pipe reader
};

char *pszCellDescs[iCellTypeCount] =
{
  "CPU Load",                                       //CPU load meter
  "Available Physical Memory",                      //physical memory
  "Drive ",                                         //disk free space
  "Files",                                          //file monitor
  "Battery Status",                                 //battery status
  "Clock",                                          //clock
  "Calendar",                                       //calendar
  "System Uptime",                                  //uptime
  "Active Tasks/Threads/Modules", //"/Processes"    //processes
  "POP3 Mailbox",                                   //POP3
  "Incoming IP Traffic",                            //incoming traffic
  "Outgoing IP Traffic",                            //outgoing traffic
  "Pipe",                                           //pipe reader
  "AICQ Status",                                    //AICQ pipe
  "Keyboard Layer/2",                               //Layer/2 pipe
  "SETI@HOME Completion Percentage",                //SETI@HOME pipe
  "Fan #1 Speed, RPM",                              //fan1 speed
  "Fan #2 Speed, RPM",                              //fan2 speed
  "Fan #3 Speed, RPM",                              //fan3 speed
  "Temperature Probe #1, Celsius",                  //temperature1C
  "Temperature Probe #1, Fahrenheit",               //temperature1F
  "Temperature Probe #2, Celsius",                  //temperature2C
  "Temperature Probe #2, Fahrenheit",               //temperature2F
  "Temperature Probe #3, Celsius",                  //temperature3C
  "Temperature Probe #3, Fahrenheit",               //temperature3F
  "CPU Core Voltage A, Volts",                      //core voltage A
  "CPU Core Voltage B, Volts",                      //core voltage B
  "+3.3 Voltage, Volts",                            //+3.3 voltage
  "+5.0 Voltage, Volts",                            //+5.0 voltage
  "-5.0 Voltage, Volts",                            //-5.0 voltage
  "+12.0 Voltage, Volts",                           //+12.0 voltage
  "-12.0 Voltage, Volts",                           //-12.0 voltage
  "CMOS Battery, Volts",                            //CMOS cattery
  "+5.0 Standby Voltage, Volts",                    //+5.0 standby voltage
  "Case Intrusion"                                  //case intrusion
};

char *pszCellPrefixes[iCellTypeCount] =
{
  "CPU: ",   //CPU load meter
  "Mem: ",   //physical memory
  "",        //disk free space
  "",        //file monitor
  "",        //battery status
  "",        //clock
  "",        //calendar
  "",        //uptime
  "",        //processes
  "Mail: ",  //POP3
  "In: ",    //incoming traffic
  "Out: ",   //outgoing traffic
  "",        //pipe reader
  "",        //AICQ pipe
  "",        //Layer/2 pipe
  "SETI: ",  //SETI@HOME pipe
  "Fan1: ",  //fan1 speed
  "Fan2: ",  //fan2 speed
  "Fan3: ",  //fan3 speed
  "Temp1: ", //temperature1C
  "Temp1: ", //temperature1F
  "Temp2: ", //temperature2C
  "Temp2: ", //temperature2F
  "Temp3: ", //temperature3C
  "Temp3: ", //temperature3F
  "CoreA: ", //core voltage A
  "CoreB: ", //core voltage B
  "+3.3: ",  //+3.3 voltage
  "+5.0: ",  //+5.0 voltage
  "-5.0: ",  //-5.0 voltage
  "+12.0: ", //+12.0 voltage
  "-12.0: ", //-12.0 voltage
  "CMOS: ",  //CMOS cattery
  "+5V: ",   //+5.0 standby voltage
  "Case: "   //case intrusion
};

char *pszCustomPipes[] =
{
  "\\PIPE\\AICQ",
  "\\PIPE\\KBDLAY2",
  "\\PIPE\\SETI-PROG.SB2",
  "\\PIPE\\TP-fan1",
  "\\PIPE\\TP-fan2",
  "\\PIPE\\TP-fan3",
  "\\PIPE\\TP-temp1",
  "\\PIPE\\TP-temp1F",
  "\\PIPE\\TP-temp2",
  "\\PIPE\\TP-temp2F",
  "\\PIPE\\TP-temp3",
  "\\PIPE\\TP-temp3F",
  "\\PIPE\\TP-VcoreA",
  "\\PIPE\\TP-VcoreB",
  "\\PIPE\\TP-3.3V",
  "\\PIPE\\TP-5V",
  "\\PIPE\\TP-m5V",
  "\\PIPE\\TP-12V",
  "\\PIPE\\TP-m12V",
  "\\PIPE\\TP-CMOS",
  "\\PIPE\\TP-VSB",
  "\\PIPE\\TP-case"
};


const int iHiddenControlCount = 6;
int iHiddenControls[iHiddenControlCount] =
{
  D_Cell_Expire,
  D_Cell_TimeoutT,
  D_Cell_Name,
  D_Cell_BrowseFile,
  D_Cell_CopyrightT,
  D_Cell_Choice,
};
int iHiddenControlSets[][iHiddenControlCount] =
{
  { 0, 1, 0, 0, 1, 0 },  //CPU load meter controls
  { 0, 1, 0, 0, 0, 0 },  //available physical memory
  { 0, 1, 0, 0, 0, 1 },  //disk free space monitor controls
  { 0, 1, 1, 1, 0, 0 },  //file monitor controls
  { 0, 1, 0, 0, 0, 0 },  //battery status monitor controls
  { 0, 1, 0, 0, 0, 0 },  //clock controls
  { 0, 1, 0, 0, 0, 0 },  //calendar controls
  { 0, 1, 0, 0, 0, 0 },  //uptime controls
  { 0, 1, 0, 0, 0, 0 },  //processes controls
  { 0, 1, 1, 0, 0, 0 },  //POP3 peeker controls
  { 0, 1, 0, 0, 0, 1 },  //incoming traffic monitor controls
  { 0, 1, 0, 0, 0, 1 },  //outgoing traffic monitor controls
  { 1, 0, 1, 0, 0, 0 }   //pipe reader controls
};


// Just a dummy data structure
typedef struct
{
  short int iSize;
} DummyData;
DummyData DummyInit = { sizeof ( DummyData ) };

// Configuration file stuff
char *pszIniFile = NULL;
char szSysBar2Pipe[] = "SysBar2_Piper";
char *pszYesNo[2] = { "no", "yes" };

int iWindowHeight = 0, iWindowWidth = 8;

enum ErrorIDs
{
  APM_CantInit    = 0,
  P3E_UnknownHost = 1,
  P3E_Socket      = 2,
  P3E_CantReach   = 3,
  P3E_UserSnd     = 4,
  P3E_UserRcv     = 5,
  P3E_User        = 6,
  P3E_PasswordSnd = 7,
  P3E_PasswordRcv = 8,
  P3E_Password    = 9,
  P3E_StatSnd     = 10,
  P3E_StatRcv     = 11,
  P3E_Stat        = 12
};
const char iErrorCount = 13;

char szErrorSection[] = "errors";
char* pszErrorIDs[] =
{
  "APM_CantInit",
  "P3E_UnknownHost",
  "P3E_Socket",
  "P3E_CantReach",
  "P3E_UserSnd",
  "P3E_UserRcv",
  "P3E_User",
  "P3E_PasswordSnd",
  "P3E_PasswordRcv",
  "P3E_Password",
  "P3E_StatSnd",
  "P3E_StatRcv",
  "P3E_Stat"
};

char* pszErrors[] =
{
  "APM error",                              //APM_CantInit
  "unknown host %s",                        //P3E_UnknownHost
  "can't create socket",                    //P3E_Socket
  "can't reach %s",                         //P3E_CantReach
  "error sending username %s",              //P3E_UserSnd
  "error receiving reply for username %s",  //P3E_UserRcv
  "no such user: %s",                       //P3E_User
  "error sending password for user %s",     //P3E_PasswordSnd
  "error receiving PASS reply for user %s", //P3E_PasswordRcv
  "wrong password for %s",                  //P3E_Password
  "error sending STAT command for %s",      //P3E_StatSnd
  "error receiving STAT reply for user %s", //P3E_StatRcv
  "can't make STAT for %s"                  //P3E_Stat
};


const int iDaysOfTheWeek = 7;
char szDaysOfTheWeek[iDaysOfTheWeek][32] =
  { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

const int iMonths = 12;
char szMonths[iMonths][32] =
{
  "Jan", "Feb", "Mar", "Apr", "May", "June",
  "July", "Aug", "Sep", "Oct", "Nov", "Dec"
};

#define FDT_Date   1
#define FDT_Time   2
#define FDT_Uptime 4

void FormatNumber( char *pszBuf, NumberOptions *pOptions, ULLONG dNumber,
  int iDisplayed = 0 )
{
  pszBuf += strlen( pszBuf );
  if ( iDisplayed ) *( pszBuf++ ) = '/';
  if ( ( pOptions->bUseOptions & NO_UseOwn ) != NO_UseOwn )
    pOptions = &DefaultNumberOptions;
  char *pszName = pOptions->szbName;

  if ( ( pOptions->bUseOptions & NO_UseGb ) &&
       ( dNumber/(1u<<30) >= pOptions->iGbLimit || !(pOptions->bUseOptions & (NO_UseMb | NO_UseKb | NO_Useb)) )
     )
  {
    pszName = pOptions->szGbName;
    dNumber /= (1u<<30);
  }
  else if ( (pOptions->bUseOptions & NO_UseMb) &&
            ( dNumber/(1u<<20) >= pOptions->iMbLimit || !(pOptions->bUseOptions & (NO_UseKb | NO_Useb)) ) 
          )
  {
    pszName = pOptions->szMbName;
    dNumber /= (1u<<20);
  }
  else if ( (pOptions->bUseOptions & NO_UseKb) &&
            ( dNumber/(1u<<10) >= pOptions->iKbLimit || !(pOptions->bUseOptions & NO_Useb) )
          )
  {
    pszName = pOptions->szKbName;
    dNumber /= (1u<<10);
  }

  sprintf( pszBuf, "%I64u%s", dNumber, pszName );
}

void FormatCount( char* pszBuf, int iCount, ULLONG dSize, CellData* pCell )
{
  if ( iCount )
  {
    if ( pCell->iCellStatus & Cell_Check0 )
    {
      sprintf( pszBuf, "%i", iCount );
      if ( pCell->iCellStatus & Cell_Check1 )
        strcat( pszBuf, " (" );
    }
    if ( pCell->iCellStatus & Cell_Check1 )
    {
      FormatNumber( pszBuf, &( pCell->noCellOptions ), dSize );
      if ( pCell->iCellStatus & Cell_Check0 )
        strcat( pszBuf, ")" );
    }
  }
}

void FormatDateTime( char *pszBuf, int iTimeOptions, int iDateOptions = 0, short siDateTime = FDT_Time ) {
  DATETIME dtIn = { 0 };
  COUNTRYCODE CountryCode = { 0 };
  COUNTRYINFO CountryInfo = { 0 };
  ULONG ulL = 0;
  DosQueryCtryInfo( sizeof ( COUNTRYINFO ), &CountryCode,
    &CountryInfo, &ulL );

  pszBuf[0] = 0;

  if ( siDateTime & FDT_Uptime ) {
    ULLONG ullTime;
    ULONG ulFreq;
    if( !DosTmrQueryTime((PQWORD)&ullTime) && !DosTmrQueryFreq(&ulFreq) ) ulL = ullTime / ulFreq;
    else {
      DosQuerySysInfo( QSV_MS_COUNT, QSV_MS_COUNT, &ulL, sizeof ( ULONG ) );
      ulL /= 1000L;
    }
    dtIn.seconds = ( UCHAR ) ( ulL % 60L );
    ulL /= 60L;
    dtIn.minutes = ( UCHAR ) ( ulL % 60L );
    ulL /= 60L;
    if ( iTimeOptions & Cell_Check1 ) {
      ULONG ulDays = ulL / 24L;
      if ( ulDays )
        sprintf( pszBuf, "%li day%s ", ulDays, ulDays > 1 ? "s" : "" );
      ulL %= 24L;
    }
    sprintf( pszBuf + strlen( pszBuf ), "%li%s%02i", ulL,
      CountryInfo.szTimeSeparator, short ( dtIn.minutes ) );
    if ( iTimeOptions & Cell_Check0 ) sprintf( pszBuf + strlen( pszBuf ),
      "%s%02i", CountryInfo.szTimeSeparator, short ( dtIn.seconds ) );
  }
  else {
    DosGetDateTime( &dtIn );

    if ( siDateTime & FDT_Time )
    {
      int iAMPM = 0;
      if ( iTimeOptions & Cell_Check1 )
      {
        if ( dtIn.hours >= 12 )
        {
          dtIn.hours -= 12;
          iAMPM = 1;
        }
        if ( dtIn.hours == 0 ) dtIn.hours = 12;
      }
      sprintf( pszBuf, "%i%s%02i", short ( dtIn.hours ),
        CountryInfo.szTimeSeparator, short ( dtIn.minutes ) );
      if ( iTimeOptions & Cell_Check0 ) sprintf( pszBuf + strlen( pszBuf ),
        "%s%02i", CountryInfo.szTimeSeparator, short ( dtIn.seconds ) );
      if ( iTimeOptions & Cell_Check1 ) strcat( pszBuf, iAMPM ? " pm" : " am" );
    }

    if ( siDateTime & FDT_Date )
    {
      if ( siDateTime & FDT_Time ) strcat( pszBuf, " " );

      if ( iDateOptions & Cell_Check0 ) sprintf( pszBuf + strlen( pszBuf ),
        "%s, ", szDaysOfTheWeek[dtIn.weekday] );

      int n1 = dtIn.day;
      int n3 = ( iDateOptions & Cell_Check2 ) ? dtIn.year : 0;
      char n2[32] = { 0 };
      if ( iDateOptions & Cell_Check1 )
      {
        if ( iDateOptions & Cell_Check3 )
        {
          CountryInfo.szDateSeparator[0] = ' ';
          CountryInfo.szDateSeparator[1] = 0;
          strcpy( n2, szMonths[short ( dtIn.month ) - 1] );
        }
        else sprintf( n2, "%02i", short ( dtIn.month ) );
      }

      switch ( CountryInfo.fsDateFmt )
      {
        case 0:
          if ( n2[0] ) sprintf( pszBuf + strlen( pszBuf ), "%s%s",
            n2, CountryInfo.szDateSeparator );
          sprintf( pszBuf + strlen( pszBuf ), "%02i", n1 );
          if ( n3 ) sprintf( pszBuf + strlen( pszBuf ), "%s%02i",
            n2[0] ? CountryInfo.szDateSeparator : " ", short ( n3 ) );
          break;

        case 1:
          sprintf( pszBuf + strlen( pszBuf ), "%02i", n1 );
          if ( n2[0] ) sprintf( pszBuf + strlen( pszBuf ), "%s%s",
            CountryInfo.szDateSeparator, n2 );
          if ( n3 ) sprintf( pszBuf + strlen( pszBuf ), "%s%02i",
            n2[0] ? CountryInfo.szDateSeparator : " ", short ( n3 ) );
          break;

        case 2:
          if ( n3 ) sprintf( pszBuf + strlen( pszBuf ), "%02i%s",
            short ( n3 ), n2[0] ? CountryInfo.szDateSeparator : " " );
          if ( n2[0] ) sprintf( pszBuf + strlen( pszBuf ), "%s%s",
            n2, CountryInfo.szDateSeparator );
          sprintf( pszBuf + strlen( pszBuf ), "%02i", n1 );
          break;
      }
    }
  }
}


// Cell config saver ---------------------------------------------------------
void SaveCellCfg( FILE *f )
{
  fprintf( f, "[%s]\ncount=%i", szCellsCfg, iCellCount );
  for ( int i = 0; i < iCellCount; i++ )
  {
    fprintf( f, "\n\ncell%03i=%s%c%s%c%s%c%s%c%s%c%i%c%i", 
      i, pszCellTypes2[pCells[i].iCellType], Delimiter,
      SB2d_ColorName( pCells[i].iCellTextColor ), Delimiter,
      pCells[i].szCellDesc, Delimiter,
      pCells[i].szCellPrefix, Delimiter,
      pCells[i].szCellEmpty, Delimiter,
      pCells[i].iCellSeconds, Delimiter,
      pCells[i].iCellStatus );
    fprintf( f, "\ncolors%03i=%s", i,
      SB2d_ColorName( pCells[i].iCellBackColor ) );
    for ( int j = 0; j < iColorConditionCount; j++ ) 
      fprintf( f, "%c%s%c%s%c%s%c%s", Delimiter,
      pszConditionTitles[pCells[i].Conditions[j].iCondition], Delimiter,
      pCells[i].Conditions[j].szText, Delimiter,
      SB2d_ColorName( pCells[i].Conditions[j].iTextColor ), Delimiter,
      SB2d_ColorName( pCells[i].Conditions[j].iBackColor ) );
    fprintf( f, "\nchangeapp%03i=%s", i, pCells[i].szCellChangeApp );
    fprintf( f, "\nclickapp%03i=%s", i, pCells[i].szCellClickApp );
    fprintf( f, "\nlog%03i=%i%c%s", i, pCells[i].iCellLogBufferSize, Delimiter, pCells[i].szCellLogFile );
    switch ( pCells[i].iCellType )
    {
      case MCT_Pipe:
      case MCT_FreeSpace:
      case MCT_FileSize:
      case MCT_POP3:
        fprintf( f, "\ndata%03i=%s", i, pCells[i].szCellData1 );
        break;
      case MCT_TrafIn:
      case MCT_TrafOut:
        fprintf( f, "\ndata%03i=%02d", i, *(int*)pCells[i].szCellData1 );
        break;
    }
    switch ( pCells[i].iCellType )
    {
      case MCT_FreeSpace:
      case MCT_FileSize:
      case MCT_PhysMem:
      case MCT_POP3:
      case MCT_TrafIn:
      case MCT_TrafOut:
        fprintf( f, "\nnumbers%03i=", i );
        SaveNumberOptions( f, &( pCells[i].noCellOptions ) );
        break;
    }
  }
  fprintf( f, "\n\n" );
}

// Cell config loader --------------------------------------------------------
void LoadCellCfg( IniFile *pCfg )
{
  iCellCount = atoi2( pCfg->Get( szCellsCfg, "count" ) );

  if ( iCellCount )
  {
    if ( ( pCells = new CellData[iCellCount] ) == 0 ) return;
    memset( pCells, 0, sizeof( CellData ) * iCellCount );
    char szTmp[32];
    char *pS;
    for ( int i = 0; i < iCellCount; i++ )
    {
      pCells[i].noCellOptions = DefaultNumberOptions;
      sprintf( szTmp, "cell%03i", i );

      if ( ( pS = pCfg->Get( szCellsCfg, szTmp ) ) )
      {
        pS = SB2_ParseValue( pS, szTmp, Delimiter );
        for ( int j = 0; j < iCellTypeCount; j++ )
          if ( ! strcmp( szTmp, pszCellTypes2[j] ) )
          {
            pCells[i].iCellType = j;
            break;
          }
        pS = SB2_ParseValue( pS, szTmp, Delimiter );
        pCells[i].iCellTextColor = SB2d_ColorA2I( szTmp );
        pS = SB2_ParseValue( pS, pCells[i].szCellDesc, Delimiter );
        pS = SB2_ParseValue( pS, pCells[i].szCellPrefix, Delimiter );
        pS = SB2_ParseValue( pS, pCells[i].szCellEmpty, Delimiter );
        pS = SB2_ParseValue( pS, pCells[i].iCellSeconds, Delimiter );
        pCells[i].iCellStatus = Cell_Enabled;
        pS = SB2_ParseValue( pS, pCells[i].iCellStatus, Delimiter );
        if ( pCells[i].iCellType >= MCT_TCPDLLs0 &&
          pCells[i].iCellType <= MCT_TCPDLLs1 && ! hmSo32DLL )
          pCells[i].iCellStatus &= ~Cell_Enabled;

        sprintf( szTmp, "colors%03i", i );
        if ( ( pS = pCfg->Get( szCellsCfg, szTmp ) ) )
        {
          pS = SB2_ParseValue( pS, szTmp, Delimiter );
          pCells[i].iCellBackColor = SB2d_ColorA2I( szTmp );
          for ( int j = 0; j < iColorConditionCount && pS; j++ )
          {
            pS = SB2_ParseValue( pS, szTmp, Delimiter );
            for ( int k = 0; k < 4; k++ )
              if ( ! strcmp( szTmp, pszConditionTitles[k] ) )
              {
                pCells[i].Conditions[j].iCondition = k;
                break;
              }
            if ( ! ( pS =
              SB2_ParseValue( pS, pCells[i].Conditions[j].szText, Delimiter ) ) ) break;
            pS = SB2_ParseValue( pS, szTmp, Delimiter );
            pCells[i].Conditions[j].iTextColor = SB2d_ColorA2I( szTmp );
            if ( pS )
            {
              pS = SB2_ParseValue( pS, szTmp, Delimiter );
              pCells[i].Conditions[j].iBackColor = SB2d_ColorA2I( szTmp );
            }
          }
        }

        sprintf( szTmp, "changeapp%03i", i );
        if ( ( pS = pCfg->Get( szCellsCfg, szTmp ) ) )
          pS = SB2_ParseValue( pS, pCells[i].szCellChangeApp, Delimiter );

        sprintf( szTmp, "clickapp%03i", i );
        if ( ( pS = pCfg->Get( szCellsCfg, szTmp ) ) )
          pS = SB2_ParseValue( pS, pCells[i].szCellClickApp, Delimiter );

        sprintf( szTmp, "log%03i", i );
        if ( ( pS = pCfg->Get( szCellsCfg, szTmp ) ) )
        {
          pS = SB2_ParseValue( pS, pCells[i].iCellLogBufferSize, Delimiter );
          pS = SB2_ParseValue( pS, pCells[i].szCellLogFile, Delimiter );
          if ( ! pCells[i].iCellLogBufferSize )
            pCells[i].iCellLogBufferSize = 2;
        }
        if ( ( pCells[i].iCellStatus & Cell_Log_On ) &&
          pCells[i].iCellLogBufferSize )
        {
          int iLogSize = pCells[i].iCellLogBufferSize * 1024;
          if ( pCells[i].pszCellLog = new char[iLogSize] )
            memset( pCells[i].pszCellLog, 0, iLogSize );
          else pCells[i].iCellStatus &= ~Cell_Log_On;
        }

        switch ( pCells[i].iCellType )
        {
          case MCT_Pipe:
          case MCT_FreeSpace:
          case MCT_FileSize:
          case MCT_POP3:
            sprintf( szTmp, "data%03i", i );
            if ( ( pS = pCfg->Get( szCellsCfg, szTmp ) ) )
              pS = SB2_ParseValue( pS, pCells[i].szCellData1, Delimiter );
            break;
          case MCT_TrafIn:
          case MCT_TrafOut:
            sprintf( szTmp, "data%03i", i );
            if ( ( pS = pCfg->Get( szCellsCfg, szTmp ) ) )
              pS = SB2_ParseValue( pS, *(int*)pCells[i].szCellData1, Delimiter );
            break;
        }

        switch ( pCells[i].iCellType )
        {
          case MCT_FreeSpace:
          case MCT_FileSize:
          case MCT_PhysMem:
          case MCT_POP3:
          case MCT_TrafIn:
          case MCT_TrafOut:
            sprintf( szTmp, "numbers%03i", i );
            if ( ( pS = pCfg->Get( szCellsCfg, szTmp ) ) )
              LoadNumberOptions( pS, &( pCells[i].noCellOptions ) );
            break;
        }
      }
    }
  }
}


// Abnormal startup handler --------------------------------------------------
void AbortStartup( void )
{
  PERRINFO pErrInfoBlk;
  PSZ pszOffSet;
  PSZ pszErrMsg;

  if ( ( pErrInfoBlk = WinGetErrorInfo( hab ) ) != ( PERRINFO ) NULL )
  {
    pszOffSet = ( ( PSZ ) pErrInfoBlk ) + pErrInfoBlk->offaoffszMsg;
    pszErrMsg = ( ( PSZ ) pErrInfoBlk ) + *( ( PSHORT ) pszOffSet );
    if ( ( int ) hwPipeFrame && ( int ) hwPipeClient )
      WinMessageBox( HWND_DESKTOP, hwPipeFrame, ( PSZ ) pszErrMsg,
        "SysBar/2 Pipe Monitor startup error :(", 8999,
        MB_MOVEABLE | MB_ERROR | MB_CANCEL );
    WinFreeErrorInfo( pErrInfoBlk );
  }
  WinPostMsg( hwPipeClient, WM_QUIT, ( MPARAM ) NULL, ( MPARAM ) NULL );
}




// Pop-up description window stuff and routines ------------------------------
int iDescCell = -1;
int iLastCell = -1;

void HideDescWindow( void )
{
  if ( iDescCell != -1 )
  {
    pPipeDesc->Hide();
    iDescCell = -1;
  }
}

void AdjustDescPos( int x, int y, short int bHideOnly = 1 )
{
  if ( x > iWindowWidth - 7 || x < 4 || y > iWindowHeight - 2 || y < 2 ||
    bAlienMenu )
  {
    HideDescWindow();
    return;
  }
  int iXOffset = 4;
  for ( int i = 0; i < iCellCount; i++ ) if ( pCells[i].iCellWidth )
  {
    if ( x >= iXOffset && x < ( iXOffset + pCells[i].iCellWidth ) )
    {
      if ( iDescCell == i ) return;
      iLastCell = iDescCell = i;
      break;
    }
    iXOffset += pCells[i].iCellWidth;
  }
  if ( i >= iCellCount || bHideOnly ) return;

  pPipeDesc->SetText( pCells[iDescCell].szCellDesc );
  pPipeDesc->AdjustSize( &fatDesc, hwPipeClient );
  SWP swp;
  WinQueryWindowPos( hwPipeFrame, &swp );
  pPipeDesc->AdjustPos( swp.x + iXOffset, swp.y + 2, 0, iWindowHeight - 4, 0 );
}

MRESULT EXPENTRY SwDescWinProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
  switch ( msg )
  {
    case WM_PAINT:
      pPipeDesc->Paint( &fatDesc, hwnd );
      break;
  
    default:
      return WinDefWindowProc( hwnd, msg, mp1, mp2 );
  }
  return ( MRESULT ) FALSE;
}


void ResizeWindow( int iDiff = 0 )
{
  HideDescWindow();
  SWP Swp;
  WinQueryWindowPos( hwPipeFrame, &Swp );
  iCellSize[2] = fatCell.lMaxBaselineExt + 4;
  iWindowHeight = iCellSize[iLarge] + 6;
  iWindowWidth = 8;
  for ( int i = 0; i < iCellCount; i++ )
    iWindowWidth += pCells[i].iCellWidth;
  if ( iDiff && ! iOrientation ) Swp.x -= iDiff;

  WinSetWindowPos( hwPipeFrame, HWND_TOP, Swp.x, Swp.y, iWindowWidth,
    iWindowHeight, SWP_MOVE | SWP_SIZE | SWP_SHOW );
  WinInvalidateRect( hwPipeFrame, NULL, TRUE );
}

void ClosePipe( int iIndex )
{
  if ( pCells[iIndex].iCellType != MCT_Pipe ) return;

  if ( pCells[iIndex].hCellHandle )
  {
    DosDisConnectNPipe( pCells[iIndex].hCellHandle );
    DosClose( pCells[iIndex].hCellHandle );
  }
  pCells[iIndex].hCellHandle = 0;
  iPipeCount--;
}

void OpenPipe( int iIndex )
{
  if ( pCells[iIndex].iCellType != MCT_Pipe ) return;

  if ( ! pCells[iIndex].szCellDesc[0] ) strncpy( pCells[iIndex].szCellDesc,
    pCells[iIndex].szCellData1, sizeof( pCells[iIndex].szCellDesc ) - 1 );

  pCells[iIndex].iCellIndex = ++iCellIndexes;
  if ( ! DosCreateNPipe( pCells[iIndex].szCellData1,
    &( pCells[iIndex].hCellHandle ), NP_ACCESS_INBOUND, NP_NOWAIT |
    NP_TYPE_BYTE | NP_READMODE_BYTE | 1, iPipeBufSize, iPipeBufSize, 0L ) )
  {
    DosSetNPipeSem( pCells[iIndex].hCellHandle, ( HSEM ) hevPipeGuard,
      pCells[iIndex].iCellIndex );
    DosConnectNPipe( pCells[iIndex].hCellHandle );
    iPipeCount++;
  }
  else pCells[iIndex].hCellHandle = 0;
}

void ResetPipe( int iIndex )
{
  DosDisConnectNPipe( pCells[iIndex].hCellHandle );
  DosConnectNPipe( pCells[iIndex].hCellHandle );
}


void PaintCell( HPS hps, int iIndex, int bFillBackground = 0 )
{
  int iBackColor = pCells[iIndex].iCellBackColor;
  int iTextColor = pCells[iIndex].iCellTextColor;

  if ( pCells[iIndex].szCellData[0] )
  {
    char* pS = pCells[iIndex].szCellData +
      strlen( pCells[iIndex].szCellPrefix );
    int l1 = strlen( pS );
    for ( int i = 0; i < iColorConditionCount; i++ )
    {
      ColorCondition* pCondition = &( pCells[iIndex].Conditions[i] );
      if ( pCondition->iCondition )
      {
        int l2 = strlen( pCondition->szText );
        if ( ( pCondition->iCondition == Condition_LessThan && ( l1 < l2 ||
            ( l1 == l2 && strcmpi( pS, pCondition->szText ) < 0 ) ) ) ||
          ( pCondition->iCondition == Condition_EqualsTo && ( l1 == l2 &&
            ! strcmpi( pS, pCondition->szText ) ) ) ||
          ( pCondition->iCondition == Condition_MoreThan && ( l1 > l2 ||
            ( l1 == l2 && strcmpi( pS, pCondition->szText ) > 0 ) ) ) )
        {
          iBackColor = pCondition->iBackColor;
          iTextColor = pCondition->iTextColor;
          break;
        }
      }
    }
  }

  iBackColor = SB2d_ColorValue( iBackColor, iDefaultBackColor );
  iTextColor = SB2d_ColorValue( iTextColor, iDefaultTextColor );

  int iXOffset = 4;
  for ( int i = 0; i < iIndex; i++ ) iXOffset += pCells[i].iCellWidth;
  if ( bFillBackground ) SB2_Border( hps, iXOffset, 3, iXOffset +
    pCells[iIndex].iCellWidth - 1, 2 + iCellSize[iLarge], 0, iBackColor );
  if ( pCells[iIndex].szCellData[0] )
  {
    GpiCreateLogFont( hps, NULL, 200L, &fatCell );
    GpiSetCharSet( hps, 200L );
    CHARBUNDLE cb;
    cb.lColor = iTextColor;
    cb.usTextAlign = TA_LEFT | TA_HALF;
    GpiSetAttrs( hps, PRIM_CHAR, CBB_COLOR | CBB_TEXT_ALIGN, 0, &cb );
    POINTL pt;
    pt.x = iXOffset + 1;
    pt.y = ( iCellSize[iLarge] >> 1 ) + 3;
    RECTL rc1;
    rc1.xLeft = iXOffset + 1;
    rc1.yBottom = 4;
    rc1.xRight = rc1.xLeft + pCells[iIndex].iCellWidth - 3;
    rc1.yTop = iCellSize[iLarge] + 1;
    ULONG uOptions = CHS_CLIP | CHS_OPAQUE;
    GpiSetBackColor( hps, iBackColor );
    GpiCharStringPosAt( hps, &pt, &rc1, uOptions,
      strlen( pCells[iIndex].szCellData ), pCells[iIndex].szCellData, NULL );
    GpiDeleteSetId( hps, 200L );
  }
}

void PaintCell( int iIndex, int bFillBackground = 0 )
{
  if ( pCells[iIndex].iCellWidth )
  {
    HPS hps = WinGetPS( hwPipeClient );
    PaintCell( hps, iIndex, bFillBackground );
    WinReleasePS( hps );
  }
}


void AdjustPipeWidth( HPS hps, int iIndex )
{
  int iDiff = 0;

  if ( pCells[iIndex].szCellData[0] )
  {
    fatCell.usCodePage = GpiQueryCp( hps );
    GpiCreateLogFont( hps, NULL, 200L, &fatCell );
    GpiSetCharSet( hps, 200L );
    CHARBUNDLE cb;
    cb.usTextAlign = TA_LEFT | TA_BOTTOM;
    GpiSetAttrs( hps, PRIM_CHAR, CBB_TEXT_ALIGN, 0, &cb );
    POINTL ptl[TXTBOX_COUNT];
    GpiQueryTextBox( hps, strlen( pCells[iIndex].szCellData ),
      pCells[iIndex].szCellData, TXTBOX_COUNT, ptl );
    iDiff = ptl[TXTBOX_TOPRIGHT].x + 4;
    GpiDeleteSetId( hps, 200L );
  }
  iDiff -= pCells[iIndex].iCellWidth;

  if ( iDiff > 0 || ( iDiff < 0 && ! (
    ( pCells[iIndex].iCellStatus & Cell_LazyShrink ) &&
    pCells[iIndex].iCellLazyCounter ) ) )
  {
    pCells[iIndex].iCellLazyCounter = iLazySeconds;
    pCells[iIndex].iCellWidth += iDiff;
    ResizeWindow( iDiff );
  }
  else PaintCell( hps, iIndex );
}


// A procedure to save current configuration ---------------------------------
void SaveOptions( void )
{
  SWP Swp;
  if ( WinQueryWindowPos( hwPipeFrame, &Swp ) )
  {
    FILE *f = fopen( pszIniFile, "wt" );
    if ( f )
    {
      fprintf( f,
        "[%s]\n"
        "delimiter=%c\n"
        "x=%i\n"
        "y=%i\n"
        "orientation=%i\n"
        "size=%i\n"
        "customsize=%i\n"
        "cornerbind=%i\n"
        "topmost=%i\n"
        "lockposition=%s\n"
        "cellcolor=%s\n"
        "textcolor=%s\n\n",
        szSysBar2Pipe,
        Delimiter, 
        Swp.x + ( iOrientation ? 0 : iWindowWidth - 8 ), Swp.y,
        iOrientation, iLarge, iCellSize[3], iBindToCorner, iTopmost,
        pszYesNo[bLockPosition], SB2_ColorName( iDefaultBackColor ),
        SB2_ColorName( iDefaultTextColor ) );
      fprintf( f, "[%s]\n%s=", szNumberSection, szNumberDefault );
      SaveNumberOptions( f, &DefaultNumberOptions );
      fprintf( f, "\n\n" );

      SaveCellCfg( f );

      SB2_SaveFontCfg( szDescFontSection, &fatDesc, f );
      SB2_SaveFontCfg( szCellFontSection, &fatCell, f );

      fprintf( f, "[%s]\nweek=", szMiscSection );
      for ( int i = 0; i < iDaysOfTheWeek; i++ ) fprintf( f, "%s%c",
        szDaysOfTheWeek[i], ( i == iDaysOfTheWeek - 1 ) ? '\n' : Delimiter );
      fprintf( f, "month=" );
      for ( i = 0; i < iMonths; i++ )
        fprintf( f, "%s%c", szMonths[i], ( i == iMonths - 1 ) ? '\n' : Delimiter );

      fprintf( f, "\n[%s]\n", szErrorSection );
      for ( i = 0; i < iErrorCount; i++ )
        fprintf( f, "%s=%s\n", pszErrorIDs[i], pszErrors[i] );

      fclose( f );
    }
  }
}



// About dialog window procedure ---------------------------------------------
MRESULT EXPENTRY AboutDlgProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
  if ( msg == WM_INITDLG )
  {
    WinSetDlgItemText( hwnd, D_Version, SB2_Strings( iSB2S_Version ) );
    WinSetDlgItemText( hwnd, D_Copyright1, SB2_Strings( iSB2S_Copyright1 ) );
    WinSetDlgItemText( hwnd, D_Copyright2, SB2_Strings( iSB2S_Copyright2 ) );
  }
  return WinDefDlgProc( hwnd, msg, mp1, mp2 );
}


static HWND hwDlgToUpdate = NULL;
void UpdateSettingsDlg( void )
{
  if ( hwDlgToUpdate )
  {
    WinSendDlgItemMsg( hwDlgToUpdate, D_Bind_Off + iBindToCorner, BM_SETCHECK,
      ( MPARAM ) 1, 0 );
    WinSendDlgItemMsg( hwDlgToUpdate, D_Grow_Left + iOrientation, BM_SETCHECK,
      ( MPARAM ) 1, 0 );
    WinSendDlgItemMsg( hwDlgToUpdate, D_Lock_Position, BM_SETCHECK,
      ( MPARAM ) bLockPosition, 0 );
  }
}

// Display settings dialog window procedure ----------------------------------
MRESULT EXPENTRY SettingsDlgProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
  switch ( msg )
  {
    case WM_INITDLG:
      {
        hwDlgToUpdate = hwnd;
        WinSendDlgItemMsg( hwnd, D_Size_Size, SPBM_SETLIMITS,
          ( MPARAM ) 128, ( MPARAM ) iCellSize[3] );
        WinSendDlgItemMsg( hwnd, D_Size_Size, SPBM_SETLIMITS,
          ( MPARAM ) 128, ( MPARAM ) 16 );
        WinSendDlgItemMsg( hwnd, D_Display_None + iTopmost, BM_SETCHECK,
          ( MPARAM ) 1, 0 );
        WinSendDlgItemMsg( hwnd, D_Size_Small + iLarge, BM_SETCHECK,
          ( MPARAM ) 1, 0 );
        UpdateSettingsDlg();
      }
      break;

    case WM_COMMAND:
      switch ( SHORT1FROMMP( mp1 ) )
      {
        case D_Desc_Font_Settings:
          {
            fd.fAttrs = fatDesc;
            fd.clrFore = CLR_WHITE;
            fd.clrBack = CLR_DARKGRAY;
            HWND hwFontDlg = WinFontDlg( HWND_DESKTOP, hwnd, &fd );
            if ( hwFontDlg && ( fd.lReturn == DID_OK ) ) fatDesc = fd.fAttrs;
          }
          break;
        case D_Pipe_Font_Settings:
          {
            fd.fAttrs = fatCell;
            fd.clrFore = SB2_ColorValue( iDefaultTextColor );
            fd.clrBack = SB2_ColorValue( iDefaultBackColor );
            HWND hwFontDlg = WinFontDlg( HWND_DESKTOP, hwnd, &fd );
            if ( hwFontDlg && ( fd.lReturn == DID_OK ) )
            {
              fatCell = fd.fAttrs;
              HPS hps = WinGetPS( hwPipeClient );
              for ( int i = 0; i < iCellCount; i++ )
                AdjustPipeWidth( hps, i );
              WinReleasePS( hps );
              if ( iLarge == 2 && ( iCellSize[2] !=
                ( fatCell.lMaxBaselineExt + 4 ) ) ) ResizeWindow();
              if ( iBindToCorner ) WinPostMsg( hwPipeClient, WM_COMMAND,
                ( MPARAM ) ( MNU_PIPE_BIND_OFF + iBindToCorner ), 0 );
            }
          }
          break;
      }
      break;

    case WM_CONTROL:
      switch ( SHORT1FROMMP( mp1 ) )
      {
        case D_Lock_Position:
          if ( bLockPosition != ( int ) WinSendDlgItemMsg( hwnd,
            D_Lock_Position, BM_QUERYCHECK, 0, 0 ) ) WinPostMsg( hwPipeClient,
            WM_COMMAND, ( MPARAM ) ( MNU_PIPE_LOCKPOSITION ), 0 );
          break;
        case D_Grow_Left:
        case D_Grow_Right:
          {
            int s = ( int ) WinSendDlgItemMsg( hwnd, D_Grow_Left,
              BM_QUERYCHECKINDEX, 0, 0 );
            if ( ( s & 1 ) == s ) WinPostMsg( hwPipeClient, WM_COMMAND,
              ( MPARAM ) ( MNU_PIPE_ORNT_LEFT + s ), 0 );
          }
          break;
        case D_Display_None:
        case D_Display_OnTop:
        case D_Display_Popup:
        case D_Display_Popup2:
          {
            int s = ( int ) WinSendDlgItemMsg( hwnd, D_Display_None,
              BM_QUERYCHECKINDEX, 0, 0 );
            if ( ( s & 3 ) == s ) WinPostMsg( hwPipeClient, WM_COMMAND,
              ( MPARAM ) ( MNU_PIPE_NOTOP + s ), 0 );
          }
          break;
        case D_Bind_Off:
        case D_Bind_NW:
        case D_Bind_NE:
        case D_Bind_SW:
        case D_Bind_SE:
          {
            int s = ( int ) WinSendDlgItemMsg( hwnd, D_Bind_Off,
              BM_QUERYCHECKINDEX, 0, 0 );
            if ( s >= 0 && s <= 4 ) WinPostMsg( hwPipeClient, WM_COMMAND,
              ( MPARAM ) ( MNU_PIPE_BIND_OFF + s ), 0 );
          }
          break;
        case D_Size_Small:
        case D_Size_Large:
        case D_Size_Auto:
        case D_Size_Custom:
          {
            int s = ( int ) WinSendDlgItemMsg( hwnd, D_Size_Small,
              BM_QUERYCHECKINDEX, 0, 0 );
            if ( ( s & 3 ) == s ) WinPostMsg( hwPipeClient, WM_COMMAND,
              ( MPARAM ) ( MNU_PIPE_SMALL + s ), 0 );
          }
          break;
        case D_Size_Size:
          {
            long lValue;
            WinSendDlgItemMsg( hwnd, D_Size_Size, SPBM_QUERYVALUE, &lValue, 0 );
            if ( iCellSize[3] != lValue )
            {
              iCellSize[3] = lValue;
              if ( iLarge == 3 ) WinPostMsg( hwPipeClient, WM_COMMAND,
                ( MPARAM ) ( MNU_PIPE_CUSTOM ), 0 );
            }
          }
          break;
      }
      break;
    default:
      return WinDefDlgProc( hwnd, msg, mp1, mp2 );
  }
  return ( MRESULT ) FALSE;
}


void EnableMoveButtons( HWND hwnd, int iIndex )
{
  int bEnable = ( iCellCount > 1 );
  WinEnableControl( hwnd, D_Cell_Up, ( bEnable && iIndex ) );
  WinEnableControl( hwnd, D_Cell_Down,
    ( bEnable && ( iIndex < iCellCount - 1 ) ) );
}

void EnableListButtons( HWND hwnd )
{
  int bEnable = ( iCellCount > 0 );
  WinEnableControl( hwnd, D_Cell_Edit, bEnable );
  WinEnableControl( hwnd, D_Cell_Clone, bEnable );
  WinEnableControl( hwnd, D_Cell_Remove, bEnable );
  int i = ( int ) WinSendDlgItemMsg( hwnd, D_Cell_List, LM_QUERYSELECTION,
    ( MPARAM ) LIT_CURSOR, ( MPARAM ) 0 );
  EnableMoveButtons( hwnd, i );
}



void CleanupCell( int i )
{
  switch ( pCells[i].iCellType )
  {
    case MCT_Pipe:
      ClosePipe( i );
      break;
  }
}
void InitCell( int i )
{
  switch ( pCells[i].iCellType )
  {
    case MCT_Pipe:
      OpenPipe( i );
      break;
    case MCT_POP3:
      {
        Pop3URL* pUrl = ( Pop3URL* ) pCells[i].szCellData0;
        pUrl->pszUsername = pUrl->pszPassword = NULL;
        pUrl->pszHost = pUrl->pData;
        pUrl->iPort = 110;
        strcpy( pUrl->pData, pCells[i].szCellData1 );
        char* p1 = strrchr( pUrl->pData, '@' );
        char* p2 = strchr( pUrl->pData, ':' );
        if ( p1 )
        {
          pUrl->pszUsername = pUrl->pData;
          if ( p2 && p2 < p1 )
          {
            *p2 = 0;
            if ( p2[1] ) pUrl->pszPassword = p2 + 1;
            p2 = strchr( p1, ':' );
          }
          *p1 = 0;
        }
        if ( p1 && p1[1] ) pUrl->pszHost = p1 + 1;
        if ( p2 )
        {
          *p2 = 0;
          if ( p2[1] ) pUrl->iPort = atoi( p2 + 1 );
        }
      }
      break;
    case MCT_TrafIn:
    case MCT_TrafOut:
      memset( pCells[i].szCellData0, 0, sizeof ( pCells[i].szCellData0 ) );
      break;
  }
  if ( pCells[i].iCellWidth ) PaintCell( i, 1 );
}

void CopyCell( int iIndex )
{
  PSZ pszDest;
 
  if ( WinOpenClipbrd( hab ) )
  {
    if ( ! DosAllocSharedMem( ( PVOID* ) &pszDest, NULL, strlen(
      pCells[iIndex].szCellData ) + 1, PAG_WRITE | PAG_COMMIT | OBJ_GIVEABLE ) )
    {
//      pszSrc = pCells[iIndex].szCellData;
      strcpy( pszDest, pCells[iIndex].szCellData );
//      while (*pszDest++ = *pszSrc++);
      WinEmptyClipbrd( hab );
      WinSetClipbrdData( hab, ( ULONG ) pszDest, CF_TEXT, CFI_POINTER );
      WinCloseClipbrd(hab);
      DosFreeMem( pszDest );
    }
  }
}

void EmptyCell( int iIndex )
{
  pCells[iIndex].szCellData[0] = 0;
  int iOldWidth = pCells[iIndex].iCellWidth;
  pCells[iIndex].iCellWidth = 0;
  ResizeWindow( - iOldWidth );
}


void RemoveCell( int i )
{
  CleanupCell( i );
  if ( pCells[i].pszCellLog ) delete pCells[i].pszCellLog;
  int iOldWidth = pCells[i].iCellWidth;
  if ( --iCellCount )
  {
    if ( i < iCellCount ) memmove( &pCells[i], &pCells[i + 1],
      ( iCellCount - i ) * sizeof( CellData ) );
  }
  else
  {
    delete pCells;
    pCells = NULL;
  }
  ResizeWindow( - iOldWidth );
}


typedef struct
{
  int iIndex;
  char szText[iPipeBufSize];
} NewCellData;

char szLogBuffer[256] = { 0 };

void WriteCellLog( int iIndex, char* szBuf, short int bClearEOL = 0 )
{
  if ( pCells[iIndex].iCellStatus & Cell_Log_On )
  {
    if ( pCells[iIndex].iCellStatus & Cell_Log_Timestamping )
    {
      FormatDateTime( szLogBuffer, Cell_Check0, Cell_Check1 | Cell_Check2,
        FDT_Date | FDT_Time );
      strcat( szLogBuffer, "  " );
    }
    else szLogBuffer[0] = 0;
    strcat( szLogBuffer, szBuf );
    if ( bClearEOL )  for ( int i = strlen( szLogBuffer ) - 1; i >= 0 &&
      ( szLogBuffer[i] == '\n' || szLogBuffer[i] == '\r' ); i-- )
      szLogBuffer[i] = 0;
    strcat( szLogBuffer, "\r\n" );
    int l = strlen( szLogBuffer );

    if ( pCells[iIndex].iCellStatus & Cell_Log_Append &&
      pCells[iIndex].szCellLogFile[0] )
    {
      HFILE hFile = 0;
      ULONG ulAction = 0;
      if ( DosOpen( pCells[iIndex].szCellLogFile, &hFile, &ulAction,
        0, FILE_ARCHIVED | FILE_NORMAL, OPEN_ACTION_CREATE_IF_NEW |
        OPEN_ACTION_OPEN_IF_EXISTS, OPEN_FLAGS_NOINHERIT |
        OPEN_SHARE_DENYNONE | OPEN_ACCESS_WRITEONLY, 0 ) == NO_ERROR )
      {
        DosSetFilePtr( hFile, 0, FILE_END, &ulAction );
        DosWrite( hFile, ( PVOID ) szLogBuffer, l, &ulAction );
        DosClose( hFile );
      }
    }

    if ( pCells[iIndex].pszCellLog )
    {
      int iLogSize = pCells[iIndex].iCellLogBufferSize * 1024;
      while ( l + strlen( pCells[iIndex].pszCellLog ) >= iLogSize )
      {
        char *p = strchr( pCells[iIndex].pszCellLog, '\n' );
        if ( p ) strcpy( pCells[iIndex].pszCellLog, p + 1 );
        else pCells[iIndex].pszCellLog[0] = 0;
      }
      strcat( pCells[iIndex].pszCellLog, szLogBuffer );
      if ( iCellToSetup == iIndex && hwLogToUpdate ) WinSetDlgItemText(
        hwLogToUpdate, D_Log_MLE, pCells[iIndex].pszCellLog );
    }
  }
}


const int iCmdLineLength = iPipeBufSize + 256;

void SpawnIt( char *pszCommand, char *pszCellContent )
{
  char szCmdLine[iCmdLineLength] = { 0 };
  char szCmdBuffer[iCmdLineLength] = { 0 };
  char *p = szCmdLine, *pszCmdLine = szCmdBuffer;
  sprintf( szCmdBuffer, pszCommand, pszCellContent );
  short int bQuote = 0;
  while ( *pszCmdLine )
  {
    if ( *pszCmdLine == '\"' ) bQuote ^= 1;
    else if ( *pszCmdLine <= ' ' && ! bQuote )
    {
      *p++ = 0;
      while ( pszCmdLine[1] > 0 && pszCmdLine[1] <= ' ' ) pszCmdLine++;
    }
    else *p++ = ( *pszCmdLine > ' ' ? *pszCmdLine : ' ' );
    pszCmdLine++;
  }
  char *argv[32] = { 0 };
  p = szCmdLine;
  for ( int i = 0; i < 31 && *p; i++ )
  {
    argv[i] = p;
    p += strlen( p ) + 1;
  }
  spawnvp( P_NOWAIT, argv[0], ( char const * const * ) argv );
}


// Sets cell new text. Performs all necessary job such as
// cleaning all the ending linefeed and space characters,
// adding the prefix or applying the "when empty" message,
// writing the log, launching the "on change" application
// and updating the cell width if needed.
//
// Parameters:
//   iIndex - cell index
//        s - new text
void SetCellData( int iIndex, char *s )
{
  static char szBuf[iPipeBufSize];

  // text cleanup
  memset( szBuf, 0, iPipeBufSize );
  if ( s && s[0] )
  {
    char *p = strpbrk( s, "\r\n" );
    if ( p ) *p = 0;
    p = s + strlen( s );
    do *p = 0;
    while ( --p > s && ( *p == '\r' || *p == '\n' || *p == ' ' ) );

    if ( s[0] )
    {
      if ( pCells[iIndex].szCellPrefix[0] )
        strcpy( szBuf, pCells[iIndex].szCellPrefix );
      strcat( szBuf, s );

      WriteCellLog( iIndex, szBuf );
    }
  }
  short int bNotEmpty = 0;

  // dealing with the empty message
  if ( szBuf[0] == 0 && pCells[iIndex].szCellEmpty[0] )
  {
    strcpy( szBuf, pCells[iIndex].szCellPrefix );
    strcat( szBuf, pCells[iIndex].szCellEmpty );
  }
  else bNotEmpty = 1;

  // actions if updated
  if ( strcmp( pCells[iIndex].szCellData, szBuf ) &&
    ( pCells[iIndex].iCellStatus & Cell_Enabled ) )
  {
    strcpy( pCells[iIndex].szCellData, szBuf );
    if ( bNotEmpty && szBuf[0] && pCells[iIndex].szCellChangeApp[0] &&
      ( pCells[iIndex].iCellStatus & Cell_Change_App ) )
      SpawnIt( pCells[iIndex].szCellChangeApp, pCells[iIndex].szCellData );
//      spawnlp( P_NOWAIT, pCells[iIndex].szCellChangeApp,
//        pCells[iIndex].szCellChangeApp, NULL );

    HPS hps = WinGetPS( hwPipeClient );
    AdjustPipeWidth( hps, iIndex );
    WinReleasePS( hps );
    if ( pCells[iIndex].iCellType == MCT_Pipe &&
      ( pCells[iIndex].iCellStatus & Cell_Expire ) )
      pCells[iIndex].iCellCounter = pCells[iIndex].iCellSeconds + 1;
  }
}



static CellData TempCell;
static CellData* pCellToSetup = &TempCell;

void UpdateCell( CellData* pCell )
{
  if ( pCell != &TempCell && pCell->iCellStatus & Cell_Enabled &&
    pCell->iCellType != MCT_Pipe ) pCells->iCellCounter = 1;
}


// Number display options dialog window procedure ----------------------------
MRESULT EXPENTRY NumberDlgProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
  switch ( msg )
  {
    case WM_INITDLG:
      {
        WinSetDlgItemText( hwnd, D_Number_GbName,
          pCurrentOptionsSet->szGbName );
        WinSetDlgItemText( hwnd, D_Number_MbName,
          pCurrentOptionsSet->szMbName );
        WinSetDlgItemText( hwnd, D_Number_KbName,
          pCurrentOptionsSet->szKbName );
        WinSetDlgItemText( hwnd, D_Number_bName, pCurrentOptionsSet->szbName );

        WinSetDlgItemShort( hwnd, D_Number_GbLimit,
          pCurrentOptionsSet->iGbLimit, 0 );
        WinSetDlgItemShort( hwnd, D_Number_MbLimit,
          pCurrentOptionsSet->iMbLimit, 0 );
        WinSetDlgItemShort( hwnd, D_Number_KbLimit,
          pCurrentOptionsSet->iKbLimit, 0 );

        WinCheckButton( hwnd, D_Number_Gb,
          (pCurrentOptionsSet->bUseOptions & NO_UseGb) != 0 );
        WinCheckButton( hwnd, D_Number_Mb,
          (pCurrentOptionsSet->bUseOptions & NO_UseMb) != 0 );
        WinCheckButton( hwnd, D_Number_Kb,
          (pCurrentOptionsSet->bUseOptions & NO_UseKb) != 0 );
        WinCheckButton( hwnd, D_Number_b,
          (pCurrentOptionsSet->bUseOptions & NO_Useb) != 0 );
        if ( pCurrentOptionsSet != &DefaultNumberOptions )
        {
          HWND hwControl = WinWindowFromID( hwnd, D_Number_Default );
          if ( hwControl )
            WinSetWindowPos( hwControl, 0, 0, 0, 0, 0, SWP_SHOW );
          WinCheckButton( hwnd, D_Number_Default,
            !(pCurrentOptionsSet->bUseOptions & NO_UseOwn) );
          for ( int i = D_Number_bName; i < D_Number_Default; i++ )
            WinEnableControl( hwnd, i,
              (pCurrentOptionsSet->bUseOptions & NO_UseOwn) != 0 );
        }
      }
      break;

    case WM_CONTROL:
      switch ( SHORT2FROMMP( mp1 ) )
      {
        case EN_CHANGE:
          switch ( SHORT1FROMMP( mp1 ) )
          {
            case D_Number_GbName:
              WinQueryDlgItemText( hwnd, D_Number_GbName,
                sizeof( pCurrentOptionsSet->szGbName ) - 1,
                pCurrentOptionsSet->szGbName );
              break;
            case D_Number_MbName:
              WinQueryDlgItemText( hwnd, D_Number_MbName,
                sizeof( pCurrentOptionsSet->szMbName ) - 1,
                pCurrentOptionsSet->szMbName );
              break;
            case D_Number_KbName:
              WinQueryDlgItemText( hwnd, D_Number_KbName,
                sizeof( pCurrentOptionsSet->szKbName ) - 1,
                pCurrentOptionsSet->szKbName );
              break;
            case D_Number_bName:
              WinQueryDlgItemText( hwnd, D_Number_bName,
                sizeof( pCurrentOptionsSet->szbName ) - 1,
                pCurrentOptionsSet->szbName );
              break;
            case D_Number_GbLimit:
              WinQueryDlgItemShort( hwnd, D_Number_GbLimit,
                ( short* ) &( pCurrentOptionsSet->iGbLimit ), 0 );
              break;
            case D_Number_MbLimit:
              WinQueryDlgItemShort( hwnd, D_Number_MbLimit,
                ( short* ) &( pCurrentOptionsSet->iMbLimit ), 0 );
              break;
            case D_Number_KbLimit:
              WinQueryDlgItemShort( hwnd, D_Number_KbLimit,
                ( short* ) &( pCurrentOptionsSet->iKbLimit ), 0 );
              break;
          }
          break;

        case BN_CLICKED:
          {
            int bResult =
              ( WinQueryButtonCheckstate( hwnd, SHORT1FROMMP( mp1 ) ) != 0 );
            int bMask = 0;
            switch ( SHORT1FROMMP( mp1 ) )
            {
              case D_Number_Gb:
                bMask = NO_UseGb;
                break;
              case D_Number_Mb:
                bMask = NO_UseMb;
                break;
              case D_Number_Kb:
                bMask = NO_UseKb;
                break;
              case D_Number_b:
                bMask = NO_Useb;
                break;
              case D_Number_Default:
                {
                  bMask = NO_UseOwn;
                  bResult = ! bResult;
                  for ( int i = D_Number_bName; i < D_Number_Default; i++ )
                    WinEnableControl( hwnd, i, bResult );
                }
                break;
            }
            pCurrentOptionsSet->bUseOptions &= ~bMask;
            if ( bResult ) pCurrentOptionsSet->bUseOptions |= bMask;
          }
          break;
      }
      break;

    default:
      return WinDefDlgProc( hwnd, msg, mp1, mp2 );
  }
  return ( MRESULT ) FALSE;
}


// Default colour setup dialog window procedure -------------------------------
MRESULT EXPENTRY DefColourDlgProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
  switch ( msg )
  {
    case WM_INITDLG:
      {
        for ( int i = D_Custom_ColorG; i < D_Custom_Color_Last; i++ )
        {
          HWND hwControl = WinWindowFromID( hwnd, i );
          if ( hwControl )
            WinSetWindowPos( hwControl, 0, 0, 0, 0, 0, SWP_HIDE );
        }
        SB2_FillColorList( hwnd, D_Text_Default, iDefaultTextColor );
        SB2_FillColorList( hwnd, D_Back_Default, iDefaultBackColor );
      }
      break;

    case WM_CONTROL:
      switch ( SHORT1FROMMP( mp1 ) )
      {
        case D_Text_Default:
          if ( SHORT2FROMMP( mp1 ) == LN_SELECT )
          {
            iDefaultTextColor = ( int ) WinSendDlgItemMsg( hwnd, D_Text_Default,
              LM_QUERYSELECTION, ( MPARAM ) LIT_CURSOR, ( MPARAM ) 0 );
            WinInvalidateRect( hwPipeFrame, NULL, TRUE );
          }
          break;
        case D_Back_Default:
          if ( SHORT2FROMMP( mp1 ) == LN_SELECT )
          {
            iDefaultBackColor = ( int ) WinSendDlgItemMsg( hwnd, D_Back_Default,
              LM_QUERYSELECTION, ( MPARAM ) LIT_CURSOR, ( MPARAM ) 0 );
            WinInvalidateRect( hwPipeFrame, NULL, TRUE );
          }
          break;
      }
      break;

    default:
      return WinDefDlgProc( hwnd, msg, mp1, mp2 );
  }
  return ( MRESULT ) FALSE;
}


// Cell colour setup dialog window procedure ----------------------------------
MRESULT EXPENTRY ColourDlgProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
  static short int bOperational = 0;
  switch ( msg )
  {
    case WM_INITDLG:
      {
        bOperational = 0;
        SB2d_FillColorList( hwnd, D_Text_Default, pCellToSetup->iCellTextColor );
        SB2d_FillColorList( hwnd, D_Back_Default, pCellToSetup->iCellBackColor );
        for ( int i = 0; i < iColorConditionCount; i++ )
        {
          WinSendDlgItemMsg( hwnd, D_Color_Condition1 + ( i << 2 ),
            SPBM_SETARRAY, ( MPARAM ) pszConditionTitles, ( MPARAM ) 4 );
          WinSendDlgItemMsg( hwnd, D_Color_Condition1 + ( i << 2 ),
            SPBM_SETCURRENTVALUE,
            ( MPARAM ) ( pCellToSetup->Conditions[i].iCondition ), 0 );
          WinSetDlgItemText( hwnd, D_Color_Text1 + ( i << 2 ),
            pCellToSetup->Conditions[i].szText );
          WinSendDlgItemMsg( hwnd, D_Color_Text1 + ( i << 2 ),
            EM_SETTEXTLIMIT, MPFROMSHORT( 31 ), 0 );
          SB2d_FillColorList( hwnd, D_Text_Color1 + ( i << 2 ),
            pCellToSetup->Conditions[i].iTextColor );
          SB2d_FillColorList( hwnd, D_Back_Color1 + ( i << 2 ),
            pCellToSetup->Conditions[i].iBackColor );
        }
        bOperational = 1;
      }
      break;

    case WM_CONTROL:
      {
        short int bInvalidate = 0;
        USHORT usControl = SHORT1FROMMP( mp1 );
        switch ( usControl )
        {
          case D_Text_Default:
            if ( SHORT2FROMMP( mp1 ) == LN_SELECT )
            {
              pCellToSetup->iCellTextColor =
                ( int ) WinSendDlgItemMsg( hwnd, D_Text_Default,
                LM_QUERYSELECTION, ( MPARAM ) LIT_CURSOR, ( MPARAM ) 0 );
              bInvalidate = 1;
            }
            break;
          case D_Back_Default:
            if ( SHORT2FROMMP( mp1 ) == LN_SELECT )
            {
              pCellToSetup->iCellBackColor =
                ( int ) WinSendDlgItemMsg( hwnd, D_Back_Default,
                LM_QUERYSELECTION, ( MPARAM ) LIT_CURSOR, ( MPARAM ) 0 );
              bInvalidate = 1;
            }
            break;
          case D_Color_Condition1:
          case D_Color_Condition2:
          case D_Color_Condition3:
            if ( SHORT2FROMMP( mp1 ) == SPBN_CHANGE )
            {
              int n = 0;
              WinSendDlgItemMsg( hwnd, usControl, SPBM_QUERYVALUE, &n, 0 );
              if ( bOperational )
              {
                pCellToSetup->Conditions[( usControl -
                  D_Color_Condition1 ) >> 2].iCondition = n;
                bInvalidate = 1;
              }
              for ( int i = 1; i < 4; i++ )
                WinEnableControl( hwnd, usControl + i, n );
            }
            break;
          case D_Text_Color1:
          case D_Text_Color2:
          case D_Text_Color3:
            if ( SHORT2FROMMP( mp1 ) == LN_SELECT )
            {
              pCellToSetup->Conditions[( usControl - D_Text_Color1 ) >>
                2].iTextColor = ( int ) WinSendDlgItemMsg( hwnd, usControl,
                LM_QUERYSELECTION, ( MPARAM ) LIT_CURSOR, ( MPARAM ) 0 );
              bInvalidate = 1;
            }
            break;
          case D_Back_Color1:
          case D_Back_Color2:
          case D_Back_Color3:
            if ( SHORT2FROMMP( mp1 ) == LN_SELECT )
            {
              pCellToSetup->Conditions[( usControl - D_Back_Color1 ) >>
                2].iBackColor = ( int ) WinSendDlgItemMsg( hwnd, usControl,
                LM_QUERYSELECTION, ( MPARAM ) LIT_CURSOR, ( MPARAM ) 0 );
              bInvalidate = 1;
            }
            break;
          case D_Color_Text1:
          case D_Color_Text2:
          case D_Color_Text3:
            if ( SHORT2FROMMP( mp1 ) == EN_CHANGE )
            {
              WinQueryDlgItemText( hwnd, usControl,
                sizeof( pCellToSetup->Conditions[0].szText ),
                pCellToSetup->Conditions[( usControl - D_Color_Text1 ) >>
                2].szText );
              bInvalidate = 1;
            }
            break;
        }
        if ( bInvalidate && ( pCellToSetup != &TempCell ) )
          WinInvalidateRect( hwPipeFrame, NULL, TRUE );
      }
      break;

    default:
      return WinDefDlgProc( hwnd, msg, mp1, mp2 );
  }
  return ( MRESULT ) FALSE;
}


// Hidden control setup procedure --------------------------------------------
void SetupHiddenControls( HWND hwnd, int iCellType )
{
  for ( int i = 0; i < iHiddenControlCount; i++ )
  {
    HWND hwControl = WinWindowFromID( hwnd, iHiddenControls[i] );
    if ( hwControl ) WinSetWindowPos( hwControl, 0, 0, 0, 0, 0,
      iHiddenControlSets[iCellType][i] ? SWP_SHOW : SWP_HIDE );
  }
  WinSetWindowText( WinWindowFromID( hwnd, D_Cell_NameT ),
    pszCellParamNames[iCellType] );
  for ( i = 0; i < iHiddenCheckCount; i++ )
  {
    HWND hwControl = WinWindowFromID( hwnd, D_Cell_Check0 + i );
    if ( hwControl )
    {
      WinSetWindowPos( hwControl, 0, 0, 0, 0, 0,
        pszCellCheckNames[iCellType][i] ? SWP_SHOW : SWP_HIDE );
      if ( pszCellCheckNames[iCellType][i] )
        WinSetWindowText( hwControl, pszCellCheckNames[iCellType][i] );
    }
  }
}


// Cell setup dialog window procedure ----------------------------------------
MRESULT EXPENTRY CellSetupDlgProc( HWND hwnd, ULONG msg, MPARAM mp1,
  MPARAM mp2 )
{
  static ULONG ulDiskCurr;
  static ULONG ulDiskMap;
  static int iDriveCount;
  static char szDrive[10];
  switch ( msg )
  {
    case WM_INITDLG:
      {
//        SB2_FillColorList( hwnd, D_Cell_Color );
        for ( int i = 0; i < iCellTypeCount; i++ )
          WinSendDlgItemMsg( hwnd, D_Cell_Type, LM_INSERTITEM, ( MPARAM )
            LIT_END, MPFROMP( pszCellTypes1[i] ) );
        WinSendDlgItemMsg( hwnd, D_Cell_Type, LM_SELECTITEM,
          ( MPARAM ) pCellToSetup->iCellType, ( MPARAM ) 1 );
        WinSendDlgItemMsg( hwnd, D_Cell_Desc, EM_SETTEXTLIMIT,
          ( MPARAM ) sizeof( pCellToSetup->szCellDesc ), 0 );
        WinSendDlgItemMsg( hwnd, D_Cell_Prefix, EM_SETTEXTLIMIT,
          ( MPARAM ) sizeof( pCellToSetup->szCellPrefix ), 0 );
        WinSendDlgItemMsg( hwnd, D_Cell_Empty, EM_SETTEXTLIMIT,
          ( MPARAM ) sizeof( pCellToSetup->szCellEmpty ), 0 );
        WinSendDlgItemMsg( hwnd, D_Cell_Name, EM_SETTEXTLIMIT,
          ( MPARAM ) 255, 0 );
        WinSendDlgItemMsg( hwnd, D_Cell_Seconds, SPBM_SETLIMITS,
          ( MPARAM ) 9999, ( MPARAM ) pCellToSetup->iCellSeconds );
        WinSendDlgItemMsg( hwnd, D_Cell_Seconds, SPBM_SETLIMITS,
          ( MPARAM ) 9999, ( MPARAM ) 1 );
        WinSetDlgItemText( hwnd, D_Cell_Name, pCellToSetup->szCellData1 );
        WinCheckButton( hwnd, D_Cell_Enable,
          ( pCellToSetup->iCellStatus & Cell_Enabled ) == Cell_Enabled );
        WinCheckButton( hwnd, D_Cell_Check0,
          ( pCellToSetup->iCellStatus & Cell_Check0 ) == Cell_Check0 );
        WinCheckButton( hwnd, D_Cell_Check1,
          ( pCellToSetup->iCellStatus & Cell_Check1 ) == Cell_Check1 );
        WinCheckButton( hwnd, D_Cell_Check2,
          ( pCellToSetup->iCellStatus & Cell_Check2 ) == Cell_Check2 );
        WinCheckButton( hwnd, D_Cell_Check3,
          ( pCellToSetup->iCellStatus & Cell_Check3 ) == Cell_Check3 );
        WinCheckButton( hwnd, D_Cell_Expire,
          ( pCellToSetup->iCellStatus & Cell_Expire ) == Cell_Expire );
        WinCheckButton( hwnd, D_Cell_LazyShrink,
          ( pCellToSetup->iCellStatus & Cell_LazyShrink ) == Cell_LazyShrink );

        WinSetDlgItemText( hwnd, D_Cell_Desc, pCellToSetup->szCellDesc );
        WinSetDlgItemText( hwnd, D_Cell_Prefix, pCellToSetup->szCellPrefix );
        WinSetDlgItemText( hwnd, D_Cell_Empty, pCellToSetup->szCellEmpty );
//        WinSendDlgItemMsg( hwnd, D_Cell_Color, LM_SELECTITEM,
//          ( MPARAM ) pCellToSetup->iCellTextColor, ( MPARAM ) 1 );
      }
      break;

    case WM_COMMAND:
      {
        switch ( SHORT1FROMMP( mp1 ) )
        {
          case D_Cell_BrowseFile:
            {
              HWND hwDlg;
              FILEDLG fild;
              char *pszTitle = "Select file to monitor";

              memset( &fild, 0, sizeof ( FILEDLG ) );
              fild.cbSize = sizeof ( FILEDLG );
              fild.fl = FDS_CENTER | FDS_OPEN_DIALOG;
              fild.pszTitle = pszTitle;
              WinQueryDlgItemText( hwnd, D_Cell_Name,
                sizeof( fild.szFullFile ), fild.szFullFile );
              hwDlg = WinFileDlg( HWND_DESKTOP, hwnd, &fild );
              if ( hwDlg && ( fild.lReturn == DID_OK ) ) WinSetDlgItemText(
                hwnd, D_Cell_Name, fild.szFullFile );
              if ( pCellToSetup->iCellType == MCT_FileSize )
              {
                char *p = strrchr( fild.szFullFile, '\\' );
                if ( ! p ) p = fild.szFullFile;
                else p++;
                WinSetDlgItemText( hwnd, D_Cell_Desc, p );
                WinSetDlgItemText( hwnd, D_Cell_Prefix, p );
              }
            }
            break;
        }
      }
      break;

    case WM_CONTROL:
      switch ( SHORT1FROMMP( mp1 ) )
      {
        case D_Cell_Type:
          if ( SHORT2FROMMP( mp1 ) == LN_SELECT )
          {
            int i = ( int ) WinSendDlgItemMsg( hwnd, D_Cell_Type,
              LM_QUERYSELECTION, ( MPARAM ) LIT_CURSOR, ( MPARAM ) 0 );
            char *pszNewDescValue = pszCellDescs[i];
            char *pszNewPrefixValue = pszCellPrefixes[i];
            char *pszNewEmptyValue = "";
            if ( pCellToSetup->iCellType != i )
            {
              if ( iCellToSetup > -1 ) CleanupCell( iCellToSetup );
              pCellToSetup->iCellType = i;
              if ( iCellToSetup > -1 ) InitCell( iCellToSetup );

              switch ( i )
              {
                case MCT_Power:
                  pszNewEmptyValue = "APM Error";
                  break;
                case MCT_FreeSpace:
                  sprintf( szDrive, "%s%c:", pszNewDescValue,
                    pCellToSetup->szCellData1[0] + 'A' - '0' );
                  pszNewDescValue = szDrive;
                  pszNewPrefixValue = szDrive + 6;
                  break;
                case MCT_FileSize:
                  pszNewEmptyValue = "No such file(s)";
                  break;
                case MCT_POP3:
                  pszNewEmptyValue = "no mail";
                  break;
              }
//              if ( i == MCT_Pipe && pCellToSetup->szCellData1[0] < ' ' )
//                WinSetDlgItemText( hwnd, D_Cell_Name, "\\PIPE\\MyPipe" );
              if ( i > MCT_Pipe )
              {
                pCellToSetup->iCellType = MCT_Pipe;
                WinSetDlgItemText( hwnd, D_Cell_Name,
                  pszCustomPipes[i - MCT_Pipe - 1] );
                i = MCT_Pipe;
              }
              if ( pszNewDescValue )
                WinSetDlgItemText( hwnd, D_Cell_Desc, pszNewDescValue );
              if ( pszNewPrefixValue )
                WinSetDlgItemText( hwnd, D_Cell_Prefix, pszNewPrefixValue );
              if ( pszNewEmptyValue )
                WinSetDlgItemText( hwnd, D_Cell_Empty, pszNewEmptyValue );
            }
            SetupHiddenControls( hwnd, i );
            if ( i >= MCT_TCPDLLs0 && i <= MCT_TCPDLLs1 && ! hmSo32DLL )
            {
              WinCheckButton( hwnd, D_Cell_Enable, 0 );
              WinEnableControl( hwnd, D_Cell_Enable, 0 );
            }
            else WinEnableControl( hwnd, D_Cell_Enable, 1 );
            WinSendDlgItemMsg( hwnd, D_Cell_Choice, LM_DELETEALL, 0, 0 );
            switch ( i )
            {
              case MCT_TrafIn:
              case MCT_TrafOut:
                if ( hmSo32DLL )
                {
                  struct ifmib *pIfmib = new struct ifmib[1];
                  if ( pIfmib )
                  {
                    int iSocket = Socket( PF_INET, SOCK_STREAM, 0 );
                    if ( iSocket != -1 )
                    {
                      memset( pIfmib, 0, sizeof ( struct ifmib ) );
                      SoIOCtl( iSocket, SIOSTATIF, ( caddr_t ) pIfmib,
                        sizeof ( struct ifmib ) );
                      int n = 0;
                      for ( int i=0, j=0; i < IFMIB_ENTRIES; i++ )
                        if( pIfmib->iftable[i].ifDescr[0] ) {
                          char Item[256];
                          sprintf(Item,"%02d: %s",pIfmib->iftable[i].ifIndex,pIfmib->iftable[i].ifDescr);
                          WinSendDlgItemMsg( hwnd, D_Cell_Choice, LM_INSERTITEM,
                            ( MPARAM ) LIT_END, MPFROMP( Item ) );
                          if ( pIfmib->iftable[i].ifIndex == *(int*) pCellToSetup->szCellData1 )
                              n = j;
                          j++;
                        }
                      SoClose( iSocket );
                      WinSendDlgItemMsg( hwnd, D_Cell_Choice, LM_SELECTITEM,
                        ( MPARAM ) n, ( MPARAM ) 1 );
                    }
                    delete[] pIfmib;
                  }
                }
                break;
              case MCT_FreeSpace:
                {
                  szDrive[1] = ':';
                  szDrive[2] = 0;
                  iDriveCount = 0;
                  DosQueryCurrentDisk( &ulDiskCurr, &ulDiskMap );
                  ULONG ulDiskMap2 = ulDiskMap;
                  int n = 0;
                  for ( i = 0; i < 26; i++ )
                  {
                    if ( ulDiskMap2 & 1 )
                    {
                      szDrive[0] = 'A' + i;
                      WinSendDlgItemMsg( hwnd, D_Cell_Choice, LM_INSERTITEM,
                        ( MPARAM ) LIT_END, MPFROMP( szDrive ) );
                      if ( n < pCellToSetup->szCellData1[0] - '0' ) n++;
                      iDriveCount++;
                    }
                    ulDiskMap2 >>= 1;
                  }
                  WinSendDlgItemMsg( hwnd, D_Cell_Choice, LM_SELECTITEM,
                    ( MPARAM ) ( n < iDriveCount ? n : 0 ), ( MPARAM ) 1 );
                }
                break;
            }
          }
          break;
        case D_Cell_Choice:
          if ( SHORT2FROMMP( mp1 ) == LN_SELECT )
          {
            int n = ( int ) WinSendDlgItemMsg( hwnd, D_Cell_Choice,
              LM_QUERYSELECTION, ( MPARAM ) LIT_CURSOR, ( MPARAM ) 0 );
            switch ( pCellToSetup->iCellType )
            {
              case MCT_FreeSpace:
                {
                  pCellToSetup->szCellData1[0] = '0' - 1;
                  ULONG ulDiskMap2 = ulDiskMap;;
                  for ( int i = 0; i <= n; ulDiskMap2 >>= 1 )
                  {
                    if ( ulDiskMap2 & 1 ) n--;
                    pCellToSetup->szCellData1[0]++;
                  }
                  sprintf( szDrive, "Drive %c:", pCellToSetup->szCellData1[0] +
                    'A' - '0' );
                  WinSetDlgItemText( hwnd, D_Cell_Desc, szDrive );
                  WinSetDlgItemText( hwnd, D_Cell_Prefix, szDrive + 6 );
                }
                break;
              case MCT_TrafIn:
              case MCT_TrafOut:
                  char Item[256];
                  WinSendDlgItemMsg( hwnd, D_Cell_Choice, LM_QUERYITEMTEXT,
                       MPFROM2SHORT( n, 256 ), MPFROMP( Item ) );
                  sscanf(Item,"%d",(int*)pCellToSetup->szCellData1);
                  break;
            }
          }
          break;

        case D_Cell_Prefix:
          if ( SHORT2FROMMP( mp1 ) == EN_CHANGE )
          {
            WinQueryDlgItemText( hwnd, D_Cell_Prefix,
              sizeof( pCellToSetup->szCellPrefix ),
              pCellToSetup->szCellPrefix );
            UpdateCell( pCellToSetup );
          }
          break;

        case D_Cell_Empty:
          if ( SHORT2FROMMP( mp1 ) == EN_CHANGE )
          {
            WinQueryDlgItemText( hwnd, D_Cell_Empty,
              sizeof( pCellToSetup->szCellEmpty ), pCellToSetup->szCellEmpty );
            UpdateCell( pCellToSetup );
          }
          break;

        case D_Cell_Name:
          if ( SHORT2FROMMP( mp1 ) == EN_CHANGE )
          {
            if ( pCellToSetup->iCellType == MCT_Pipe )
            {
              char szBuffer[sizeof( pCellToSetup->szCellData1 )];
              WinQueryDlgItemText( hwnd, D_Cell_Name,
                sizeof( pCellToSetup->szCellData1 ), szBuffer );
              if ( strnicmp( szBuffer, "\\PIPE\\", 6 ) )
              {
                char szBuf[sizeof( pCellToSetup->szCellData1 )];
                strcpy( szBuf, szBuffer );
                strcpy( szBuffer, "\\PIPE" );
                char *p2 = &( szBuffer[5] );
                if ( szBuf[0] != '\\' ) *p2++ = '\\';
                strcpy( p2, szBuf );
                WinSetDlgItemText( hwnd, D_Cell_Name, szBuffer );
                break;
              }
            }
            if ( pCellToSetup->iCellType != MCT_FreeSpace &&
              pCellToSetup->iCellType != MCT_TrafIn &&
              pCellToSetup->iCellType != MCT_TrafOut )
            {
              if ( iCellToSetup > -1 )
              {
                CleanupCell( iCellToSetup );
                WinQueryDlgItemText( hwnd, D_Cell_Name,
                  sizeof( pCellToSetup->szCellData1 ),
                  pCellToSetup->szCellData1 );
                InitCell( iCellToSetup );
                UpdateCell( pCellToSetup );
              }
            }
          }
          break;

        case D_Cell_Seconds:
          if ( SHORT2FROMMP( mp1 ) == SPBN_CHANGE )
          {
            WinSendDlgItemMsg( hwnd, D_Cell_Seconds, SPBM_QUERYVALUE,
              ( short* ) &( pCellToSetup->iCellSeconds ), 0 );
            pCellToSetup->iCellCounter = pCellToSetup->iCellSeconds;
          }
          break;

        case D_Cell_Enable:
          if ( SHORT2FROMMP( mp1 ) == BN_CLICKED )
          {
            if ( WinQueryButtonCheckstate( hwnd, D_Cell_Enable ) )
            {
              if ( ! ( pCellToSetup->iCellStatus & Cell_Enabled ) &&
                iCellToSetup > -1 ) InitCell( iCellToSetup );
              pCellToSetup->iCellStatus |= Cell_Enabled;
            }
            else
            {
              if ( pCellToSetup->iCellStatus & Cell_Enabled &&
                iCellToSetup > -1 ) CleanupCell( iCellToSetup );
              pCellToSetup->iCellStatus &= ~Cell_Enabled;
            }
          }
          break;

        case D_Cell_Expire:
          if ( SHORT2FROMMP( mp1 ) == BN_CLICKED )
          {
            if ( WinQueryButtonCheckstate( hwnd, D_Cell_Expire ) )
              pCellToSetup->iCellStatus |= Cell_Expire;
            else pCellToSetup->iCellStatus &= ~Cell_Expire;
          }
          break;

        case D_Cell_LazyShrink:
          if ( SHORT2FROMMP( mp1 ) == BN_CLICKED )
          {
            if ( WinQueryButtonCheckstate( hwnd, D_Cell_LazyShrink ) )
              pCellToSetup->iCellStatus |= Cell_LazyShrink;
            else pCellToSetup->iCellStatus &= ~Cell_LazyShrink;
          }
          break;

        case D_Cell_Check0:
          if ( SHORT2FROMMP( mp1 ) == BN_CLICKED )
          {
            if ( WinQueryButtonCheckstate( hwnd, D_Cell_Check0 ) )
              pCellToSetup->iCellStatus |= Cell_Check0;
            else pCellToSetup->iCellStatus &= ~Cell_Check0;
            UpdateCell( pCellToSetup );
          }
          break;

        case D_Cell_Check1:
          if ( SHORT2FROMMP( mp1 ) == BN_CLICKED )
          {
            if ( WinQueryButtonCheckstate( hwnd, D_Cell_Check1 ) )
              pCellToSetup->iCellStatus |= Cell_Check1;
            else pCellToSetup->iCellStatus &= ~Cell_Check1;
            UpdateCell( pCellToSetup );
          }
          break;

        case D_Cell_Check2:
          if ( SHORT2FROMMP( mp1 ) == BN_CLICKED )
          {
            if ( WinQueryButtonCheckstate( hwnd, D_Cell_Check2 ) )
              pCellToSetup->iCellStatus |= Cell_Check2;
            else pCellToSetup->iCellStatus &= ~Cell_Check2;
            UpdateCell( pCellToSetup );
          }
          break;

        case D_Cell_Check3:
          if ( SHORT2FROMMP( mp1 ) == BN_CLICKED )
          {
            if ( WinQueryButtonCheckstate( hwnd, D_Cell_Check3 ) )
              pCellToSetup->iCellStatus |= Cell_Check3;
            else pCellToSetup->iCellStatus &= ~Cell_Check3;
            UpdateCell( pCellToSetup );
          }
          break;
      }
      break;

    case WM_DESTROY:
      WinQueryDlgItemText( hwnd, D_Cell_Desc,
        sizeof( pCellToSetup->szCellDesc ), pCellToSetup->szCellDesc );
      if ( pCellToSetup->iCellType != MCT_FreeSpace &&
        pCellToSetup->iCellType != MCT_TrafIn &&
        pCellToSetup->iCellType != MCT_TrafOut )
         WinQueryDlgItemText( hwnd, D_Cell_Name,
           sizeof( pCellToSetup->szCellData1 ), pCellToSetup->szCellData1 );
    default:
      return WinDefDlgProc( hwnd, msg, mp1, mp2 );
  }
  return ( MRESULT ) FALSE;
}


// Cell application setup dialog window procedure ----------------------------
MRESULT EXPENTRY CellAppSetupDlgProc( HWND hwnd, ULONG msg, MPARAM mp1,
  MPARAM mp2 )
{
  switch ( msg )
  {
    case WM_INITDLG:
      {
        WinCheckButton( hwnd, D_App_IncomeX,
          ( pCellToSetup->iCellStatus & Cell_Change_App ) == Cell_Change_App );
        WinCheckButton( hwnd, D_App_DblclickX,
          ( pCellToSetup->iCellStatus & Cell_Click_App ) == Cell_Click_App );
        WinSendDlgItemMsg( hwnd, D_App_IncomeTxt, EM_SETTEXTLIMIT,
          ( MPARAM ) sizeof( pCellToSetup->szCellChangeApp ), 0 );
        WinSendDlgItemMsg( hwnd, D_App_DblclickTxt, EM_SETTEXTLIMIT,
          ( MPARAM ) sizeof( pCellToSetup->szCellClickApp ), 0 );
        WinSetDlgItemText( hwnd, D_App_IncomeTxt, pCellToSetup->szCellChangeApp );
        WinSetDlgItemText( hwnd, D_App_DblclickTxt, pCellToSetup->szCellClickApp );
      }
      break;

    case WM_COMMAND:
      {
        switch ( SHORT1FROMMP( mp1 ) )
        {
          case D_App_IncomeBrw:
          case D_App_DblclickBrw:
            {
              HWND hwDlg;
              FILEDLG fild;
              char *pszTitle = "Select file to execute";
              int n = ( SHORT1FROMMP( mp1 ) == D_App_IncomeBrw );

              memset( &fild, 0, sizeof ( FILEDLG ) );
              fild.cbSize = sizeof ( FILEDLG );
              fild.fl = FDS_CENTER | FDS_OPEN_DIALOG;
              fild.pszTitle = pszTitle;
              WinQueryDlgItemText( hwnd, n ? D_App_IncomeTxt :
                D_App_DblclickTxt, sizeof( fild.szFullFile ), fild.szFullFile );
              hwDlg = WinFileDlg( HWND_DESKTOP, hwnd, &fild );
              if ( hwDlg && ( fild.lReturn == DID_OK ) ) WinSetDlgItemText(
                hwnd, n ? D_App_IncomeTxt : D_App_DblclickTxt,
                fild.szFullFile );
            }
            break;
        }
      }
      break;

    case WM_CONTROL:
      switch ( SHORT1FROMMP( mp1 ) )
      {
        case D_App_IncomeTxt:
          if ( SHORT2FROMMP( mp1 ) == EN_CHANGE ) WinQueryDlgItemText( hwnd,
            D_App_IncomeTxt, sizeof( pCellToSetup->szCellChangeApp ),
            pCellToSetup->szCellChangeApp );
          break;

        case D_App_DblclickTxt:
          if ( SHORT2FROMMP( mp1 ) == EN_CHANGE ) WinQueryDlgItemText( hwnd,
            D_App_DblclickTxt, sizeof( pCellToSetup->szCellClickApp ),
            pCellToSetup->szCellClickApp );
          break;

        case D_App_IncomeX:
          if ( SHORT2FROMMP( mp1 ) == BN_CLICKED )
          {
            if ( WinQueryButtonCheckstate( hwnd, D_App_IncomeX ) )
              pCellToSetup->iCellStatus |= Cell_Change_App;
            else pCellToSetup->iCellStatus &= ~Cell_Change_App;
          }
          break;

        case D_App_DblclickX:
          if ( SHORT2FROMMP( mp1 ) == BN_CLICKED )
          {
            if ( WinQueryButtonCheckstate( hwnd, D_App_DblclickX ) )
              pCellToSetup->iCellStatus |= Cell_Click_App;
            else pCellToSetup->iCellStatus &= ~Cell_Click_App;
          }
          break;
      }
      break;

    default:
      return WinDefDlgProc( hwnd, msg, mp1, mp2 );
  }
  return ( MRESULT ) FALSE;
}


// Cell log setup dialog window procedure ------------------------------------
MRESULT EXPENTRY CellLogSetupDlgProc( HWND hwnd, ULONG msg, MPARAM mp1,
  MPARAM mp2 )
{
  switch ( msg )
  {
    case WM_INITDLG:
      {
        WinCheckButton( hwnd, D_Log_Enable,
          ( pCellToSetup->iCellStatus & Cell_Log_On ) == Cell_Log_On );
        WinCheckButton( hwnd, D_Log_Timestamping, ( pCellToSetup->iCellStatus &
          Cell_Log_Timestamping ) == Cell_Log_Timestamping );
        WinCheckButton( hwnd, D_Log_AppendX,
          ( pCellToSetup->iCellStatus & Cell_Log_Append ) == Cell_Log_Append );
        WinSendDlgItemMsg( hwnd, D_Log_AppendFile, EM_SETTEXTLIMIT,
          ( MPARAM ) sizeof( pCellToSetup->szCellLogFile ), 0 );
        WinSetDlgItemText( hwnd, D_Log_AppendFile, pCellToSetup->szCellLogFile );
        WinSendDlgItemMsg( hwnd, D_Log_Buffer, SPBM_SETLIMITS,
          ( MPARAM ) 64, ( MPARAM ) pCellToSetup->iCellLogBufferSize );
        WinSendDlgItemMsg( hwnd, D_Log_Buffer, SPBM_SETLIMITS,
          ( MPARAM ) 64, ( MPARAM ) 1 );
        WinSetDlgItemText( hwnd, D_Log_MLE, pCellToSetup->pszCellLog );
        hwLogToUpdate = hwnd;
      }
      break;

    case WM_COMMAND:
      {
        switch ( SHORT1FROMMP( mp1 ) )
        {
          case D_Log_AppendBrw:
            {
              HWND hwDlg;
              FILEDLG fild;
              char *pszTitle = "Select log file";

              memset( &fild, 0, sizeof ( FILEDLG ) );
              fild.cbSize = sizeof ( FILEDLG );
              fild.fl = FDS_CENTER | FDS_OPEN_DIALOG;
              fild.pszTitle = pszTitle;
              WinQueryDlgItemText( hwnd, D_Log_AppendFile,
                sizeof( fild.szFullFile ), fild.szFullFile );
              hwDlg = WinFileDlg( HWND_DESKTOP, hwnd, &fild );
              if ( hwDlg && ( fild.lReturn == DID_OK ) ) WinSetDlgItemText(
                hwnd, D_Log_AppendFile, fild.szFullFile );
            }
            break;
        }
      }
      break;

    case WM_CONTROL:
      switch ( SHORT1FROMMP( mp1 ) )
      {
        case D_Log_Enable:
          if ( SHORT2FROMMP( mp1 ) == BN_CLICKED )
          {
            if ( WinQueryButtonCheckstate( hwnd, D_Log_Enable ) )
            {
              pCellToSetup->iCellStatus |= Cell_Log_On;
              if ( pCellToSetup->pszCellLog == NULL )
              {
                int iBufferSize = pCellToSetup->iCellLogBufferSize << 10;
                if ( pCellToSetup->pszCellLog = new char[iBufferSize] )
                  memset( pCellToSetup->pszCellLog, 0, iBufferSize );
              }
            }
            else pCellToSetup->iCellStatus &= ~Cell_Log_On;
          }
          break;

        case D_Log_Timestamping:
          if ( SHORT2FROMMP( mp1 ) == BN_CLICKED )
          {
            if ( WinQueryButtonCheckstate( hwnd, D_Log_Timestamping ) )
              pCellToSetup->iCellStatus |= Cell_Log_Timestamping;
            else pCellToSetup->iCellStatus &= ~Cell_Log_Timestamping;
          }
          break;

        case D_Log_AppendX:
          if ( SHORT2FROMMP( mp1 ) == BN_CLICKED )
          {
            if ( WinQueryButtonCheckstate( hwnd, D_Log_AppendX ) )
              pCellToSetup->iCellStatus |= Cell_Log_Append;
            else pCellToSetup->iCellStatus &= ~Cell_Log_Append;
          }
          break;

        case D_Log_AppendFile:
          if ( SHORT2FROMMP( mp1 ) == EN_CHANGE ) WinQueryDlgItemText( hwnd,
            D_Log_AppendFile, sizeof( pCellToSetup->szCellLogFile ),
            pCellToSetup->szCellLogFile );
          break;

        case D_Log_Buffer:
          if ( SHORT2FROMMP( mp1 ) == SPBN_CHANGE )
          {
            int iBufferSize = pCellToSetup->iCellLogBufferSize;
            WinSendDlgItemMsg( hwnd, D_Log_Buffer, SPBM_QUERYVALUE,
              ( short* ) &( iBufferSize ), 0 );
            if ( iBufferSize != pCellToSetup->iCellLogBufferSize ||
              ( ( pCellToSetup->iCellStatus & Cell_Log_On ) &&
              ! pCellToSetup->pszCellLog ) )
            {
              int iNewBufferSize = iBufferSize * 1024;
              char* pBuffer = new char[iNewBufferSize];
              if ( pBuffer )
              {
                memset( pBuffer, 0, iNewBufferSize );
                if ( pCellToSetup->pszCellLog )
                {
                  char* pOldBuffer = pCellToSetup->pszCellLog;
                  if ( iBufferSize < pCellToSetup->iCellLogBufferSize &&
                    strlen( pCellToSetup->pszCellLog ) > iNewBufferSize - 1 )
                  {
                    pCellToSetup->pszCellLog +=
                      strlen( pCellToSetup->pszCellLog ) - iNewBufferSize;
                    do pCellToSetup->pszCellLog++;
                    while ( pCellToSetup->pszCellLog[-1] != '\n' );
                  }
                  strcpy( pBuffer, pCellToSetup->pszCellLog );
                  pCellToSetup->pszCellLog = pBuffer;
                  delete pOldBuffer;
                }
                else pCellToSetup->pszCellLog = pBuffer;
              }
            }
            pCellToSetup->iCellLogBufferSize = iBufferSize;
          }
          break;
      }
      break;

    case WM_DESTROY:
      hwLogToUpdate = NULLHANDLE;
    default:
      return WinDefDlgProc( hwnd, msg, mp1, mp2 );
  }
  return ( MRESULT ) FALSE;
}



// Main properties dialog window procedure -----------------------------------
MRESULT EXPENTRY CellNotebookDlgProc( HWND hwnd, ULONG msg, MPARAM mp1,
  MPARAM mp2 )
{
  static ULONG ulFirstPageDlg = NULL;
  static ULONG ulColorsPageDlg = NULL;
  static ULONG ulNumberPageDlg = NULL;
  static ULONG ulProgramPageDlg = NULL;
  static ULONG ulLogPageDlg = NULL;
  static HWND hwFirstPageDlg = NULL;
  static HWND hwColorsPageDlg = NULL;
  static HWND hwNumberPageDlg = NULL;
  static HWND hwProgramPageDlg = NULL;
  static HWND hwLogPageDlg = NULL;
  switch ( msg )
  {
    case WM_INITDLG:
      if ( mp2 )
      {
        HWND hwControl = WinWindowFromID( hwnd, DID_OK );
        if ( hwControl ) WinSetWindowPos( hwControl, 0, 0, 0, 0, 0,
          iCellToSetup == -1 ? SWP_SHOW : SWP_HIDE );
        hwControl = WinWindowFromID( hwnd, DID_CANCEL );
        WinSetWindowText( hwControl,
          iCellToSetup == -1 ? "~Cancel" : "~Close" );

        WinSendDlgItemMsg( hwnd, D_Cell_Notebook, BKM_SETNOTEBOOKCOLORS,
          MPFROMLONG( SYSCLR_DIALOGBACKGROUND ),
          MPFROMLONG( BKA_BACKGROUNDPAGECOLORINDEX ) );
        HWND hwCellNotebook = WinWindowFromID( hwnd, D_Cell_Notebook );

        ulFirstPageDlg = SB2_AddDlgPage( hwnd, D_Cell_Notebook, "Cell",
          "General cell settings" );
        hwFirstPageDlg = WinLoadDlg( hwCellNotebook, hwCellNotebook,
          CellSetupDlgProc, ( HMODULE ) 0, D_Cell_Page1, NULL );
        WinSendDlgItemMsg( hwnd, D_Cell_Notebook, BKM_SETPAGEWINDOWHWND,
          ( MPARAM ) ulFirstPageDlg, ( MPARAM ) hwFirstPageDlg );

        ulColorsPageDlg = SB2_AddDlgPage( hwnd, D_Cell_Notebook, "Colors",
          "Cell color options" );
        hwColorsPageDlg = WinLoadDlg( hwCellNotebook, hwCellNotebook,
          ColourDlgProc, ( HMODULE ) 0, D_Cell_PageColors, NULL );
        WinSendDlgItemMsg( hwnd, D_Cell_Notebook, BKM_SETPAGEWINDOWHWND,
          ( MPARAM ) ulColorsPageDlg, ( MPARAM ) hwColorsPageDlg );

        ulNumberPageDlg = SB2_AddDlgPage( hwnd, D_Cell_Notebook, "Numbers",
          "Number display options" );
        hwNumberPageDlg = WinLoadDlg( hwCellNotebook, hwCellNotebook,
          NumberDlgProc, ( HMODULE ) 0, D_Number_Dialog, NULL );
        WinSendDlgItemMsg( hwnd, D_Cell_Notebook, BKM_SETPAGEWINDOWHWND,
          ( MPARAM ) ulNumberPageDlg, ( MPARAM ) hwNumberPageDlg );
/*
        ulNumberPageDlg = ( ULONG ) WinSendDlgItemMsg( hwnd, D_Cell_Notebook,
          BKM_INSERTPAGE, ( MPARAM ) NULL, MPFROM2SHORT( ( BKA_MINOR |
          BKA_STATUSTEXTON | BKA_AUTOPAGESIZE ), BKA_LAST ) );
        WinSendMsg( hwCellNotebook, BKM_SETSTATUSLINETEXT,
          ( MPARAM ) ulNumberPageDlg,
          MPFROMP ( "Number display options (page 2 of 2)" ) );
        hwNumberPageDlg = WinLoadDlg( hwCellNotebook, hwCellNotebook,
          NumberDlgProc, ( HMODULE ) 0, D_Number_Dialog, NULL );
        WinSendMsg( hwCellNotebook, BKM_SETPAGEWINDOWHWND,
          ( MPARAM ) ulNumberPageDlg, ( MPARAM ) hwNumberPageDlg );
*/
        ulProgramPageDlg = SB2_AddDlgPage( hwnd, D_Cell_Notebook, "Programs",
          "Application settings" );
        hwProgramPageDlg = WinLoadDlg( hwCellNotebook, hwCellNotebook,
          CellAppSetupDlgProc, ( HMODULE ) 0, D_Cell_PageApp, NULL );
        WinSendDlgItemMsg( hwnd, D_Cell_Notebook, BKM_SETPAGEWINDOWHWND,
          ( MPARAM ) ulProgramPageDlg, ( MPARAM ) hwProgramPageDlg );

        ulLogPageDlg = SB2_AddDlgPage( hwnd, D_Cell_Notebook, "Logging",
          "Cell log settings" );
        hwLogPageDlg = WinLoadDlg( hwCellNotebook, hwCellNotebook,
          CellLogSetupDlgProc, ( HMODULE ) 0, D_Cell_PageLog, NULL );
        WinSendDlgItemMsg( hwnd, D_Cell_Notebook, BKM_SETPAGEWINDOWHWND,
          ( MPARAM ) ulLogPageDlg, ( MPARAM ) hwLogPageDlg );
/*
        ulNumberPageDlg = SB2_AddDlgPage( hwnd, D_Cell_Notebook, "Numbers",
          "Number display settings" );
        hwNumberPageDlg = WinLoadDlg( hwCellNotebook, hwCellNotebook,
          NumberDlgProc, ( HMODULE ) 0, D_Number_Dialog, NULL );
        WinSendDlgItemMsg( hwnd, D_Cell_Notebook, BKM_SETPAGEWINDOWHWND,
          ( MPARAM ) ulNumberPageDlg, ( MPARAM ) hwNumberPageDlg );
*/
        WinSendDlgItemMsg( hwnd, D_Cell_Notebook, BKM_SETDIMENSIONS,
          MPFROM2SHORT ( 21, 21 ), MPFROMSHORT ( BKA_PAGEBUTTON ) );
        WinSendDlgItemMsg( hwnd, D_Cell_Notebook, BKM_SETDIMENSIONS,
          MPFROM2SHORT ( 70, 20 ), MPFROMSHORT ( BKA_MAJORTAB ) );
        WinSendDlgItemMsg( hwnd, D_Cell_Notebook, BKM_SETDIMENSIONS,
          MPFROM2SHORT ( 0, 0 ), MPFROMSHORT ( BKA_MINORTAB ) );
      }
      break;

    case WM_CLOSE:
      WinDestroyWindow( hwFirstPageDlg );
      WinDestroyWindow( hwColorsPageDlg );
      WinDestroyWindow( hwNumberPageDlg );
      WinDestroyWindow( hwProgramPageDlg );
      WinDestroyWindow( hwLogPageDlg );
      hwFirstPageDlg = NULL; 
      hwColorsPageDlg = NULL;
      hwNumberPageDlg = NULL;
      hwProgramPageDlg = NULL;
      hwLogPageDlg = NULL;
    default:
      return WinDefDlgProc( hwnd, msg, mp1, mp2 );
  }
  return ( MRESULT ) FALSE;
}


int EditCell( HWND hwnd, int iIndex = -1 )
{
  if ( iIndex >= 0 ) TempCell = pCells[iIndex];

  iCellToSetup = iIndex;
  pCellToSetup = ( iIndex == -1 ? &TempCell : &pCells[iIndex] );
  pCurrentOptionsSet = &( pCellToSetup->noCellOptions );

  int iResult = WinDlgBox( HWND_DESKTOP, hwnd, CellNotebookDlgProc,
    ( HMODULE ) 0, D_Cell_Setup, ( PVOID ) &DummyInit );
  iCellToSetup = -1;
  pCellToSetup = NULL;

/*
  if ( iResult == DID_OK )
  {
    TempCell.iCellCounter = 0;
    TempCell.hCellHandle = 0;
    if ( iIndex >= 0 )
    {
      CleanupCell( iIndex );
      strcpy( TempCell.szCellData, pCells[iIndex].szCellData );
      TempCell.iCellWidth = pCells[iIndex].iCellWidth;
      pCells[iIndex] = TempCell;
      InitCell( iIndex );
    }
    }
*/
  pCurrentOptionsSet = &DefaultNumberOptions;
  return ( iResult == DID_OK );
}


// Cell settings dialog window procedure -------------------------------------
MRESULT EXPENTRY CellsDlgProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
  switch ( msg )
  {
    case WM_INITDLG:
      {
        memset( &TempCell, 0, sizeof( TempCell ) );
//        SB2_FillColorList( hwnd, D_Cell_Color );
//        WinSendDlgItemMsg( hwnd, D_Cell_Desc, EM_SETTEXTLIMIT,
//          ( MPARAM ) sizeof( TempCell.szCellDesc ), 0 );
//        WinSendDlgItemMsg( hwnd, D_Cell_Prefix, EM_SETTEXTLIMIT,
//          ( MPARAM ) sizeof( TempCell.szCellPrefix ), 0 );
//        WinSendDlgItemMsg( hwnd, D_Cell_App, EM_SETTEXTLIMIT,
//          ( MPARAM ) sizeof( TempCell.szCellClickApp ), 0 );
        for ( int i = 0; i < iCellCount; i++ )
          WinSendDlgItemMsg( hwnd, D_Cell_List, LM_INSERTITEM,
            ( MPARAM ) LIT_END, MPFROMP( pCells[i].szCellDesc ) );
        WinSendDlgItemMsg( hwnd, D_Cell_List, LM_SELECTITEM,
          ( MPARAM ) 0, ( MPARAM ) 1 );
        EnableListButtons( hwnd );
      }
      break;

    case WM_COMMAND:
      {
        int i = ( int ) WinSendDlgItemMsg( hwnd, D_Cell_List,
          LM_QUERYSELECTION, ( MPARAM ) LIT_CURSOR, ( MPARAM ) 0 );

        switch ( SHORT1FROMMP( mp1 ) )
        {
          case D_Cell_Edit:
            if ( EditCell( hwnd, i ) ) WinSendDlgItemMsg( hwnd, D_Cell_List,
              LM_SETITEMTEXT, ( MPARAM ) i, MPFROMP( TempCell.szCellDesc ) );
            break;

          case D_Cell_New:
            TempCell.iCellType = 0;
            TempCell.iCellTextColor = 0;
            TempCell.iCellBackColor = 0;
            TempCell.iCellSeconds = 1;
            TempCell.szCellDesc[0] = 0;
            TempCell.szCellPrefix[0] = 0;
            TempCell.szCellClickApp[0] = 0;
            TempCell.noCellOptions = DefaultNumberOptions;
          case D_Cell_Clone:
            TempCell.pszCellLog = NULL;
            TempCell.hCellHandle = NULLHANDLE;
            if ( EditCell( hwnd ) )
            {
              CellData *pNewCells = new CellData[iCellCount + 1];
              if ( pNewCells )
              {
                TempCell.szCellData[0] = 0;
                TempCell.iCellWidth = 0;
                if ( pCells )
                {
                  memcpy( pNewCells, pCells, iCellCount * sizeof( CellData ) );
                  delete pCells;
                }
                iCellCount++;
                pCells = pNewCells;
                pCells[iCellCount - 1] = TempCell;
                InitCell( iCellCount - 1 );
                WinSendDlgItemMsg( hwnd, D_Cell_List, LM_INSERTITEM, ( MPARAM )
                  LIT_END, MPFROMP( TempCell.szCellDesc ) );
                WinSendDlgItemMsg( hwnd, D_Cell_List, LM_SELECTITEM,
                  ( MPARAM ) ( iCellCount - 1 ), ( MPARAM ) 1 );
              }
            }
            else if ( TempCell.pszCellLog ) delete TempCell.pszCellLog;
            break;

          case D_Cell_Remove:
            RemoveCell( i );
            WinSendDlgItemMsg( hwnd, D_Cell_List, LM_DELETEITEM,
              ( MPARAM ) i, ( MPARAM ) 0 );
            if ( iCellCount ) WinSendDlgItemMsg( hwnd, D_Cell_List,
              LM_SELECTITEM, ( MPARAM ) 0, ( MPARAM ) 1 );
            break;

          case D_Cell_Up:
          case D_Cell_Down:
            {
              int iOffset = SHORT1FROMMP( mp1 ) == D_Cell_Up ? -1 : 1;
              {
                CellData tmp = pCells[i];
                pCells[i] = pCells[i + iOffset];
                pCells[i + iOffset] = tmp;
                PaintCell( i, 1 );
                PaintCell( i + iOffset, 1 );
              }
              WinSendDlgItemMsg( hwnd, D_Cell_List, LM_SETITEMTEXT,
                ( MPARAM ) i, MPFROMP( pCells[i].szCellDesc ) );
              WinSendDlgItemMsg( hwnd, D_Cell_List, LM_SETITEMTEXT,
                ( MPARAM ) ( i + iOffset ),
                MPFROMP( pCells[i + iOffset].szCellDesc ) );
              WinSendDlgItemMsg( hwnd, D_Cell_List, LM_SELECTITEM,
                ( MPARAM ) ( i + iOffset ), ( MPARAM ) 1 );
            }
            break;

        }
        EnableListButtons( hwnd );
      }
      break;

    case WM_CONTROL:
      switch ( SHORT1FROMMP( mp1 ) )
      {
        case D_Cell_List:
          if ( SHORT2FROMMP( mp1 ) == LN_SELECT )
          {
            int i = ( int ) WinSendDlgItemMsg( hwnd, D_Cell_List,
              LM_QUERYSELECTION, ( MPARAM ) LIT_CURSOR, ( MPARAM ) 0 );
            if ( i >= 0 && i < iCellCount ) TempCell = pCells[i];
            EnableMoveButtons( hwnd, i );
          }
          break;
      }
      break;

    default:
      return WinDefDlgProc( hwnd, msg, mp1, mp2 );
  }
  return ( MRESULT ) FALSE;
}




// Main properties dialog window procedure -----------------------------------
MRESULT EXPENTRY PropertiesDlgProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
  static ULONG ulSettingsPage;
  static ULONG ulColorsPage;
  static ULONG ulNumberPage;
  static ULONG ulCellsPage;
  static ULONG ulAboutPage;
  static HWND hwSettingsDlg = NULL;
  static HWND hwColorsDlg = NULL;
  static HWND hwNumberDlg = NULL;
  static HWND hwCellsDlg = NULL;
  static HWND hwAboutDlg = NULL;

  switch ( msg )
  {
    case WM_INITDLG:
      if ( mp2 )
      {
        WinSetWindowText( hwnd, "SysBar/2 Pipe Monitor - Properties" );
        WinSendDlgItemMsg( hwnd, D_Prop_Notebook, BKM_SETNOTEBOOKCOLORS,
          MPFROMLONG( SYSCLR_DIALOGBACKGROUND ),
          MPFROMLONG( BKA_BACKGROUNDPAGECOLORINDEX ) );
        HWND hwNotebook = WinWindowFromID( hwnd, D_Prop_Notebook );

        ulSettingsPage = SB2_AddDlgPage( hwnd, D_Prop_Notebook, "Display",
          "Appearance settings (page 1 of 3)" );
        hwSettingsDlg = WinLoadDlg( hwNotebook, hwNotebook, SettingsDlgProc,
          ( HMODULE ) 0, DLG_SETTINGS, NULL );
        WinSendDlgItemMsg( hwnd, D_Prop_Notebook, BKM_SETPAGEWINDOWHWND,
          ( MPARAM ) ulSettingsPage, ( MPARAM ) hwSettingsDlg );

        ulColorsPage = ( ULONG ) WinSendDlgItemMsg( hwnd, D_Prop_Notebook,
          BKM_INSERTPAGE, ( MPARAM ) NULL, MPFROM2SHORT( ( BKA_MINOR |
          BKA_STATUSTEXTON | BKA_AUTOPAGESIZE ), BKA_LAST ) );
        WinSendMsg( hwNotebook, BKM_SETSTATUSLINETEXT, ( MPARAM ) ulColorsPage,
          MPFROMP ( "Cell colors (page 2 of 3)" ) );
        hwColorsDlg = WinLoadDlg( hwNotebook, hwNotebook, DefColourDlgProc,
          ( HMODULE ) 0, D_Cell_PageColors, NULL );
        WinSendMsg( hwNotebook, BKM_SETPAGEWINDOWHWND,
          ( MPARAM ) ulColorsPage, ( MPARAM ) hwColorsDlg );

        ulNumberPage = ( ULONG ) WinSendDlgItemMsg( hwnd, D_Prop_Notebook,
          BKM_INSERTPAGE, ( MPARAM ) NULL, MPFROM2SHORT( ( BKA_MINOR |
          BKA_STATUSTEXTON | BKA_AUTOPAGESIZE ), BKA_LAST ) );
        WinSendMsg( hwNotebook, BKM_SETSTATUSLINETEXT, ( MPARAM ) ulNumberPage,
          MPFROMP ( "Number display options (page 3 of 3)" ) );
        hwNumberDlg = WinLoadDlg( hwNotebook, hwNotebook, NumberDlgProc,
          ( HMODULE ) 0, D_Number_Dialog, NULL );
        WinSendMsg( hwNotebook, BKM_SETPAGEWINDOWHWND,
          ( MPARAM ) ulNumberPage, ( MPARAM ) hwNumberDlg );

        ulCellsPage = SB2_AddDlgPage( hwnd, D_Prop_Notebook, "Cells",
          "Information cells" );
        hwCellsDlg = WinLoadDlg( hwNotebook, hwNotebook, CellsDlgProc,
          ( HMODULE ) 0, D_Cell_Dialog, NULL );
        WinSendDlgItemMsg( hwnd, D_Prop_Notebook, BKM_SETPAGEWINDOWHWND,
          ( MPARAM ) ulCellsPage, ( MPARAM ) hwCellsDlg );

        ulAboutPage = SB2_AddDlgPage( hwnd, D_Prop_Notebook, "About",
          "About SysBar/2 Pipe Monitor" );
        hwAboutDlg = WinLoadDlg( hwNotebook, hwNotebook, AboutDlgProc,
          ( HMODULE ) 0, DLG_ABOUT, NULL );
        WinSendDlgItemMsg( hwnd, D_Prop_Notebook, BKM_SETPAGEWINDOWHWND,
          ( MPARAM ) ulAboutPage, ( MPARAM ) hwAboutDlg );

        WinSendDlgItemMsg( hwnd, D_Prop_Notebook, BKM_SETDIMENSIONS,
          MPFROM2SHORT ( 21, 21 ), MPFROMSHORT ( BKA_PAGEBUTTON ) );
        WinSendDlgItemMsg( hwnd, D_Prop_Notebook, BKM_SETDIMENSIONS,
          MPFROM2SHORT ( 80, 20 ), MPFROMSHORT ( BKA_MAJORTAB ) );
        WinSendDlgItemMsg( hwnd, D_Prop_Notebook, BKM_SETDIMENSIONS,
          MPFROM2SHORT ( 0, 0 ), MPFROMSHORT ( BKA_MINORTAB ) );
      }
      break;
/*
    case WM_COMMAND:
      switch ( SHORT1FROMMP( mp1 ) )
      {
        case DID_OK:
        case DID_CANCEL:
          WinDismissDlg( hwnd, SHORT1FROMMP( mp1 ) );
          break;
        default:
          return WinDefDlgProc( hwnd, msg, mp1, mp2 );
      }
      break;
*/
    case WM_CLOSE:
      hwDlgToUpdate = NULL;
      WinDestroyWindow( hwSettingsDlg );
      WinDestroyWindow( hwNumberDlg );
      WinDestroyWindow( hwCellsDlg );
      WinDestroyWindow( hwAboutDlg );
      WinPostMsg( hwPipeClient, WM_COMMAND, ( MPARAM ) MNU_PIPE_SAVE, 0 );
    default:
      return WinDefDlgProc( hwnd, msg, mp1, mp2 );
  }
  return ( MRESULT ) FALSE;
}

HWND hwPrevWindow = HWND_DESKTOP;

void PopUpMainWindow( void )
{
  if ( iTopmost >= 2 ) hwPrevWindow = WinQueryWindow( hwPipeFrame, QW_PREV );
  WinSetWindowPos( hwPipeFrame, HWND_TOP, 0L, 0L, 0L, 0L,
    SWP_ZORDER | SWP_SHOW );
  bLastVisible = 1;
}





// Main window procedure -----------------------------------------------------
MRESULT EXPENTRY PipeWinProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
  switch ( msg )
  {
    case WM_CREATE:
      {
//        HPS hps = WinGetPS( hwnd );
//        hbQ = GpiLoadBitmap( hps, NULLHANDLE, BMP_Q, 20, 20 );
//        WinReleasePS( hps );
        hmPopupMenu = WinLoadMenu( hwnd, NULLHANDLE, MNU_PIPE );
        MENUITEM mi;
        WinSendMsg( hmPopupMenu, MM_QUERYITEM,
          MPFROM2SHORT( MNU_PIPE_SUBMENU, TRUE ), MPFROMP( &mi ) );
        ULONG ulStyle = WinQueryWindowULong( mi.hwndSubMenu, QWL_STYLE );
        WinSetWindowULong( mi.hwndSubMenu, QWL_STYLE, ulStyle |
          MS_CONDITIONALCASCADE );
        WinStartTimer( hab, hwnd, TID_USERMAX + 9, 1000 );
      }
      break;

    case WM_COMMAND:
      {
        USHORT uCommand = SHORT1FROMMP( mp1 );
        HideDescWindow();

        switch ( uCommand )
        {
          case MNU_PIPE_SAVE:
            SaveOptions();
            break;
          case MNU_PIPE_PROPERTIES:
            WinDlgBox( HWND_DESKTOP, hwPipeFrame, PropertiesDlgProc,
              hmSysBar2Dll, DLG_PROPERTIES, ( PVOID ) &DummyInit );
            break;
          case MNU_PIPE_CLOSE:
            WinPostMsg( hwPipeFrame, WM_CLOSE, 0, 0 );
            break;

          case MNU_PIPE_LOCKPOSITION:
            bLockPosition ^= 1;
            break;

          case MNU_PIPE_NOTOP:
          case MNU_PIPE_TOPMOST:
          case MNU_PIPE_POPUP:
          case MNU_PIPE_POPUP2:
            iTopmost = uCommand - MNU_PIPE_NOTOP;
            break;
          case MNU_PIPE_SMALL:
          case MNU_PIPE_LARGE:
          case MNU_PIPE_AUTO:
          case MNU_PIPE_CUSTOM:
            iLarge = uCommand - int ( MNU_PIPE_SMALL );
            ResizeWindow();
            if ( iBindToCorner ) WinPostMsg( hwnd, WM_COMMAND,
              ( MPARAM ) ( MNU_PIPE_BIND_OFF + iBindToCorner ), 0 );
            break;
          case MNU_PIPE_BIND_OFF:
          case MNU_PIPE_BIND_NW:
          case MNU_PIPE_BIND_NE:
          case MNU_PIPE_BIND_SW:
          case MNU_PIPE_BIND_SE:
            if ( ( iBindToCorner = uCommand - MNU_PIPE_BIND_OFF ) > 0 )
            {
              int x = -1;
              int y = -1;
              if ( iBindToCorner == 1 || iBindToCorner == 2 )
                y = WinQuerySysValue( HWND_DESKTOP, SV_CYSCREEN ) -
                  iWindowHeight + 1;
              if ( iBindToCorner == 2 || iBindToCorner == 4 )
                x = WinQuerySysValue( HWND_DESKTOP, SV_CXSCREEN ) -
                  iWindowWidth + 1;
              WinSetWindowPos( hwPipeFrame, HWND_TOP, x, y, 0, 0, SWP_MOVE );

              int iNewOrientation = ( iBindToCorner & 1 ) ? 1 : 0;
              if ( iNewOrientation != iOrientation )
                WinPostMsg( hwPipeClient, WM_COMMAND,
                  ( MPARAM ) ( MNU_PIPE_ORNT_LEFT + iNewOrientation ), 0 );
            }
            break;
          case MNU_PIPE_ORNT_LEFT:
          case MNU_PIPE_ORNT_RIGHT:
            iOrientation = int ( uCommand ) - MNU_PIPE_ORIENTATION - 1;
            ResizeWindow();
            UpdateSettingsDlg();
            if ( iBindToCorner ) WinPostMsg( hwnd, WM_COMMAND,
              ( MPARAM ) ( MNU_PIPE_BIND_OFF + iBindToCorner ), 0 );
            break;

          case MNU_PIPE_COPY:
            if ( iLastCell != -1 ) CopyCell( iLastCell );
            break;

          case MNU_PIPE_EMPTY:
            if ( iLastCell != -1 ) EmptyCell( iLastCell );
            break;

          case MNU_PIPE_REMOVE:
            if ( iLastCell != -1 )
            {
              RemoveCell( iLastCell );
              SaveOptions();
            }
            break;

          case MNU_PIPE_SETUP:
            if ( iLastCell != -1 )
            {
              EditCell( hwnd, iLastCell );
              SaveOptions();
            }
            break;

          case MNU_PIPE_EVENT:
            {
              NewCellData *p = ( NewCellData* ) mp2;
              if ( p )
              {
                SetCellData( p->iIndex, p->szText );
                delete p;
              }
            }
            break;
        }
      }
      break;

    case WM_BUTTON1DOWN:
      {
        bLastVisible = 0;
        iLastCell = iDescCell;
        HideDescWindow();
        if ( ! bLockPosition )
          WinPostMsg( hwPipeFrame, WM_TRACKFRAME, ( MPARAM ) TF_MOVE, 0L );
      }
      break;

    case WM_BUTTON1DBLCLK:
      bLastVisible = 0;
      if ( iLastCell >= 0 ) if ( pCells[iLastCell].szCellClickApp[0] &&
        ( pCells[iLastCell].iCellStatus & Cell_Click_App ) )
      {
/*
        CHAR LoadError[CCHMAXPATH] = { 0 };
        PSZ pszArgs = NULL;
        PSZ pszEnvs = NULL;
        RESULTCODES ChildRC = { 0 };
        DosExecPgm( LoadError, sizeof( LoadError ), EXEC_ASYNCRESULT, pszArgs,
          pszEnvs, &ChildRC, pPipes[iLastCell].szPipeExec );
*/
        SpawnIt( pCells[iLastCell].szCellClickApp,
          pCells[iLastCell].szCellData );
//        spawnlp( P_NOWAIT, pCells[iLastCell].szCellClickApp,
//            pCells[iLastCell].szCellClickApp, NULL );
      }
      break;

    case WM_BUTTON2CLICK:
      {
        bLastVisible = 0;
        iLastCell = iDescCell;
        HideDescWindow();
        int x = SHORT1FROMMP( mp1 ),
          y = SHORT2FROMMP( mp1 );
        WinEnableMenuItem( hmPopupMenu, MNU_PIPE_SETUP, ( iLastCell > -1 ) );
        WinEnableMenuItem( hmPopupMenu, MNU_PIPE_SUBMENU, ( iLastCell > -1 ) );
        WinEnableMenuItem( hmPopupMenu, MNU_PIPE_COPY, ( iLastCell > -1 ) );
        bAlienMenu = 1;
        WinPopupMenu( hwnd, hwnd, hmPopupMenu, x, y, 0,
          PU_HCONSTRAIN | PU_VCONSTRAIN | PU_NONE |
          PU_KEYBOARD | PU_MOUSEBUTTON1 | PU_MOUSEBUTTON2 );
      }
      break;

    case WM_TIMER:
      switch ( ( USHORT ) ( mp1 ) )
      {
        case TID_USERMAX + 9:
          {
            DosPostEventSem( hevTimed );
            SWP swp;
            WinQueryWindowPos( hwPipeFrame, &swp );
            POINTL ptl = { 0, 0 };
            WinQueryPointerPos( HWND_DESKTOP, &ptl );
            ptl.x -= swp.x;
            ptl.y -= swp.y;
            AdjustDescPos( ptl.x, ptl.y );

            short int bOnTop = iTopmost;

            if ( iTopmost >= 2 )
            {
              SWP swp;
              WinQueryWindowPos( hwPipeFrame, &swp );
              POINTL ptl = { 0, 0 };
              WinQueryPointerPos( HWND_DESKTOP, &ptl );
              ptl.x -= swp.x;
              ptl.y -= swp.y;
              if ( ptl.x >= 0 && ptl.y >= 0 &&
                ptl.x < iWindowWidth && ptl.y < iWindowHeight )
              {
                if ( ! bLastVisible ) bLastVisible = bOnTop = 1;
              }
              else if ( bLastVisible )
              {
                if ( iTopmost == 3 ) WinSetWindowPos( hwPipeFrame, hwPrevWindow,
                  0L, 0L, 0L, 0L, SWP_ZORDER );
                bLastVisible = 0;
              }
            }
    
            if ( bOnTop == 1 ) PopUpMainWindow();
          }
          break;
      }
      break;

    case WM_ERASEBACKGROUND:
      bClearIt = 1;
      break;

    case WM_PAINT:
      {
        HPS hps;
        RECTL rc;
        hps = WinBeginPaint( hwnd, 0L, &rc );

        if ( bClearIt )
        {
          bClearIt = 0;
          SB2_Border( hps, 0, 0, iWindowWidth - 1, iWindowHeight - 1, 1,
            SB2c_Filler, 1 );
        }
        for ( int i = 0; i < iCellCount; i++ )
          if ( pCells[i].iCellWidth ) PaintCell( hps, i, 1 );
        WinEndPaint( hps );
      }
      break;

    case WM_CLOSE:
      HideDescWindow();
      SaveOptions();
      WinPostMsg( hwnd, WM_QUIT, 0, 0 );
      break;

    case WM_DESTROY:
      {
        for ( int i = 0; i < iCellCount; i++ )
        {
          ClosePipe( i );
          WriteCellLog( i, "*Closed*\n" );
        }
      }
      DosCloseEventSem( hevPipeGuard );
      DosCloseEventSem( hevTimed );
      DosCloseEventSem( hevHeavyTimed );
      WinStopTimer( hab, hwnd, TID_USERMAX + 9 );
      break;

    case WM_MENUEND:
      bAlienMenu = 0;
      break;

    case WM_MOUSEMOVE:
      if ( iTopmost >= 2 && ! bLastVisible ) PopUpMainWindow();
      AdjustDescPos( SHORT1FROMMP( mp1 ), SHORT2FROMMP( mp1 ), 0 );
    
    default:
      return WinDefWindowProc( hwnd, msg, mp1, mp2 );
  }
  return ( MRESULT ) FALSE;
}


char aStack[12288];

void FAR CheckStatus( void FAR *pParam )
{
  for ( int i = 0; i < iCellCount; i++ )
  {
    InitCell( i );
    WriteCellLog( i, "*Started*" );
  }

  ULONG uPosts;
  while ( 1 )
  {
/*
  if ( argc <= 1 ) return 1;


  iPipeCount = argc - 1;
  iPipeStatsCount = iPipeCount * 2;
  pPipeHandles = new HPIPE[iPipeCount];
  pPipeStates = new PIPESEMSTATE[iPipeStatsCount];

  for ( int i = 0; i < iPipeCount; i++ )
  {
    DosCreateNPipe( argv[i + 1], &pPipeHandles[i], NP_ACCESS_INBOUND,
      NP_NOWAIT | NP_TYPE_BYTE | NP_READMODE_BYTE | 1, iPipeBufSize,
      iPipeBufSize, 0L );
    DosSetNPipeSem( pPipeHandles[i], ( HSEM ) hevPipeGuard, i );
    DosConnectNPipe( pPipeHandles[i] );
  }

  while ( 1 )
  {
    ULONG uPosts;
*/
      DosResetEventSem( hevPipeGuard, &uPosts );
      if ( DosWaitEventSem( hevPipeGuard, SEM_INDEFINITE_WAIT ) ) break;
      int iPipeStatsCount = ( iPipeCount << 1 );
      PPIPESEMSTATE pPipeStates = new PIPESEMSTATE[iPipeStatsCount];
      if ( pPipeStates )
      {
        DosQueryNPipeSemState( ( HSEM ) hevPipeGuard, pPipeStates,
          sizeof ( PIPESEMSTATE ) * iPipeStatsCount );
        for ( int i = iPipeStatsCount - 1; i >= 0; i-- )
        {
          if ( pPipeStates[i].fStatus )
          {
            int iIndex;
            for ( iIndex = 0; iIndex < iCellCount; iIndex++ )
              if ( pPipeStates[i].usKey == pCells[iIndex].iCellIndex ) break;
            if ( iIndex < iCellCount ) switch ( pPipeStates[i].fStatus )
            {
              case NPSS_RDATA:
                {
                  ULONG iCount = 0;
                  while ( DosRead( pCells[iIndex].hCellHandle, PipeBuffer,
                    iPipeBufSize - 1, &iCount ) == 0 )
                  {
                    if ( iCount )
                    {
                      PipeBuffer[iCount] = 0;
                      NewCellData *p = new NewCellData;
                      if ( p )
                      {
                        p->iIndex = iIndex;
                        memset( p->szText, 0, iPipeBufSize );
                        strncpy( p->szText, ( char* ) PipeBuffer,
                          iPipeBufSize - 1 );
                        WinPostMsg( hwPipeClient, WM_COMMAND,
                          ( MPARAM ) MNU_PIPE_EVENT, ( MPARAM ) p );
                      }
                    }
                    else
                    {
                      ResetPipe( iIndex );
                      break;
                    }
                  }
                }
                break;

              case NPSS_CLOSE:
                ResetPipe( iIndex );
                break;
            }
          }
        }
        delete pPipeStates;
      }
/*
    for ( int i = iPipeStatsCount - 1; i >= 0; i-- )
    {
      switch ( pPipeStates[i].fStatus )
      {
        case NPSS_RDATA:
          {
            ULONG iCount = 0;
            while ( DosRead( pPipeHandles[pPipeStates[i].usKey], PipeBuffer,
              iPipeBufSize, &iCount ) == 0 )
            {
              if ( iCount )
              {
                PipeBuffer[iCount] = 0;
                printf( "[#i]  read %i bytes: <%s>\n",
                  pPipeStates[i].usKey, iCount, PipeBuffer );
              }
              else break;
            }
          }
          break;

        case NPSS_CLOSE:
          DosDisConnectNPipe( pPipeHandles[pPipeStates[i].usKey] );
          DosConnectNPipe( pPipeHandles[pPipeStates[i].usKey] );
          break;
      }
    }
  }

  for ( i = 0; i < iPipeCount; i++ )
  {
    DosDisConnectNPipe( pPipeHandles[i] );
    DosClose( pPipeHandles[i] );
  }

  delete pPipeHandles;
  delete pPipeStates;
*/
  }
  _endthread();
}


typedef struct
{
  USHORT usParmLength;
  USHORT usPowerFlags;
  UCHAR ucACStatus;
  UCHAR ucBatteryStatus;
  UCHAR ucBatteryLife;
} IOCTLPowerStatus;
#define IOCTL_POWER 0x000C
#define POWER_GETPOWERSTATUS 0x0060

#define PQ_BUFSIZE  0x10000

char aStack2[12288];

struct ifmib Ifmib;
short int bIfmibUpdated;
time_t tIfmibTime = 0;
int iIfmibSocket = -1;

void FAR TimedProcedure( void FAR *pParam )
{
  ULONG uPosts;
  ProcessCollection pcList;
  static FSALLOCATE fsa;

  while ( 1 )
  {
    DosResetEventSem( hevTimed, &uPosts );
    if ( DosWaitEventSem( hevTimed, SEM_INDEFINITE_WAIT ) ) break;
    int iHeavyCells = 0;
    bIfmibUpdated = 0;
    for ( int i = 0; i < iCellCount; i++ )
      if ( pCells[i].iCellStatus & Cell_Enabled )
    {
      if ( pCells[i].iCellLazyCounter ) pCells[i].iCellLazyCounter--;
      if ( --( pCells[i].iCellCounter ) > 0 ) continue;

      NewCellData *p = NULL;
      switch ( pCells[i].iCellType )
      {
        case MCT_Pipe:
          if ( ( pCells[i].iCellStatus & Cell_Expire ) && pCells[i].iCellWidth )
            EmptyCell( i );
          break;
        case MCT_CPULoad:
          {
            if ( p = new NewCellData )
            {
              int iLoad = 1;
              if ( ( dpsc ) && ( pCells[i].iCellStatus & Cell_Check1 ) )
              {
                CPUUTIL cpuu[CPUS];
                ULLONG* pdTimePrev = ( (ULLONG*) pCells[i].szCellData0 );
                ULLONG* pdBusyPrev = ( (ULLONG*) pCells[i].szCellData0 ) + CPUS;
                if ( ! dpsc( CMD_KI_RDCNT, ( ULONG ) &cpuu[0], 0, 0 ) )
                {
                  int cpun = 0;
                  if(pCells[i].iCellStatus & Cell_Check2) cpun = 1;
                  ULLONG dTime = LL2F( cpuu[cpun].ulTimeHigh, cpuu[cpun].ulTimeLow );
                  ULLONG dBusy = LL2F( cpuu[cpun].ulBusyHigh, cpuu[cpun].ulBusyLow );
                  if( dTime == pdTimePrev[cpun] ) iLoad = 0;
                  else iLoad = (dBusy - pdBusyPrev[cpun])*100 / (dTime - pdTimePrev[cpun]);
                  pdTimePrev[cpun] = dTime;
                  pdBusyPrev[cpun] = dBusy;
                }
              }
              else
              {
                pcList.CollectData();
                iLoad = pcList.GetCPULoad();
                if ( iLoad < 0 ) iLoad = 0;
              }
              int iMaxLoad = ( pCells[i].iCellStatus & Cell_Check0 ) ? 99 : 100;
              if ( iLoad > iMaxLoad ) iLoad = iMaxLoad;
              if ( iLoad >= 0 ) sprintf( p->szText , "%02d%%", iLoad );
            }
          }
          break;
        case MCT_PhysMem:
          {
            if ( p = new NewCellData )
            {
              ULONG ulPhysMemFree;
              APIRET16 rc = Dos16MemAvail( &ulPhysMemFree );
              p->szText[0] = 0;
              FormatNumber( p->szText, &( pCells[i].noCellOptions ),
                ULLONG(ulPhysMemFree) );
            }
          }
          break;
        case MCT_Power:
          {
            HFILE hAPM;
            ULONG ulAction;
            DosOpen( "APM$", &hAPM, &ulAction, 0, 0,
              OPEN_ACTION_OPEN_IF_EXISTS,
              OPEN_ACCESS_READWRITE | OPEN_SHARE_DENYNONE, NULL );
            if ( p = new NewCellData )
            {
              strcpy( p->szText, pszErrors[APM_CantInit] );
              IOCTLPowerStatus BatStatus;
              BatStatus.usParmLength = 7;
              ULONG ulSize = sizeof( BatStatus );
              USHORT usRet;
              ULONG usRetSize = sizeof( usRet );
              if ( hAPM )
              {
                APIRET rc = DosDevIOCtl( ( HFILE ) hAPM, IOCTL_POWER,
                  POWER_GETPOWERSTATUS, ( PVOID ) &BatStatus,
                  sizeof( BatStatus ), &ulSize, ( PVOID ) &usRet,
                  sizeof( usRet ), &usRetSize );
                if ( BatStatus.ucACStatus == 1 || BatStatus.ucACStatus == 0 )
                {
                  strcpy( p->szText, BatStatus.ucACStatus == 1 ? "AC" : "DC" );
                  if ( BatStatus.ucBatteryLife <= 100 ) sprintf( p->szText + 2,
                    ": %i%%", ( int ) BatStatus.ucBatteryLife );
                }
                DosClose( hAPM );
              }
            }
          }
          break;
        case MCT_FreeSpace:
          {
            memset( &fsa, 0, sizeof( FSALLOCATE ) );
            if ( p = new NewCellData )
            {
              DosQueryFSInfo( pCells[i].szCellData1[0] - '0' + 1, FSIL_ALLOC,
                ( void * ) &fsa, sizeof( FSALLOCATE ) );
              ULLONG dSize = ULLONG( fsa.cUnitAvail ) *
                ULLONG( fsa.cSectorUnit ) * ULLONG( fsa.cbSector );
              p->szText[0] = 0;
              if ( dSize > 0 )
                FormatNumber( p->szText, &( pCells[i].noCellOptions ), dSize );
            }
          }
          break;
        case MCT_Clock:
          {
            if ( p = new NewCellData )
              FormatDateTime( p->szText, pCells[i].iCellStatus );
          }
          break;
        case MCT_Uptime:
          {
            if ( p = new NewCellData )
              FormatDateTime( p->szText, pCells[i].iCellStatus, 0, FDT_Uptime );
          }
          break;
        case MCT_Calendar:
          {
            if ( p = new NewCellData )
              FormatDateTime( p->szText, 0, pCells[i].iCellStatus, FDT_Date );
          }
          break;
        case MCT_Processes:
          {
            if ( p = new NewCellData )
            {
              p->szText[0] = 0;
              int iDisplayed = 0;
              if ( pCells[i].iCellStatus & Cell_Check0 )
              {
                sprintf( p->szText, "%lu", WinQuerySwitchList( hab, NULL, 0 ) );
                iDisplayed++;
              }
              if ( pCells[i].iCellStatus &
                ( Cell_Check1 | Cell_Check2 | Cell_Check3 ) )
              {
                PQTOPLEVEL pqData = ( PQTOPLEVEL ) new char[PQ_BUFSIZE];
                if ( pqData )
                {
                  memset( pqData, 0, PQ_BUFSIZE );
                  if ( ! DosQuerySysState( 1, 0, 0, 0, pqData, PQ_BUFSIZE ) )
                  {
                    PQGLOBAL pqGlobal = pqData->gbldata;
                    if ( pCells[i].iCellStatus & Cell_Check1 )
                      sprintf( p->szText + strlen( p->szText ), "%s%lu",
                        iDisplayed++ ? "/" : "", pqGlobal->threadcnt );
                    if ( pCells[i].iCellStatus & Cell_Check2 )
                      sprintf( p->szText + strlen( p->szText ), "%s%lu",
                        iDisplayed++ ? "/" : "", pqGlobal->modulecnt );
//                    if ( pCells[i].iCellStatus & Cell_Check3 )
//                      sprintf( p->szText + strlen( p->szText ), "%s%lu",
//                        iDisplayed++ ? "/" : "", pqGlobal->proccnt );
                  }
                  delete pqData;
                }
              }
            }
          }
          break;
        case MCT_FileSize:
          {
            if ( p = new NewCellData )
            {
              HDIR hFindHandle = HDIR_SYSTEM;
              FILEFINDBUF3 FindBuffer = { 0 };
              ULONG ulResultBufLen = sizeof( FILEFINDBUF3 );
              ULONG ulFindCount = 1;
              ULLONG dSize = 0;
              int iCount = 0;

              if ( DosFindFirst( pCells[i].szCellData1, &hFindHandle,
                FILE_NORMAL/* | FILE_READONLY | FILE_SYSTEM | FILE_HIDDEN |
                FILE_ARCHIVED*/, &FindBuffer, ulResultBufLen, &ulFindCount,
                FIL_STANDARD ) == NO_ERROR )
              {
                do
                {
                  dSize += FindBuffer.cbFile;
                  iCount++;
                }
                while ( DosFindNext( hFindHandle, &FindBuffer, ulResultBufLen,
                  &ulFindCount ) == NO_ERROR );
              }
              DosFindClose( hFindHandle );

              p->szText[0] = 0;
              FormatCount( p->szText, iCount, dSize, &pCells[i] );
            }
          }
          break;

        case MCT_TrafIn:
        case MCT_TrafOut:
          {
            if ( p = new NewCellData )
            {
              p->szText[0] = 0;
              if ( iIfmibSocket == -1 )
                iIfmibSocket = Socket( PF_INET, SOCK_STREAM, 0 );
              if ( iIfmibSocket != -1 )
              {
                if ( ! bIfmibUpdated )
                {
                  memset( &Ifmib, 0, sizeof ( struct ifmib ) );
                  SoIOCtl( iIfmibSocket, SIOSTATIF,
                    ( caddr_t ) &Ifmib, sizeof ( struct ifmib ) );
                  tIfmibTime = time( NULL );
                  bIfmibUpdated = 1;
                }
                for ( int j = 0; j < IFMIB_ENTRIES; j++ )
                {
                  if ( Ifmib.iftable[j].ifIndex == *(int*)pCells[i].szCellData1 )
                  {
                    TrafData* pTrafData = ( TrafData* ) pCells[i].szCellData0;
                    int iOctets = ( pCells[i].iCellType == MCT_TrafIn ?
                      Ifmib.iftable[j].ifInOctets :
                      Ifmib.iftable[j].ifOutOctets );
                    if ( pTrafData->uInitialized != TRAF_INIT )
                    {
                      pTrafData->uInitialized = TRAF_INIT;
                      pTrafData->tInitialTime = pTrafData->tLastTime = tIfmibTime;
                      pTrafData->lMaxCPS = 0;
                      pTrafData->lBytes = 0;
                      pTrafData->uInitialOctets = pTrafData->iLastOctets = iOctets;
                    }
                    else
                    {
                      ULLONG dCPS = (ULONG)(iOctets - pTrafData->iLastOctets);
                      pTrafData->lBytes += dCPS;

                      if( tIfmibTime != pTrafData->tLastTime ) {
                        dCPS /= (tIfmibTime - pTrafData->tLastTime);
                        if ( pTrafData->lMaxCPS < dCPS ) pTrafData->lMaxCPS = dCPS;
                      }

                      int iDisplayed = 0;
                      if ( pCells[i].iCellStatus & Cell_Check0 )
                        FormatNumber( p->szText, &( pCells[i].noCellOptions ),
                          dCPS, iDisplayed++ );
                      if ( pCells[i].iCellStatus & Cell_Check1 )
                        FormatNumber( p->szText, &( pCells[i].noCellOptions ),
                          pTrafData->lBytes / (tIfmibTime - pTrafData->tInitialTime),
                          iDisplayed++ );
                      if ( pCells[i].iCellStatus & Cell_Check2 )
                        FormatNumber( p->szText, &( pCells[i].noCellOptions ),
                          pTrafData->lMaxCPS, iDisplayed++ );
                      if ( pCells[i].iCellStatus & Cell_Check3 )
                        FormatNumber( p->szText, &( pCells[i].noCellOptions ),
                          pTrafData->uInitialOctets + pTrafData->lBytes, iDisplayed );

                      pTrafData->tLastTime = tIfmibTime;
                      pTrafData->iLastOctets = iOctets;
                    }
                    break;
                  }
                }
              }
            }
          }
          break;


        default:
          iHeavyCells++;
          pCells[i].hCellHandle = 1;
          break;
      }

      if ( p )
      {
        p->iIndex = i;
        WinPostMsg( hwPipeClient, WM_COMMAND, ( MPARAM ) MNU_PIPE_EVENT,
          ( MPARAM ) p );
      }

      pCells[i].iCellCounter = pCells[i].iCellSeconds;
    }
    if ( iHeavyCells ) DosPostEventSem( hevHeavyTimed );
  }
  _endthread();
}

char aStack3[12288];

int RecvData( int iSocket, char* pszBuf, int iBufSize )
{
  int iBytes = 0, l = 0;
  for ( ; iBufSize > 0; pszBuf += l, iBufSize -= l )
  {
    if ( ( l = Recv( iSocket, pszBuf, iBufSize, 0 ) ) > 0 )
    {
      pszBuf[l] = 0;
      iBytes += l;
      if ( pszBuf[l - 1] == '\n' || pszBuf[l - 1] == '\r' ) break;
    }
    else break;
  }
  return iBytes;
}

void FAR HeavyTimedProcedure( void FAR *pParam )
{
  ULONG uPosts;
  char szBuf[128];
  long l;

  while ( 1 )
  {
    DosResetEventSem( hevHeavyTimed, &uPosts );
    if ( DosWaitEventSem( hevHeavyTimed, SEM_INDEFINITE_WAIT ) ) break;
    for ( int i = 0; i < iCellCount; i++ )
      if ( pCells[i].iCellStatus & Cell_Enabled && pCells[i].hCellHandle )
    {
      NewCellData *p = NULL;
      switch ( pCells[i].iCellType )
      {
        case MCT_POP3:
          {
            pCells[i].hCellHandle = 0;
            if ( p = new NewCellData )
            {
              p->szText[0] = 0;
              short int bShowErrors = pCells[i].iCellStatus & Cell_Check2;
              short int bWriteLog = ( ( pCells[i].iCellStatus & ( Cell_Log_On |
                Cell_Check3 ) ) == ( Cell_Log_On | Cell_Check3 ) );

              Pop3URL* pUrl = ( Pop3URL* ) pCells[i].szCellData0;
              unsigned long ulAddress = InetAddr( pUrl->pszHost );
              if ( ulAddress == -1L )
              {
                struct hostent *hp = GetHostByName( pUrl->pszHost );
                if ( hp ) ulAddress = *( unsigned long* ) hp->h_addr;
              }
              if ( ulAddress == -1L )
              {
                if ( bShowErrors ) sprintf( p->szText,
                  pszErrors[P3E_UnknownHost], pUrl->pszHost );
                break;
              }

              int iSocket = Socket( AF_INET, SOCK_STREAM, 0 );
              if ( iSocket == -1 )
              {
                if ( bShowErrors )
                  sprintf( p->szText, pszErrors[P3E_Socket] );
                break;
              }
              sockaddr_in sa;
              memset( &sa, 0, sizeof ( sa ) );
              sa.sin_family = AF_INET;
              sa.sin_addr.s_addr = ulAddress;
              sa.sin_port = BSwap( pUrl->iPort );
              Connect( iSocket, ( struct sockaddr* ) &sa, sizeof ( sa ) );

              do
              {
                l = RecvData( iSocket, szBuf, sizeof ( szBuf ) );
                if ( l <= 0 )
                {
                  if ( bShowErrors ) sprintf( p->szText,
                    pszErrors[P3E_CantReach], pUrl->pszHost );
                  break;
                }
                else
                {
                  szBuf[l] = 0;
                  if ( bWriteLog ) WriteCellLog( i, szBuf, 1 );
                }

                sprintf( szBuf, "USER %s\r\n", pUrl->pszUsername );
                if ( Send( iSocket, szBuf, strlen( szBuf ), 0 ) <= 0 )
                {
                  if ( bShowErrors ) sprintf( p->szText,
                    pszErrors[P3E_UserSnd], pUrl->pszUsername );
                  break;
                }
                else if ( bWriteLog ) WriteCellLog( i, szBuf, 1 );
                if ( ( l = RecvData( iSocket, szBuf, sizeof ( szBuf ) ) ) <= 0 )
                {
                  if ( bShowErrors ) sprintf( p->szText,
                    pszErrors[P3E_UserRcv], pUrl->pszUsername );
                  break;
                }
                else
                {
                  szBuf[l] = 0;
                  if ( bWriteLog ) WriteCellLog( i, szBuf, 1 );
                }
                if ( szBuf[0] == '-' )
                {
                  if ( bShowErrors ) sprintf( p->szText,
                    pszErrors[P3E_User], pUrl->pszUsername );
                  break;
                }

                sprintf( szBuf, "PASS %s\r\n", pUrl->pszPassword );
                if ( Send( iSocket, szBuf, strlen( szBuf ), 0 ) <=0 )
                {
                  if ( bShowErrors ) sprintf( p->szText,
                    pszErrors[P3E_PasswordSnd], pUrl->pszUsername );
                  break;
                }
                else if ( bWriteLog ) WriteCellLog( i, szBuf, 1 );
                if ( ( l = RecvData( iSocket, szBuf, sizeof ( szBuf ) ) ) <= 0 )
                {
                  if ( bShowErrors ) sprintf( p->szText,
                    pszErrors[P3E_PasswordRcv], pUrl->pszUsername );
                  break;
                }
                else
                {
                  szBuf[l] = 0;
                  if ( bWriteLog ) WriteCellLog( i, szBuf, 1 );
                }
                if ( szBuf[0] == '-' )
                {
                  if ( bShowErrors ) sprintf( p->szText,
                    pszErrors[P3E_Password], pUrl->pszUsername );
                  break;
                }

                sprintf( szBuf, "STAT\r\n" );
                if ( Send( iSocket, szBuf, strlen( szBuf ), 0 ) <=0 )
                {
                  if ( bShowErrors ) sprintf( p->szText,
                    pszErrors[P3E_StatSnd], pUrl->pszUsername );
                  break;
                }
                else if ( bWriteLog ) WriteCellLog( i, szBuf, 1 );
                if ( ( l = RecvData( iSocket, szBuf, sizeof ( szBuf ) ) ) <= 0 )
                {
                  if ( bShowErrors ) sprintf( p->szText,
                    pszErrors[P3E_StatRcv], pUrl->pszUsername );
                  break;
                }
                else
                {
                  szBuf[l] = 0;
                  if ( bWriteLog ) WriteCellLog( i, szBuf, 1 );
                }
                if ( szBuf[0] == '-' )
                {
                  if ( bShowErrors ) sprintf( p->szText,
                    pszErrors[P3E_Stat], pUrl->pszUsername );
                  break;
                }
                else
                {
                  if ( pCells[i].iCellStatus & Cell_Check3 )
                    strcpy( p->szText, szBuf );
                  else
                  {
                    ULLONG iCount = 0, iSize = 0;
                    sscanf( szBuf, "+OK %I64u %I64u", &iCount, &iSize );
                    if ( iCount > 0 ) FormatCount( p->szText, iCount,
                      iSize, &pCells[i] );
                  }
                }
              }
              while ( 0 );
              sprintf( szBuf, "QUIT\r\n" );
              Send( iSocket, szBuf, strlen( szBuf ), 0 );
              if ( bWriteLog ) WriteCellLog( i, szBuf, 1 );
              SoClose( iSocket );

            }
          }
          break;
      }

      if ( p )
      {
        p->iIndex = i;
        WinPostMsg( hwPipeClient, WM_COMMAND, ( MPARAM ) MNU_PIPE_EVENT,
          ( MPARAM ) p );
      }
    }
  }
  _endthread();
}


ULONG APIENTRY BugHandler( PEXCEPTIONREPORTRECORD p1,
  PEXCEPTIONREGISTRATIONRECORD p2, PCONTEXTRECORD p3, PVOID pv)
{
  if ( p1->ExceptionNum == XCPT_ACCESS_VIOLATION &&
    p1->ExceptionInfo[1] == 0xFFFFFFFF && p1->ExceptionInfo[0] == 0 )
    return XCPT_CONTINUE_EXECUTION;
  else return XCPT_CONTINUE_SEARCH;
}


// Main procedure ------------------------------------------------------------
int main( int argc, char *argv[] )
{
  HMQ hmq;
  QMSG qmsg;
  char szClassName[] = "SysBar2PipeClass";
  char szDescClassName[] = "SysBar2PipeDescClass";

  memset( &fd, 0, sizeof ( FONTDLG ) );
  fd.cbSize = sizeof ( FONTDLG );
  fd.pszFamilyname = szFamilyname;
  fd.usFamilyBufLen = sizeof ( szFamilyname );
  fd.fl = FNTS_BITMAPONLY | FNTS_CENTER | FNTS_INITFROMFATTRS;

  if ( ( hab = WinInitialize( 0 ) ) == 0L ) AbortStartup();
  if ( ( hmq = WinCreateMsgQueue( hab, 0 ) ) == 0L ) AbortStartup();

  {
    EXCEPTIONREGISTRATIONRECORD RegRec = { 0 };
    RegRec.ExceptionHandler = ( ERR ) BugHandler;
    if ( DosSetExceptionHandler( &RegRec ) != NO_ERROR ) AbortStartup();
  }

  hmSysBar2Dll = SB2_Init();
  iDefaultBackColor = SB2_ColorCount() - 1;

  if ( ! DosLoadModule( NULL, 0, "DOSCALL1.DLL", &hmDosCall1 ) )
    DosQueryProcAddr( hmDosCall1, 976, "DosPerfSysCall", ( PFN* ) &dpsc );
  if ( dpsc ) dpsc(CMD_KI_ENABLE, 0, 0, 0);
  else pszCellCheckNames[MCT_CPULoad][1] = NULL;

  int n = 0;
  if ( ! DosLoadModule( NULL, 0, "SO32DLL.DLL", &hmSo32DLL ) &&
    ! DosLoadModule( NULL, 0, "TCP32DLL.DLL", &hmTCP32DLL ) )
  {
    for ( int i = 0; i < iSocketCallCount; i++ ) if ( ! DosQueryProcAddr(
      i < iTCPCallCount ? hmTCP32DLL : hmSo32DLL, 0, pszSocketCalls[i],
      pSocketFns[i] ) ) n++;
  }
  if ( n < iSocketCallCount )
  {
    if ( hmSo32DLL ) DosFreeModule( hmSo32DLL );
    if ( hmTCP32DLL ) DosFreeModule( hmTCP32DLL );
    hmSo32DLL = NULLHANDLE;
    hmTCP32DLL = NULLHANDLE;
  }

  if ( ! WinRegisterClass( hab, ( PSZ ) szClassName, 
    ( PFNWP ) PipeWinProc, 0L, 0 ) ) AbortStartup();
  if ( ! WinRegisterClass( hab, ( PSZ ) szDescClassName,
    ( PFNWP ) SwDescWinProc, CS_SIZEREDRAW, 0 ) ) AbortStartup();

  {
    TIB *tib;
    PIB *pib;
    DosGetInfoBlocks( &tib, &pib );
    char szSemName[48];

    sprintf( szSemName, "\\SEM32\\PIPE\\SB2_PIPE%i", pib->pib_ulpid );
    if ( DosCreateEventSem( szSemName, &hevPipeGuard, 0L, 0L ) )
      AbortStartup();

    sprintf( szSemName, "\\SEM32\\SB2PIPE_T%i", pib->pib_ulpid );
    if ( DosCreateEventSem( szSemName, &hevTimed, 0, 0 ) ) AbortStartup();
    else _beginthread( TimedProcedure, aStack2, sizeof ( aStack2 ), NULL );

    sprintf( szSemName, "\\SEM32\\SB2PIPE_HT%i", pib->pib_ulpid );
    if ( DosCreateEventSem( szSemName, &hevHeavyTimed, 0, 0 ) ) AbortStartup();
    else _beginthread( HeavyTimedProcedure, aStack3, sizeof ( aStack3 ), NULL );
  }

  if ( argc > 1 ) pszIniFile = argv[1];
  else
  {
    pszIniFile = new char[strlen( argv[0] ) + 1];
    SB2_CfgFilename( pszIniFile, argv[0] );
  }

  ULONG ulWinStyle = FCF_AUTOICON | FCF_ICON | FCF_NOBYTEALIGN | FCF_TASKLIST;

  if ( ( hwPipeFrame = WinCreateStdWindow( HWND_DESKTOP, 0, &ulWinStyle,
    szClassName, 0, 0, NULLHANDLE, ICO_MAIN, &hwPipeClient ) ) == 0L )
    AbortStartup();

  if ( ! ( pPipeDesc = new DescWindow ) ) AbortStartup();
  else if ( ! pPipeDesc->CreateWindow( szDescClassName ) ) AbortStartup();

  WinSetWindowText( hwPipeFrame, "SysBar/2 Pipe Monitor" );

//  iSwCellSize[2] = WinQuerySysValue( HWND_DESKTOP, SV_CXICON ) + 2;

  {
    IniFile *pCfg = new IniFile( pszIniFile );
    int x = 0, y = 0, s = 0;
    if ( pCfg && ( *pCfg )() )
    {
      Delimiter = *pCfg->Get( szSysBar2Pipe, "delimiter", "," );
      x = atoi2( pCfg->Get( szSysBar2Pipe, "x" ) );
      y = atoi2( pCfg->Get( szSysBar2Pipe, "y" ) );
      if ( ( s = atoi2( pCfg->Get( szSysBar2Pipe, "customsize" ) ) ) > 0 )
        iCellSize[3] = s;
      iLarge = atoi2( pCfg->Get( szSysBar2Pipe, "size" ) ) & 3;
      iBindToCorner = atoi2( pCfg->Get( szSysBar2Pipe, "cornerbind" ) );
      if ( iBindToCorner > 4 || iBindToCorner < 0 ) iBindToCorner = 0;
      iOrientation = atoi2( pCfg->Get( szSysBar2Pipe, "orientation" ) ) & 1;
      iTopmost = atoi2( pCfg->Get( szSysBar2Pipe, "topmost" ) ) & 3;
      bLockPosition = ( strcmp2(
        pCfg->Get( szSysBar2Pipe, "lockposition" ), pszYesNo[1] ) == 0 );
      char* s = pCfg->Get( szSysBar2Pipe, "cellcolor" );
      if ( s ) iDefaultBackColor = SB2_ColorA2I( s );
      s = pCfg->Get( szSysBar2Pipe, "textcolor" );
      if ( s ) iDefaultTextColor = SB2_ColorA2I( s );
      s = pCfg->Get( szNumberSection, szNumberDefault );

      LoadNumberOptions( s, &DefaultNumberOptions );
      LoadCellCfg( pCfg );

      char *p = pCfg->Get( szMiscSection, "week" );
      for ( int i = 0; p && i < iDaysOfTheWeek; i++ )
        p = SB2_ParseValue( p, szDaysOfTheWeek[i], Delimiter );
      p = pCfg->Get( szMiscSection, "month" );
      for ( i = 0; p && i < iMonths; i++ ) p = SB2_ParseValue( p, szMonths[i], Delimiter );
      for ( i = 0; i < iErrorCount; i++ )
        if ( ( p = pCfg->Get( szErrorSection, pszErrorIDs[i] ) ) && *p )
        {
          char *p2 = new char[strlen( p ) + 1];
          if ( p2 )
          {
            strcpy( p2, p );
            pszErrors[i] = p2;
          }
        }
    }
    SB2_LoadFontCfg( szDescFontSection, &fatDesc, pCfg );
    SB2_LoadFontCfg( szCellFontSection, &fatCell, pCfg );

    delete pCfg;
    if ( ! WinSetWindowPos( hwPipeFrame, HWND_TOP, x, y, 0, 0, SWP_MOVE ) )
      AbortStartup();
    else
    {
      ResizeWindow();
      WinPostMsg( hwPipeFrame, WM_COMMAND,
        ( MPARAM ) ( MNU_PIPE_BIND_OFF + iBindToCorner ), 0L );
    }
  }

  _beginthread( CheckStatus, aStack, sizeof ( aStack ), NULL );

  if ( ! iCellCount ) WinPostMsg( hwPipeClient, WM_COMMAND,
    ( MPARAM ) MNU_PIPE_PROPERTIES, 0 );

  while( WinGetMsg( hab, &qmsg, 0L, 0, 0 ) ) WinDispatchMsg( hab, &qmsg );

  if ( iIfmibSocket != -1 )
  {
    SoClose( iIfmibSocket );
    iIfmibSocket = -1;
  }
  if ( hmDosCall1 )
  {
    DosFreeModule( hmDosCall1 );
    dpsc = NULL;
    hmDosCall1 = NULLHANDLE;
  }
  if ( hmSo32DLL )
  {
    DosFreeModule( hmSo32DLL );
    hmSo32DLL = NULLHANDLE;
  }
  if ( hmTCP32DLL )
  {
    DosFreeModule( hmTCP32DLL );
    hmTCP32DLL = NULLHANDLE;
  }

  SB2_Over();

  delete pPipeDesc;
  WinDestroyWindow( hwPipeFrame );
  WinDestroyMsgQueue( hmq );
  WinTerminate( hab );

  if ( argc == 1 ) delete pszIniFile;

  return 0;
}

