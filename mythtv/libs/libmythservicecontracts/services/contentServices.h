//////////////////////////////////////////////////////////////////////////////
// Program Name: contentServices.h
// Created     : Mar. 7, 2011
//
// Purpose - Content Services API Interface definition 
//
// Copyright (c) 2010 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef CONTENTSERVICES_H_
#define CONTENTSERVICES_H_

#include <QFileInfo>
#include <QStringList>

#include "service.h"
#include "datacontracts/artworkInfoList.h"
#include "datacontracts/liveStreamInfoList.h"

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

class SERVICE_PUBLIC ContentServices : public Service  //, public QScriptable ???
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.33" );
    Q_CLASSINFO( "DownloadFile_Method",            "POST" )

    public:

        // Must call InitializeCustomTypes for each unique Custom Type used
        // in public slots below.

        ContentServices( QObject *parent = 0 ) : Service( parent )
        {
            DTC::ArtworkInfoList::InitializeCustomTypes();
            DTC::LiveStreamInfoList::InitializeCustomTypes();
        }

    public slots:

        virtual QFileInfo           GetFile             ( const QString   &StorageGroup,
                                                          const QString   &FileName ) = 0;

        virtual QFileInfo           GetImageFile        ( const QString   &StorageGroup,
                                                          const QString   &FileName,
                                                          int Width, int Height ) = 0;

        virtual QStringList         GetFileList         ( const QString   &StorageGroup ) = 0;

        virtual QFileInfo           GetRecordingArtwork ( const QString   &Type,
                                                          const QString   &Inetref,
                                                          int Season, int Width,
                                                          int Height ) = 0;

        virtual DTC::ArtworkInfoList*
                                    GetRecordingArtworkList( int              ChanId,
                                                             const QDateTime &StartTime  ) = 0;

        virtual DTC::ArtworkInfoList*
                                    GetProgramArtworkList( const QString &Inetref,
                                                           int            Season  ) = 0;


        virtual QFileInfo           GetVideoArtwork     ( const QString   &Type,
                                                          int Id, int Width,
                                                          int Height ) = 0;

        virtual QFileInfo           GetAlbumArt         ( int Id, int Width, int Height ) = 0;

        virtual QFileInfo           GetPreviewImage     ( int              ChanId,
                                                          const QDateTime &StartTime,
                                                          int              Width,    
                                                          int              Height,   
                                                          int              SecsIn ) = 0;

        virtual QFileInfo           GetRecording        ( int              ChanId,
                                                          const QDateTime &StartTime ) = 0;

        virtual QFileInfo           GetMusic            ( int Id ) = 0;
        virtual QFileInfo           GetVideo            ( int Id ) = 0;

        virtual QString             GetHash             ( const QString   &StorageGroup,
                                                          const QString   &FileName ) = 0;

        virtual bool                DownloadFile        ( const QString   &URL,
                                                          const QString   &StorageGroup ) = 0;

        virtual DTC::LiveStreamInfo     *AddLiveStream          ( const QString   &StorageGroup,
                                                                  const QString   &FileName,
                                                                  const QString   &HostName,
                                                                  int              MaxSegments,
                                                                  int              Width,
                                                                  int              Height,
                                                                  int              Bitrate,
                                                                  int              AudioBitrate,
                                                                  int              SampleRate ) = 0;

        virtual DTC::LiveStreamInfo     *AddRecordingLiveStream ( int              ChanId,
                                                                  const QDateTime &StartTime,
                                                                  int              MaxSegments,
                                                                  int              Width,
                                                                  int              Height,
                                                                  int              Bitrate,
                                                                  int              AudioBitrate,
                                                                  int              SampleRate ) = 0;

        virtual DTC::LiveStreamInfo     *AddVideoLiveStream     ( int              Id,
                                                                  int              MaxSegments,
                                                                  int              Width,
                                                                  int              Height,
                                                                  int              Bitrate,
                                                                  int              AudioBitrate,
                                                                  int              SampleRate ) = 0;

        virtual DTC::LiveStreamInfo     *GetLiveStream            ( int Id ) = 0;
        virtual DTC::LiveStreamInfoList *GetLiveStreamList        ( const QString &FileName ) = 0;

        virtual DTC::LiveStreamInfo     *StopLiveStream         ( int Id ) = 0;
        virtual bool                     RemoveLiveStream       ( int Id ) = 0;
};

#endif

