/*

  SysBar/2 Utility Set  version 0.23

  Task Switcher module (modified by Eugen Kuleshov)

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

#define INCL_GPI
#define INCL_WIN
#define INCL_DOSPROCESS
#define INCL_DOSRESOURCES

#include <os2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "..\SysBar2.h"
#include "SB2_Switcher_res.h"

// Main application and window handles
HAB hab;
HWND hwSwClient = NULLHANDLE;
HWND hwSwFrame = NULLHANDLE;
DescWindow *pTaskDesc;
HWND hmPopupMenu = NULLHANDLE;
HWND hmTaskMenu = NULLHANDLE;
HMODULE hmSysBar2Dll = NULLHANDLE;
HBITMAP hbQ = NULLHANDLE;
HDC hMemDC = NULLHANDLE;
HPS hMemPS = NULLHANDLE;
HBITMAP hbMem = NULLHANDLE;
HWND hwLastActive = NULLHANDLE;


ULONG ulSCValues[] =
{
  SC_RESTORE,
  SC_MOVE,
  SC_SIZE,
  SC_MINIMIZE,
  SC_MAXIMIZE,
  SC_HIDE,
  SC_SYSMENU
};

// Appearance flags and modes
short int bActive = 0;
short int iTopmost = 1;
short int iLarge = 0;
short int bSwap = 0;
short int bLockPosition = 0;
short int bSwitchlistTitles = 0;
short int bCheckPID = 1;
short int bCheckVis = 1;
short int bCheckJump = 1;
short int bAlienMenu = 0;
short int bForceRescan = 0;
short int bClearIt = 0;
short int bFixedSize = 0;
int iFixedSize = 16;
static short int bLastVisible = 0;
short int iBindToCorner = 0;
int iOrientation = 0;
int iCellColor = 0;
FATTRS fatDesc;
FONTDLG fd;
char szDescFontSection[] = "desc_font";
char szFamilyname[FACESIZE] = "";
char *pszOtherFltConfig[] =
{
  "other_filters",   // 0
  "pid",             // 1
  "visibility",      // 2
  "jump"             // 3
};
const int iHide2SwCount = 3;
char *pszHide2Sw[] =
{
  "default",   // 0
  "always",    // 1
  "never"      // 2
};


// Just a dummy data structure
typedef struct
{
  short int iSize;
} DummyData;
DummyData DummyInit = { sizeof ( DummyData ) };

// Configuration file stuff
char *pszIniFile = NULL;
char szSysBar2Sw[] = "SysBar2_Switcher";
char *pszYesNo[2] = { "no", "yes" };

// Task list structures and counters
int iTaskCount = 0;
int iRealTaskCount = 0;
SWENTRY *pTasks = NULL;
PSWBLOCK pRealTasks = NULL;
int iCurrTask = -1;

// Window size variables
int iSwCellSize[4] = { 22, 29, 34, 22 };
int iSwWHeight = 0,
  iSwWWidth = 0;
int iSwWrkLeft = 0,
  iSwWrkBottom = 0,
  iSwWrkRight = 0,
  iSwWrkTop = 0;


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
    if ( ( int ) hwSwFrame && ( int ) hwSwClient )
      WinMessageBox( HWND_DESKTOP, hwSwFrame, ( PSZ ) pszErrMsg,
        "SysBar/2 Task switcher startup error :(", 8999, 
        MB_MOVEABLE | MB_ERROR | MB_CANCEL );
    WinFreeErrorInfo( pErrInfoBlk );
  }
  WinPostMsg( hwSwClient, WM_QUIT, ( MPARAM ) NULL, ( MPARAM ) NULL );
}


//MRESULT EXPENTRY _FilterWndProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 );
void SaveOptions( void );

void SetSwitchlistVisibility( SWENTRY& swe, int bShow = 1 )
{
  swe.swctl.uchVisibility = ( bShow ? SWL_VISIBLE : SWL_INVISIBLE );
  WinChangeSwitchEntry( swe.hswitch, &( swe.swctl ) );
}

void SetTaskJumpability( SWENTRY& swe, int bJumpable = 1 )
{
  swe.swctl.fbJump = ( bJumpable ? SWL_JUMPABLE : SWL_NOTJUMPABLE );
  WinChangeSwitchEntry( swe.hswitch, &( swe.swctl ) );
}


// Filter dialog class declaration and methods -------------------------------
const int iMaxFilterString = 96;
#define WFO_HideDefault       0
#define WFO_HideAlways        1
#define WFO_HideNever         2
#define WFO_HideMask          3
#define WFO_HideInTasklist    4
#define WFO_MakeNonjumpable   8
typedef struct
{
  char szText[iMaxFilterString];
  int iTextLength;
  int iOptions;
} WinFilterEntry;
int iExceptionCount;
WinFilterEntry *pExceptions;
WinFilterEntry TempException;
char *pszExceptionSection = "hidefilter";
short int bExceptionsEnabled = 1;


int FindExceptionIndex( char *s )
{
  for ( int i = iExceptionCount - 1; i >= 0; i-- ) if ( strncmp( s,
    pExceptions[i].szText, pExceptions[i].iTextLength ) == 0 ) return i;
  return -1;
}

void SaveExceptions( FILE *f )
{
  fprintf( f,
    "[%s]\n"
    "enabled=%s\n"
    "count=%i",
    pszExceptionSection, pszYesNo[bExceptionsEnabled], iExceptionCount );
  for ( int i = 0; i < iExceptionCount; i++ ) fprintf( f, "\n%03i=%s,%i", i,
    pExceptions[i].szText, pExceptions[i].iOptions );
  fprintf( f, "\n\n" );
}

void LoadExceptions( IniFile *pCfg )
{
  bExceptionsEnabled = ( strcmp2( pCfg->Get( pszExceptionSection, "enabled" ),
    pszYesNo[1] ) == 0 );
  iExceptionCount = atoi2( pCfg->Get( pszExceptionSection, "count" ) );
  if ( iExceptionCount )
  {
    pExceptions = new WinFilterEntry[iExceptionCount];
    if ( pExceptions )
    {
      memset( pExceptions, 0, iExceptionCount * sizeof ( WinFilterEntry ) );
      char szTmp[5];
      char *pS;
      for ( int i = 0; i < iExceptionCount; i++ )
      {
        sprintf( szTmp, "%03i", i );
        if ( ( pS = pCfg->Get( pszExceptionSection, szTmp ) ) )
        {
          pS = SB2_ParseValue( pS, pExceptions[i].szText );
          SB2_ParseValue( pS, pExceptions[i].iOptions );
          pExceptions[i].iTextLength = strlen( pExceptions[i].szText );
        }
      }
    }
    else iExceptionCount = 0;
  }
}

int CheckExceptions( SWENTRY& swe, short int bChangesAllowed = 0 )
{
  if ( ! bExceptionsEnabled ) return 1;
  for ( int i = iExceptionCount - 1; i >= 0; i-- )
  {
    if ( strncmp( swe.swctl.szSwtitle,
      pExceptions[i].szText, pExceptions[i].iTextLength ) == 0 )
    {
      int iOptions = pExceptions[i].iOptions;

      if ( bChangesAllowed )
      {
        int bChangesMade = 0;
        if ( swe.swctl.uchVisibility == SWL_VISIBLE &&
          ( iOptions & WFO_HideInTasklist ) )
        {
          swe.swctl.uchVisibility = SWL_INVISIBLE;
          bChangesMade = 1;
        }
        else if ( swe.swctl.uchVisibility == SWL_INVISIBLE && !
          ( iOptions & WFO_HideInTasklist ) )
        {
          swe.swctl.uchVisibility = SWL_VISIBLE;
          bChangesMade = 1;
        }
        if ( swe.swctl.fbJump == SWL_JUMPABLE &&
          ( iOptions & WFO_MakeNonjumpable ) )
        {
          swe.swctl.fbJump = SWL_NOTJUMPABLE;
          bChangesMade = 1;
        }
        else if ( swe.swctl.fbJump == SWL_NOTJUMPABLE && !
          ( iOptions & WFO_MakeNonjumpable ) )
        {
          swe.swctl.fbJump = SWL_JUMPABLE;
          bChangesMade = 1;
        }

        if ( bChangesMade )
          WinChangeSwitchEntry( swe.hswitch, &( swe.swctl ) );
      }

      if ( iOptions & WFO_HideNever ) return 1;
      else if ( iOptions & WFO_HideAlways ) return 0;
    }
  }
  return ( ( swe.swctl.uchVisibility == SWL_VISIBLE || ! bCheckVis ) &&
    ( swe.swctl.fbJump == SWL_JUMPABLE || ! bCheckJump ) &&
    ( swe.swctl.idProcess > 0 || ! bCheckPID ) );
}

// Exception setup dialog window procedure ------------------------------------
MRESULT EXPENTRY ExceptionSetupDlgProc( HWND hwnd, ULONG msg, MPARAM mp1,
  MPARAM mp2 )
{
  switch ( msg )
  {
    case WM_INITDLG:
      {
        WinSetDlgItemText( hwnd, D_Filter_Title, TempException.szText );
        WinSendDlgItemMsg( hwnd, D_Filter_Invisible, BM_SETCHECK,
          ( MPARAM ) ( ( TempException.iOptions & WFO_HideInTasklist ) ==
          WFO_HideInTasklist ), 0 );
        WinSendDlgItemMsg( hwnd, D_Filter_NonJumpable, BM_SETCHECK,
          ( MPARAM ) ( ( TempException.iOptions & WFO_MakeNonjumpable ) ==
          WFO_MakeNonjumpable ), 0 );
        for ( int i = 0; i < iHide2SwCount; i++ )
          WinSendDlgItemMsg( hwnd, D_Filter_Hide4SwOnly, LM_INSERTITEM,
            ( MPARAM ) LIT_END, MPFROMP( pszHide2Sw[i] ) );
        WinSendDlgItemMsg( hwnd, D_Filter_Hide4SwOnly, LM_SELECTITEM,
          ( MPARAM ) ( TempException.iOptions & WFO_HideMask ), ( MPARAM ) 1 );
      }
      break;

    case WM_DESTROY:
      WinQueryDlgItemText( hwnd, D_Filter_Title,
        sizeof( TempException.szText ), TempException.szText );
      TempException.iOptions = 0;
      if ( WinQueryButtonCheckstate( hwnd, D_Filter_Invisible ) )
        TempException.iOptions |= WFO_HideInTasklist;
      if ( WinQueryButtonCheckstate( hwnd, D_Filter_NonJumpable ) )
        TempException.iOptions |= WFO_MakeNonjumpable;
      TempException.iOptions |= ( WFO_HideMask & ( int )
        WinSendDlgItemMsg( hwnd, D_Filter_Hide4SwOnly,
          LM_QUERYSELECTION, ( MPARAM ) LIT_CURSOR, ( MPARAM ) 0 ) );

    default:
      return WinDefDlgProc( hwnd, msg, mp1, mp2 );
  }
  return ( MRESULT ) FALSE;
}

void FillExceptionList( HWND hwnd, int iID )
{
  WinSendDlgItemMsg( hwnd, iID, LM_DELETEALL, ( MPARAM ) 0, ( MPARAM ) 0 );
  for ( int i = 0; i < iExceptionCount; i++ )
    WinSendDlgItemMsg( hwnd, iID, LM_INSERTITEM,
      ( MPARAM ) LIT_END, MPFROMP( pExceptions[i].szText ) );
  if ( iExceptionCount ) WinSendDlgItemMsg( hwnd, iID,
    LM_SELECTITEM, ( MPARAM ) 0, ( MPARAM ) 1 );
}

int CompareExceptions( const void *p1, const void *p2 )
{
  return strcmp( ( ( WinFilterEntry* ) p1 )->szText,
    ( ( WinFilterEntry* ) p2 )->szText );
}

int EditException( HWND hwnd, int iIndex = -1 )
{
  if ( iIndex >= 0 ) TempException = pExceptions[iIndex];

  int iReturn = ( WinDlgBox( HWND_DESKTOP, hwnd, ExceptionSetupDlgProc,
    ( HMODULE ) 0, DLG_FILTERS, ( PVOID ) &DummyInit ) == DID_OK );

  if ( iReturn )
  {
    if ( ! ( TempException.iTextLength = strlen( TempException.szText ) ) )
      return 0;
    if ( iIndex == -1 )
    {
      WinFilterEntry *pNewExceptions = new WinFilterEntry[iExceptionCount + 1];
      if ( pNewExceptions )
      {
        if ( iExceptionCount == 0 )
        {
          pNewExceptions[0] = TempException;
          pExceptions=pNewExceptions;
          iExceptionCount = 1;
          return iReturn;
        }
        memmove( pNewExceptions, pExceptions,
          iExceptionCount * sizeof( WinFilterEntry ) );
        WinFilterEntry *pOldExceptions = pExceptions;
        pExceptions = pNewExceptions;
        iIndex = iExceptionCount++;
        delete pOldExceptions;
      }
      else return 0;
    }
    pExceptions[iIndex] = TempException;
    qsort( pExceptions, iExceptionCount, sizeof( WinFilterEntry ), CompareExceptions );
    bForceRescan = 1;
  }

  return iReturn;
}

void RemoveException( int i )
{
  if ( --iExceptionCount )
  {
    if ( i < iExceptionCount ) memmove( &pExceptions[i], &pExceptions[i + 1],
      ( iExceptionCount - i ) * sizeof( WinFilterEntry ) );
  }
  else
  {
    delete pExceptions;
    pExceptions = NULL;
  }
  bForceRescan = 1;
}



// A procedure to save current configuration ---------------------------------
void SaveOptions( void )
{
  SWP Swp;
  if ( WinQueryWindowPos( hwSwFrame, &Swp ) )
  {
    FILE *f = fopen( pszIniFile, "wt" );
    if ( f )
    {
      fprintf( f,
        "[%s]\n"
        "x=%i\n"
        "y=%i\n"
        "orientation=%i\n"
        "size=%i\n"
        "customsize=%i\n"
        "cornerbind=%i\n"
        "topmost=%i\n"
        "lockposition=%s\n"
        "swapbuttons=%s\n"
        "switchlisttitles=%s\n"
        "cellcolor=%s\n"
        "fixedsize=%s\n"
        "fixedcells=%i\n\n",
        szSysBar2Sw,
        iOrientation == 2 ? Swp.x + Swp.cx - 12 : Swp.x,
        iOrientation == 1 ? Swp.y + Swp.cy - 12 : Swp.y,
        iOrientation, iLarge, iSwCellSize[3], iBindToCorner,
        iTopmost, pszYesNo[bLockPosition], pszYesNo[bSwap],
        pszYesNo[bSwitchlistTitles], SB2_ColorName( iCellColor ),
        pszYesNo[bFixedSize], iFixedSize );
      SaveExceptions( f );
      fprintf( f,
        "[%s]\n"
        "%s=%s\n"
        "%s=%s\n"
        "%s=%s\n\n",
        pszOtherFltConfig[0],
        pszOtherFltConfig[1], pszYesNo[bCheckPID],
        pszOtherFltConfig[2], pszYesNo[bCheckVis],
        pszOtherFltConfig[3], pszYesNo[bCheckJump] );
      SB2_SaveFontCfg( szDescFontSection, &fatDesc, f );
      fclose( f );
    }
  }
}


// Pop-up description window stuff and routines ------------------------------
int iDescTask = -1;

void HideDescWindow( void )
{
  if ( iDescTask != -1 )
  {
    pTaskDesc->Hide();
    iDescTask = -1;
  }
}


static char szTaskTitle[256] = { 0 };

void AdjustDescPos( int x, int y )
{
  if ( x < iSwWrkLeft || x > iSwWrkRight || y < iSwWrkBottom ||
    y > iSwWrkTop || bAlienMenu )
  {
    HideDescWindow();
    return;
  }
  int i = ( ( iOrientation & 1 ) ?
    ( y - iSwWrkBottom ) : ( x - iSwWrkLeft ) ) / iSwCellSize[iLarge];
  if ( iOrientation == 1 || iOrientation == 2 )
    i = iTaskCount - i - 1;
  if ( i >= iTaskCount )
  {
    HideDescWindow();
    return;
  }
  if ( i == iDescTask ) return;
  iDescTask = i;

  szTaskTitle[0] = 0;
  if ( ! bSwitchlistTitles ) WinQueryWindowText(
    pTasks[iDescTask].swctl.hwnd, sizeof ( szTaskTitle ), szTaskTitle );
  pTaskDesc->SetText(
    szTaskTitle[0] ? szTaskTitle : pTasks[iDescTask].swctl.szSwtitle );
  pTaskDesc->AdjustSize( &fatDesc, hwSwClient );
  SWP swp;
  WinQueryWindowPos( hwSwFrame, &swp );
  x = y = 2;
  int dx = 0, dy = 0;
  switch ( iOrientation )
  {
    case 0:
      x = 10;
      dx = iSwCellSize[iLarge];
      break;
    case 1:
      y = iSwWHeight - iSwCellSize[iLarge] - 10;
      dy = -iSwCellSize[iLarge];
      break;
    case 2:
      x = iSwWWidth - iSwCellSize[iLarge] - 10;
      dx = -iSwCellSize[iLarge];
      break;
    case 3:
      y = 10;
      dy = iSwCellSize[iLarge];
      break;
  }
  pTaskDesc->AdjustPos( x + swp.x + dx * iDescTask, y + swp.y + dy * iDescTask,
    iSwCellSize[iLarge], iSwCellSize[iLarge], iOrientation & 1 );
}

MRESULT EXPENTRY TaskDlgProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 );


MRESULT EXPENTRY SwDescWinProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
  switch ( msg )
  {
    case WM_PAINT:
      pTaskDesc->Paint( &fatDesc, hwnd );
      break;
  
    default:
      return WinDefWindowProc( hwnd, msg, mp1, mp2 );
  }
  return ( MRESULT ) FALSE;
}

int iTasksToShow = 0;

// Main window shape creator -------------------------------------------------
void ResizeSw( int _iNewCount, short int bResizeOnly = 1 )
{
  int iNewCount = bFixedSize ? iFixedSize : _iNewCount;
  HideDescWindow();
  SWP Swp;
  WinQueryWindowPos( hwSwFrame, &Swp );
  iSwWrkLeft = iSwWrkBottom = 2;
  iSwWrkRight = iSwWrkTop = iSwCellSize[iLarge] + 1;
  if ( iNewCount != iTasksToShow && ( Swp.x > 0 || Swp.y > 0 ) )
    switch ( iOrientation )
  {
    case 1:
      Swp.y -= ( iNewCount - iTasksToShow ) * iSwCellSize[iLarge];
      break;
    case 2:
      Swp.x -= ( iNewCount - iTasksToShow ) * iSwCellSize[iLarge];
      break;
  }
  if ( iOrientation & 1 )
  {
    iSwWWidth = iSwCellSize[iLarge] + 4;
    iSwWHeight = iNewCount * iSwCellSize[iLarge] + 12;
    if ( iOrientation == 3 ) iSwWrkBottom = 10;
    iSwWrkTop = iSwWrkBottom + iNewCount * iSwCellSize[iLarge] - 1;
  }
  else
  {
    iSwWHeight = iSwCellSize[iLarge] + 4;
    iSwWWidth = iNewCount * iSwCellSize[iLarge] + 12;
    if ( iOrientation == 0 ) iSwWrkLeft = 10;
    iSwWrkRight = iSwWrkLeft + iNewCount * iSwCellSize[iLarge] - 1;
  }
  iTasksToShow = iNewCount;
  if ( ! bResizeOnly ) iTaskCount = _iNewCount;
  WinSetWindowPos( hwSwFrame, HWND_TOP, Swp.x, Swp.y, iSwWWidth, iSwWHeight,
    SWP_MOVE | SWP_SIZE | SWP_SHOW );
  WinInvalidateRect( hwSwFrame, NULL, TRUE );

  WinSendMsg( hmPopupMenu, MM_DELETEITEM, MPFROM2SHORT(( USHORT) IDM_TASK, TRUE), ( MPARAM) NULL);
  HWND hwndTaskMenu = WinCreateMenu( HWND_OBJECT, NULL);
  WinSetWindowUShort( hwndTaskMenu, QWS_ID, IDM_TASK);
  MENUITEM mi;
  mi.iPosition = MIT_END;
  mi.afStyle = MIS_TEXT | MIS_BITMAP;
  mi.afAttribute = 0;
  mi.hwndSubMenu = 0;
//  mi.hItem = 0;
//  mi.id = IDM_TASK + 1;
  int iCount = iTasksToShow < iTaskCount ? iTasksToShow : iTaskCount;
  for( int i = 0; i<iCount; i++) {
    szTaskTitle[0] = 0;
    if( !bSwitchlistTitles) WinQueryWindowText( pTasks[ i].swctl.hwnd, sizeof( szTaskTitle), szTaskTitle);
    mi.id = IDM_TASK + 1 + i;
//    if( pTasks[ i].swctl.hwndIcon) {
//      mi.afStyle = MIS_TEXT | MIS_BITMAP;
//      mi.hItem = pTasks[ i].swctl.hwndIcon;
//    } else  {
      mi.afStyle = MIS_TEXT;
      mi.hItem = 0;
//    }
    WinSendMsg( hwndTaskMenu, MM_INSERTITEM, MPFROMP( &mi), szTaskTitle[ 0] ? szTaskTitle : pTasks[ i].swctl.szSwtitle);
  }

  mi.afStyle = MIS_TEXT | MIS_SUBMENU;
  mi.afAttribute = 0;
  mi.hwndSubMenu = hwndTaskMenu;
  mi.id = IDM_TASK;
  mi.hItem = 0;
  WinSendMsg( hmPopupMenu, MM_INSERTITEM, MPFROMP( &mi), "Tasks");

}



// Task list checker ---------------------------------------------------------
void QueryTasks( void )
{
  int n = WinQuerySwitchList( hab, NULL, 0 );
  int iSize = sizeof ( ULONG ) + n * ( sizeof ( SWENTRY ) );
  PSWBLOCK pBlock = ( PSWBLOCK ) new char[iSize];
  n = WinQuerySwitchList( hab, pBlock, iSize );
  if ( n != iRealTaskCount || memcmp( pBlock, pRealTasks, iSize ) || bForceRescan )
  {
    bForceRescan = 0;
    int iNewCount = 0;
    delete pRealTasks;
    pRealTasks = pBlock;
    iRealTaskCount = n;
    SWENTRY *pEntry = pRealTasks->aswentry;
    for ( int i = 0; i < n; i++ )
      if ( CheckExceptions( pEntry[i] ) ) iNewCount++;
    delete pTasks;
    pTasks = new SWENTRY[iNewCount];
    SWENTRY *pNewEntry = &pTasks[iNewCount];
    for ( i = 0; i < n; i++ ) if ( CheckExceptions( pEntry[i], 1 ) )
    {
      pNewEntry--;
      *pNewEntry = pEntry[i];
      if ( ! pNewEntry->swctl.hwndIcon ) pNewEntry->swctl.hwndIcon = ( HWND )
        WinSendMsg( pNewEntry->swctl.hwnd, WM_QUERYICON, 0L, 0L );
    }
    ResizeSw( iNewCount, 0 );
  }
  else delete pBlock;
}


// Swich to task procedure ---------------------------------------------------
void SwitchToTask( int n )
{
  SWP swp;
  hwLastActive = pTasks[n].swctl.hwnd;
  WinQueryWindowPos( pTasks[n].swctl.hwnd, &swp );
  if ( swp.fl & SWP_HIDE ) WinSetWindowPos( pTasks[n].swctl.hwnd,
    0, 0, 0, 0, 0, SWP_SHOW );
  WinSwitchToProgram( pTasks[n].hswitch );
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


void SetHexDecValue( char *pszBuf, ULONG uValue, HWND hw, ULONG uID )
{
  sprintf( pszBuf, "0x%08X (%i)", uValue, uValue );
  WinSetDlgItemText( hw, uID, pszBuf );
}
void SetHexStrValue( char *pszBuf, ULONG uValue, char *pS, HWND hw, ULONG uID )
{
  sprintf( pszBuf, "0x%08X (%s)", uValue, pS );
  WinSetDlgItemText( hw, uID, pszBuf );
}
char *pszProgTypes[] =
{
  "default",
  "fullscreen",
  "windowable VIO",
  "PM application",
  "VDM",
  "group",
  "DLL",
  "widowed VDM",
  "PDD",
  "VDD",
  "window - real mode",
  "window - prot mode",
  "window - auto",
  "3.0 std seamless VDM",
  "3.0 std seamless common",
  "3.1 std seamless VDM",
  "3.1 std seamless common",
  "3.1 enh seamless VDM",
  "3.1 enh seamless common",
  "3.1 enhanced",
  "3.1 standard"
};

// Task control dialog procedures --------------------------------------------
MRESULT EXPENTRY TaskDlgProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
  switch ( msg )
  {
    case WM_INITDLG:
      WinSendDlgItemMsg( hwnd, D_Info_Icon, SM_SETHANDLE,
        ( MPARAM ) pTasks[iCurrTask].swctl.hwndIcon, 0L );
      WinSetDlgItemText( hwnd, D_Info_Title, pTasks[iCurrTask].swctl.szSwtitle );
      {
        char szBuf[128];
        SetHexDecValue( szBuf, pTasks[iCurrTask].swctl.hwnd,
          hwnd, D_Info_HWND1 );
        SetHexDecValue( szBuf, pTasks[iCurrTask].swctl.hprog,
          hwnd, D_Info_HPROG1 );
        SetHexDecValue( szBuf, pTasks[iCurrTask].swctl.idProcess,
          hwnd, D_Info_PID1 );
        SetHexDecValue( szBuf, pTasks[iCurrTask].swctl.idSession,
          hwnd, D_Info_SID1 );
        char *pS = "";
        switch ( pTasks[iCurrTask].swctl.uchVisibility )
        {
          case SWL_VISIBLE:
            pS = "visible";
            break;
          case SWL_INVISIBLE:
            pS = "invisible";
            break;
          case SWL_GRAYED:
            pS = "grayed";
            break;
        }
        SetHexStrValue( szBuf, pTasks[iCurrTask].swctl.uchVisibility, pS,
          hwnd, D_Info_Vis1 );
        switch ( pTasks[iCurrTask].swctl.fbJump )
        {
        case SWL_JUMPABLE:
            pS = "jumpable";
            break;
          case SWL_NOTJUMPABLE:
            pS = "not jumpable";
            break;
          default:
            pS = "";
            break;
        }
        SetHexStrValue( szBuf, pTasks[iCurrTask].swctl.fbJump, pS,
          hwnd, D_Info_Jump1 );
        ULONG ul = pTasks[iCurrTask].swctl.bProgType;
        SetHexStrValue( szBuf, ul, ( ul < 20 ? pszProgTypes[ul] : "" ),
          hwnd, D_Info_PType1 );
      }
      break;

    case WM_COMMAND:
      {
        int c = int ( mp1 );
        switch ( c )
        {
          case DID_CANCEL:
            WinSendDlgItemMsg( hwnd, D_Info_Icon, SM_SETHANDLE, 0L, 0L );
            WinDismissDlg( hwnd, c );
        }
      }
      break;

    case WM_CLOSE:
      WinSendDlgItemMsg( hwnd, D_Info_Icon, SM_SETHANDLE, 0L, 0L );
    
    default:
      return WinDefDlgProc( hwnd, msg, mp1, mp2 );
  }
  return ( MRESULT ) FALSE;
}


const int iMaxAlienItems = 128;
typedef struct
{
  ULONG ulID;
  char bSysCommand;
} AlienMenuItem;
AlienMenuItem AlienItems[iMaxAlienItems];
int iAlienItemCount = 0;

void StealMenu( HWND hMenu, void *pBuf )
{

  MENUITEM miCopy = { MIT_END, MIS_SEPARATOR, 0, 0, 0, 0 };

  int iItemCount = ( int ) WinSendMsg( hMenu, MM_QUERYITEMCOUNT, 0, 0 );
  for ( int i = 0; i < iItemCount; i++ )
  {
    int iID =
      ( int ) WinSendMsg( hMenu, MM_ITEMIDFROMPOSITION, ( MPARAM ) i, 0 );
    WinSendMsg( hMenu, MM_QUERYITEM, MPFROM2SHORT( iID, TRUE ),
      ( MPARAM ) pBuf );
    PMENUITEM pMenu = ( PMENUITEM ) pBuf;
    miCopy.id = 0;
    miCopy.afAttribute = 0;

    if ( pMenu->afStyle & MIS_SEPARATOR )
    {
      if ( ! ( miCopy.afStyle & MIS_SEPARATOR ) )
      {
        miCopy.afStyle = pMenu->afStyle;
        miCopy.id = pMenu->id;
        WinSendMsg( hmTaskMenu, MM_INSERTITEM, &miCopy, NULL );
      }
    }
    else if ( pMenu->afStyle & MIS_SUBMENU )
    {
      if ( ! ( miCopy.afStyle & MIS_SEPARATOR ) )
      {
        miCopy.afStyle = MIS_SEPARATOR;
        WinSendMsg( hmTaskMenu, MM_INSERTITEM, &miCopy, NULL );
      }
      miCopy.id = pMenu->id;
      miCopy.afStyle = MIS_STATIC;
      miCopy.afAttribute = pMenu->afAttribute;
      WinSendMsg( hMenu, MM_QUERYITEMTEXT, MPFROM2SHORT( iID, 512 ),
        ( MPARAM ) pBuf );
      WinSendMsg( hmTaskMenu, MM_INSERTITEM, &miCopy, pBuf );
      StealMenu( pMenu->hwndSubMenu, pBuf );
      miCopy.afStyle = MIS_SUBMENU;
    }
    else if ( pMenu->afStyle & ( MIS_TEXT | MIS_STATIC ) )
    {
      if ( miCopy.afStyle & MIS_SUBMENU )
      {
        miCopy.afStyle = MIS_SEPARATOR;
        WinSendMsg( hmTaskMenu, MM_INSERTITEM, &miCopy, NULL );
      }
      miCopy.afStyle = pMenu->afStyle & ~MIS_SYSCOMMAND;
      miCopy.afAttribute = pMenu->afAttribute;
      if ( pMenu->afStyle & MIS_TEXT )
      {
        if ( iAlienItemCount >= iMaxAlienItems ) continue;
        AlienItems[iAlienItemCount].ulID = pMenu->id;
        AlienItems[iAlienItemCount].bSysCommand =
          ( pMenu->afStyle & MIS_SYSCOMMAND ? 1 : 0 );
        miCopy.id = MNU_SW_ALIEN0 + iAlienItemCount++;
      }
      else miCopy.id = pMenu->id;
      WinSendMsg( hMenu, MM_QUERYITEMTEXT, MPFROM2SHORT( iID, 512 ),
        ( MPARAM ) pBuf );
      WinSendMsg( hmTaskMenu, MM_INSERTITEM, &miCopy, pBuf );
    }
  }
}

void TaskControlDlg( int i, int x, int y )
{
  iCurrTask = i;

  int iItemCount = ( int ) WinSendMsg( hmTaskMenu, MM_QUERYITEMCOUNT, 0, 0 );
  for ( int j = iItemCount - 1; j > 2; j-- )
  {
    int iID = ( int ) WinSendMsg( hmTaskMenu, MM_ITEMIDFROMPOSITION,
      ( MPARAM ) j, 0 );
    WinSendMsg( hmTaskMenu, MM_REMOVEITEM, MPFROM2SHORT( iID, TRUE ), 0 );
  }

  HWND hmSysMenu = WinWindowFromID( pTasks[i].swctl.hwnd, FID_SYSMENU );
  if ( hmSysMenu )
  {
    PVOID pMem;
    APIRET rc = DosAllocSharedMem( &pMem, NULL, 512,
      PAG_COMMIT | OBJ_GIVEABLE | PAG_READ | PAG_WRITE );
    if ( ! rc )
    {
      TID tid;
      PID pid;
      WinQueryWindowProcess( pTasks[i].swctl.hwnd, &pid, &tid );
      rc = DosGiveSharedMem( pMem, pid, PAG_READ | PAG_WRITE );
      MENUITEM *mi1 = ( MENUITEM * ) pMem;
      WinSendMsg( hmSysMenu, MM_QUERYITEM,
        MPFROM2SHORT( SC_SYSMENU, TRUE ), ( MPARAM ) mi1 );
      iAlienItemCount = 0;
      if ( mi1->hwndSubMenu ) StealMenu( mi1->hwndSubMenu, pMem );
      DosFreeMem( pMem );
    }
  }
  else
  {
    MENUITEM mi = { MIT_END, MIS_TEXT, 0, MNU_SW_TSK_CLOSE, 0, 0 };
    WinSendMsg( hmTaskMenu, MM_INSERTITEM, &mi, "~Close" );
  }
  bAlienMenu = 1;
  i = FindExceptionIndex( pTasks[i].swctl.szSwtitle );
  WinEnableMenuItem( hmTaskMenu, MNU_SW_EXC_EDIT, i > -1 );
  WinEnableMenuItem( hmTaskMenu, MNU_SW_EXC_REMOVE, i > -1 );
  WinPopupMenu( hwSwFrame, hwSwFrame,
    hmTaskMenu, x, y, 0, PU_HCONSTRAIN | PU_VCONSTRAIN | PU_NONE |
    PU_KEYBOARD | PU_MOUSEBUTTON1 | PU_MOUSEBUTTON2 );
}


// More task control routines ------------------------------------------------
int iTaskToSwitch = -1;


int DetermineTask( int x, int y )
{
  int i = ( ( iOrientation & 1 ) ? ( y - iSwWrkBottom ) :
    ( x - iSwWrkLeft ) ) / iSwCellSize[iLarge];
  if ( iOrientation == 1 || iOrientation == 2 ) i = iTaskCount - i - 1;
  return i;
}

void CloseTask( int iTask )
{
  if ( pTasks[iTask].swctl.bProgType == PROG_PM )
    WinPostMsg( pTasks[iTask].swctl.hwnd, WM_CLOSE, 0L, 0L );
  else WinPostMsg( pTasks[iTask].swctl.hwnd, WM_SYSCOMMAND,
    ( MPARAM ) SC_CLOSE, MPFROM2SHORT( CMDSRC_MENU, TRUE ) );
}

/*
void GoToTask( int x, int y )
{
  int i = ( ( iOrientation & 1 ) ? ( y - iSwWrkBottom ) :
    ( x - iSwWrkLeft ) ) / iSwCellSize[iLarge];
  if ( iOrientation == 1 || iOrientation == 2 ) i = iTaskCount - i - 1;
  iTaskToSwitch = i;
  i = WinQuerySysValue( HWND_DESKTOP, SV_DBLCLKTIME );
  WinStartTimer( hab, hwSwClient, TID_USERMAX + 10, i );
}
*/

