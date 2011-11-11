//////////////////////////////////////////////////////////////////////////////
// Program Name: dvr.cpp
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

#include <QMap>

#include "dvr.h"

#include "compat.h"
#include "mythversion.h"
#include "mythcorecontext.h"
#include "scheduler.h"
#include "autoexpire.h"
#include "jobqueue.h"
#include "encoderlink.h"
#include "remoteutil.h"

#include "serviceUtil.h"

extern QMap<int, EncoderLink *> tvList;
extern AutoExpire  *expirer;

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::ProgramList* Dvr::GetRecordedList( bool bDescending,
                                        int  nStartIndex,
                                        int  nCount      )
{
    QMap< QString, ProgramInfo* > recMap;

    if (gCoreContext->GetScheduler())
        recMap = gCoreContext->GetScheduler()->GetRecording();

    QMap< QString, uint32_t > inUseMap    = ProgramInfo::QueryInUseMap();
    QMap< QString, bool >     isJobRunning= ProgramInfo::QueryJobsRunning(JOB_COMMFLAG);

    ProgramList progList;

    int desc = 0;
    if (bDescending)
        desc = -1;

    LoadFromRecorded( progList, false, inUseMap, isJobRunning, recMap, desc );

    QMap< QString, ProgramInfo* >::iterator mit = recMap.begin();

    for (; mit != recMap.end(); mit = recMap.erase(mit))
        delete *mit;

    // ----------------------------------------------------------------------
    // Build Response
    // ----------------------------------------------------------------------

    DTC::ProgramList *pPrograms = new DTC::ProgramList();

    nStartIndex   = min( nStartIndex, (int)progList.size() );
    nCount        = (nCount > 0) ? min( nCount, (int)progList.size() ) : progList.size();
    int nEndIndex = min((nStartIndex + nCount), (int)progList.size() );

    for( int n = nStartIndex; n < nEndIndex; n++)
    {
        ProgramInfo *pInfo = progList[ n ];

        DTC::Program *pProgram = pPrograms->AddNewProgram();

        FillProgramInfo( pProgram, pInfo, true );
    }

    // ----------------------------------------------------------------------

    pPrograms->setStartIndex    ( nStartIndex     );
    pPrograms->setCount         ( nCount          );
    pPrograms->setTotalAvailable( progList.size() );
    pPrograms->setAsOf          ( QDateTime::currentDateTime() );
    pPrograms->setVersion       ( MYTH_BINARY_VERSION );
    pPrograms->setProtoVer      ( MYTH_PROTO_VERSION  );

    return pPrograms;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::Program* Dvr::GetRecorded( int              nChanId,
                                const QDateTime &dStartTime  )
{
    if (nChanId <= 0 || !dStartTime.isValid())
        throw( QString("Channel ID or StartTime appears invalid."));

    ProgramInfo *pInfo = new ProgramInfo(nChanId, dStartTime);

    DTC::Program *pProgram = new DTC::Program();
    FillProgramInfo( pProgram, pInfo, true );

    return pProgram;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool Dvr::RemoveRecorded( int              nChanId,
                          const QDateTime &dStartTime  )
{
    if (nChanId <= 0 || !dStartTime.isValid())
        throw( QString("Channel ID or StartTime appears invalid."));

    bool bResult = false;

    ProgramInfo *pInfo = new ProgramInfo(nChanId, dStartTime);

    if (pInfo->HasPathname())
        bResult = RemoteDeleteRecording(nChanId, dStartTime, true, false);

    return bResult;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::ProgramList* Dvr::GetExpiringList( int nStartIndex, 
                                        int nCount      )
{
    pginfolist_t  infoList;

    if (expirer)
        expirer->GetAllExpiring( infoList );

    // ----------------------------------------------------------------------
    // Build Response
    // ----------------------------------------------------------------------

    DTC::ProgramList *pPrograms = new DTC::ProgramList();

    nStartIndex   = min( nStartIndex, (int)infoList.size() );
    nCount        = (nCount > 0) ? min( nCount, (int)infoList.size() ) : infoList.size();
    int nEndIndex = min((nStartIndex + nCount), (int)infoList.size() );

    for( int n = nStartIndex; n < nEndIndex; n++)
    {
        ProgramInfo *pInfo = infoList[ n ];

        if (pInfo != NULL)
        {
            DTC::Program *pProgram = pPrograms->AddNewProgram();

            FillProgramInfo( pProgram, pInfo, true );

            delete pInfo;
        }
    }

    // ----------------------------------------------------------------------

    pPrograms->setStartIndex    ( nStartIndex     );
    pPrograms->setCount         ( nCount          );
    pPrograms->setTotalAvailable( infoList.size() );
    pPrograms->setAsOf          ( QDateTime::currentDateTime() );
    pPrograms->setVersion       ( MYTH_BINARY_VERSION );
    pPrograms->setProtoVer      ( MYTH_PROTO_VERSION  );

    return pPrograms;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::EncoderList* Dvr::GetEncoderList()
{
    DTC::EncoderList* pList = new DTC::EncoderList();

    QMap<int, EncoderLink *>::Iterator iter = tvList.begin();

    for (; iter != tvList.end(); ++iter)
    {
        EncoderLink *elink = *iter;

        if (elink != NULL)
        {
            DTC::Encoder *pEncoder = pList->AddNewEncoder();
            
            pEncoder->setId            ( elink->GetCardID()       );
            pEncoder->setState         ( elink->GetState()        );
            pEncoder->setLocal         ( elink->IsLocal()         );
            pEncoder->setConnected     ( elink->IsConnected()     );
            pEncoder->setSleepStatus   ( elink->GetSleepStatus()  );
          //  pEncoder->setLowOnFreeSpace( elink->isLowOnFreeSpace());

            if (pEncoder->Local())
                pEncoder->setHostName( gCoreContext->GetHostName() );
            else
                pEncoder->setHostName( elink->GetHostName() );

            switch ( pEncoder->State() )
            {
                case kState_WatchingLiveTV:
                case kState_RecordingOnly:
                case kState_WatchingRecording:
                {
                    ProgramInfo  *pInfo = elink->GetRecording();

                    if (pInfo)
                    {
                        DTC::Program *pProgram = pEncoder->Recording();

                        FillProgramInfo( pProgram, pInfo, true, true );

                        delete pInfo;
                    }

                    break;
                }

                default:
                    break;
            }
        }
    }
    return pList;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::ProgramList* Dvr::GetUpcomingList( int  nStartIndex,
                                        int  nCount,
                                        bool bShowAll )
{
    RecordingList  recordingList;
    RecordingList  tmpList;
    bool hasConflicts;
    LoadFromScheduler(tmpList, hasConflicts);

    // Sort the upcoming into only those which will record
    RecordingList::iterator it = tmpList.begin();
    for(; it < tmpList.end(); ++it)
    {
        if (!bShowAll && ((*it)->GetRecordingStatus() <= rsWillRecord) &&
            ((*it)->GetRecordingStartTime() >=
             QDateTime::currentDateTime()))
        {
            recordingList.push_back(new RecordingInfo(**it));
        }
        else if (bShowAll && ((*it)->GetRecordingStartTime() >=
             QDateTime::currentDateTime()))
        {
            recordingList.push_back(new RecordingInfo(**it));
        }
    }

    // ----------------------------------------------------------------------
    // Build Response
    // ----------------------------------------------------------------------

    DTC::ProgramList *pPrograms = new DTC::ProgramList();

    nStartIndex   = min( nStartIndex, (int)recordingList.size() );
    nCount        = (nCount > 0) ? min( nCount, (int)recordingList.size() ) : recordingList.size();
    int nEndIndex = min((nStartIndex + nCount), (int)recordingList.size() );

    for( int n = nStartIndex; n < nEndIndex; n++)
    {
        ProgramInfo *pInfo = recordingList[ n ];

        DTC::Program *pProgram = pPrograms->AddNewProgram();

        FillProgramInfo( pProgram, pInfo, true );
    }

    // ----------------------------------------------------------------------

    pPrograms->setStartIndex    ( nStartIndex     );
    pPrograms->setCount         ( nCount          );
    pPrograms->setTotalAvailable( recordingList.size() );
    pPrograms->setAsOf          ( QDateTime::currentDateTime() );
    pPrograms->setVersion       ( MYTH_BINARY_VERSION );
    pPrograms->setProtoVer      ( MYTH_PROTO_VERSION  );

    return pPrograms;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::ProgramList* Dvr::GetConflictList( int  nStartIndex,
                                        int  nCount       )
{
    RecordingList  recordingList;
    RecordingList  tmpList;
    bool hasConflicts;
    LoadFromScheduler(tmpList, hasConflicts);

    // Sort the upcoming into only those which are conflicts
    RecordingList::iterator it = tmpList.begin();
    for(; it < tmpList.end(); ++it)
    {
        if (((*it)->GetRecordingStatus() == rsConflict) &&
            ((*it)->GetRecordingStartTime() >=
             QDateTime::currentDateTime()))
        {
            recordingList.push_back(new RecordingInfo(**it));
        }
    }

    // ----------------------------------------------------------------------
    // Build Response
    // ----------------------------------------------------------------------

    DTC::ProgramList *pPrograms = new DTC::ProgramList();

    nStartIndex   = min( nStartIndex, (int)recordingList.size() );
    nCount        = (nCount > 0) ? min( nCount, (int)recordingList.size() ) : recordingList.size();
    int nEndIndex = min((nStartIndex + nCount), (int)recordingList.size() );

    for( int n = nStartIndex; n < nEndIndex; n++)
    {
        ProgramInfo *pInfo = recordingList[ n ];

        DTC::Program *pProgram = pPrograms->AddNewProgram();

        FillProgramInfo( pProgram, pInfo, true );
    }

    // ----------------------------------------------------------------------

    pPrograms->setStartIndex    ( nStartIndex     );
    pPrograms->setCount         ( nCount          );
    pPrograms->setTotalAvailable( recordingList.size() );
    pPrograms->setAsOf          ( QDateTime::currentDateTime() );
    pPrograms->setVersion       ( MYTH_BINARY_VERSION );
    pPrograms->setProtoVer      ( MYTH_PROTO_VERSION  );

    return pPrograms;
}

bool Dvr::RemoveRecordSchedule ( uint nRecordId )
{
    bool bResult = false;

    if (nRecordId <= 0 )
        throw( QString("Record ID appears invalid."));

    RecordingRule *pRule = new RecordingRule();
    pRule->m_recordID = nRecordId;

    bResult = pRule->Delete();

    return bResult;
}

DTC::RecRuleList* Dvr::GetRecordScheduleList( int nStartIndex,
                                              int nCount      )
{
    vector<ProgramInfo *> infoList;
    RemoteGetAllScheduledRecordings(infoList);

    // ----------------------------------------------------------------------
    // Build Response
    // ----------------------------------------------------------------------

    DTC::RecRuleList *pRecRules = new DTC::RecRuleList();

    nStartIndex   = min( nStartIndex, (int)infoList.size() );
    nCount        = (nCount > 0) ? min( nCount, (int)infoList.size() ) : infoList.size();
    int nEndIndex = min((nStartIndex + nCount), (int)infoList.size() );

    for( int n = nStartIndex; n < nEndIndex; n++)
    {
        RecordingRule *rule = new RecordingRule();
        ProgramInfo *pInfo = infoList[ n ];
        rule->LoadByProgram(pInfo);

        if (pInfo != NULL)
        {
            DTC::RecRule *pRecRule = pRecRules->AddNewRecRule();

            FillRecRuleInfo( pRecRule, rule );

            delete rule;
            delete pInfo;
        }
    }

    // ----------------------------------------------------------------------

    pRecRules->setStartIndex    ( nStartIndex     );
    pRecRules->setCount         ( nCount          );
    pRecRules->setTotalAvailable( infoList.size() );
    pRecRules->setAsOf          ( QDateTime::currentDateTime() );
    pRecRules->setVersion       ( MYTH_BINARY_VERSION );
    pRecRules->setProtoVer      ( MYTH_PROTO_VERSION  );

    return pRecRules;
}

DTC::RecRule* Dvr::GetRecordSchedule( uint nRecordId )
{
    if (nRecordId <= 0 )
        throw( QString("Record ID appears invalid."));

    RecordingRule *pRule = new RecordingRule();
    pRule->m_recordID = nRecordId;
    pRule->Load();

    DTC::RecRule *pRecRule = new DTC::RecRule();
    FillRecRuleInfo( pRecRule, pRule );

    return pRecRule;
}

bool Dvr::EnableRecordSchedule ( uint nRecordId )
{
    bool bResult = false;

    if (nRecordId <= 0 )
        throw( QString("Record ID appears invalid."));

    RecordingRule *pRule = new RecordingRule();
    pRule->m_recordID = nRecordId;
    pRule->Load();

    if (pRule->IsLoaded())
    {
        pRule->m_isInactive = false;
        pRule->Save();
        bResult = true;
    }

    return bResult;
}

bool Dvr::DisableRecordSchedule( uint nRecordId )
{
    bool bResult = false;

    if (nRecordId <= 0 )
        throw( QString("Record ID appears invalid."));

    RecordingRule *pRule = new RecordingRule();
    pRule->m_recordID = nRecordId;
    pRule->Load();

    if (pRule->IsLoaded())
    {
        pRule->m_isInactive = true;
        pRule->Save();
        bResult = true;
    }

    return bResult;
}

