// POSIX headers
#include <sys/types.h>
#include <unistd.h>

// C headers
#include <cstdlib>

// C++ headers
#include <iostream>
#include <algorithm>
using namespace std;

// Qt headers
#include <QRegExp>
#include <QMap>
#include <QLayout>
#include <QLabel>
#include <QApplication>
#include <QFile>
#include <QFileInfo>

// MythTV headers
#include "recordinginfo.h"
#include "scheduledrecording.h"
#include "util.h"
#include "mythcontext.h"
#include "dialogbox.h"
#include "remoteutil.h"
#include "tvremoteutil.h"
#include "jobqueue.h"
#include "mythdb.h"
#include "mythverbose.h"
#include "storagegroup.h"
#include "previewgenerator.h"
#include "channelutil.h"
#include "programlist.h"
#include "progdetails_temp.h"

#define LOC QString("RecordingInfo: ")
#define LOC_ERR QString("RecordingInfo, Error: ")

static bool insert_program(const RecordingInfo*,
                           const ScheduledRecording*);

/** \fn RecordingInfo::clone(const ProgramInfo &other)
 *  \brief Copies important fields from other RecordingInfo.
 */
RecordingInfo &RecordingInfo::clone(const ProgramInfo &other)
{
    if (record)
    {
        record->deleteLater();
        record = NULL;
    }

    ProgramInfo::clone(other);

    return *this;
}

void RecordingInfo::clear(void)
{
    ProgramInfo::clear();
    
    if (record)
    {
        record->deleteLater();
        record = NULL;
    }
}


/** \fn RecordingInfo::~RecordingInfo()
 *  \brief Destructor deletes "record" if it exists.
 */
RecordingInfo::~RecordingInfo()
{
    if (record)
    {
        record->deleteLater();
        record = NULL;
    }
}


/** \fn RecordingInfo::ToMap(InfoMap&, bool) const
 *  \brief Converts RecordingInfo into QString QHash containing each field
 *         in RecordingInfo converted into localized strings.
 */
void RecordingInfo::ToMap(InfoMap &progMap, bool showrerecord) const
{
    ProgramInfo::ToMap(progMap, showrerecord);

    QString iconpath = ChannelUtil::GetIcon(chanid.toUInt());
    if (!iconpath.isEmpty())
        progMap["iconpath"] = iconpath;
}

/** \fn RecordingInfo::IsFindApplicable(void) const
 *  \brief Returns true if a search should be employed to find a matching
 *         program.
 */
bool RecordingInfo::IsFindApplicable(void) const
{
    return rectype == kFindDailyRecord ||
           rectype == kFindWeeklyRecord;
}

/** \fn RecordingInfo::GetProgramRecordingStatus()
 *  \brief Returns the recording type for this RecordingInfo, creating
 *         "record" field if necessary.
 *  \sa RecordingType, ScheduledRecording
 */
RecordingType RecordingInfo::GetProgramRecordingStatus(void)
{
    if (record == NULL)
    {
        record = new ScheduledRecording();
        record->loadByProgram(this);
    }

    return record->getRecordingType();
}

/** \fn RecordingInfo::GetProgramRecordingProfile() const
 *  \brief Returns recording profile name that will be, or was used,
 *         for this program, creating "record" field if necessary.
 *  \sa ScheduledRecording
 */
QString RecordingInfo::GetProgramRecordingProfile(void) const
{
    if (record == NULL)
    {
        record = new ScheduledRecording();
        record->loadByProgram(this);
    }

    return record->getProfileName();
}

/** \fn RecordingInfo::GetAutoRunJobs()
 *  \brief Returns a bitmap of which jobs are attached to this RecordingInfo.
 *  \sa JobTypes, getProgramFlags()
 */
int RecordingInfo::GetAutoRunJobs(void) const
{
    if (record == NULL)
    {
        record = new ScheduledRecording();
        record->loadByProgram(this);
    }

    return record->GetAutoRunJobs();
}

/** \fn RecordingInfo::ApplyRecordRecID(void)
 *  \brief Sets recordid to match ScheduledRecording recordid
 */
void RecordingInfo::ApplyRecordRecID(void)
{
    MSqlQuery query(MSqlQuery::InitCon());

    if (getRecordID() < 0)
    {
        VERBOSE(VB_IMPORTANT,
                "ProgInfo Error: ApplyRecordRecID(void) needs recordid");
        return;
    }

    query.prepare("UPDATE recorded "
                  "SET recordid = :RECID "
                  "WHERE chanid = :CHANID AND starttime = :START");

    if (rectype == kOverrideRecord && parentid > 0)
        query.bindValue(":RECID", parentid);
    else
        query.bindValue(":RECID",  getRecordID());
    query.bindValue(":CHANID", chanid);
    query.bindValue(":START",  recstartts);

    if (!query.exec())
        MythDB::DBError(LOC + "RecordID update", query);
}

/**
 *  \brief Sets RecordingType of "record", creating "record" if it
 *         does not exist.
 *  \param newstate State to apply to "record" RecordingType.
 */
// newstate uses same values as return of GetProgramRecordingState
void RecordingInfo::ApplyRecordStateChange(RecordingType newstate, bool save)
{
    GetProgramRecordingStatus();
    if (newstate == kOverrideRecord || newstate == kDontRecord)
        record->makeOverride();
    record->setRecordingType(newstate);

    if (save)
        record->Save();
}

