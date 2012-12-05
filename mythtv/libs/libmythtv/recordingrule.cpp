
#include "recordingrule.h"

// libmythbase
#include "mythdb.h"

// libmyth
#include "mythcorecontext.h"

// libmythtv
#include "scheduledrecording.h" // For RescheduleMatch()
#include "playgroup.h" // For GetInitialName()
#include "recordingprofile.h" // For constants
#include "mythdate.h"

static inline QString null_to_empty(const QString &str)
{
    return str.isEmpty() ? "" : str;
}

// If the GetNumSetting() calls here are ever removed, update schema
// upgrade 1302 in dbcheck.cpp to manually apply them to the Default
// template.  Failing to do so will cause users upgrading from older
// versions to lose those settings.

RecordingRule::RecordingRule()
  : m_recordID(-1), m_parentRecID(0),
    m_isInactive(false),
    m_season(0),
    m_episode(0),
    m_starttime(),
    m_startdate(),
    m_endtime(),
    m_enddate(),
    m_inetref(), // String could be null when we trying to insert into DB
    m_channelid(0),
    m_findday(0),
    m_findtime(QTime::fromString("00:00:00", Qt::ISODate)),
    m_findid(QDate(1970, 1, 1).daysTo(MythDate::current().toLocalTime().date())
             + 719528),
    m_type(kNotRecording),
    m_searchType(kNoSearch),
    m_recPriority(0),
    m_prefInput(0),
    m_startOffset(gCoreContext->GetNumSetting("DefaultStartOffset", 0)),
    m_endOffset(gCoreContext->GetNumSetting("DefaultEndOffset", 0)),
    m_dupMethod(static_cast<RecordingDupMethodType>(
                gCoreContext->GetNumSetting("prefDupMethod", kDupCheckSubDesc))),
    m_dupIn(kDupsInAll),
    m_filter(GetDefaultFilter()),
    m_recProfile(QObject::tr("Default")),
    m_recGroup("Default"),
    m_storageGroup("Default"),
    m_playGroup("Default"),
    m_autoExpire(gCoreContext->GetNumSetting("AutoExpireDefault", 0)),
    m_maxEpisodes(0),
    m_maxNewest(false),
    m_autoCommFlag(gCoreContext->GetNumSetting("AutoCommercialFlag", 1)),
    m_autoTranscode(gCoreContext->GetNumSetting("AutoTranscode", 0)),
    m_transcoder(gCoreContext->GetNumSetting("DefaultTranscoder",
                RecordingProfile::TranscoderAutodetect)),
    m_autoUserJob1(gCoreContext->GetNumSetting("AutoRunUserJob1", 0)),
    m_autoUserJob2(gCoreContext->GetNumSetting("AutoRunUserJob2", 0)),
    m_autoUserJob3(gCoreContext->GetNumSetting("AutoRunUserJob3", 0)),
    m_autoUserJob4(gCoreContext->GetNumSetting("AutoRunUserJob4", 0)),
    m_autoMetadataLookup(gCoreContext->GetNumSetting("AutoMetadataLookup", 1)),
    m_nextRecording(MythDate::fromString("0000-00-00T00:00:00")),
    m_lastRecorded(MythDate::fromString("0000-00-00T00:00:00")),
    m_lastDeleted(MythDate::fromString("0000-00-00T00:00:00")),
    m_averageDelay(100),
    m_recordTable("record"),
    m_tempID(0),
    m_isOverride(false),
    m_isTemplate(false),
    m_template(),
    m_progInfo(NULL),
    m_loaded(false)
{
    QDateTime dt = MythDate::current();
    m_enddate = m_startdate = dt.date();
    m_endtime = m_starttime = dt.time();
}

