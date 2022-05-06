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

// Qt
#include <QMap>

// MythTV
#include "libmyth/programinfo.h"
#include "libmyth/remoteutil.h"
#include "libmythbase/compat.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythevent.h"
#include "libmythbase/mythscheduler.h"
#include "libmythbase/mythversion.h"
#include "libmythbase/programtypes.h"
#include "libmythbase/recordingtypes.h"
#include "libmythbase/storagegroup.h"
#include "libmythtv/cardutil.h"
#include "libmythtv/channelutil.h"
#include "libmythtv/inputinfo.h"
#include "libmythtv/jobqueue.h"
#include "libmythtv/playgroup.h"
#include "libmythtv/programdata.h"
#include "libmythtv/recordinginfo.h"
#include "libmythtv/recordingprofile.h"
#include "libmythtv/tv_rec.h"

// MythBackend
#include "autoexpire.h"
#include "backendcontext.h"
#include "dvr.h"
#include "encoderlink.h"
#include "scheduler.h"
#include "serviceUtil.h"

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::ProgramList* Dvr::GetRecordedList( bool           bDescending,
                                        int            nStartIndex,
                                        int            nCount,
                                        const QString &sTitleRegEx,
                                        const QString &sRecGroup,
                                        const QString &sStorageGroup,
                                        const QString &sCategory,
                                        const QString &sSort,
                                        bool           bIgnoreLiveTV,
                                        bool           bIgnoreDeleted
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

    if (bIgnoreLiveTV && (sRecGroup == "LiveTV"))
    {
        bIgnoreLiveTV = false;
        LOG(VB_GENERAL, LOG_ERR, QString("Setting Ignore%1=false because RecGroup=%1")
                                         .arg(sRecGroup));
    }

    if (bIgnoreDeleted && (sRecGroup == "Deleted"))
    {
        bIgnoreDeleted = false;
        LOG(VB_GENERAL, LOG_ERR, QString("Setting Ignore%1=false because RecGroup=%1")
                                         .arg(sRecGroup));
    }

    LoadFromRecorded( progList, false, inUseMap, isJobRunning, recMap, desc,
                      sSort, bIgnoreLiveTV, bIgnoreDeleted );

    QMap< QString, ProgramInfo* >::iterator mit = recMap.begin();

    for (; mit != recMap.end(); mit = recMap.erase(mit))
        delete *mit;

    // ----------------------------------------------------------------------
    // Build Response
    // ----------------------------------------------------------------------

    auto *pPrograms = new DTC::ProgramList();
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

DTC::ProgramList* Dvr::GetOldRecordedList( bool             bDescending,
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

    auto *pPrograms = new DTC::ProgramList();

    nCount        = (int)progList.size();
    int nEndIndex = (int)progList.size();

    for( int n = 0; n < nEndIndex; n++)
    {
        ProgramInfo *pInfo = progList[ n ];

        DTC::Program *pProgram = pPrograms->AddNewProgram();

        FillProgramInfo( pProgram, pInfo, true );
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

    auto *pProgram = new DTC::Program();
    FillProgramInfo( pProgram, &pi, true );

    return pProgram;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool Dvr::AddRecordedCredits(int RecordedId, const QJsonObject &jsonObj)
{
    // Verify the corresponding recording exists
    RecordingInfo ri(RecordedId);
    if (!ri.HasPathname())
        throw QString("AddRecordedCredits: recordedid %1 does "
                      "not exist.").arg(RecordedId);

    DBCredits* credits = jsonCastToCredits(jsonObj);
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

    delete credits;
    return true;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

int Dvr::AddRecordedProgram(const QJsonObject &jsonObj)
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
    pi->m_credits = jsonCastToCredits(cast);
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

bool Dvr::ReactivateRecording(int RecordedId,
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

bool Dvr::RescheduleRecordings(void)
{
    ScheduledRecording::RescheduleMatch(0, 0, 0, QDateTime(),
                                        "RescheduleRecordings");
    return true;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool Dvr::AllowReRecord ( int RecordedId )
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

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::CutList* Dvr::GetRecordedCutList ( int RecordedId,
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

    auto* pCutList = new DTC::CutList();
    if (offsettype.toLower() == "position")
        marktype = 1;
    else if (offsettype.toLower() == "duration")
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
    int marktype = 0;
    if ((RecordedId <= 0) &&
        (chanid <= 0 || !recstarttsRaw.isValid()))
        throw QString("Recorded ID or Channel ID and StartTime appears invalid.");

    RecordingInfo ri;
    if (RecordedId > 0)
        ri = RecordingInfo(RecordedId);
    else
        ri = RecordingInfo(chanid, recstarttsRaw.toUTC());

    auto* pCutList = new DTC::CutList();
    if (offsettype.toLower() == "position")
        marktype = 1;
    else if (offsettype.toLower() == "duration")
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
    MarkTypes marktype = MARK_UNSET;
    if (RecordedId <= 0)
        throw QString("Recorded ID appears invalid.");

    RecordingInfo ri;
    ri = RecordingInfo(RecordedId);

    auto* pCutList = new DTC::CutList();
    if (offsettype.toLower() == "bytes")
        marktype = MARK_GOP_BYFRAME;
    else if (offsettype.toLower() == "duration")
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

DTC::MarkupList* Dvr::GetRecordedMarkup ( int RecordedId )
{
    RecordingInfo ri;
    ri = RecordingInfo(RecordedId);

    if (!ri.HasPathname())
        throw QString("Invalid RecordedId %1").arg(RecordedId);

    QVector<ProgramInfo::MarkupEntry> mapMark;
    QVector<ProgramInfo::MarkupEntry> mapSeek;

    ri.QueryMarkup(mapMark, mapSeek);

    auto* pMarkupList = new DTC::MarkupList();
    for (auto entry : qAsConst(mapMark))
    {
        DTC::Markup *pMarkup = pMarkupList->AddNewMarkup();
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
        DTC::Markup *pSeek = pMarkupList->AddNewSeek();
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

bool Dvr::SetRecordedMarkup (int RecordedId, const QJsonObject &jsonObj)
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

DTC::ProgramList* Dvr::GetExpiringList( int nStartIndex,
                                        int nCount      )
{
    pginfolist_t  infoList;

    if (gExpirer)
        gExpirer->GetAllExpiring( infoList );

    // ----------------------------------------------------------------------
    // Build Response
    // ----------------------------------------------------------------------

    auto *pPrograms = new DTC::ProgramList();

    nStartIndex   = (nStartIndex > 0) ? std::min( nStartIndex, (int)infoList.size() ) : 0;
    nCount        = (nCount > 0) ? std::min( nCount, (int)infoList.size() ) : infoList.size();
    int nEndIndex = std::min((nStartIndex + nCount), (int)infoList.size() );

    for( int n = nStartIndex; n < nEndIndex; n++)
    {
        ProgramInfo *pInfo = infoList[ n ];

        if (pInfo != nullptr)
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
    auto* pList = new DTC::EncoderList();

    QReadLocker tvlocker(&TVRec::s_inputsLock);
    QList<InputInfo> inputInfoList = CardUtil::GetAllInputInfo();
    for (auto * elink : qAsConst(gTVList))
    {
        if (elink != nullptr)
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

            for (const auto & inputInfo : qAsConst(inputInfoList))
            {
                if (inputInfo.m_inputId == static_cast<uint>(elink->GetInputID()))
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
    auto *pList = new DTC::InputList();

    QList<InputInfo> inputInfoList = CardUtil::GetAllInputInfo();
    for (const auto & inputInfo : qAsConst(inputInfoList))
    {
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

QStringList Dvr::GetProgramCategories( bool OnlyRecorded )
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
    auto* filterList = new DTC::RecRuleFilterList();

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
        "    WHERE g.recgroup NOT IN ('Deleted', 'LiveTV') "
        "    AND r.deletepending = 0 "
        "    GROUP BY title, inetref "
        "    ORDER BY title");

    query.prepare(querystr);

    auto *pTitleInfos = new DTC::TitleInfoList();
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

    auto *pPrograms = new DTC::ProgramList();

    nStartIndex   = (nStartIndex > 0) ? std::min( nStartIndex, (int)recordingList.size() ) : 0;
    nCount        = (nCount > 0) ? std::min( nCount, (int)recordingList.size() ) : recordingList.size();
    int nEndIndex = std::min((nStartIndex + nCount), (int)recordingList.size() );

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

    auto *pPrograms = new DTC::ProgramList();

    nStartIndex   = (nStartIndex > 0) ? std::min( nStartIndex, (int)recordingList.size() ) : 0;
    nCount        = (nCount > 0) ? std::min( nCount, (int)recordingList.size() ) : recordingList.size();
    int nEndIndex = std::min((nStartIndex + nCount), (int)recordingList.size() );

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
                               const QString&   sTitle,
                               const QString&   sSubtitle,
                               const QString&   sDescription,
                               const QString&   sCategory,
                               const QDateTime& recstarttsRaw,
                               const QDateTime& recendtsRaw,
                               const QString&   sSeriesId,
                               const QString&   sProgramId,
                               int       nChanId,
                               const QString&   sStation,
                               int       nFindDay,
                               QTime     tFindTime,
                               int       nParentId,
                               bool      bInactive,
                               uint      nSeason,
                               uint      nEpisode,
                               const QString&   sInetref,
                               QString   sType,
                               QString   sSearchType,
                               int       nRecPriority,
                               uint      nPreferredInput,
                               int       nStartOffset,
                               int       nEndOffset,
                               const QDateTime& lastrectsRaw,
                               QString   sDupMethod,
                               QString   sDupIn,
                               bool      bNewEpisOnly,
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
    QDateTime lastrects = lastrectsRaw.toUTC();
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
    if (rule.m_searchType == kManualSearch)
        rule.m_dupMethod = kDupCheckNone;
    else
        rule.m_dupMethod = dupMethodFromString(sDupMethod);
    rule.m_dupIn = dupInFromStringAndBool(sDupIn, bNewEpisOnly);

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
    {
        rule.m_recGroupID = CreateRecordingGroup(sRecGroup);
        if (rule.m_recGroupID <= 0)
            rule.m_recGroupID = RecordingInfo::kDefaultRecGroup;
    }
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

    rule.m_lastRecorded = lastrects;

    QString msg;
    if (!rule.IsValid(msg))
        throw QString(msg);

    rule.Save();

    uint recid = rule.m_recordID;

    return recid;
}

bool Dvr::UpdateRecordSchedule ( uint      nRecordId,
                                 const QString&   sTitle,
                                 const QString&   sSubtitle,
                                 const QString&   sDescription,
                                 const QString&   sCategory,
                                 const QDateTime& dStartTimeRaw,
                                 const QDateTime& dEndTimeRaw,
                                 const QString&   sSeriesId,
                                 const QString&   sProgramId,
                                 int       nChanId,
                                 const QString&   sStation,
                                 int       nFindDay,
                                 QTime     tFindTime,
                                 bool      bInactive,
                                 uint      nSeason,
                                 uint      nEpisode,
                                 const QString&   sInetref,
                                 QString   sType,
                                 QString   sSearchType,
                                 int       nRecPriority,
                                 uint      nPreferredInput,
                                 int       nStartOffset,
                                 int       nEndOffset,
                                 QString   sDupMethod,
                                 QString   sDupIn,
                                 bool      bNewEpisOnly,
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
    if (nRecordId == 0 )
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
    if (pRule.m_searchType == kManualSearch)
        pRule.m_dupMethod = kDupCheckNone;
    else
        pRule.m_dupMethod = dupMethodFromString(sDupMethod);
    pRule.m_dupIn = dupInFromStringAndBool(sDupIn, bNewEpisOnly);

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
    {
        pRule.m_recGroupID = CreateRecordingGroup(sRecGroup);
        if (pRule.m_recGroupID <= 0)
            pRule.m_recGroupID = RecordingInfo::kDefaultRecGroup;
    }
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
        throw QString(msg);

    bool bResult = pRule.Save();

    return bResult;
}

bool Dvr::RemoveRecordSchedule ( uint nRecordId )
{
    bool bResult = false;

    if (nRecordId == 0 )
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
    Scheduler::SchedSortColumn sortingColumn = Scheduler::kSortTitle;
    if (Sort.toLower() == "lastrecorded")
        sortingColumn = Scheduler::kSortLastRecorded;
    else if (Sort.toLower() == "nextrecording")
        sortingColumn = Scheduler::kSortNextRecording;
    else if (Sort.toLower() == "title")
        sortingColumn = Scheduler::kSortTitle;    // NOLINT(bugprone-branch-clone)
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

    auto *pRecRules = new DTC::RecRuleList();

    nStartIndex   = (nStartIndex > 0) ? std::min( nStartIndex, (int)recList.size() ) : 0;
    nCount        = (nCount > 0) ? std::min( nCount, (int)recList.size() ) : recList.size();
    int nEndIndex = std::min((nStartIndex + nCount), (int)recList.size() );

    for( int n = nStartIndex; n < nEndIndex; n++)
    {
        RecordingInfo *info = recList[n];

        if (info != nullptr)
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
                                      const QString&   sTemplate,
                                      int       nRecordedId,
                                      int       nChanId,
                                      const QDateTime& dStartTimeRaw,
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
        RecordingInfo::LoadStatus status = RecordingInfo::kNoProgram;
        RecordingInfo info(nChanId, dStartTime, false, 0h, &status);
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

    auto *pRecRule = new DTC::RecRule();
    FillRecRuleInfo( pRecRule, &rule );

    return pRecRule;
}

bool Dvr::EnableRecordSchedule ( uint nRecordId )
{
    bool bResult = false;

    if (nRecordId == 0 )
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

    if (nRecordId == 0 )
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

int Dvr::RecordedIdForKey(int chanid, const QDateTime &recstarttsRaw)
{
    int recordedid = 0;

    if (!RecordingInfo::QueryRecordedIdForKey(recordedid, chanid,
                                              recstarttsRaw))
        return -1;

    return recordedid;
}

int Dvr::RecordedIdForPathname(const QString & pathname)
{
    uint recordedid = 0;

    if (!ProgramInfo::QueryRecordedIdFromPathname(pathname, recordedid))
        return -1;

    return recordedid;
}

QString Dvr::RecStatusToString(int RecStatus)
{
    auto type = static_cast<RecStatus::Type>(RecStatus);
    return RecStatus::toString(type);
}

QString Dvr::RecStatusToDescription(int RecStatus, int recType,
                                    const QDateTime &StartTime)
{
    //if (!StartTime.isValid())
    //    throw QString("StartTime appears invalid.");
    auto recstatusType = static_cast<RecStatus::Type>(RecStatus);
    auto recordingType = static_cast<RecordingType>(recType);
    return RecStatus::toDescription(recstatusType, recordingType, StartTime);
}

QString Dvr::RecTypeToString(const QString& recType)
{
    bool ok = false;
    auto enumType = static_cast<RecordingType>(recType.toInt(&ok, 10));
    if (ok)
        return toString(enumType);
    // RecordingType type = static_cast<RecordingType>(recType);
    return toString(recTypeFromString(recType));
}

QString Dvr::RecTypeToDescription(const QString& recType)
{
    bool ok = false;
    auto enumType = static_cast<RecordingType>(recType.toInt(&ok, 10));
    if (ok)
        return toDescription(enumType);
    // RecordingType type = static_cast<RecordingType>(recType);
    return toDescription(recTypeFromString(recType));
}

QString Dvr::DupInToString(const QString& DupIn)
{
    // RecordingDupInType type= static_cast<RecordingDupInType>(DupIn);
    // return toString(type);
    return toString(dupInFromString(DupIn));
}

QString Dvr::DupInToDescription(const QString& DupIn)
{
    // RecordingDupInType type= static_cast<RecordingDupInType>(DupIn);
    //return toDescription(type);
    return toDescription(dupInFromString(DupIn));
}

QString Dvr::DupMethodToString(const QString& DupMethod)
{
    // RecordingDupMethodType method = static_cast<RecordingDupMethodType>(DupMethod);
    return toString(dupMethodFromString(DupMethod));
}

QString Dvr::DupMethodToDescription(const QString& DupMethod)
{
    // RecordingDupMethodType method = static_cast<RecordingDupMethodType>(DupMethod);
    return toDescription(dupMethodFromString(DupMethod));
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

int Dvr::ManageJobQueue( const QString   &sAction,
                         const QString   &sJobName,
                         int              nJobId,
                         int              nRecordedId,
                               QDateTime  jobstarttsRaw,
                               QString    sRemoteHost,
                               QString    sJobArgs )
{
    int nReturn = -1;

    if (!m_parsedParams.contains("jobname") &&
        !m_parsedParams.contains("recordedid") )
    {
        LOG(VB_GENERAL, LOG_ERR, "JobName and RecordedId are required.");
        return nReturn;
    }

    if (sRemoteHost.isEmpty())
        sRemoteHost = gCoreContext->GetHostName();

    int jobType = JobQueue::GetJobTypeFromName(sJobName);

    if (jobType == JOB_NONE)
        return nReturn;

    RecordingInfo ri = RecordingInfo(nRecordedId);

    if (!ri.GetChanID())
        return nReturn;

    if ( sAction == "Remove")
    {
        if (!m_parsedParams.contains("jobid") || nJobId < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, "For Remove, a valid JobId is required.");
            return nReturn;
        }

        if (!JobQueue::SafeDeleteJob(nJobId, jobType, ri.GetChanID(),
                                     ri.GetRecordingStartTime()))
            return nReturn;

        return nJobId;
    }

    if ( sAction != "Add")
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Illegal Action name '%1'. Use: Add, "
                                         "or Remove").arg(sAction));
        return nReturn;
    }

    if (((jobType & JOB_USERJOB) != 0) &&
         gCoreContext->GetSetting(sJobName, "").isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("%1 hasn't been defined.")
            .arg(sJobName));
        return nReturn;
    }

    if (!gCoreContext->GetBoolSettingOnHost(QString("JobAllow%1").arg(sJobName),
                                            sRemoteHost, false))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("%1 hasn't been allowed on host %2.")
                                         .arg(sJobName, sRemoteHost));
        return nReturn;
    }

    if (!jobstarttsRaw.isValid())
        jobstarttsRaw = QDateTime::currentDateTime();

    if (!JobQueue::InJobRunWindow(jobstarttsRaw))
        return nReturn;

    if (sJobArgs.isNull())
        sJobArgs = "";

    bool bReturn = JobQueue::QueueJob(jobType,
                                 ri.GetChanID(),
                                 ri.GetRecordingStartTime(),
                                 sJobArgs,
                                 QString("Dvr/ManageJobQueue"), // comment col.
                                 sRemoteHost,
                                 JOB_NO_FLAGS,
                                 JOB_QUEUED,
                                 jobstarttsRaw.toUTC());

    if (!bReturn)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("%1 job wasn't queued because of a "
                                         "database error or because it was "
                                         "already running/stopping etc.")
                                         .arg(sJobName));

        return nReturn;
    }

    return JobQueue::GetJobID(jobType, ri.GetChanID(),
                              ri.GetRecordingStartTime());
}