/** \fn RecordingInfo::ApplyRecordRecPriorityChange(int)
 *  \brief Sets recording priority of "record", creating "record" if it
 *         does not exist.
 *  \param newrecpriority New recording priority.
 */
void RecordingInfo::ApplyRecordRecPriorityChange(int newrecpriority)
{
    GetProgramRecordingStatus();
    record->setRecPriority(newrecpriority);
    record->Save();
}

/** \fn RecordingInfo::ApplyRecordRecGroupChange(const QString &newrecgroup)
 *  \brief Sets the recording group, both in this RecordingInfo
 *         and in the database.
 *  \param newrecgroup New recording group.
 */
void RecordingInfo::ApplyRecordRecGroupChange(const QString &newrecgroup)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("UPDATE recorded"
                  " SET recgroup = :RECGROUP"
                  " WHERE chanid = :CHANID"
                  " AND starttime = :START ;");
    query.bindValue(":RECGROUP", newrecgroup);
    query.bindValue(":START", recstartts);
    query.bindValue(":CHANID", chanid);

    if (!query.exec())
        MythDB::DBError("RecGroup update", query);

    recgroup = newrecgroup;
}

/** \fn RecordingInfo::ApplyRecordPlayGroupChange(const QString &newplaygroup)
 *  \brief Sets the recording group, both in this RecordingInfo
 *         and in the database.
 *  \param newplaygroup New recording group.
 */
void RecordingInfo::ApplyRecordPlayGroupChange(const QString &newplaygroup)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("UPDATE recorded"
                  " SET playgroup = :PLAYGROUP"
                  " WHERE chanid = :CHANID"
                  " AND starttime = :START ;");
    query.bindValue(":PLAYGROUP", newplaygroup);
    query.bindValue(":START", recstartts);
    query.bindValue(":CHANID", chanid);

    if (!query.exec())
        MythDB::DBError("PlayGroup update", query);

    playgroup = newplaygroup;
}

/** \fn RecordingInfo::ApplyStorageGroupChange(const QString &newstoragegroup)
 *  \brief Sets the storage group, both in this RecordingInfo
 *         and in the database.
 *  \param newstoragegroup New storage group.
 */
void RecordingInfo::ApplyStorageGroupChange(const QString &newstoragegroup)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("UPDATE recorded"
                  " SET storagegroup = :STORAGEGROUP"
                  " WHERE chanid = :CHANID"
                  " AND starttime = :START ;");
    query.bindValue(":STORAGEGROUP", newstoragegroup);
    query.bindValue(":START", recstartts);
    query.bindValue(":CHANID", chanid);

    if (!query.exec())
        MythDB::DBError("StorageGroup update", query);

    storagegroup = newstoragegroup;
}

/** \fn RecordingInfo::ApplyRecordRecTitleChange(const QString &newTitle, const QString &newSubtitle)
 *  \brief Sets the recording title and subtitle, both in this RecordingInfo
 *         and in the database.
 *  \param newTitle New recording title.
 *  \param newSubtitle New recording subtitle
 */
void RecordingInfo::ApplyRecordRecTitleChange(const QString &newTitle, const QString &newSubtitle)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("UPDATE recorded"
                  " SET title = :TITLE, subtitle = :SUBTITLE"
                  " WHERE chanid = :CHANID"
                  " AND starttime = :START ;");
    query.bindValue(":TITLE", newTitle);
    query.bindValue(":SUBTITLE", newSubtitle);
    query.bindValue(":CHANID", chanid);
    query.bindValue(":START", recstartts.toString("yyyyMMddhhmmss"));

    if (!query.exec())
        MythDB::DBError("RecTitle update", query);

    title = newTitle;
    subtitle = newSubtitle;
}

/** \brief Sets the transcoder profile for a recording
 *  \param profile Descriptive name of the profile. ie: Autodetect
 */
void RecordingInfo::ApplyTranscoderProfileChange(const QString &profile) const
{
    if (profile == "Default") // use whatever is already in the transcoder
        return;

    MSqlQuery query(MSqlQuery::InitCon());

    if (profile == "Autodetect")
    {
        query.prepare("UPDATE recorded "
                      "SET transcoder = 0 "
                      "WHERE chanid = :CHANID "
                      "AND starttime = :START");
        query.bindValue(":CHANID",  chanid);
        query.bindValue(":START",  recstartts);

        if (!query.exec())
            MythDB::DBError(LOC + "unable to update transcoder "
                                  "in recorded table", query);
    }
    else
    {
        MSqlQuery pidquery(MSqlQuery::InitCon());
        pidquery.prepare("SELECT r.id "
                         "FROM recordingprofiles r, profilegroups p "
                         "WHERE r.profilegroup = p.id "
                             "AND p.name = 'Transcoders' "
                             "AND r.name = :PROFILE ");
        pidquery.bindValue(":PROFILE",  profile);

        if (!pidquery.exec())
        {
            MythDB::DBError("ProgramInfo: unable to query transcoder "
                            "profile ID", query);
        }
        else if (pidquery.next())
        {
            query.prepare("UPDATE recorded "
                          "SET transcoder = :TRANSCODER "
                          "WHERE chanid = :CHANID "
                          "AND starttime = :START");
            query.bindValue(":TRANSCODER", pidquery.value(0).toInt());
            query.bindValue(":CHANID",  chanid);
            query.bindValue(":START",  recstartts);

            if (!query.exec())
                MythDB::DBError(LOC + "unable to update transcoder "
                                     "in recorded table", query);
        }
        else
        {
            VERBOSE(VB_IMPORTANT, "ProgramInfo: unable to query transcoder "
                    "profile ID");
        }
    }
}