bool RecordingRule::Load(bool asTemplate)
{
    if (m_recordID <= 0)
        return false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT type, search, "
    "recpriority, prefinput, startoffset, endoffset, dupmethod, dupin, "
    "inactive, profile, recgroup, storagegroup, playgroup, autoexpire, "
    "maxepisodes, maxnewest, autocommflag, autotranscode, transcoder, "
    "autouserjob1, autouserjob2, autouserjob3, autouserjob4, "
    "autometadata, parentid, title, subtitle, description, season, episode, "
    "category, starttime, startdate, endtime, enddate, seriesid, programid, "
    "inetref, chanid, station, findday, findtime, findid, "
    "next_record, last_record, last_delete, avg_delay, filter "
    "FROM record WHERE recordid = :RECORDID ;");

    query.bindValue(":RECORDID", m_recordID);

    if (!query.exec())
    {
        MythDB::DBError("SELECT record", query);
        return false;
    }

    if (!query.next())
        return false;

    // Schedule
    if (!asTemplate)
    {
        m_type = static_cast<RecordingType>(query.value(0).toInt());
        m_searchType = static_cast<RecSearchType>(query.value(1).toInt());
    }
    m_recPriority = query.value(2).toInt();
    m_prefInput = query.value(3).toInt();
    m_startOffset = query.value(4).toInt();
    m_endOffset = query.value(5).toInt();
    m_dupMethod = static_cast<RecordingDupMethodType>
        (query.value(6).toInt());
    m_dupIn = static_cast<RecordingDupInType>(query.value(7).toInt());
    m_filter = query.value(47).toUInt();
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
    m_autoMetadataLookup = query.value(23).toBool();

    // Original rule id for override rule
    if (!asTemplate)
        m_parentRecID = query.value(24).toInt();

    // Recording metadata
    if (!asTemplate)
    {
        m_title = query.value(25).toString();
        m_subtitle = query.value(26).toString();
        m_description = query.value(27).toString();
        m_season = query.value(28).toUInt();
        m_episode = query.value(29).toUInt();
        m_category = query.value(30).toString();
        m_starttime = query.value(31).toTime();
        m_startdate = query.value(32).toDate();
        m_endtime = query.value(33).toTime();
        m_enddate = query.value(34).toDate();
        m_seriesid = query.value(35).toString();
        m_programid = query.value(36).toString();
        m_inetref = query.value(37).toString();
    }

    // Associated data for rule types
    if (!asTemplate)
    {
        m_channelid = query.value(38).toInt();
        m_station = query.value(39).toString();
        m_findday = query.value(40).toInt();
        m_findtime = query.value(41).toTime();
        m_findid = query.value(42).toInt();
    }

    // Statistic fields - Used to generate statistics about particular rules
    // and influence watch list weighting
    if (!asTemplate)
    {
        m_nextRecording = MythDate::as_utc(query.value(43).toDateTime());
        m_lastRecorded = MythDate::as_utc(query.value(44).toDateTime());
        m_lastDeleted = MythDate::as_utc(query.value(45).toDateTime());
        m_averageDelay = query.value(46).toInt();
    }

    m_isOverride = (m_type == kOverrideRecord || m_type == kDontRecord);
    m_isTemplate = (m_type == kTemplateRecord);
    m_template = (asTemplate || m_isTemplate) ? 
        query.value(30).toString() : "";

    if (!asTemplate)
        m_loaded = true;

    return true;
}

bool RecordingRule::LoadByProgram(const ProgramInfo* proginfo)
{
    if (!proginfo)
        return false;

    m_progInfo = proginfo;

    m_recordID = proginfo->GetRecordingRuleID();
    if (m_recordID)
        Load();
    else
        LoadTemplate(proginfo->GetCategory(), proginfo->GetCategoryType());

    if (m_type != kTemplateRecord &&
        (m_searchType == kNoSearch || m_searchType == kManualSearch))
    {
        AssignProgramInfo();
        if (!proginfo->GetRecordingRuleID())
            m_playGroup = PlayGroup::GetInitialName(proginfo);
    }

    m_loaded = true;
    return true;
}

