
#include "recordingrule.h"

#include <utility>

#include <QTimeZone>

// libmythbase
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythdb.h"
#include "libmythbase/mythsorthelper.h"

// libmythtv
#include "scheduledrecording.h" // For RescheduleMatch()
#include "playgroup.h" // For GetInitialName()
#include "recordingprofile.h" // For constants

static inline QString null_to_empty(const QString &str)
{
    return str.isEmpty() ? "" : str;
}

// If the GetNumSetting() calls here are ever removed, update schema
// upgrade 1302 in dbcheck.cpp to manually apply them to the Default
// template.  Failing to do so will cause users upgrading from older
// versions to lose those settings.

RecordingRule::RecordingRule()
  : m_findtime(QTime::fromString("00:00:00", Qt::ISODate)),
    m_findid(QDate(1970, 1, 1).daysTo(MythDate::current().toLocalTime().date())
             + 719528)
{
    QDateTime dt = MythDate::current();
    m_enddate = m_startdate = dt.date();
    m_endtime = m_starttime = dt.time();

    ensureSortFields();
}

/**
 *  \brief Ensure that the sortTitle and sortSubtitle fields are set.
 */
void RecordingRule::ensureSortFields(void)
{
    std::shared_ptr<MythSortHelper>sh = getMythSortHelper();

    if (m_sortTitle.isEmpty() and not m_title.isEmpty())
        m_sortTitle = sh->doTitle(m_title);
    if (m_sortSubtitle.isEmpty() and not m_subtitle.isEmpty())
        m_sortSubtitle = sh->doTitle(m_subtitle);
}

/** Load a single rule from the recorded table.  Which rule is
 *  specified in the member variable m_recordID.
 */
