/*

  SysBar/2 Utility Set  version 0.23

  Clock module

  ..................................................................

  Copyright (c) 1995-1999  Dmitry I. Platonoff
                           All rights reserved

                         dplatonoff@canada.com

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

#define INCL_DOSDATETIME
#define INCL_GPI
#define INCL_WIN

#include <os2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "..\SysBar2.h"
#include "SB2_Clock_res.h"


// Clock dimensions
int iClockWidth[2] = { 94, 134 };
int iClockHeight[2] = { 26, 33 };
int iClockMonthX[2] = { 21, 27 };
int iClockMonthW[2] = { 14, 20 };
int iClockMonthH[2] = { 6, 9 };
int iDigitW[2] = { 8, 12 };
int iDigitH[2] = { 14, 21 };

// Clock handles
HAB hab;
HWND hwClockClient = NULLHANDLE;
HWND hwClockFrame = NULLHANDLE;
HWND hmPopupMenu = NULLHANDLE;
HMODULE hmSysBar2Dll = NULLHANDLE;
HBITMAP hbMonths[2][2];

// Clock settings
short int iTopmost = 1;
short int bMonochrome = 0;
short int bLarge = 0;
short int bLockPosition = 0;
short int b12h = 0;
short int bSeconds = 1;
short int iBindToCorner = 0;
static short int bLastVisible = 0;

inline int CurrentWidth( void )
{
  return iClockWidth[bLarge] - ( bSeconds ? 0 : ( iDigitW[bLarge] * 3 ) );
}



// Config file stuff
char *pszIniFile = NULL;
char szSysBar2Clock[] = "SysBar2_Clock";
char *pszYesNo[2] = { "no", "yes" };

// Just a dummy data structure
typedef struct
{
  short int iSize;
} DummyData;
DummyData DummyInit = { sizeof ( DummyData ) };


// Config string conversion routines -----------------------------------------
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


// Startup canceller ---------------------------------------------------------
void AbortStartup( void )
{
  PERRINFO pErrInfoBlk;
  PSZ pszOffSet;
  PSZ pszErrMsg;

  if ( ( pErrInfoBlk = WinGetErrorInfo( hab ) ) != ( PERRINFO ) NULL )
  {
    pszOffSet = ( ( PSZ ) pErrInfoBlk ) + pErrInfoBlk->offaoffszMsg;
    pszErrMsg = ( ( PSZ ) pErrInfoBlk ) + *( ( PSHORT ) pszOffSet );
    if ( ( int ) hwClockFrame && ( int ) hwClockClient )
      WinMessageBox( HWND_DESKTOP, hwClockFrame, ( PSZ ) pszErrMsg,
        "SysBar/2 Clock startup error :(", 8999, 
        MB_MOVEABLE | MB_ERROR | MB_CANCEL );
    WinFreeErrorInfo( pErrInfoBlk );
  }
  WinPostMsg( hwClockClient, WM_QUIT, ( MPARAM ) NULL, ( MPARAM ) NULL );
}


// Configuration saver -------------------------------------------------------
void SaveOptions( void )
{
  SWP Swp;
  if ( WinQueryWindowPos( hwClockFrame, &Swp ) )
  {
    FILE *f = fopen( pszIniFile, "wt" );
    if ( f )
    {
      fprintf( f, 
        "[%s]\n"
        "x=%i\n"
        "y=%i\n"
        "topmost=%i\n"
        "monochrome=%s\n"
        "largesize=%s\n"
        "12hmode=%s\n"
        "seconds=%s\n"
        "lockposition=%s\n"
        "cornerbind=%i\n",
        szSysBar2Clock, Swp.x, Swp.y, iTopmost,
        pszYesNo[bMonochrome], pszYesNo[bLarge], pszYesNo[b12h],
        pszYesNo[bSeconds], pszYesNo[bLockPosition], iBindToCorner );
      fclose( f );
    }
  }
}


// Month names
char *pszMonths[12] =
{
  "January", "February", "March", "April", "May", "June", "July", "August",
  "September", "October", "November", "December"
};

// Settings dialog window procedure ------------------------------------------
MRESULT EXPENTRY SettingsDlgProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
  switch ( msg )
  {
    case WM_INITDLG:
      {
        DATETIME dtIn;
        DosGetDateTime( &dtIn );
        WinSendDlgItemMsg( hwnd, D_Time_Hours, SPBM_SETLIMITS,
          ( MPARAM ) 23, ( MPARAM ) 0 );
        WinSendDlgItemMsg( hwnd, D_Time_Hours, SPBM_SETCURRENTVALUE,
          ( MPARAM ) dtIn.hours, 0 );
        WinSendDlgItemMsg( hwnd, D_Time_Minutes, SPBM_SETLIMITS,
          ( MPARAM ) 59, ( MPARAM ) 0 );
        WinSendDlgItemMsg( hwnd, D_Time_Minutes, SPBM_SETCURRENTVALUE,
          ( MPARAM ) dtIn.minutes, 0 );
        WinSendDlgItemMsg( hwnd, D_Time_Seconds, SPBM_SETLIMITS,
          ( MPARAM ) 59, ( MPARAM ) 0 );
        WinSendDlgItemMsg( hwnd, D_Time_Seconds, SPBM_SETCURRENTVALUE,
          ( MPARAM ) dtIn.seconds, 0 );
        WinSendDlgItemMsg( hwnd, D_Date_Day, SPBM_SETLIMITS,
          ( MPARAM ) 31, ( MPARAM ) 1 );
        WinSendDlgItemMsg( hwnd, D_Date_Day, SPBM_SETCURRENTVALUE,
          ( MPARAM ) dtIn.day, 0 );
        WinSendDlgItemMsg( hwnd, D_Date_Month, SPBM_SETARRAY,
          ( MPARAM ) pszMonths, ( MPARAM ) 12 );
        WinSendDlgItemMsg( hwnd, D_Date_Month, SPBM_SETCURRENTVALUE,
          ( MPARAM ) ( dtIn.month - 1 ), 0 );
        WinSendDlgItemMsg( hwnd, D_Display_None + iTopmost, BM_SETCHECK,
         ( MPARAM ) 1, 0 );
        WinSendDlgItemMsg( hwnd, D_Size_Small + bLarge, BM_SETCHECK,
          ( MPARAM ) 1, 0 );
        WinSendDlgItemMsg( hwnd, D_Bind_Off + iBindToCorner, BM_SETCHECK,
          ( MPARAM ) 1, 0 );
        WinSendDlgItemMsg( hwnd, D_Display_Mono, BM_SETCHECK,
          ( MPARAM ) bMonochrome, 0 );
        WinSendDlgItemMsg( hwnd, D_Display_12h, BM_SETCHECK,
          ( MPARAM ) b12h, 0 );
        WinSendDlgItemMsg( hwnd, D_Display_Seconds, BM_SETCHECK,
          ( MPARAM ) bSeconds, 0 );
        WinSendDlgItemMsg( hwnd, D_Lock_Position, BM_SETCHECK,
          ( MPARAM ) bLockPosition, 0 );
      }
      break;

    case WM_COMMAND:
      switch ( SHORT1FROMMP( mp1 ) )
      {
        case D_Time_Set:
          {
            DATETIME dtOut;
            DosGetDateTime( &dtOut );
            long lValue;
            WinSendDlgItemMsg( hwnd, D_Time_Hours, SPBM_QUERYVALUE, 
              &lValue, 0 );
            dtOut.hours = ( UCHAR ) lValue;
            WinSendDlgItemMsg( hwnd, D_Time_Minutes, SPBM_QUERYVALUE,
              &lValue, 0 );
            dtOut.minutes = ( UCHAR ) lValue;
            WinSendDlgItemMsg( hwnd, D_Time_Seconds, SPBM_QUERYVALUE,
              &lValue, 0 );
            dtOut.seconds = ( UCHAR ) lValue;
            DosSetDateTime( &dtOut );
          }
          break;
          
        case D_Date_Set:
          {
            DATETIME dtOut;
            DosGetDateTime( &dtOut );
            long lValue;
            WinSendDlgItemMsg( hwnd, D_Date_Day, SPBM_QUERYVALUE,
              &lValue, 0 );
            dtOut.day = ( UCHAR ) lValue;
            WinSendDlgItemMsg( hwnd, D_Date_Month, SPBM_QUERYVALUE,
              &lValue, 0 );
            dtOut.month = ( UCHAR ) ( lValue + 1 );
            DosSetDateTime( &dtOut );
          }
          break;

        default:
          return WinDefDlgProc( hwnd, msg, mp1, mp2 );
      }
      break;

    case WM_CONTROL:
      switch ( SHORT1FROMMP( mp1 ) )
      {
        case D_Lock_Position:
          if ( bLockPosition != ( int ) WinSendDlgItemMsg( hwnd,
            D_Lock_Position, BM_QUERYCHECK, 0, 0 ) ) WinPostMsg( hwClockClient,
            WM_COMMAND, ( MPARAM ) ( MNU_CLOCK_LOCKPOSITION ), 0 );
          break;
        case D_Display_None:
        case D_Display_OnTop:
        case D_Display_Popup:
        case D_Display_Popup2:
          {
            int s = ( int ) WinSendDlgItemMsg( hwnd, D_Display_None,
              BM_QUERYCHECKINDEX, 0, 0 );
            if ( ( s & 3 ) == s ) WinPostMsg( hwClockClient, WM_COMMAND,
              ( MPARAM ) ( MNU_CLOCK_NOTOP + s ), 0 );
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
            if ( s >= 0 && s <= 4 ) WinPostMsg( hwClockClient, WM_COMMAND,
              ( MPARAM ) ( MNU_CLOCK_BIND_OFF + s ), 0 );
          }
        case D_Size_Small:
        case D_Size_Large:
          {
            int s = ( int ) WinSendDlgItemMsg( hwnd, D_Size_Small,
              BM_QUERYCHECKINDEX, 0, 0 );
            if ( s == 0 || s == 1 ) WinPostMsg( hwClockClient, WM_COMMAND,
              ( MPARAM ) ( MNU_CLOCK_SMALL + s ), 0 );
          }
          break;
        case D_Display_Mono:
          if ( bMonochrome != ( int ) WinSendDlgItemMsg( hwnd, D_Display_Mono,
            BM_QUERYCHECK, 0, 0 ) ) WinPostMsg( hwClockClient, WM_COMMAND,
            ( MPARAM ) ( MNU_CLOCK_MONO ), 0 );
          break;
        case D_Display_12h:
          if ( b12h != ( int ) WinSendDlgItemMsg( hwnd, D_Display_12h,
            BM_QUERYCHECK, 0, 0 ) ) WinPostMsg( hwClockClient, WM_COMMAND,
            ( MPARAM ) ( MNU_CLOCK_12H ), 0 );
          break;
        case D_Display_Seconds:
          if ( bSeconds != ( int ) WinSendDlgItemMsg( hwnd, D_Display_Seconds,
            BM_QUERYCHECK, 0, 0 ) ) WinPostMsg( hwClockClient, WM_COMMAND,
            ( MPARAM ) ( MNU_CLOCK_SECONDS ), 0 );
          break;
      }
      break;

    default:
      return WinDefDlgProc( hwnd, msg, mp1, mp2 );
  }
  return ( MRESULT ) FALSE;
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


// Main properties dialog window procedure -----------------------------------
MRESULT EXPENTRY PropertiesDlgProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
  static short int bInit = 1;
  static ULONG ulAboutPage;
  static ULONG ulSettingsPage;
  static HWND hwSettingsDlg = NULL;
  static HWND hwAboutDlg = NULL;

  switch ( msg )
  {
    case WM_INITDLG:
      if ( mp2 )
      {
        WinSetWindowText( hwnd, "SysBar/2 Clock - Properties" );
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
          "About SysBar/2 Clock" );
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
      WinPostMsg( hwClockClient, WM_COMMAND, ( MPARAM ) MNU_CLOCK_SAVE, 0 );
    default:
      return WinDefDlgProc( hwnd, msg, mp1, mp2 );
  }
  return ( MRESULT ) FALSE;
}


HWND hwPrevWindow = HWND_DESKTOP;

void PopUpMainWindow( void )
{
  if ( iTopmost >= 2 ) hwPrevWindow = WinQueryWindow( hwClockFrame, QW_PREV );
  WinSetWindowPos( hwClockFrame, HWND_TOP, 0L, 0L, 0L, 0L,
    SWP_ZORDER | SWP_SHOW );
  bLastVisible = 1;
}


// Clear flag
short int bClearIt = 0;

void InvalidateInside( HWND hwnd )
{
  RECTL re = { 6, 5, 0, 0 };
  re.xRight = CurrentWidth() - 6;
  re.yTop = iClockHeight[bLarge] - 5;
  WinInvalidateRect( hwnd, &re, FALSE );
}

// Main window procedure -----------------------------------------------------
MRESULT EXPENTRY ClockWinProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
  switch ( msg )
  {
    case WM_CREATE:
      {
        HPS hps = WinGetPS( hwnd );
        hbMonths[0][0] = GpiLoadBitmap( hps, NULLHANDLE, BMP_MONTHS,
          iClockMonthW[0] * 12, iClockMonthH[0] );
        hbMonths[0][1] = GpiLoadBitmap( hps, NULLHANDLE, BMP_MONTHSBW,
          iClockMonthW[0] * 12, iClockMonthH[0] );
        hbMonths[1][0] = GpiLoadBitmap( hps, NULLHANDLE, BMP_MONTHS2,
          iClockMonthW[1] * 12, iClockMonthH[1] );
        hbMonths[1][1] = GpiLoadBitmap( hps, NULLHANDLE, BMP_MONTHS2BW,
          iClockMonthW[1] * 12, iClockMonthH[1] );
        WinReleasePS( hps );
        hmPopupMenu = WinLoadMenu( hwnd, NULLHANDLE, MNU_CLOCK );
        WinStartTimer( hab, hwnd, TID_USERMAX + 7, 1000 );
      }
      break;
      
    case WM_COMMAND:
      {
        USHORT uCommand = SHORT1FROMMP( mp1 );
        switch ( uCommand )
        {
          case MNU_CLOCK_NOTOP:
          case MNU_CLOCK_TOPMOST:
          case MNU_CLOCK_POPUP:
          case MNU_CLOCK_POPUP2:
            iTopmost = uCommand - MNU_CLOCK_NOTOP;
            break;
            
          case MNU_CLOCK_BIND_OFF:
          case MNU_CLOCK_BIND_NW:
          case MNU_CLOCK_BIND_NE:
          case MNU_CLOCK_BIND_SW:
          case MNU_CLOCK_BIND_SE:
            if ( ( iBindToCorner = uCommand - MNU_CLOCK_BIND_OFF ) > 0 )
            {
              int x = -1;
              int y = -1;
              if ( iBindToCorner == 1 || iBindToCorner == 2 )
                y = WinQuerySysValue( HWND_DESKTOP, SV_CYSCREEN ) -
                  iClockHeight[bLarge] + 1;
              if ( iBindToCorner == 2 || iBindToCorner == 4 )
                x = WinQuerySysValue( HWND_DESKTOP, SV_CXSCREEN ) -
                  CurrentWidth() + 1;
              WinSetWindowPos( hwClockFrame, HWND_TOP, x, y, 0, 0, SWP_MOVE );
            }
            break;

          case MNU_CLOCK_LOCKPOSITION:
            bLockPosition ^= 1;
            break;

          case MNU_CLOCK_MONO:
            bMonochrome ^= 1;
            InvalidateInside( hwnd );
            break;

          case MNU_CLOCK_SECONDS:
            bSeconds ^= 1;
            InvalidateInside( hwnd );
            WinPostMsg( hwnd, WM_COMMAND,
              ( MPARAM ) ( MNU_CLOCK_SMALL + bLarge ), 0 );
            break;

          case MNU_CLOCK_12H:
            b12h ^= 1;
            bClearIt = 1;
            InvalidateInside( hwnd );
            break;

          case MNU_CLOCK_PROPERTIES:
            hwPrevWindow = HWND_TOP;
            WinDlgBox( HWND_DESKTOP, hwClockFrame, PropertiesDlgProc,
              hmSysBar2Dll, DLG_PROPERTIES, ( PVOID ) &DummyInit );
            break;

          case MNU_CLOCK_SMALL:
          case MNU_CLOCK_LARGE:
            bLarge = uCommand - MNU_CLOCK_SMALL;
            {
              SWP Swp;
              WinQueryWindowPos( hwClockFrame, &Swp );
              WinInvalidateRect( hwClockFrame, NULL, TRUE );
              WinSetWindowPos( hwClockFrame, HWND_TOP, 0, 0,
                CurrentWidth(), iClockHeight[bLarge],
                SWP_SIZE | SWP_SHOW );
              if ( iBindToCorner ) WinPostMsg( hwnd, WM_COMMAND,
                ( MPARAM ) ( MNU_CLOCK_BIND_OFF + iBindToCorner ), 0 );
            }
            break;

          case MNU_CLOCK_SAVE:
            SaveOptions();
            break;
            
          case MNU_CLOCK_CLOSE:
            WinPostMsg( hwClockFrame, WM_CLOSE, 0, 0 );
            break;
        }
      }
      break;

    case WM_BUTTON1DOWN:
      bLastVisible = 0;
      if ( ! bLockPosition )
        WinPostMsg( hwClockFrame, WM_TRACKFRAME, ( MPARAM ) TF_MOVE, 0L );
      break;
      
    case WM_BUTTON2CLICK:
      bLastVisible = 0;
      WinPopupMenu( hwnd, hwnd, hmPopupMenu, SHORT1FROMMP( mp1 ), 
        SHORT2FROMMP( mp1 ), 0, PU_HCONSTRAIN | PU_VCONSTRAIN |
        PU_NONE | PU_KEYBOARD | PU_MOUSEBUTTON1 | PU_MOUSEBUTTON2 );
      break;
      
    case WM_TIMER:
      if ( ( USHORT ) ( mp1 ) == TID_USERMAX + 7 )
      {
        InvalidateInside( hwnd );

        short int bOnTop = iTopmost;

        if ( iTopmost >= 2 )
        {
          SWP swp;
          WinQueryWindowPos( hwClockFrame, &swp );
          POINTL ptl = { 0, 0 };
          WinQueryPointerPos( HWND_DESKTOP, &ptl );
          ptl.x -= swp.x;
          ptl.y -= swp.y;
          if ( ptl.x >= 0 && ptl.y >= 0 &&
            ptl.x < CurrentWidth() && ptl.y < iClockHeight[bLarge] )
          {
            if ( ! bLastVisible ) bLastVisible = bOnTop = 1;
          }
          else if ( bLastVisible )
          {
            if ( iTopmost == 3 ) WinSetWindowPos( hwClockFrame, hwPrevWindow,
              0L, 0L, 0L, 0L, SWP_ZORDER );
            bLastVisible = 0;
          }
        }

        if ( bOnTop == 1 ) PopUpMainWindow();
      }
      break;

    case WM_ERASEBACKGROUND:
      {
        bClearIt = 1;
/*        
        HPS hps = ( HPS ) mp1;
        SB2_Border( hps, 0, 0, iClockWidth[bLarge] - 1,
          iClockHeight[bLarge] - 1, 1, SB2c_Filler, 1 );
        SB2_Border( hps, 4, 3, iClockWidth[bLarge] - 5,
          iClockHeight[bLarge] - 4, 0, CLR_BLACK );
*/          
      }
      break;

    case WM_PAINT:
      {
        HPS hps;
        RECTL rc;
        hps = WinBeginPaint( hwnd, 0L, &rc );
/*  
        // DC
        DEVOPENSTRUC dop = { 0L, "DISPLAY", NULL, 0L, 0L, 0L, 0L, 0L, 0L };
        HDC hdcMem = DevOpenDC( hab, OD_MEMORY, "*", 5L,
          ( PDEVOPENDATA ) & dop, NULLHANDLE );
          
        // PS  
        SIZEL sizl = { 0, 0 };
        HPS hpsMem = GpiCreatePS( hab, hdcMem, &sizl, GPIT_MICRO |
          GPIA_ASSOC | PU_PELS );
          
        // bitmap  
        BITMAPINFOHEADER2 bmih;  
        memset( &bmih, 0, sizeof ( BITMAPINFOHEADER2 ) );
        bmih.cbFix = sizeof ( BITMAPINFOHEADER2 );
        bmih.cx = iClockWidth[bLarge];
        bmih.cy = iClockHeight[bLarge];
        bmih.cPlanes = 1;
        bmih.cBitCount = 4;
        HBITMAP hbMem = GpiCreateBitmap( hpsMem, &bmih, 0L, NULL, NULL );
        GpiSetBitmap( hpsMem, hbMem );
*/
        if ( bClearIt )
        {
          bClearIt = 0;
          SB2_Border( hps, 0, 0, CurrentWidth() - 1,
            iClockHeight[bLarge] - 1, 1, SB2c_Filler, 1 );
          SB2_Border( hps, 4, 3, CurrentWidth() - 5,
            iClockHeight[bLarge] - 4, 0, CLR_BLACK );
        }

        DATETIME dtIn;
        DosGetDateTime( &dtIn );
        char szTimeBuf[11];
        int iAMPM = 0;
        if ( b12h )
        {
          if ( dtIn.hours >= 12 )
          {
            dtIn.hours -= 12;
            iAMPM = 1;
          }
          if ( dtIn.hours == 0 ) dtIn.hours = 12;
        }
        short int bLess12 = ( dtIn.hours <= 9 );
        sprintf( szTimeBuf, "%02i:%02i:%02i%02i", short ( dtIn.hours ),
          short ( dtIn.minutes ), short ( dtIn.seconds ), short ( dtIn.day ) );
        POINTL pt4[4];
        if ( b12h )
        {
          SB2_LargeDigits( hps, 8, 6, szTimeBuf, bMonochrome, bLarge, 2 );
          pt4[0].x = iDigitW[bLarge] * 2 + 10;
          pt4[0].y = ( iClockHeight[bLarge] >> 1 ) + 1 + bLarge;
          pt4[1].x = pt4[0].x - 1 + ( iClockMonthW[bLarge] / 3 );
          pt4[1].y = pt4[0].y + iClockMonthH[bLarge] - 1;
          pt4[2].x = iClockMonthW[bLarge] * 3 +
            ( iAMPM ? ( iClockMonthW[bLarge] / 3 ) + 1 : 0 );
          pt4[2].y = 0;
          pt4[3].x = pt4[2].x + ( iClockMonthW[bLarge] / 3 );
          pt4[3].y = iClockMonthH[bLarge];
          GpiWCBitBlt( hps, hbMonths[bLarge][bMonochrome], 4L, pt4,
            ROP_SRCCOPY, BBO_IGNORE );
          SB2_LargeDigits( hps, iDigitW[bLarge] * 3 + 8, 6, szTimeBuf + 3,
            bMonochrome, bLarge, bSeconds ? 5 : 2 );
        }
        else SB2_LargeDigits( hps, 8, 6, szTimeBuf, bMonochrome, bLarge,
          bSeconds ? 8 : 5 );
        SB2_SmallDigits( hps, CurrentWidth() - iClockMonthX[bLarge] + 1,
          6, szTimeBuf + 8, bMonochrome, bLarge );
        pt4[0].x = CurrentWidth() - iClockMonthX[bLarge];
        pt4[0].y = ( iClockHeight[bLarge] >> 1 ) + 1 + bLarge;
        pt4[1].x = pt4[0].x + iClockMonthW[bLarge] - 1;
        pt4[1].y = pt4[0].y + iClockMonthH[bLarge] - 1;
        pt4[2].x = ( dtIn.month - 1 ) * iClockMonthW[bLarge];
        pt4[2].y = 0;
        pt4[3].x = pt4[2].x + iClockMonthW[bLarge];
        pt4[3].y = iClockMonthH[bLarge];
        GpiWCBitBlt( hps, hbMonths[bLarge][bMonochrome], 4L, pt4,
          ROP_SRCCOPY, BBO_IGNORE );
/*        
        pt4[0].x = 0;
        pt4[0].y = 0;
        pt4[1].x = iClockWidth[bLarge];
        pt4[1].y = iClockHeight[bLarge];
        pt4[2].x = 0;
        pt4[2].y = 0;
        pt4[3].x = iClockWidth[bLarge];
        pt4[3].y = iClockHeight[bLarge];
        GpiBitBlt( hps, hpsMem, 4L, pt4, ROP_SRCCOPY, BBO_IGNORE );
        
        GpiDeleteBitmap( hbMem );
        GpiDestroyPS( hpsMem );
        DevCloseDC( hdcMem );
*/          
        WinEndPaint( hps );
      }
      break;
      
    case WM_CLOSE:
      SaveOptions();
      WinPostMsg( hwnd, WM_QUIT, 0, 0 );
      break;
      
    case WM_DESTROY:
      WinStopTimer( hab, hwnd, TID_USERMAX + 7 );
      GpiDeleteBitmap( hbMonths[0][0] );
      GpiDeleteBitmap( hbMonths[0][1] );
      GpiDeleteBitmap( hbMonths[1][0] );
      GpiDeleteBitmap( hbMonths[1][1] );
      break;
      
    case WM_MOUSEMOVE:
      if ( iTopmost >= 2 && ! bLastVisible ) PopUpMainWindow();

    default:
      return WinDefWindowProc( hwnd, msg, mp1, mp2 );
  }
  return ( MRESULT ) FALSE;
}