bool RecordingRule::LoadBySearch(RecSearchType lsearch, QString textname,
                                 QString forwhat, QString from,
                                 ProgramInfo *pginfo)
{
    MSqlQuery query(MSqlQuery::InitCon());

    int rid = 0;
    query.prepare("SELECT recordid FROM record WHERE "
                  "search = :SEARCH AND description LIKE :FORWHAT");
    query.bindValue(":SEARCH", lsearch);
    query.bindValue(":FORWHAT", forwhat);

    if (query.exec())
    {
        if (query.next())
            rid = query.value(0).toInt();
        // else rid is zero, which is valid, we're looking at a new rule
    }
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
        LoadTemplate("Default");

        QString searchType;
        m_searchType = lsearch;
        searchType = SearchTypeToString(m_searchType);

        QString ltitle = QString("%1 (%2)").arg(textname).arg(searchType);
        m_title = ltitle;
        m_subtitle = from;
        m_description = forwhat;

        if (pginfo)
        {
            m_findday =
                (pginfo->GetScheduledStartTime().toLocalTime().date()
                 .dayOfWeek() + 1) % 7;
            m_findtime = pginfo->GetScheduledStartTime().toLocalTime().time();
            m_findid = QDate(1970, 1, 1).daysTo(
                pginfo->GetScheduledStartTime().toLocalTime().date()) + 719528;
        }
    }

    m_loaded = true;
    return true;
}

bool RecordingRule::LoadTemplate(QString category, QString categoryType)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT recordid, category, "
                  "       (category = :CAT1) AS catmatch, "
                  "       (category = :CATTYPE1) AS typematch "
                  "FROM record "
                  "WHERE type = :TEMPLATE AND "
                  "      (category = :CAT2 OR category = :CATTYPE2 "
                  "       OR category = 'Default') "
                  "ORDER BY catmatch DESC, typematch DESC"
                  );
    query.bindValue(":TEMPLATE", kTemplateRecord);
    query.bindValue(":CAT1", category);
    query.bindValue(":CAT2", category);
    query.bindValue(":CATTYPE1", categoryType);
    query.bindValue(":CATTYPE2", categoryType);

    if (!query.exec())
    {
        MythDB::DBError("LoadByTemplate", query);
        return false;
    }

    if (!query.next())
        return false;

    int savedRecordID = m_recordID;
    m_recordID = query.value(0).toInt();
    bool result = Load(true);
    m_recordID = savedRecordID;

    return result;
}

bool RecordingRule::MakeTemplate(QString category)
{
    if (m_recordID > 0)
        return false;

    if (category.compare(QObject::tr("Default"), Qt::CaseInsensitive) == 0)
    {
        category = "Default";
        m_title = QObject::tr("Default (Template)");
    }
    else
    {
        m_title = category + QObject::tr(" (Template)");
    }

    LoadTemplate(category);
    m_recordID = 0;
    m_type = kNotRecording;
    m_category = category;
    m_loaded = true;
    m_isTemplate = true;

    return true;
}

bool RecordingRule::ModifyPowerSearchByID(int rid, QString textname,
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
    m_description = forwhat;

    m_loaded = true;
    return true;
}

bool RecordingRule::MakeOverride(void)
{
    if (m_recordID <= 0)
        return false;

    if (m_type == kOverrideRecord || m_type == kDontRecord)
        return false;

    m_isOverride = true;
    m_parentRecID = m_recordID;
    m_recordID = 0;
    m_type = kNotRecording;
    m_isInactive = 0;

    if (m_searchType != kManualSearch)
        m_searchType = kNoSearch;

    AssignProgramInfo();

    return true;
}

