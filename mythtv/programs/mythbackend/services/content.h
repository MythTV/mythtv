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

#ifndef CONTENT_H
#define CONTENT_H

#include <QScriptEngine>

#include "services/contentServices.h"

class Content : public ContentServices
{
    Q_OBJECT

    public:

        Q_INVOKABLE explicit Content( QObject *parent = 0 ) {}

    public:

        QFileInfo           GetFile             ( const QString   &StorageGroup,
                                                  const QString   &FileName );

        QFileInfo           GetImageFile        ( const QString   &StorageGroup,
                                                  const QString   &FileName,
                                                  int Width, int Height );

        QStringList         GetFileList         ( const QString   &StorageGroup );

        QStringList         GetDirList          ( const QString   &StorageGroup );

        QFileInfo           GetRecordingArtwork ( const QString   &Type,
                                                  const QString   &Inetref,
                                                  int Season, int Width,
                                                  int Height);

        DTC::ArtworkInfoList*
                            GetRecordingArtworkList( int              RecordedId,
                                                     int              ChanId,
                                                     const QDateTime &StartTime  );

        DTC::ArtworkInfoList*
                            GetProgramArtworkList( const QString &Inetref,
                                                   int            Season  );

        QFileInfo           GetVideoArtwork     ( const QString   &Type,
                                                  int Id, int Width, int Height );

        QFileInfo           GetAlbumArt         ( int Id, int Width, int Height );

        QFileInfo           GetPreviewImage     ( int              RecordedId,
                                                  int              ChanId,
                                                  const QDateTime &StartTime,
                                                  int              Width,
                                                  int              Height,
                                                  int              SecsIn,
                                                  const QString   &Format);

        QFileInfo           GetRecording        ( int              RecordedId,
                                                  int              ChanId,
                                                  const QDateTime &StartTime );

        QFileInfo           GetMusic            ( int Id );
        QFileInfo           GetVideo            ( int Id );

        QString             GetHash             ( const QString   &StorageGroup,
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

        DTC::LiveStreamInfo     *AddRecordingLiveStream ( int              RecordedId,
                                                          int              ChanId,
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
        DTC::LiveStreamInfoList *GetLiveStreamList        ( const QString &FileName );

        DTC::LiveStreamInfo     *StopLiveStream         ( int Id );
        bool                     RemoveLiveStream       ( int Id );
};

// --------------------------------------------------------------------------
// The following class wrapper is due to a limitation in Qt Script Engine.  It
// requires all methods that return pointers to user classes that are derived from
// QObject actually return QObject* (not the user class *).  If the user class pointer
// is returned, the script engine treats it as a QVariant and doesn't create a
// javascript prototype wrapper for it.
//
// This class allows us to keep the rich return types in the main API class while
// offering the script engine a class it can work with.
//
// Only API Classes that return custom classes needs to implement these wrappers.
//
// We should continue to look for a cleaning solution to this problem.
// --------------------------------------------------------------------------

class ScriptableContent : public QObject
{
    Q_OBJECT

    private:

        Content  m_obj;
        QScriptEngine *m_pEngine;

    public:

        Q_INVOKABLE ScriptableContent( QScriptEngine *pEngine, QObject *parent = 0 ) : QObject( parent )
        {
            m_pEngine = pEngine;
        }

    public slots:

        QObject* GetRecordingArtworkList( int RecordedId )
        {
            SCRIPT_CATCH_EXCEPTION( NULL,
                return m_obj.GetRecordingArtworkList( RecordedId, 0, QDateTime() );
            )
        }

        QObject* GetProgramArtworkList( const QString &Inetref,
                                              int      Season  )
        {
            SCRIPT_CATCH_EXCEPTION( NULL,
                return m_obj.GetProgramArtworkList( Inetref, Season );
            )
        }

        QString GetHash ( const QString   &StorageGroup,
                          const QString   &FileName )
        {
            SCRIPT_CATCH_EXCEPTION( QString(),
                return m_obj.GetHash( StorageGroup, FileName );
            )
        }

        // HTTP Live Streaming
        QObject* AddLiveStream ( const QString   &StorageGroup,
                                 const QString   &FileName,
                                 const QString   &HostName,
                                 int              MaxSegments,
                                 int              Width,
                                 int              Height,
                                 int              Bitrate,
                                 int              AudioBitrate,
                                 int              SampleRate )
        {
            SCRIPT_CATCH_EXCEPTION( NULL,
                return m_obj.AddLiveStream(StorageGroup, FileName, HostName,
                                       MaxSegments, Width, Height, Bitrate,
                                       AudioBitrate, SampleRate);
            )
        }

        QObject* AddRecordingLiveStream (int              RecordedId,
                                         int              MaxSegments,
                                         int              Width,
                                         int              Height,
                                         int              Bitrate,
                                         int              AudioBitrate,
                                         int              SampleRate )
        {
            SCRIPT_CATCH_EXCEPTION( NULL,
                return m_obj.AddRecordingLiveStream(RecordedId, 0, QDateTime(),
                                                MaxSegments,
                                                Width, Height, Bitrate,
                                                AudioBitrate, SampleRate);
            )
        }

        QObject* AddVideoLiveStream( int              Id,
                                     int              MaxSegments,
                                     int              Width,
                                     int              Height,
                                     int              Bitrate,
                                     int              AudioBitrate,
                                     int              SampleRate )
        {
            SCRIPT_CATCH_EXCEPTION( NULL,
                return m_obj.AddVideoLiveStream(Id, MaxSegments, Width, Height,
                                            Bitrate, AudioBitrate, SampleRate);
            )
        }

        QObject* GetLiveStream( int Id )
        {
            SCRIPT_CATCH_EXCEPTION( NULL,
                return m_obj.GetLiveStream( Id );
            )
        }

        QObject* GetLiveStreamList( const QString &FileName )
        {
            SCRIPT_CATCH_EXCEPTION( NULL,
                return m_obj.GetLiveStreamList( FileName );
            )
        }

        QObject* StopLiveStream( int Id )
        {
            SCRIPT_CATCH_EXCEPTION( NULL,
                return m_obj.StopLiveStream(Id);
            )
        }

        bool RemoveLiveStream( int Id )
        {
            SCRIPT_CATCH_EXCEPTION( false,
                return m_obj.RemoveLiveStream(Id);
            )
        }
};

Q_SCRIPT_DECLARE_QMETAOBJECT_MYTHTV( ScriptableContent, QObject*);

#endif
