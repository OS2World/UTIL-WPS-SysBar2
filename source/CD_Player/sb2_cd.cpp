/*

  SysBar/2 Utility Set  version 0.23

  CD Player module

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
#define __IBMC__

#define INCL_DOSDEVICES
#define INCL_DOSDEVIOCTL
#define INCL_DOSPROCESS
#define INCL_DOSSEMAPHORES
#define INCL_GPI
#define INCL_WIN

#include <os2.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "..\SysBar2.h"
#include "CDengine.h"
#include "SB2_CD.h"
#include "SB2_CD_res.h"


CDDrive* CurrentDrive = NULL;
CDROMDeviceMap CDMap = { 0, 0 };



// Just a dummy data structure
typedef struct
{
  short int iSize;
} DummyData;
DummyData DummyInit = { sizeof ( DummyData ) };


inline int strcmp2( char *s1, char *s2 )
{
  if ( ! s1 || ! s2 ) return -1;
  else return strcmp( s1, s2 );
}

inline int atoi2( char *s )
{
  if ( s ) return atoi( s );
  else return 0;
}


// Application handles
HEV hev;
char szSemName[] = "\\SEM32\\SB2CD_EVT";
HAB hab;
HWND hwCDClient1 = NULLHANDLE;
HWND hwCDFrame = NULLHANDLE;
HWND hmPopupMenu = NULLHANDLE;
HMODULE hmSysBar2Dll = NULLHANDLE;

FATTRS fat;


// Mode switches
short int iTopmost = 1;
short int bActive = 0;
static short int bLastVisible = 0;
short int bPlayInserted = 1;
short int bFullScan = 0;
short int bMonochrome = 0;
short int bLockPosition = 0;
short int bLarge = 0;
short int iBindToCorner = 0;
short int bAutodetectDrive = 0;
short int bLoopDisk = 0;
short int bRandomOrder = 0;

// INI-file stuff
char *pszIniFile = NULL;
char szSysBar2CD[] = "SysBar2_CD";

// Main window dimentions
int iWindowWidth[2] = { 230, 254 };
int iTimeWindowWidth[2] = { 68, 92 };
int iWindowHeight[2] = { 26, 33 };
int iStatusWindowY[2] = { 10, 14 };
int iTrackX[2] = { 50, 70 };

// Button bitmaps
const int iBBCount = 8;
const int iBBHeight = 6;
const int iBBWidth = 16;
int iButtonWidth[2] = { 32, 32 };
int iButtonHeight[2] = { 8, 12 };
HBITMAP hbButtons[iBBCount];
enum ButtonBitmaps
{
  iBBmpTime,
  iBBmpLeft,
  iBBmpPlay,
  iBBmpEject,
  iBBmpRight,
  iBBmpPause,
  iBBmpStop,
  iBBmpClose
};

// Mode bitmaps
const int iMBCount = 3;
const int iMBHeight = 5;
const int iMBWidth = 6;
HBITMAP hbModes[iMBCount][2];

const int iButtonCount = 5;

// Button class
class CDButton
{
public:
  HWND hWnd;
  HBITMAP hBitmap;
  ULONG uID;
  int x, y, cx, cy;
  CDButton( int i );
  void Draw( HPS hps, short int iRaised = 1 ) 
    { SB2_Button( hps, x, y, x + cx - 1, y + cy - 1, iRaised, hBitmap,
      iBBWidth, iBBHeight ); }
  void SizeWindow( int i );
} *CDButtons[iButtonCount];

CDButton::CDButton( int i )
{
  uID = i + ID_CDBUTTON;
  hBitmap = hbButtons[i];
  hWnd = WinCreateWindow( hwCDFrame, WC_BUTTON, NULL, 
    ( ULONG ) ( WS_VISIBLE | BS_USERBUTTON | BS_NOPOINTERFOCUS ),
    0, 0, 0, 0, hwCDFrame, HWND_TOP, uID, NULL, NULL );
  SizeWindow( i );
}

void CDButton::SizeWindow( int i )
{
  cx = iButtonWidth[bLarge] - 1;
  cy = iButtonHeight[bLarge];
  x = iTimeWindowWidth[bLarge] + ( i * iButtonWidth[bLarge] );
  y = 2;
  WinSetWindowPos( hWnd, HWND_TOP, x, y, cx, cy,
    SWP_MOVE | SWP_SIZE | SWP_SHOW );
}



// Status messages
char *pszCDMessage = NULL;
//char szCDM_TrayOpened[] = "Tray is opened";
char szCDM_NoDisk[] = "No CD in drive";
char szCDM_MediaPresent[] = "Media present";
char szCDM_NonAudio[] = "Non-audio CD inserted";
char szCDM_Playing[] = "Track ";
char szCDM_Paused[] = "Paused";
char *pszCDMessages[] =
{
  szCDM_NoDisk,
  szCDM_NoDisk,
  szCDM_MediaPresent,
  szCDM_Playing,
  szCDM_Paused
};

// Yes/no strings
char *pszYesNo[2] = { "no", "yes" };


// Track/playing info
int iTimeMode = 0;


void AbortStartup( void )
{
  PERRINFO pErrInfoBlk;
  PSZ pszOffSet;
  PSZ pszErrMsg;

  if ( ( pErrInfoBlk = WinGetErrorInfo( hab ) ) != ( PERRINFO ) NULL )
  {
    pszOffSet = ( ( PSZ ) pErrInfoBlk ) + pErrInfoBlk->offaoffszMsg;
    pszErrMsg = ( ( PSZ ) pErrInfoBlk ) + *( ( PSHORT ) pszOffSet );
    if ( ( int ) hwCDFrame && ( int ) hwCDClient1 )
      WinMessageBox( HWND_DESKTOP, hwCDFrame, ( PSZ ) pszErrMsg,
        "SysBar/2 CD Player startup error :(", 8999,
        MB_MOVEABLE | MB_ERROR | MB_CANCEL );
    WinFreeErrorInfo( pErrInfoBlk );
  }
  WinPostMsg( hwCDClient1, WM_QUIT, ( MPARAM ) NULL, ( MPARAM ) NULL );
}


// Window invalidation functions
void InvalidateTimeWindow( void )
{
  RECTL re = { 6, 5, iTimeWindowWidth[bLarge] - 5, iWindowHeight[bLarge] - 6 };
  WinInvalidateRect( hwCDClient1, &re, FALSE );
}

void InvalidateSongWindow( void )
{
  RECTL re = { iTimeWindowWidth[bLarge] + 1, 11, iWindowWidth[bLarge] - 6,
    iWindowHeight[bLarge] - 4 };
  WinInvalidateRect( hwCDClient1, &re, FALSE );
}


enum CD_Actions
{
  CDA_Nothing = 0,
  CDA_Play,
  CDA_Stop,
  CDA_Pause,
  CDA_Resume,
  CDA_Eject,
  CDA_Load,
  CDA_NextTrack,
  CDA_PrevTrack,
  CDA_SetTrack
};
CD_Actions iActionToDo = 0;
int iNewTrack = 0;

void DeliverCDAction( int iAction = 0 )
{
  iActionToDo = iAction;
  DosPostEventSem( hev );
}


void AdjustButtonBitmaps( void )
{
  CDButtons[2]->hBitmap =
    hbButtons[CurrentDrive->usStatus == CDS_Playing ? iBBmpPause : iBBmpPlay];
  CDButtons[3]->hBitmap = hbButtons[CurrentDrive->usStatus == CDS_TrayOpened ?
    iBBmpClose : CurrentDrive->usStatus >= CDS_Playing ? iBBmpStop : iBBmpEject ];
  WinInvalidateRect( hwCDFrame, NULL, TRUE );
}


void UpdateTrackMenu( short int bForceEmpty = 0 )
{
  MENUITEM mi;
  WinSendMsg( hmPopupMenu, MM_QUERYITEM,
    MPFROM2SHORT( MNU_CD_PLAYMENU, TRUE ), MPFROMP( &mi ) );
  HWND hmPlayMenu = mi.hwndSubMenu;
  int n = 0;
  if ( WinSendMsg( hmPlayMenu, MM_QUERYITEMCOUNT, NULL, NULL ) ) do
  {
    n = ( int )
      WinSendMsg( hmPlayMenu, MM_ITEMIDFROMPOSITION, MPFROMSHORT( 0 ), NULL );
  }
  while (
    WinSendMsg( hmPlayMenu, MM_DELETEITEM, MPFROM2SHORT( n, FALSE ), NULL ) );

  n = 0;
  if ( ! bForceEmpty )
  {
    char szTrackBuffer[16];
    mi.afStyle = MIS_TEXT;
    mi.afAttribute = 0;
    mi.hItem = 0;
    mi.hwndSubMenu = NULLHANDLE;
    for ( int i = 0; i < CurrentDrive->iTrackCount; i++ )
    {
      if ( CurrentDrive->Tracks[i].bPlayable )
      {
        int iTrackNumber = CurrentDrive->Tracks[i].iTrackNumber;
        sprintf( szTrackBuffer, "Track %s%i", ( iTrackNumber > 9 ? "" : "~" ),
          iTrackNumber );
        mi.iPosition = n++;
        mi.id = MNU_CD_TRACKxFIRST + i;
        WinSendMsg( hmPlayMenu, MM_INSERTITEM, MPFROMP( &mi ),
          MPFROMP( szTrackBuffer ) );
      }
    }
  }
  WinEnableMenuItem( hmPopupMenu, MNU_CD_PLAYMENU, n > 0 );
}


char aStack[8192];
short int bContinueLoop = 1;
const int iIdleTimeout = 4;
const int iNormalTimeout = 2;
const int iPlayTimeout = 1;
int iCheckCounter = 0;
int iCheckCounterTarget = iIdleTimeout;

void FAR CheckStatus( void FAR *pParam )
{
  HMQ hmq2 = WinCreateMsgQueue( hab, 0 );

  ULONG uPosts;
  while ( bContinueLoop )
  {
    if ( CurrentDrive )
    {
      short int bInvalidateButtons = 0;
      short int bInvalidateTime = 0;

      int iToDo = iActionToDo;
      iActionToDo = 0;
      switch ( iToDo )
      {
        case CDA_PrevTrack:
          CurrentDrive->PrevTrack();
          bInvalidateTime = 1;
          break;

        case CDA_NextTrack:
          CurrentDrive->NextTrack();
          bInvalidateTime = 1;
          break;

        case CDA_SetTrack:
          CurrentDrive->SkipToTrack( iNewTrack );
          bInvalidateTime = 1;
          break;

        case CDA_Play:
          CurrentDrive->Play();
          break;

        case CDA_Resume:
          CurrentDrive->Resume();
          break;

        case CDA_Pause:
          if ( ! CurrentDrive->Pause() )
          {
            bInvalidateButtons = 1;
            iCheckCounterTarget = iNormalTimeout;
          }
          break;

        case CDA_Load:
          if ( ! CurrentDrive->LoadTray() ) bInvalidateButtons = 1;
          break;

        case CDA_Eject:
          if ( ! CurrentDrive->EjectTray() )
          {
            bInvalidateButtons = 1;
            iCheckCounterTarget = iIdleTimeout;
            UpdateTrackMenu( 1 );
          }
          break;

        case CDA_Stop:
          if ( ! CurrentDrive->Stop() )
          {
            iCheckCounterTarget = iNormalTimeout;
            bInvalidateButtons = 1;
          }
          break;
      }

      int iStatusChange = CurrentDrive->CheckStatus();

      switch ( iStatusChange )
      {
        case CDS_MediaInserted:
          if ( bPlayInserted ) {
            CurrentDrive->Shuffle(bRandomOrder);
            UpdateTrackMenu();
            if ( !CurrentDrive->Play() ) iCheckCounterTarget = iPlayTimeout;
          }
          else iCheckCounterTarget =
            CurrentDrive->iTrackCount ? iNormalTimeout : iIdleTimeout;
          UpdateTrackMenu();
          break;

        case CDS_MediaRemoved:
          iCheckCounterTarget = iIdleTimeout;
          UpdateTrackMenu();
          break;

        case CDS_PlayingStarted:
          iCheckCounterTarget = iPlayTimeout;
          break;

        case CDS_PlayingStopped:
          CurrentDrive->Stop();
          if( bLoopDisk ) {
            CurrentDrive->Shuffle(bRandomOrder);
            UpdateTrackMenu();
            CurrentDrive->Play();
          }
          else iCheckCounterTarget = iNormalTimeout;
          break;
          
//        case CDS_TrackChanged:
//        case CDS_PositionChanged:
      }

      if ( iStatusChange || iToDo )
      {
        if ( iStatusChange < CDS_PositionChanged ) bInvalidateButtons = 1;
        else
        {
          bInvalidateTime = 1;
          if ( iStatusChange == CDS_TrackChanged ) InvalidateSongWindow();
        }
      }

      if ( bInvalidateButtons ) AdjustButtonBitmaps();
      if ( bInvalidateTime ) InvalidateTimeWindow();
    }
    DosResetEventSem( hev, &uPosts );
    if ( DosWaitEventSem( hev, SEM_INDEFINITE_WAIT ) ) break;
  }

  WinDestroyMsgQueue( hmq2 );
  _endthread();
}


void SaveOptions( void )
{
  FILE *f = fopen( pszIniFile, "wt" );
  if ( f )
  {
    SWP Swp;
    if ( WinQueryWindowPos( hwCDFrame, &Swp ) )
      fprintf( f, 
        "[%s]\n"
        "x=%i\n"
        "y=%i\n"
        "timemode=%i\n"
        "topmost=%i\n"
        "monochrome=%s\n"
        "largesize=%s\n"
        "cornerbind=%i\n"
        "lockposition=%s\n"
        "driveletter=%c\n"
        "autodetectdrive=%s\n"
        "autoplayinserted=%s\n"
        "randomorder=%s\n"
        "loopdisk=%s\n\n",
        szSysBar2CD, Swp.x, Swp.y, iTimeMode, iTopmost,
        pszYesNo[bMonochrome], pszYesNo[bLarge], iBindToCorner,
        pszYesNo[bLockPosition], CurrentDrive->usDrive + 'A',
        pszYesNo[bAutodetectDrive], pszYesNo[bPlayInserted], 
        pszYesNo[bRandomOrder], pszYesNo[bLoopDisk] );
    fclose( f );
  }
}


MRESULT EXPENTRY SettingsDlgProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
  switch ( msg )
  {
    case WM_INITDLG:
      WinSendDlgItemMsg( hwnd, D_Size_Small + bLarge, BM_SETCHECK,
        ( MPARAM ) 1, 0 );
      WinSendDlgItemMsg( hwnd, D_Display_None + iTopmost, BM_SETCHECK,
        ( MPARAM ) 1, 0 );
      WinSendDlgItemMsg( hwnd, D_Bind_Off + iBindToCorner, BM_SETCHECK,
        ( MPARAM ) 1, 0 );
      WinSendDlgItemMsg( hwnd, D_Display_Mono, BM_SETCHECK,
        ( MPARAM ) bMonochrome, 0 );
      WinSendDlgItemMsg( hwnd, D_Player_AutoPlay, BM_SETCHECK,
        ( MPARAM ) bPlayInserted, 0 );
      WinSendDlgItemMsg( hwnd, D_Lock_Position, BM_SETCHECK,
        ( MPARAM ) bLockPosition, 0 );
      WinSendDlgItemMsg( hwnd, D_Player_Autodetect, BM_SETCHECK,
        ( MPARAM ) bAutodetectDrive, 0 );
      WinSendDlgItemMsg( hwnd, D_Order_Continuous + bRandomOrder, BM_SETCHECK,
        ( MPARAM ) 1, 0 );
      WinSendDlgItemMsg( hwnd, D_Loop_Disc, BM_SETCHECK,
        ( MPARAM ) bLoopDisk, 0 );

      {
        char szDrive[3] = "A:";
        int n = CDDeviceFinder( CDMap );
        if ( n > 0 )
        {
          for ( int i = 0; i < n; i++ )
          {
            szDrive[0] = CDMap.usFirstLetter + i + 'A';
            WinSendDlgItemMsg( hwnd, D_Player_Drive, LM_INSERTITEM,
              ( MPARAM ) LIT_END, MPFROMP( szDrive ) );
          }
        }
        szDrive[0] = CurrentDrive->usDrive + 'A';
        WinSetDlgItemText( hwnd, D_Player_Drive, szDrive );
      }

      WinEnableControl( hwnd, D_Player_Drive, bAutodetectDrive == 0 );
      WinEnableControl( hwnd, D_Player_DriveS, bAutodetectDrive == 0 );

      break;
    
    case WM_COMMAND:
      switch ( SHORT1FROMMP( mp1 ) )
      {
        case D_Player_DriveS:
          {
            char szDrive[3];
            WinQueryDlgItemText( hwnd, D_Player_Drive, sizeof( szDrive ),
              szDrive );
            if ( CurrentDrive ) delete CurrentDrive;
            CurrentDrive = new CDDrive( szDrive[0] );
          }
          break;
      }
      break;

    case WM_CONTROL:
      if ( SHORT2FROMMP( mp1 ) == BN_CLICKED )
        switch ( SHORT1FROMMP( mp1 ) )
      {
        case D_Lock_Position:
          if ( bLockPosition != ( int ) WinSendDlgItemMsg( hwnd,
            D_Lock_Position, BM_QUERYCHECK, 0, 0 ) ) WinPostMsg( hwCDClient1,
            WM_COMMAND, ( MPARAM ) ( MNU_CD_LOCKPOSITION ), 0 );
          break;
        case D_Player_Autodetect:
          if ( bAutodetectDrive != ( int ) WinSendDlgItemMsg( hwnd,
            D_Player_Autodetect, BM_QUERYCHECK, 0, 0 ) )
          {
            WinPostMsg( hwCDClient1,
              WM_COMMAND, ( MPARAM ) ( MNU_CD_AUTODETECT ), 0 );
            WinEnableControl( hwnd, D_Player_Drive, bAutodetectDrive );
            WinEnableControl( hwnd, D_Player_DriveS, bAutodetectDrive );
          }
          break;

        case D_Order_Continuous:
        case D_Order_Random:
          {
            int s = ( int ) WinSendDlgItemMsg( hwnd, D_Order_Continuous,
              BM_QUERYCHECKINDEX, 0, 0 );
            if ( s == 0 || s == 1 ) WinPostMsg( hwCDClient1, WM_COMMAND,
              ( MPARAM ) ( MNU_CD_CONTINUOUS + s ), 0 );
          }
          break;

        case D_Loop_Disc:
          if ( bLoopDisk != ( int ) WinSendDlgItemMsg( hwnd, D_Loop_Disc, BM_QUERYCHECK, 0, 0 ) ) 
              WinPostMsg( hwCDClient1, WM_COMMAND, ( MPARAM ) ( MNU_CD_LOOPDISC ), 0 );
          break;

        case D_Size_Small:
        case D_Size_Large:
          {
            int s = ( int ) WinSendDlgItemMsg( hwnd, D_Size_Small,
              BM_QUERYCHECKINDEX, 0, 0 );
            if ( s == 0 || s == 1 ) WinPostMsg( hwCDClient1, WM_COMMAND,
              ( MPARAM ) ( MNU_CD_SMALL + s ), 0 );
          }
          break;
        case D_Display_None:
        case D_Display_OnTop:
        case D_Display_Popup:
        case D_Display_Popup2:
          {
            int s = ( int ) WinSendDlgItemMsg( hwnd, D_Display_None,
              BM_QUERYCHECKINDEX, 0, 0 );
            if ( ( s & 3 ) == s ) WinPostMsg( hwCDClient1, WM_COMMAND,
              ( MPARAM ) ( MNU_CD_NOTOP + s ), 0 );
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
            if ( s >= 0 && s <= 4 ) WinPostMsg( hwCDClient1, WM_COMMAND,
              ( MPARAM ) ( MNU_CD_BIND_OFF + s ), 0 );
          }
          break;
        case D_Display_Mono:
          if ( bMonochrome != ( int ) WinSendDlgItemMsg( hwnd, D_Display_Mono,
            BM_QUERYCHECK, 0, 0 ) )  WinPostMsg( hwCDClient1, WM_COMMAND,
            ( MPARAM ) ( MNU_CD_MONO ), 0 );
          break;
        case D_Player_AutoPlay:
          if ( bPlayInserted != ( int ) WinSendDlgItemMsg( hwnd,
            D_Player_AutoPlay, BM_QUERYCHECK, 0, 0 ) ) WinPostMsg( hwCDClient1,
            WM_COMMAND, ( MPARAM ) ( MNU_CD_PLAYINSERTED ), 0 );
          break;
      }
      break;
      
    default:
      return WinDefDlgProc( hwnd, msg, mp1, mp2 );
  }
  return ( MRESULT ) FALSE;
}


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


// Main properties dialog window procedure -----------------------------------
MRESULT EXPENTRY PropertiesDlgProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
  static ULONG ulAboutPage;
  static ULONG ulSettingsPage;
  static HWND hwSettingsDlg = NULL;
  static HWND hwAboutDlg = NULL;

  switch ( msg )
  {
    case WM_INITDLG:
      if ( mp2 )
      {
        WinSetWindowText( hwnd, "SysBar/2 CD Player - Properties" );
        WinSendDlgItemMsg( hwnd, D_Prop_Notebook, BKM_SETNOTEBOOKCOLORS,
          MPFROMLONG( SYSCLR_DIALOGBACKGROUND ),
          MPFROMLONG( BKA_BACKGROUNDPAGECOLORINDEX ) );
        HWND hwNotebook = WinWindowFromID( hwnd, D_Prop_Notebook );

        ulSettingsPage = SB2_AddDlgPage( hwnd, D_Prop_Notebook, "General",
          "General settings" );
        hwSettingsDlg = WinLoadDlg( hwNotebook, hwNotebook, SettingsDlgProc,
          ( HMODULE ) 0, DLG_SETTINGS, NULL );
        WinSendDlgItemMsg( hwnd, D_Prop_Notebook, BKM_SETPAGEWINDOWHWND,
          ( MPARAM ) ulSettingsPage, ( MPARAM ) hwSettingsDlg );

        ulAboutPage = SB2_AddDlgPage( hwnd, D_Prop_Notebook, "About",
          "About SysBar/2 CD Player" );
        hwAboutDlg = WinLoadDlg( hwNotebook, hwNotebook, AboutDlgProc,
          ( HMODULE ) 0, DLG_ABOUT, NULL );
        WinSendDlgItemMsg( hwnd, D_Prop_Notebook, BKM_SETPAGEWINDOWHWND,
          ( MPARAM ) ulAboutPage, ( MPARAM ) hwAboutDlg );

        WinSendDlgItemMsg( hwnd, D_Prop_Notebook, BKM_SETDIMENSIONS,
          MPFROM2SHORT ( 21, 21 ), MPFROMSHORT ( BKA_PAGEBUTTON ) );
        WinSendDlgItemMsg( hwnd, D_Prop_Notebook, BKM_SETDIMENSIONS,
          MPFROM2SHORT ( 80, 20 ), MPFROMSHORT ( BKA_MAJORTAB ) );
      }
      break;

    case WM_CLOSE:
      WinDestroyWindow( hwSettingsDlg );
      WinDestroyWindow( hwAboutDlg );
      WinPostMsg( hwCDClient1, WM_COMMAND, ( MPARAM ) MNU_CD_SAVE, 0 );
    default:
      return WinDefDlgProc( hwnd, msg, mp1, mp2 );
  }
  return ( MRESULT ) FALSE;
}


HWND hwPrevWindow = HWND_DESKTOP;

void PopUpMainWindow( void )
{
  if ( iTopmost >= 2 ) hwPrevWindow = WinQueryWindow( hwCDFrame, QW_PREV );
  WinSetWindowPos( hwCDFrame, HWND_TOP, 0L, 0L, 0L, 0L, SWP_ZORDER | SWP_SHOW );
  bLastVisible = 1;
}


short int bClearIt = 0;


MRESULT EXPENTRY CDWinProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
  switch ( msg )
  {
    case WM_CREATE:
      if ( bActive == 0 )
      {
        bActive = 1;

        HPS hps = WinGetPS( hwnd );
        for ( int i = 0; i < iBBCount; i++ )
          hbButtons[i] = GpiLoadBitmap( hps, NULLHANDLE, BMP_BBNULL + i, 
            iBBWidth, iBBHeight );
        for ( i = 0; i < iMBCount; i++ )
        {
          hbModes[i][0] = GpiLoadBitmap( hps, NULLHANDLE,
            BMP_MBNULL + ( i << 1 ),  iMBWidth, iMBHeight );
          hbModes[i][1] = GpiLoadBitmap( hps, NULLHANDLE,
            BMP_MBNULL + ( i << 1 ) + 1, iMBWidth, iMBHeight );
        }
        WinReleasePS( hps );

        hmPopupMenu = WinLoadMenu( hwnd, NULLHANDLE, MNU_SB2_CD );
        WinStartTimer( hab, hwnd, TID_USERMAX + 8, 500 );
      }
      break;
      
    case WM_COMMAND:
      {
        USHORT uCommand = SHORT1FROMMP( mp1 );
        switch ( uCommand )
        {
          case MNU_CD_NOTOP:
          case MNU_CD_TOPMOST:
          case MNU_CD_POPUP:
          case MNU_CD_POPUP2:
            iTopmost = uCommand - MNU_CD_NOTOP;
            break;

          case MNU_CD_BIND_OFF:
          case MNU_CD_BIND_NW:
          case MNU_CD_BIND_NE:
          case MNU_CD_BIND_SW:
          case MNU_CD_BIND_SE:
            if ( ( iBindToCorner = uCommand - MNU_CD_BIND_OFF ) > 0 )
            {
              int x = -1;
              int y = -1;
              if ( iBindToCorner == 1 || iBindToCorner == 2 )
                y = WinQuerySysValue( HWND_DESKTOP, SV_CYSCREEN ) -
                  iWindowHeight[bLarge] + 1;
              if ( iBindToCorner == 2 || iBindToCorner == 4 )
                x = WinQuerySysValue( HWND_DESKTOP, SV_CXSCREEN ) -
                  iWindowWidth[bLarge] + 1;
              WinSetWindowPos( hwCDFrame, HWND_TOP, x, y, 0, 0, SWP_MOVE );
            }
            break;

          case MNU_CD_CONTINUOUS:
          case MNU_CD_RANDOM:
            {
              bRandomOrder = (uCommand == MNU_CD_RANDOM);
              CDStatuses s = CurrentDrive->usStatus;
              if( s==CDS_Playing || s==CDS_Paused ) DeliverCDAction( CDA_Stop );
              CurrentDrive->Shuffle( bRandomOrder );
              UpdateTrackMenu();
              if( s==CDS_Playing ) DeliverCDAction( CDA_Play );
              break;
            }
          case MNU_CD_LOOPDISC:
            bLoopDisk ^= 1;
            break;

          case MNU_CD_SMALL:
          case MNU_CD_LARGE:
            bLarge = uCommand - MNU_CD_SMALL;
            fat.lMaxBaselineExt = bLarge ? 16 : 13;
            fat.lAveCharWidth = bLarge ? 6 : 5;
            {
              SWP Swp;
              WinQueryWindowPos( hwCDFrame, &Swp );
              WinInvalidateRect( hwCDFrame, NULL, TRUE );
              WinSetWindowPos( hwCDFrame, HWND_TOP, 0, 0, iWindowWidth[bLarge],
                iWindowHeight[bLarge], SWP_SIZE | SWP_SHOW );
             if ( bActive == 1 )
             {
               for ( int i = 0; i < 5; i++ ) CDButtons[i] = new CDButton( i );
               bActive = 2;
             }
             else for ( int i = 0; i < 5; i++ ) CDButtons[i]->SizeWindow( i );
            }
            if ( iBindToCorner ) WinPostMsg( hwnd, WM_COMMAND,
              ( MPARAM ) ( MNU_CD_BIND_OFF + iBindToCorner ), 0 );
            break;
            
          case MNU_CD_LOCKPOSITION:
            bLockPosition ^= 1;
            break;

          case MNU_CD_MONO:
            bMonochrome ^= 1;
            WinInvalidateRect( hwCDFrame, NULL, TRUE );
            break;
            
          case MNU_CD_PROPERTIES:
            hwPrevWindow = HWND_TOP;
            WinDlgBox( HWND_DESKTOP, hwCDFrame, PropertiesDlgProc,
              hmSysBar2Dll, DLG_PROPERTIES, ( PVOID ) &DummyInit );
            break;

          case MNU_CD_SAVE:
            SaveOptions();
            break;

          case MNU_CD_AUTODETECT:
            if ( ( bAutodetectDrive ^= 1 ) == 1 )
            {
              USHORT usDrive =
                ( CDDeviceFinder( CDMap ) ? CDMap.usFirstLetter + 'A' : 'D' );
              if ( CurrentDrive ) delete CurrentDrive;
              CurrentDrive = new CDDrive( usDrive );
            }
            break;

          case MNU_CD_PLAYINSERTED:
            bPlayInserted ^= 1;
            break;

          case MNU_CD_CLOSE:
            WinPostMsg( hwCDFrame, WM_CLOSE, 0, 0 );
            break;

          case ID_CDTIMEBTN:
            if ( CurrentDrive->iTrackCount )
            {
              iTimeMode = ( iTimeMode + 1 ) & 3;
              InvalidateTimeWindow();
            }
            break;

          case ID_CDLEFTBTN:
            DeliverCDAction( CDA_PrevTrack );
            break;

          case ID_CDRIGHTBTN:
            DeliverCDAction( CDA_NextTrack );
            break;
            
          case ID_CDPLAYBTN:
            switch ( CurrentDrive->usStatus )
            {
              case CDS_MediaPresent:
                DeliverCDAction( CDA_Play );
                break;

              case CDS_Paused:
                DeliverCDAction( CDA_Resume );
                break;

              case CDS_Playing:
                DeliverCDAction( CDA_Pause );
                break;
            }
            break;

          case ID_CDSTOPBTN:
            switch ( CurrentDrive->usStatus )
            {
              case CDS_TrayOpened:
                DeliverCDAction( CDA_Load );
                break;

              case CDS_NoMedia:
              case CDS_MediaPresent:
                DeliverCDAction( CDA_Eject );
                break;

              case CDS_Playing:
              case CDS_Paused:
                DeliverCDAction( CDA_Stop );
                break;
            }
            break;

          default:
            if ( uCommand >= MNU_CD_TRACKxFIRST &&
              uCommand <= MNU_CD_TRACKxLAST )
            {
              iNewTrack = uCommand - MNU_CD_TRACKxFIRST;
              DeliverCDAction( CDA_SetTrack );
            }
        }
      }
      break;   

    case WM_CONTROL:
      {
        USHORT uID = SHORT1FROMMP( mp1 );
        if ( uID >= ID_CDBUTTON )
        {
          USHORT uCode = SHORT2FROMMP( mp1 );
          switch ( uCode )
          {
            case BN_PAINT:
              USERBUTTON *pUB = ( USERBUTTON * ) mp2;
              WinPostMsg( hwCDFrame, WM_PAINTBUTTON,
                ( MPARAM ) ( uID - ID_CDBUTTON ),
                ( MPARAM ) ( pUB->fsState & BDS_HILITED ? 0 : 1 ) );
              break;
          }
        }
        
      }
      break;
      
    case WM_BUTTON1DOWN:
      bLastVisible = 0;
      if ( ! bLockPosition )
        WinPostMsg( hwCDFrame, WM_TRACKFRAME, ( MPARAM ) TF_MOVE, 0L );
      break;
      
    case WM_BUTTON2CLICK:
      bLastVisible = 0;
      WinPopupMenu( hwnd, hwnd, hmPopupMenu, SHORT1FROMMP( mp1 ), 
        SHORT2FROMMP( mp1 ), 0, PU_HCONSTRAIN | PU_VCONSTRAIN |
        PU_NONE | PU_KEYBOARD | PU_MOUSEBUTTON1 | PU_MOUSEBUTTON2 );
      break;
      
    case WM_TIMER:
      if ( ( USHORT ) ( mp1 ) == TID_USERMAX + 8 )
      {
        if ( ++iCheckCounter >= iCheckCounterTarget )
        {
          DosPostEventSem( hev );
          iCheckCounter = 0;
        }

        short int bOnTop = iTopmost;

        if ( iTopmost >= 2 )
        {
          SWP swp;
          WinQueryWindowPos( hwCDFrame, &swp );
          POINTL ptl = { 0, 0 };
          WinQueryPointerPos( HWND_DESKTOP, &ptl );
          ptl.x -= swp.x;
          ptl.y -= swp.y;
          if ( ptl.x >= 0 && ptl.y >= 0 &&
            ptl.x < iWindowWidth[bLarge] && ptl.y < iWindowHeight[bLarge] )
          {
            if ( ! bLastVisible ) bLastVisible = bOnTop = 1;
          }
          else if ( bLastVisible )
          {
            if ( iTopmost == 3 ) WinSetWindowPos( hwCDFrame, hwPrevWindow,
              0L, 0L, 0L, 0L, SWP_ZORDER );
            bLastVisible = 0;
          }
        }

        if ( bOnTop == 1 ) PopUpMainWindow();

      }
      break;
      
    case WM_PAINTBUTTON:
      {
        HPS hps = WinGetPS( hwnd );
        CDButtons[int ( mp1 )]->Draw( hps, ( short int ) mp2 );
        WinReleasePS( hps );
      }
      break;
      
    case WM_ERASEBACKGROUND:
      {
        bClearIt = 1;
/*      
        HPS hps = ( HPS ) mp1;
        SB2_Border( hps, 0, 0, iWindowWidth[bLarge] - 1,
          iWindowHeight[bLarge] - 1, 1, SB2c_Filler, 1 );
        SB2_Border( hps, 4, 3, iTimeWindowWidth[bLarge] - 3,
          iWindowHeight[bLarge] - 4, 0, CLR_BLACK );
        SB2_Border( hps, iTimeWindowWidth[bLarge], iStatusWindowY[bLarge],
          iWindowWidth[bLarge] - 5, iWindowHeight[bLarge] - 4, 0, CLR_BLACK );
*/          
      }
      break;
      
    case WM_PAINT:
      {
        HPS hps;
        RECTL rc;
        hps = WinBeginPaint( hwnd, 0L, &rc );

        if ( bClearIt )
        {
          bClearIt = 0;
          SB2_Border( hps, 0, 0, iWindowWidth[bLarge] - 1,
            iWindowHeight[bLarge] - 1, 1, SB2c_Filler, 1 );
          SB2_Border( hps, 4, 3, iTimeWindowWidth[bLarge] - 3,
            iWindowHeight[bLarge] - 4, 0, CLR_BLACK );
          SB2_Border( hps, iTimeWindowWidth[bLarge], iStatusWindowY[bLarge],
            iWindowWidth[bLarge] - 5, iWindowHeight[bLarge] - 4, 0, CLR_BLACK );
          for ( int i = 0; i < iButtonCount; i++ ) CDButtons[i]->Draw( hps );
        }

        if ( CurrentDrive->iTrackCount )
        {
          char szBuffer[6];
          ULONG ulResultingTime = CurrentDrive->ulCurrentPosition;

          switch ( iTimeMode )
          {
            case 0:
              ulResultingTime = CDDrive::MSFSub( ulResultingTime,
                CurrentDrive->Tracks[CurrentDrive->iCurrentTrack].ulStartingTime );
              break;
            case 1:
              ulResultingTime = CDDrive::MSFSub(
                CurrentDrive->Tracks[CurrentDrive->iCurrentTrack].ulEndingTime,
                ulResultingTime );
              break;
            case 2:
              ulResultingTime = CDDrive::MSFSub( ulResultingTime,
                CurrentDrive->Tracks[0].ulStartingTime );
              break;
            case 3:
              ulResultingTime = CDDrive::MSFSub(
                CurrentDrive->Tracks[CurrentDrive->iTrackCount - 1].ulEndingTime,
                ulResultingTime );
              break;
          }

          sprintf( szBuffer, "%02i:%02i",
            ( ulResultingTime >> 16 ) & 255, ( ulResultingTime >> 8 ) & 255 );
          SB2_LargeDigits( hps, 8, 6, szBuffer, bMonochrome, bLarge );
          
          sprintf( szBuffer, "%02i",
            CurrentDrive->Tracks[CurrentDrive->iCurrentTrack].iTrackNumber );
          SB2_SmallDigits( hps, iTrackX[bLarge], ( iWindowHeight[bLarge] >>
            1 ) + 1 + bLarge, szBuffer, bMonochrome, bLarge );
            
          POINTL pt4[4] =
          {
            { iTrackX[bLarge] + ( bLarge << 1 ) + 3, 6 + ( bLarge << 1 ) },
            { iTrackX[bLarge] + ( bLarge << 1 ) + 2 + iMBWidth,
              5 + iMBHeight + ( bLarge << 1 ) },
            { 0, 0 }, { iMBWidth, iMBHeight }
          };
          int iStatus = 0;
          GpiWCBitBlt( hps, hbModes[CurrentDrive->usStatus -
            CDS_MediaPresent][bMonochrome], 4L, pt4, ROP_SRCCOPY, BBO_IGNORE );
        }

        SB2_Border( hps, iTimeWindowWidth[bLarge], iStatusWindowY[bLarge],
          iWindowWidth[bLarge] - 5, iWindowHeight[bLarge] - 4, 0, CLR_BLACK );
        fat.usCodePage = GpiQueryCp( hps );
        GpiCreateLogFont( hps, NULL, 200L, &fat );
        GpiSetCharSet( hps, 200L );
        CHARBUNDLE cb;
        cb.lColor = bMonochrome ? CLR_WHITE : CLR_GREEN;
        cb.usTextAlign = TA_LEFT | TA_BASE;
        GpiSetAttrs( hps, PRIM_CHAR, CBB_COLOR | CBB_TEXT_ALIGN, 0, &cb );

        POINTL pt;
        pt.x = iTimeWindowWidth[bLarge] + 2;
        pt.y = iStatusWindowY[bLarge] + 3 + bLarge;
        RECTL rc1;
        rc1.xLeft = iTimeWindowWidth[bLarge] + 2;
        rc1.yBottom = iStatusWindowY[bLarge] + 1;
        rc1.xRight = iWindowWidth[bLarge] - 6;
        rc1.yTop = iWindowHeight[bLarge] - 5;
        ULONG uOptions = CHS_CLIP;
        char *pS =
          CurrentDrive->usStatus > CDS_NoMedia && ! CurrentDrive->iTrackCount ?
          szCDM_NonAudio : pszCDMessages[CurrentDrive->usStatus];
        if ( CurrentDrive->usStatus == CDS_Playing )
        {
          char szBuffer[16];
          sprintf( szBuffer, "%s %i", pS,
            CurrentDrive->Tracks[CurrentDrive->iCurrentTrack].iTrackNumber );
          pS = szBuffer;
        }
        GpiCharStringPosAt( hps, &pt, &rc1, uOptions, strlen( pS ), pS, NULL );
        GpiDeleteSetId( hps, 200L );

        WinEndPaint( hps );
      }
      break;
      
    case WM_CLOSE:
      SaveOptions();
      CurrentDrive->Pause();
      CurrentDrive->Close();
      WinPostMsg( hwnd, WM_QUIT, 0, 0 );
      break;      
      
    case WM_DESTROY:
      if ( bActive )
      {
        bContinueLoop = 0;
        bActive = 0;
        WinStopTimer( hab, hwnd, TID_USERMAX + 8 );
        for ( int i = 0; i < iBBCount; i++ )
          GpiDeleteBitmap( hbButtons[i] );
        for ( i = 0; i < iMBCount; i++ )
        {
          GpiDeleteBitmap( hbModes[i][0] );
          GpiDeleteBitmap( hbModes[i][1] );
        }
      }
      break;

    case WM_MOUSEMOVE:
      if ( iTopmost >= 2 && ! bLastVisible ) PopUpMainWindow();

    default:
      return WinDefWindowProc( hwnd, msg, mp1, mp2 );
  }
  return ( MRESULT ) FALSE;
}