bool RecordingRule::Save(bool sendSig)
{
    QString sql =   "SET type = :TYPE, search = :SEARCHTYPE, "
                    "recpriority = :RECPRIORITY, prefinput = :INPUT, "
                    "startoffset = :STARTOFFSET, endoffset = :ENDOFFSET, "
                    "dupmethod = :DUPMETHOD, dupin = :DUPIN, "
                    "filter = :FILTER, "
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
                    "autometadata = :AUTOMETADATA, "
                    "parentid = :PARENTID, title = :TITLE, "
                    "subtitle = :SUBTITLE, season = :SEASON, episode = :EPISODE, "
                    "description = :DESCRIPTION, category = :CATEGORY, "
                    "starttime = :STARTTIME, startdate = :STARTDATE, "
                    "endtime = :ENDTIME, enddate = :ENDDATE, seriesid = :SERIESID, "
                    "programid = :PROGRAMID, inetref = :INETREF, chanid = :CHANID, "
                    "station = :STATION, findday = :FINDDAY, "
                    "findtime = :FINDTIME, findid = :FINDID, "
                    "next_record = :NEXTREC, last_record = :LASTREC, "
                    "last_delete = :LASTDELETE, avg_delay = :AVGDELAY ";

    QString sqlquery;
    if (m_recordID > 0 || (m_recordTable != "record" && m_tempID > 0))
        sqlquery = QString("UPDATE %1 %2 WHERE recordid = :RECORDID;")
                                                        .arg(m_recordTable).arg(sql);
    else
        sqlquery = QString("INSERT INTO %1 %2;").arg(m_recordTable).arg(sql);

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
    query.bindValue(":FILTER", m_filter);
    query.bindValue(":INACTIVE", m_isInactive);
    query.bindValue(":RECPROFILE", null_to_empty(m_recProfile));
    query.bindValue(":RECGROUP", null_to_empty(m_recGroup));
    query.bindValue(":STORAGEGROUP", null_to_empty(m_storageGroup));
    query.bindValue(":PLAYGROUP", null_to_empty(m_playGroup));
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
    query.bindValue(":AUTOMETADATA", m_autoMetadataLookup);
    query.bindValue(":PARENTID", m_parentRecID);
    query.bindValue(":TITLE", m_title);
    query.bindValue(":SUBTITLE", null_to_empty(m_subtitle));
    query.bindValue(":DESCRIPTION", null_to_empty(m_description));
    query.bindValue(":SEASON", m_season);
    query.bindValue(":EPISODE", m_episode);
    query.bindValue(":CATEGORY", null_to_empty(m_category));
    query.bindValue(":STARTTIME", m_starttime);
    query.bindValue(":STARTDATE", m_startdate);
    query.bindValue(":ENDTIME", m_endtime);
    query.bindValue(":ENDDATE", m_enddate);
    query.bindValue(":SERIESID", null_to_empty(m_seriesid));
    query.bindValue(":PROGRAMID", null_to_empty(m_programid));
    query.bindValue(":INETREF", null_to_empty(m_inetref));
    query.bindValue(":CHANID", m_channelid);
    query.bindValue(":STATION", null_to_empty(m_station));
    query.bindValue(":FINDDAY", m_findday);
    query.bindValue(":FINDTIME", m_findtime);
    query.bindValue(":FINDID", m_findid);
    query.bindValue(":NEXTREC", m_nextRecording);
    query.bindValue(":LASTREC", m_lastRecorded);
    query.bindValue(":LASTDELETE", m_lastDeleted);
    query.bindValue(":AVGDELAY", m_averageDelay);

    if (m_recordTable != "record" && m_tempID > 0)
        query.bindValue(":RECORDID", m_tempID);
    else if (m_recordID > 0)
        query.bindValue(":RECORDID", m_recordID);

    if (!query.exec())
        MythDB::DBError("UPDATE/INSERT record", query);
    else if (m_recordTable != "record" && m_tempID <= 0)
        m_tempID = query.lastInsertId().toInt();
    else if (m_recordID <= 0)
        m_recordID = query.lastInsertId().toInt();

    if (sendSig)
        ScheduledRecording::RescheduleMatch(m_recordID, 0, 0, QDateTime(),
            QString("SaveRule %1").arg(m_title));

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
        ScheduledRecording::RescheduleMatch(m_recordID, 0, 0, QDateTime(),
            QString("DeleteRule %1").arg(m_title));

    // Set m_recordID to zero, the rule is no longer in the database so it's
    // not valid. Should you want, this allows a rule to be removed from the
    // database and then re-inserted with Save()
    m_recordID = 0;

    return true;
}

