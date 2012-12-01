/*

  SysBar/2 Utility Set  version 0.23

  SYSBAR2.DLL main module

  ..................................................................

  Copyright (c) 1995-1999  Dmitry I. Platonoff
                           All rights reserved

                         dip@platonoff.com

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

#define INCL_DOSMODULEMGR
#define INCL_GPI
#define INCL_WIN

#include <os2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "SysBar2_.h"
#include "SysBar2_res.h"



// Structure to describe a bitmap with digit pictures
struct DigitBitmap
{
  HBITMAP hBitmap[2];  //bitmap handle
  int iWidth;          //single digit width
  int iXStep;          //horisontal size of digit to display
  int iHeight;         //single digit height
};
static struct DigitBitmap bLargeDigits[2] =
  { { { 0, 0 }, 8, 8, 14 }, { { 0, 0 }, 12, 12, 21 } };
static struct DigitBitmap bSmallDigits[2] =
  { { { 0, 0 }, 5, 7, 6 }, { { 0, 0 }, 7, 10, 9 } };


static short int iSysBar2Modules = 0;  // SysBar2.DLL call counter

static HMODULE hmSysBar2 = NULLHANDLE;  // SysBar2.DLL module handle

// Copyright strings
static char *szSysBar2[3] =
{
  "SysBar/2 Utility Set  version 0.23",
  "(c) 1995,1999 by Dmitry I. Platonoff <dip@platonoff.com>",
  "(c) 2002,2004 by Max A. Alekseyev <relf@os2.ru>"
};

char* __export SB2_Strings( int iIndex )
{
  return szSysBar2[iIndex];
}


// Resource loader
void SB2_LoadResources( void )
{
  int iID = BMP_LARGEDIGITS;
  HPS hps = WinGetPS( HWND_DESKTOP );
  for ( int i = 0; i < 2; i++ ) for ( int j = 0; j < 2; j++ )
  {
    bLargeDigits[i].hBitmap[j] = GpiLoadBitmap( hps, hmSysBar2, iID,
      bLargeDigits[i].iWidth * 11, bLargeDigits[i].iHeight );
    bSmallDigits[i].hBitmap[j] = GpiLoadBitmap( hps, hmSysBar2, iID++ + 4,
      bSmallDigits[i].iWidth * 10, bSmallDigits[i].iHeight );
  }
  WinReleasePS( hps );
}

// Initialzation/registration procedure
HMODULE __export SB2_Init( void )
{
  if ( iSysBar2Modules++ == 0 )
  {
    DosQueryModuleHandle( "SYSBAR2.DLL", ( PHMODULE ) & hmSysBar2 );
    
    SB2_LoadResources();
  }

  return hmSysBar2;
}

// Resource dumper
void SB2_FreeResources( void )
{
  for ( int i = 0; i < 2; i++ ) for ( int j = 0; j < 2; j++ )
  {
    GpiDeleteBitmap( bLargeDigits[i].hBitmap[j] );
    GpiDeleteBitmap( bSmallDigits[i].hBitmap[j] );
  }
}

// Unregistration/shutdown procedure
void __export SB2_Over( void )
{
  if ( --iSysBar2Modules == 0 )
    SB2_FreeResources();
}

// Config filename creator
char* __export SB2_CfgFilename( char *pszCfg, char *pszExe )
{
  if ( pszCfg )
  {
    int l = strlen( pszExe );
    strcpy( pszCfg, pszExe );
    pszCfg[l - 3] = 'C';
    pszCfg[l - 2] = 'f';
    pszCfg[l - 1] = 'g';
  }
  return pszCfg;
}

// Procedure to display some digits passed as the character string
void SB2_DisplayDigits( struct DigitBitmap bDigits, HPS hps, int x, int y,
  char *pszString, short int bMonochrome, int iCount )
{
  if ( ! iCount ) iCount = strlen( pszString );

  POINTL pt4[4];
  for ( short i = 0; i < iCount; i++ )
  {
    pt4[0].x = i * bDigits.iXStep + x;
    pt4[0].y = y;
    pt4[1].x = pt4[0].x + bDigits.iWidth - 1;
    pt4[1].y = pt4[0].y + bDigits.iHeight - 1;
    pt4[2].x = ( pszString[i] - '0' ) * bDigits.iWidth;
    pt4[2].y = 0;
    pt4[3].x = pt4[2].x + bDigits.iWidth;
    pt4[3].y = bDigits.iHeight;
    GpiWCBitBlt( hps, bDigits.hBitmap[bMonochrome], 4L, pt4,
      ROP_SRCCOPY, BBO_IGNORE );
  }
}

// Procedure to display some large (LCD-style) digits passed as the 
//  character string
void __export SB2_LargeDigits( HPS hps, int x, int y, char *pszString, 
  short int bMonochrome, short int bLarge, int iCount )
{
  SB2_DisplayDigits( bLargeDigits[bLarge], hps, x, y, pszString, bMonochrome,
    iCount );
}

// Procedure to display some small digits passed as the character string
void __export SB2_SmallDigits( HPS hps, int x, int y, char *pszString,
  short int bMonochrome, short int bLarge, int iCount )
{
  SB2_DisplayDigits( bSmallDigits[bLarge], hps, x, y, pszString, bMonochrome,
    iCount );
}

void __export SB2_Border( HPS hps, int iLeft, int iBottom, int iRight,
  int iTop, short int iRaised, int iFiller, int bOutlined )
{
  POINTL pt;
  if ( bOutlined )
  {
    GpiSetColor( hps, CLR_BLACK );
    pt.x = iLeft;
    pt.y = iBottom;
    GpiMove( hps, &pt );
    pt.x = iRight;
    pt.y = iTop;
    GpiBox( hps, DRO_OUTLINE, &pt, 0, 0 );
  }
  if ( iFiller )
  {
    GpiSetColor( hps, iFiller );
    pt.x = iLeft + bOutlined;
    pt.y = iBottom + bOutlined;
    GpiMove( hps, &pt );
    pt.x = iRight - bOutlined;
    pt.y = iTop - bOutlined;
    GpiBox( hps, DRO_FILL, &pt, 0, 0 );
  }
  GpiSetColor( hps, iRaised == 1 ? SB2c_Brighter : SB2c_Darker );
  pt.x = iLeft + bOutlined;
  pt.y = iBottom + bOutlined;
  GpiMove( hps, &pt );
  pt.y = iTop - bOutlined;
  GpiLine( hps, &pt );
  pt.x = iRight - bOutlined;
  GpiLine( hps, &pt );
  GpiSetColor( hps, iRaised ? ( iRaised == 1 ? SB2c_Darker : SB2c_Filler ) :
    SB2c_Brighter );
  pt.y = iBottom + bOutlined;
  GpiLine( hps, &pt );
  pt.x = iLeft + bOutlined;
  GpiLine( hps, &pt );
}


// Method to read a line from the file and strip extra spaces & comments
// Returns "cleaned" string
char* /*__export*/ IniFile::Read( void )
{
  // Get next text line from the source file
  if ( ReadString() )
  {
    sBuffer[0] = 0;
    return sBuffer;
  }
  
  // Strip leading spaces
  char* curr_ptr = sBuffer;
  while ( *curr_ptr == ' ' || *curr_ptr == '\t' ) curr_ptr++;
  
  // Strip comments (the comment starts with the ";" character in the beginning
  //  of the text line or with the " ;" sequence in the middle of the line)
  char* semicolon = strchr( curr_ptr, ';' );
  if ( semicolon && ( semicolon == curr_ptr || *( semicolon - 1 ) == ' ' ||
    *( semicolon - 1 ) == '\t' ) ) *semicolon = 0;
    
  // Cut extra spaces in the end of line
  if ( *curr_ptr )
  {
    semicolon = curr_ptr + strlen( curr_ptr ) - 1;
    while( ( *semicolon == ' ' || *semicolon == 9 || *semicolon == 13 ||
       *semicolon == 10 ) && *semicolon ) *semicolon-- = 0;
  }
  
  return curr_ptr;
}

