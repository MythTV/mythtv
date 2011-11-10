//////////////////////////////////////////////////////////////////////////////
// Program Name: dvrServices.h
// Created     : Mar. 7, 2011
//
// Purpose - DVR Services API Interface definition 
//
// Copyright (c) 2010 David Blain <dblain@mythtv.org>
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

#ifndef DVRSERVICES_H_
#define DVRSERVICES_H_

#include "service.h"

#include "datacontracts/programList.h"
#include "datacontracts/encoderList.h"
#include "datacontracts/recRuleList.h"

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

class SERVICE_PUBLIC DvrServices : public Service  //, public QScriptable ???
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.2" );
    Q_CLASSINFO( "RemoveRecordedItem_Method",                   "POST" )
    Q_CLASSINFO( "CreateRecordSchedule_Method",                 "POST" )
    Q_CLASSINFO( "RemoveRecordSchedule_Method",                 "POST" )
    Q_CLASSINFO( "EnableRecordSchedule_Method",                 "POST" )
    Q_CLASSINFO( "DisableRecordSchedule_Method",                "POST" )

    public:

        // Must call InitializeCustomTypes for each unique Custom Type used
        // in public slots below.

        DvrServices( QObject *parent = 0 ) : Service( parent )
        {
            DTC::ProgramList::InitializeCustomTypes();
            DTC::EncoderList::InitializeCustomTypes();
            DTC::RecRuleList::InitializeCustomTypes();
        }

    public slots:

        virtual DTC::ProgramList*  GetExpiring           ( int              StartIndex, 
                                                           int              Count      ) = 0;

        virtual DTC::ProgramList* GetRecorded            ( bool             Descending,
                                                           int              StartIndex,
                                                           int              Count      ) = 0;

        virtual DTC::Program*     GetRecordedItem        ( int              ChanId,
                                                           const QDateTime &StartTime  ) = 0;

        virtual bool              RemoveRecordedItem     ( int              ChanId,
                                                           const QDateTime &StartTime  ) = 0;

        virtual DTC::ProgramList* GetConflicts           ( int              StartIndex,
                                                           int              Count      ) = 0;

        virtual DTC::ProgramList* GetUpcoming            ( int              StartIndex,
                                                           int              Count,
                                                           bool             ShowAll    ) = 0;

        virtual DTC::EncoderList*  Encoders              ( ) = 0;

        // Recording Rules

//        virtual bool               CreateRecordSchedule  ( ) = 0;

        virtual bool               RemoveRecordSchedule  ( uint             RecordId   ) = 0;

        virtual DTC::RecRuleList*  GetRecordSchedules    ( int              StartIndex,
                                                           int              Count      ) = 0;

        virtual DTC::RecRule*      GetRecordSchedule     ( uint             RecordId   ) = 0;

        virtual bool               EnableRecordSchedule  ( uint             RecordId   ) = 0;

        virtual bool               DisableRecordSchedule ( uint             RecordId   ) = 0;

};

#endif

