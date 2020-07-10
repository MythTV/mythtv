//////////////////////////////////////////////////////////////////////////////
// Program Name: video.h
// Created     : Apr. 21, 2011
//
// Copyright (c) 2011 Robert McNamara <rmcnamara@mythtv.org>
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

#ifndef VIDEO_H
#define VIDEO_H

#include <QScriptEngine>

#include "videometadatalistmanager.h"

#include "services/videoServices.h"

class Video : public VideoServices
{
    Q_OBJECT

    public:

        Q_INVOKABLE explicit Video( QObject */*parent*/ = nullptr ) {}

    public:

        /* Video Metadata Methods */

        DTC::VideoMetadataInfoList*  GetVideoList    ( const QString  &Folder,
                                                       const QString  &Sort,
                                                       bool           Descending,
                                                       int            StartIndex,
                                                       int            Count      ) override; // VideoServices

        DTC::VideoMetadataInfo*   GetVideo           ( int      Id               ) override; // VideoServices

        DTC::VideoMetadataInfo*   GetVideoByFileName ( const QString  &FileName  ) override; // VideoServices

        DTC::VideoLookupList*     LookupVideo        ( const QString    &Title,
                                                       const QString    &Subtitle,
                                                       const QString    &Inetref,
                                                       int              Season,
                                                       int              Episode,
                                                       const QString    &GrabberType,
                                                       bool             AllowGeneric ) override; // VideoServices

        bool                      RemoveVideoFromDB  ( int      Id               ) override; // VideoServices

        bool                      AddVideo           ( const QString  &FileName,
                                                       const QString  &HostName  ) override; // VideoServices

        bool                      UpdateVideoWatchedStatus ( int  Id,
                                                             bool Watched ) override; // VideoServices

        bool                      UpdateVideoMetadata      ( int           Id,
                                                             const QString &Title,
                                                             const QString &SubTitle,
                                                             const QString &TagLine,
                                                             const QString &Director,
                                                             const QString &Studio,
                                                             const QString &Plot,
                                                             const QString &Rating,
                                                             const QString &Inetref,
                                                             int           CollectionRef,
                                                             const QString &HomePage,
                                                             int           Year,
                                                             const QDate   &ReleaseDate,
                                                             float         UserRating,
                                                             int           Length,
                                                             int           PlayCount,
                                                             int           Season,
                                                             int           Episode,
                                                             int           ShowLevel,
                                                             const QString &FileName,
                                                             const QString &Hash,
                                                             const QString &CoverFile,
                                                             int           ChildID,
                                                             bool          Browse,
                                                             bool          Watched,
                                                             bool          Processed,
                                                             const QString &PlayCommand,
                                                             int           Category,
                                                             const QString &Trailer,
                                                             const QString &Host,
                                                             const QString &Screenshot,
                                                             const QString &Banner,
                                                             const QString &Fanart,
                                                             const QDate   &InsertDate,
                                                             const QString &ContentType,
                                                             const QString &Genres,
                                                             const QString &Cast,
                                                             const QString &Countries
        ) override; // VideoServices

        long                      GetSavedBookmark (         int   Id ) override;

        bool                      SetSavedBookmark (         int   Id,
                                                             long  Offset ) override;

        /* Bluray Methods */

        DTC::BlurayInfo*          GetBluray          ( const QString  &Path      ) override; // VideoServices

        DTC::VideoStreamInfoList* GetStreamInfo ( const QString &StorageGroup,
                                                  const QString &FileName  ) override;  // VideoServices
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

class ScriptableVideo : public QObject
{
    Q_OBJECT

    private:

        Video          m_obj;
        QScriptEngine *m_pEngine;

    public:

        Q_INVOKABLE explicit ScriptableVideo( QScriptEngine *pEngine, QObject *parent = nullptr ) : QObject( parent )
        {
            m_pEngine = pEngine;
        }

    public slots:

        QObject* GetVideoList(       const QString    &Folder,
                                     const QString    &Sort,
                                     bool             Descending,
                                     int              StartIndex,
                                     int              Count      )
        {
            SCRIPT_CATCH_EXCEPTION( nullptr,
                return m_obj.GetVideoList( Folder, Sort, Descending,
                                           StartIndex, Count );
            )
        }

        QObject* GetVideo( int  Id )
        {
            SCRIPT_CATCH_EXCEPTION( nullptr,
                return m_obj.GetVideo( Id );
            )
        }

        QObject* GetVideoByFileName( const QString    &FileName  )
        {
            SCRIPT_CATCH_EXCEPTION( nullptr,
                return m_obj.GetVideoByFileName( FileName );
            )
        }

        QObject* LookupVideo( const QString    &Title,
                              const QString    &Subtitle,
                              const QString    &Inetref,
                              int              Season,
                              int              Episode,
                              const QString    &GrabberType,
                              bool             AllowGeneric )
        {
            SCRIPT_CATCH_EXCEPTION( nullptr,
                return m_obj.LookupVideo( Title, Subtitle, Inetref,
                                          Season, Episode, GrabberType,
                                          AllowGeneric );
            )
        }

        bool RemoveVideoFromDB( int Id )
        {
            SCRIPT_CATCH_EXCEPTION( false,
                return m_obj.RemoveVideoFromDB( Id );
            )
        }

        bool AddVideo( const QString  &FileName,
                       const QString  &HostName  )
        {
            SCRIPT_CATCH_EXCEPTION( false,
                return m_obj.AddVideo( FileName, HostName );
            )
        }

        bool UpdateVideoMetadata( int           Id,
                                  const QString &Title,
                                  const QString &SubTitle,
                                  const QString &TagLine,
                                  const QString &Director,
                                  const QString &Studio,
                                  const QString &Plot,
                                  const QString &Rating,
                                  const QString &Inetref,
                                  int           CollectionRef,
                                  const QString &HomePage,
                                  int           Year,
                                  const QDate   &ReleaseDate,
                                  float         UserRating,
                                  int           Length,
                                  int           PlayCount,
                                  int           Season,
                                  int           Episode,
                                  int           ShowLevel,
                                  const QString &FileName,
                                  const QString &Hash,
                                  const QString &CoverFile,
                                  int           ChildID,
                                  bool          Browse,
                                  bool          Watched,
                                  bool          Processed,
                                  const QString &PlayCommand,
                                  int           Category,
                                  const QString &Trailer,
                                  const QString &Host,
                                  const QString &Screenshot,
                                  const QString &Banner,
                                  const QString &Fanart,
                                  const QDate   &InsertDate,
                                  const QString &ContentType,
                                  const QString &Genres,
                                  const QString &Cast,
                                  const QString &Countries
                                )
        {
            SCRIPT_CATCH_EXCEPTION( false,
                return m_obj.UpdateVideoMetadata( Id,
                                                  Title,
                                                  SubTitle,
                                                  TagLine,
                                                  Director,
                                                  Studio,
                                                  Plot,
                                                  Rating,
                                                  Inetref,
                                                  CollectionRef,
                                                  HomePage,
                                                  Year,
                                                  ReleaseDate,
                                                  UserRating,
                                                  Length,
                                                  PlayCount,
                                                  Season,
                                                  Episode,
                                                  ShowLevel,
                                                  FileName,
                                                  Hash,
                                                  CoverFile,
                                                  ChildID,
                                                  Browse,
                                                  Watched,
                                                  Processed,
                                                  PlayCommand,
                                                  Category,
                                                  Trailer,
                                                  Host,
                                                  Screenshot,
                                                  Banner,
                                                  Fanart,
                                                  InsertDate,
                                                  ContentType,
                                                  Genres,
                                                  Cast,
                                                  Countries);
            )
        }
};

// NOLINTNEXTLINE(modernize-use-auto)
Q_SCRIPT_DECLARE_QMETAOBJECT_MYTHTV( ScriptableVideo, QObject*);

#endif
