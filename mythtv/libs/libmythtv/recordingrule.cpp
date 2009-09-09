
#include "recordingrule.h"

// libmythdb
#include "mythdb.h"

// libmyth
#include "mythcontext.h"

// libmythtv
#include "scheduledrecording.h" // For signalChange()
#include "playgroup.h" // For GetInitialName()

RecordingRule::RecordingRule()
  : m_recordID(-1), m_parentRecID(0),
    m_starttime(QTime::currentTime()),
    m_startdate(QDate::currentDate()),
    m_endtime(QTime::currentTime()),
    m_enddate(QDate::currentDate()),
    m_findday(-1),
    m_findtime(QTime::fromString("00:00:00", Qt::ISODate)),
    m_findid(QDate(1970, 1, 1).daysTo(QDate::currentDate()) + 719528),
    m_type(kNotRecording),
    m_searchType(kNoSearch),
    m_recPriority(0),
    m_prefInput(0),
    m_startOffset(gContext->GetNumSetting("DefaultStartOffset", 0)),
    m_endOffset(gContext->GetNumSetting("DefaultStartOffset", 0)),
    m_dupMethod(static_cast<RecordingDupMethodType>(
                gContext->GetNumSetting("prefDupMethod", kDupCheckSubDesc))),
    m_dupIn(kDupsInAll),
    m_recProfile(QObject::tr("Default")),
    m_recGroup("Default"),
    m_storageGroup("Default"),
    m_playGroup("Default"),
    m_autoExpire(gContext->GetNumSetting("AutoExpireDefault", 0)),
    m_maxEpisodes(0),
    m_maxNewest(false),
    m_autoCommFlag(gContext->GetNumSetting("AutoCommercialFlag", 1)),
    m_autoTranscode(gContext->GetNumSetting("AutoTranscode", 0)),
    m_transcoder(gContext->GetNumSetting("DefaultTranscoder",
                RecordingProfile::TranscoderAutodetect)),
    m_autoUserJob1(gContext->GetNumSetting("AutoRunUserJob1", 0)),
    m_autoUserJob2(gContext->GetNumSetting("AutoRunUserJob2", 0)),
    m_autoUserJob3(gContext->GetNumSetting("AutoRunUserJob3", 0)),
    m_autoUserJob4(gContext->GetNumSetting("AutoRunUserJob4", 0)),
    m_nextRecording(QDateTime::fromString("0000-00-00T00:00:00", Qt::ISODate)),
    m_lastRecorded(QDateTime::fromString("0000-00-00T00:00:00", Qt::ISODate)),
    m_lastDeleted(QDateTime::fromString("0000-00-00T00:00:00", Qt::ISODate)),
    m_averageDelay(100)
{
}