/** \fn RecordingInfo::ToggleRecord(void)
 *  \brief Cycles through recording types.
 *
 *   If the program recording status is kNotRecording,
 *   ApplyRecordStateChange(kSingleRecord) is called.
 *   If the program recording status is kSingleRecording,
 *   ApplyRecordStateChange(kFindOneRecord) is called.
 *   <br>etc...
 *
 *   The states in order are: kNotRecording, kSingleRecord, kFindOneRecord,
 *     kWeekslotRecord, kFindWeeklyRecord, kTimeslotRecord, kFindDailyRecord,
 *     kChannelRecord, kAllRecord.<br>
 *   And: kOverrideRecord, kDontRecord.
 *
 *   That is if you the recording is in any of the first set of states,
 *   we cycle through those, if not we toggle between kOverrideRecord and
 *   kDontRecord.
 */
void RecordingInfo::ToggleRecord(void)
{
    RecordingType curType = GetProgramRecordingStatus();

    switch (curType)
    {
        case kNotRecording:
            ApplyRecordStateChange(kSingleRecord);
            break;
        case kSingleRecord:
            ApplyRecordStateChange(kFindOneRecord);
            break;
        case kFindOneRecord:
            ApplyRecordStateChange(kAllRecord);
            break;
        case kAllRecord:
            ApplyRecordStateChange(kSingleRecord);
            break;

        case kOverrideRecord:
            ApplyRecordStateChange(kDontRecord);
            break;
        case kDontRecord:
            ApplyRecordStateChange(kOverrideRecord);
            break;

        default:
            ApplyRecordStateChange(kAllRecord);
            break;
/*
        case kNotRecording:
            ApplyRecordStateChange(kSingleRecord);
            break;
        case kSingleRecord:
            ApplyRecordStateChange(kFindOneRecord);
            break;
        case kFindOneRecord:
            ApplyRecordStateChange(kWeekslotRecord);
            break;
        case kWeekslotRecord:
            ApplyRecordStateChange(kFindWeeklyRecord);
            break;
        case kFindWeeklyRecord:
            ApplyRecordStateChange(kTimeslotRecord);
            break;
        case kTimeslotRecord:
            ApplyRecordStateChange(kFindDailyRecord);
            break;
        case kFindDailyRecord:
            ApplyRecordStateChange(kChannelRecord);
            break;
        case kChannelRecord:
            ApplyRecordStateChange(kAllRecord);
            break;
        case kAllRecord:
        default:
            ApplyRecordStateChange(kNotRecording);
            break;
        case kOverrideRecord:
            ApplyRecordStateChange(kDontRecord);
            break;
        case kDontRecord:
            ApplyRecordStateChange(kOverrideRecord);
            break;
*/
    }
}

/** \fn RecordingInfo::GetScheduledRecording(void)
 *  \brief Returns the "record" field, creating it if necessary.
 */
ScheduledRecording* RecordingInfo::GetScheduledRecording(void)
{
    GetProgramRecordingStatus();
    return record;
}

/** \fn RecordingInfo::getRecordID(void)
 *  \brief Returns a record id, creating "record" it if necessary.
 */
int RecordingInfo::getRecordID(void)
{
    GetProgramRecordingStatus();
    recordid = record->getRecordID();
    return recordid;
}

/**
 *  \brief Inserts this RecordingInfo into the database as an existing recording.
 *
 *  This method, of course, only works if a recording has been scheduled
 *  and started.
 *
 *  \param ext    File extension for recording
 */
void RecordingInfo::StartedRecording(QString ext)
{
    QString dirname = pathname;

    if (!record)
    {
        record = new ScheduledRecording();
        record->loadByProgram(this);
    }

    hostname = gContext->GetHostName();
    pathname = CreateRecordBasename(ext);

    int count = 0;
    while (!insert_program(this, record) && count < 50)
    {
        recstartts = recstartts.addSecs(1);
        pathname = CreateRecordBasename(ext);
        count++;
    }

    if (count >= 50)
    {
        VERBOSE(VB_IMPORTANT, "Couldn't insert program");
        return;
    }

    pathname = dirname + "/" + pathname;

    VERBOSE(VB_FILE, QString(LOC + "StartedRecording: Recording to '%1'")
                             .arg(pathname));


    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("DELETE FROM recordedseek WHERE chanid = :CHANID"
                  " AND starttime = :START;");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":START", recstartts);

    if (!query.exec() || !query.isActive())
        MythDB::DBError("Clear seek info on record", query);

    query.prepare("DELETE FROM recordedmarkup WHERE chanid = :CHANID"
                  " AND starttime = :START;");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":START", recstartts);

    if (!query.exec() || !query.isActive())
        MythDB::DBError("Clear markup on record", query);

    query.prepare("REPLACE INTO recordedcredits"
                 " SELECT * FROM credits"
                 " WHERE chanid = :CHANID AND starttime = :START;");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":START", startts);
    if (!query.exec() || !query.isActive())
        MythDB::DBError("Copy program credits on record", query);

    query.prepare("REPLACE INTO recordedprogram"
                 " SELECT * from program"
                 " WHERE chanid = :CHANID AND starttime = :START"
                 " AND title = :TITLE;");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":START", startts);
    query.bindValue(":TITLE", title);
    if (!query.exec() || !query.isActive())
        MythDB::DBError("Copy program data on record", query);

    query.prepare("REPLACE INTO recordedrating"
                 " SELECT * from programrating"
                 " WHERE chanid = :CHANID AND starttime = :START;");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":START", startts);
    if (!query.exec() || !query.isActive())
        MythDB::DBError("Copy program ratings on record", query);
}