int main( int argc, char *argv[] )
{
  HMQ hmq;
  QMSG qmsg;
  char szClassName[] = "SysBar2CDClass";

  fat.usRecordLength = sizeof ( FATTRS );
  fat.fsSelection = 0;
  fat.lMatch = 0;
  fat.idRegistry = 0;
  fat.usCodePage = 0;
  fat.fsType = 0;
  fat.fsFontUse = FATTR_FONTUSE_NOMIX;
  strcpy( fat.szFacename, "Helv" );

  if ( ( hab = WinInitialize( 0 ) ) == 0L ) AbortStartup();
  if ( ( hmq = WinCreateMsgQueue( hab, 0 ) ) == 0L ) AbortStartup();

  hmSysBar2Dll = SB2_Init();

  {
    TIB *tib;
    PIB *pib;
    DosGetInfoBlocks( &tib, &pib );
    char szSemNameBuffer[48];

    sprintf( szSemNameBuffer, "%s%i", szSemName, pib->pib_ulpid );

    if ( DosCreateEventSem( szSemNameBuffer, &hev, 0, 0 ) ) AbortStartup();
    _beginthread( CheckStatus, aStack, sizeof ( aStack ), NULL );
  }

  if ( ! WinRegisterClass( hab, ( PSZ ) szClassName, 
    ( PFNWP ) CDWinProc, 0L, 0 ) ) AbortStartup();

  pszIniFile = new char[strlen( argv[0] ) + 1];
  SB2_CfgFilename( pszIniFile, argv[0] );

  ULONG ulWinStyle = FCF_AUTOICON | FCF_ICON | FCF_NOBYTEALIGN | FCF_TASKLIST;
    
  if ( ( hwCDFrame = WinCreateStdWindow( HWND_DESKTOP, 0, &ulWinStyle,
    szClassName, 0, 0, NULLHANDLE, ICO_MAIN, &hwCDClient1 ) ) == 0L )
    AbortStartup();

  WinSetWindowText( hwCDFrame, "SysBar/2 CD Player" );

  srand( hwCDFrame );

  {
    int x = 0, y = 0;
    IniFile *pCfg = new IniFile( pszIniFile );

    USHORT usDrive =
      ( CDDeviceFinder( CDMap ) ? CDMap.usFirstLetter + 'A' : 'D' );

    if ( pCfg && ( *pCfg )() )
    {
      x = atoi2( pCfg->Get( szSysBar2CD, "x" ) );
      y = atoi2( pCfg->Get( szSysBar2CD, "y" ) );
      iTimeMode = atoi2( pCfg->Get( szSysBar2CD, "timemode" ) ) & 3;
      iTopmost = atoi2( pCfg->Get( szSysBar2CD, "topmost" ) ) & 3;
      bMonochrome = 
        ( strcmp2( pCfg->Get( szSysBar2CD, "monochrome" ), pszYesNo[1] ) == 0 );
      bLarge = 
        ( strcmp2( pCfg->Get( szSysBar2CD, "largesize" ), pszYesNo[1] ) == 0 );
      iBindToCorner = atoi2( pCfg->Get( szSysBar2CD, "cornerbind" ) );
      if ( iBindToCorner > 4 || iBindToCorner < 0 ) iBindToCorner = 0;
      bLockPosition = ( strcmp2(
        pCfg->Get( szSysBar2CD, "lockposition" ), pszYesNo[1] ) == 0 );
      bPlayInserted = ( strcmp2(
        pCfg->Get( szSysBar2CD, "autoplayinserted" ), pszYesNo[1] ) == 0 );
      bRandomOrder = ( strcmp2(
        pCfg->Get( szSysBar2CD, "randomorder" ), pszYesNo[1] ) == 0 );
      bLoopDisk = ( strcmp2(
        pCfg->Get( szSysBar2CD, "loopdisk" ), pszYesNo[1] ) == 0 );

      if ( ( bAutodetectDrive = ( strcmp2( pCfg->Get( szSysBar2CD,
        "autodetectdrive" ), pszYesNo[1] ) == 0 ) ) == 0 )
      {
        char *szDrive = pCfg->Get( szSysBar2CD, "driveletter" );
        if ( szDrive && *szDrive ) usDrive = *szDrive;
      }
    }
    delete pCfg;
    CurrentDrive = new CDDrive( usDrive );

    if ( ! WinSetWindowPos( hwCDFrame, HWND_TOP, x, y, 0, 0, SWP_MOVE ) )
      AbortStartup();
    else
    {
      WinPostMsg( hwCDFrame, WM_COMMAND,
        ( MPARAM ) ( MNU_CD_BIND_OFF + iBindToCorner ), 0L );
      WinPostMsg( hwCDFrame, WM_COMMAND,
        ( MPARAM ) ( MNU_CD_SMALL + bLarge ), 0L );
    }
  }

  while ( WinGetMsg( hab, &qmsg, 0L, 0, 0 ) ) WinDispatchMsg( hab, &qmsg );
  
  SB2_Over();

  WinDestroyWindow( hwCDFrame );

  DosCloseEventSem( hev );

  delete pszIniFile;

  WinDestroyMsgQueue( hmq );
  WinTerminate( hab );

  return 0;
}