void MinimizeTask( int iTask )
{
/*
  iTaskToSwitch = -1;
  WinStopTimer( hab, hwSwClient, TID_USERMAX + 10 );
  int x = SHORT1FROMMP( mp1 ),
    y = SHORT2FROMMP( mp1 );
  if ( x >= iSwWrkLeft && x <= iSwWrkRight &&
    y >= iSwWrkBottom && y <= iSwWrkTop )
  {
    int i = ( ( iOrientation & 1 ) ? ( y - iSwWrkBottom ) :
      ( x - iSwWrkLeft ) ) / iSwCellSize[iLarge];
    if ( iOrientation == 1 || iOrientation == 2 ) i = iTaskCount - i - 1;
  */
    WinPostMsg( pTasks[iTask].swctl.hwnd, WM_SYSCOMMAND,
      ( MPARAM ) SC_MINIMIZE, MPFROM2SHORT( CMDSRC_MENU, TRUE ) );

//  }
}


static HWND hwDlgToUpdate = NULL;
void UpdateSettingsDlg( void )
{
  if ( hwDlgToUpdate )
  {
    WinSendDlgItemMsg( hwDlgToUpdate, D_Bind_Off + iBindToCorner, BM_SETCHECK,
      ( MPARAM ) 1, 0 );
    WinSendDlgItemMsg( hwDlgToUpdate, D_Grow_East + iOrientation, BM_SETCHECK,
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
          ( MPARAM ) 128, ( MPARAM ) iSwCellSize[3] );
        WinSendDlgItemMsg( hwnd, D_Size_Size, SPBM_SETLIMITS,
          ( MPARAM ) 128, ( MPARAM ) 8 );
        WinSendDlgItemMsg( hwnd, D_Display_None + iTopmost, BM_SETCHECK,
          ( MPARAM ) 1, 0 );
        WinSendDlgItemMsg( hwnd, D_Size_Small + iLarge, BM_SETCHECK,
          ( MPARAM ) 1, 0 );
        UpdateSettingsDlg();
      }
      break;

    case WM_COMMAND:
      break;

    case WM_CONTROL:
      switch ( SHORT1FROMMP( mp1 ) )
      {
        case D_Lock_Position:
          if ( bLockPosition != ( int ) WinSendDlgItemMsg( hwnd,
            D_Lock_Position, BM_QUERYCHECK, 0, 0 ) ) WinPostMsg( hwSwClient,
            WM_COMMAND, ( MPARAM ) ( MNU_SW_LOCKPOSITION ), 0 );
          break;
        case D_Grow_North:
        case D_Grow_East:
        case D_Grow_South:
        case D_Grow_West:
          {
            int s = ( int ) WinSendDlgItemMsg( hwnd, D_Grow_East,
              BM_QUERYCHECKINDEX, 0, 0 );
            if ( ( s & 3 ) == s ) WinPostMsg( hwSwClient, WM_COMMAND,
              ( MPARAM ) ( MNU_SW_ORNT_EAST + s ), 0 );
          }
          break;
        case D_Display_None:
        case D_Display_OnTop:
        case D_Display_Popup:
        case D_Display_Popup2:
          {
            int s = ( int ) WinSendDlgItemMsg( hwnd, D_Display_None,
              BM_QUERYCHECKINDEX, 0, 0 );
            if ( ( s & 3 ) == s ) WinPostMsg( hwSwClient, WM_COMMAND,
              ( MPARAM ) ( MNU_SW_NOTOP + s ), 0 );
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
            if ( s >= 0 && s <= 4 ) WinPostMsg( hwSwClient, WM_COMMAND,
              ( MPARAM ) ( MNU_SW_BIND_OFF + s ), 0 );
          }
          break;
        case D_Size_Small:
        case D_Size_Large:
        case D_Size_Fit:
        case D_Size_Custom:
          {
            int s = ( int ) WinSendDlgItemMsg( hwnd, D_Size_Small,
              BM_QUERYCHECKINDEX, 0, 0 );
            if ( ( s & 3 ) == s ) WinPostMsg( hwSwClient, WM_COMMAND,
              ( MPARAM ) ( MNU_SW_SMALL + s ), 0 );
          }
          break;
        case D_Size_Size:
          {
            long lValue;
            WinSendDlgItemMsg( hwnd, D_Size_Size, SPBM_QUERYVALUE, &lValue, 0 );
            if ( iSwCellSize[3] != lValue )
            {
              iSwCellSize[3] = lValue;
              if ( iLarge == 3 ) WinPostMsg( hwSwClient, WM_COMMAND,
                ( MPARAM ) ( MNU_SW_CUSTOM ), 0 );
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

// Display settings dialog window procedure ----------------------------------
MRESULT EXPENTRY Settings2DlgProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
  switch ( msg )
  {
    case WM_INITDLG:
      {
        hwDlgToUpdate = hwnd;
        WinSendDlgItemMsg( hwnd, D_Fixed_Size, SPBM_SETLIMITS,
          ( MPARAM ) 128, ( MPARAM ) iFixedSize );
        WinSendDlgItemMsg( hwnd, D_Fixed_Size, SPBM_SETLIMITS,
          ( MPARAM ) 128, ( MPARAM ) 1 );
        WinSendDlgItemMsg( hwnd, D_Mouse_Swap, BM_SETCHECK,
          ( MPARAM ) bSwap, 0 );
        WinSendDlgItemMsg( hwnd, D_Switchlist_Titles, BM_SETCHECK,
          ( MPARAM ) bSwitchlistTitles, 0 );
        WinSendDlgItemMsg( hwnd, D_Fixed_SizeX, BM_SETCHECK,
          ( MPARAM ) bFixedSize, 0 );
        SB2_FillColorList( hwnd, D_Cell_Color, iCellColor );
      }
      break;

    case WM_COMMAND:
      switch ( SHORT1FROMMP( mp1 ) )
      {
        case D_Font_Settings:
          {
            HWND hwFontDlg = WinFontDlg( HWND_DESKTOP, hwnd, &fd );
            if ( hwFontDlg && ( fd.lReturn == DID_OK ) ) fatDesc = fd.fAttrs;
          }
          break;
      }
      break;

    case WM_CONTROL:
      switch ( SHORT1FROMMP( mp1 ) )
      {
        case D_Mouse_Swap:
          if ( bSwap != ( int ) WinSendDlgItemMsg( hwnd, D_Mouse_Swap,
            BM_QUERYCHECK, 0, 0 ) ) WinPostMsg( hwSwClient, WM_COMMAND,
            ( MPARAM ) ( MNU_SW_SWAPBUTTONS ), 0 );
          break;
        case D_Switchlist_Titles:
          if ( bSwitchlistTitles != ( int ) WinSendDlgItemMsg( hwnd,
            D_Switchlist_Titles, BM_QUERYCHECK, 0, 0 ) ) WinPostMsg( hwSwClient,
            WM_COMMAND, ( MPARAM ) ( MNU_SW_SWITCHLISTTITLES ), 0 );
          break;
        case D_Fixed_SizeX:
          if ( bFixedSize != ( int ) WinSendDlgItemMsg( hwnd,
            D_Fixed_SizeX, BM_QUERYCHECK, 0, 0 ) ) WinPostMsg( hwSwClient,
            WM_COMMAND, ( MPARAM ) ( MNU_SW_FIXED_SIZE ), 0 );
          break;
        case D_Fixed_Size:
          {
            long lValue;
            WinSendDlgItemMsg( hwnd, D_Fixed_Size, SPBM_QUERYVALUE, &lValue, 0 );
            if ( iFixedSize != lValue )
            {
              iFixedSize = lValue;
              if ( bFixedSize ) WinPostMsg( hwSwClient, WM_COMMAND,
                ( MPARAM ) ( MNU_SW_RESIZE ), 0 );
            }
          }
          break;
        case D_Cell_Color:
          if ( SHORT2FROMMP( mp1 ) == LN_SELECT )
          {
            iCellColor = ( int ) WinSendDlgItemMsg( hwnd, D_Cell_Color,
              LM_QUERYSELECTION, ( MPARAM ) LIT_CURSOR, ( MPARAM ) 0 );
            WinInvalidateRect( hwSwFrame, NULL, TRUE );
          }
          break;
      }
      break;
    default:
      return WinDefDlgProc( hwnd, msg, mp1, mp2 );
  }
  return ( MRESULT ) FALSE;
}

void EnableListButtons( HWND hwnd )
{
  int bEnable = ( iExceptionCount > 0 );
  WinEnableControl( hwnd, D_Filter_Edit, bEnable );
  WinEnableControl( hwnd, D_Filter_Clone, bEnable );
  WinEnableControl( hwnd, D_Filter_Remove, bEnable );
}

// Other filters dialog window procedure ----------------------------------
MRESULT EXPENTRY OtherFltDlgProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
  char szTemp[iMaxFilterString];

  switch ( msg )
  {
    case WM_INITDLG:
      {
        hwDlgToUpdate = hwnd;
        WinSendDlgItemMsg( hwnd, D_Filter_Enable, BM_SETCHECK,
          ( MPARAM ) bExceptionsEnabled, 0 );
        WinSendDlgItemMsg( hwnd, D_Check_PID, BM_SETCHECK,
          ( MPARAM ) bCheckPID, 0 );
        WinSendDlgItemMsg( hwnd, D_Check_Vis, BM_SETCHECK,
          ( MPARAM ) bCheckVis, 0 );
        WinSendDlgItemMsg( hwnd, D_Check_Jump, BM_SETCHECK,
          ( MPARAM ) bCheckJump, 0 );
        FillExceptionList( hwnd, D_Filter_List );
        EnableListButtons( hwnd );
      }
      break;

    case WM_COMMAND:
      {
        int i = ( int ) WinSendDlgItemMsg( hwnd, D_Filter_List,
          LM_QUERYSELECTION, ( MPARAM ) LIT_CURSOR, ( MPARAM ) 0 );
        if ( i == LIT_NONE && SHORT1FROMMP( mp1 ) != D_Filter_New ) break;

        switch ( SHORT1FROMMP( mp1 ) )
        {
          case D_Filter_New:
          case D_Filter_Clone:
            if ( SHORT1FROMMP( mp1 ) == D_Filter_New )
              memset( &TempException, 0, sizeof ( WinFilterEntry ) );
            else TempException = pExceptions[i];
            i = -1;
          case D_Filter_Edit:
            if ( EditException( hwnd, i ) )
              FillExceptionList( hwnd, D_Filter_List );
            break;

          case D_Filter_Remove:
            RemoveException( i );
            WinSendDlgItemMsg( hwnd, D_Filter_List, LM_DELETEITEM,
              ( MPARAM ) i, ( MPARAM ) 0 );
            if ( iExceptionCount ) WinSendDlgItemMsg( hwnd, D_Filter_List,
              LM_SELECTITEM, ( MPARAM ) 0, ( MPARAM ) 1 );
            break;
        }
        EnableListButtons( hwnd );
      }
      break;

    case WM_CONTROL:
      switch ( SHORT1FROMMP( mp1 ) )
      {
        case D_Filter_Enable:
          bExceptionsEnabled = ( int ) WinSendDlgItemMsg( hwnd,
            D_Filter_Enable, BM_QUERYCHECK, 0, 0 );
          bForceRescan = 1;
          break;
        case D_Check_PID:
          if ( bCheckPID != ( int ) WinSendDlgItemMsg( hwnd, D_Check_PID,
            BM_QUERYCHECK, 0, 0 ) ) WinPostMsg( hwSwClient, WM_COMMAND,
            ( MPARAM ) ( MNU_SW_CHECK_PID ), 0 );
          bForceRescan = 1;
          break;
        case D_Check_Vis:
          if ( bCheckVis != ( int ) WinSendDlgItemMsg( hwnd, D_Check_Vis,
            BM_QUERYCHECK, 0, 0 ) ) WinPostMsg( hwSwClient, WM_COMMAND,
            ( MPARAM ) ( MNU_SW_CHECK_VIS ), 0 );
          bForceRescan = 1;
          break;
        case D_Check_Jump:
          if ( bCheckJump != ( int ) WinSendDlgItemMsg( hwnd, D_Check_Jump,
            BM_QUERYCHECK, 0, 0 ) ) WinPostMsg( hwSwClient, WM_COMMAND,
            ( MPARAM ) ( MNU_SW_CHECK_JUMP ), 0 );
          bForceRescan = 1;
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
  static short int bInit = 1;
  static ULONG ulAboutPage;
  static ULONG ulFilterPage;
  static ULONG ulSettingsPage;
  static ULONG ulSettings2Page;
  static HWND hwSettingsDlg = NULL;
  static HWND hwSettings2Dlg = NULL;
  static HWND hwFilterDlg = NULL;
  static HWND hwAboutDlg = NULL;

  switch ( msg )
  {
    case WM_INITDLG:
      if ( mp2 )
      {
        WinSetWindowText( hwnd, "SysBar/2 Task Switcher - Properties" );
        WinSendDlgItemMsg( hwnd, D_Prop_Notebook, BKM_SETNOTEBOOKCOLORS,
          MPFROMLONG( SYSCLR_DIALOGBACKGROUND ),
          MPFROMLONG( BKA_BACKGROUNDPAGECOLORINDEX ) );
        HWND hwNotebook = WinWindowFromID( hwnd, D_Prop_Notebook );

        ulSettingsPage = SB2_AddDlgPage( hwnd, D_Prop_Notebook, "Display",
          "Appearance settings (page 1 of 2)" );
        hwSettingsDlg = WinLoadDlg( hwNotebook, hwNotebook, SettingsDlgProc,
          ( HMODULE ) 0, DLG_SETTINGS, NULL );
        WinSendDlgItemMsg( hwnd, D_Prop_Notebook, BKM_SETPAGEWINDOWHWND,
          ( MPARAM ) ulSettingsPage, ( MPARAM ) hwSettingsDlg );

        ulSettings2Page = ( ULONG ) WinSendDlgItemMsg( hwnd, D_Prop_Notebook,
          BKM_INSERTPAGE, ( MPARAM ) NULL, MPFROM2SHORT( ( BKA_MINOR |
          BKA_STATUSTEXTON | BKA_AUTOPAGESIZE ), BKA_LAST ) );
        WinSendMsg( hwNotebook, BKM_SETSTATUSLINETEXT, ( MPARAM ) ulSettings2Page,
          MPFROMP ( "Other display options (page 2 of 2)" ) );
        hwSettings2Dlg = WinLoadDlg( hwNotebook, hwNotebook, Settings2DlgProc,
          ( HMODULE ) 0, DLG_SETTINGS2, NULL );
        WinSendMsg( hwNotebook, BKM_SETPAGEWINDOWHWND,
          ( MPARAM ) ulSettings2Page, ( MPARAM ) hwSettings2Dlg );

        ulFilterPage = SB2_AddDlgPage( hwnd, D_Prop_Notebook, "Filters",
          "Filters and exceptions" );
        hwFilterDlg = WinLoadDlg( hwNotebook, hwNotebook, OtherFltDlgProc,
          ( HMODULE ) 0, DLG_FLT_OTHER, NULL );
        WinSendMsg( hwNotebook, BKM_SETPAGEWINDOWHWND,
          ( MPARAM ) ulFilterPage, ( MPARAM ) hwFilterDlg );

        ulAboutPage = SB2_AddDlgPage( hwnd, D_Prop_Notebook, "About",
          "About SysBar/2 Task Switcher" );
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
      WinDestroyWindow( hwSettings2Dlg );
      WinDestroyWindow( hwFilterDlg );
      WinDestroyWindow( hwAboutDlg );
      WinPostMsg( hwSwClient, WM_COMMAND, ( MPARAM ) MNU_SW_SAVE, 0 );
    default:
      return WinDefDlgProc( hwnd, msg, mp1, mp2 );
  }
  return ( MRESULT ) FALSE;
}


HWND hwPrevWindow = HWND_DESKTOP;

void PopUpMainWindow( void )
{
  if ( iTopmost >= 2 ) hwPrevWindow = WinQueryWindow( hwSwFrame, QW_PREV );
  WinSetWindowPos( hwSwFrame, HWND_TOP, 0L, 0L, 0L, 0L, SWP_ZORDER | SWP_SHOW );
  bLastVisible = 1;
}

static ULONG ulOldMsg = 0;


// Main window procedure -----------------------------------------------------
MRESULT EXPENTRY SwWinProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
  switch ( msg )
  {
    case WM_CREATE:
      {
        HPS hps = WinGetPS( hwnd );
        hbQ = GpiLoadBitmap( hps, NULLHANDLE, BMP_Q, 20, 20 );
        WinReleasePS( hps );

        hMemDC = DevOpenDC( hab, OD_MEMORY, "*", 0, NULL, NULLHANDLE );
        SIZEL sizel = { 0, 0 };
        hMemPS = GpiCreatePS( hab, hMemDC, &sizel,
          PU_PELS | GPIF_LONG | GPIT_NORMAL | GPIA_ASSOC );
        GpiAssociate( hMemPS, hMemDC );
        BITMAPINFOHEADER2 bmih;  
        memset( &bmih, 0, sizeof ( BITMAPINFOHEADER2 ) );
        bmih.cbFix = sizeof ( BITMAPINFOHEADER2 );
        bmih.cx = bmih.cy = WinQuerySysValue( HWND_DESKTOP, SV_CXICON ) + 2;
        bmih.cPlanes = 1;
        bmih.cBitCount = 8;
        hbMem = GpiCreateBitmap( hMemPS, &bmih, 0L, NULL, NULL );
        GpiSetBitmap( hMemPS, hbMem );

        hmPopupMenu = WinLoadMenu( hwnd, NULLHANDLE, MNU_SW );
        hmTaskMenu = WinLoadMenu( hwnd, NULLHANDLE, MNU_SW_TSK_MENU );
        MENUITEM mi;
        WinSendMsg( hmTaskMenu, MM_QUERYITEM,
          MPFROM2SHORT( MNU_SW_EXC, TRUE ), MPFROMP( &mi ) );
        ULONG ulStyle = WinQueryWindowULong( mi.hwndSubMenu, QWL_STYLE );
        WinSetWindowULong( mi.hwndSubMenu, QWL_STYLE, ulStyle |
          MS_CONDITIONALCASCADE );

        WinStartTimer( hab, hwnd, TID_USERMAX + 9, 1000 );
      }
      break;

    case WM_MENUEND:
      bAlienMenu = 0;
      break;

    case WM_COMMAND:
      {
        USHORT uCommand = SHORT1FROMMP( mp1 );
        HideDescWindow();
/*
        if ( bAlienMenu )
        {
          bAlienMenu = 0;
          WinPostMsg( pTasks[iCurrTask].swctl.hwnd, WM_SYSCOMMAND,
            ( MPARAM ) uCommand, MPFROM2SHORT( CMDSRC_MENU, TRUE ) );
          iCurrTask = -1;
        }
        else
*/
        switch ( uCommand )
        {
          case MNU_SW_SAVE:
            SaveOptions();
            break;
          case MNU_SW_PROPERTIES:
            WinDlgBox( HWND_DESKTOP, hwSwFrame, PropertiesDlgProc,
              hmSysBar2Dll, DLG_PROPERTIES, ( PVOID ) &DummyInit );
            break;
          case MNU_SW_CLOSE:
            WinPostMsg( hwSwFrame, WM_CLOSE, 0, 0 );
            break;

          case MNU_SW_NOTOP:
          case MNU_SW_TOPMOST:
          case MNU_SW_POPUP:
          case MNU_SW_POPUP2:
            iTopmost = uCommand - MNU_SW_NOTOP;
            break;
          case MNU_SW_SMALL:
          case MNU_SW_LARGE:
          case MNU_SW_FITICONS:
          case MNU_SW_CUSTOM:
            iLarge = uCommand - int ( MNU_SW_SMALL );
            if ( iLarge == 2 )
              iSwCellSize[2] = WinQuerySysValue( HWND_DESKTOP, SV_CXICON ) + 4;
            ResizeSw( iTaskCount );
            if ( iBindToCorner ) WinPostMsg( hwnd, WM_COMMAND,
              ( MPARAM ) ( MNU_SW_BIND_OFF + iBindToCorner ), 0 );
            break;
          case MNU_SW_LOCKPOSITION:
            bLockPosition ^= 1;
            break;
          case MNU_SW_SWAPBUTTONS:
            bSwap ^= 1;
            break;
          case MNU_SW_FIXED_SIZE:
            bFixedSize ^= 1;
          case MNU_SW_RESIZE:
            ResizeSw( iTaskCount );
            break;
          case MNU_SW_SWITCHLISTTITLES:
            bSwitchlistTitles ^= 1;
            break;
          case MNU_SW_BIND_OFF:
          case MNU_SW_BIND_NW:
          case MNU_SW_BIND_NE:
          case MNU_SW_BIND_SW:
          case MNU_SW_BIND_SE:
            if ( ( iBindToCorner = uCommand - MNU_SW_BIND_OFF ) > 0 )
            {
              int x = -1;
              int y = -1;
              if ( iBindToCorner == 1 || iBindToCorner == 2 )
                y = WinQuerySysValue( HWND_DESKTOP, SV_CYSCREEN ) -
                  iSwWHeight + 1;
              if ( iBindToCorner == 2 || iBindToCorner == 4 )
                x = WinQuerySysValue( HWND_DESKTOP, SV_CXSCREEN ) -
                  iSwWWidth + 1;
              WinSetWindowPos( hwSwFrame, HWND_TOP, x, y, 0, 0, SWP_MOVE );

              int iNewOrientation = iOrientation;
              switch ( iBindToCorner )
              {
                case 1:
                  iNewOrientation = iOrientation & 1;
                  break;
                case 2:
                  iNewOrientation = ( iOrientation & 1 ) ? 1 : 2;
                  break;
                case 3:
                  iNewOrientation = ( iOrientation & 1 ) ? 3 : 0;
                  break;
                case 4:
                  iNewOrientation = iOrientation | 2;
                  break;
              }
              if ( iNewOrientation != iOrientation )
                WinPostMsg( hwSwClient, WM_COMMAND,
                  ( MPARAM ) ( MNU_SW_ORNT_EAST + iNewOrientation ), 0 );
            }
            break;

          case MNU_SW_ORNT_EAST:
          case MNU_SW_ORNT_SOUTH:
          case MNU_SW_ORNT_WEST:
          case MNU_SW_ORNT_NORTH:
            iOrientation = int ( uCommand ) - MNU_SW_ORIENTATION - 1;
            ResizeSw( iTaskCount );
            UpdateSettingsDlg();
            if ( iBindToCorner ) WinPostMsg( hwnd, WM_COMMAND,
              ( MPARAM ) ( MNU_SW_BIND_OFF + iBindToCorner ), 0 );
            break;

          case MNU_SW_TSK_CLOSE:
            CloseTask( iCurrTask );
            break;
          case MNU_SW_EXC_CREATE:
            memset( &TempException, 0, sizeof ( WinFilterEntry ) );
            strcpy( TempException.szText, pTasks[iCurrTask].swctl.szSwtitle );
          case MNU_SW_EXC_EDIT:
            {
              int i = -1;
              if ( uCommand == MNU_SW_EXC_EDIT )
                i = FindExceptionIndex( pTasks[iCurrTask].swctl.szSwtitle );
              EditException( hwnd, i );
              bForceRescan = 1;
            }
            break;
          case MNU_SW_EXC_REMOVE:
            {
              int i = FindExceptionIndex( pTasks[iCurrTask].swctl.szSwtitle );
              if ( i != -1 ) RemoveException( i );
            }
            break;
//          case MNU_SW_TSK_HIDE:
//            HideFilter.AddString( pTasks[iCurrTask].swctl.szSwtitle );
//            break;
//          case MNU_SW_TSK_SHOW:
//            ShowFilter.AddString( pTasks[iCurrTask].swctl.szSwtitle );
//            break;
          case MNU_SW_TSK_INFO:
            WinDlgBox( HWND_DESKTOP, hwSwFrame, TaskDlgProc,
              NULLHANDLE, DLG_TASK, NULL );
            break;

          case MNU_SW_CHECK_PID:
            bCheckPID ^= 1;
            break;
          case MNU_SW_CHECK_VIS:
            bCheckVis ^= 1;
            break;
          case MNU_SW_CHECK_JUMP:
            bCheckJump ^= 1;
            break;

          default:
            if ( uCommand >= MNU_SW_ALIEN0 && uCommand <= MNU_SW_ALIEN1 &&
              iCurrTask >= 0 ) WinPostMsg( pTasks[iCurrTask].swctl.hwnd,
              AlienItems[uCommand - MNU_SW_ALIEN0].bSysCommand ? WM_SYSCOMMAND :
              WM_COMMAND, ( MPARAM ) AlienItems[uCommand - MNU_SW_ALIEN0].ulID,
              MPFROM2SHORT( CMDSRC_MENU, TRUE ) );
            break;
        }
        iCurrTask = -1;
      }
      break;

    case WM_BUTTON1DOWN:
      {
        HideDescWindow();
        int x = SHORT1FROMMP( mp1 ),
          y = SHORT2FROMMP( mp1 );
        if ( ( x < iSwWrkLeft || x > iSwWrkRight ||
          y < iSwWrkBottom || y > iSwWrkTop || bAlienMenu ) && ! bLockPosition )
          WinPostMsg( hwSwFrame, WM_TRACKFRAME, ( MPARAM ) TF_MOVE, 0L );
        else return WinDefWindowProc( hwnd, msg, mp1, mp2 );
      }
      break;

    case WM_BUTTON1UP:
    case WM_BUTTON2UP:
      if ( ulOldMsg == msg ) ulOldMsg = 0;
      else
      {
        HideDescWindow();
        short int bWhich = ( msg == WM_BUTTON2UP );
        int x = SHORT1FROMMP( mp1 ),
          y = SHORT2FROMMP( mp1 );
        if ( x >= iSwWrkLeft && x <= iSwWrkRight &&
          y >= iSwWrkBottom && y <= iSwWrkTop && ! bAlienMenu )
        {
          LONG lState = 0x8000 &
            WinGetKeyState( HWND_DESKTOP, bWhich ? VK_BUTTON1 : VK_BUTTON2 );
          if ( lState ) ulOldMsg = bWhich ? WM_BUTTON1UP : WM_BUTTON2UP;
          int iTask = DetermineTask( x, y );
          if ( bSwap ^ bWhich )
          {
            if ( lState ) MinimizeTask( iTask );
            else TaskControlDlg( iTask, x, y );
          }
          else
          {
            if ( lState ) CloseTask( iTask );
            else if ( hwLastActive == pTasks[iTask].swctl.hwnd )
            {
              MinimizeTask( iTask );
              hwLastActive = NULLHANDLE;
            }
            else SwitchToTask( iTask );
          }
        }
        else if ( bWhich ) WinPopupMenu( hwnd, hwnd, hmPopupMenu, x, y, 0,
          PU_HCONSTRAIN | PU_VCONSTRAIN | PU_NONE |
          PU_KEYBOARD | PU_MOUSEBUTTON1 | PU_MOUSEBUTTON2 );
      }
      break;
/*
    case WM_BUTTON2CLICK:
      {
        HideDescWindow();
        int x = SHORT1FROMMP( mp1 ),
          y = SHORT2FROMMP( mp1 );
        if ( x >= iSwWrkLeft && x <= iSwWrkRight &&
          y >= iSwWrkBottom && y <= iSwWrkTop )
        {
          if ( bSwap ) GoToTask( x, y );
          else TaskControlDlg( x, y );
        }
        else WinPopupMenu( hwnd, hwnd, hmPopupMenu, x, y, 0,
          PU_HCONSTRAIN | PU_VCONSTRAIN | PU_NONE |
          PU_KEYBOARD | PU_MOUSEBUTTON1 | PU_MOUSEBUTTON2 );
      }
      break;
*/
/*
    case WM_BUTTON2DBLCLK:
      if ( bSwap ) MinimizeTask( mp1 );
      break;

    case WM_BUTTON1DBLCLK:
      if ( ! bSwap ) MinimizeTask( mp1 );
      break;
*/

    case WM_TIMER:
      switch ( SHORT1FROMMP( mp1 ) )
      {
        case TID_USERMAX + 9:
          if ( ! bAlienMenu )
          {
            QueryTasks();
            HWND hwActive = WinQueryActiveWindow( HWND_DESKTOP );
            if ( hwActive != hwSwFrame && hwActive != hwSwClient )
              hwLastActive = hwActive;
          }
          
          {
            SWP swp;
            WinQueryWindowPos( hwSwFrame, &swp );
            POINTL ptl = { 0, 0 };
            WinQueryPointerPos( HWND_DESKTOP, &ptl );
            ptl.x -= swp.x;
            ptl.y -= swp.y;
            AdjustDescPos( ptl.x, ptl.y );

            short int bOnTop = iTopmost;

            if ( iTopmost >= 2 )
            {
              SWP swp;
              WinQueryWindowPos( hwSwFrame, &swp );
              POINTL ptl = { 0, 0 };
              WinQueryPointerPos( HWND_DESKTOP, &ptl );
              ptl.x -= swp.x;
              ptl.y -= swp.y;
              if ( ptl.x >= 0 && ptl.y >= 0 &&
                ptl.x < iSwWWidth && ptl.y < iSwWHeight )
              {
                if ( ! bLastVisible ) bLastVisible = bOnTop = 1;
              }
              else if ( bLastVisible )
              {
                if ( iTopmost == 3 ) WinSetWindowPos( hwSwFrame, hwPrevWindow,
                  0L, 0L, 0L, 0L, SWP_ZORDER );
                bLastVisible = 0;
              }
            }
    
            if ( bOnTop == 1 ) PopUpMainWindow();
          }
          break;
/*
        case TID_USERMAX + 10:
          WinStopTimer( hab, hwnd, TID_USERMAX + 10 );
          if ( iTaskToSwitch != -1 )
          {
            SwitchToTask( iTaskToSwitch );
            iTaskToSwitch = -1;
          }
          break;
*/
      }
      break;

    case WM_ERASEBACKGROUND:
      {
        bClearIt = 1;
/*
        HPS hps = ( HPS ) ( mp1 );
        if ( iTaskCount )
        {
          SB2_Border( hps, 0, 0, iSwWWidth - 1, iSwWHeight - 1, 1, 
            SB2c_Filler, 1 );
          int x = 2, y = 2, dx = 0, dy = 0;
          switch ( iOrientation )
          {
            case 0:
              x = 10;
              dx = iSwCellSize[iLarge];
              break;
            case 1:
              y = iSwWHeight - iSwCellSize[iLarge] - 10;
              dy = -iSwCellSize[iLarge];
              break;
            case 2:
              x = iSwWWidth - iSwCellSize[iLarge] - 10;
              dx = -iSwCellSize[iLarge];
              break;
            case 3:
              y = 10;
              dy = iSwCellSize[iLarge];
              break;
          }
          for ( int i = 0; i < iTaskCount; i++ )
          {
            SB2_Border( hps, x, y, x + iSwCellSize[iLarge] - 1,
              y + iSwCellSize[iLarge] - 1, 0, CLR_BLACK );
            x += dx;
            y += dy;
          }
        }
*/
      }
      break;

    case WM_PAINT:
      {
        HPS hps;
        RECTL rc;
        hps = WinBeginPaint( hwnd, 0L, &rc );

        int iColor = SB2_ColorValue( iCellColor );
        int iCount = iTasksToShow < iTaskCount ? iTasksToShow : iTaskCount;
        if ( bClearIt )
        {
          bClearIt = 0;
          SB2_Border( hps, 0, 0, iSwWWidth - 1, iSwWHeight - 1, 1,
            SB2c_Filler, 1 );
          int x = 2, y = 2, dx = 0, dy = 0;
          switch ( iOrientation )
          {
            case 0:
              x = 10;
              dx = iSwCellSize[iLarge];
              break;
            case 1:
              y = iSwWHeight - iSwCellSize[iLarge] - 10;
              dy = -iSwCellSize[iLarge];
              break;
            case 2:
              x = iSwWWidth - iSwCellSize[iLarge] - 10;
              dx = -iSwCellSize[iLarge];
              break;
            case 3:
              y = 10;
              dy = iSwCellSize[iLarge];
              break;
          }
          for ( int i = 0; i < iCount; i++ )
          {
            SB2_Border( hps, x, y, x + iSwCellSize[iLarge] - 1,
              y + iSwCellSize[iLarge] - 1, 0, iColor );
            x += dx;
            y += dy;
          }
        }

        int x = 3, y = 3, dx = 0, dy = 0;
        switch ( iOrientation )
        {
          case 0:
            x = 11;
            dx = iSwCellSize[iLarge];
            break;
          case 1:
            y = iSwWHeight - iSwCellSize[iLarge] - 9;
            dy = -iSwCellSize[iLarge];
            break;
          case 2:
            x = iSwWWidth - iSwCellSize[iLarge] - 9;
            dx = -iSwCellSize[iLarge];
            break;
          case 3:
            y = 11;
            dy = iSwCellSize[iLarge];
            break;
        }
        for ( int i = 0; i < iCount; i++ )
        {
          POINTL pt4[4];
          pt4[0].x = x + 1;
          pt4[0].y = y + 1;
          pt4[1].x = x + iSwCellSize[iLarge] - 3;
          pt4[1].y = y + iSwCellSize[iLarge] - 3;

          int iSize = WinQuerySysValue( HWND_DESKTOP, SV_CXICON );
          RECTL re = { 0, 0, iSize + 2, iSize + 2 };
          WinFillRect( hMemPS, &re, iColor );
          if ( pTasks[i].swctl.hwndIcon )
          {
            ULONG ulOption = DP_NORMAL;
            if ( ( iSwCellSize[iLarge] * 5 ) < ( iSize * 4 ) )
            {
              pt4[0].x++;
              pt4[0].y++;
              pt4[1].x--;
              pt4[1].y--;
              iSize >>= 1;
              ulOption = 0x0004;
            }
            WinDrawPointer( hMemPS, 0, 0, pTasks[i].swctl.hwndIcon, ulOption );
          }
          else
          {
            if ( iSize > iSwCellSize[iLarge] - 4 ) iSize = iSwCellSize[iLarge] - 4;
            int iOffset = ( iSize - 20 ) >> 1;
            POINTL p4[4] = { { iOffset, iOffset },
              { iOffset + 19, iOffset + 19 }, { 0, 0 }, { 20, 20 } };
            GpiWCBitBlt( hMemPS, hbQ, 4L, p4, ROP_SRCPAINT, BBO_IGNORE );
          }
          pt4[2].x = 0;
          pt4[2].y = 0;
          pt4[3].x = iSize;
          pt4[3].y = iSize;
          GpiBitBlt( hps, hMemPS, 4L, pt4, ROP_SRCCOPY, BBO_IGNORE );
          x += dx;
          y += dy;
        }
        WinEndPaint( hps );
      }
      break;

    case WM_CLOSE:
      HideDescWindow();
      SaveOptions();
      WinPostMsg( hwnd, WM_QUIT, 0, 0 );
      break;

    case WM_DESTROY:
      WinStopTimer( hab, hwnd, TID_USERMAX + 9 );
      delete pTasks;
      delete pRealTasks;
      GpiDeleteBitmap( hbQ );
      GpiDeleteBitmap( hbMem );
      GpiDestroyPS( hMemPS );
      DevCloseDC( hMemDC );
      break;

    case WM_MOUSEMOVE:
      if ( iTopmost >= 2 && ! bLastVisible ) PopUpMainWindow();
      AdjustDescPos( SHORT1FROMMP( mp1 ), SHORT2FROMMP( mp1 ) );
    
    default:
      return WinDefWindowProc( hwnd, msg, mp1, mp2 );
  }
  return ( MRESULT ) FALSE;
}


// main() :) -----------------------------------------------------------------
int main( int argc, char *argv[] )
{
  HMQ hmq;
  QMSG qmsg;
  char szClassName[] = "SysBar2SwClass";
  char szDescClassName[] = "SysBar2SwDescClass";

  memset( &fd, 0, sizeof ( FONTDLG ) );
  fd.cbSize = sizeof ( FONTDLG );
  fd.pszFamilyname = szFamilyname;
  fd.usFamilyBufLen = sizeof ( szFamilyname );
  fd.fl = FNTS_BITMAPONLY | FNTS_CENTER | FNTS_INITFROMFATTRS;
  fd.clrFore = CLR_WHITE;
  fd.clrBack = CLR_DARKGRAY;

  if ( ( hab = WinInitialize( 0 ) ) == 0L ) AbortStartup();
  if ( ( hmq = WinCreateMsgQueue( hab, 0 ) ) == 0L ) AbortStartup();

  hmSysBar2Dll = SB2_Init();
  iCellColor = SB2_ColorCount() - 1;

  if ( ! WinRegisterClass( hab, ( PSZ ) szClassName, 
    ( PFNWP ) SwWinProc, 0L, 0 ) ) AbortStartup();
  if ( ! WinRegisterClass( hab, ( PSZ ) szDescClassName,
    ( PFNWP ) SwDescWinProc, CS_SIZEREDRAW, 0 ) ) AbortStartup();

  if ( argc > 1 ) pszIniFile = argv[1];
  else
  {
    pszIniFile = new char[strlen( argv[0] ) + 1];
    SB2_CfgFilename( pszIniFile, argv[0] );
  }

  ULONG ulWinStyle = FCF_AUTOICON | FCF_ICON | FCF_NOBYTEALIGN | FCF_TASKLIST;

  if ( ( hwSwFrame = WinCreateStdWindow( HWND_DESKTOP, 0, &ulWinStyle,
    szClassName, 0, 0, NULLHANDLE, ICO_MAIN, &hwSwClient ) ) == 0L )
    AbortStartup();

  if ( ! ( pTaskDesc = new DescWindow ) ) AbortStartup();
  else if ( ! pTaskDesc->CreateWindow( szDescClassName ) ) AbortStartup();

  WinSetWindowText( hwSwFrame, "SysBar/2 Task Switcher" );

  iSwCellSize[2] = WinQuerySysValue( HWND_DESKTOP, SV_CXICON ) + 4;

  {
    IniFile *pCfg = new IniFile( pszIniFile );
    int x = 0, y = 0, s = 0;
    if ( pCfg && ( *pCfg )() )
    {
      x = atoi2( pCfg->Get( szSysBar2Sw, "x" ) );
      y = atoi2( pCfg->Get( szSysBar2Sw, "y" ) );
      if ( ( s = atoi2( pCfg->Get( szSysBar2Sw, "customsize" ) ) ) > 0 )
        iSwCellSize[3] = s;
      iLarge = atoi2( pCfg->Get( szSysBar2Sw, "size" ) ) & 3;
      iBindToCorner = atoi2( pCfg->Get( szSysBar2Sw, "cornerbind" ) );
      if ( iBindToCorner > 4 || iBindToCorner < 0 ) iBindToCorner = 0;
      iOrientation = atoi2( pCfg->Get( szSysBar2Sw, "orientation" ) ) & 3;
      iTopmost = atoi2( pCfg->Get( szSysBar2Sw, "topmost" ) ) & 3;
      bLockPosition = ( strcmp2(
        pCfg->Get( szSysBar2Sw, "lockposition" ), pszYesNo[1] ) == 0 );
      bSwap = ( strcmp2(
        pCfg->Get( szSysBar2Sw, "swapbuttons" ), pszYesNo[1] ) == 0 );
      bSwitchlistTitles = ( strcmp2(
        pCfg->Get( szSysBar2Sw, "switchlisttitles" ), pszYesNo[1] ) == 0 );
      bFixedSize = ( strcmp2(
        pCfg->Get( szSysBar2Sw, "fixedsize" ), pszYesNo[1] ) == 0 );
      iFixedSize = atoi2( pCfg->Get( szSysBar2Sw, "fixedcells" ) ) & 255;
      char* p = pCfg->Get( szSysBar2Sw, "cellcolor" );
      if ( p ) iCellColor = SB2_ColorA2I( p );
      LoadExceptions( pCfg );
      bCheckPID = ( strcmp2( pCfg->Get( pszOtherFltConfig[0],
        pszOtherFltConfig[1] ), pszYesNo[1], 0 ) == 0 );
      bCheckVis = ( strcmp2( pCfg->Get( pszOtherFltConfig[0],
        pszOtherFltConfig[2] ), pszYesNo[1], 0 ) == 0 );
      bCheckJump = ( strcmp2( pCfg->Get( pszOtherFltConfig[0],
        pszOtherFltConfig[3] ), pszYesNo[1], 0 ) == 0 );
    }
    SB2_LoadFontCfg( szDescFontSection, &fatDesc, pCfg );
    delete pCfg;
    fd.fAttrs = fatDesc;
    if ( ! WinSetWindowPos( hwSwFrame, HWND_TOP, x, y, 0, 0, SWP_MOVE ) )
      AbortStartup();
    else
    {
      QueryTasks();
      WinPostMsg( hwSwFrame, WM_COMMAND,
        ( MPARAM ) ( MNU_SW_BIND_OFF + iBindToCorner ), 0L );
    }
  }

  while( WinGetMsg( hab, &qmsg, 0L, 0, 0 ) ) WinDispatchMsg( hab, &qmsg );

  SB2_Over();

  delete pTaskDesc;
  WinDestroyWindow( hwSwFrame );
  WinDestroyMsgQueue( hmq );
  WinTerminate( hab );

  if ( argc == 1 ) delete pszIniFile;

  return 0;
}

