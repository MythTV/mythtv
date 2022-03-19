//////////////////////////////////////////////////////////////////////////////
// Program Name: image.h
// Created     : Jul. 27, 2012
//
// Copyright (c) 2012 Robert Siebert <trebor_s@web.de>
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

#ifndef IMAGE_H
#define IMAGE_H

#include "mythconfig.h"
#if CONFIG_QTSCRIPT
#include <QScriptEngine>
#endif
#include "services/imageServices.h"
#include "imagemanager.h"


class Image : public ImageServices
{
    Q_OBJECT

public:
    Q_INVOKABLE explicit Image( QObject */*parent*/ = nullptr ) {}

public:
    QString                     GetImageInfo                ( int   id,
                                                              const QString &tag ) override; // ImageServices

    DTC::ImageMetadataInfoList* GetImageInfoList            ( int   id ) override; // ImageServices

    bool                        RemoveImage        ( int   id ) override; // ImageServices
    bool                        RenameImage        ( int   id,
                                                     const QString &newName ) override; // ImageServices

    bool                        StartSync          ( void ) override; // ImageServices
    bool                        StopSync           ( void ) override; // ImageServices
    DTC::ImageSyncInfo*         GetSyncStatus      ( void ) override; // ImageServices

    bool                        CreateThumbnail    (int   id) override; // ImageServices

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
class ScriptableImage : public QObject
{
    Q_OBJECT

    private:

        Image          m_obj;
        QScriptEngine *m_pEngine;

    public:

        Q_INVOKABLE explicit ScriptableImage( QScriptEngine *pEngine, QObject *parent = nullptr ) : QObject( parent )
        {
            m_pEngine = pEngine;
        }

    public slots:

        QString GetImageInfo( int   Id,
                              const QString &Tag )
        {
            SCRIPT_CATCH_EXCEPTION( QString(),
                return m_obj.GetImageInfo( Id, Tag );
            )
        }

        QObject* GetImageInfoList( int   Id )
        {
            SCRIPT_CATCH_EXCEPTION( nullptr,
                return m_obj.GetImageInfoList( Id );
            )
        }

        bool RemoveImage( int Id )
        {
            SCRIPT_CATCH_EXCEPTION( false,
                return m_obj.RemoveImage( Id );
            )
        }

        bool RenameImage( int   Id,
                          const QString &NewName )
        {
            SCRIPT_CATCH_EXCEPTION( false,
                return m_obj.RenameImage( Id, NewName );
            )
        }

        bool StartSync( void )
        {
            SCRIPT_CATCH_EXCEPTION( false,
                return m_obj.StartSync();
            )
        }

        bool StopSync( void )
        {
            SCRIPT_CATCH_EXCEPTION( false,
                return m_obj.StopSync();
            )
        }

        QObject* GetSyncStatus( void )
        {
            SCRIPT_CATCH_EXCEPTION( nullptr,
                return m_obj.GetSyncStatus();
            )
        }

        bool CreateThumbnail    ( int   Id )
        {
            SCRIPT_CATCH_EXCEPTION( false,
                return m_obj.CreateThumbnail( Id );
            )
        }
};

// NOLINTNEXTLINE(modernize-use-auto)
Q_SCRIPT_DECLARE_QMETAOBJECT_MYTHTV( ScriptableImage, QObject*)
#endif

#endif
