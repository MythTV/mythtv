//////////////////////////////////////////////////////////////////////////////
// Program Name: video.h
// Created     : Apr. 21, 2011
//
// Copyright (c) 2011 Robert McNamara <rmcnamara@mythtv.org>
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or at your option any later version of the LGPL.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef VIDEO_H
#define VIDEO_H

#include <QScriptEngine>
#include <QDateTime>

#include "videometadatalistmanager.h"

#include "services/videoServices.h"

class Video : public VideoServices
{
    Q_OBJECT

    public:

        Q_INVOKABLE Video( QObject *parent = 0 ) {}

    public:

        /* Video Metadata Methods */

        DTC::VideoMetadataInfoList*  GetVideos       ( bool     Descending,
                                                       int      StartIndex,
                                                       int      Count      );

        DTC::VideoMetadataInfo*   GetVideoById       ( int      Id         );

        DTC::VideoMetadataInfo*   GetVideoByFilename ( const QString  &Filename  );

        bool                      RemoveVideoFromDB  ( int      Id         );

    private:

        DTC::VideoMetadataInfo*   GetInfoFromMetadata(
                             VideoMetadataListManager::VideoMetadataPtr metadata );
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

        QObject* GetVideos(          bool             Descending,
                                     int              StartIndex,
                                     int              Count      )
        {
            return m_obj.GetVideos( Descending, StartIndex, Count );
        }

        QObject* GetVideoById(       int              Id         )
        {
            return m_obj.GetVideoById( Id );
        }

        QObject* GetVideoByFilename( const QString    &Filename  )
        {
            return m_obj.GetVideoByFilename( Filename );
        }

        bool RemoveVideoFromDB(      int              Id         )
        {
            return m_obj.RemoveVideoFromDB( Id );
        }

};


Q_SCRIPT_DECLARE_QMETAOBJECT( ScriptableVideo, QObject*);

#endif