static bool insert_program(const RecordingInfo        *pg,
                           const ScheduledRecording *schd)
{
    MSqlQuery query(MSqlQuery::InitCon());

    //query.prepare("LOCK TABLES recorded WRITE");
    if (!query.exec("LOCK TABLES recorded WRITE"))
    {
        MythDB::DBError("insert_program -- lock", query);
        return false;
    }

    query.prepare(
        "SELECT recordid "
        "    FROM recorded "
        "    WHERE chanid    = :CHANID AND "
        "          starttime = :STARTS");
    query.bindValue(":CHANID", pg->chanid);
    query.bindValue(":STARTS", pg->recstartts);

    if (!query.exec() || query.size())
    {
        if (!query.isActive())
            MythDB::DBError("insert_program -- select", query);
        else
            VERBOSE(VB_IMPORTANT, "recording already exists...");

        if (!query.exec("UNLOCK TABLES"))
            MythDB::DBError("insert_program -- unlock tables", query);
        return false;
    }

    query.prepare(
        "INSERT INTO recorded "
        "   (chanid,    starttime,   endtime,         title,            "
        "    subtitle,  description, hostname,        category,         "
        "    recgroup,  autoexpire,  recordid,        seriesid,         "
        "    programid, stars,       previouslyshown, originalairdate,  "
        "    findid,    transcoder,  playgroup,       recpriority,      "
        "    basename,  progstart,   progend,         profile,          "
        "    duplicate, storagegroup) "
        "VALUES"
        "  (:CHANID,   :STARTS,     :ENDS,           :TITLE,            "
        "   :SUBTITLE, :DESC,       :HOSTNAME,       :CATEGORY,         "
        "   :RECGROUP, :AUTOEXP,    :RECORDID,       :SERIESID,         "
        "   :PROGRAMID,:STARS,      :REPEAT,         :ORIGAIRDATE,      "
        "   :FINDID,   :TRANSCODER, :PLAYGROUP,      :RECPRIORITY,      "
        "   :BASENAME, :PROGSTART,  :PROGEND,        :PROFILE,          "
        "   0,         :STORGROUP) "
        );

    if (pg->rectype == kOverrideRecord)
        query.bindValue(":RECORDID",    pg->parentid);
    else
        query.bindValue(":RECORDID",    pg->recordid);

    if (pg->hasAirDate)
        query.bindValue(":ORIGAIRDATE", pg->originalAirDate);
    else
        query.bindValue(":ORIGAIRDATE", "0000-00-00");

    query.bindValue(":CHANID",      pg->chanid);
    query.bindValue(":STARTS",      pg->recstartts);
    query.bindValue(":ENDS",        pg->recendts);
    query.bindValue(":TITLE",       pg->title);
    query.bindValue(":SUBTITLE",    pg->subtitle);
    query.bindValue(":DESC",        pg->description);
    query.bindValue(":HOSTNAME",    pg->hostname);
    query.bindValue(":CATEGORY",    pg->category);
    query.bindValue(":RECGROUP",    pg->recgroup);
    query.bindValue(":AUTOEXP",     schd->GetAutoExpire());
    query.bindValue(":SERIESID",    pg->seriesid);
    query.bindValue(":PROGRAMID",   pg->programid);
    query.bindValue(":FINDID",      pg->findid);
    query.bindValue(":STARS",       pg->stars);
    query.bindValue(":REPEAT",      pg->repeat);
    query.bindValue(":TRANSCODER",  schd->GetTranscoder());
    query.bindValue(":PLAYGROUP",   pg->playgroup);
    query.bindValue(":RECPRIORITY", schd->getRecPriority());
    query.bindValue(":BASENAME",    pg->pathname);
    query.bindValue(":STORGROUP",   pg->storagegroup);
    query.bindValue(":PROGSTART",   pg->startts);
    query.bindValue(":PROGEND",     pg->endts);
    query.bindValue(":PROFILE",     schd->getProfileName());

    bool ok = query.exec() && (query.numRowsAffected() > 0);
    bool active = query.isActive();

    if (!query.exec("UNLOCK TABLES"))
        MythDB::DBError("insert_program -- unlock tables", query);

    if (!ok && !active)
        MythDB::DBError("insert_program -- insert", query);

    else if (pg->recordid > 0)
    {
        query.prepare("UPDATE channel SET last_record = NOW() "
                      "WHERE chanid = :CHANID");
        query.bindValue(":CHANID", pg->chanid);
        if (!query.exec())
            MythDB::DBError("insert_program -- channel last_record", query);

        query.prepare("UPDATE record SET last_record = NOW() "
                      "WHERE recordid = :RECORDID");
        query.bindValue(":RECORDID", pg->recordid);
        if (!query.exec())
            MythDB::DBError("insert_program -- record last_record", query);

        if (pg->rectype == kOverrideRecord && pg->parentid > 0)
        {
            query.prepare("UPDATE record SET last_record = NOW() "
                          "WHERE recordid = :PARENTID");
            query.bindValue(":PARENTID", pg->parentid);
            if (!query.exec())
                MythDB::DBError("insert_program -- record last_record override",
                                query);
        }
    }

    return ok;
}

