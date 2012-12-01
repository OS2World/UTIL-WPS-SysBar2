/*

  C++ class library for file streams
    version 1.00

  ..................................................................

  Copyright (c) 1995  Dmitry I. Platonoff
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

#if !defined(__ffile_h)
#define __ffile_h

#include <stdio.h>
#include <io.h>

const int iFFileMaxString = 256;

// Here is the class that represents a standard C file stream,
//  but has some features, e.g. auto-closing at destruction
class ffile
{
protected:
  unsigned uLinesGone;  //line counter (text files only)

public:

  FILE* f;  //pointer to FILE structure
  long lFileSize;  //file size (for read-only mode)
  char sBuffer[iFFileMaxString];  //buffer for text lines (text files only)

  ffile( char *pszFilename, const char* pszMode )  //Constructor (parameters
  {                                               //are the same as for fopen)
    if ( f = fopen( pszFilename, pszMode ) )
    {
      lFileSize = filelength( fileno( f ) );
      uLinesGone = 0;
    }
  }

  ~ffile() { if ( f ) fclose( f ); }  //destructor that closes file

  FILE* operator()() { return f; }  //"()" operator - for use FILE
  													 //structure outside the class
  long GetPosition( void ) { return ftell( f ); }  //position in file

  int ReadString( void )  //method to read a single line from a text file
  {
    if ( feof( f ) || ! fgets( sBuffer, iFFileMaxString, f ) ) return -1;
    else { uLinesGone++; return 0; }
  }

  unsigned GetCurrLine( void ) { return uLinesGone; }  //current line number

  void Close( void ) { if ( fclose( f ) == 0 ) f = NULL; }
};


#endif


