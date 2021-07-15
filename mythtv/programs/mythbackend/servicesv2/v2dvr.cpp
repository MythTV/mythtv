//////////////////////////////////////////////////////////////////////////////
// Program Name: v2dvr.cpp
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

#include "v2dvr.h"
#include "libmythbase/http/mythhttpmetaservice.h"
#include "autoexpire.h"
#include "programinfo.h"
#include "programdata.h"
#include "channelutil.h"
#include "scheduler.h"
#include "mythversion.h"
#include "mythcorecontext.h"
#include "mythscheduler.h"
#include "jobqueue.h"
#include "v2serviceUtil.h"
#include "tv_rec.h"
#include "cardutil.h"
#include "encoderlink.h"
#include "storagegroup.h"
#include "playgroup.h"

extern QMap<int, EncoderLink *> tvList;
extern AutoExpire  *expirer;

// This will be initialised in a thread safe manner on first use
Q_GLOBAL_STATIC_WITH_ARGS(MythHTTPMetaService, s_service,
    (DVR_HANDLE, V2Dvr::staticMetaObject, std::bind(&V2Dvr::RegisterCustomTypes)))

void V2Dvr::RegisterCustomTypes()
{
    qRegisterMetaType<V2ProgramList*>("V2ProgramList");
    qRegisterMetaType<V2Program*>("V2Program");
    qRegisterMetaType<V2CutList*>("V2CutList");
    qRegisterMetaType<V2MarkupList*>("V2MarkupList");
    qRegisterMetaType<V2EncoderList*>("V2EncoderList");
    qRegisterMetaType<V2InputList*>("V2InputList");
    qRegisterMetaType<V2RecRuleFilterList*>("V2RecRuleFilterList");
    qRegisterMetaType<V2TitleInfoList*>("V2TitleInfoList");
}

V2Dvr::V2Dvr()
  : MythHTTPService(s_service)
{
}