/** \fn RecordingInfo::FinishedRecording(bool prematurestop)
 *  \brief If not a premature stop, adds program to history of recorded
 *         programs.
 *  \param prematurestop If true, we only fetch the recording status.
 */
void RecordingInfo::FinishedRecording(bool prematurestop)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("UPDATE recorded SET endtime = :ENDTIME, "
                  "       duplicate = 1 "
                  "WHERE chanid = :CHANID AND "
                  "    starttime = :STARTTIME ");
    query.bindValue(":ENDTIME", recendts);
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);

    if (!query.exec())
        MythDB::DBError("FinishedRecording update", query);

    GetProgramRecordingStatus();
    if (!prematurestop)
        record->doneRecording(*this);
}

/** \fn RecordingInfo::UpdateRecordingEnd(void)
 *  \brief Update information in the recorded table when the end-time
 *  of a recording is changed.
 */
void RecordingInfo::UpdateRecordingEnd(void)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("UPDATE recorded SET endtime = :ENDTIME "
                  "WHERE chanid = :CHANID AND "
                  "    starttime = :STARTTIME ");
    query.bindValue(":ENDTIME", recendts);

    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);

    if (!query.exec())
        MythDB::DBError("UpdateRecordingEnd update", query);
}

/** \fn RecordingInfo::ReactivateRecording(void)
 *  \brief Asks the scheduler to restart this recording if possible.
 */
void RecordingInfo::ReactivateRecording(void)
{
    MSqlQuery result(MSqlQuery::InitCon());

    result.prepare("UPDATE oldrecorded SET reactivate = 1 "
                   "WHERE station = :STATION AND "
                   "  starttime = :STARTTIME AND "
                   "  title = :TITLE;");
    result.bindValue(":STARTTIME", startts);
    result.bindValue(":TITLE", title);
    result.bindValue(":STATION", chansign);

    if (!result.exec())
        MythDB::DBError("ReactivateRecording", result);

    ScheduledRecording::signalChange(0);
}

/**
 *  \brief Adds recording history, creating "record" it if necessary.
 */
void RecordingInfo::AddHistory(bool resched, bool forcedup)
{
    bool dup = (recstatus == rsRecorded || forcedup);
    RecStatusType rs = (recstatus == rsCurrentRecording) ?
        rsPreviousRecording : recstatus;
    oldrecstatus = recstatus;
    if (dup)
        reactivate = false;

    MSqlQuery result(MSqlQuery::InitCon());

    result.prepare("REPLACE INTO oldrecorded (chanid,starttime,"
                   "endtime,title,subtitle,description,category,"
                   "seriesid,programid,findid,recordid,station,"
                   "rectype,recstatus,duplicate,reactivate) "
                   "VALUES(:CHANID,:START,:END,:TITLE,:SUBTITLE,:DESC,"
                   ":CATEGORY,:SERIESID,:PROGRAMID,:FINDID,:RECORDID,"
                   ":STATION,:RECTYPE,:RECSTATUS,:DUPLICATE,:REACTIVATE);");
    result.bindValue(":CHANID", chanid);
    result.bindValue(":START", startts.toString(Qt::ISODate));
    result.bindValue(":END", endts.toString(Qt::ISODate));
    result.bindValue(":TITLE", title);
    result.bindValue(":SUBTITLE", subtitle);
    result.bindValue(":DESC", description);
    result.bindValue(":CATEGORY", category);
    result.bindValue(":SERIESID", seriesid);
    result.bindValue(":PROGRAMID", programid);
    result.bindValue(":FINDID", findid);
    result.bindValue(":RECORDID", recordid);
    result.bindValue(":STATION", chansign);
    result.bindValue(":RECTYPE", rectype);
    result.bindValue(":RECSTATUS", rs);
    result.bindValue(":DUPLICATE", dup);
    result.bindValue(":REACTIVATE", reactivate);

    if (!result.exec())
        MythDB::DBError("addHistory", result);

    if (dup && findid)
    {
        result.prepare("REPLACE INTO oldfind (recordid, findid) "
                       "VALUES(:RECORDID,:FINDID);");
        result.bindValue(":RECORDID", recordid);
        result.bindValue(":FINDID", findid);

        if (!result.exec())
            MythDB::DBError("addFindHistory", result);
    }

    // The adding of an entry to oldrecorded may affect near-future
    // scheduling decisions, so recalculate if told
    if (resched)
        ScheduledRecording::signalChange(0);
}

/** \fn RecordingInfo::DeleteHistory(void)
 *  \brief Deletes recording history, creating "record" it if necessary.
 */