// Main procedure ------------------------------------------------------------
int main( int argc, char *argv[] )
{
  HMQ hmq;
  QMSG qmsg;
  char szClassName[] = "SysBar2ClockClass";

  if ( ( hab = WinInitialize( 0 ) ) == 0L ) AbortStartup();
  if ( ( hmq = WinCreateMsgQueue( hab, 0 ) ) == 0L ) AbortStartup();

  hmSysBar2Dll = SB2_Init();

  if ( ! WinRegisterClass( hab, ( PSZ ) szClassName, 
    ( PFNWP ) ClockWinProc, 0L, 0 ) ) AbortStartup();

  pszIniFile = new char[strlen( argv[0] ) + 1];
  SB2_CfgFilename( pszIniFile,  argv[0] );
  
  ULONG ulWinStyle = FCF_AUTOICON | FCF_ICON | FCF_NOBYTEALIGN | FCF_TASKLIST;
    
  if ( ( hwClockFrame = WinCreateStdWindow( HWND_DESKTOP, 0, &ulWinStyle,
    szClassName, 0, 0, NULLHANDLE, ICO_MAIN, &hwClockClient ) ) == 0L )
    AbortStartup();

  WinSetWindowText( hwClockFrame, "SysBar/2 Clock" );

  {
    IniFile *pCfg = new IniFile( pszIniFile );
    int x = 0, y = 0;
    if ( pCfg && ( *pCfg )() )
    {
      x = atoi2( pCfg->Get( szSysBar2Clock, "x" ) );
      y = atoi2( pCfg->Get( szSysBar2Clock, "y" ) );
      iTopmost = atoi2( pCfg->Get( szSysBar2Clock, "topmost" ) ) & 3;
      bMonochrome = ( strcmp2(
        pCfg->Get( szSysBar2Clock, "monochrome" ), pszYesNo[1] ) == 0 );
      bLarge = ( strcmp2(
        pCfg->Get( szSysBar2Clock, "largesize" ), pszYesNo[1] ) == 0 );
      b12h = ( strcmp2(
        pCfg->Get( szSysBar2Clock, "12hmode" ), pszYesNo[1] ) == 0 );
      bSeconds = ( strcmp2(
        pCfg->Get( szSysBar2Clock, "seconds" ), pszYesNo[1] ) == 0 );
      bLockPosition = ( strcmp2(
        pCfg->Get( szSysBar2Clock, "lockposition" ), pszYesNo[1] ) == 0 );
      iBindToCorner = atoi2( pCfg->Get( szSysBar2Clock, "cornerbind" ) );
      if ( iBindToCorner > 4 || iBindToCorner < 0 ) iBindToCorner = 0;
    }
    delete pCfg;
    if ( ! WinSetWindowPos( hwClockFrame, HWND_TOP, x, y, 0, 0, SWP_MOVE ) ) 
      AbortStartup();
    else
    {
      WinPostMsg( hwClockFrame, WM_COMMAND,
        ( MPARAM ) ( MNU_CLOCK_BIND_OFF + iBindToCorner ), 0L );
      WinPostMsg( hwClockFrame, WM_COMMAND,
        ( MPARAM ) ( MNU_CLOCK_SMALL + bLarge ), 0L );
    }
  }

  while( WinGetMsg( hab, &qmsg, 0L, 0, 0 ) ) WinDispatchMsg( hab, &qmsg );
  
  SB2_Over();

  WinDestroyWindow( hwClockFrame );
  WinDestroyMsgQueue( hmq );
  WinTerminate( hab );

  delete pszIniFile;
  
  return 0;
}

