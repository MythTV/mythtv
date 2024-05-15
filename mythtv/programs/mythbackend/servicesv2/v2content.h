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
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef V2CONTENT_H
#define V2CONTENT_H

#include <QFileInfo>

#include "libmythbase/http/mythhttpservice.h"
#include "v2artworkInfoList.h"

#define CONTENT_SERVICE QString("/Content/")
#define CONTENT_HANDLE  QString("Content")

class V2Content : public MythHTTPService
{
    Q_OBJECT
    Q_CLASSINFO("Version",      "2.0")
    Q_CLASSINFO("DownloadFile",           "methods=POST;name=bool")
    Q_CLASSINFO("AddLiveStream",          "methods=GET,POST,HEAD")
    Q_CLASSINFO("AddRecordingLiveStream", "methods=GET,POST,HEAD")
    Q_CLASSINFO("AddVideoLiveStream",     "methods=GET,POST,HEAD")
    Q_CLASSINFO("GetLiveStream",          "methods=GET,POST,HEAD")
    Q_CLASSINFO("GetLiveStreamList",      "methods=GET,POST,HEAD")
    Q_CLASSINFO("StopLiveStream",         "methods=GET,POST,HEAD")
    Q_CLASSINFO("RemoveLiveStream",       "methods=GET,POST,HEAD;name=bool")

    public:

        V2Content();
        ~V2Content() override  = default;
        static void RegisterCustomTypes();

    public slots:

        static QFileInfo    GetFile             ( const QString   &StorageGroup,
                                                  const QString   &FileName );

        static QFileInfo    GetImageFile        ( const QString   &StorageGroup,
                                                  const QString   &FileName,
                                                  int Width, int Height );

        static QStringList  GetFileList         ( const QString   &StorageGroup );

        static QStringList  GetDirList          ( const QString   &StorageGroup );

        static QFileInfo    GetRecordingArtwork ( const QString   &Type,
                                                  const QString   &Inetref,
                                                  int Season, int Width,
                                                  int Height);

        static V2ArtworkInfoList*
                            GetRecordingArtworkList( int              RecordedId,
                                                     int              ChanId,
                                                     const QDateTime &StartTime  );

        static V2ArtworkInfoList*
                            GetProgramArtworkList( const QString &Inetref,
                                                   int            Season  );

        static QFileInfo    GetVideoArtwork     ( const QString   &Type,
                                                  int Id, int Width, int Height );

        static QFileInfo    GetAlbumArt         ( int Id, int Width, int Height );

        static QFileInfo    GetPreviewImage     ( int              RecordedId,
                                                  int              ChanId,
                                                  const QDateTime &StartTime,
                                                  int              Width,
                                                  int              Height,
                                                  int              SecsIn,
                                                  const QString   &Format);

        QFileInfo    GetRecording               ( int              RecordedId,
                                                  int              ChanId,
                                                  const QDateTime &StartTime,
                                                  const QString   &Download );

        static QFileInfo    GetMusic            ( int Id );
        static QFileInfo    GetVideo            ( int Id );

        static QString      GetHash             ( const QString   &StorageGroup,
                                                  const QString   &FileName );

        static bool         DownloadFile        ( const QString   &URL,
                                                  const QString   &StorageGroup );

        // // HTTP Live Streaming
        // V2LiveStreamInfo     *AddLiveStream          ( const QString   &StorageGroup,
        //                                                   const QString   &FileName,
        //                                                   const QString   &HostName,
        //                                                   int              MaxSegments,
        //                                                   int              Width,
        //                                                   int              Height,
        //                                                   int              Bitrate,
        //                                                   int              AudioBitrate,
        //                                                   int              SampleRate );

        // V2LiveStreamInfo     *AddRecordingLiveStream ( int              RecordedId,
        //                                                   int              ChanId,
        //                                                   const QDateTime &StartTime,
        //                                                   int              MaxSegments,
        //                                                   int              Width,
        //                                                   int              Height,
        //                                                   int              Bitrate,
        //                                                   int              AudioBitrate,
        //                                                   int              SampleRate );

        // V2LiveStreamInfo     *AddVideoLiveStream     ( int              Id,
        //                                                   int              MaxSegments,
        //                                                   int              Width,
        //                                                   int              Height,
        //                                                   int              Bitrate,
        //                                                   int              AudioBitrate,
        //                                                   int              SampleRate );

        // V2LiveStreamInfo     *GetLiveStream            ( int Id );
        // V2LiveStreamInfoList *GetLiveStreamList        ( const QString &FileName );

        // V2LiveStreamInfo     *StopLiveStream         ( int Id );
        // bool                     RemoveLiveStream       ( int Id );

  private:
        Q_DISABLE_COPY(V2Content)


};


#endif
