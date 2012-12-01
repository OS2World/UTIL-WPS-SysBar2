/*

  SysBar/2 Utility Set  version 0.23

  CD-player engine for IOCtl -- definitions

  ..................................................................

  Copyright (c) 1995-1999  Dmitry I. Platonoff <dip@platonoff.com>
  Copyright (c) 2002,04    Max Alekseyev       <relf@os2.ru>

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

#pragma pack(1)

#define INCL_DOSDEVICES
#define INCL_DOSDEVIOCTL
#include <os2.h>


// CD device finder ----------------------------------------------------------
typedef struct _CDROMDeviceMap
{
  USHORT usDriveCount;
  USHORT usFirstLetter;
} CDROMDeviceMap;

// Fills CDROMDeviceMap structure and returns the number of CD-ROMs found.
extern int CDDeviceFinder( CDROMDeviceMap& CDMap );



// CD Drive class ------------------------------------------------------------
typedef struct _CDTrack
{
  short int iTrackNumber;  // "official" track number from TOC (1, 2, ...)
  short int bPlayable;     // TRUE for audio tracks
  ULONG ulStartingTime;    // track starting time (MSF)
  ULONG ulEndingTime;      // track ending time (starting time of next track)
} CDTrack;

enum CDStatuses
{
  CDS_TrayOpened,
  CDS_NoMedia,
  CDS_MediaPresent,
  CDS_Playing,
  CDS_Paused
};

enum CDStatusChanges
{
  CDS_Unchanged,
  CDS_MediaInserted,
  CDS_MediaRemoved,
  CDS_PlayingStarted,
  CDS_PlayingStopped,
  CDS_PositionChanged,
  CDS_TrackChanged
};

class CDDrive
{
protected:
  void FillDevice( void );
  APIRET TrayAction( UCHAR ucCommand );
  int AdjustTrack( void );

public:
  HFILE hCD;                // device handle (used only for audio CDs)
  char szDevice[3];         // drive name (e.g. "D:")

  USHORT usDrive;           // drive index (0 - A:, 1 - B:, etc.)
  CDStatuses usStatus;      // current device status
  ULONG ulCurrentPosition;  // current head position (MSF)
  USHORT iCurrentTrack;     // current track
  CDTrack *Tracks;          // tracks
  int iTrackCount;          // track count
  int iFirstTrack;          // first track #

  // Constructor. Initialized with the drive letter.
  CDDrive( USHORT usDriveLetter );

  // Destructor. Calls Close().
  ~CDDrive( void );


  // Device status checked. To call from parallel independent thread.
  CDStatusChanges CheckStatus( void );


    // Opens the device, reads the TOC and fills Tracks array of
   //   closes the device if no playable tracks has been detected.
  //    Called by CheckStatus() when a CD insertion detected.
  APIRET Open( void );

  // Stops playing and closes the device.
  APIRET Close( void );


  // Stops playing and ejects disk (or empty tray).
  APIRET EjectTray( void );

  // Loads tray
  APIRET LoadTray( void );


   // Stops playing, positions to the given track and resumes playing.
  //   (if played before).
  APIRET SkipToTrack( int iTrack );

  // Calls SkipToTrack() with the next playable track index.
  APIRET NextTrack( void );

  // Calls SkipToTrack() with the previous playable track index.
  APIRET PrevTrack( void );


  // Stops playing.
  APIRET Pause( void );

  // Starts playing from the beginning of current track.
  APIRET Play( void );

  // Resumes playing from the current position.
  APIRET Resume( void );

  // Positions to the given location.
  APIRET Seek( ULONG ulPosition );

  // Calls Pause() and then positions to the first playable track.
  APIRET Stop( void );

  // if true, shuffle tracks; if false, order them
  void Shuffle( bool );

  // MSF calculations.
  static ULONG MSFAdd( ULONG ulValue1, ULONG ulValue2 );
  static ULONG MSFSub( ULONG ulValue1, ULONG ulValue2 );
};


