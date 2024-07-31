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

#include "libmythbase/mythconfig.h"
#if CONFIG_QTSCRIPT
#include <QScriptEngine>
#endif

#include "libmythservicecontracts/services/contentServices.h"

class Content : public ContentServices
{
    Q_OBJECT

    public:

        Q_INVOKABLE explicit Content( QObject */*parent*/ = nullptr ) {}

    public:

        QFileInfo           GetFile             ( const QString   &StorageGroup,
                                                  const QString   &FileName ) override; // ContentServices

        QFileInfo           GetImageFile        ( const QString   &StorageGroup,
                                                  const QString   &FileName,
                                                  int Width, int Height ) override; // ContentServices

        QStringList         GetFileList         ( const QString   &StorageGroup ) override; // ContentServices

        QStringList         GetDirList          ( const QString   &StorageGroup ) override; // ContentServices

        QFileInfo           GetRecordingArtwork ( const QString   &Type,
                                                  const QString   &Inetref,
                                                  int Season, int Width,
                                                  int Height) override; // ContentServices

        DTC::ArtworkInfoList*
                            GetRecordingArtworkList( int              RecordedId,
                                                     int              ChanId,
                                                     const QDateTime &recstarttsRaw  ) override; // ContentServices

        DTC::ArtworkInfoList*
                            GetProgramArtworkList( const QString &Inetref,
                                                   int            Season  ) override; // ContentServices

        QFileInfo           GetVideoArtwork     ( const QString   &Type,
                                                  int Id, int Width, int Height ) override; // ContentServices

        QFileInfo           GetAlbumArt         ( int Id, int Width, int Height ) override; // ContentServices

        QFileInfo           GetPreviewImage     ( int              RecordedId,
                                                  int              ChanId,
                                                  const QDateTime &recstarttsRaw,
                                                  int              Width,
                                                  int              Height,
                                                  int              SecsIn,
                                                  const QString   &Format) override; // ContentServices

        QFileInfo           GetRecording        ( int              RecordedId,
                                                  int              ChanId,
                                                  const QDateTime &recstarttsRaw ) override; // ContentServices

        QFileInfo           GetMusic            ( int Id ) override; // ContentServices
        QFileInfo           GetVideo            ( int Id ) override; // ContentServices

        QString             GetHash             ( const QString   &StorageGroup,
                                                  const QString   &FileName ) override; // ContentServices

        bool                DownloadFile        ( const QString   &URL,
                                                  const QString   &StorageGroup ) override; // ContentServices

        // HTTP Live Streaming
        DTC::LiveStreamInfo     *AddLiveStream          ( const QString   &StorageGroup,
                                                          const QString   &FileName,
                                                          const QString   &HostName,
                                                          int              MaxSegments,
                                                          int              Width,
                                                          int              Height,
                                                          int              Bitrate,
                                                          int              AudioBitrate,
                                                          int              SampleRate ) override; // ContentServices

        DTC::LiveStreamInfo     *AddRecordingLiveStream ( int              RecordedId,
                                                          int              ChanId,
                                                          const QDateTime &recstarttsRaw,
                                                          int              MaxSegments,
                                                          int              Width,
                                                          int              Height,
                                                          int              Bitrate,
                                                          int              AudioBitrate,
                                                          int              SampleRate ) override; // ContentServices

        DTC::LiveStreamInfo     *AddVideoLiveStream     ( int              Id,
                                                          int              MaxSegments,
                                                          int              Width,
                                                          int              Height,
                                                          int              Bitrate,
                                                          int              AudioBitrate,
                                                          int              SampleRate ) override; // ContentServices

        DTC::LiveStreamInfo     *GetLiveStream            ( int Id ) override; // ContentServices
        DTC::LiveStreamInfoList *GetLiveStreamList        ( const QString &FileName ) override; // ContentServices

        DTC::LiveStreamInfo     *StopLiveStream         ( int Id ) override; // ContentServices
        bool                     RemoveLiveStream       ( int Id ) override; // ContentServices
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

#if CONFIG_QTSCRIPT
class ScriptableContent : public QObject
{
    Q_OBJECT

    private:

        Content  m_obj;
        QScriptEngine *m_pEngine;

    public:

        Q_INVOKABLE explicit ScriptableContent( QScriptEngine *pEngine, QObject *parent = nullptr )
          : QObject( parent ), m_pEngine(pEngine)
        {
        }

    public slots:

        QObject* GetRecordingArtworkList( int RecordedId )
        {
            SCRIPT_CATCH_EXCEPTION( nullptr,
                return m_obj.GetRecordingArtworkList( RecordedId, 0, QDateTime() );
            )
        }

        QObject* GetProgramArtworkList( const QString &Inetref,
                                              int      Season  )
        {
            SCRIPT_CATCH_EXCEPTION( nullptr,
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
            SCRIPT_CATCH_EXCEPTION( nullptr,
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
            SCRIPT_CATCH_EXCEPTION( nullptr,
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
            SCRIPT_CATCH_EXCEPTION( nullptr,
                return m_obj.AddVideoLiveStream(Id, MaxSegments, Width, Height,
                                            Bitrate, AudioBitrate, SampleRate);
            )
        }

        QObject* GetLiveStream( int Id )
        {
            SCRIPT_CATCH_EXCEPTION( nullptr,
                return m_obj.GetLiveStream( Id );
            )
        }

        QObject* GetLiveStreamList( const QString &FileName )
        {
            SCRIPT_CATCH_EXCEPTION( nullptr,
                return m_obj.GetLiveStreamList( FileName );
            )
        }

        QObject* StopLiveStream( int Id )
        {
            SCRIPT_CATCH_EXCEPTION( nullptr,
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

// NOLINTNEXTLINE(modernize-use-auto)
Q_SCRIPT_DECLARE_QMETAOBJECT_MYTHTV( ScriptableContent, QObject*);
#endif

#endif