bool RecordingRule::Load()
{
    if (m_recordID <= 0)
        return false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT type, search, "
    "recpriority, prefinput, startoffset, endoffset, dupmethod, dupin, "
    "inactive, profile, recgroup, storagegroup, playgroup, autoexpire, "
    "maxepisodes, maxnewest, autocommflag, autotranscode, transcoder, "
    "autouserjob1, autouserjob2, autouserjob3, autouserjob4, "
    "parentid, title, subtitle, description, category, "
    "starttime, startdate, endtime, enddate, seriesid, programid, "
    "chanid, station, findday, findtime, findid, "
    "next_record, last_record, last_delete, avg_delay "
    "FROM record WHERE recordid = :RECORDID ;");

    query.bindValue(":RECORDID", m_recordID);

    if (query.exec() && query.next())
    {
        // Schedule
        m_type = static_cast<RecordingType>(query.value(0).toInt());
        m_searchType = static_cast<RecSearchType>(query.value(1).toInt());
        m_recPriority = query.value(2).toInt();
        m_prefInput = query.value(3).toInt();
        m_startOffset = query.value(4).toInt();
        m_endOffset = query.value(5).toInt();
        m_dupMethod = static_cast<RecordingDupMethodType>
                                                (query.value(6).toInt());
        m_dupIn = static_cast<RecordingDupInType>(query.value(7).toInt());
        m_isInactive = query.value(8).toBool();
        // Storage
        m_recProfile = query.value(9).toString();
        m_recGroup = query.value(10).toString();
        m_storageGroup = query.value(11).toString();
        m_playGroup = query.value(12).toString();
        m_autoExpire = query.value(13).toBool();
        m_maxEpisodes = query.value(14).toInt();
        m_maxNewest = query.value(15).toBool();
        // Post Process
        m_autoCommFlag = query.value(16).toBool();
        m_autoTranscode = query.value(17).toBool();
        m_transcoder = query.value(18).toInt();
        m_autoUserJob1 = query.value(19).toBool();
        m_autoUserJob2 = query.value(20).toBool();
        m_autoUserJob3 = query.value(21).toBool();
        m_autoUserJob4 = query.value(22).toBool();
        // Original rule id for override rule
        m_parentRecID = query.value(23).toInt();
        // Recording metadata
        m_title = query.value(24).toString();
        m_subtitle = query.value(25).toString();
        m_description = query.value(26).toString();
        m_category = query.value(27).toString();
        m_starttime = query.value(28).toTime();
        m_startdate = query.value(29).toDate();
        m_endtime = query.value(30).toTime();
        m_enddate = query.value(31).toDate();
        m_seriesid = query.value(32).toString();
        m_programid = query.value(33).toString();
        // Associated data for rule types
        m_channelid = query.value(34).toInt();
        m_station = query.value(35).toString();
        m_findday = query.value(36).toInt();
        m_findtime = query.value(37).toTime();
        m_findid = query.value(38).toInt();
        // Statistic fields - Used to generate statistics about particular rules
        // and influence watch list weighting
        m_nextRecording = query.value(39).toDateTime();
        m_lastRecorded = query.value(40).toDateTime();
        m_lastDeleted = query.value(41).toDateTime();
        m_averageDelay = query.value(42).toInt();
    }
    else
    {
        MythDB::DBError("SELECT record", query);
        return false;
    }

    return true;
}

bool RecordingRule::LoadByProgram(const ProgramInfo* proginfo)
{
    if (proginfo->recordid)
    {
        m_recordID = proginfo->recordid;
        if (!Load())
            return false;
    }

    if (m_searchType == kNoSearch || m_searchType == kManualSearch)
    {
        if (proginfo->recordid)
        {
            m_title = proginfo->title;
            m_subtitle = proginfo->subtitle;
            m_description = proginfo->description;
            m_channelid = proginfo->chanid.toInt();
            m_station = proginfo->chansign;
            m_startdate = proginfo->startts.date();
            m_starttime = proginfo->startts.time();
            m_enddate = proginfo->endts.date();
            m_endtime = proginfo->endts.time();
            m_seriesid = proginfo->seriesid;
            m_programid = proginfo->programid;
            if (m_findday < 0)
            {
                m_findday = (proginfo->startts.date().dayOfWeek() + 1) % 7;
                m_findtime = proginfo->startts.time();

                QDate epoch(1970, 1, 1);
                m_findid = epoch.daysTo(proginfo->startts.date()) + 719528;
            }
            else
            {
                if (m_findid > 0)
                    m_findid = proginfo->findid;
                else
                {
                    QDate epoch(1970, 1, 1);
                    m_findid = epoch.daysTo(proginfo->startts.date()) + 719528;
                }
            }
            m_category = proginfo->category;
        }
        else
            m_playGroup = PlayGroup::GetInitialName(proginfo);
    }

    return true;
}

