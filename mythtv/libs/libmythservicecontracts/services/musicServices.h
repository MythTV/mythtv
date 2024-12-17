//////////////////////////////////////////////////////////////////////////////
// Program Name: musicServices.h
// Created     : July 20, 2017
//
// Purpose - Imported Music Services API Interface definition
//
// Copyright (c) 2017 Paul Harrison <pharrison@mythtv.org>
//
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef MUSICSERVICES_H_
#define MUSICSERVICES_H_

#include <QFileInfo>
#include <QStringList>

#include "libmythservicecontracts/service.h"

#include "libmythservicecontracts/datacontracts/musicMetadataInfoList.h"

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// Notes -
//
//  * This implementation can't handle declared default parameters
//
//  * When called, any missing params are sent default values for its datatype
//
//  * Q_CLASSINFO( "<methodName>_Method", ...) is used to determine HTTP method
//    type.  Defaults to "BOTH", available values:
//          "GET", "POST" or "BOTH"
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class SERVICE_PUBLIC MusicServices : public Service  //, public QScriptable ???
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.0" );

    public:

        // Must call InitializeCustomTypes for each unique Custom Type used
        // in public slots below.

        MusicServices( QObject *parent = nullptr ) : Service( parent )
        {
            DTC::MusicMetadataInfoList::InitializeCustomTypes();
        }

    public slots:

        // Music Metadata

        virtual DTC::MusicMetadataInfoList* GetTrackList       ( int              StartIndex,
                                                                 int              Count      ) = 0;

        virtual DTC::MusicMetadataInfo*     GetTrack           ( int              Id         ) = 0;
};

#endif
