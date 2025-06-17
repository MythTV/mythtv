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

// Qt
#include <QJsonArray>
#include <QJsonDocument>

// MythTV
#include "libmythbase/http/mythhttpmetaservice.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythscheduler.h"
#include "libmythbase/mythversion.h"
#include "libmythbase/programinfo.h"
#include "libmythbase/storagegroup.h"
#include "libmythtv/cardutil.h"
#include "libmythtv/channelutil.h"
#include "libmythtv/jobqueue.h"
#include "libmythtv/playgroup.h"
#include "libmythtv/programdata.h"
#include "libmythtv/tv_rec.h"

// MythBackend
#include "autoexpire.h"
#include "backendcontext.h"
#include "encoderlink.h"
#include "scheduler.h"
#include "v2dvr.h"
#include "v2serviceUtil.h"
#include "v2titleInfoList.h"

// This will be initialised in a thread safe manner on first use
Q_GLOBAL_STATIC_WITH_ARGS(MythHTTPMetaService, s_service,
    (DVR_HANDLE, V2Dvr::staticMetaObject, &V2Dvr::RegisterCustomTypes))

void V2Dvr::RegisterCustomTypes()
{
    qRegisterMetaType<V2ProgramList*>("V2ProgramList");
    qRegisterMetaType<V2Program*>("V2Program");
    qRegisterMetaType<V2CutList*>("V2CutList");
    qRegisterMetaType<V2Cutting*>("V2Cutting");
    qRegisterMetaType<V2MarkupList*>("V2MarkupList");
    qRegisterMetaType<V2Markup*>("V2Markup");
    qRegisterMetaType<V2EncoderList*>("V2EncoderList");
    qRegisterMetaType<V2Encoder*>("V2Encoder");
    qRegisterMetaType<V2InputList*>("V2InputList");
    qRegisterMetaType<V2Input*>("V2Input");
    qRegisterMetaType<V2RecRuleFilterList*>("V2RecRuleFilterList");
    qRegisterMetaType<V2RecRuleFilter*>("V2RecRuleFilter");
    qRegisterMetaType<V2TitleInfoList*>("V2TitleInfoList");
    qRegisterMetaType<V2TitleInfo*>("V2TitleInfo");
    qRegisterMetaType<V2RecRule*>("V2RecRule");
    qRegisterMetaType<V2RecRuleList*>("V2RecRuleList");
    qRegisterMetaType<V2ChannelInfo*>("V2ChannelInfo");
    qRegisterMetaType<V2RecordingInfo*>("V2RecordingInfo");
    qRegisterMetaType<V2ArtworkInfoList*>("V2ArtworkInfoList");
    qRegisterMetaType<V2ArtworkInfo*>("V2ArtworkInfo");
    qRegisterMetaType<V2CastMemberList*>("V2CastMemberList");
    qRegisterMetaType<V2CastMember*>("V2CastMember");
    qRegisterMetaType<V2PlayGroup*>("V2PlayGroup");
    qRegisterMetaType<V2PowerPriority*>("V2PowerPriority");
    qRegisterMetaType<V2PowerPriorityList*>("V2PowerPriorityList");
}

V2Dvr::V2Dvr()
  : MythHTTPService(s_service)
{
}