// Get the next value of the same entry
// Parameter: entry name
// Returns entry value
char* /*__export*/ IniFile::GetNext( char* pszEntry )
{
  // Read an input file until the end
  while ( ! feof( f ) )
  {
    char* s = Read();  //get the next line and strip spaces and comments
    
    if ( *s == '[' ) return NULL;  //return if we've reached the next section
    
    // Search for "=" char
    char* equals = strchr( s, '=' );
    if ( equals )
    {
      // Get the rest of line (all after "=")
      *equals = 0;
      if ( equals++ != s )
      {
        // Strip spaces between the entry name and "="
        char* entryend = equals - 2;
        while ( entryend > s && ( *entryend == ' ' || *entryend == 9 ) )
          *entryend-- = 0;
      }
    }
    
    // Compare entry name with given. Return the rest of line if OK
    if ( ! strcmp( s, pszEntry ) ) return equals;
  }
  
  // Otherwise return NULL
  return NULL;
}

// Method to get the first value of entry from specified section
// Parameters are:
//   pszSection - section name,
//   pszEntry - entry name,
//   pszDefault - default value (used when entry not found)
// Returns entry value in the static buffer
char* /*__export*/ IniFile::Get( char* pszSection, char* pszEntry,
  char* pszDefault )
{
  // Rewind to the beginning if INI-file
  fseek( f, 0L, 0 );
  
  // Read an input file until the end
  while ( ! feof( f ) )
  {
    char* s = Read();  //get next line
    
    // Search for section name
    if ( s[0] == '[' )
    {
      char* close = strchr( ++s, ']' );
      if ( close )
      {
        *close = 0;

        // Compare section name with given
        if ( ! strcmp( s, pszSection ) )
        {
          // Look for an entry and return its value at success
          if ( s = GetNext( pszEntry ) ) return s;
          else return pszDefault;  //otherwise return the default value
        }
      }
    }
  }
  return pszDefault;  //if section not found, return the default value
}


