/*

  SysBar/2 Utility Set  version 0.23

  SYSBAR2.DLL public library definitions

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

#include "ffile.h"


#define SB2c_Filler   CLR_DARKGRAY
#define SB2c_Brighter CLR_PALEGRAY
#define SB2c_Darker   CLR_BLACK
//#define SB2c_Filler   CLR_PALEGRAY
//#define SB2c_Brighter CLR_WHITE
//#define SB2c_Darker   CLR_DARKGRAY
#define DLG_PROPERTIES       2100
#define D_Prop_Notebook      2101


extern HMODULE SB2_Init( void );
extern void SB2_Over( void );

extern char* SB2_Strings( int iIndex );
const int iSB2S_Version = 0;
const int iSB2S_Copyright1 = 1;
const int iSB2S_Copyright2 = 2;


extern ULONG SB2_AddDlgPage( HWND hwnd, ULONG ulNotebookID,
  char *pszTabText, char *pszComment );

extern char *SB2_CfgFilename( char *pszCfg, char *pszExe );

extern void SB2_LargeDigits( HPS hps, int x, int y, char *pszString,
  short int bMonochrome, short int bLarge, int iCount = 0 );
extern void SB2_SmallDigits( HPS hps, int x, int y, char *pszString, 
  short int bMonochrome, short int bLarge, int iCount = 0 );

extern void SB2_Border( HPS hps, int iLeft, int iBottom, int iRight, int iTop,
  short int iRaised = 1, int iFiller = SB2c_Filler, int bOutlined = 0 );

extern void SB2_Button( HPS hps, int iLeft, int iBottom, int iRight, int iTop,
  short int iRaised, HBITMAP hBitmap, int iBitmapWidth, int iBitmapHeight );



// INI-style file reader
class IniFile : public ffile
{
public:
  IniFile( char* pszFilename, const char* pszMode = "rt" ) :  //Constructor
    ffile( pszFilename, pszMode ) {}			     //to open a file

  char* Read( void );  //Method to read a line from the file and strip
 		      //extra spaces & comments

           //Method to get the first value of entry from specified section
  char* Get( char* pszSection, char* pszEntry, char* pszDefault = NULL );

  char* GetNext( char* pszEntry );  //Get the next value of the same entry

};

extern void SB2_LoadFontCfg( char *pszSection, FATTRS *pF, IniFile *pCfg );
extern void SB2_SaveFontCfg( char *pszSection, FATTRS *pF, FILE *f );

extern void SB2_FillColorList( HWND hwnd, ULONG ulID, int iSelected = 0 );
extern char* SB2_ColorName( int iIndex );
extern int SB2_ColorValue( int iIndex );
extern int SB2_ColorA2I( char *pszName );
extern int SB2_ColorCount( void );

extern char* SB2_ParseValue( char *s, int& i, char delim = ',' );
extern char* SB2_ParseValue( char *s, char *d, char delim = ',' );


class DescWindow
{
  char *pszText;
public:

  HWND hwDescFrame;
  HWND hwDescClient;
  int iDescX;
  int iDescY;
  int iDescWidth;
  int iDescHeight;

  DescWindow( void );
  ~DescWindow( void );
  HWND CreateWindow( char *pszClassName );
  void SetText( char *pszText );
  void Hide( void );
  void AdjustSize( FATTRS *pF, HWND hwnd );
  void AdjustPos( int x, int y, int dx, int dy, int bVertical );
  void MoveTo( int x, int y );
  void Paint( FATTRS *pF, HWND hwnd );
};


