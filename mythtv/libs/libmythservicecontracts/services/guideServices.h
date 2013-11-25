//////////////////////////////////////////////////////////////////////////////
// Program Name: guideservices.h
// Created     : Mar. 7, 2011
//
// Purpose - Program Guide Services API Interface definition 
//
// Copyright (c) 2010 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef GUIDESERVICES_H_
#define GUIDESERVICES_H_

#include <QFileInfo>

#include "service.h"
#include "datacontracts/programGuide.h"
#include "datacontracts/programAndChannel.h"
#include "datacontracts/channelGroupList.h"

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

class SERVICE_PUBLIC GuideServices : public Service  //, public QScriptable ???
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "2.1" );

    public:

        // Must call InitializeCustomTypes for each unique Custom Type used
        // in public slots below.

        GuideServices( QObject *parent = 0 ) : Service( parent )
        {
            DTC::ProgramGuide::InitializeCustomTypes();
            DTC::Program     ::InitializeCustomTypes();
            DTC::ChannelGroup::InitializeCustomTypes();
            DTC::ChannelGroupList::InitializeCustomTypes();
        }

    public slots:

        virtual DTC::ProgramGuide*  GetProgramGuide     ( const QDateTime &StartTime  ,
                                                          const QDateTime &EndTime    ,
                                                          int              StartChanId,
                                                          int              NumChannels,
                                                          bool             Details,
                                                          int              ChannelGroupId) = 0;

        virtual DTC::Program*       GetProgramDetails   ( int              ChanId,
                                                          const QDateTime &StartTime ) = 0;

        virtual QFileInfo           GetChannelIcon      ( int              ChanId,
                                                          int              Width ,
                                                          int              Height ) = 0;
        virtual DTC::ChannelGroupList* GetChannelGroupList ( bool          IncludeEmpty ) = 0;
};

#endif

