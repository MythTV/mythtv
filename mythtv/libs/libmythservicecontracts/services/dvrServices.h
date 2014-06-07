//////////////////////////////////////////////////////////////////////////////
// Program Name: dvrServices.h
// Created     : Mar. 7, 2011
//
// Purpose - DVR Services API Interface definition 
//
// Copyright (c) 2010 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef DVRSERVICES_H_
#define DVRSERVICES_H_

#include "service.h"

#include "datacontracts/programList.h"
#include "datacontracts/encoderList.h"
#include "datacontracts/recRule.h"
#include "datacontracts/recRuleList.h"
#include "datacontracts/recRuleFilter.h"
#include "datacontracts/recRuleFilterList.h"
#include "datacontracts/titleInfoList.h"
#include "datacontracts/input.h"
#include "datacontracts/inputList.h"

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
    Q_CLASSINFO( "version"    , "5.0" );
    Q_CLASSINFO( "RemoveRecorded_Method",                       "POST" )
    Q_CLASSINFO( "DeleteRecording_Method",                      "POST" )
    Q_CLASSINFO( "UnDeleteRecording",                           "POST" )
    Q_CLASSINFO( "UpdateRecordedWatchedStatus_Method",          "POST" )
    Q_CLASSINFO( "AddRecordSchedule_Method",                    "POST" )
    Q_CLASSINFO( "UpdateRecordSchedule_Method",                 "POST" )
    Q_CLASSINFO( "RemoveRecordSchedule_Method",                 "POST" )
    Q_CLASSINFO( "AddDontRecordSchedule",                       "POST" )
    Q_CLASSINFO( "EnableRecordSchedule_Method",                 "POST" )
    Q_CLASSINFO( "DisableRecordSchedule_Method",                "POST" )


    public:

        // Must call InitializeCustomTypes for each unique Custom Type used
        // in public slots below.

        DvrServices( QObject *parent = 0 ) : Service( parent )
        {
            DTC::ProgramList::InitializeCustomTypes();
            DTC::EncoderList::InitializeCustomTypes();
            DTC::InputList::InitializeCustomTypes();
            DTC::RecRuleList::InitializeCustomTypes();
            DTC::TitleInfoList::InitializeCustomTypes();
            DTC::RecRuleFilterList::InitializeCustomTypes();
        }

    public slots:

        virtual DTC::ProgramList*  GetExpiringList       ( int              StartIndex, 
                                                           int              Count      ) = 0;

        virtual DTC::ProgramList*  GetRecordedList       ( bool             Descending,
                                                           int              StartIndex,
                                                           int              Count,
                                                           const QString   &TitleRegEx,
                                                           const QString   &RecGroup,
                                                           const QString   &StorageGroup ) = 0;

        virtual DTC::Program*      GetRecorded           ( int              ChanId,
                                                           const QDateTime &StartTime  ) = 0;

        virtual bool               RemoveRecorded        ( int              ChanId,
                                                           const QDateTime &StartTime,
                                                           bool             ForceDelete,
                                                           bool             AllowRerecord ) = 0;

        virtual bool               DeleteRecording       ( int              ChanId,
                                                           const QDateTime &StartTime,
                                                           bool             ForceDelete,
                                                           bool             AllowRerecord ) = 0;

        virtual bool               UnDeleteRecording     ( int              ChanId,
                                                           const QDateTime &StartTime ) = 0;

        virtual bool               UpdateRecordedWatchedStatus ( int   ChanId,
                                                                 const QDateTime &StartTime,
                                                                 bool  Watched) = 0;

        virtual DTC::ProgramList*  GetConflictList       ( int              StartIndex,
                                                           int              Count,
                                                           int              RecordId ) = 0;

        virtual DTC::ProgramList*  GetUpcomingList       ( int              StartIndex,
                                                           int              Count,
                                                           bool             ShowAll,
                                                           int              RecordId,
                                                           int              RecStatus ) = 0;

        virtual DTC::EncoderList*  GetEncoderList        ( ) = 0;

        virtual DTC::InputList*    GetInputList          ( ) = 0;

        virtual QStringList        GetRecGroupList       ( ) = 0;

        virtual QStringList        GetRecStorageGroupList ( ) = 0;

        virtual QStringList        GetPlayGroupList      ( ) = 0;

        virtual DTC::RecRuleFilterList* GetRecRuleFilterList ( ) = 0;

        virtual QStringList        GetTitleList          ( const QString   &RecGroup ) = 0;

        virtual DTC::TitleInfoList* GetTitleInfoList     ( ) = 0;

        // Recording Rules

        virtual uint               AddRecordSchedule     ( QString   Title,
                                                           QString   Subtitle,
                                                           QString   Description,
                                                           QString   Category,
                                                           QDateTime StartTime,
                                                           QDateTime EndTime,
                                                           QString   SeriesId,
                                                           QString   ProgramId,
                                                           int       ChanId,
                                                           QString   Station,
                                                           int       FindDay,
                                                           QTime     FindTime,
                                                           int       ParentId,
                                                           bool      Inactive,
                                                           uint      Season,
                                                           uint      Episode,
                                                           QString   Inetref,
                                                           QString   Type,
                                                           QString   SearchType,
                                                           int       RecPriority,
                                                           uint      PreferredInput,
                                                           int       StartOffset,
                                                           int       EndOffset,
                                                           QString   DupMethod,
                                                           QString   DupIn,
                                                           uint      Filter,
                                                           QString   RecProfile,
                                                           QString   RecGroup,
                                                           QString   StorageGroup,
                                                           QString   PlayGroup,
                                                           bool      AutoExpire,
                                                           int       MaxEpisodes,
                                                           bool      MaxNewest,
                                                           bool      AutoCommflag,
                                                           bool      AutoTranscode,
                                                           bool      AutoMetaLookup,
                                                           bool      AutoUserJob1,
                                                           bool      AutoUserJob2,
                                                           bool      AutoUserJob3,
                                                           bool      AutoUserJob4,
                                                           int       Transcoder        ) = 0;

        virtual bool               UpdateRecordSchedule  ( uint      RecordId,
                                                           QString   Title,
                                                           QString   Subtitle,
                                                           QString   Description,
                                                           QString   Category,
                                                           QDateTime StartTime,
                                                           QDateTime EndTime,
                                                           QString   SeriesId,
                                                           QString   ProgramId,
                                                           int       ChanId,
                                                           QString   Station,
                                                           int       FindDay,
                                                           QTime     FindTime,
                                                           bool      Inactive,
                                                           uint      Season,
                                                           uint      Episode,
                                                           QString   Inetref,
                                                           QString   Type,
                                                           QString   SearchType,
                                                           int       RecPriority,
                                                           uint      PreferredInput,
                                                           int       StartOffset,
                                                           int       EndOffset,
                                                           QString   DupMethod,
                                                           QString   DupIn,
                                                           uint      Filter,
                                                           QString   RecProfile,
                                                           QString   RecGroup,
                                                           QString   StorageGroup,
                                                           QString   PlayGroup,
                                                           bool      AutoExpire,
                                                           int       MaxEpisodes,
                                                           bool      MaxNewest,
                                                           bool      AutoCommflag,
                                                           bool      AutoTranscode,
                                                           bool      AutoMetaLookup,
                                                           bool      AutoUserJob1,
                                                           bool      AutoUserJob2,
                                                           bool      AutoUserJob3,
                                                           bool      AutoUserJob4,
                                                           int       Transcoder        ) = 0;

        virtual bool               RemoveRecordSchedule  ( uint             RecordId   ) = 0;

        virtual bool               AddDontRecordSchedule ( int              ChanId,
                                                           const QDateTime &StartTime,
                                                           bool             NeverRecord ) = 0;

        virtual DTC::RecRuleList*  GetRecordScheduleList ( int              StartIndex,
                                                           int              Count,
                                                           const QString    &Sort,
                                                           bool Descending  ) = 0;

        virtual DTC::RecRule*      GetRecordSchedule     ( uint             RecordId,
                                                           QString          Template,
                                                           int              ChanId,
                                                           QDateTime        StartTime,
                                                           bool             MakeOverride ) = 0;

        virtual bool               EnableRecordSchedule  ( uint             RecordId   ) = 0;

        virtual bool               DisableRecordSchedule ( uint             RecordId   ) = 0;

        // The following are all temporary, pending implementation of a
        // 'enum metadata' endpoint
        virtual QString            RecStatusToString     ( int              RecStatus  ) = 0;

        virtual QString            RecStatusToDescription( int              RecStatus,
                                                           int              RecType,
                                                           const QDateTime &StartTime ) = 0;

        virtual QString            RecTypeToString       ( QString          RecType   ) = 0;

        virtual QString            RecTypeToDescription  ( QString          RecType   ) = 0;

        virtual QString            DupMethodToString     ( QString          DupMethod ) = 0;

        virtual QString            DupMethodToDescription( QString          DupMethod ) = 0;

        virtual QString            DupInToString         ( QString          DupIn     ) = 0;

        virtual QString            DupInToDescription    ( QString          DupIn     ) = 0;
};

#endif

