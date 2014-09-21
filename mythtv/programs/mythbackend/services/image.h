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

#include <QScriptEngine>
#include "services/imageServices.h"



class Image : public ImageServices
{
    Q_OBJECT

public:
    Q_INVOKABLE Image( QObject *parent = 0 ) {}

public:
    bool                        SetImageInfo                ( int   Id,
                                                              const QString &Tag,
                                                              const QString &Value );

    bool                        SetImageInfoByFileName      ( const QString &FileName,
                                                              const QString &Tag,
                                                              const QString &Value );

    QString                     GetImageInfo                ( int   Id,
                                                              const QString &Tag );

    QString                     GetImageInfoByFileName      ( const QString &FileName,
                                                              const QString &Tag );

    DTC::ImageMetadataInfoList* GetImageInfoList            ( int   Id );

    DTC::ImageMetadataInfoList* GetImageInfoListByFileName  ( const QString &FileName );

    bool                        RemoveImageFromDB  ( int   Id );
    bool                        RemoveImage        ( int   Id );

    bool                        StartSync          ( void );
    bool                        StopSync           ( void );
    DTC::ImageSyncInfo*         GetSyncStatus      ( void );

    bool                        StartThumbnailGeneration ( void );
    bool                        StopThumbnailGeneration  ( void );
    bool                        CreateThumbnail    ( int   Id );
    bool                        RecreateThumbnail  ( int   Id );
    bool                        SetThumbnailSize   ( int   Width,
                                                     int   Height);
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

class ScriptableImage : public QObject
{
    Q_OBJECT

    private:

        Image          m_obj;
        QScriptEngine *m_pEngine;

    public:

        Q_INVOKABLE ScriptableImage( QScriptEngine *pEngine, QObject *parent = 0 ) : QObject( parent )
        {
            m_pEngine = pEngine;
        }

    public slots:

        bool SetImageInfo ( int   Id,
                            const QString &Tag,
                            const QString &Value )
        {
            SCRIPT_CATCH_EXCEPTION( false,
                return m_obj.SetImageInfo( Id, Tag, Value );
            )
        }

        bool SetImageInfoByFileName ( const QString &FileName,
                                      const QString &Tag,
                                      const QString &Value )
        {
            SCRIPT_CATCH_EXCEPTION( false,
                return m_obj.SetImageInfoByFileName( FileName,
                                                     Tag,
                                                     Value );
            )
        }

        QString GetImageInfo( int   Id,
                              const QString &Tag )
        {
            SCRIPT_CATCH_EXCEPTION( QString(),
                return m_obj.GetImageInfo( Id, Tag );
            )
        }

        QString GetImageInfoByFileName( const QString &FileName,
                                        const QString &Tag )
        {
            SCRIPT_CATCH_EXCEPTION( QString(),
                return m_obj.GetImageInfoByFileName( FileName, Tag );
            )
        }

        QObject* GetImageInfoList( int   Id )
        {
            SCRIPT_CATCH_EXCEPTION( NULL,
                return m_obj.GetImageInfoList( Id );
            )
        }

        QObject* GetImageInfoListByFileName ( const QString &FileName )
        {
            SCRIPT_CATCH_EXCEPTION( NULL,
                return m_obj.GetImageInfoListByFileName( FileName );
            )
        }

        bool RemoveImageFromDB( int Id )
        {
            SCRIPT_CATCH_EXCEPTION( false,
                return m_obj.RemoveImageFromDB( Id );
            )
        }

        bool RemoveImage( int Id )
        {
            SCRIPT_CATCH_EXCEPTION( false,
                return m_obj.RemoveImage( Id );
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
            SCRIPT_CATCH_EXCEPTION( NULL,
                return m_obj.GetSyncStatus();
            )
        }

        bool StartThumbnailGeneration ( void )
        {
            SCRIPT_CATCH_EXCEPTION( false,
                return m_obj.StartThumbnailGeneration();
            )
        }

        bool StopThumbnailGeneration  ( void )
        {
            SCRIPT_CATCH_EXCEPTION( false,
                return m_obj.StopThumbnailGeneration();
            )
        }

        bool CreateThumbnail    ( int   Id )
        {
            SCRIPT_CATCH_EXCEPTION( false,
                return m_obj.CreateThumbnail( Id );
            )
        }

        bool RecreateThumbnail  ( int   Id )
        {
            SCRIPT_CATCH_EXCEPTION( false,
                return m_obj.RecreateThumbnail( Id );
            )
        }

        bool SetThumbnailSize   ( int   Width,
                                  int   Height)
        {
            SCRIPT_CATCH_EXCEPTION( false,
                return m_obj.SetThumbnailSize( Width, Height );
            )
        }

};

Q_SCRIPT_DECLARE_QMETAOBJECT_MYTHTV( ScriptableImage, QObject*)

#endif