void RecordingInfo::DeleteHistory(void)
{
    MSqlQuery result(MSqlQuery::InitCon());

    result.prepare("DELETE FROM oldrecorded WHERE title = :TITLE AND "
                   "starttime = :START AND station = :STATION");
    result.bindValue(":TITLE", title);
    result.bindValue(":START", recstartts);
    result.bindValue(":STATION", chansign);

    if (!result.exec())
        MythDB::DBError("deleteHistory", result);

    if (/*duplicate &&*/ findid)
    {
        result.prepare("DELETE FROM oldfind WHERE "
                       "recordid = :RECORDID AND findid = :FINDID");
        result.bindValue(":RECORDID", recordid);
        result.bindValue(":FINDID", findid);

        if (!result.exec())
            MythDB::DBError("deleteFindHistory", result);
    }

    // The removal of an entry from oldrecorded may affect near-future
    // scheduling decisions, so recalculate
    ScheduledRecording::signalChange(0);
}

/** \fn RecordingInfo::ForgetHistory(void)
 *  \brief Forget the recording of a program so it will be recorded again.
 *
 * The duplicate flags in both the recorded and old recorded tables are set
 * to 0. This causes these records to be skipped in the left join in the BUSQ
 * In addition, any "Never Record" fake entries are removed from the oldrecorded
 * table and any entries in the oldfind table are removed.
 */
void RecordingInfo::ForgetHistory(void)
{
    MSqlQuery result(MSqlQuery::InitCon());

    result.prepare("UPDATE recorded SET duplicate = 0 "
                   "WHERE chanid = :CHANID "
                       "AND starttime = :STARTTIME "
                       "AND title = :TITLE;");
    result.bindValue(":STARTTIME", recstartts);
    result.bindValue(":TITLE", title);
    result.bindValue(":CHANID", chanid);

    if (!result.exec())
        MythDB::DBError("forgetRecorded", result);

    result.prepare("UPDATE oldrecorded SET duplicate = 0 "
                   "WHERE duplicate = 1 "
                   "AND title = :TITLE AND "
                   "((programid = '' AND subtitle = :SUBTITLE"
                   "  AND description = :DESC) OR "
                   " (programid <> '' AND programid = :PROGRAMID) OR "
                   " (findid <> 0 AND findid = :FINDID))");
    result.bindValue(":TITLE", title);
    result.bindValue(":SUBTITLE", subtitle);
    result.bindValue(":DESC", description);
    result.bindValue(":PROGRAMID", programid);
    result.bindValue(":FINDID", findid);

    if (!result.exec())
        MythDB::DBError("forgetHistory", result);

    result.prepare("DELETE FROM oldrecorded "
                   "WHERE recstatus = :NEVER AND duplicate = 0");
    result.bindValue(":NEVER", rsNeverRecord);

    if (!result.exec())
        MythDB::DBError("forgetNeverHisttory", result);

    if (findid)
    {
        result.prepare("DELETE FROM oldfind WHERE "
                       "recordid = :RECORDID AND findid = :FINDID");
        result.bindValue(":RECORDID", recordid);
        result.bindValue(":FINDID", findid);

        if (!result.exec())
            MythDB::DBError("forgetFindHistory", result);
    }

    // The removal of an entry from oldrecorded may affect near-future
    // scheduling decisions, so recalculate
    ScheduledRecording::signalChange(0);
}

/** \fn RecordingInfo::SetDupHistory(void)
 *  \brief Set the duplicate flag in oldrecorded.
 */
void RecordingInfo::SetDupHistory(void)
{
    MSqlQuery result(MSqlQuery::InitCon());

    result.prepare("UPDATE oldrecorded SET duplicate = 1 "
                   "WHERE duplicate = 0 "
                   "AND title = :TITLE AND "
                   "((programid = '' AND subtitle = :SUBTITLE"
                   "  AND description = :DESC) OR "
                   " (programid <> '' AND programid = :PROGRAMID) OR "
                   " (findid <> 0 AND findid = :FINDID))");
    result.bindValue(":TITLE", title);
    result.bindValue(":SUBTITLE", subtitle);
    result.bindValue(":DESC", description);
    result.bindValue(":PROGRAMID", programid);
    result.bindValue(":FINDID", findid);

    if (!result.exec())
        MythDB::DBError("setDupHistory", result);

    ScheduledRecording::signalChange(0);
}

/** \fn RecordingInfo::EditRecording(void)
 *  \brief Creates a dialog for editing the recording status,
 *         blocking until user leaves dialog.
 */
void RecordingInfo::EditRecording(void)
{
    if (recordid == 0)
        EditScheduled();
    else if (recstatus <= rsWillRecord)
        ShowRecordingDialog();
    else
        ShowNotRecordingDialog();
}

/** \fn RecordingInfo::EditScheduled(void)
 *  \brief Creates a dialog for editing the recording status,
 *         blocking until user leaves dialog.
 */
void RecordingInfo::EditScheduled(void)
{
    GetProgramRecordingStatus();
    record->exec();
}

/** \fn RecordingInfo::showDetails(void) const
 *  \brief Pops up a DialogBox with program info.
 */