bool RecordingRule::LoadBySearch(RecSearchType lsearch, QString textname,
                                 QString forwhat, QString from)
{
    MSqlQuery query(MSqlQuery::InitCon());

    int rid = 0;
    query.prepare("SELECT recordid FROM record WHERE "
                  "search = :SEARCH AND description LIKE :FORWHAT");
    query.bindValue(":SEARCH", lsearch);
    query.bindValue(":FORWHAT", forwhat);

    if (query.exec() && query.next())
            rid = query.value(0).toInt();
    else
    {
        MythDB::DBError("loadBySearch", query);
        return false;
    }

    if (rid)
    {
        m_recordID = rid;
        if (!Load())
            return false;
    }
    else
    {
        QString searchType;
        switch (m_searchType)
        {
            case kPowerSearch:
                searchType = QObject::tr("Power Search");
                break;
            case kTitleSearch:
                searchType = QObject::tr("Title Search");
                break;
            case kKeywordSearch:
                searchType = QObject::tr("Keyword Search");
                break;
            case kPeopleSearch:
                searchType = QObject::tr("People Search");
                break;
            default:
                searchType = QObject::tr("Unknown Search");
                break;
        }
        
        QString ltitle = QString("%1 (%2)").arg(textname).arg(searchType);
        m_title = ltitle;
        m_subtitle = from;
        m_description = m_searchFor = forwhat;
        m_findday = (m_startdate.dayOfWeek() + 1) % 7;
        QDate epoch(1970, 1, 1);
        m_findid = epoch.daysTo(m_startdate) + 719528;
        m_searchTypeString = searchType;
    }

    return true;
}

bool RecordingRule::modifyPowerSearchByID(int rid, QString textname,
                                          QString forwhat, QString from)
{
    if (rid <= 0)
        return false;

    m_recordID = rid;
    if (!Load() || m_searchType != kPowerSearch)
        return false;

    QString ltitle = QString("%1 (%2)").arg(textname)
                                       .arg(QObject::tr("Power Search"));
    m_title = ltitle;
    m_subtitle = from;
    m_description = m_searchFor = forwhat;
    m_searchTypeString = QObject::tr("Power Search");

    return true;
}

