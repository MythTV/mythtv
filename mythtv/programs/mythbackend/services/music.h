//////////////////////////////////////////////////////////////////////////////
// Program Name: music.h
// Created     : July 20, 2017
//
// Copyright (c) 2017 Paul Harrison <pharrison@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef MUSIC_H
#define MUSIC_H

#include <QScriptEngine>
#include "services/musicServices.h"

class Music : public MusicServices
{
    Q_OBJECT

    public:

        Q_INVOKABLE explicit Music( QObject */*parent*/ = 0 ) {}

    public:

        /* Music Metadata Methods */

        DTC::MusicMetadataInfoList*  GetTrackList    ( int      StartIndex,
                                                       int      Count      );

        DTC::MusicMetadataInfo*      GetTrack        ( int      Id               );


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

class ScriptableMusic : public QObject
{
    Q_OBJECT

    private:

        Music          m_obj;
        QScriptEngine *m_pEngine;

    public:

        Q_INVOKABLE ScriptableMusic( QScriptEngine *pEngine, QObject *parent = 0 ) : QObject( parent )
        {
            m_pEngine = pEngine;
        }

    public slots:

        QObject* GetTrackList(       int              StartIndex,
                                     int              Count      )
        {
            SCRIPT_CATCH_EXCEPTION( NULL,
                return m_obj.GetTrackList(StartIndex, Count );
            )
        }

        QObject* GetTrack( int  Id )
        {
            SCRIPT_CATCH_EXCEPTION( NULL,
                return m_obj.GetTrack( Id );
            )
        }
};

Q_SCRIPT_DECLARE_QMETAOBJECT_MYTHTV( ScriptableMusic, QObject*);

#endif
