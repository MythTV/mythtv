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

        Q_INVOKABLE Video( QObject *parent = 0 ) {}

    public:

        /* Video Metadata Methods */

        DTC::VideoMetadataInfoList*  GetVideoList    ( bool     Descending,
                                                       int      StartIndex,
                                                       int      Count            );

        DTC::VideoMetadataInfo*   GetVideo           ( int      Id               );

        DTC::VideoMetadataInfo*   GetVideoByFileName ( const QString  &FileName  );

        DTC::VideoLookupList*     LookupVideo        ( const QString    &Title,
                                                       const QString    &Subtitle,
                                                       const QString    &Inetref,
                                                       int              Season,
                                                       int              Episode,
                                                       const QString    &GrabberType,
                                                       bool             AllowGeneric );

        bool                      RemoveVideoFromDB  ( int      Id               );

        bool                      AddVideo           ( const QString  &FileName,
                                                       const QString  &HostName  );

        /* Bluray Methods */

        DTC::BlurayInfo*          GetBluray          ( const QString  &Path      );

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

        Video    m_obj;

    public:

        Q_INVOKABLE ScriptableVideo( QObject *parent = 0 ) : QObject( parent ) {}

    public slots:

        QObject* GetVideoList(          bool             Descending,
                                     int              StartIndex,
                                     int              Count      )
        {
            return m_obj.GetVideoList( Descending, StartIndex, Count );
        }

        QObject* GetVideo(       int              Id         )
        {
            return m_obj.GetVideo( Id );
        }

        QObject* GetVideoByFileName( const QString    &FileName  )
        {
            return m_obj.GetVideoByFileName( FileName );
        }

        QObject* LookupVideo( const QString    &Title,
                              const QString    &Subtitle,
                              const QString    &Inetref,
                              int              Season,
                              int              Episode,
                              const QString    &GrabberType,
                              bool             AllowGeneric )
        {
            return m_obj.LookupVideo( Title, Subtitle, Inetref,
                                      Season, Episode, GrabberType,
                                      AllowGeneric );
        }

        bool RemoveVideoFromDB(      int              Id         )
        {
            return m_obj.RemoveVideoFromDB( Id );
        }

        bool AddVideo( const QString  &FileName,
                       const QString  &HostName  )
        {
            return m_obj.AddVideo( FileName, HostName );
        }
};


Q_SCRIPT_DECLARE_QMETAOBJECT( ScriptableVideo, QObject*);

#endif
