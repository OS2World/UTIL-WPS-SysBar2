/*

  SysBar/2 Utility Set  version 0.23

  CD-player engine for IOCtl

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


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "CDengine.h"


// IOCTL definitions ---------------------------------------------------------
#define IOCTL_CDROMDISK 0x0080
#define IOCTL_CDROMAUDIO 0x0081
#define DSK_UNLOCKEJECTMEDIA 0x0040
#define DSK_GETLOCKSTATUS 0x0066
#define CDROMDISK_SEEK 0x0050
#define CDROMDISK_DEVICESTATUS 0x0060
#define CDROMDISK_GETDRIVER 0x0061
#define CDROMDISK_GETHEADLOC 0x0070
#define CDROMAUDIO_SETCHANNELCTRL 0x0040
#define CDROMAUDIO_PLAYAUDIO 0x0050
#define CDROMAUDIO_STOPAUDIO 0x0051
#define CDROMAUDIO_RESUMEAUDIO 0x0052
#define CDROMAUDIO_GETAUDIODISK 0x0061
#define CDROMAUDIO_GETAUDIOTRACK 0x0062


// CD device finder ----------------------------------------------------------
int CDDeviceFinder( CDROMDeviceMap& CDMap )
{
  HFILE hf;
  ULONG ulAction = 0;
  ULONG ulParamSize = sizeof ( ulAction );
  ULONG ulDataSize = sizeof ( CDROMDeviceMap );

  if ( DosOpen( "CD-ROM2$", &hf, &ulAction, 0, FILE_NORMAL,
    OPEN_ACTION_FAIL_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS,
    OPEN_ACCESS_READONLY | OPEN_SHARE_DENYNONE, NULL ) ) return 0;
  DosDevIOCtl( hf, 0x82, 0x60, NULL, 0, &ulParamSize,
    ( PVOID ) &CDMap, sizeof ( CDROMDeviceMap ), &ulDataSize );
  DosClose( hf );
  return CDMap.usDriveCount;
}


//  CD Drive class -----------------------------------------------------------
typedef struct _NullHandleParams
{
  UCHAR ucCommand;
  USHORT usDrive;
} NullHandleParams;

typedef struct _CDSignature
{
  UCHAR sSignature[4];
} CDSignature;

typedef struct _CDAudioTracks
{
  UCHAR ucFirstTrack;
  UCHAR ucLastTrack;
  ULONG ulLeadOutAddress;
} CDAudioTracks;

typedef struct _CDSeekParam
{
  CDSignature CD01;
  UCHAR ucAddrMode;
  ULONG ulPosition;
} CDSeekParam;

typedef struct _CDPlayParam
{
  CDSignature CD01;
  UCHAR ucAddrMode;
  ULONG ulFromPosition;
  ULONG ulTillPosition;
} CDPlayParam;

typedef struct _CDGetLocParam
{
  CDSignature CD01;
  UCHAR ucAddrMode;
} CDGetLocParam;

typedef struct _CDTrackParam
{
  CDSignature CD01;
  UCHAR ucTrack;
} CDTrackParam;

typedef struct _CDTrackData
{
  ULONG ulAddress;
  UCHAR ucControl;
} CDTrackData;

typedef struct _CDChanCtrlData
{
  UCHAR ucInput0;
  UCHAR ucVolume0;
  UCHAR ucInput1;
  UCHAR ucVolume1;
  UCHAR ucInput2;
  UCHAR ucVolume2;
  UCHAR ucInput3;
  UCHAR ucVolume3;
} CDChanCtrlData;


CDDrive::CDDrive( USHORT usDriveLetter )
{
  usDrive = ( usDriveLetter & 0x5F ) - 'A';
  hCD = NULLHANDLE;
  usStatus = CDS_NoMedia;
  ulCurrentPosition = 0;
  iCurrentTrack = -1;
  Tracks = 0;
  iTrackCount = 0;
  iFirstTrack = 0;
}

APIRET CDDrive::Open( void )
{
  FillDevice();

  ulCurrentPosition = 0;
  iCurrentTrack = -1;

  ULONG ulAction = 0;
  APIRET rc;
  // open CD-ROM drive device
  if ( ! ( rc = DosOpen( szDevice, &hCD, &ulAction, 0L, 0L,
    OPEN_ACTION_FAIL_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS, OPEN_FLAGS_DASD |
    OPEN_FLAGS_FAIL_ON_ERROR | OPEN_FLAGS_NOINHERIT | OPEN_SHARE_DENYNONE,
    0L ) ) )
  {
    CDSignature cds1 = { 'C', 'D', '0', '1' };
    CDSignature cds2 = { 0 };
    ULONG ulParamSize = sizeof ( CDSignature );
    ULONG ulDataSize = sizeof ( CDSignature );
    // check for proper driver signature
    if ( ! ( rc = DosDevIOCtl( hCD, IOCTL_CDROMDISK, CDROMDISK_GETDRIVER,
      ( PVOID ) &cds1, sizeof( CDSignature ), &ulParamSize,
      ( PVOID ) &cds2, sizeof( CDSignature ), &ulDataSize ) ) )
    {
      if ( memcmp( &cds1, &cds2, sizeof ( CDSignature ) ) ) rc = -1;
      else
      {
        CDAudioTracks cdat = { 0 };
        ulDataSize = sizeof ( CDAudioTracks );
        // get track count
        if ( ! ( rc = DosDevIOCtl( hCD,
          IOCTL_CDROMAUDIO, CDROMAUDIO_GETAUDIODISK,
          ( PVOID ) &cds1, sizeof( CDSignature ), &ulParamSize,
          ( PVOID ) &cdat, sizeof( CDAudioTracks ), &ulDataSize ) ) )
        {
          iFirstTrack = cdat.ucFirstTrack;
          iTrackCount = cdat.ucLastTrack - cdat.ucFirstTrack + 1;
          if ( ( Tracks = new CDTrack[iTrackCount] ) )
          {
            int iPlayableTracks = 0;
            // build TOC
            for ( int i = 0; i < iTrackCount; i++ )
            {
              CDTrackParam cdtp =
                { { 'C', 'D', '0', '1' }, ( UCHAR ) i + cdat.ucFirstTrack };
              ulParamSize = sizeof ( CDTrackParam );
              CDTrackData cdtd = { 0 };
              ulDataSize = sizeof ( CDTrackData );
              if ( ! ( rc = DosDevIOCtl( hCD,
                IOCTL_CDROMAUDIO, CDROMAUDIO_GETAUDIOTRACK,
                ( PVOID ) &cdtp, sizeof( CDTrackParam ), &ulParamSize,
                ( PVOID ) &cdtd, sizeof( CDTrackData ), &ulDataSize ) ) )
              {
                Tracks[i].ulStartingTime = cdtd.ulAddress;
                Tracks[i].bPlayable = ( ( cdtd.ucControl & 64 ) == 0 );
                iPlayableTracks += Tracks[i].bPlayable;
              }
              Tracks[i].iTrackNumber = i + cdat.ucFirstTrack;
              if ( i ) Tracks[i - 1].ulEndingTime = Tracks[i].ulStartingTime;
            }
            Tracks[iTrackCount - 1].ulEndingTime = cdat.ulLeadOutAddress;

            usStatus = CDS_MediaPresent;

            // stop drive
            if ( iPlayableTracks )
            {
              rc = Stop();

              // reset volume
              CDSignature cds = { 'C', 'D', '0', '1' };
              ulParamSize = sizeof ( CDSignature );
              CDChanCtrlData cdccd = { 0, 255, 1, 255, 2, 255, 3, 255 };
              ulDataSize = sizeof ( CDChanCtrlData );
              DosDevIOCtl( hCD, IOCTL_CDROMAUDIO, CDROMAUDIO_SETCHANNELCTRL,
                ( PVOID ) &cds, sizeof( CDTrackParam ), &ulParamSize,
                ( PVOID ) &cdccd, sizeof( CDTrackData ), &ulDataSize );
            }
            else
            {
              // close the device if there's no playable tracks
              rc = Pause();
              Close();
            }
          }
          else rc = -1;
        }
      }
    }
  }

  if ( rc ) Close();
  return rc;
}

void CDDrive::FillDevice( void )
{
  szDevice[0] = usDrive + 'A';
  szDevice[1] = ':';
  szDevice[2] = 0;
}

CDDrive::~CDDrive( void )
{
  Close();
}

CDStatusChanges CDDrive::CheckStatus( void )
{
  CDStatusChanges iStatusChange = CDS_Unchanged;

  NullHandleParams glsp = { 0, usDrive };
  ULONG ulParamSize = sizeof ( NullHandleParams );
  USHORT usData = 0;
  ULONG ulDataSize = sizeof ( USHORT );

  // look for media presence
  if ( DosDevIOCtl( ( HFILE ) -1, IOCTL_DISK, DSK_GETLOCKSTATUS,
    ( PVOID ) &glsp, sizeof( NullHandleParams ), &ulParamSize,
    ( PVOID ) &usData, sizeof( USHORT ), &ulDataSize ) == 0 )
  {
    // if media is present
    if ( usData & 4 )
    {
      // if there was no media
      if ( usStatus <= CDS_NoMedia )
      {
        // try to open the device
        if ( Open() == 0 ) iStatusChange = CDS_MediaInserted;
      }
      // if the media was already in drive and it has been opened
      else if ( hCD )
      {
        CDSignature cds1 = { 'C', 'D', '0', '1' };
        ULONG ulParamSize = sizeof ( CDSignature );
        ULONG ulData = 0L;
        ULONG ulDataSize = sizeof ( ULONG );
        if ( DosDevIOCtl( hCD, IOCTL_CDROMDISK, CDROMDISK_DEVICESTATUS,
          ( PVOID ) &cds1, sizeof( CDSignature ), &ulParamSize,
          ( PVOID ) &ulData, sizeof ( ULONG ), &ulDataSize ) == 0 )
        {
          // if the device is currently playing
          if ( ulData & 0x1000 )
          {
            CDGetLocParam cdglp = { { 'C', 'D', '0', '1' }, ( UCHAR ) 1 };
            ULONG ulParamSize = sizeof ( CDGetLocParam );
            ULONG ulPosition = 0L;
            ULONG ulDataSize = sizeof ( ULONG );
            if ( DosDevIOCtl( hCD, IOCTL_CDROMDISK, CDROMDISK_GETHEADLOC,
              ( PVOID ) &cdglp, sizeof( CDGetLocParam ), &ulParamSize,
              ( PVOID ) &ulPosition, sizeof ( ULONG ), &ulDataSize ) == 0 )
            {
              if ( ulPosition > Tracks[iCurrentTrack].ulEndingTime ||
                ( ulPosition & 0xFFFF00 ) != ( ulCurrentPosition & 0xFFFF00 ) )
              {
                ulCurrentPosition = ulPosition;
                int iOldTrack = iCurrentTrack;
                iStatusChange = ( iOldTrack == AdjustTrack() ) ?
                  CDS_PositionChanged : CDS_TrackChanged;
              }
            }

            if ( usStatus != CDS_Playing )
            {
              usStatus = CDS_Playing;
              iStatusChange = CDS_PlayingStarted;
            }
          }
          else
          {
            if ( usStatus == CDS_Playing )
            {
              usStatus = CDS_MediaPresent;
              iStatusChange = CDS_PlayingStopped;
            }
          }
        }
      }
    }
    // if there's no media
    else
    {
      // if there was a media removal
      if ( usStatus > CDS_NoMedia )
      {
        Close();
        usStatus = CDS_NoMedia;
        iStatusChange = CDS_MediaRemoved;
      }
    }
  }

  return iStatusChange;
}

APIRET CDDrive::Pause( void )
{
  APIRET rc = -1;
  if ( hCD )
  {
    CDSignature cds1 = { 'C', 'D', '0', '1' };
    ULONG ulParamSize = sizeof ( CDSignature );
    if ( ( rc = DosDevIOCtl( hCD, IOCTL_CDROMAUDIO, CDROMAUDIO_STOPAUDIO,
      ( PVOID ) &cds1, sizeof( CDSignature ), &ulParamSize,
      NULL, 0, NULL ) ) == 0 ) usStatus = CDS_Paused;
  }
  return rc;
}

APIRET CDDrive::Close( void )
{
  APIRET rc = -1;
  if ( hCD && ! ( rc = DosClose( hCD ) ) ) hCD = NULLHANDLE;

  if ( Tracks )
  {
    delete[] Tracks;
    Tracks = 0;
    iTrackCount = 0;
  }

  if ( usStatus > CDS_MediaPresent ) usStatus = CDS_MediaPresent;
  ulCurrentPosition = 0;
  iCurrentTrack = -1;

  return rc;
}

APIRET CDDrive::TrayAction( UCHAR ucCommand )
{
  NullHandleParams nhp = { ucCommand, usDrive };
  ULONG ulParamSize = sizeof ( NullHandleParams );

  return DosDevIOCtl( ( HFILE ) -1, IOCTL_DISK, DSK_UNLOCKEJECTMEDIA,
    ( PVOID ) &nhp, sizeof( NullHandleParams ), &ulParamSize, NULL, 0, NULL );
}

APIRET CDDrive::Seek( ULONG ulPosition )
{
  APIRET rc = -1;
  if ( hCD )
  {
    CDSeekParam cdsp = { { 'C', 'D', '0', '1' }, ( UCHAR ) 1, ulPosition };
    ULONG ulParamSize = sizeof ( CDSeekParam );
    if ( ( rc = DosDevIOCtl( hCD, IOCTL_CDROMDISK, CDROMDISK_SEEK,
      ( PVOID ) &cdsp, sizeof( CDSeekParam ), &ulParamSize,
      NULL, 0, NULL ) ) == 0 )
    {
      ulCurrentPosition = ulPosition;
      AdjustTrack();
    }
  }
  return rc;
}

int CDDrive::AdjustTrack( void )
{
  iCurrentTrack = -1;
  for ( int i = 0; i < iTrackCount; i++ )
    if ( ulCurrentPosition >= Tracks[i].ulStartingTime &&
      ulCurrentPosition < Tracks[i].ulEndingTime )
    {
      iCurrentTrack = i;
      break;
    }
  return iCurrentTrack;
}

APIRET CDDrive::Stop( void )
{
  APIRET rc = Pause();
  if ( ! rc ) for ( int i = 0; i < iTrackCount; i++ ) if ( Tracks[i].bPlayable )
  {
    if ( ( rc = Seek( Tracks[i].ulStartingTime ) ) == 0 )
      usStatus = CDS_MediaPresent;
    break;
  }
  return rc;
}

APIRET CDDrive::Play( )
{
  APIRET rc = -1;
  if ( hCD )
  {
    AdjustTrack();
    CDPlayParam cdpp =
      { { 'C', 'D', '0', '1' }, ( UCHAR ) 1, ulCurrentPosition, 0L };

    int i;
    //if ( bCurrentTrackOnly ) i = iCurrentTrack; else
       for ( i = iTrackCount - 1; i >= 0; i-- ) if ( Tracks[i].bPlayable ) break;
    cdpp.ulTillPosition = Tracks[i].ulEndingTime;
      
    ULONG ulParamSize = sizeof ( CDPlayParam );
    rc = DosDevIOCtl( hCD, IOCTL_CDROMAUDIO, CDROMAUDIO_PLAYAUDIO,
      ( PVOID ) &cdpp, sizeof( CDPlayParam ), &ulParamSize,
      NULL, 0, NULL );
  }
  return rc;
}

APIRET CDDrive::Resume( void )
{
  APIRET rc = -1;
  if ( hCD )
  {
    CDSignature cds1 = { 'C', 'D', '0', '1' };
    ULONG ulParamSize = sizeof ( CDSignature );
    rc = DosDevIOCtl( hCD, IOCTL_CDROMAUDIO, CDROMAUDIO_RESUMEAUDIO,
      ( PVOID ) &cds1, sizeof( CDSignature ), &ulParamSize,
      NULL, 0, NULL );
  }
  return rc;
}

APIRET CDDrive::EjectTray( void )
{
  APIRET rc = TrayAction( 2 );
  if ( ! rc )
  {
    Close();
    usStatus = CDS_TrayOpened;
  }
  return rc;
}

APIRET CDDrive::LoadTray( void )
{
  APIRET rc = TrayAction( 3 );
  if ( ! rc ) usStatus = CDS_NoMedia;
  return rc;
}

APIRET CDDrive::SkipToTrack( int iTrack )
{
  USHORT usOldStatus = usStatus;
  APIRET rc = 0;
  if ( ! iTrackCount ) return -1;
  if ( usOldStatus == CDS_Playing ) rc = Pause();
  while ( ! Tracks[iTrack].bPlayable )
  {
    if ( ++iTrack == iTrackCount ) iTrack = 0;
  }
  if ( ! rc ) rc = Seek( Tracks[iTrack].ulStartingTime );
  if ( ! rc ) switch ( usOldStatus )
  {
    case CDS_Playing:
      rc = Play();
      break;
    case CDS_Paused:
      usStatus = CDS_MediaPresent;
      break;
  }
  return rc;
}

APIRET CDDrive::NextTrack()
{
  if ( ! iTrackCount ) return -1;
  int n = iCurrentTrack;
  do
  {
    if ( ++n == iTrackCount ) n = 0;
  }
  while ( ! Tracks[n].bPlayable );
  return SkipToTrack( n );
}

APIRET CDDrive::PrevTrack()
{
  if ( ! iTrackCount ) return -1;
  int n = iCurrentTrack;
  do
  {
    if ( --n < 0 ) n = iTrackCount - 1;
  }
  while ( ! Tracks[n].bPlayable );
  return SkipToTrack( n );
}

static ULONG CDDrive::MSFAdd( ULONG ulValue1, ULONG ulValue2 )
{
  ULONG ulResult = ulValue1 + ulValue2;
  if ( ( ulResult & 255 ) >= 75 ) ulResult += 0x100 - 75;
  if ( ( ulResult & 0xFF00 ) >= 0x3C00 ) ulResult += 0x10000 - 0x3C00;
  return ulResult;
}

static ULONG CDDrive::MSFSub( ULONG ulValue1, ULONG ulValue2 )
{
  ULONG ulResult = ulValue1 - ulValue2;
  if ( ( ulResult & 255 ) >= 75 ) ulResult -= 0x100 - 75;
  if ( ( ulResult & 0xFF00 ) >= 0x3C00 ) ulResult -= 0x10000 - 0x3C00;
  return ulResult;
}

void CDDrive::Shuffle( bool chaos )
{
  if( chaos ) {
    for(int i=0; i<iTrackCount-1; i++) {
      int j = i + rand()%(iTrackCount-i);
      CDTrack temp = Tracks[i];
      Tracks[i] = Tracks[j];
      Tracks[j] = temp;
    }
  }
  else {
    CDTrack *newTracks = new CDTrack[iTrackCount];
    for(int i=0;i<iTrackCount;i++) newTracks[Tracks[i].iTrackNumber-iFirstTrack] = Tracks[i];
    delete[] Tracks;
    Tracks = newTracks;
  }
  SkipToTrack(0);
}