void RecordingInfo::showDetails(void) const
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ProgDetailsTemp *details_dialog  = new ProgDetailsTemp(mainStack, this);

    if (!details_dialog->Create())
    {
        delete details_dialog;
        return;
    }

    mainStack->AddScreen(details_dialog);

    // HACK begin - remove when everything is using mythui
    if (GetMythMainWindow()->currentWidget())
    {
        QWidget *widget = GetMythMainWindow()->currentWidget();
        vector<QWidget *> widgetList;

        while (widget)
        {
            widgetList.push_back(widget);
            GetMythMainWindow()->detach(widget);
            widget = GetMythMainWindow()->currentWidget();
        }

        GetMythMainWindow()->GetPaintWindow()->raise();
        GetMythMainWindow()->GetPaintWindow()->setFocus();

        int screenCount = mainStack->TotalScreens();
        do
        {
            qApp->processEvents();
            usleep(5000);
        } while (mainStack->TotalScreens() >= screenCount);

        vector<QWidget*>::reverse_iterator it;
        for (it = widgetList.rbegin(); it != widgetList.rend(); ++it)
        {
            GetMythMainWindow()->attach(*it);
        }
    }
    // HACK end
}

void RecordingInfo::ShowRecordingDialog(void)
{
    QDateTime now = QDateTime::currentDateTime();

    QString message = title;

    if (subtitle != "")
        message += QString(" - \"%1\"").arg(subtitle);

    message += "\n\n";
    message += RecStatusDesc();

    DialogBox *dlg = new DialogBox(gContext->GetMainWindow(), message);
    int button = 0, ok = -1, react = -1, stop = -1, addov = -1, forget = -1,
        clearov = -1, edend = -1, ednorm = -1, edcust = -1;

    dlg->AddButton(QObject::tr("OK"));
    ok = button++;

    if (recstartts < now && recendts > now)
    {
        if (recstatus != rsRecording)
        {
            dlg->AddButton(QObject::tr("Reactivate"));
            react = button++;
        }
        else
        {
            dlg->AddButton(QObject::tr("Stop recording"));
            stop = button++;
        }
    }
    if (recendts > now)
    {
        if (rectype != kSingleRecord && rectype != kOverrideRecord)
        {
            if (recstartts > now)
            {
                dlg->AddButton(QObject::tr("Don't record"));
                addov = button++;
            }
            if (recstatus != rsRecording && rectype != kFindOneRecord &&
                !((findid == 0 || !IsFindApplicable()) &&
                  catType == "series" &&
                  programid.contains(QRegExp("0000$"))) &&
                ((!(dupmethod & kDupCheckNone) && !programid.isEmpty() &&
                  (findid != 0 || !IsFindApplicable())) ||
                 ((dupmethod & kDupCheckSub) && !subtitle.isEmpty()) ||
                 ((dupmethod & kDupCheckDesc) && !description.isEmpty()) ||
                 ((dupmethod & kDupCheckSubThenDesc) && (!subtitle.isEmpty() || !description.isEmpty())) ))
            {
                dlg->AddButton(QObject::tr("Never record"));
                forget = button++;
            }
        }

        if (rectype != kOverrideRecord && rectype != kDontRecord)
        {
            if (recstatus == rsRecording)
            {
                dlg->AddButton(QObject::tr("Change Ending Time"));
                edend = button++;
            }
            else
            {
                dlg->AddButton(QObject::tr("Edit Options"));
                ednorm = button++;

                if (rectype != kSingleRecord && rectype != kFindOneRecord)
                {
                    dlg->AddButton(QObject::tr("Add Override"));
                    edcust = button++;
                }
            }
        }

        if (rectype == kOverrideRecord || rectype == kDontRecord)
        {
            if (recstatus == rsRecording)
            {
                dlg->AddButton(QObject::tr("Change Ending Time"));
                edend = button++;
            }
            else
            {
                dlg->AddButton(QObject::tr("Edit Override"));
                ednorm = button++;
                dlg->AddButton(QObject::tr("Clear Override"));
                clearov = button++;
            }
        }
    }

    DialogCode code = dlg->exec();
    int ret = MythDialog::CalcItemIndex(code);
    dlg->deleteLater();
    dlg = NULL;

    if (ret == react)
        ReactivateRecording();
    else if (ret == stop)
    {
        ProgramInfo *p = GetProgramFromRecorded(chanid, recstartts);
        if (p)
        {
            RemoteStopRecording(p);
            delete p;
        }
    }
    else if (ret == addov)
        ApplyRecordStateChange(kDontRecord);
    else if (ret == forget)
    {
        recstatus = rsNeverRecord;
        startts = QDateTime::currentDateTime();
        endts = recstartts;
        AddHistory(true, true);
    }
    else if (ret == clearov)
        ApplyRecordStateChange(kNotRecording);
    else if (ret == edend)
    {
        GetProgramRecordingStatus();
        if (rectype != kSingleRecord && rectype != kOverrideRecord &&
            rectype != kFindOneRecord)
        {
            record->makeOverride();
            record->setRecordingType(kOverrideRecord);
        }
        record->exec();
    }
    else if (ret == ednorm)
    {
        GetProgramRecordingStatus();
        record->exec();
    }
    else if (ret == edcust)
    {
        GetProgramRecordingStatus();
        record->makeOverride();
        record->exec();
    }

    return;
}