V2ProgramList* V2Dvr::GetExpiringList( int nStartIndex,
                                        int nCount      )
{
    pginfolist_t  infoList;

    if (gExpirer)
        gExpirer->GetAllExpiring( infoList );

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
                                       const QString &sSort,
                                       bool           bIgnoreLiveTV,
                                       bool           bIgnoreDeleted,
                                       bool           bIncChannel,
                                       bool           bDetails,
                                       bool           bIncCast,
                                       bool           bIncArtWork,
                                       bool           bIncRecording
                                     )
{
    if (!HAS_PARAMv2("IncChannel"))
        bIncChannel = true;

    if (!HAS_PARAMv2("Details"))
        bDetails = true;

    if (!HAS_PARAMv2("IncCast"))
        bIncCast = true;

    if (!HAS_PARAMv2("IncArtwork"))
        bIncArtWork = true;

    if (!HAS_PARAMv2("IncRecording"))
        bIncRecording = true;

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
        V2FillProgramInfo( pProgram, pInfo, bIncChannel, bDetails, bIncCast, bIncArtWork, bIncRecording );
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
// Note that you should not specify both Title and TitleRegEx, that is counter-
// productive and would only work if the TitleRegEx matched the Title.
/////////////////////////////////////////////////////////////////////////////

V2ProgramList* V2Dvr::GetOldRecordedList( bool             bDescending,
                                           int              nStartIndex,
                                           int              nCount,
                                           const QDateTime &sStartTime,
                                           const QDateTime &sEndTime,
                                           const QString   &sTitle,
                                           const QString   &TitleRegEx,
                                           const QString   &SubtitleRegEx,
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

    if (!TitleRegEx.isEmpty())
    {
        clause << "title REGEXP :TitleRegEx";
        bindings[":TitleRegEx"] = TitleRegEx;
    }

    if (!SubtitleRegEx.isEmpty())
    {
        clause << "subtitle REGEXP :SubtitleRegEx";
        bindings[":SubtitleRegEx"] = SubtitleRegEx;
    }

    if (!sSeriesId.isEmpty())
    {
        clause << "seriesid = :SeriesId";
        bindings[":SeriesId"] = sSeriesId;
    }

    if (!clause.isEmpty())
    {
        sSQL += QString(" AND (%1) ").arg(clause.join(" AND "));
    }

    QStringList sortByFields;
    sortByFields << "starttime" <<  "title" <<  "subtitle" << "season" << "episode" << "category"
                                  <<  "channum" << "rectype" << "recstatus" << "duration" ;
    QStringList fields = sSort.split(",");
    // Add starttime as last or only sort.
        fields << "starttime";
    sSQL += " ORDER BY ";
    bool first = true;
    for (const QString& oneField : std::as_const(fields))
    {
        QString field = oneField.simplified().toLower();
        if (field.isEmpty())
            continue;
        if (sortByFields.contains(field))
        {
            if (first)
                first = false;
            else
                sSQL += ", ";
            if (field == "channum")
            {
                // this is to sort numerically rather than alphabetically
                field = "channum*1000-ifnull(regexp_substr(channum,'-.*'),0)";
            }
            else if (field == "duration")
            {
                field = "timestampdiff(second,starttime,endtime)";
            }
            sSQL += field;
            if (bDescending)
                sSQL += " DESC ";
            else
                sSQL += " ASC ";
        }
        else
        {
            LOG(VB_GENERAL, LOG_WARNING, QString("V2Dvr::GetOldRecordedList() got an unknown sort field '%1' - ignoring").arg(oneField));
        }
    }

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

bool       V2Dvr::RemoveOldRecorded   ( int              ChanId,
                                        const QDateTime &StartTime,
                                        bool            Reschedule )
{
    if (!HAS_PARAMv2("ChanId") || !HAS_PARAMv2("StartTime"))
        throw QString("Channel ID and StartTime appears invalid.");
    QString sql("DELETE FROM oldrecorded "
        " WHERE chanid = :ChanId AND starttime = :StartTime" );
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(sql);
    query.bindValue(":ChanId", ChanId);
    query.bindValue(":StartTime", StartTime);
    if (!query.exec())
    {
        MythDB::DBError("RemoveOldRecorded", query);
        return false;
    }
    if (query.numRowsAffected() <= 0)
        return false;
    if (!HAS_PARAMv2("Reschedule"))
        Reschedule = true;
    if (Reschedule)
        RescheduleRecordings();
    return true;
}

bool       V2Dvr::UpdateOldRecorded   ( int              ChanId,
                                        const QDateTime &StartTime,
                                        bool            Duplicate,
                                        bool            Reschedule )
{
    if (!HAS_PARAMv2("ChanId") || !HAS_PARAMv2("StartTime"))
        throw QString("Channel ID and StartTime appears invalid.");
    if (!HAS_PARAMv2("Duplicate"))
        throw QString("Error: Nothing to change.");
    QString sql("UPDATE oldrecorded "
        " SET Duplicate = :Duplicate "
        " WHERE chanid = :ChanId AND starttime = :StartTime" );
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(sql);
    query.bindValue(":Duplicate", Duplicate);
    query.bindValue(":ChanId", ChanId);
    query.bindValue(":StartTime", StartTime);
    if (!query.exec())
    {
        MythDB::DBError("UpdateOldRecorded", query);
        return false;
    }
    if (!HAS_PARAMv2("Reschedule"))
        Reschedule = true;
    if (Reschedule)
        RescheduleRecordings();
    return true;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

V2Program* V2Dvr::GetRecorded(int RecordedId,
                              int chanid, const QDateTime &StartTime)
{
    if ((RecordedId <= 0) &&
        (chanid <= 0 || !StartTime.isValid()))
        throw QString("Recorded ID or Channel ID and StartTime appears invalid.");

    // TODO Should use RecordingInfo
    ProgramInfo pi;
    if (RecordedId > 0)
        pi = ProgramInfo(RecordedId);
    else
        pi = ProgramInfo(chanid, StartTime.toUTC());

    auto *pProgram = new V2Program();
    V2FillProgramInfo( pProgram, &pi, true );

    return pProgram;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool V2Dvr::AddRecordedCredits(int RecordedId, const QString & Cast)
{
    QJsonDocument jsonDoc = QJsonDocument::fromJson(Cast.toUtf8());
    // Verify the corresponding recording exists
    RecordingInfo ri(RecordedId);
    if (!ri.HasPathname())
        throw QString("AddRecordedCredits: recordedid %1 does "
                      "not exist.").arg(RecordedId);

    DBCredits* credits = V2jsonCastToCredits(jsonDoc.object());
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

int V2Dvr::AddRecordedProgram(const QString &Program)
{
    QJsonDocument doc     = QJsonDocument::fromJson(Program.toUtf8());
    QJsonObject program   = doc.object();
    QJsonObject channel   = program["Channel"].toObject();
    QJsonObject recording = program["Recording"].toObject();
    QJsonObject cast      = program["Cast"].toObject();

    auto     *pi = new ProgInfo();
    int       chanid = channel.value("ChanId").toVariant().toString().toUInt();

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
    pi->m_stars         = program.value("Stars").toVariant().toString().toFloat();
    pi->m_categoryType  = string_to_myth_category_type(program.value("CatType").toString(""));
    pi->m_seriesId      = program.value("SeriesId").toString("");
    pi->m_programId     = program.value("ProgramId").toString("");
    pi->m_inetref       = program.value("Inetref").toString("");
    pi->m_previouslyshown = false;
    pi->m_listingsource = 0;
//    pi->m_ratings =
//    pi->m_genres =
    pi->m_season        = program.value("Season").toVariant()
                                 .toString().toUInt();
    pi->m_episode       = program.value("Episode").toVariant()
                                 .toString().toUInt();
    pi->m_totalepisodes = program.value("TotalEpisodes").toVariant()
                          .toString().toUInt();

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
                     static_cast<RecStatus::Type>(recording.value("Status").toInt()),
                     false, // reactivate
                     recording.value("RecordedId").toString("0").toInt(),
                     0, // parentid
                     static_cast<RecordingType>(recording.value("RecType").toInt()),
                     static_cast<RecordingDupInType>(recording.value("DupInType").toInt()),
                     static_cast<RecordingDupMethodType>(recording.value("DupMethod").toInt()),
                     channel.value("SourceId").toVariant().toString().toUInt(),
                     channel.value("InputId").toVariant().toString().toUInt(),
                     0, // findid
                     channel.value("CommFree").toBool(),
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
                         int chanid, const QDateTime &StartTime,
                         bool forceDelete, bool allowRerecord)
{
    return DeleteRecording(RecordedId, chanid, StartTime, forceDelete,
                           allowRerecord);
}


bool V2Dvr::DeleteRecording(int RecordedId,
                          int chanid, const QDateTime &StartTime,
                          bool forceDelete, bool allowRerecord)
{
    if ((RecordedId <= 0) &&
        (chanid <= 0 || !StartTime.isValid()))
        throw QString("Recorded ID or Channel ID and StartTime appears invalid.");

    // TODO Should use RecordingInfo
    ProgramInfo pi;
    if (RecordedId > 0)
        pi = ProgramInfo(RecordedId);
    else
        pi = ProgramInfo(chanid, StartTime.toUTC());

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
                            int chanid, const QDateTime &StartTime)
{
    if ((RecordedId <= 0) &&
        (chanid <= 0 || !StartTime.isValid()))
        throw QString("Recorded ID or Channel ID and StartTime appears invalid.");

    RecordingInfo ri;
    if (RecordedId > 0)
        ri = RecordingInfo(RecordedId);
    else
        ri = RecordingInfo(chanid, StartTime.toUTC());

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
// Supply one of the following
// RecordedId
// or
// ChanId and StartTime
// or
// RecordId
/////////////////////////////////////////////////////////////////////////////

bool V2Dvr::ReactivateRecording(int RecordedId,
                              int ChanId, const QDateTime &StartTime,
                              int RecordId )
{
    RecordingInfo ri;
    if (RecordedId > 0)
        ri = RecordingInfo(RecordedId);
    else if (ChanId > 0 && StartTime.isValid())
        ri = RecordingInfo(ChanId, StartTime.toUTC());
    else if (RecordId > 0)
    {
        // Find latest recording for that record id
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT recordedid FROM recorded "
                      " WHERE recordid = :RECORDID "
                      " ORDER BY starttime DESC LIMIT 1");
        query.bindValue(":RECORDID", RecordId);
        if (!query.exec())
        {
            MythDB::DBError("ReactivateRecording", query);
            return false;
        }
        int recId {0};
        if (query.next())
            recId = query.value(0).toInt();
        else
            return false;
        ri = RecordingInfo(recId);
    }
    else
    {
        throw QString("Recorded ID or Channel ID and StartTime or RecordId are invalid.");
    }
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

bool V2Dvr::AllowReRecord ( int RecordedId, int ChanId, const QDateTime &StartTime)
{
    if (RecordedId > 0)
    {
        if (ChanId > 0 || StartTime.isValid())
            throw QString("ERROR RecordedId param cannot be used with ChanId or StartTime.");
    }
    else if (ChanId > 0)
    {
        if (!StartTime.isValid())
            throw QString("ERROR ChanId param requires a valid StartTime.");
    }
    else
    {
        throw QString("ERROR RecordedId or (ChanId and StartTime) required.");
    }

    if (RecordedId > 0)
    {
        RecordingInfo ri = RecordingInfo(RecordedId);
        if (!ri.GetChanID())
            throw QString("ERROR RecordedId %1 not found").arg(RecordedId);
        ri.ForgetHistory();
    }
    else
    {
        ProgramInfo *progInfo = LoadProgramFromProgram(ChanId, StartTime);
        if (progInfo == nullptr)
            throw QString("ERROR Guide data for Chanid %1 at StartTime %2 not found")
                .arg(ChanId).arg(StartTime.toString());
        RecordingInfo recInfo(*progInfo);
        recInfo.ForgetHistory();
        delete progInfo;
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////
// Prefer Dvr/UpdateRecordedMetadata. Some day, this should go away.
/////////////////////////////////////////////////////////////////////////////

bool V2Dvr::UpdateRecordedWatchedStatus ( int RecordedId,
                                        int   chanid,
                                        const QDateTime &StartTime,
                                        bool  watched)
{
    // LOG(VB_GENERAL, LOG_WARNING, "Deprecated, use Dvr/UpdateRecordedMetadata.");

    if ((RecordedId <= 0) &&
        (chanid <= 0 || !StartTime.isValid()))
        throw QString("Recorded ID or Channel ID and StartTime appears invalid.");

    // TODO Should use RecordingInfo
    ProgramInfo pi;
    if (RecordedId > 0)
        pi = ProgramInfo(RecordedId);
    else
        pi = ProgramInfo(chanid, StartTime.toUTC());

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
                            const QDateTime &StartTime,
                            const QString &offsettype )
{
    if ((RecordedId <= 0) &&
        (chanid <= 0 || !StartTime.isValid()))
        throw QString("Recorded ID or Channel ID and StartTime appears invalid.");

    RecordingInfo ri;
    if (RecordedId > 0)
        ri = RecordingInfo(RecordedId);
    else
        ri = RecordingInfo(chanid, StartTime.toUTC());
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
// Get last play position
// Providing -1 for the RecordedId will return response of -1.
// This is a way to check if this api, and the other LastPlayPos APIs,
// are supported
/////////////////////////////////////////////////////////////////////////////

long V2Dvr::GetLastPlayPos( int RecordedId,
                            int chanid,
                            const QDateTime &StartTime,
                            const QString &offsettype )
{
    if (RecordedId == -1)
        return -1;

    if ((RecordedId <= 0) &&
        (chanid <= 0 || !StartTime.isValid()))
        throw QString("Recorded ID or Channel ID and StartTime appears invalid.");

    RecordingInfo ri;
    if (RecordedId > 0)
        ri = RecordingInfo(RecordedId);
    else
        ri = RecordingInfo(chanid, StartTime.toUTC());
    uint64_t offset = 0;
    bool isend=true;
    uint64_t position = ri.QueryLastPlayPos();
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
// Prefer Dvr/UpdateRecordedMetadata. Some day, this should go away.
/////////////////////////////////////////////////////////////////////////////

bool V2Dvr::SetSavedBookmark( int RecordedId,
                            int chanid,
                            const QDateTime &StartTime,
                            const QString &offsettype,
                            long Offset )
{
    // LOG(VB_GENERAL, LOG_WARNING, "Deprecated, use Dvr/UpdateRecordedMetadata.");

    if ((RecordedId <= 0) &&
        (chanid <= 0 || !StartTime.isValid()))
        throw QString("Recorded ID or Channel ID and StartTime appears invalid.");

    if (Offset < 0)
        throw QString("Offset must be >= 0.");

    RecordingInfo ri;
    if (RecordedId > 0)
        ri = RecordingInfo(RecordedId);
    else
        ri = RecordingInfo(chanid, StartTime.toUTC());
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
    {
        position = Offset;
    }
    ri.SaveBookmark(position);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
// Set last Play Position. Check if this is supported by first calling
// Get Last Play Position with -1.
// Prefer Dvr/UpdateRecordedMetadata. Some day, this should go away.
/////////////////////////////////////////////////////////////////////////////

bool V2Dvr::SetLastPlayPos( int RecordedId,
                            int chanid,
                            const QDateTime &StartTime,
                            const QString &offsettype,
                            long Offset )
{
    // LOG(VB_GENERAL, LOG_WARNING, "Deprecated, use Dvr/UpdateRecordedMetadata.");

    if ((RecordedId <= 0) &&
        (chanid <= 0 || !StartTime.isValid()))
        throw QString("Recorded ID or Channel ID and StartTime appears invalid.");

    if (Offset < 0)
        throw QString("Offset must be >= 0.");

    RecordingInfo ri;
    if (RecordedId > 0)
        ri = RecordingInfo(RecordedId);
    else
        ri = RecordingInfo(chanid, StartTime.toUTC());
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
    {
        position = Offset;
    }
    ri.SaveLastPlayPos(position);
    return true;
}

V2CutList* V2Dvr::GetRecordedCutList ( int RecordedId,
                                        int chanid,
                                        const QDateTime &StartTime,
                                        const QString &offsettype,
                                        bool IncludeFps )
{
    int marktype = 0;
    if ((RecordedId <= 0) &&
        (chanid <= 0 || !StartTime.isValid()))
        throw QString("Recorded ID or Channel ID and StartTime appears invalid.");

    RecordingInfo ri;
    if (RecordedId > 0)
        ri = RecordingInfo(RecordedId);
    else
        ri = RecordingInfo(chanid, StartTime.toUTC());

    auto* pCutList = new V2CutList();
    if (offsettype.toLower() == "position")
        marktype = 1;
    else if (offsettype.toLower() == "duration")
        marktype = 2;
    else
        marktype = 0;

    V2FillCutList(pCutList, &ri, marktype, IncludeFps);

    return pCutList;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

V2CutList* V2Dvr::GetRecordedCommBreak ( int RecordedId,
                                          int chanid,
                                          const QDateTime &StartTime,
                                          const QString &offsettype,
                                          bool IncludeFps )
{
    int marktype = 0;
    if ((RecordedId <= 0) &&
        (chanid <= 0 || !StartTime.isValid()))
        throw QString("Recorded ID or Channel ID and StartTime appears invalid.");

    RecordingInfo ri;
    if (RecordedId > 0)
        ri = RecordingInfo(RecordedId);
    else
        ri = RecordingInfo(chanid, StartTime.toUTC());

    auto* pCutList = new V2CutList();
    if (offsettype.toLower() == "position")
        marktype = 1;
    else if (offsettype.toLower() == "duration")
        marktype = 2;
    else
        marktype = 0;

    V2FillCommBreak(pCutList, &ri, marktype, IncludeFps);

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
    for (const auto& entry : std::as_const(mapMark))
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
    for (const auto& entry : std::as_const(mapSeek))
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

bool V2Dvr::SetRecordedMarkup(int RecordedId, const QString &MarkupList)
{
    RecordingInfo ri;
    ri = RecordingInfo(RecordedId);

    if (!ri.HasPathname())
        throw QString("Invalid RecordedId %1").arg(RecordedId);

    QVector<ProgramInfo::MarkupEntry> mapMark;
    QVector<ProgramInfo::MarkupEntry> mapSeek;

    QJsonDocument doc        = QJsonDocument::fromJson(MarkupList.toUtf8());
    QJsonObject   markuplist = doc.object();

    QJsonArray  marks = markuplist["Mark"].toArray();
    for (const auto & m : std::as_const(marks))
    {
        QJsonObject markup = m.toObject();
        ProgramInfo::MarkupEntry entry;

        QString typestr = markup.value("Type").toString("");
        entry.type  = markTypeFromString(typestr);
        entry.frame = markup.value("Frame").toVariant()
                            .toString().toLongLong();
        QString data  = markup.value("Data").toString("NULL");
        entry.isDataNull = (data == "NULL");
        if (!entry.isDataNull)
            entry.data = data.toLongLong();

        mapMark.append(entry);
    }

    QJsonArray  seeks = markuplist["Seek"].toArray();
    for (const auto & m : std::as_const(seeks))
    {
        QJsonObject markup = m.toObject();
        ProgramInfo::MarkupEntry entry;

        QString typestr = markup.value("Type").toString("");
        entry.type  = markTypeFromString(typestr);
        entry.frame = markup.value("Frame").toVariant().toString().toLongLong();
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
    FillEncoderList(pList->GetEncoders(), pList);
    return pList;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

V2InputList* V2Dvr::GetInputList()
{
    auto *pList = new V2InputList();

    QList<InputInfo> inputInfoList = CardUtil::GetAllInputInfo(false);
    for (const auto & inputInfo : std::as_const(inputInfoList))
    {
        V2Input *input = pList->AddNewInput();
        V2FillInputInfo(input, inputInfo);
    }

    return pList;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QStringList V2Dvr::GetRecGroupList( const QString &UsedBy)
{
    MSqlQuery query(MSqlQuery::InitCon());
    if (UsedBy.compare("recorded",Qt::CaseInsensitive) == 0)
        query.prepare("SELECT DISTINCT recgroup FROM recorded "
            "ORDER BY recgroup");
    else if (UsedBy.compare("schedule",Qt::CaseInsensitive) == 0)
        query.prepare("SELECT DISTINCT recgroup FROM record "
            "ORDER BY recgroup");
    else
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

V2PlayGroup* V2Dvr::GetPlayGroup    ( const QString & Name )
{
    auto* playGroup = new V2PlayGroup();

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT name, titlematch, skipahead, skipback, timestretch, jump "
                "FROM playgroup WHERE name = :NAME ");
    query.bindValue(":NAME", Name);

    if (query.exec())
    {
        if (query.next())
        {
            playGroup->setName(query.value(0).toString());
            playGroup->setTitleMatch(query.value(1).toString());
            playGroup->setSkipAhead(query.value(2).toInt());
            playGroup->setSkipBack(query.value(3).toInt());
            playGroup->setTimeStretch(query.value(4).toInt());
            playGroup->setJump(query.value(5).toInt());
        }
        else
        {
            throw QString("Play Group Not Found.");
        }
   }
    return playGroup;
}

bool V2Dvr::RemovePlayGroup    ( const QString & Name )
{

    if (Name.compare("Default", Qt::CaseInsensitive) == 0)
        throw QString("ERROR: Cannot delete Default entry");
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM playgroup "
        "WHERE name = :NAME");

    query.bindValue(":NAME", Name);

    return query.exec();
}

bool V2Dvr::AddPlayGroup    ( const QString & Name,
                              const QString & TitleMatch,
                              int             SkipAhead,
                              int             SkipBack,
                              int             TimeStretch,
                              int             Jump )
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("INSERT INTO playgroup "
        "(name, titlematch, skipahead, skipback, timestretch, jump) "
        " VALUES(:NAME, :TITLEMATCH, :SKIPAHEAD, :SKIPBACK, :TIMESTRETCH, :JUMP)");
    query.bindValue(":NAME", Name);
    query.bindValue(":TITLEMATCH", TitleMatch);
    query.bindValue(":SKIPAHEAD", SkipAhead);
    query.bindValue(":SKIPBACK", SkipBack);
    query.bindValue(":TIMESTRETCH", TimeStretch);
    query.bindValue(":JUMP", Jump);

    return query.exec();
}

bool V2Dvr::UpdatePlayGroup ( const QString & Name,
                              const QString & TitleMatch,
                              int             SkipAhead,
                              int             SkipBack,
                              int             TimeStretch,
                              int             Jump )
{
    if (Name.isEmpty())
        throw QString("ERROR: Name is not specified");

    bool ok = false;

    QString sql = "UPDATE playgroup SET ";
    if (HAS_PARAMv2("TitleMatch"))
    {
        sql.append(" titlematch = :TITLEMATCH ");
        ok = true;
    }
    if (HAS_PARAMv2("SkipAhead"))
    {
        if (ok)
            sql.append(",");
        sql.append(" skipahead = :SKIPAHEAD ");
        ok = true;
    }
    if (HAS_PARAMv2("SkipBack"))
    {
        if (ok)
            sql.append(",");
        sql.append(" skipback = :SKIPBACK ");
        ok = true;
    }
    if (HAS_PARAMv2("TimeStretch"))
    {
        if (ok)
            sql.append(",");
        sql.append(" timestretch = :TIMESTRETCH ");
        ok = true;
    }
    if (HAS_PARAMv2("Jump"))
    {
        if (ok)
            sql.append(",");
        sql.append(" jump = :JUMP ");
        ok = true;
    }
    if (ok)
    {
        sql.append(" WHERE name = :NAME ");
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare(sql);
        query.bindValue(":NAME", Name);
        if (HAS_PARAMv2("TitleMatch"))
            query.bindValue(":TITLEMATCH", TitleMatch);
        if (HAS_PARAMv2("SkipAhead"))
            query.bindValue(":SKIPAHEAD", SkipAhead);
        if (HAS_PARAMv2("SkipBack"))
            query.bindValue(":SKIPBACK", SkipBack);
        if (HAS_PARAMv2("TimeStretch"))
            query.bindValue(":TIMESTRETCH", TimeStretch);
        if (HAS_PARAMv2("Jump"))
            query.bindValue(":JUMP", Jump);
        if (query.exec())
            return true;
    }
    return false;
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
                                        int  nRecordId,
                                        const QString  &Sort )
{
    auto *pPrograms = new V2ProgramList();
    int size = FillUpcomingList(pPrograms->GetPrograms(), pPrograms,
                                         nStartIndex,
                                         nCount,
                                         true, // bShowAll,
                                         nRecordId,
                                         RecStatus::Conflict,
                                         Sort);

    pPrograms->setStartIndex    ( nStartIndex     );
    pPrograms->setCount         ( nCount          );
    pPrograms->setTotalAvailable( size );
    pPrograms->setAsOf          ( MythDate::current() );
    pPrograms->setVersion       ( MYTH_BINARY_VERSION );
    pPrograms->setProtoVer      ( MYTH_PROTO_VERSION  );

    return pPrograms;
}

V2ProgramList* V2Dvr::GetUpcomingList( int  nStartIndex,
                                        int  nCount,
                                        bool bShowAll,
                                        int  nRecordId,
                                        const QString &RecStatus,
                                        const QString &Sort,
                                        const QString &RecGroup )
{
    int nRecStatus = 0;
    if (!RecStatus.isEmpty())
    {
        // Handle enum name
        QMetaEnum meta = QMetaEnum::fromType<RecStatus::Type>();
        bool ok {false};
        nRecStatus = meta.keyToValue(RecStatus.toLocal8Bit(), &ok);
        // if enum name not valid try for int nRecStatus
        if (!ok)
            nRecStatus = RecStatus.toInt(&ok);
        // if still not valid use 99999 to trigger an "unknown" response
        if (!ok)
            nRecStatus = 99999;
    }
    auto *pPrograms = new V2ProgramList();
    int size = FillUpcomingList(pPrograms->GetPrograms(), pPrograms,
                                         nStartIndex,
                                         nCount,
                                         bShowAll,
                                         nRecordId,
                                         nRecStatus,
                                         Sort,
                                         RecGroup );

    pPrograms->setStartIndex    ( nStartIndex     );
    pPrograms->setCount         ( nCount          );
    pPrograms->setTotalAvailable( size );
    pPrograms->setAsOf          ( MythDate::current() );
    pPrograms->setVersion       ( MYTH_BINARY_VERSION );
    pPrograms->setProtoVer      ( MYTH_PROTO_VERSION  );

    return pPrograms;
}

uint V2Dvr::AddRecordSchedule   (
                               const QString&   sTitle,
                               const QString&   sSubtitle,
                               const QString&   sDescription,
                               const QString&   sCategory,
                               const QDateTime& StartTime,
                               const QDateTime& EndTime,
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
                               const QDateTime& LastRecorded,
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
                               int       nTranscoder,
                               const QString&   AutoExtend)
{
    QDateTime recstartts = StartTime.toUTC();
    QDateTime recendts = EndTime.toUTC();
    QDateTime lastrects = LastRecorded.toUTC();
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
        rule.m_recGroupID = V2CreateRecordingGroup(sRecGroup);
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
    rule.m_autoExtend = autoExtendTypeFromString(AutoExtend);

    rule.m_lastRecorded = lastrects;

    QString msg;
    if (!rule.IsValid(msg))
        throw QString(msg);

    bool success = rule.Save();
    if (!success)
        throw QString("DATABASE ERROR: Check for duplicate recording rule");

    uint recid = rule.m_recordID;

    return recid;
}

bool V2Dvr::UpdateRecordSchedule ( uint      nRecordId,
                                 const QString&   sTitle,
                                 const QString&   sSubtitle,
                                 const QString&   sDescription,
                                 const QString&   sCategory,
                                 const QDateTime& StartTime,
                                 const QDateTime& EndTime,
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
                                 int       nTranscoder,
                                 const QString&   AutoExtend)
{
    if (nRecordId == 0 )
        throw QString("Record ID is invalid.");

    RecordingRule pRule;
    pRule.m_recordID = nRecordId;
    pRule.Load();

    if (!pRule.IsLoaded())
        throw QString("Record ID does not exist.");

    QDateTime recstartts = StartTime.toUTC();
    QDateTime recendts = EndTime.toUTC();

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
        pRule.m_recGroupID = V2CreateRecordingGroup(sRecGroup);
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

    if (!AutoExtend.isEmpty())
        pRule.m_autoExtend = autoExtendTypeFromString(AutoExtend);

    QString msg;
    if (!pRule.IsValid(msg))
        throw QString(msg);

    bool bResult = pRule.Save();

    return bResult;
}

bool V2Dvr::RemoveRecordSchedule ( uint nRecordId )
{
    bool bResult = false;

    if (nRecordId == 0 )
        throw QString("Record ID does not exist.");

    RecordingRule pRule;
    pRule.m_recordID = nRecordId;

    bResult = pRule.Delete();

    return bResult;
}

bool V2Dvr::AddDontRecordSchedule(int nChanId, const QDateTime &dStartTime,
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
    {
        recInfo.ApplyRecordStateChange(kDontRecord);
    }

    return bResult;
}

V2RecRuleList* V2Dvr::GetRecordScheduleList( int nStartIndex,
                                              int nCount,
                                              const QString  &Sort,
                                              bool Descending )
{
    Scheduler::SchedSortColumn sortingColumn {Scheduler::kSortTitle};
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

    auto *pRecRules = new V2RecRuleList();

    nStartIndex   = (nStartIndex > 0) ? std::min( nStartIndex, (int)recList.size() ) : 0;
    nCount        = (nCount > 0) ? std::min( nCount, (int)recList.size() ) : recList.size();
    int nEndIndex = std::min((nStartIndex + nCount), (int)recList.size() );

    for( int n = nStartIndex; n < nEndIndex; n++)
    {
        RecordingInfo *info = recList[n];

        if (info != nullptr)
        {
            V2RecRule *pRecRule = pRecRules->AddNewRecRule();

            V2FillRecRuleInfo( pRecRule, info->GetRecordingRule() );
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

V2RecRule* V2Dvr::GetRecordSchedule( uint      nRecordId,
                                      const QString&   sTemplate,
                                      int       nRecordedId,
                                      int       nChanId,
                                      const QDateTime& StartTime,
                                      bool      bMakeOverride )
{
    RecordingRule rule;
    QDateTime dStartTime = StartTime.toUTC();

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

    auto *pRecRule = new V2RecRule();
    V2FillRecRuleInfo( pRecRule, &rule );

    return pRecRule;
}

bool V2Dvr::EnableRecordSchedule ( uint nRecordId )
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

bool V2Dvr::DisableRecordSchedule( uint nRecordId )
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

int V2Dvr::RecordedIdForKey(int chanid, const QDateTime &StartTime)
{
    int recordedid = 0;

    if (!RecordingInfo::QueryRecordedIdForKey(recordedid, chanid,
                                              StartTime))
        return -1;

    return recordedid;
}

int V2Dvr::RecordedIdForPathname(const QString & pathname)
{
    uint recordedid = 0;

    if (!ProgramInfo::QueryRecordedIdFromPathname(pathname, recordedid))
        return -1;

    return recordedid;
}

QString V2Dvr::RecStatusToString(const QString & RecStatus)
{
    // Handle enum name
    QMetaEnum meta = QMetaEnum::fromType<RecStatus::Type>();
    bool ok {false};
    int value = meta.keyToValue(RecStatus.toLocal8Bit(), &ok);
    // if enum name not valid try for int value
    if (!ok)
        value = RecStatus.toInt(&ok);
    // if still not valid use 0 to trigger an "unknown" response
    if (!ok)
        value = 0;
    auto type = static_cast<RecStatus::Type>(value);
    return RecStatus::toString(type);
}

QString V2Dvr::RecStatusToDescription(const QString &  RecStatus, int recType,
                                    const QDateTime &StartTime)
{
    // Handle enum name
    QMetaEnum meta = QMetaEnum::fromType<RecStatus::Type>();
    bool ok {false};
    int value = meta.keyToValue(RecStatus.toLocal8Bit(), &ok);
    // if enum name not valid try for int value
    if (!ok)
        value = RecStatus.toInt(&ok);
    // if still not valid use 0 to trigger an "unknown" response
    if (!ok)
        value = 0;
    auto recstatusType = static_cast<RecStatus::Type>(value);
    auto recordingType = static_cast<RecordingType>(recType);
    return RecStatus::toDescription(recstatusType, recordingType, StartTime);
}

QString V2Dvr::RecTypeToString(const QString& recType)
{
    bool ok = false;
    auto enumType = static_cast<RecordingType>(recType.toInt(&ok, 10));
    if (ok)
        return toString(enumType);
    // RecordingType type = static_cast<RecordingType>(recType);
    return toString(recTypeFromString(recType));
}

QString V2Dvr::RecTypeToDescription(const QString& recType)
{
    bool ok = false;
    auto enumType = static_cast<RecordingType>(recType.toInt(&ok, 10));
    if (ok)
        return toDescription(enumType);
    // RecordingType type = static_cast<RecordingType>(recType);
    return toDescription(recTypeFromString(recType));
}

QString V2Dvr::DupInToString(const QString& DupIn)
{
    // RecordingDupInType type= static_cast<RecordingDupInType>(DupIn);
    // return toString(type);
    return toString(dupInFromString(DupIn));
}

QString V2Dvr::DupInToDescription(const QString& DupIn)
{
    // RecordingDupInType type= static_cast<RecordingDupInType>(DupIn);
    //return toDescription(type);
    return toDescription(dupInFromString(DupIn));
}

QString V2Dvr::DupMethodToString(const QString& DupMethod)
{
    // RecordingDupMethodType method = static_cast<RecordingDupMethodType>(DupMethod);
    return toString(dupMethodFromString(DupMethod));
}

QString V2Dvr::DupMethodToDescription(const QString& DupMethod)
{
    // RecordingDupMethodType method = static_cast<RecordingDupMethodType>(DupMethod);
    return toDescription(dupMethodFromString(DupMethod));
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

int V2Dvr::ManageJobQueue( const QString   &sAction,
                         const QString   &sJobName,
                         int              nJobId,
                         int              nRecordedId,
                               QDateTime  JobStartTime,
                               QString    sRemoteHost,
                               QString    sJobArgs )
{
    int nReturn = -1;

    if (!HAS_PARAMv2("JobName") ||
        !HAS_PARAMv2("RecordedId") )
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
        if (!HAS_PARAMv2("JobId") || nJobId < 0)
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
        LOG(VB_GENERAL, LOG_NOTICE, QString("Note: %1 hasn't been allowed on host %2.")
                                            .arg(sJobName, sRemoteHost));
        // return nReturn;
    }

    if (!JobStartTime.isValid())
        JobStartTime = QDateTime::currentDateTime();

    if (!JobQueue::InJobRunWindow(JobStartTime))
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
                                 JobStartTime.toUTC());

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

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool V2Dvr::UpdateRecordedMetadata ( uint             RecordedId,
                                     bool             AutoExpire,
                                     long             BookmarkOffset,
                                     const QString   &BookmarkOffsetType,
                                     bool             Damaged,
                                     const QString   &Description,
                                     uint             Episode,
                                     const QString   &Inetref,
                                     long             LastPlayOffset,
                                     const QString   &LastPlayOffsetType,
                                     QDate            OriginalAirDate,
                                     bool             Preserve,
                                     uint             Season,
                                     uint             Stars,
                                     const QString   &SubTitle,
                                     const QString   &Title,
                                     bool             Watched,
                                     const QString   &RecGroup )

{
    if (m_request->m_queries.size() < 2 || !HAS_PARAMv2("RecordedId"))
    {
        LOG(VB_GENERAL, LOG_ERR, "No RecordedId, or no parameters to change.");
        return false;
    }

    auto pi = ProgramInfo(RecordedId);
    auto ri = RecordingInfo(RecordedId);

    if (!ri.GetChanID())
        return false;

    if (HAS_PARAMv2("AutoExpire"))
        pi.SaveAutoExpire(AutoExpire ? kNormalAutoExpire :
                                        kDisableAutoExpire, false);

    if (HAS_PARAMv2("BookmarkOffset"))
    {
        uint64_t position =0;

        if (BookmarkOffsetType.toLower() == "position")
        {
            if (!ri.QueryPositionKeyFrame(&position, BookmarkOffset, true))
                return false;
        }
        else if (BookmarkOffsetType.toLower() == "duration")
        {
            if (!ri.QueryDurationKeyFrame(&position, BookmarkOffset, true))
                return false;
        }
        else
        {
            position = BookmarkOffset;
        }

        ri.SaveBookmark(position);
    }

    if (HAS_PARAMv2("Damaged"))
        pi.SaveVideoProperties(VID_DAMAGED, Damaged ?  VID_DAMAGED : 0);

    if (HAS_PARAMv2("Description") ||
        HAS_PARAMv2("SubTitle")    ||
        HAS_PARAMv2("Title"))
    {

        QString tmp_description;
        QString tmp_subtitle;
        QString tmp_title;

        if (HAS_PARAMv2("Description"))
            tmp_description = Description;
        else
            tmp_description = ri.GetDescription();

        if (HAS_PARAMv2("SubTitle"))
            tmp_subtitle = SubTitle;
        else
            tmp_subtitle = ri.GetSubtitle();

        if (HAS_PARAMv2("Title"))
            tmp_title = Title;
        else
            tmp_title = ri.GetTitle();

        ri.ApplyRecordRecTitleChange(tmp_title, tmp_subtitle, tmp_description);
    }

    if (HAS_PARAMv2("Episode") ||
        HAS_PARAMv2("Season"))
    {
        int tmp_episode = 0;
        int tmp_season = 0;

        if (HAS_PARAMv2("Episode"))
            tmp_episode = Episode;
        else
            tmp_episode = ri.GetEpisode();

        if (HAS_PARAMv2("Season"))
            tmp_season = Season;
        else
            tmp_season = ri.GetSeason();

        pi.SaveSeasonEpisode(tmp_season, tmp_episode);
    }

    if (HAS_PARAMv2("Inetref"))
        pi.SaveInetRef(Inetref);

    if (HAS_PARAMv2("LastPlayOffset"))
    {

        if (LastPlayOffset < 0)
            throw QString("LastPlayOffset must be >= 0.");

        uint64_t position = LastPlayOffset;
        bool isend=true;

        if (HAS_PARAMv2("LastPlayOffsetType"))
        {
            if (LastPlayOffsetType.toLower() == "position")
            {
                if (!ri.QueryPositionKeyFrame(&position, LastPlayOffset, isend))
                        return false;
            }
            else if (LastPlayOffsetType.toLower() == "duration")
            {
                if (!ri.QueryDurationKeyFrame(&position, LastPlayOffset, isend))
                        return false;
            }
        }

        ri.SaveLastPlayPos(position);

        return true;

    }

    if (HAS_PARAMv2("OriginalAirDate"))
    {
        // OriginalAirDate can be set to null by submitting value 'null' in json
        if (!OriginalAirDate.isValid() && !OriginalAirDate.isNull())
        {
            LOG(VB_GENERAL, LOG_ERR, "Need valid OriginalAirDate yyyy-mm-dd.");
            return false;
        }
        ri.ApplyOriginalAirDateChange(OriginalAirDate);
    }

    if (HAS_PARAMv2("Preserve"))
        pi.SavePreserve(Preserve);

    if (HAS_PARAMv2("Stars"))
    {
        if (Stars > 10)
        {
            LOG(VB_GENERAL, LOG_ERR, "Recording stars can be 0 to 10.");
            return false;
        }
        ri.ApplyStarsChange(Stars * 0.1F);
    }

    if (HAS_PARAMv2("Watched"))
        pi.SaveWatched(Watched);

    if (HAS_PARAMv2("RecGroup"))
        ri.ApplyRecordRecGroupChange(RecGroup);

    return true;
}

// Get a single record by filling PriorityName, otherwise all records
V2PowerPriorityList* V2Dvr::GetPowerPriorityList (const QString &PriorityName )
{
    auto *pList = new V2PowerPriorityList();

    MSqlQuery query(MSqlQuery::InitCon());

    QString sql("SELECT priorityname, recpriority, selectclause "
                "FROM powerpriority ");

    if (!PriorityName.isEmpty())
        sql.append(" WHERE priorityname = :NAME ");

    query.prepare(sql);

    if (!PriorityName.isEmpty())
        query.bindValue(":NAME", PriorityName);

    if (query.exec())
    {
        while (query.next())
        {
            V2PowerPriority * pRec = pList->AddNewPowerPriority();
            pRec->setPriorityName(query.value(0).toString());
            pRec->setRecPriority(query.value(1).toInt());
            pRec->setSelectClause(query.value(2).toString());
        }
    }
    else
    {
        throw (QString("Error accessing powerpriority table"));
    }

    return pList;
}

bool V2Dvr::RemovePowerPriority ( const QString & PriorityName )
{
    if (PriorityName.isEmpty())
        return false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM powerpriority WHERE priorityname = :PRIORITYNAME");
    query.bindValue(":PRIORITYNAME", PriorityName);

    return query.exec();
}

bool V2Dvr::AddPowerPriority    ( const QString & PriorityName,
                                  int             RecPriority,
                                  const QString & SelectClause )
{
    if (PriorityName.isEmpty())
        throw QString("ERROR: PriorityName is not specified");
    if (SelectClause.isEmpty())
        throw QString("ERROR: SelectClause is required");
    QString msg = CheckPowerQuery(SelectClause);
    if (! msg.isEmpty() )
        throw(msg);
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("INSERT INTO powerpriority "
                " (priorityname, recpriority, selectclause) "
                " VALUES(:PRIORITYNAME, :RECPRIORITY, :SELECTCLAUSE) ");
    query.bindValue(":PRIORITYNAME", PriorityName);
    query.bindValue(":RECPRIORITY", RecPriority);
    query.bindValue(":SELECTCLAUSE", SelectClause);
    if (!query.exec())
        throw(query.lastError().databaseText());
    return true;
}

bool V2Dvr::UpdatePowerPriority ( const QString & PriorityName,
                                  int             RecPriority,
                                  const QString & SelectClause )
{
    if (PriorityName.isEmpty())
        throw QString("ERROR: PriorityName is not specified");
    if (!HAS_PARAMv2("RecPriority") && !HAS_PARAMv2("SelectClause"))
        throw QString("ERROR: RecPriority or SelectClause is required");

    if (HAS_PARAMv2("SelectClause"))
    {
        QString msg = CheckPowerQuery(SelectClause);
        if (! msg.isEmpty() )
            throw(msg);
    }
    MSqlQuery query(MSqlQuery::InitCon());
    bool comma = false;
    QString sql("UPDATE powerpriority SET ");
    if ( HAS_PARAMv2("RecPriority") )
    {
        sql.append(" recpriority = :RECPRIORITY ");
        comma = true;
    }
    if ( HAS_PARAMv2("SelectClause") )
    {
        if (comma)
            sql.append(" , ");
        sql.append(" selectclause = :SELECTCLAUSE ");
    }
    sql.append(" where priorityname = :PRIORITYNAME ");
    query.prepare(sql);
    query.bindValue(":PRIORITYNAME", PriorityName);
    if ( HAS_PARAMv2("RecPriority") )
        query.bindValue(":RECPRIORITY", RecPriority);
    if ( HAS_PARAMv2("SelectClause") )
        query.bindValue(":SELECTCLAUSE", SelectClause);
    if (!query.exec())
        throw(query.lastError().databaseText());
    return query.numRowsAffected() > 0;
}

QString V2Dvr::CheckPowerQuery(const QString & SelectClause)
{
    QString msg;
    QString sql = QString("SELECT (%1) FROM (recordmatch, record, "
                           "program, channel, capturecard, "
                           "oldrecorded) WHERE NULL").arg(SelectClause);
    while (true)
    {
        int i = sql.indexOf("RECTABLE");
        if (i == -1) break;
        sql = sql.replace(i, strlen("RECTABLE"), "record");
    }

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(sql);

    if (!query.exec())
    {
        msg = tr("An error was found when checking") + ":\n\n";
        msg += query.executedQuery();
        msg += "\n\n" + tr("The database error was") + ":\n";
        msg += query.lastError().databaseText();
    }
    return msg;
}
