//////////////////////////////////////////////////////////////////////////////
// Program Name: contentServices.h
// Created     : Apr. 21, 2011
//
// Purpose - Imported Video Services API Interface definition
//
// Copyright (c) 2011 Robert McNamara <rmcnamara@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef VIDEOSERVICES_H_
#define VIDEOSERVICES_H_

#include <QFileInfo>
#include <QStringList>

#include "service.h"

#include "datacontracts/videoMetadataInfoList.h"
#include "datacontracts/videoLookupInfoList.h"
#include "datacontracts/blurayInfo.h"
#include "datacontracts/videoStreamInfoList.h"

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

class SERVICE_PUBLIC VideoServices : public Service  //, public QScriptable ???
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.5" );
    Q_CLASSINFO( "AddVideo_Method",                    "POST" )
    Q_CLASSINFO( "RemoveVideoFromDB_Method",           "POST" )
    Q_CLASSINFO( "UpdateVideoWatchedStatus_Method",    "POST" )
    Q_CLASSINFO( "UpdateVideoMetadata_Method",         "POST" )
    Q_CLASSINFO( "SetSavedBookmark_Method",            "POST" )

    public:

        // Must call InitializeCustomTypes for each unique Custom Type used
        // in public slots below.

        VideoServices( QObject *parent = nullptr ) : Service( parent )
        {
            DTC::VideoMetadataInfoList::InitializeCustomTypes();
            DTC::VideoLookupList::InitializeCustomTypes();
            DTC::BlurayInfo::InitializeCustomTypes();
            DTC::VideoStreamInfoList::InitializeCustomTypes();
        }

    public slots:

        // Video Metadata

        virtual DTC::VideoMetadataInfoList* GetVideoList       ( const QString    &Folder,
                                                                 const QString    &Sort,
                                                                 bool             Descending,
                                                                 int              StartIndex,
                                                                 int              Count      ) = 0;

        virtual DTC::VideoMetadataInfo*     GetVideo           ( int              Id         ) = 0;

        virtual DTC::VideoMetadataInfo*     GetVideoByFileName ( const QString    &FileName  ) = 0;

        virtual DTC::VideoLookupList*       LookupVideo        ( const QString    &Title,
                                                                 const QString    &Subtitle,
                                                                 const QString    &Inetref,
                                                                 int              Season,
                                                                 int              Episode,
                                                                 const QString    &GrabberType,
                                                                 bool             AllowGeneric) = 0;

        virtual bool                        AddVideo           ( const QString    &FileName,
                                                                 const QString    &HostName  ) = 0;

        virtual bool                        RemoveVideoFromDB  ( int              Id         ) = 0;
        // Bluray Metadata

        virtual DTC::BlurayInfo*            GetBluray          ( const QString    &Path      ) = 0;

        virtual bool                        UpdateVideoWatchedStatus ( int  Id,
                                                                       bool Watched ) = 0;

        virtual bool                        UpdateVideoMetadata      ( int           Id,
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
                                                                       const QString &Countries) = 0;

        virtual DTC::VideoStreamInfoList*     GetStreamInfo (          const QString &StorageGroup,
                                                                       const QString &FileName  ) = 0;

        virtual long                          GetSavedBookmark       ( int           Id) = 0;

        virtual bool                          SetSavedBookmark       ( int           Id,
                                                                       long          Offset ) = 0;

};

#endif
