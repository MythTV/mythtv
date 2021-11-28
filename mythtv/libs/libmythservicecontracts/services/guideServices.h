//////////////////////////////////////////////////////////////////////////////
// Program Name: guideservices.h
// Created     : Mar. 7, 2011
//
// Purpose - Program Guide Services API Interface definition
//
// Copyright (c) 2010 David Blain <dblain@mythtv.org>
//
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef GUIDESERVICES_H_
#define GUIDESERVICES_H_

#include <QFileInfo>

#include "service.h"
#include "datacontracts/programGuide.h"
#include "datacontracts/programAndChannel.h"
#include "datacontracts/channelGroupList.h"
#include "datacontracts/programList.h"

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
    Q_CLASSINFO( "version"    , "2.4" )
    Q_CLASSINFO( "AddToChannelGroup_Method",                     "POST" )
    Q_CLASSINFO( "RemoveFromChannelGroup_Method",                "POST" )

    public:

        // Must call InitializeCustomTypes for each unique Custom Type used
        // in public slots below.

        GuideServices( QObject *parent = nullptr ) : Service( parent )
        {
            DTC::ProgramGuide::InitializeCustomTypes();
            DTC::ProgramList ::InitializeCustomTypes();
            DTC::Program     ::InitializeCustomTypes();
            DTC::ChannelGroup::InitializeCustomTypes();
            DTC::ChannelGroupList::InitializeCustomTypes();
        }

    public slots:

        virtual DTC::ProgramGuide*  GetProgramGuide     ( const QDateTime &StartTime  ,
                                                          const QDateTime &EndTime    ,
                                                          bool             Details,
                                                          int              ChannelGroupId,
                                                          int              StartIndex,
                                                          int              Count,
                                                          bool             WithInvisible) = 0;

        virtual DTC::ProgramList*   GetProgramList      ( int              StartIndex,
                                                          int              Count,
                                                          const QDateTime &StartTime  ,
                                                          const QDateTime &EndTime    ,
                                                          int              ChanId,
                                                          const QString   &TitleFilter,
                                                          const QString   &CategoryFilter,
                                                          const QString   &PersonFilter,
                                                          const QString   &KeywordFilter,
                                                          bool             OnlyNew,
                                                          bool             Details,
                                                          const QString   &Sort,
                                                          bool             Descending,
                                                          bool             WithInvisible) = 0;

        virtual DTC::Program*       GetProgramDetails   ( int              ChanId,
                                                          const QDateTime &StartTime ) = 0;

        virtual QFileInfo           GetChannelIcon      ( int              ChanId,
                                                          int              Width ,
                                                          int              Height ) = 0;

        virtual DTC::ChannelGroupList* GetChannelGroupList ( bool          IncludeEmpty ) = 0;

        virtual QStringList         GetCategoryList     ( ) = 0;

        virtual QStringList         GetStoredSearches   ( const QString &Type ) = 0;

        virtual bool                AddToChannelGroup   ( int              ChannelGroupId,
                                                          int              ChanId ) = 0;

        virtual bool                RemoveFromChannelGroup ( int           ChannelGroupId,
                                                             int           ChanId ) = 0;
};

#endif