void RecordingInfo::ShowNotRecordingDialog(void)
{
    QString timeFormat = gContext->GetSetting("TimeFormat", "h:mm AP");

    QString message = title;

    if (subtitle != "")
        message += QString(" - \"%1\"").arg(subtitle);

    message += "\n\n";
    message += RecStatusDesc();

    if (recstatus == rsConflict || recstatus == rsLaterShowing)
    {
        vector<ProgramInfo *> *confList = RemoteGetConflictList(this);

        if (confList->size())
            message += QObject::tr(" The following programs will be recorded "
                                   "instead:\n");

        for (int maxi = 0; confList->begin() != confList->end() &&
             maxi < 4; maxi++)
        {
            ProgramInfo *p = *confList->begin();
            message += QString("%1 - %2  %3")
                .arg(p->recstartts.toString(timeFormat))
                .arg(p->recendts.toString(timeFormat)).arg(p->title);
            if (p->subtitle != "")
                message += QString(" - \"%1\"").arg(p->subtitle);
            message += "\n";
            delete p;
            confList->erase(confList->begin());
        }
        message += "\n";
        while (!confList->empty())
        {
            delete confList->back();
            confList->pop_back();
        }
        delete confList;
    }

    DialogBox *dlg = new DialogBox(gContext->GetMainWindow(), message);
    int button = 0, ok = -1, react = -1, addov = -1, clearov = -1,
        ednorm = -1, edcust = -1, forget = -1, addov1 = -1, forget1 = -1;

    dlg->AddButton(QObject::tr("OK"));
    ok = button++;

    QDateTime now = QDateTime::currentDateTime();

    if ((recstartts < now) && (recendts > now) &&
        (recstatus != rsDontRecord) && (recstatus != rsNotListed))
    {
        dlg->AddButton(QObject::tr("Reactivate"));
        react = button++;
    }

    if (recendts > now)
    {
        if ((rectype != kSingleRecord &&
             rectype != kOverrideRecord) &&
            (recstatus == rsDontRecord ||
             recstatus == rsPreviousRecording ||
             recstatus == rsCurrentRecording ||
             recstatus == rsEarlierShowing ||
             recstatus == rsOtherShowing ||
             recstatus == rsNeverRecord ||
             recstatus == rsRepeat ||
             recstatus == rsInactive ||
             recstatus == rsLaterShowing))
        {
            dlg->AddButton(QObject::tr("Record anyway"));
            addov = button++;
            if (recstatus == rsPreviousRecording || recstatus == rsNeverRecord)
            {
                dlg->AddButton(QObject::tr("Forget Previous"));
                forget = button++;
            }
        }

        if (rectype != kOverrideRecord && rectype != kDontRecord)
        {
            if (rectype != kSingleRecord &&
                recstatus != rsPreviousRecording &&
                recstatus != rsCurrentRecording &&
                recstatus != rsNeverRecord &&
                recstatus != rsNotListed)
            {
                if (recstartts > now)
                {
                    dlg->AddButton(QObject::tr("Don't record"));
                    addov1 = button++;
                }
                if (rectype != kFindOneRecord &&
                    !((findid == 0 || !IsFindApplicable()) &&
                      catType == "series" &&
                      programid.contains(QRegExp("0000$"))) &&
                    ((!(dupmethod & kDupCheckNone) && !programid.isEmpty() &&
                      (findid != 0 || !IsFindApplicable())) ||
                     ((dupmethod & kDupCheckSub) && !subtitle.isEmpty()) ||
                     ((dupmethod & kDupCheckDesc) && !description.isEmpty())))
                {
                    dlg->AddButton(QObject::tr("Never record"));
                    forget1 = button++;
                }
            }

            dlg->AddButton(QObject::tr("Edit Options"));
            ednorm = button++;

            if (rectype != kSingleRecord && rectype != kFindOneRecord &&
                recstatus != rsNotListed)
            {
                dlg->AddButton(QObject::tr("Add Override"));
                edcust = button++;
            }
        }

        if (rectype == kOverrideRecord || rectype == kDontRecord)
        {
            dlg->AddButton(QObject::tr("Edit Override"));
            ednorm = button++;

            dlg->AddButton(QObject::tr("Clear Override"));
            clearov = button++;
        }
    }

    DialogCode code = dlg->exec();
    int ret = MythDialog::CalcItemIndex(code);
    dlg->deleteLater();
    dlg = NULL;

    if (ret == react)
        ReactivateRecording();
    else if (ret == addov)
    {
        ApplyRecordStateChange(kOverrideRecord);
        if (recstartts < now)
            ReactivateRecording();
    }
    else if (ret == forget)
        ForgetHistory();
    else if (ret == addov1)
        ApplyRecordStateChange(kDontRecord);
    else if (ret == forget1)
    {
        recstatus = rsNeverRecord;
        startts = QDateTime::currentDateTime();
        endts = recstartts;
        AddHistory(true, true);
    }
    else if (ret == clearov)
        ApplyRecordStateChange(kNotRecording);
    else if (ret == ednorm)
    {
        GetProgramRecordingStatus();
        record->exec();
    }
    else if (ret == edcust)
    {
        GetProgramRecordingStatus();
        record->makeOverride();
        record->exec();
    }

    return;
}

void RecordingInfo::makeOverride()
{
    GetProgramRecordingStatus();
    record->makeOverride();
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
