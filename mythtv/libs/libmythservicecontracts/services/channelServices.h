//////////////////////////////////////////////////////////////////////////////
// Program Name: channelServices.h
// Created     : Apr. 8, 2011
//
// Purpose - Channel Services API Interface definition
//
// Copyright (c) 2010 Robert McNamara <rmcnamara@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef CHANNELSERVICES_H_
#define CHANNELSERVICES_H_

#include "service.h"

#include "datacontracts/channelInfoList.h"
#include "datacontracts/videoSource.h"
#include "datacontracts/videoSourceList.h"
#include "datacontracts/videoMultiplex.h"
#include "datacontracts/videoMultiplexList.h"
#include "datacontracts/lineup.h"

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

class SERVICE_PUBLIC ChannelServices : public Service
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.3" );
    Q_CLASSINFO( "AddDBChannel_Method",              "POST" )
    Q_CLASSINFO( "UpdateDBChannel_Method",           "POST" )
    Q_CLASSINFO( "RemoveDBChannel_Method",           "POST" )
    Q_CLASSINFO( "AddVideoSource_Method",            "POST" )
    Q_CLASSINFO( "UpdateVideoSource_Method",         "POST" )
    Q_CLASSINFO( "RemoveVideoSource_Method",         "POST" )

    public:

        // Must call InitializeCustomTypes for each unique Custom Type used
        // in public slots below.

        ChannelServices( QObject *parent = 0 ) : Service( parent )
        {
            DTC::ChannelInfoList::InitializeCustomTypes();
            DTC::VideoSource::InitializeCustomTypes();
            DTC::VideoSourceList::InitializeCustomTypes();
            DTC::VideoMultiplex::InitializeCustomTypes();
            DTC::VideoMultiplexList::InitializeCustomTypes();
            DTC::Lineup::InitializeCustomTypes();
            DTC::LineupList::InitializeCustomTypes();
        }

    public slots:

        /* Channel Methods */

        virtual DTC::ChannelInfoList*  GetChannelInfoList  ( uint           SourceID,
                                                             uint           StartIndex,
                                                             uint           Count      ) = 0;

        virtual DTC::ChannelInfo*      GetChannelInfo      ( uint           ChanID     ) = 0;

        virtual bool                   UpdateDBChannel     ( uint          MplexID,
                                                             uint          SourceID,
                                                             uint          ChannelID,
                                                             const QString &CallSign,
                                                             const QString &ChannelName,
                                                             const QString &ChannelNumber,
                                                             uint          ServiceID,
                                                             uint          ATSCMajorChannel,
                                                             uint          ATSCMinorChannel,
                                                             bool          UseEIT,
                                                             bool          visible,
                                                             const QString &FrequencyID,
                                                             const QString &Icon,
                                                             const QString &Format,
                                                             const QString &XMLTVID,
                                                             const QString &DefaultAuthority ) = 0;

        virtual bool                   AddDBChannel        ( uint          MplexID,
                                                             uint          SourceID,
                                                             uint          ChannelID,
                                                             const QString &CallSign,
                                                             const QString &ChannelName,
                                                             const QString &ChannelNumber,
                                                             uint          ServiceID,
                                                             uint          ATSCMajorChannel,
                                                             uint          ATSCMinorChannel,
                                                             bool          UseEIT,
                                                             bool          visible,
                                                             const QString &FrequencyID,
                                                             const QString &Icon,
                                                             const QString &Format,
                                                             const QString &XMLTVID,
                                                             const QString &DefaultAuthority ) = 0;

        virtual bool                   RemoveDBChannel     ( uint          ChannelID ) = 0;

        /* Video Source Methods */

        virtual DTC::VideoSourceList*     GetVideoSourceList     ( void ) = 0;

        virtual DTC::VideoSource*         GetVideoSource         ( uint SourceID ) = 0;

        virtual bool                      UpdateVideoSource      ( uint          SourceID,
                                                                   const QString &SourceName,
                                                                   const QString &Grabber,
                                                                   const QString &UserId,
                                                                   const QString &FreqTable,
                                                                   const QString &LineupId,
                                                                   const QString &Password,
                                                                   bool          UseEIT,
                                                                   const QString &ConfigPath,
                                                                   int           NITId ) = 0;

        virtual int                       AddVideoSource         ( const QString &SourceName,
                                                                   const QString &Grabber,
                                                                   const QString &UserId,
                                                                   const QString &FreqTable,
                                                                   const QString &LineupId,
                                                                   const QString &Password,
                                                                   bool          UseEIT,
                                                                   const QString &ConfigPath,
                                                                   int           NITId ) = 0;

        virtual bool                      RemoveVideoSource      ( uint          SourceID ) = 0;

        virtual DTC::LineupList*          GetDDLineupList        ( const QString &Source,
                                                                   const QString &UserId,
                                                                   const QString &Password ) = 0;

        virtual int                       FetchChannelsFromSource( const uint SourceId,
                                                                   const uint CardId,
                                                                   bool       WaitForFinish ) = 0;

        /* Multiplex Methods */

        virtual DTC::VideoMultiplexList*  GetVideoMultiplexList  ( uint SourceID,
                                                                   uint StartIndex,
                                                                   uint Count      ) = 0;

        virtual DTC::VideoMultiplex*      GetVideoMultiplex      ( uint MplexID    ) = 0;

        virtual QStringList               GetXMLTVIdList         ( uint SourceID ) = 0;
};

#endif

