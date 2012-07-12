//////////////////////////////////////////////////////////////////////////////
// Program Name: content.h
// Created     : Mar. 7, 2011
//
// Copyright (c) 2011 David Blain <dblain@mythtv.org>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef CONTENT_H
#define CONTENT_H

#include <QScriptEngine>

#include "services/contentServices.h"

class Content : public ContentServices
{
    Q_OBJECT

    public:

        Q_INVOKABLE Content( QObject *parent = 0 ) {}

    public:

        QFileInfo           GetFile             ( const QString   &StorageGroup,
                                                  const QString   &FileName );

        QFileInfo           GetImageFile        ( const QString   &StorageGroup,
                                                  const QString   &FileName,
                                                  int Width, int Height );

        QStringList         GetFileList         ( const QString   &StorageGroup );

        QFileInfo           GetRecordingArtwork ( const QString   &Type,
                                                  const QString   &Inetref,
                                                  int Season, int Width,
                                                  int Height);

        DTC::ArtworkInfoList*
                            GetRecordingArtworkList( int              ChanId,
                                                     const QDateTime &StartTime  );

        DTC::ArtworkInfoList*
                            GetProgramArtworkList( const QString &Inetref,
                                                   int            Season  );

        QFileInfo           GetVideoArtwork     ( const QString   &Type,
                                                  int Id, int Width, int Height );

        QFileInfo           GetAlbumArt         ( int Id, int Width, int Height );

        QFileInfo           GetPreviewImage     ( int              ChanId,
                                                  const QDateTime &StartTime,
                                                  int              Width,
                                                  int              Height,
                                                  int              SecsIn );

        QFileInfo           GetRecording        ( int              ChanId,
                                                  const QDateTime &StartTime );

        QFileInfo           GetMusic            ( int Id );
        QFileInfo           GetVideo            ( int Id );

        QString             GetHash             ( const QString   &storageGroup,
                                                  const QString   &FileName );

        bool                DownloadFile        ( const QString   &URL,
                                                  const QString   &StorageGroup );

        // HTTP Live Streaming
        DTC::LiveStreamInfo     *AddLiveStream          ( const QString   &StorageGroup,
                                                          const QString   &FileName,
                                                          const QString   &HostName,
                                                          int              MaxSegments,
                                                          int              Width,
                                                          int              Height,
                                                          int              Bitrate,
                                                          int              AudioBitrate,
                                                          int              SampleRate );

        DTC::LiveStreamInfo     *AddRecordingLiveStream ( int              ChanId,
                                                          const QDateTime &StartTime,
                                                          int              MaxSegments,
                                                          int              Width,
                                                          int              Height,
                                                          int              Bitrate,
                                                          int              AudioBitrate,
                                                          int              SampleRate );

        DTC::LiveStreamInfo     *AddVideoLiveStream     ( int              Id,
                                                          int              MaxSegments,
                                                          int              Width,
                                                          int              Height,
                                                          int              Bitrate,
                                                          int              AudioBitrate,
                                                          int              SampleRate );

        DTC::LiveStreamInfo     *GetLiveStream            ( int Id );
        DTC::LiveStreamInfoList *GetLiveStreamList        ( void );
        DTC::LiveStreamInfoList *GetFilteredLiveStreamList( const QString &FileName );

        DTC::LiveStreamInfo     *StopLiveStream         ( int Id );
        bool                     RemoveLiveStream       ( int Id );
};

Q_SCRIPT_DECLARE_QMETAOBJECT( Content, QObject*);

#endif