void __export SB2_Button( HPS hps, int iLeft, int iBottom, int iRight,
  int iTop, short int iRaised, 
  HBITMAP hBitmap, int iBitmapWidth, int iBitmapHeight )
{
  SB2_Border( hps, iLeft, iBottom, iRight, iTop, iRaised == 1 ? 1 : 2,
    SB2c_Filler, 0 );
  if ( hBitmap )
  {
    POINTL pt4[4];
    pt4[0].x = ( ( iRight + iLeft - iBitmapWidth + 1 ) >> 1 ) + 1 - iRaised;
    pt4[0].y = ( ( iTop + iBottom - iBitmapHeight + 1 ) >> 1 ) - 1 + iRaised;
    pt4[1].x = pt4[0].x + iBitmapWidth - 1;
    pt4[1].y = pt4[0].y + iBitmapHeight - 1;
    pt4[2].x = 0;
    pt4[2].y = 0;
    pt4[3].x = iBitmapWidth;
    pt4[3].y = iBitmapHeight;
    GpiWCBitBlt( hps, hBitmap, 4L, pt4, ROP_SRCCOPY, BBO_IGNORE );
  }
}

// Dialog page adder
ULONG __export SB2_AddDlgPage( HWND hwnd, ULONG ulNotebookID,
  char *pszTabText, char *pszComment )
{
  ULONG ulPageID = ( ULONG ) WinSendDlgItemMsg( hwnd, ulNotebookID,
    BKM_INSERTPAGE, ( MPARAM ) NULL, MPFROM2SHORT( ( BKA_MAJOR |
    BKA_STATUSTEXTON | BKA_AUTOPAGESIZE ), BKA_LAST ) );
  WinSendDlgItemMsg( hwnd, ulNotebookID, BKM_SETTABTEXT,
    ( MPARAM ) ulPageID, MPFROMP ( pszTabText ) );
  WinSendDlgItemMsg( hwnd, ulNotebookID, BKM_SETSTATUSLINETEXT,
    ( MPARAM ) ulPageID, MPFROMP ( pszComment ) );
  return ulPageID;
}



inline int atoi2( char *s, int iDefault = 0 )
{
  if ( s && *s ) return atoi( s );
  else return iDefault;
}



static char *pszFontConfig[] =
{
  "fsSelection",     // 0
  "lMatch",          // 1
  "szFacename",      // 2
  "idRegistry",      // 3
  "lMaxBaselineExt", // 4
  "lAveCharWidth",   // 5
  "fsType"           // 6
};