bool RecordingRule::Save(bool sendSig)
{
    QString sql = "record "
                    "SET type = :TYPE, search = :SEARCHTYPE, "
                    "recpriority = :RECPRIORITY, prefinput = :INPUT, "
                    "startoffset = :STARTOFFSET, endoffset = :ENDOFFSET, "
                    "dupmethod = :DUPMETHOD, dupin = :DUPIN, "
                    "inactive = :INACTIVE, profile = :RECPROFILE, "
                    "recgroup = :RECGROUP, storagegroup = :STORAGEGROUP, "
                    "playgroup = :PLAYGROUP, autoexpire = :AUTOEXPIRE, "
                    "maxepisodes = :MAXEPISODES, maxnewest = :MAXNEWEST, "
                    "autocommflag = :AUTOCOMMFLAG, "
                    "autotranscode = :AUTOTRANSCODE, "
                    "transcoder = :TRANSCODER, autouserjob1 = :AUTOUSERJOB1, "
                    "autouserjob2 = :AUTOUSERJOB2, "
                    "autouserjob3 = :AUTOUSERJOB3, "
                    "autouserjob4 = :AUTOUSERJOB4, "
                    "parentid = :PARENTID, title = :TITLE, "
                    "subtitle = :SUBTITLE, description = :DESCRIPTION, "
                    "category = :CATEGORY, starttime = :STARTTIME, "
                    "startdate = :STARTDATE, endtime = :ENDTIME, "
                    "enddate = :ENDDATE, seriesid = :SERIESID, "
                    "programid = :PROGRAMID, chanid = :CHANID, "
                    "station = :STATION, findday = :FINDDAY, "
                    "findtime = :FINDTIME, findid = :FINDID, "
                    "next_record = :NEXTREC, last_record = :LASTREC, "
                    "last_delete = :LASTDELETE, avg_delay = :AVGDELAY ";

    QString sqlquery;
    if (m_recordID > 0)
        sqlquery = QString("UPDATE %1 WHERE recordid = :RECORDID;").arg(sql);
    else
        sqlquery = QString("INSERT INTO %1;").arg(sql);

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(sqlquery);
    query.bindValue(":TYPE", m_type);
    query.bindValue(":SEARCHTYPE", m_searchType);
    query.bindValue(":RECPRIORITY", m_recPriority);
    query.bindValue(":INPUT", m_prefInput);
    query.bindValue(":STARTOFFSET",  m_startOffset);
    query.bindValue(":ENDOFFSET", m_endOffset);
    query.bindValue(":DUPMETHOD", m_dupMethod);
    query.bindValue(":DUPIN", m_dupIn);
    query.bindValue(":INACTIVE", m_isInactive);
    query.bindValue(":RECPROFILE", m_recProfile);
    query.bindValue(":RECGROUP", m_recGroup);
    query.bindValue(":STORAGEGROUP", m_storageGroup);
    query.bindValue(":PLAYGROUP", m_playGroup);
    query.bindValue(":AUTOEXPIRE", m_autoExpire);
    query.bindValue(":MAXEPISODES", m_maxEpisodes);
    query.bindValue(":MAXNEWEST", m_maxNewest);
    query.bindValue(":AUTOCOMMFLAG", m_autoCommFlag);
    query.bindValue(":AUTOTRANSCODE", m_autoTranscode);
    query.bindValue(":TRANSCODER", m_transcoder);
    query.bindValue(":AUTOUSERJOB1", m_autoUserJob1);
    query.bindValue(":AUTOUSERJOB2", m_autoUserJob2);
    query.bindValue(":AUTOUSERJOB3", m_autoUserJob3);
    query.bindValue(":AUTOUSERJOB4", m_autoUserJob4);
    query.bindValue(":PARENTID", m_parentRecID);
    query.bindValue(":TITLE", m_title);
    query.bindValue(":SUBTITLE", m_subtitle);
    query.bindValue(":DESCRIPTION", m_description);
    query.bindValue(":CATEGORY", m_category);
    query.bindValue(":STARTTIME", m_starttime);
    query.bindValue(":STARTDATE", m_startdate);
    query.bindValue(":ENDTIME", m_endtime);
    query.bindValue(":ENDDATE", m_enddate);
    query.bindValue(":SERIESID", m_seriesid);
    query.bindValue(":PROGRAMID", m_programid);
    query.bindValue(":CHANID", m_channelid);
    query.bindValue(":STATION", m_station);
    query.bindValue(":FINDDAY", m_findday);
    query.bindValue(":FINDTIME", m_findtime);
    query.bindValue(":FINDID", m_findid);
    query.bindValue(":NEXTREC", m_nextRecording);
    query.bindValue(":LASTREC", m_lastRecorded);
    query.bindValue(":LASTDELETE", m_lastDeleted);
    query.bindValue(":AVGDELAY", m_averageDelay);

    if (m_recordID > 0)
        query.bindValue(":RECORDID", m_recordID);

    if (!query.exec())
        MythDB::DBError("UPDATE/INSERT record", query);

    if (sendSig)
        ScheduledRecording::signalChange(m_recordID);

    return true;
}

bool RecordingRule::Delete(bool sendSig)
{
    if (m_recordID < 0)
        return false;

    QString querystr;
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM record WHERE recordid = :RECORDID");
    query.bindValue(":RECORDID", m_recordID);
    if (!query.exec())
    {
        MythDB::DBError("ScheduledRecording::remove -- record", query);
        return false;
    }

    query.prepare("DELETE FROM oldfind WHERE recordid = :RECORDID");
    query.bindValue(":RECORDID", m_recordID);
    if (!query.exec())
    {
        MythDB::DBError("ScheduledRecording::remove -- oldfind", query);
    }

    if (sendSig)
        ScheduledRecording::signalChange(m_recordID);

    return true;
}