void RecordingRule::ToMap(InfoMap &infoMap) const
{
    if (m_title == "Default (Template)")
        infoMap["title"] = QObject::tr("Default (Template)");
    else
        infoMap["title"] = m_title;
    infoMap["subtitle"] = m_subtitle;
    infoMap["description"] = m_description;
    infoMap["season"] = QString::number(m_season);
    infoMap["episode"] = QString::number(m_episode);

    if (m_category == "Default")
        infoMap["category"] = QObject::tr("Default");
    else
        infoMap["category"] = m_category;
    infoMap["callsign"] = m_station;

    QDateTime starttm(m_startdate, m_starttime, Qt::UTC);
    infoMap["starttime"] = MythDate::toString(starttm, MythDate::kTime);
    infoMap["startdate"] = MythDate::toString(
        starttm, MythDate::kDateFull | MythDate::kSimplify);

    QDateTime endtm(m_enddate, m_endtime, Qt::UTC);
    infoMap["endtime"] = MythDate::toString(endtm, MythDate::kTime);
    infoMap["enddate"] = MythDate::toString(
        endtm, MythDate::kDateFull | MythDate::kSimplify);

    infoMap["inetref"] = m_inetref;
    infoMap["chanid"] = m_channelid;
    infoMap["channel"] = m_station;

    QDateTime startts(m_startdate, m_starttime, Qt::UTC);
    QDateTime endts(m_enddate, m_endtime, Qt::UTC);

    QString length;
    int hours, minutes, seconds;
    seconds = startts.secsTo(endts);

    minutes = seconds / 60;
    infoMap["lenmins"] = QObject::tr("%n minute(s)","",minutes);
    hours   = minutes / 60;
    minutes = minutes % 60;

    QString minstring = QObject::tr("%n minute(s)","",minutes);

    if (hours > 0)
    {
        infoMap["lentime"] = QString("%1 %2")
                                    .arg(QObject::tr("%n hour(s)","", hours))
                                    .arg(minstring);
    }
    else
        infoMap["lentime"] = minstring;


    infoMap["timedate"] = MythDate::toString(
        startts, MythDate::kDateTimeFull | MythDate::kSimplify) + " - " +
        MythDate::toString(endts, MythDate::kTime);

    infoMap["shorttimedate"] =
        MythDate::toString(
            startts, MythDate::kDateTimeShort | MythDate::kSimplify) + " - " +
        MythDate::toString(endts, MythDate::kTime);

    if (m_type == kDailyRecord || m_type == kWeeklyRecord)
    {
        QDateTime ldt =
            QDateTime(MythDate::current().toLocalTime().date(), m_findtime,
                      Qt::LocalTime);
        QString findfrom = MythDate::toString(ldt, MythDate::kTime);
        if (m_type == kWeeklyRecord)
        {
            int daynum = (m_findday + 5) % 7 + 1;
            findfrom = QString("%1, %2").arg(QDate::shortDayName(daynum))
                                        .arg(findfrom);
        }
        infoMap["subtitle"] = QObject::tr("(%1 or later) %3",
                                          "e.g. (Sunday or later) program "
                                          "subtitle").arg(findfrom)
                                          .arg(m_subtitle);
    }

    infoMap["searchtype"] = SearchTypeToString(m_searchType);
    if (m_searchType != kNoSearch)
        infoMap["searchforwhat"] = m_description;

    using namespace MythDate;
    if (m_nextRecording.isValid())
    {
        infoMap["nextrecording"] = MythDate::toString(
            m_nextRecording, kDateFull | kAddYear);
    }
    if (m_lastRecorded.isValid())
    {
        infoMap["lastrecorded"] = MythDate::toString(
            m_lastRecorded, kDateFull | kAddYear);
    }
    if (m_lastDeleted.isValid())
    {
        infoMap["lastdeleted"] = MythDate::toString(
            m_lastDeleted, kDateFull | kAddYear);
    }

    infoMap["ruletype"] = toString(m_type);
    infoMap["rectype"] = toString(m_type);

    if (m_template == "Default")
        infoMap["template"] = QObject::tr("Default");
    else
        infoMap["template"] = m_template;
}

void RecordingRule::UseTempTable(bool usetemp, QString table)
{
    MSqlQuery query(MSqlQuery::SchedCon());

    if (usetemp)
    {
        m_recordTable = table;

        query.prepare("SELECT GET_LOCK(:LOCK, 2);");
        query.bindValue(":LOCK", "DiffSchedule");
        if (!query.exec())
        {
            MythDB::DBError("Obtaining lock in testRecording", query);
            return;
        }

        query.prepare(QString("DROP TABLE IF EXISTS %1;").arg(table));
        if (!query.exec())
        {
            MythDB::DBError("Deleting old table in testRecording", query);
            return;
        }

        query.prepare(QString("CREATE TABLE %1 SELECT * FROM record;")
                        .arg(table));
        if (!query.exec())
        {
            MythDB::DBError("Create new temp table", query);
            return;
        }

        query.prepare(QString("ALTER TABLE %1 MODIFY recordid int(10) "
                              "UNSIGNED NOT NULL AUTO_INCREMENT primary key;")
                              .arg(table));
        if (!query.exec())
        {
            MythDB::DBError("Modify recordid column to include "
                            "auto-increment", query);
            return;
        }

        if (m_recordID > 0)
            m_tempID = m_recordID;

        Save(false);
    }
    else
    {
        query.prepare("SELECT RELEASE_LOCK(:LOCK);");
        query.bindValue(":LOCK", "DiffSchedule");
        if (!query.exec())
        {
            MythDB::DBError("Free lock", query);
            return;
        }
        m_recordTable = "record";
        m_tempID = 0;
    }
}