bool RecordingRule::Load(bool asTemplate)
{
    if (m_recordID <= 0)
        return false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT type, search, " // 00-01
    "recpriority, prefinput, startoffset, endoffset, dupmethod, dupin, " // 02-07
    "inactive, profile, recgroup, storagegroup, playgroup, autoexpire, " // 08-13
    "maxepisodes, maxnewest, autocommflag, autotranscode, transcoder, " // 14-18
    "autouserjob1, autouserjob2, autouserjob3, autouserjob4, " // 19-22
    "autometadata, parentid, title, subtitle, description, season, episode, " // 23-29
    "category, starttime, startdate, endtime, enddate, seriesid, programid, " // 30-36
    "inetref, chanid, station, findday, findtime, findid, " // 37-42
    "next_record, last_record, last_delete, avg_delay, filter, recgroupid, " // 43-48
    "autoextend " // 49
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
    m_autoExtend = static_cast<AutoExtendType>(query.value(49).toUInt());

    // Storage
    m_recProfile = query.value(9).toString();
    m_recGroupID = query.value(48).toUInt();
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

    if (!asTemplate)
    {
        // Original rule id for override rule
        m_parentRecID = query.value(24).toInt();

        // Recording metadata
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

        // Associated data for rule types
        m_channelid = query.value(38).toInt();
        m_station = query.value(39).toString();
        m_findday = query.value(40).toInt();
        m_findtime = query.value(41).toTime();
        m_findid = query.value(42).toInt();

        // Statistic fields - Used to generate statistics about particular rules
        // and influence watch list weighting
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

    ensureSortFields();
    return true;
}

bool RecordingRule::LoadByProgram(const ProgramInfo* proginfo)
{
    if (!proginfo)
        return false;

    m_progInfo = proginfo;

    m_recordID = proginfo->GetRecordingRuleID();
    if (m_recordID)
    {
        if (!Load())
            return false;
    }
    else
    {
        LoadTemplate(proginfo->GetTitle(), proginfo->GetCategory(),
                     proginfo->GetCategoryTypeString());
    }

    if (m_type != kTemplateRecord &&
        (m_searchType == kNoSearch || m_searchType == kManualSearch))
    {
        AssignProgramInfo();
        if (!proginfo->GetRecordingRuleID())
            m_playGroup = PlayGroup::GetInitialName(proginfo);
    }

    ensureSortFields();
    m_loaded = true;
    return true;
}

/**
 *  Load a recording rule based on search parameters.
 *
 *  \param lsearch The type of search.
 *  \param textname Unused.  This is the raw text from a power search
 *                  box, or the title from a custom search box.
 *  \param forwhat  Sql describing the program search.  I.E. Somthing
 *                  like "program.title LIKE '%title%'".
 *  \param joininfo Sql used to join additional tables into the
 *                  search.  It is inserted into a query after the
 *                  FROM clause and before the WHERE clause.  For an
 *                  example, see the text inserted when matching on a
 *                  genre.
 *  \param pginfo   A pointer to an existing ProgramInfo structure.
 *  \return True if search data was successfully loaded.
 */
bool RecordingRule::LoadBySearch(RecSearchType lsearch, const QString& textname,
                                 const QString& forwhat, QString joininfo,
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

        QString ltitle = QString("%1 (%2)").arg(textname, searchType);
        m_title = ltitle;
        m_sortTitle = nullptr;
        m_subtitle = m_sortSubtitle = std::move(joininfo);
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

    ensureSortFields();
    m_loaded = true;
    return true;
}

bool RecordingRule::LoadTemplate(const QString& title,
                                 const QString& category,
                                 const QString& categoryType)
{
    QString lcategory = category.isEmpty() ? "Default" : category;
    QString lcategoryType = categoryType.isEmpty() ? "Default" : categoryType;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT recordid, category, "
                  "       (category = :TITLE1) AS titlematch, "
                  "       (category = :CAT1) AS catmatch, "
                  "       (category = :CATTYPE1) AS typematch "
                  "FROM record "
                  "WHERE type = :TEMPLATE AND "
                  "      (category IN (:TITLE2, :CAT2, :CATTYPE2) "
                  "       OR category = 'Default') "
                  "ORDER BY titlematch DESC, catmatch DESC, typematch DESC"
                  );
    query.bindValue(":TEMPLATE", kTemplateRecord);
    query.bindValue(":TITLE1", title);
    query.bindValue(":TITLE2", title);
    query.bindValue(":CAT1", lcategory);
    query.bindValue(":CAT2", lcategory);
    query.bindValue(":CATTYPE1", lcategoryType);
    query.bindValue(":CATTYPE2", lcategoryType);

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

    if (category.compare(tr("Default"), Qt::CaseInsensitive) == 0)
    {
        category = "Default";
        m_title = tr("Default (Template)");
        m_sortTitle = m_title;
    }
    else
    {
        //: %1 is the category
        m_title = tr("%1 (Template)").arg(category);
        m_sortTitle = m_title;
    }

    LoadTemplate(category);
    m_recordID = 0;
    m_type = kNotRecording;
    m_category = category;
    m_loaded = true;
    m_isTemplate = true;

    return true;
}