V2ProgramList* V2Dvr::GetExpiringList( int nStartIndex,
                                        int nCount      )
{
    pginfolist_t  infoList;

    if (expirer)
        expirer->GetAllExpiring( infoList );

    // ----------------------------------------------------------------------
    // Build Response
    // ----------------------------------------------------------------------

    auto *pPrograms = new V2ProgramList();

    nStartIndex   = (nStartIndex > 0) ? std::min( nStartIndex, (int)infoList.size() ) : 0;
    nCount        = (nCount > 0) ? std::min( nCount, (int)infoList.size() ) : infoList.size();
    int nEndIndex = std::min((nStartIndex + nCount), (int)infoList.size() );

    for( int n = nStartIndex; n < nEndIndex; n++)
    {
        ProgramInfo *pInfo = infoList[ n ];

        if (pInfo != nullptr)
        {
            V2Program *pProgram = pPrograms->AddNewProgram();

            V2FillProgramInfo( pProgram, pInfo, true );

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

V2ProgramList* V2Dvr::GetRecordedList( bool           bDescending,
                                        int            nStartIndex,
                                        int            nCount,
                                        const QString &sTitleRegEx,
                                        const QString &sRecGroup,
                                        const QString &sStorageGroup,
                                        const QString &sCategory,
                                        const QString &sSort
                                      )
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

    LoadFromRecorded( progList, false, inUseMap, isJobRunning, recMap, desc, sSort );

    QMap< QString, ProgramInfo* >::iterator mit = recMap.begin();

    for (; mit != recMap.end(); mit = recMap.erase(mit))
        delete *mit;

    // ----------------------------------------------------------------------
    // Build Response
    // ----------------------------------------------------------------------

    auto *pPrograms = new V2ProgramList();
    int nAvailable = 0;

    int nMax      = (nCount > 0) ? nCount : progList.size();

    nAvailable = 0;
    nCount = 0;

    QRegularExpression rTitleRegEx
        { sTitleRegEx, QRegularExpression::CaseInsensitiveOption };

    for (auto *pInfo : progList)
    {
        if (pInfo->IsDeletePending() ||
            (!sTitleRegEx.isEmpty() && !pInfo->GetTitle().contains(rTitleRegEx)) ||
            (!sRecGroup.isEmpty() && sRecGroup != pInfo->GetRecordingGroup()) ||
            (!sStorageGroup.isEmpty() && sStorageGroup != pInfo->GetStorageGroup()) ||
            (!sCategory.isEmpty() && sCategory != pInfo->GetCategory()))
            continue;

        if ((nAvailable < nStartIndex) ||
            (nCount >= nMax))
        {
            ++nAvailable;
            continue;
        }

        ++nAvailable;
        ++nCount;

        V2Program *pProgram = pPrograms->AddNewProgram();

        V2FillProgramInfo( pProgram, pInfo, true );
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

V2ProgramList* V2Dvr::GetOldRecordedList( bool             bDescending,
                                           int              nStartIndex,
                                           int              nCount,
                                           const QDateTime &sStartTime,
                                           const QDateTime &sEndTime,
                                           const QString   &sTitle,
                                           const QString   &sSeriesId,
                                           int              nRecordId,
                                           const QString   &sSort)
{
    if (!sStartTime.isNull() && !sStartTime.isValid())
        throw QString("StartTime is invalid");

    if (!sEndTime.isNull() && !sEndTime.isValid())
        throw QString("EndTime is invalid");

    const QDateTime& dtStartTime = sStartTime;
    const QDateTime& dtEndTime   = sEndTime;

    if (!sEndTime.isNull() && dtEndTime < dtStartTime)
        throw QString("EndTime is before StartTime");

    // ----------------------------------------------------------------------
    // Build SQL statement for Program Listing
    // ----------------------------------------------------------------------

    ProgramList  progList;
    MSqlBindings bindings;
    QString      sSQL;

    if (!dtStartTime.isNull())
    {
        sSQL += " AND endtime >= :StartDate ";
        bindings[":StartDate"] = dtStartTime;
    }

    if (!dtEndTime.isNull())
    {
        sSQL += " AND starttime <= :EndDate ";
        bindings[":EndDate"] = dtEndTime;
    }

    QStringList clause;

    if (nRecordId > 0)
    {
        clause << "recordid = :RecordId";
        bindings[":RecordId"] = nRecordId;
    }

    if (!sTitle.isEmpty())
    {
        clause << "title = :Title";
        bindings[":Title"] = sTitle;
    }

    if (!sSeriesId.isEmpty())
    {
        clause << "seriesid = :SeriesId";
        bindings[":SeriesId"] = sSeriesId;
    }

    if (!clause.isEmpty())
    {
        sSQL += QString(" AND (%1) ").arg(clause.join(" OR "));
    }

    if (sSort == "starttime")
        sSQL += "ORDER BY starttime ";    // NOLINT(bugprone-branch-clone)
    else if (sSort == "title")
        sSQL += "ORDER BY title ";
    else
        sSQL += "ORDER BY starttime ";

    if (bDescending)
        sSQL += "DESC ";
    else
        sSQL += "ASC ";

    uint nTotalAvailable = (nStartIndex == 0) ? 1 : 0;
    LoadFromOldRecorded( progList, sSQL, bindings,
                         (uint)nStartIndex, (uint)nCount, nTotalAvailable );

    // ----------------------------------------------------------------------
    // Build Response
    // ----------------------------------------------------------------------

    auto *pPrograms = new V2ProgramList();

    nCount        = (int)progList.size();
    int nEndIndex = (int)progList.size();

    for( int n = 0; n < nEndIndex; n++)
    {
        ProgramInfo *pInfo = progList[ n ];

        V2Program *pProgram = pPrograms->AddNewProgram();

        V2FillProgramInfo( pProgram, pInfo, true );
    }

    // ----------------------------------------------------------------------

    pPrograms->setStartIndex    ( nStartIndex     );
    pPrograms->setCount         ( nCount          );
    pPrograms->setTotalAvailable( nTotalAvailable );
    pPrograms->setAsOf          ( MythDate::current() );
    pPrograms->setVersion       ( MYTH_BINARY_VERSION );
    pPrograms->setProtoVer      ( MYTH_PROTO_VERSION  );

    return pPrograms;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

V2Program* V2Dvr::GetRecorded(int RecordedId,
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

    auto *pProgram = new V2Program();
    V2FillProgramInfo( pProgram, &pi, true );

    return pProgram;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool V2Dvr::AddRecordedCredits(int RecordedId, const QJsonObject &jsonObj)
{
    // Verify the corresponding recording exists
    RecordingInfo ri(RecordedId);
    if (!ri.HasPathname())
        throw QString("AddRecordedCredits: recordedid %1 does "
                      "not exist.").arg(RecordedId);

    DBCredits* credits = V2jsonCastToCredits(jsonObj);
    if (credits == nullptr)
        throw QString("AddRecordedCredits: Failed to parse cast from json.");

    MSqlQuery query(MSqlQuery::InitCon());
    for (auto & person : *credits)
    {
        if (!person.InsertDB(query, ri.GetChanID(),
                             ri.GetScheduledStartTime(), true))
            throw QString("AddRecordedCredits: Failed to add credit "
                          "%1 to DB").arg(person.toString());
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

int V2Dvr::AddRecordedProgram(const QJsonObject &jsonObj)
{
    QJsonObject root      = jsonObj;
    QJsonObject program   = root["Program"].toObject();
    QJsonObject channel   = program["Channel"].toObject();
    QJsonObject recording = program["Recording"].toObject();
    QJsonObject cast      = program["Cast"].toObject();

    auto     *pi = new ProgInfo();
    int       chanid = channel.value("ChanId").toString("0").toUInt();
    QString   hostname = program["HostName"].toString("");

    if (ChannelUtil::GetChanNum(chanid).isEmpty())
        throw QString("AddRecordedProgram: chanid %1 does "
                      "not exist.").arg(chanid);

    pi->m_title           = program.value("Title").toString("");
    pi->m_subtitle        = program.value("SubTitle").toString("");
    pi->m_description     = program.value("Description").toString("");
    pi->m_category        = program.value("Category").toString("");
    pi->m_starttime       = QDateTime::fromString(program.value("StartTime")
                                                  .toString(""), Qt::ISODate);
    pi->m_endtime         = QDateTime::fromString(program.value("EndTime")
                                                  .toString(""), Qt::ISODate);
    pi->m_originalairdate = QDate::fromString(program.value("Airdate").toString(),
                                              Qt::ISODate);
    pi->m_airdate       = pi->m_originalairdate.year();
    pi->m_partnumber    = program.value("PartNumber").toString("0").toUInt();
    pi->m_parttotal     = program.value("PartTotal").toString("0").toUInt();
    pi->m_syndicatedepisodenumber = "";
    pi->m_subtitleType  = ProgramInfo::SubtitleTypesFromNames
                          (program.value("SubPropNames").toString(""));
    pi->m_audioProps    = ProgramInfo::AudioPropertiesFromNames
                          (program.value("AudioPropNames").toString(""));
    pi->m_videoProps    = ProgramInfo::VideoPropertiesFromNames
                          (program.value("VideoPropNames").toString(""));
    pi->m_stars         = program.value("Stars").toString("0.0").toFloat();
    pi->m_categoryType  = string_to_myth_category_type(program.value("CatType").toString(""));
    pi->m_seriesId      = program.value("SeriesId").toString("");
    pi->m_programId     = program.value("ProgramId").toString("");
    pi->m_inetref       = program.value("Inetref").toString("");
    pi->m_previouslyshown = false;
    pi->m_listingsource = 0;
//    pi->m_ratings =
//    pi->m_genres =
    pi->m_season        = program.value("Season").toString("0").toUInt();
    pi->m_episode       = program.value("Episode").toString("0").toUInt();
    pi->m_totalepisodes = program.value("TotalEpisodes").toString("0").toUInt();

    pi->m_channel        = channel.value("ChannelName").toString("");

    pi->m_startts        = recording.value("StartTs").toString("");
    pi->m_endts          = recording.value("EndTs").toString("");
    QDateTime recstartts = QDateTime::fromString(pi->m_startts, Qt::ISODate);
    QDateTime recendts   = QDateTime::fromString(pi->m_endts, Qt::ISODate);

    pi->m_title_pronounce = "";
    pi->m_credits = V2jsonCastToCredits(cast);
    pi->m_showtype  = "";
    pi->m_colorcode = "";
    pi->m_clumpidx  = "";
    pi->m_clumpmax  = "";

    // pi->m_ratings =

    /* Create a recordedprogram DB entry. */
    MSqlQuery query(MSqlQuery::InitCon());
    if (!pi->InsertDB(query, chanid, true))
    {
        throw QString("AddRecordedProgram: "
                      "Failed to add recordedprogram entry.");
    }

    /* Create recorded DB entry */
    RecordingInfo ri(pi->m_title, pi->m_title,
                     pi->m_subtitle, pi->m_subtitle,
                     pi->m_description,
                     pi->m_season, pi->m_episode,
                     pi->m_totalepisodes,
                     pi->m_syndicatedepisodenumber,
                     pi->m_category,
                     chanid,
                     channel.value("ChanNum").toString("0"),
                     channel.value("CallSign").toString(""),
                     pi->m_channel,
                     recording.value("RecGroup").toString(""),
                     recording.value("PlayGroup").toString(""),
                     hostname,
                     recording.value("StorageGroup").toString(""),
                     pi->m_airdate,
                     pi->m_partnumber,
                     pi->m_parttotal,
                     pi->m_seriesId,
                     pi->m_programId,
                     pi->m_inetref,
                     pi->m_categoryType,
                     recording.value("Priority").toString("0").toInt(),
                     pi->m_starttime,
                     pi->m_endtime,
                     recstartts,
                     recendts,
                     pi->m_stars,
                     pi->m_originalairdate,
                     program.value("Repeat").toString("false").toLower() == "true",
                     static_cast<RecStatus::Type>(recording.value("Status").toString("-3").toInt()),
                     false, // reactivate
                     recording.value("RecordedId").toString("0").toInt(),
                     0, // parentid
                     static_cast<RecordingType>(recording.value("RecType").toString("0").toInt()),
                     static_cast<RecordingDupInType>(recording.value("DupInType").toString("0").toInt()),
                     static_cast<RecordingDupMethodType>(recording.value("DupMethod").toString("0").toInt()),
                     channel.value("SourceId").toString("0").toUInt(),
                     channel.value("InputId").toString("0").toUInt(),
                     0, // findid
                     channel.value("CommFree").toString("false").toLower() == "true",
                     pi->m_subtitleType,
                     pi->m_videoProps,
                     pi->m_audioProps,
                     false, // future
                     0, // schedorder
                     0, // mplexid
                     0, // sgroupid,
                     recording.value("EncoderName").toString(""));

    ri.ProgramFlagsFromNames(program.value("ProgramFlagNames").toString(""));

    QString filename = program.value("FileName").toString("");
    QString ext("");
    int idx = filename.lastIndexOf('.');
    if (idx > 0)
        ext = filename.right(filename.size() - idx - 1);
    // Inserts this RecordingInfo into the database as an existing recording
    if (!ri.InsertRecording(ext, true))
        throw QString("Failed to create RecordingInfo database entry. "
                      "Non unique starttime?");

    ri.InsertFile();

    ri.SaveAutoExpire(ri.IsAutoExpirable() ?
                      kNormalAutoExpire : kDisableAutoExpire);
    ri.SavePreserve(ri.IsPreserved());
    ri.SaveCommFlagged(ri.IsCommercialFlagged() ?
                       COMM_FLAG_DONE : COMM_FLAG_NOT_FLAGGED);
    ri.SaveWatched(ri.IsWatched());
    // TODO: Cutlist

    ri.SetRecordingStatus(RecStatus::Recorded);
    ri.SendUpdateEvent();

    return ri.GetRecordingID();
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool V2Dvr::RemoveRecorded(int RecordedId,
                         int chanid, const QDateTime &recstarttsRaw,
                         bool forceDelete, bool allowRerecord)
{
    return DeleteRecording(RecordedId, chanid, recstarttsRaw, forceDelete,
                           allowRerecord);
}


bool V2Dvr::DeleteRecording(int RecordedId,
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
            .arg(QString::number(pi.GetChanID()),
                 pi.GetRecordingStartTime(MythDate::ISODate),
                 forceDelete ? "FORCE" : "NO_FORCE",
                 allowRerecord ? "FORGET" : "NO_FORGET");
        MythEvent me(cmd);

        gCoreContext->dispatch(me);
        return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool V2Dvr::UnDeleteRecording(int RecordedId,
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

bool V2Dvr::StopRecording(int RecordedId)
{
    if (RecordedId <= 0)
        throw QString("RecordedId param is invalid.");

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
    throw QString("RecordedId %1 not found").arg(RecordedId);

    return false;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool V2Dvr::ReactivateRecording(int RecordedId,
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
        ri.ReactivateRecording();
        return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool V2Dvr::RescheduleRecordings(void)
{
    ScheduledRecording::RescheduleMatch(0, 0, 0, QDateTime(),
                                        "RescheduleRecordings");
    return true;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool V2Dvr::AllowReRecord ( int RecordedId )
{
    if (RecordedId <= 0)
        throw QString("RecordedId param is invalid.");

    RecordingInfo ri = RecordingInfo(RecordedId);

    if (!ri.GetChanID())
        throw QString("RecordedId %1 not found").arg(RecordedId);

    ri.ForgetHistory();

    return true;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool V2Dvr::UpdateRecordedWatchedStatus ( int RecordedId,
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

long V2Dvr::GetSavedBookmark( int RecordedId,
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
    uint64_t offset = 0;
    bool isend=true;
    uint64_t position = ri.QueryBookmark();
    // if no bookmark return 0
    if (position == 0)
        return 0;
    if (offsettype.toLower() == "position"){
        // if bookmark cannot be converted to a keyframe we will
        // just return the actual frame saved as the bookmark
        if (ri.QueryKeyFramePosition(&offset, position, isend))
            return offset;
    }
    if (offsettype.toLower() == "duration"){
        if (ri.QueryKeyFrameDuration(&offset, position, isend))
            return offset;
        // If bookmark cannot be converted to a duration return -1
        return -1;
    }
    return position;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool V2Dvr::SetSavedBookmark( int RecordedId,
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
    uint64_t position = 0;
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

V2CutList* V2Dvr::GetRecordedCutList ( int RecordedId,
                                        int chanid,
                                        const QDateTime &recstarttsRaw,
                                        const QString &offsettype )
{
    int marktype = 0;
    if ((RecordedId <= 0) &&
        (chanid <= 0 || !recstarttsRaw.isValid()))
        throw QString("Recorded ID or Channel ID and StartTime appears invalid.");

    RecordingInfo ri;
    if (RecordedId > 0)
        ri = RecordingInfo(RecordedId);
    else
        ri = RecordingInfo(chanid, recstarttsRaw.toUTC());

    auto* pCutList = new V2CutList();
    if (offsettype.toLower() == "position")
        marktype = 1;
    else if (offsettype.toLower() == "duration")
        marktype = 2;
    else
        marktype = 0;

    V2FillCutList(pCutList, &ri, marktype);

    return pCutList;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

V2CutList* V2Dvr::GetRecordedCommBreak ( int RecordedId,
                                          int chanid,
                                          const QDateTime &recstarttsRaw,
                                          const QString &offsettype )
{
    int marktype = 0;
    if ((RecordedId <= 0) &&
        (chanid <= 0 || !recstarttsRaw.isValid()))
        throw QString("Recorded ID or Channel ID and StartTime appears invalid.");

    RecordingInfo ri;
    if (RecordedId > 0)
        ri = RecordingInfo(RecordedId);
    else
        ri = RecordingInfo(chanid, recstarttsRaw.toUTC());

    auto* pCutList = new V2CutList();
    if (offsettype.toLower() == "position")
        marktype = 1;
    else if (offsettype.toLower() == "duration")
        marktype = 2;
    else
        marktype = 0;

    V2FillCommBreak(pCutList, &ri, marktype);

    return pCutList;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

V2CutList* V2Dvr::GetRecordedSeek ( int RecordedId,
                                     const QString &offsettype )
{
    MarkTypes marktype = MARK_UNSET;
    if (RecordedId <= 0)
        throw QString("Recorded ID appears invalid.");

    RecordingInfo ri;
    ri = RecordingInfo(RecordedId);

    auto* pCutList = new V2CutList();
    if (offsettype.toLower() == "bytes")
        marktype = MARK_GOP_BYFRAME;
    else if (offsettype.toLower() == "duration")
        marktype = MARK_DURATION_MS;
    else
    {
        delete pCutList;
        throw QString("Type must be 'BYTES' or 'DURATION'.");
    }

    V2FillSeek(pCutList, &ri, marktype);

    return pCutList;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

V2MarkupList* V2Dvr::GetRecordedMarkup ( int RecordedId )
{
    RecordingInfo ri;
    ri = RecordingInfo(RecordedId);

    if (!ri.HasPathname())
        throw QString("Invalid RecordedId %1").arg(RecordedId);

    QVector<ProgramInfo::MarkupEntry> mapMark;
    QVector<ProgramInfo::MarkupEntry> mapSeek;

    ri.QueryMarkup(mapMark, mapSeek);

    auto* pMarkupList = new V2MarkupList();
    for (auto entry : qAsConst(mapMark))
    {
        V2Markup *pMarkup = pMarkupList->AddNewMarkup();
        QString typestr = toString(static_cast<MarkTypes>(entry.type));
        pMarkup->setType(typestr);
        pMarkup->setFrame(entry.frame);
        if (entry.isDataNull)
            pMarkup->setData("NULL");
        else
            pMarkup->setData(QString::number(entry.data));
    }
    for (auto entry : qAsConst(mapSeek))
    {
        V2Markup *pSeek = pMarkupList->AddNewSeek();
        QString typestr = toString(static_cast<MarkTypes>(entry.type));
        pSeek->setType(typestr);
        pSeek->setFrame(entry.frame);
        if (entry.isDataNull)
            pSeek->setData("NULL");
        else
            pSeek->setData(QString::number(entry.data));
    }


    return pMarkupList;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool V2Dvr::SetRecordedMarkup (int RecordedId, const QJsonObject &jsonObj)
{
    RecordingInfo ri;
    ri = RecordingInfo(RecordedId);

    if (!ri.HasPathname())
        throw QString("Invalid RecordedId %1").arg(RecordedId);

    QVector<ProgramInfo::MarkupEntry> mapMark;
    QVector<ProgramInfo::MarkupEntry> mapSeek;

    QJsonObject markuplist = jsonObj["MarkupList"].toObject();

    QJsonArray  marks = markuplist["Mark"].toArray();
    for (const auto & m : marks)
    {
        QJsonObject markup = m.toObject();
        ProgramInfo::MarkupEntry entry;

        QString typestr = markup.value("Type").toString("");
        entry.type  = markTypeFromString(typestr);
        entry.frame = markup.value("Frame").toString("-1").toLongLong();
        QString data  = markup.value("Data").toString("NULL");
        entry.isDataNull = (data == "NULL");
        if (!entry.isDataNull)
            entry.data = data.toLongLong();

        mapMark.append(entry);
    }

    QJsonArray  seeks = markuplist["Seek"].toArray();
    for (const auto & m : seeks)
    {
        QJsonObject markup = m.toObject();
        ProgramInfo::MarkupEntry entry;

        QString typestr = markup.value("Type").toString("");
        entry.type  = markTypeFromString(typestr);
        entry.frame = markup.value("Frame").toString("-1").toLongLong();
        QString data  = markup.value("Data").toString("NULL");
        entry.isDataNull = (data == "NULL");
        if (!entry.isDataNull)
            entry.data = data.toLongLong();

        mapSeek.append(entry);
    }

    ri.SaveMarkup(mapMark, mapSeek);

    return true;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

V2EncoderList* V2Dvr::GetEncoderList()
{
    auto* pList = new V2EncoderList();

    QReadLocker tvlocker(&TVRec::s_inputsLock);
    QList<InputInfo> inputInfoList = CardUtil::GetAllInputInfo();
    for (auto * elink : qAsConst(tvList))
    {
        if (elink != nullptr)
        {
            V2Encoder *pEncoder = pList->AddNewEncoder();

            pEncoder->setId            ( elink->GetInputID()      );
            pEncoder->setState         ( elink->GetState()        );
            pEncoder->setLocal         ( elink->IsLocal()         );
            pEncoder->setConnected     ( elink->IsConnected()     );
            pEncoder->setSleepStatus   ( elink->GetSleepStatus()  );

            if (pEncoder->GetLocal())
                pEncoder->setHostName( gCoreContext->GetHostName() );
            else
                pEncoder->setHostName( elink->GetHostName() );

            for (const auto & inputInfo : qAsConst(inputInfoList))
            {
                if (inputInfo.m_inputId == static_cast<uint>(elink->GetInputID()))
                {
                    V2Input *input = pEncoder->AddNewInput();
                    V2FillInputInfo(input, inputInfo);
                }
            }

            switch ( pEncoder->GetState() )
            {
                case kState_WatchingLiveTV:
                case kState_RecordingOnly:
                case kState_WatchingRecording:
                {
                    ProgramInfo  *pInfo = elink->GetRecording();

                    if (pInfo)
                    {
                        V2Program *pProgram = pEncoder->Recording();

                        V2FillProgramInfo( pProgram, pInfo, true, true );

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

V2InputList* V2Dvr::GetInputList()
{
    auto *pList = new V2InputList();

    QList<InputInfo> inputInfoList = CardUtil::GetAllInputInfo();
    for (const auto & inputInfo : qAsConst(inputInfoList))
    {
        V2Input *input = pList->AddNewInput();
        V2FillInputInfo(input, inputInfo);
    }

    return pList;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QStringList V2Dvr::GetRecGroupList()
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

QStringList V2Dvr::GetProgramCategories( bool OnlyRecorded )
{
    MSqlQuery query(MSqlQuery::InitCon());

    if (OnlyRecorded)
        query.prepare("SELECT DISTINCT category FROM recorded ORDER BY category");
    else
        query.prepare("SELECT DISTINCT category FROM program ORDER BY category");

    QStringList result;
    if (!query.exec())
    {
        MythDB::DBError("GetProgramCategories", query);
        return result;
    }

    while (query.next())
        result << query.value(0).toString();

    return result;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QStringList V2Dvr::GetRecStorageGroupList()
{
    return StorageGroup::getRecordingsGroups();
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QStringList V2Dvr::GetPlayGroupList()
{
    return PlayGroup::GetNames();
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

V2RecRuleFilterList* V2Dvr::GetRecRuleFilterList()
{
    auto* filterList = new V2RecRuleFilterList();

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT filterid, description, newruledefault "
                    "FROM recordfilter ORDER BY filterid");

    if (query.exec())
    {
        while (query.next())
        {
            V2RecRuleFilter* ruleFilter = filterList->AddNewRecRuleFilter();
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

QStringList V2Dvr::GetTitleList(const QString& RecGroup)
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

V2TitleInfoList* V2Dvr::GetTitleInfoList()
{
    MSqlQuery query(MSqlQuery::InitCon());

    QString querystr = QString(
        "SELECT title, inetref, count(title) as count "
        "    FROM recorded AS r "
        "    JOIN recgroups AS g ON r.recgroupid = g.recgroupid "
        "    WHERE g.recgroup NOT IN ('Deleted', 'LiveTV') "
        "    AND r.deletepending = 0 "
        "    GROUP BY title, inetref "
        "    ORDER BY title");

    query.prepare(querystr);

    auto *pTitleInfos = new V2TitleInfoList();
    if (!query.exec())
    {
        MythDB::DBError("GetTitleList recorded", query);
        return pTitleInfos;
    }

    while (query.next())
    {
        V2TitleInfo *pTitleInfo = pTitleInfos->AddNewTitleInfo();

        pTitleInfo->setTitle(query.value(0).toString());
        pTitleInfo->setInetref(query.value(1).toString());
        pTitleInfo->setCount(query.value(2).toInt());
    }

    return pTitleInfos;
}


V2ProgramList* V2Dvr::GetConflictList( int  nStartIndex,
                                        int  nCount,
                                        int  nRecordId       )
{
    RecordingList  recordingList; // Auto-delete deque
    RecList  tmpList; // Standard deque, objects must be deleted

    if (nRecordId <= 0)
        nRecordId = -1;

    // NOTE: Fetching this information directly from the schedule is
    //       significantly faster than using ProgramInfo::LoadFromScheduler()
    auto *scheduler = dynamic_cast<Scheduler*>(gCoreContext->GetScheduler());
    if (scheduler)
        scheduler->GetAllPending(tmpList, nRecordId);

    // Sort the upcoming into only those which are conflicts
    // NOLINTNEXTLINE(modernize-loop-convert)
    for (auto it = tmpList.begin(); it < tmpList.end(); ++it)
    {
        if (((*it)->GetRecordingStatus() == RecStatus::Conflict) &&
            ((*it)->GetRecordingStartTime() >= MythDate::current()))
        {
            recordingList.push_back(new RecordingInfo(**it));
        }
        delete *it;
        *it = nullptr;
    }

    // ----------------------------------------------------------------------
    // Build Response
    // ----------------------------------------------------------------------

    auto *pPrograms = new V2ProgramList();

    nStartIndex   = (nStartIndex > 0) ? std::min( nStartIndex, (int)recordingList.size() ) : 0;
    nCount        = (nCount > 0) ? std::min( nCount, (int)recordingList.size() ) : recordingList.size();
    int nEndIndex = std::min((nStartIndex + nCount), (int)recordingList.size() );

    for( int n = nStartIndex; n < nEndIndex; n++)
    {
        ProgramInfo *pInfo = recordingList[ n ];

        V2Program *pProgram = pPrograms->AddNewProgram();

        V2FillProgramInfo( pProgram, pInfo, true );
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

V2ProgramList* V2Dvr::GetUpcomingList( int  nStartIndex,
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
    auto *scheduler = dynamic_cast<Scheduler*>(gCoreContext->GetScheduler());
    if (scheduler)
        scheduler->GetAllPending(tmpList, nRecordId);

    // Sort the upcoming into only those which will record
    // NOLINTNEXTLINE(modernize-loop-convert)
    for (auto it = tmpList.begin(); it < tmpList.end(); ++it)
    {
        if ((nRecStatus != 0) &&
            ((*it)->GetRecordingStatus() != nRecStatus))
        {
            delete *it;
            *it = nullptr;
            continue;
        }

        if (!bShowAll && ((((*it)->GetRecordingStatus() >= RecStatus::Pending) &&
                           ((*it)->GetRecordingStatus() <= RecStatus::WillRecord)) ||
                          ((*it)->GetRecordingStatus() == RecStatus::Recorded) ||
                          ((*it)->GetRecordingStatus() == RecStatus::Conflict)) &&
            ((*it)->GetRecordingEndTime() > MythDate::current()))
        {   // NOLINT(bugprone-branch-clone)
            recordingList.push_back(new RecordingInfo(**it));
        }
        else if (bShowAll &&
                 ((*it)->GetRecordingEndTime() > MythDate::current()))
        {
            recordingList.push_back(new RecordingInfo(**it));
        }

        delete *it;
        *it = nullptr;
    }

    // ----------------------------------------------------------------------
    // Build Response
    // ----------------------------------------------------------------------

    auto *pPrograms = new V2ProgramList();

    nStartIndex   = (nStartIndex > 0) ? std::min( nStartIndex, (int)recordingList.size() ) : 0;
    nCount        = (nCount > 0) ? std::min( nCount, (int)recordingList.size() ) : recordingList.size();
    int nEndIndex = std::min((nStartIndex + nCount), (int)recordingList.size() );

    for( int n = nStartIndex; n < nEndIndex; n++)
    {
        ProgramInfo *pInfo = recordingList[ n ];

        V2Program *pProgram = pPrograms->AddNewProgram();

        V2FillProgramInfo( pProgram, pInfo, true );
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
