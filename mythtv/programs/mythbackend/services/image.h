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
};

Q_SCRIPT_DECLARE_QMETAOBJECT( Image, QObject*)

#endif
