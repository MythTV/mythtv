//////////////////////////////////////////////////////////////////////////////
// Program Name: channelServices.h
// Created     : Apr. 8, 2011
//
// Purpose - Channel Services API Interface definition
//
// Copyright (c) 2010 Robert McNamara <rmcnamara@mythtv.org>
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

#ifndef CHANNELSERVICES_H_
#define CHANNELSERVICES_H_

#include "service.h"

#include "datacontracts/channelInfoList.h"
#include "datacontracts/videoSource.h"
#include "datacontracts/videoSourceList.h"
#include "datacontracts/videoMultiplex.h"
#include "datacontracts/videoMultiplexList.h"

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
    Q_CLASSINFO( "version"    , "1.03" );
    Q_CLASSINFO( "UpdateDBChannel_Method",         "POST" )
    Q_CLASSINFO( "CreateDBChannel_Method",         "POST" )
    Q_CLASSINFO( "DeleteDBChannel_Method",         "POST" )

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
        }

    public slots:

        /* Channel Methods */

        virtual DTC::ChannelInfoList*  GetChannelInfoList  ( int           SourceID,
                                                             int           StartIndex,
                                                             int           Count      ) = 0;

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

        virtual bool                   CreateDBChannel     ( uint          MplexID,
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

        virtual bool                   DeleteDBChannel     ( uint          ChannelID ) = 0;

        /* Video Source Methods */

        virtual DTC::VideoSourceList*  GetVideoSourceList  ( void ) = 0;

        /* Multiplex Methods */

        virtual DTC::VideoMultiplexList*  GetVideoMultiplexList  ( int SourceID,
                                                                   int StartIndex,
                                                                   int Count      ) = 0;
};

#endif

