//////////////////////////////////////////////////////////////////////////////
// Program Name: dvr.cpp
// Created     : Mar. 7, 2011
//
// Copyright (c) 2011 David Blain <dblain@mythtv.org>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

#include <QMap>
#include <QRegExp>

#include "dvr.h"

#include "compat.h"
#include "mythversion.h"
#include "mythcorecontext.h"
#include "mythevent.h"
#include "scheduler.h"
#include "autoexpire.h"
#include "jobqueue.h"
#include "encoderlink.h"
#include "remoteutil.h"
#include "mythdate.h"
#include "recordinginfo.h"
#include "cardutil.h"
#include "inputinfo.h"
#include "programtypes.h"
#include "recordingtypes.h"

#include "serviceUtil.h"
#include "mythscheduler.h"
#include "storagegroup.h"
#include "playgroup.h"
#include "recordingprofile.h"

#include "scheduler.h"

extern QMap<int, EncoderLink *> tvList;
extern AutoExpire  *expirer;

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::ProgramList* Dvr::GetRecordedList( bool           bDescending,
                                        int            nStartIndex,
                                        int            nCount,
                                        const QString &sTitleRegEx,
                                        const QString &sRecGroup,
                                        const QString &sStorageGroup )
{
    QMap< QString, ProgramInfo* > recMap;

    if (gCoreContext->GetScheduler())
        recMap = gCoreContext->GetScheduler()->GetRecording();

    QMap< QString, uint32_t > inUseMap    = ProgramInfo::QueryInUseMap();
    QMap< QString, bool >     isJobRunning= ProgramInfo::QueryJobsRunning(JOB_COMMFLAG);

    ProgramList progList;

    int desc = 1;
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
    int nAvailable = 0;

    int nMax      = (nCount > 0) ? nCount : progList.size();

    nAvailable = 0;
    nCount = 0;

    QRegExp rTitleRegEx        = QRegExp(sTitleRegEx, Qt::CaseInsensitive);

    for( unsigned int n = 0; n < progList.size(); n++)
    {
        ProgramInfo *pInfo = progList[ n ];

        if (pInfo->IsDeletePending() ||
            (!sTitleRegEx.isEmpty() && !pInfo->GetTitle().contains(rTitleRegEx)) ||
            (!sRecGroup.isEmpty() && sRecGroup != pInfo->GetRecordingGroup()) ||
            (!sStorageGroup.isEmpty() && sStorageGroup != pInfo->GetStorageGroup()))
            continue;

        if ((nAvailable < nStartIndex) ||
            (nCount >= nMax))
        {
            ++nAvailable;
            continue;
        }

        ++nAvailable;
        ++nCount;

        DTC::Program *pProgram = pPrograms->AddNewProgram();

        FillProgramInfo( pProgram, pInfo, true );
    }

    // ----------------------------------------------------------------------

    pPrograms->setStartIndex    ( nStartIndex     );
    pPrograms->setCount         ( nCount          );
    pPrograms->setTotalAvailable( nAvailable      );
    pPrograms->setAsOf          ( MythDate::current() );
    pPrograms->setVersion       ( MYTH_BINARY_VERSION );
    pPrograms->setProtoVer      ( MYTH_PROTO_VERSION  );

    return pPrograms;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::Program* Dvr::GetRecorded(int RecordedId,
                               int chanid, const QDateTime &recstarttsRaw)
{
    if ((RecordedId <= 0) &&
        (chanid <= 0 || !recstarttsRaw.isValid()))
        throw QString("Recorded ID or Channel ID and StartTime appears invalid.");

    // TODO Should use RecordingInfo
    ProgramInfo pi;
    if (RecordedId > 0)
        pi = ProgramInfo(RecordedId);
    else
        pi = ProgramInfo(chanid, recstarttsRaw.toUTC());

    DTC::Program *pProgram = new DTC::Program();
    FillProgramInfo( pProgram, &pi, true );

    return pProgram;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool Dvr::RemoveRecorded(int RecordedId,
                         int chanid, const QDateTime &recstarttsRaw,
                         bool forceDelete, bool allowRerecord)
{
    return DeleteRecording(RecordedId, chanid, recstarttsRaw, forceDelete,
                           allowRerecord);
}


bool Dvr::DeleteRecording(int RecordedId,
                          int chanid, const QDateTime &recstarttsRaw,
                          bool forceDelete, bool allowRerecord)
{
    if ((RecordedId <= 0) &&
        (chanid <= 0 || !recstarttsRaw.isValid()))
        throw QString("Recorded ID or Channel ID and StartTime appears invalid.");

    // TODO Should use RecordingInfo
    ProgramInfo pi;
    if (RecordedId > 0)
        pi = ProgramInfo(RecordedId);
    else
        pi = ProgramInfo(chanid, recstarttsRaw.toUTC());

    if (pi.GetChanID() && pi.HasPathname())
    {
        QString cmd = QString("DELETE_RECORDING %1 %2 %3 %4")
            .arg(pi.GetChanID())
            .arg(pi.GetRecordingStartTime(MythDate::ISODate))
            .arg(forceDelete ? "FORCE" : "NO_FORCE")
            .arg(allowRerecord ? "FORGET" : "NO_FORGET");
        MythEvent me(cmd);

        gCoreContext->dispatch(me);
        return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool Dvr::UnDeleteRecording(int RecordedId,
                            int chanid, const QDateTime &recstarttsRaw)
{
    if ((RecordedId <= 0) &&
        (chanid <= 0 || !recstarttsRaw.isValid()))
        throw QString("Recorded ID or Channel ID and StartTime appears invalid.");

    RecordingInfo ri;
    if (RecordedId > 0)
        ri = RecordingInfo(RecordedId);
    else
        ri = RecordingInfo(chanid, recstarttsRaw.toUTC());

    if (ri.GetChanID() && ri.HasPathname())
    {
        QString cmd = QString("UNDELETE_RECORDING %1 %2")
            .arg(ri.GetChanID())
            .arg(ri.GetRecordingStartTime(MythDate::ISODate));
        MythEvent me(cmd);

        gCoreContext->dispatch(me);
        return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool Dvr::StopRecording(int RecordedId)
{
    if (RecordedId <= 0)
        throw QString("Recorded ID is invalid.");

    RecordingInfo ri = RecordingInfo(RecordedId);

    if (ri.GetChanID())
    {
        QString cmd = QString("STOP_RECORDING %1 %2")
                      .arg(ri.GetChanID())
                      .arg(ri.GetRecordingStartTime(MythDate::ISODate));
        MythEvent me(cmd);

        gCoreContext->dispatch(me);
        return true;
    }
    else
        throw QString("RecordID %1 not found").arg(RecordedId);

    return false;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool Dvr::ReactivateRecording(int RecordedId)
{
    if (RecordedId <= 0)
        throw QString("Recorded ID is invalid.");

    RecordingInfo ri = RecordingInfo(RecordedId);

    ri.ReactivateRecording();

    return true;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool Dvr::RescheduleRecordings(void)
{
    QString cmd = QString("RESCHEDULE_RECORDINGS");
    MythEvent me(cmd);

    gCoreContext->dispatch(me);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool Dvr::UpdateRecordedWatchedStatus ( int RecordedId,
                                        int   chanid,
                                        const QDateTime &recstarttsRaw,
                                        bool  watched)
{
    if ((RecordedId <= 0) &&
        (chanid <= 0 || !recstarttsRaw.isValid()))
        throw QString("Recorded ID or Channel ID and StartTime appears invalid.");

    // TODO Should use RecordingInfo
    ProgramInfo pi;
    if (RecordedId > 0)
        pi = ProgramInfo(RecordedId);
    else
        pi = ProgramInfo(chanid, recstarttsRaw.toUTC());

    if (pi.GetChanID() && pi.HasPathname())
    {
        pi.SaveWatched(watched);
        return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

long Dvr::GetSavedBookmark( int RecordedId,
                            int chanid,
                            const QDateTime &recstarttsRaw,
                            const QString &offsettype )
{
    if ((RecordedId <= 0) &&
        (chanid <= 0 || !recstarttsRaw.isValid()))
        throw QString("Recorded ID or Channel ID and StartTime appears invalid.");

    RecordingInfo ri;
    if (RecordedId > 0)
        ri = RecordingInfo(RecordedId);
    else
        ri = RecordingInfo(chanid, recstarttsRaw.toUTC());
    uint64_t offset;
    bool isend=true;
    uint64_t position = ri.QueryBookmark();
    if (offsettype.toLower() == "position"){
        ri.QueryKeyFramePosition(&offset, position, isend);
        return offset;
    }
    else if (offsettype.toLower() == "duration"){
        ri.QueryKeyFrameDuration(&offset, position, isend);
        return offset;
    }
    else
        return position;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool Dvr::SetSavedBookmark( int RecordedId,
                            int chanid,
                            const QDateTime &recstarttsRaw,
                            const QString &offsettype,
                            long Offset )
{
    if ((RecordedId <= 0) &&
        (chanid <= 0 || !recstarttsRaw.isValid()))
        throw QString("Recorded ID or Channel ID and StartTime appears invalid.");

    if (Offset < 0)
        throw QString("Offset must be >= 0.");

    RecordingInfo ri;
    if (RecordedId > 0)
        ri = RecordingInfo(RecordedId);
    else
        ri = RecordingInfo(chanid, recstarttsRaw.toUTC());
    uint64_t position;
    bool isend=true;
    if (offsettype.toLower() == "position"){
        if (!ri.QueryPositionKeyFrame(&position, Offset, isend))
                return false;
    }
    else if (offsettype.toLower() == "duration"){
        if (!ri.QueryDurationKeyFrame(&position, Offset, isend))
                return false;
    }
    else
        position = Offset;
    ri.SaveBookmark(position);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::CutList* Dvr::GetRecordedCutList ( int RecordedId,
                                        int chanid,
                                        const QDateTime &recstarttsRaw,
                                        const QString &offsettype )
{
    int marktype;
    if ((RecordedId <= 0) &&
        (chanid <= 0 || !recstarttsRaw.isValid()))
        throw QString("Recorded ID or Channel ID and StartTime appears invalid.");

    RecordingInfo ri;
    if (RecordedId > 0)
        ri = RecordingInfo(RecordedId);
    else
        ri = RecordingInfo(chanid, recstarttsRaw.toUTC());

    DTC::CutList* pCutList = new DTC::CutList();
    if (offsettype == "Position")
        marktype = 1;
    else if (offsettype == "Duration")
        marktype = 2;
    else
        marktype = 0;

    FillCutList(pCutList, &ri, marktype);

    return pCutList;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::CutList* Dvr::GetRecordedCommBreak ( int RecordedId,
                                          int chanid,
                                          const QDateTime &recstarttsRaw,
                                          const QString &offsettype )
{
    int marktype;
    if ((RecordedId <= 0) &&
        (chanid <= 0 || !recstarttsRaw.isValid()))
        throw QString("Recorded ID or Channel ID and StartTime appears invalid.");

    RecordingInfo ri;
    if (RecordedId > 0)
        ri = RecordingInfo(RecordedId);
    else
        ri = RecordingInfo(chanid, recstarttsRaw.toUTC());

    DTC::CutList* pCutList = new DTC::CutList();
    if (offsettype == "Position")
        marktype = 1;
    else if (offsettype == "Duration")
        marktype = 2;
    else
        marktype = 0;

    FillCommBreak(pCutList, &ri, marktype);

    return pCutList;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::CutList* Dvr::GetRecordedSeek ( int RecordedId,
                                     const QString &offsettype )
{
    MarkTypes marktype;
    if (RecordedId <= 0)
        throw QString("Recorded ID appears invalid.");

    RecordingInfo ri;
    ri = RecordingInfo(RecordedId);

    DTC::CutList* pCutList = new DTC::CutList();
    if (offsettype == "BYTES")
        marktype = MARK_GOP_BYFRAME;
    else if (offsettype == "DURATION")
        marktype = MARK_DURATION_MS;
    else
    {
        delete pCutList;
        throw QString("Type must be 'BYTES' or 'DURATION'.");
    }

    FillSeek(pCutList, &ri, marktype);

    return pCutList;
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

    nStartIndex   = (nStartIndex > 0) ? min( nStartIndex, (int)infoList.size() ) : 0;
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
    pPrograms->setAsOf          ( MythDate::current() );
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

    QList<InputInfo> inputInfoList = CardUtil::GetAllInputInfo();
    QMap<int, EncoderLink *>::Iterator iter = tvList.begin();

    for (; iter != tvList.end(); ++iter)
    {
        EncoderLink *elink = *iter;

        if (elink != NULL)
        {
            DTC::Encoder *pEncoder = pList->AddNewEncoder();

            pEncoder->setId            ( elink->GetInputID()      );
            pEncoder->setState         ( elink->GetState()        );
            pEncoder->setLocal         ( elink->IsLocal()         );
            pEncoder->setConnected     ( elink->IsConnected()     );
            pEncoder->setSleepStatus   ( elink->GetSleepStatus()  );
          //  pEncoder->setLowOnFreeSpace( elink->isLowOnFreeSpace());

            if (pEncoder->Local())
                pEncoder->setHostName( gCoreContext->GetHostName() );
            else
                pEncoder->setHostName( elink->GetHostName() );

            QList<InputInfo>::iterator it = inputInfoList.begin();
            for (; it < inputInfoList.end(); ++it)
            {
                InputInfo inputInfo = *it;
                if (inputInfo.inputid == static_cast<uint>(elink->GetInputID()))
                {
                    DTC::Input *input = pEncoder->AddNewInput();
                    FillInputInfo(input, inputInfo);
                }
            }

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

DTC::InputList* Dvr::GetInputList()
{
    DTC::InputList *pList = new DTC::InputList();

    QList<InputInfo> inputInfoList = CardUtil::GetAllInputInfo();
    QList<InputInfo>::iterator it = inputInfoList.begin();
    for (; it < inputInfoList.end(); ++it)
    {
        InputInfo inputInfo = *it;
        DTC::Input *input = pList->AddNewInput();
        FillInputInfo(input, inputInfo);
    }

    return pList;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QStringList Dvr::GetRecGroupList()
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT recgroup FROM recgroups WHERE recgroup <> 'Deleted' "
                  "ORDER BY recgroup");

    QStringList result;
    if (!query.exec())
    {
        MythDB::DBError("GetRecGroupList", query);
        return result;
    }

    while (query.next())
        result << query.value(0).toString();

    return result;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QStringList Dvr::GetRecStorageGroupList()
{
    return StorageGroup::getRecordingsGroups();
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QStringList Dvr::GetPlayGroupList()
{
    return PlayGroup::GetNames();
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::RecRuleFilterList* Dvr::GetRecRuleFilterList()
{
    DTC::RecRuleFilterList* filterList = new DTC::RecRuleFilterList();

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT filterid, description, newruledefault "
                    "FROM recordfilter ORDER BY filterid");

    if (query.exec())
    {
        while (query.next())
        {
            DTC::RecRuleFilter* ruleFilter = filterList->AddNewRecRuleFilter();
            ruleFilter->setId(query.value(0).toInt());
            ruleFilter->setDescription(QObject::tr(query.value(1).toString()
                                         .toUtf8().constData()));
        }
    }

    return filterList;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QStringList Dvr::GetTitleList(const QString& RecGroup)
{
    MSqlQuery query(MSqlQuery::InitCon());

    QString querystr = "SELECT DISTINCT title FROM recorded "
                       "WHERE deletepending = 0";

    if (!RecGroup.isEmpty())
        querystr += " AND recgroup = :RECGROUP";
    else
        querystr += " AND recgroup != 'Deleted'";

    querystr += " ORDER BY title";

    query.prepare(querystr);

    if (!RecGroup.isEmpty())
        query.bindValue(":RECGROUP", RecGroup);

    QStringList result;
    if (!query.exec())
    {
        MythDB::DBError("GetTitleList recorded", query);
        return result;
    }

    while (query.next())
        result << query.value(0).toString();

    return result;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::TitleInfoList* Dvr::GetTitleInfoList()
{
    MSqlQuery query(MSqlQuery::InitCon());

    QString querystr = QString(
        "SELECT title, inetref, count(title) as count "
        "    FROM recorded AS r "
        "    JOIN recgroups AS g ON r.recgroupid = g.recgroupid "
        "    WHERE g.recgroup != 'LiveTV' "
        "    AND r.deletepending = 0 "
        "    GROUP BY title, inetref "
        "    ORDER BY title");

    query.prepare(querystr);

    DTC::TitleInfoList *pTitleInfos = new DTC::TitleInfoList();
    if (!query.exec())
    {
        MythDB::DBError("GetTitleList recorded", query);
        return pTitleInfos;
    }

    while (query.next())
    {
        DTC::TitleInfo *pTitleInfo = pTitleInfos->AddNewTitleInfo();

        pTitleInfo->setTitle(query.value(0).toString());
        pTitleInfo->setInetref(query.value(1).toString());
        pTitleInfo->setCount(query.value(2).toInt());
    }

    return pTitleInfos;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::ProgramList* Dvr::GetUpcomingList( int  nStartIndex,
                                        int  nCount,
                                        bool bShowAll,
                                        int  nRecordId,
                                        int  nRecStatus )
{
    RecordingList  recordingList; // Auto-delete deque
    RecList  tmpList; // Standard deque, objects must be deleted

    if (nRecordId <= 0)
        nRecordId = -1;

    // NOTE: Fetching this information directly from the schedule is
    //       significantly faster than using ProgramInfo::LoadFromScheduler()
    Scheduler *scheduler = dynamic_cast<Scheduler*>(gCoreContext->GetScheduler());
    if (scheduler)
        scheduler->GetAllPending(tmpList, nRecordId);

    // Sort the upcoming into only those which will record
    RecList::iterator it = tmpList.begin();
    for(; it < tmpList.end(); ++it)
    {
        if ((nRecStatus != 0) &&
            ((*it)->GetRecordingStatus() != nRecStatus))
        {
            delete *it;
            *it = NULL;
            continue;
        }

        if (!bShowAll && ((((*it)->GetRecordingStatus() >= RecStatus::Pending) &&
                           ((*it)->GetRecordingStatus() <= RecStatus::WillRecord)) ||
                          ((*it)->GetRecordingStatus() == RecStatus::Conflict)) &&
            ((*it)->GetRecordingEndTime() > MythDate::current()))
        {
            recordingList.push_back(new RecordingInfo(**it));
        }
        else if (bShowAll &&
                 ((*it)->GetRecordingEndTime() > MythDate::current()))
        {
            recordingList.push_back(new RecordingInfo(**it));
        }

        delete *it;
        *it = NULL;
    }

    // ----------------------------------------------------------------------
    // Build Response
    // ----------------------------------------------------------------------

    DTC::ProgramList *pPrograms = new DTC::ProgramList();

    nStartIndex   = (nStartIndex > 0) ? min( nStartIndex, (int)recordingList.size() ) : 0;
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
    pPrograms->setAsOf          ( MythDate::current() );
    pPrograms->setVersion       ( MYTH_BINARY_VERSION );
    pPrograms->setProtoVer      ( MYTH_PROTO_VERSION  );

    return pPrograms;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::ProgramList* Dvr::GetConflictList( int  nStartIndex,
                                        int  nCount,
                                        int  nRecordId       )
{
    RecordingList  recordingList; // Auto-delete deque
    RecList  tmpList; // Standard deque, objects must be deleted

    if (nRecordId <= 0)
        nRecordId = -1;

    // NOTE: Fetching this information directly from the schedule is
    //       significantly faster than using ProgramInfo::LoadFromScheduler()
    Scheduler *scheduler = dynamic_cast<Scheduler*>(gCoreContext->GetScheduler());
    if (scheduler)
        scheduler->GetAllPending(tmpList, nRecordId);

    // Sort the upcoming into only those which are conflicts
    RecList::iterator it = tmpList.begin();
    for(; it < tmpList.end(); ++it)
    {
        if (((*it)->GetRecordingStatus() == RecStatus::Conflict) &&
            ((*it)->GetRecordingStartTime() >= MythDate::current()))
        {
            recordingList.push_back(new RecordingInfo(**it));
        }
        delete *it;
        *it = NULL;
    }

    // ----------------------------------------------------------------------
    // Build Response
    // ----------------------------------------------------------------------

    DTC::ProgramList *pPrograms = new DTC::ProgramList();

    nStartIndex   = (nStartIndex > 0) ? min( nStartIndex, (int)recordingList.size() ) : 0;
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
    pPrograms->setAsOf          ( MythDate::current() );
    pPrograms->setVersion       ( MYTH_BINARY_VERSION );
    pPrograms->setProtoVer      ( MYTH_PROTO_VERSION  );

    return pPrograms;
}

uint Dvr::AddRecordSchedule   (
                               QString   sTitle,
                               QString   sSubtitle,
                               QString   sDescription,
                               QString   sCategory,
                               QDateTime recstarttsRaw,
                               QDateTime recendtsRaw,
                               QString   sSeriesId,
                               QString   sProgramId,
                               int       nChanId,
                               QString   sStation,
                               int       nFindDay,
                               QTime     tFindTime,
                               int       nParentId,
                               bool      bInactive,
                               uint      nSeason,
                               uint      nEpisode,
                               QString   sInetref,
                               QString   sType,
                               QString   sSearchType,
                               int       nRecPriority,
                               uint      nPreferredInput,
                               int       nStartOffset,
                               int       nEndOffset,
                               QString   sDupMethod,
                               QString   sDupIn,
                               uint      nFilter,
                               QString   sRecProfile,
                               QString   sRecGroup,
                               QString   sStorageGroup,
                               QString   sPlayGroup,
                               bool      bAutoExpire,
                               int       nMaxEpisodes,
                               bool      bMaxNewest,
                               bool      bAutoCommflag,
                               bool      bAutoTranscode,
                               bool      bAutoMetaLookup,
                               bool      bAutoUserJob1,
                               bool      bAutoUserJob2,
                               bool      bAutoUserJob3,
                               bool      bAutoUserJob4,
                               int       nTranscoder)
{
    QDateTime recstartts = recstarttsRaw.toUTC();
    QDateTime recendts = recendtsRaw.toUTC();
    RecordingRule rule;
    rule.LoadTemplate("Default");

    if (sType.isEmpty())
        sType = "single";

    if (sSearchType.isEmpty())
        sSearchType = "none";

    if (sDupMethod.isEmpty())
        sDupMethod = "subtitleanddescription";

    if (sDupIn.isEmpty())
        sDupIn = "all";

    rule.m_title = sTitle;
    rule.m_subtitle = sSubtitle;
    rule.m_description = sDescription;

    rule.m_startdate = recstartts.date();
    rule.m_starttime = recstartts.time();
    rule.m_enddate = recendts.date();
    rule.m_endtime = recendts.time();

    rule.m_type = recTypeFromString(sType);
    rule.m_searchType = searchTypeFromString(sSearchType);
    rule.m_dupMethod = dupMethodFromString(sDupMethod);
    rule.m_dupIn = dupInFromString(sDupIn);

    if (sRecProfile.isEmpty())
        sRecProfile = "Default";

    if (sRecGroup.isEmpty())
        sRecGroup = "Default";

    if (sStorageGroup.isEmpty())
        sStorageGroup = "Default";

    if (sPlayGroup.isEmpty())
        sPlayGroup = "Default";

    rule.m_category = sCategory;
    rule.m_seriesid = sSeriesId;
    rule.m_programid = sProgramId;

    rule.m_channelid = nChanId;
    rule.m_station = sStation;

    rule.m_findday = nFindDay;
    rule.m_findtime = tFindTime;

    rule.m_recProfile = sRecProfile;
    rule.m_recGroupID = RecordingInfo::GetRecgroupID(sRecGroup);
    if (rule.m_recGroupID == 0)
        rule.m_recGroupID = RecordingInfo::kDefaultRecGroup;
    rule.m_storageGroup = sStorageGroup;
    rule.m_playGroup = sPlayGroup;

    rule.m_parentRecID = nParentId;
    rule.m_isInactive = bInactive;

    rule.m_season = nSeason;
    rule.m_episode = nEpisode;
    rule.m_inetref = sInetref;

    rule.m_recPriority = nRecPriority;
    rule.m_prefInput = nPreferredInput;
    rule.m_startOffset = nStartOffset;
    rule.m_endOffset = nEndOffset;
    rule.m_filter = nFilter;

    rule.m_autoExpire = bAutoExpire;
    rule.m_maxEpisodes = nMaxEpisodes;
    rule.m_maxNewest = bMaxNewest;

    rule.m_autoCommFlag = bAutoCommflag;
    rule.m_autoTranscode = bAutoTranscode;
    rule.m_autoMetadataLookup = bAutoMetaLookup;

    rule.m_autoUserJob1 = bAutoUserJob1;
    rule.m_autoUserJob2 = bAutoUserJob2;
    rule.m_autoUserJob3 = bAutoUserJob3;
    rule.m_autoUserJob4 = bAutoUserJob4;

    rule.m_transcoder = nTranscoder;

    QString msg;
    if (!rule.IsValid(msg))
        throw msg;

    rule.Save();

    uint recid = rule.m_recordID;

    return recid;
}

bool Dvr::UpdateRecordSchedule ( uint      nRecordId,
                                 QString   sTitle,
                                 QString   sSubtitle,
                                 QString   sDescription,
                                 QString   sCategory,
                                 QDateTime dStartTimeRaw,
                                 QDateTime dEndTimeRaw,
                                 QString   sSeriesId,
                                 QString   sProgramId,
                                 int       nChanId,
                                 QString   sStation,
                                 int       nFindDay,
                                 QTime     tFindTime,
                                 bool      bInactive,
                                 uint      nSeason,
                                 uint      nEpisode,
                                 QString   sInetref,
                                 QString   sType,
                                 QString   sSearchType,
                                 int       nRecPriority,
                                 uint      nPreferredInput,
                                 int       nStartOffset,
                                 int       nEndOffset,
                                 QString   sDupMethod,
                                 QString   sDupIn,
                                 uint      nFilter,
                                 QString   sRecProfile,
                                 QString   sRecGroup,
                                 QString   sStorageGroup,
                                 QString   sPlayGroup,
                                 bool      bAutoExpire,
                                 int       nMaxEpisodes,
                                 bool      bMaxNewest,
                                 bool      bAutoCommflag,
                                 bool      bAutoTranscode,
                                 bool      bAutoMetaLookup,
                                 bool      bAutoUserJob1,
                                 bool      bAutoUserJob2,
                                 bool      bAutoUserJob3,
                                 bool      bAutoUserJob4,
                                 int       nTranscoder)
{
    if (nRecordId <= 0 )
        throw QString("Record ID is invalid.");

    RecordingRule pRule;
    pRule.m_recordID = nRecordId;
    pRule.Load();

    if (!pRule.IsLoaded())
        throw QString("Record ID does not exist.");

    QDateTime recstartts = dStartTimeRaw.toUTC();
    QDateTime recendts = dEndTimeRaw.toUTC();

    pRule.m_isInactive = bInactive;
    if (sType.isEmpty())
        sType = "single";

    if (sSearchType.isEmpty())
        sSearchType = "none";

    if (sDupMethod.isEmpty())
        sDupMethod = "subtitleanddescription";

    if (sDupIn.isEmpty())
        sDupIn = "all";

    pRule.m_type = recTypeFromString(sType);
    pRule.m_searchType = searchTypeFromString(sSearchType);
    pRule.m_dupMethod = dupMethodFromString(sDupMethod);
    pRule.m_dupIn = dupInFromString(sDupIn);

    if (sRecProfile.isEmpty())
        sRecProfile = "Default";

    if (sRecGroup.isEmpty())
        sRecGroup = "Default";

    if (sStorageGroup.isEmpty())
        sStorageGroup = "Default";

    if (sPlayGroup.isEmpty())
        sPlayGroup = "Default";

    if (!sTitle.isEmpty())
        pRule.m_title = sTitle;

    if (!sSubtitle.isEmpty())
        pRule.m_subtitle = sSubtitle;

    if(!sDescription.isEmpty())
        pRule.m_description = sDescription;

    if (!sCategory.isEmpty())
        pRule.m_category = sCategory;

    if (!sSeriesId.isEmpty())
        pRule.m_seriesid = sSeriesId;

    if (!sProgramId.isEmpty())
        pRule.m_programid = sProgramId;

    if (nChanId)
        pRule.m_channelid = nChanId;
    if (!sStation.isEmpty())
        pRule.m_station = sStation;

    pRule.m_startdate = recstartts.date();
    pRule.m_starttime = recstartts.time();
    pRule.m_enddate = recendts.date();
    pRule.m_endtime = recendts.time();

    pRule.m_findday = nFindDay;
    pRule.m_findtime = tFindTime;

    pRule.m_recProfile = sRecProfile;
    pRule.m_recGroupID = RecordingInfo::GetRecgroupID(sRecGroup);
    if (pRule.m_recGroupID == 0)
        pRule.m_recGroupID = RecordingInfo::kDefaultRecGroup;
    pRule.m_storageGroup = sStorageGroup;
    pRule.m_playGroup = sPlayGroup;

    pRule.m_isInactive = bInactive;

    pRule.m_season = nSeason;
    pRule.m_episode = nEpisode;
    pRule.m_inetref = sInetref;

    pRule.m_recPriority = nRecPriority;
    pRule.m_prefInput = nPreferredInput;
    pRule.m_startOffset = nStartOffset;
    pRule.m_endOffset = nEndOffset;
    pRule.m_filter = nFilter;

    pRule.m_autoExpire = bAutoExpire;
    pRule.m_maxEpisodes = nMaxEpisodes;
    pRule.m_maxNewest = bMaxNewest;

    pRule.m_autoCommFlag = bAutoCommflag;
    pRule.m_autoTranscode = bAutoTranscode;
    pRule.m_autoMetadataLookup = bAutoMetaLookup;

    pRule.m_autoUserJob1 = bAutoUserJob1;
    pRule.m_autoUserJob2 = bAutoUserJob2;
    pRule.m_autoUserJob3 = bAutoUserJob3;
    pRule.m_autoUserJob4 = bAutoUserJob4;

    pRule.m_transcoder = nTranscoder;

    QString msg;
    if (!pRule.IsValid(msg))
        throw msg;

    bool bResult = pRule.Save();

    return bResult;
}

bool Dvr::RemoveRecordSchedule ( uint nRecordId )
{
    bool bResult = false;

    if (nRecordId <= 0 )
        throw QString("Record ID does not exist.");

    RecordingRule pRule;
    pRule.m_recordID = nRecordId;

    bResult = pRule.Delete();

    return bResult;
}

bool Dvr::AddDontRecordSchedule(int nChanId, const QDateTime &dStartTime,
                                bool bNeverRecord)
{
    bool bResult = true;

    if (nChanId <= 0 || !dStartTime.isValid())
        throw QString("Program does not exist.");

    ProgramInfo *pi = LoadProgramFromProgram(nChanId, dStartTime.toUTC());

    if (!pi)
        throw QString("Program does not exist.");

    // Why RecordingInfo instead of ProgramInfo? Good question ...
    RecordingInfo recInfo = RecordingInfo(*pi);

    delete pi;

    if (bNeverRecord)
    {
        recInfo.ApplyNeverRecord();
    }
    else
        recInfo.ApplyRecordStateChange(kDontRecord);

    return bResult;
}

DTC::RecRuleList* Dvr::GetRecordScheduleList( int nStartIndex,
                                              int nCount,
                                              const QString  &Sort,
                                              bool Descending )
{
    Scheduler::SchedSortColumn sortingColumn;
    if (Sort.toLower() == "lastrecorded")
        sortingColumn = Scheduler::kSortLastRecorded;
    else if (Sort.toLower() == "nextrecording")
        sortingColumn = Scheduler::kSortNextRecording;
    else if (Sort.toLower() == "title")
        sortingColumn = Scheduler::kSortTitle;
    else if (Sort.toLower() == "priority")
        sortingColumn = Scheduler::kSortPriority;
    else if (Sort.toLower() == "type")
        sortingColumn = Scheduler::kSortType;
    else
        sortingColumn = Scheduler::kSortTitle;

    RecList recList;
    Scheduler::GetAllScheduled(recList, sortingColumn, !Descending);

    // ----------------------------------------------------------------------
    // Build Response
    // ----------------------------------------------------------------------

    DTC::RecRuleList *pRecRules = new DTC::RecRuleList();

    nStartIndex   = (nStartIndex > 0) ? min( nStartIndex, (int)recList.size() ) : 0;
    nCount        = (nCount > 0) ? min( nCount, (int)recList.size() ) : recList.size();
    int nEndIndex = min((nStartIndex + nCount), (int)recList.size() );

    for( int n = nStartIndex; n < nEndIndex; n++)
    {
        RecordingInfo *info = recList[n];

        if (info != NULL)
        {
            DTC::RecRule *pRecRule = pRecRules->AddNewRecRule();

            FillRecRuleInfo( pRecRule, info->GetRecordingRule() );
        }
    }

    // ----------------------------------------------------------------------

    pRecRules->setStartIndex    ( nStartIndex     );
    pRecRules->setCount         ( nCount          );
    pRecRules->setTotalAvailable( recList.size() );
    pRecRules->setAsOf          ( MythDate::current() );
    pRecRules->setVersion       ( MYTH_BINARY_VERSION );
    pRecRules->setProtoVer      ( MYTH_PROTO_VERSION  );

    while (!recList.empty())
    {
        delete recList.back();
        recList.pop_back();
    }

    return pRecRules;
}

DTC::RecRule* Dvr::GetRecordSchedule( uint      nRecordId,
                                      QString   sTemplate,
                                      int       nRecordedId,
                                      int       nChanId,
                                      QDateTime dStartTimeRaw,
                                      bool      bMakeOverride )
{
    RecordingRule rule;
    QDateTime dStartTime = dStartTimeRaw.toUTC();

    if (nRecordId > 0)
    {
        rule.m_recordID = nRecordId;
        if (!rule.Load())
            throw QString("Record ID does not exist.");
    }
    else if (!sTemplate.isEmpty())
    {
        if (!rule.LoadTemplate(sTemplate))
            throw QString("Template does not exist.");
    }
    else if (nRecordedId > 0) // Loads from the Recorded/Recorded Program Table
    {
        // Despite the use of ProgramInfo, this only applies to Recordings.
        ProgramInfo recInfo(nRecordedId);
        if (!rule.LoadByProgram(&recInfo))
            throw QString("Recording does not exist");
    }
    else if (nChanId > 0 && dStartTime.isValid()) // Loads from Program Table, should NOT be used with recordings
    {
        // Despite the use of RecordingInfo, this only applies to programs in the
        // present or future, not to recordings? Confused yet?
        RecordingInfo::LoadStatus status;
        RecordingInfo info(nChanId, dStartTime, false, 0, &status);
        if (status != RecordingInfo::kFoundProgram)
            throw QString("Program does not exist.");
        RecordingRule *pRule = info.GetRecordingRule();
        if (bMakeOverride && rule.m_type != kSingleRecord &&
            rule.m_type != kOverrideRecord && rule.m_type != kDontRecord)
            pRule->MakeOverride();
        rule = *pRule;
    }
    else
    {
        throw QString("Invalid request.");
    }

    DTC::RecRule *pRecRule = new DTC::RecRule();
    FillRecRuleInfo( pRecRule, &rule );

    return pRecRule;
}

bool Dvr::EnableRecordSchedule ( uint nRecordId )
{
    bool bResult = false;

    if (nRecordId <= 0 )
        throw QString("Record ID appears invalid.");

    RecordingRule pRule;
    pRule.m_recordID = nRecordId;
    pRule.Load();

    if (pRule.IsLoaded())
    {
        pRule.m_isInactive = false;
        bResult = pRule.Save();
    }

    return bResult;
}

bool Dvr::DisableRecordSchedule( uint nRecordId )
{
    bool bResult = false;

    if (nRecordId <= 0 )
        throw QString("Record ID appears invalid.");

    RecordingRule pRule;
    pRule.m_recordID = nRecordId;
    pRule.Load();

    if (pRule.IsLoaded())
    {
        pRule.m_isInactive = true;
        bResult = pRule.Save();
    }

    return bResult;
}

int Dvr::RecordedIdForPathname(const QString & pathname)
{
    uint recordedid;

    if (!ProgramInfo::QueryRecordedIdFromPathname(pathname, recordedid))
        return -1;

    return recordedid;
}

QString Dvr::RecStatusToString(int RecStatus)
{
    RecStatus::Type type = static_cast<RecStatus::Type>(RecStatus);
    return RecStatus::toString(type);
}

QString Dvr::RecStatusToDescription(int RecStatus, int recType,
                                    const QDateTime &StartTime)
{
    //if (!StartTime.isValid())
    //    throw QString("StartTime appears invalid.");
    RecStatus::Type rsType = static_cast<RecStatus::Type>(RecStatus);
    RecordingType recordingType = static_cast<RecordingType>(recType);
    return RecStatus::toDescription(rsType, recordingType, StartTime);
}

QString Dvr::RecTypeToString(QString recType)
{
    bool ok;
    RecordingType enumType = static_cast<RecordingType>(recType.toInt(&ok, 10));
    if (ok)
        return toString(enumType);
    // RecordingType type = static_cast<RecordingType>(recType);
    return toString(recTypeFromString(recType));
}

QString Dvr::RecTypeToDescription(QString recType)
{
    bool ok;
    RecordingType enumType = static_cast<RecordingType>(recType.toInt(&ok, 10));
    if (ok)
        return toDescription(enumType);
    // RecordingType type = static_cast<RecordingType>(recType);
    return toDescription(recTypeFromString(recType));
}

QString Dvr::DupInToString(QString DupIn)
{
    // RecordingDupInType type= static_cast<RecordingDupInType>(DupIn);
    // return toString(type);
    return toString(dupInFromString(DupIn));
}

QString Dvr::DupInToDescription(QString DupIn)
{
    // RecordingDupInType type= static_cast<RecordingDupInType>(DupIn);
    //return toDescription(type);
    return toDescription(dupInFromString(DupIn));
}

QString Dvr::DupMethodToString(QString DupMethod)
{
    // RecordingDupMethodType method = static_cast<RecordingDupMethodType>(DupMethod);
    return toString(dupMethodFromString(DupMethod));
}

QString Dvr::DupMethodToDescription(QString DupMethod)
{
    // RecordingDupMethodType method = static_cast<RecordingDupMethodType>(DupMethod);
    return toDescription(dupMethodFromString(DupMethod));
}