void RecordingRule::AssignProgramInfo()
{
    if (!m_progInfo)
        return;

    m_title       = m_progInfo->GetTitle();
    m_subtitle    = m_progInfo->GetSubtitle();
    m_description = m_progInfo->GetDescription();
    m_channelid   = m_progInfo->GetChanID();
    m_station     = m_progInfo->GetChannelSchedulingID();
    m_startdate   = m_progInfo->GetScheduledStartTime().date();
    m_starttime   = m_progInfo->GetScheduledStartTime().time();
    m_enddate     = m_progInfo->GetScheduledEndTime().date();
    m_endtime     = m_progInfo->GetScheduledEndTime().time();
    m_seriesid    = m_progInfo->GetSeriesID();
    m_programid   = m_progInfo->GetProgramID();
    if (m_recordID <= 0)
    {
        m_findday =
            (m_progInfo->GetScheduledStartTime().toLocalTime().date()
             .dayOfWeek() + 1) % 7;
        m_findtime = m_progInfo->GetScheduledStartTime().toLocalTime().time();
        m_findid = QDate(1970, 1, 1).daysTo(
            m_progInfo->GetScheduledStartTime().toLocalTime().date()) + 719528;
    }
    else
    {
        if (m_findid > 0)
            m_findid = m_progInfo->GetFindID();
        else
        {
            QDate epoch(1970, 1, 1);
            m_findid = epoch.daysTo(
                m_progInfo->GetScheduledStartTime().toLocalTime().date())
                + 719528;
        }
    }
    m_category = m_progInfo->GetCategory();

    if (m_inetref.isEmpty())
    {
        m_inetref = m_progInfo->GetInetRef();
        m_season  = m_progInfo->GetSeason();
        m_episode = m_progInfo->GetEpisode();
    }
}

unsigned RecordingRule::GetDefaultFilter(void)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT SUM(1 << filterid) FROM recordfilter "
                  "WHERE filterid >= 0 AND filterid < :NUMFILTERS AND "
                  "      TRIM(clause) <> '' AND newruledefault <> 0");
    query.bindValue(":NUMFILTERS", RecordingRule::kNumFilters);
    if (!query.exec())
    {
        MythDB::DBError("GetDefaultFilter", query);
        return 0;
    }

    if (!query.next())
        return 0;
    
    return query.value(0).toUInt();
}

QString RecordingRule::SearchTypeToString(const RecSearchType searchType)
{
    QString searchTypeString;

    switch (searchType)
    {
        case kNoSearch:
            searchTypeString = ""; // Allow themers to decide what to display
            break;
        case kPowerSearch:
            searchTypeString = QObject::tr("Power Search");
            break;
        case kTitleSearch:
            searchTypeString = QObject::tr("Title Search");
            break;
        case kKeywordSearch:
            searchTypeString = QObject::tr("Keyword Search");
            break;
        case kPeopleSearch:
            searchTypeString = QObject::tr("People Search");
            break;
        default:
            searchTypeString = QObject::tr("Unknown Search");
            break;
    }

    return searchTypeString;
}

QStringList RecordingRule::GetTemplateNames(void)
{
    QStringList result;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT category "
                  "FROM record "
                  "WHERE type = :TEMPLATE "
                  "ORDER BY category = 'Default' DESC, category"
                  );
    query.bindValue(":TEMPLATE", kTemplateRecord);

    if (!query.exec())
    {
        MythDB::DBError("LoadByTemplate", query);
        return result;
    }

    while (query.next())
        result << query.value(0).toString();

    return result;
}