void __export SB2_LoadFontCfg( char *pszSection, FATTRS *pF, IniFile *pCfg )
{
  memset( pF, 0, sizeof ( FATTRS ) );
  pF->usRecordLength = sizeof ( FATTRS );
  pF->usCodePage = 0;
  pF->lMaxBaselineExt = 13;
  pF->lAveCharWidth = 5;
  pF->fsFontUse = FATTR_FONTUSE_NOMIX;
  strcpy( pF->szFacename, "Helv" );
  if ( pCfg && ( *pCfg )() )
  {
    pF->fsSelection =
      atoi2( pCfg->Get( pszSection, pszFontConfig[0] ) );
    pF->lMatch = atoi2( pCfg->Get( pszSection, pszFontConfig[1] ) );
    pF->idRegistry = atoi2( pCfg->Get( pszSection, pszFontConfig[3] ) );
    pF->lMaxBaselineExt =
      atoi2( pCfg->Get( pszSection, pszFontConfig[4] ), 13 );
    pF->lAveCharWidth =
      atoi2( pCfg->Get( pszSection, pszFontConfig[5] ), 5 );
    pF->fsType = atoi2( pCfg->Get( pszSection, pszFontConfig[6] ) );
    char *pS = pCfg->Get( pszSection, pszFontConfig[2] );
    if ( pS && *pS ) strcpy( pF->szFacename, pS );
  }
}

void __export SB2_SaveFontCfg( char *pszSection, FATTRS *pF, FILE *f )
{
  fprintf( f,
    "[%s]\n"
    "%s=%i\n"
    "%s=%i\n"
    "%s=%s\n"
    "%s=%i\n"
    "%s=%i\n"
    "%s=%i\n"
    "%s=%i\n\n",
    pszSection,
    pszFontConfig[0], pF->fsSelection,
    pszFontConfig[1], pF->lMatch,
    pszFontConfig[2], pF->szFacename,
    pszFontConfig[3], pF->idRegistry,
    pszFontConfig[4], pF->lMaxBaselineExt,
    pszFontConfig[5], pF->lAveCharWidth,
    pszFontConfig[6], pF->fsType );
}

void DescWindow::Hide( void )
{
  WinSetWindowPos( hwDescFrame, 0, 0, 0, 0, 0, SWP_HIDE );
}
void DescWindow::SetText( char *pszNewText )
{
  int l = strlen( pszNewText );
  while ( l > 0 && ( pszNewText[l - 1] == '\r' || pszNewText[l - 1] == '\n' ) )
    l--;
  char *p = new char[l + 1];
  if ( p )
  {
    for ( int i = 0; i < l ; i++ ) p[i] = ( pszNewText[i] == '\n' ||
      pszNewText[i] == '\r' ? ' ' : pszNewText[i] );
    p[i] = 0;

    if ( pszText ) delete pszText;
    pszText = p;
  }
}
void DescWindow::AdjustSize( FATTRS *pF, HWND hwnd )
{
  if ( pszText )
  {
    HPS hps = WinGetPS( hwnd );
    pF->usCodePage = GpiQueryCp( hps );
    GpiCreateLogFont( hps, NULL, 200L, pF );
    GpiSetCharSet( hps, 200L );
    CHARBUNDLE cb;
    cb.usTextAlign = TA_LEFT | TA_BASE;
    GpiSetAttrs( hps, PRIM_CHAR, CBB_TEXT_ALIGN, 0, &cb );
    POINTL ptl[TXTBOX_COUNT];
    GpiQueryTextBox( hps, strlen( pszText ), pszText, TXTBOX_COUNT, ptl );
    iDescWidth = ptl[TXTBOX_TOPRIGHT].x + 10;
    iDescHeight = ptl[TXTBOX_TOPRIGHT].y + 10;
    GpiDeleteSetId( hps, 200L );
    WinReleasePS( hps );
  }
}
void DescWindow::MoveTo( int x, int y )
{
  iDescX = x;
  iDescY = y;
  WinSetWindowPos( hwDescFrame, HWND_TOP, x, y, iDescWidth, iDescHeight,
    SWP_MOVE | SWP_SIZE | SWP_SHOW | SWP_ZORDER );
  WinInvalidateRect( hwDescFrame, NULL, TRUE );
}
void DescWindow::Paint( FATTRS *pF, HWND hwnd )
{
  HPS hps;
  RECTL rc;
  hps = WinBeginPaint( hwnd, 0L, &rc );
  SB2_Border( hps, 0, 0, iDescWidth - 1, iDescHeight - 1, 1, SB2c_Filler, 1 );
//  SB2_Border( hps, 2, 2, iDescWidth - 3, iDescHeight - 3, 0, CLR_BLACK );
  if ( pszText )
  {
    GpiCreateLogFont( hps, NULL, 200L, pF );
    GpiSetCharSet( hps, 200L );
    CHARBUNDLE cb;
    cb.lColor = CLR_WHITE;
    cb.usTextAlign = TA_LEFT | TA_BASE;
    GpiSetAttrs( hps, PRIM_CHAR, CBB_COLOR | CBB_TEXT_ALIGN, 0, &cb );
    POINTL pt;
    pt.x = 4;
    pt.y = 6;
    RECTL rc1;
    rc1.xLeft = 4;
    rc1.yBottom = 4;
    rc1.xRight = iDescWidth - 5;
    rc1.yTop = iDescHeight - 5;
    ULONG uOptions = CHS_CLIP;
    GpiCharStringPosAt( hps, &pt, &rc1, uOptions, strlen( pszText ), pszText,
      NULL );
    GpiDeleteSetId( hps, 200L );
  }
  WinEndPaint( hps );
}
HWND DescWindow::CreateWindow( char *pszClassName )
{
  ULONG ulWinStyle = FCF_NOBYTEALIGN;
  return ( hwDescFrame = WinCreateStdWindow( HWND_DESKTOP, 0, &ulWinStyle,
    pszClassName, 0, 0, NULLHANDLE, 1, &hwDescClient ) );
}
DescWindow::~DescWindow( void )
{
  if ( hwDescFrame ) WinDestroyWindow( hwDescFrame );
  if ( pszText ) delete pszText;
}
DescWindow::DescWindow( void )
{
  pszText = NULL;
  hwDescFrame = NULL;
}
void DescWindow::AdjustPos( int x, int y, int dx, int dy, int bVertical )
{
  int iMaxX = WinQuerySysValue( HWND_DESKTOP, SV_CXSCREEN );
  int iMaxY = WinQuerySysValue( HWND_DESKTOP, SV_CYSCREEN );
  if ( bVertical )
  {
    x += ( ( x + dx + 3 + iDescWidth ) < iMaxX ) ? dx + 3 : - 3 - iDescWidth;
    if ( y < 0 ) y = 0;
    else if ( y + iDescHeight >= iMaxY ) y = iMaxY - iDescHeight;
  }
  else
  {
    y += ( ( y - 3 - iDescHeight ) < 0 ) ? dy + 3 : -3 - iDescHeight;
    if ( x < 0 ) x = 0;
    else if ( x + iDescWidth >= iMaxX ) x = iMaxX - iDescWidth;
  }
  MoveTo( x, y );
}

