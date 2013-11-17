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

        Q_INVOKABLE Content( QObject *parent = 0 ) {}

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
                                                  int              SecsIn,
                                                  const QString   &Format);

        QFileInfo           GetRecording        ( int              ChanId,
                                                  const QDateTime &StartTime );

        QFileInfo           GetMusic            ( int Id );
        QFileInfo           GetVideo            ( int Id );

        QString             GetHash             ( const QString   &StorageGroup,
                                                  const QString   &FileName );

        bool                DownloadFile        ( const QString   &URL,
                                                  const QString   &StorageGroup );

        bool                DeleteFile          ( const QString   &StorageGroup,
                                                  const QString   &FileName );

        bool                RenameFile          ( const QString   &StorageGroup,
                                                  const QString   &FileName,
                                                  const QString   &NewName );

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

    public:

        Q_INVOKABLE ScriptableContent( QObject *parent = 0 ) : QObject( parent ) {}

    public slots:
        QObject* GetRecordingArtworkList(       int        ChanId,
                                          const QDateTime &StartTime  )
        {
            return m_obj.GetRecordingArtworkList( ChanId, StartTime );
        }

        QObject* GetProgramArtworkList( const QString &Inetref,
                                              int      Season  )
        {
            return m_obj.GetProgramArtworkList( Inetref, Season );
        }

        QString GetHash ( const QString   &StorageGroup,
                          const QString   &FileName )
        {
            return m_obj.GetHash( StorageGroup, FileName );
        }

        QObject* GetLiveStream(      int              Id )
        {
            return m_obj.GetLiveStream( Id );
        }

        QObject* GetLiveStreamList(  const QString &FileName )
        {
            return m_obj.GetLiveStreamList( FileName );
        }
};

Q_SCRIPT_DECLARE_QMETAOBJECT( ScriptableContent, QObject*);

#endif


