//////////////////////////////////////////////////////////////////////////////
// Program Name: content.h
// Created     : Mar. 7, 2011
//
// Copyright (c) 2011 David Blain <dblain@mythtv.org>
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

        QStringList         GetFileList         ( const QString   &StorageGroup );

        QFileInfo           GetVideoArt         ( int Id );

        QFileInfo           GetAlbumArt         ( int Id, int Width, int Height );

        QFileInfo           GetPreviewImage     ( int              ChanId,
                                                  const QDateTime &StartTime,
                                                  int              Width,    
                                                  int              Height,   
                                                  int              SecsIn );

        QFileInfo           GetRecording        ( int              ChanId,
                                                  const QDateTime &StartTime );

        QFileInfo           GetMusic            ( int Id );
        QFileInfo           GetVideo            ( int Id );

        QString             GetHash             ( const QString   &storageGroup,
                                                  const QString   &FileName );
};

Q_SCRIPT_DECLARE_QMETAOBJECT( Content, QObject*);

#endif