bool RecordingRule::ModifyPowerSearchByID(int rid, const QString& textname,
                                          QString forwhat, QString joininfo)
{
    if (rid <= 0)
        return false;

    m_recordID = rid;
    if (!Load() || m_searchType != kPowerSearch)
        return false;

    QString ltitle = QString("%1 (%2)").arg(textname, tr("Power Search"));
    m_title = ltitle;
    m_sortTitle = nullptr;
    m_subtitle = m_sortSubtitle = std::move(joininfo);
    m_description = std::move(forwhat);

    ensureSortFields();
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
    m_isInactive = false;

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
                    "filter = :FILTER, autoextend = :AUTOEXTEND, "
                    "inactive = :INACTIVE, profile = :RECPROFILE, "
                    "recgroup = :RECGROUP, "
                    "recgroupid = :RECGROUPID, "
                    "storagegroup = :STORAGEGROUP, "
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
    {
        sqlquery = QString("UPDATE %1 %2 WHERE recordid = :RECORDID;")
                                                        .arg(m_recordTable, sql);
    }
    else
    {
        sqlquery = QString("INSERT INTO %1 %2;").arg(m_recordTable, sql);
    }

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
    query.bindValue(":AUTOEXTEND", static_cast<int>(m_autoExtend));
    query.bindValue(":INACTIVE", m_isInactive);
    query.bindValue(":RECPROFILE", null_to_empty(m_recProfile));
    // Temporary, remove once transition to recgroupid is complete
    query.bindValue(":RECGROUP", null_to_empty(RecordingInfo::GetRecgroupString(m_recGroupID)));
    query.bindValue(":RECGROUPID", m_recGroupID);
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
    {
        MythDB::DBError("UPDATE/INSERT record", query);
        return false;
    }

    if (m_recordTable != "record" && m_tempID <= 0)
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

    if (m_searchType == kManualSearch && m_recordID)
    {
        query.prepare("DELETE FROM program WHERE manualid = :RECORDID");
        query.bindValue(":RECORDID", m_recordID);
        if (!query.exec())
        {
            MythDB::DBError("ScheduledRecording::remove -- oldfind", query);
        }
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

void RecordingRule::ToMap(InfoMap &infoMap, uint date_format) const
{
    if (m_title == "Default (Template)")
    {
        infoMap["title"] = tr("Default (Template)");
        infoMap["sorttitle"] = tr("Default (Template)");
    }
    else
    {
        infoMap["title"] = m_title;
        infoMap["sorttitle"] = m_sortTitle;
    }
    infoMap["subtitle"] = m_subtitle;
    infoMap["sortsubtitle"] = m_sortSubtitle;
    infoMap["description"] = m_description;
    infoMap["season"] = QString::number(m_season);
    infoMap["episode"] = QString::number(m_episode);

    if (m_category == "Default")
        infoMap["category"] = tr("Default", "category");
    else
        infoMap["category"] = m_category;
    infoMap["callsign"] = m_station;

#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
    QDateTime starttm(m_startdate, m_starttime, Qt::UTC);
#else
    QDateTime starttm(m_startdate, m_starttime, QTimeZone(QTimeZone::UTC));
#endif
    infoMap["starttime"] = MythDate::toString(starttm, date_format | MythDate::kTime);
    infoMap["startdate"] = MythDate::toString(
        starttm, date_format | MythDate::kDateFull | MythDate::kSimplify);

#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
    QDateTime endtm(m_enddate, m_endtime, Qt::UTC);
#else
    QDateTime endtm(m_enddate, m_endtime, QTimeZone(QTimeZone::UTC));
#endif
    infoMap["endtime"] = MythDate::toString(endtm, date_format | MythDate::kTime);
    infoMap["enddate"] = MythDate::toString(
        endtm, date_format | MythDate::kDateFull | MythDate::kSimplify);

    infoMap["inetref"] = m_inetref;
    infoMap["chanid"] = QString::number(m_channelid);
    infoMap["channel"] = m_station;

#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
    QDateTime startts(m_startdate, m_starttime, Qt::UTC);
    QDateTime endts(m_enddate, m_endtime, Qt::UTC);
#else
    static const QTimeZone utc(QTimeZone::UTC);
    QDateTime startts(m_startdate, m_starttime, utc);
    QDateTime endts(m_enddate, m_endtime, utc);
#endif

    int seconds = startts.secsTo(endts);
    int minutes = seconds / 60;
    int hours   = minutes / 60;
    minutes = minutes % 60;

    infoMap["lenmins"] = QCoreApplication::translate("(Common)", "%n minute(s)",
        "", minutes);

    QString minstring  = QCoreApplication::translate("(Common)", "%n minute(s)",
        "", minutes);

    QString hourstring = QCoreApplication::translate("(Common)", "%n hour(s)",
        "", hours);

    if (hours > 0)
    {
        //: Time duration, %1 is replaced by the hours, %2 by the minutes
        infoMap["lentime"] = QCoreApplication::translate("(Common)", "%1 %2",
            "Hours and minutes").arg(hourstring, minstring);
    }
    else
    {
        infoMap["lentime"] = minstring;
    }


    infoMap["timedate"] = MythDate::toString(
        startts, date_format | MythDate::kDateTimeFull | MythDate::kSimplify) + " - " +
        MythDate::toString(endts, date_format | MythDate::kTime);

    infoMap["shorttimedate"] =
        MythDate::toString(
            startts, date_format | MythDate::kDateTimeShort | MythDate::kSimplify) + " - " +
        MythDate::toString(endts, date_format | MythDate::kTime);

    if (m_type == kDailyRecord || m_type == kWeeklyRecord)
    {
#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
        QDateTime ldt =
            QDateTime(MythDate::current().toLocalTime().date(), m_findtime,
                      Qt::LocalTime);
#else
        QDateTime ldt =
            QDateTime(MythDate::current().toLocalTime().date(), m_findtime, utc);
#endif
        QString findfrom = MythDate::toString(ldt, date_format | MythDate::kTime);
        if (m_type == kWeeklyRecord)
        {
            int daynum = ((m_findday + 5) % 7) + 1;
            findfrom = QString("%1, %2")
		 .arg(gCoreContext->GetQLocale().dayName(daynum, QLocale::ShortFormat),
                      findfrom);
        }
        infoMap["subtitle"] = tr("(%1 or later) %3",
                                 "e.g. (Sunday or later) program "
                                 "subtitle").arg(findfrom, m_subtitle);
    }

    infoMap["searchtype"] = SearchTypeToString(m_searchType);
    if (m_searchType != kNoSearch)
        infoMap["searchforwhat"] = m_description;

    using namespace MythDate;
    if (m_nextRecording.isValid())
    {
        infoMap["nextrecording"] = MythDate::toString(
            m_nextRecording, date_format | kDateFull | kAddYear);
    }
    if (m_lastRecorded.isValid())
    {
        infoMap["lastrecorded"] = MythDate::toString(
            m_lastRecorded, date_format | kDateFull | kAddYear);
    }
    if (m_lastDeleted.isValid())
    {
        infoMap["lastdeleted"] = MythDate::toString(
            m_lastDeleted, date_format | kDateFull | kAddYear);
    }

    infoMap["ruletype"] = toString(m_type);
    infoMap["rectype"] = toString(m_type);
    infoMap["autoextend"] = toString(m_autoExtend);

    if (m_template == "Default")
        infoMap["template"] = tr("Default", "Default template");
    else
        infoMap["template"] = m_template;
}

void RecordingRule::UseTempTable(bool usetemp, const QString& table)
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
    m_sortTitle   = m_progInfo->GetSortTitle();
    m_subtitle    = m_progInfo->GetSubtitle();
    m_sortSubtitle= m_progInfo->GetSortSubtitle();
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
    ensureSortFields();
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
            searchTypeString = tr("Power Search");
            break;
        case kTitleSearch:
            searchTypeString = tr("Title Search");
            break;
        case kKeywordSearch:
            searchTypeString = tr("Keyword Search");
            break;
        case kPeopleSearch:
            searchTypeString = tr("People Search");
            break;
        default:
            searchTypeString = tr("Unknown Search");
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

bool RecordingRule::IsValid(QString &msg) const
{
    bool isOverride = false;
    switch (m_type)
    {
    case kSingleRecord:
    case kDailyRecord:
    case kAllRecord:
    case kWeeklyRecord:
    case kOneRecord:
    case kTemplateRecord:
        break;
    case kOverrideRecord:
    case kDontRecord:
        isOverride = true;
        break;
    case kNotRecording:
    default:
        msg = QString("Invalid recording type.");
        return false;
    }

    bool isNormal = false;
    bool isSearch = false;
    bool isManual = false;
    switch (m_searchType)
    {
    case kNoSearch:
        isNormal = true;
        break;
    case kPowerSearch:
    case kTitleSearch:
    case kKeywordSearch:
    case kPeopleSearch:
        isSearch = true;
        break;
    case kManualSearch:
        isManual = true;
        break;
    default:
        msg = QString("Invalid search type.");
        return false;
    }

    if (!isOverride &&
        ((isNormal && (m_type == kDailyRecord || m_type == kWeeklyRecord)) ||
        (isSearch && (m_type != kDailyRecord && m_type != kWeeklyRecord &&
                      m_type != kOneRecord && m_type != kAllRecord)) ||
        (isManual && (m_type != kDailyRecord && m_type != kWeeklyRecord &&
                      m_type != kSingleRecord && m_type != kAllRecord))))
    {
        msg = QString("Invalid recording type/search type.");
        return false;
    }

    if ((m_parentRecID && !isOverride) ||
        (!m_parentRecID && isOverride))
    {
        msg = QString("Invalid parentID/override.");
        return false;
    }

    // Inactive overrides cause errors so are disallowed.
    if (isOverride && m_isInactive)
    {
        msg = QString("Invalid Inactive Override.");
        return false;
    }

    if (m_title.isEmpty())
    {
        msg = QString("Invalid title.");
        return false;
    }

    if (m_searchType == kPowerSearch)
    {
        if (m_description.contains(';') || m_subtitle.contains(';'))
        {
            msg = QString("Invalid SQL, contains semicolon");
            return false;
        }
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare(QString("SELECT NULL FROM (program, channel, oldrecorded AS oldrecstatus) "
                              "%1 WHERE %2 LIMIT 5")
                      .arg(m_subtitle, m_description));
        if (!query.exec())
        {
            msg = QString("Invalid SQL Where clause." + query.lastError().databaseText());
            return false;
        }
    }
    else if (isSearch)
    {
        if (m_description.isEmpty())
        {
            msg = QString("Invalid search value.");
            return false;
        }
    }

    if (m_type != kTemplateRecord && !isSearch)
    {
        if (!m_startdate.isValid() || !m_starttime.isValid() ||
            !m_enddate.isValid() || !m_endtime.isValid())
        {
            msg = QString("Invalid start/end date/time.");
            return false;
        }
#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
        qint64 secsto = QDateTime(m_startdate, m_starttime, Qt::UTC)
            .secsTo(QDateTime(m_enddate, m_endtime, Qt::UTC));
#else
        static const QTimeZone utc(QTimeZone::UTC);
        qint64 secsto = QDateTime(m_startdate, m_starttime, utc)
            .secsTo(QDateTime(m_enddate, m_endtime, utc));
#endif
        if (secsto <= 0 || secsto > (24 * 3600))
        {
            msg = QString("Invalid duration.");
            return false;
        }

        if (!m_channelid || m_station.isEmpty())
        {
            msg = QString("Invalid channel.");
            return false;
        }
    }

    if (m_type != kTemplateRecord
        && (m_findday < 0 || m_findday > 6 || !m_findtime.isValid()) )
    {
        msg = QString("Invalid find values.");
        return false;
    }

    if (m_recPriority < -99 || m_recPriority > 99)
    {
        msg = QString("Invalid priority.");
        return false;
    }

    if (m_startOffset < -480 || m_startOffset > 480 ||
        m_endOffset < -480 || m_endOffset > 480)
    {
        msg = QString("Invalid start/end offset.");
        return false;
    }

    if (m_prefInput < 0)
    {
        msg = QString("Invalid preferred input.");
        return false;
    }

    unsigned valid_mask = (1 << kNumFilters) - 1;
    if ((m_filter & valid_mask) != m_filter)
    {
        msg = QString("Invalid filter value.");
        return false;
    }

    if (m_recProfile.isEmpty() || (m_recGroupID == 0) ||
        m_storageGroup.isEmpty() || m_playGroup.isEmpty())
    {
        msg = QString("Invalid group value.");
        return false;
    }

    if (m_maxEpisodes < 0)
    {
        msg = QString("Invalid max episodes value.");
        return false;
    }

    if (m_dupIn & ~(kDupsInAll | kDupsNewEpi))
    {
        msg = QString("Invalid duplicate scope.");
        return false;
    }

    if (m_dupMethod & ~(kDupCheckNone | kDupCheckSub |
                        kDupCheckDesc | kDupCheckSubThenDesc))
    {
        msg = QString("Invalid duplicate method.");
        return false;
    }

    if (m_transcoder < 0)
    {
        msg = QString("Invalid transcoder value.");
        return false;
    }

    if (m_autoExtend >= AutoExtendType::Last)
    {
        msg = QString("Invalid auto extend value.");
        return false;
    }

    return true;
}