static char *pszColorNames[] =
{
  "white", "palegray", "blue", "red", "pink", "green", "cyan", "yellow",
  "darkgray", "darkblue", "darkred", "darkpink", "darkgreen", "darkcyan",
  "brown", "black"
};
int iColorValues[] =
{
  CLR_WHITE, CLR_PALEGRAY, CLR_BLUE, CLR_RED, CLR_PINK, CLR_GREEN, CLR_CYAN,
  CLR_YELLOW, CLR_DARKGRAY, CLR_DARKBLUE, CLR_DARKRED, CLR_DARKPINK,
  CLR_DARKGREEN, CLR_DARKCYAN, CLR_BROWN, CLR_BLACK
};
const int iColorCount = 16;

void __export SB2_FillColorList( HWND hwnd, ULONG ulID, int iSelected )
{
  for ( int i = 0; i < iColorCount; i++ )
    WinSendDlgItemMsg( hwnd, ulID, LM_INSERTITEM, ( MPARAM )
      LIT_END, MPFROMP( pszColorNames[i] ) );
  WinSendDlgItemMsg( hwnd, ulID, LM_SELECTITEM,
    ( MPARAM ) iSelected, ( MPARAM ) 1 );
}

char* __export SB2_ColorName( int iIndex )
{
  return pszColorNames[iIndex];
}

int __export SB2_ColorValue( int iIndex )
{
  return iColorValues[iIndex];
}

int __export SB2_ColorA2I( char *pszName )
{
  for ( int i = 0; i < iColorCount; i++ )
    if ( ! strcmp( pszName, pszColorNames[i] ) ) return i;
  return 0;
}

int __export SB2_ColorCount( void )
{
  return iColorCount;
}


char* __export SB2_ParseValue( char *s, int& i, char delim = ',')
{
  if ( ! s || s[0] == 0 ) return NULL;
  char *pS = strchr( s, delim );
  if ( pS ) *pS++ = 0;
  i = atoi( s );
  return pS;
}

char* __export SB2_ParseValue( char *s, char *d, char delim = ',')
{
  if ( ! s || s[0] == 0 ) return NULL;
  char *pS = strchr( s, delim );
  if ( pS ) *pS++ = 0;
  strcpy( d, s );
  return pS;
}
