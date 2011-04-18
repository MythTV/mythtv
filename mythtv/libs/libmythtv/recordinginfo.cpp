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
#include <QFileInfo>
#include <QRegExp>
#include <QFile>
#include <QMap>

// MythTV headers
#include "recordinginfo.h"
#include "recordingrule.h"
#include "scheduledrecording.h"
#include "util.h"
#include "mythcorecontext.h"
#include "dialogbox.h"
#include "remoteutil.h"
#include "tvremoteutil.h"
#include "jobqueue.h"
#include "mythdb.h"
#include "mythverbose.h"
#include "previewgenerator.h"
#include "channelutil.h"

#define LOC      QString("RecordingInfo(%1): ").arg(GetBasename())
#define LOC_WARN QString("RecordingInfo(%1), Warning: ").arg(GetBasename())
#define LOC_ERR  QString("RecordingInfo(%1), Error: ").arg(GetBasename())

QString RecordingInfo::unknownTitle;
// works only for integer divisors of 60
static const uint kUnknownProgramLength = 30;

RecordingInfo::RecordingInfo(
    const QString &_title,
    const QString &_subtitle,
    const QString &_description,
    const QString &_category,

    uint _chanid,
    const QString &_chanstr,
    const QString &_chansign,
    const QString &_channame,

    const QString &_recgroup,
    const QString &_playgroup,

    const QString &_hostname,
    const QString &_storagegroup,

    uint _year,

    const QString &_seriesid,
    const QString &_programid,
    const QString &_catType,

    int _recpriority,

    const QDateTime &_startts,
    const QDateTime &_endts,
    const QDateTime &_recstartts,
    const QDateTime &_recendts,

    float _stars,
    const QDate &_originalAirDate,

    bool _repeat,

    RecStatusType _oldrecstatus,
    bool _reactivate,

    uint _recordid,
    uint _parentid,
    RecordingType _rectype,
    RecordingDupInType _dupin,
    RecordingDupMethodType _dupmethod,

    uint _sourceid,
    uint _inputid,
    uint _cardid,

    uint _findid,

    bool _commfree,
    uint _subtitleType,
    uint _videoproperties,
    uint _audioproperties) :
    ProgramInfo(
        _title, _subtitle, _description, _category,
        _chanid, _chanstr, _chansign, _channame, QString(),
        _recgroup, _playgroup,
        _startts, _endts, _recstartts, _recendts,
        _seriesid, _programid),
    oldrecstatus(rsUnknown),
    savedrecstatus(rsUnknown),
    record(NULL)
{
    hostname = _hostname;
    storagegroup = _storagegroup;

    year = _year;

    catType = _catType;

    recpriority = _recpriority;

    stars = clamp(_stars, 0.0f, 1.0f);
    originalAirDate = _originalAirDate;
    if (originalAirDate.isValid() && originalAirDate < QDate(1940, 1, 1))
        originalAirDate = QDate();

    programflags &= ~FL_REPEAT;
    programflags |= _repeat ? FL_REPEAT : 0;
    programflags &= ~FL_REACTIVATE;
    programflags |= _reactivate ? FL_REACTIVATE : 0;
    programflags &= ~FL_CHANCOMMFREE;
    programflags |= _commfree ? FL_CHANCOMMFREE : 0;

    oldrecstatus = _oldrecstatus;

    recstatus = (oldrecstatus == rsAborted ||
                 oldrecstatus == rsNotListed ||
                 _reactivate) ? rsUnknown : oldrecstatus;

    recordid = _recordid;
    parentid = _parentid;
    rectype = _rectype;
    dupin = _dupin;
    dupmethod = _dupmethod;

    sourceid = _sourceid;
    inputid = _inputid;
    cardid = _cardid;

    findid = _findid;

    properties = ((_subtitleType    << 11) |
                  (_videoproperties << 6)  |
                  _audioproperties);

    if (recstartts >= recendts)
    {
        // start/end-offsets are invalid so ignore
        recstartts = startts;
        recendts   = endts;
    }
}

RecordingInfo::RecordingInfo(
    const QString &_title,
    const QString &_subtitle,
    const QString &_description,
    const QString &_category,

    uint _chanid,
    const QString &_chanstr,
    const QString &_chansign,
    const QString &_channame,

    const QString &_recgroup,
    const QString &_playgroup,

    const QString &_seriesid,
    const QString &_programid,

    int _recpriority,

    const QDateTime &_startts,
    const QDateTime &_endts,
    const QDateTime &_recstartts,
    const QDateTime &_recendts,

    RecStatusType _recstatus,

    uint _recordid,
    RecordingType _rectype,
    RecordingDupInType _dupin,
    RecordingDupMethodType _dupmethod,

    uint _findid,

    bool _commfree) :
    ProgramInfo(
        _title, _subtitle, _description, _category,
        _chanid, _chanstr, _chansign, _channame, QString(),
        _recgroup, _playgroup,
        _startts, _endts, _recstartts, _recendts,
        _seriesid, _programid),
    oldrecstatus(rsUnknown),
    savedrecstatus(rsUnknown),
    record(NULL)
{
    recpriority = _recpriority;

    recstatus = _recstatus,

    recordid = _recordid;
    rectype = _rectype;
    dupin = _dupin;
    dupmethod = _dupmethod;

    findid = _findid;

    programflags &= ~FL_CHANCOMMFREE;
    programflags |= _commfree ? FL_CHANCOMMFREE : 0;
}

/** \brief Fills RecordingInfo for the program that air at
 *         "dtime" on "channel".
 *  \param chanid  %Channel ID on which to search for program.
 *  \param dtime   Date and Time for which we desire the program.
 *  \param genUnknown Generate a full entry for live-tv if unknown
 *  \param clampHoursMax Clamp the maximum time to X hours from dtime.
 *  \return LoadStatus describing what happened.
 */
RecordingInfo::RecordingInfo(
    uint _chanid, const QDateTime &desiredts,
    bool genUnknown, uint maxHours, LoadStatus *status) :
    oldrecstatus(rsUnknown),
    savedrecstatus(rsUnknown),
    record(NULL)
{
    ProgramList schedList;
    ProgramList progList;

    MSqlBindings bindings;
    QString querystr = "WHERE program.chanid    = :CHANID   AND "
                       "      program.starttime < :STARTTS1 AND "
                       "      program.endtime   > :STARTTS2 ";
    bindings[":CHANID"] = QString::number(_chanid);
    QString str_startts = desiredts.toString("yyyy-MM-ddThh:mm:50");
    bindings[":STARTTS1"] = str_startts;
    bindings[":STARTTS2"] = str_startts;

    ::LoadFromScheduler(schedList);
    LoadFromProgram(progList, querystr, bindings, schedList, false);

    if (!progList.empty())
    {
        ProgramInfo *pginfo = progList[0];

        if (maxHours > 0)
        {
            if (desiredts.secsTo(
                    pginfo->GetScheduledEndTime()) > (int)maxHours * 3600)
            {
                pginfo->SetScheduledEndTime(desiredts.addSecs(maxHours * 3600));
                pginfo->SetRecordingEndTime(pginfo->GetScheduledEndTime());
            }
        }

        *this = *pginfo;
        if (status)
            *status = kFoundProgram;
        return;
    }

    recstartts = startts = desiredts;
    recendts   = endts   = desiredts;
    lastmodified         = desiredts;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT chanid, channum, callsign, name, "
                  "commmethod, outputfilters "
                  "FROM channel "
                  "WHERE chanid = :CHANID");
    query.bindValue(":CHANID", _chanid);

    if (!query.exec())
    {
        MythDB::DBError("Loading Program overlapping a datetime", query);
        if (status)
            *status = kNoProgram;
        return;
    }

    if (!query.next())
    {
        if (status)
            *status = kNoProgram;
        return;
    }

    chanid               = query.value(0).toUInt();
    chanstr              = query.value(1).toString();
    chansign             = query.value(2).toString();
    channame             = query.value(3).toString();
    programflags &= ~FL_CHANCOMMFREE;
    programflags |= (query.value(4).toInt() == COMM_DETECT_COMMFREE) ?
        FL_CHANCOMMFREE : 0;
    chanplaybackfilters  = query.value(5).toString();

    {
        QMutexLocker locker(&staticDataLock);
        if (unknownTitle.isEmpty())
            unknownTitle = gCoreContext->GetSetting("UnknownTitle");
        title = unknownTitle;
        title.detach();
    }

    if (!genUnknown)
    {
        if (status)
            *status = kFakedZeroMinProgram;
        return;
    }

    // Round endtime up to the next half-hour.
    endts.setTime(QTime(endts.time().hour(),
                        endts.time().minute() / kUnknownProgramLength
                        * kUnknownProgramLength));
    endts = endts.addSecs(kUnknownProgramLength * 60);

    // if under a minute, bump it up to the next half hour
    if (startts.secsTo(endts) < 60)
        endts = endts.addSecs(kUnknownProgramLength * 60);

    recendts = endts;

    // Find next program starttime
    bindings.clear();
    QDateTime nextstart = startts;
    querystr = "WHERE program.chanid    = :CHANID  AND "
               "      program.starttime > :STARTTS "
               "GROUP BY program.starttime ORDER BY program.starttime LIMIT 1 ";
    bindings[":CHANID"]  = QString::number(_chanid);
    bindings[":STARTTS"] = desiredts.toString("yyyy-MM-ddThh:mm:50");

    LoadFromProgram(progList, querystr, bindings, schedList, false);

    if (!progList.empty())
        nextstart = (*progList.begin())->GetScheduledStartTime();

    if (nextstart > startts && nextstart < recendts)
        recendts = endts = nextstart;

    if (status)
        *status = kFakedLiveTVProgram;
}

/// \brief Copies important fields from other RecordingInfo.
void RecordingInfo::clone(const RecordingInfo &other,
                          bool ignore_non_serialized_data)
{
    bool is_same =
        (chanid && recstartts.isValid() && startts.isValid() &&
         chanid     == other.GetChanID() &&
         recstartts == other.GetRecordingStartTime() &&
         startts    == other.GetScheduledStartTime());

    ProgramInfo::clone(other, ignore_non_serialized_data);

    if (!is_same)
    {
        delete record;
        record = NULL;
    }

    if (!ignore_non_serialized_data)
    {
        oldrecstatus   = other.oldrecstatus;
        savedrecstatus = other.savedrecstatus;
    }
}

/// \brief Copies important fields from ProgramInfo
void RecordingInfo::clone(const ProgramInfo &other,
                          bool ignore_non_serialized_data)
{
    bool is_same =
        (chanid && recstartts.isValid() && startts.isValid() &&
         chanid     == other.GetChanID() &&
         recstartts == other.GetRecordingStartTime() &&
         startts    == other.GetScheduledStartTime());

    ProgramInfo::clone(other, ignore_non_serialized_data);

    if (!is_same)
    {
        delete record;
        record = NULL;
    }

    oldrecstatus   = rsUnknown;
    savedrecstatus = rsUnknown;
}

void RecordingInfo::clear(void)
{
    ProgramInfo::clear();

    delete record;
    record = NULL;

    oldrecstatus = rsUnknown;
    savedrecstatus = rsUnknown;
}


/** \fn RecordingInfo::~RecordingInfo()
 *  \brief Destructor deletes "record" if it exists.
 */
RecordingInfo::~RecordingInfo()
{
    delete record;
    record = NULL;
}

/** \fn RecordingInfo::GetProgramRecordingStatus()
 *  \brief Returns the recording type for this RecordingInfo, creating
 *         "record" field if necessary.
 *  \sa RecordingType, RecordingRule
 */
RecordingType RecordingInfo::GetProgramRecordingStatus(void)
{
    if (record == NULL)
    {
        record = new RecordingRule();
        record->LoadByProgram(this);
    }

    return record->m_type;
}

/** \fn RecordingInfo::GetProgramRecordingProfile() const
 *  \brief Returns recording profile name that will be, or was used,
 *         for this program, creating "record" field if necessary.
 *  \sa RecordingRule
 */
QString RecordingInfo::GetProgramRecordingProfile(void) const
{
    if (record == NULL)
    {
        record = new RecordingRule();
        record->LoadByProgram(this);
    }

    return record->m_recProfile;
}

/** \brief Returns a bitmap of which jobs are attached to this RecordingInfo.
 *  \sa JobTypes, GetProgramFlags()
 */
int RecordingInfo::GetAutoRunJobs(void) const
{
    if (record == NULL)
    {
        record = new RecordingRule();
        record->LoadByProgram(this);
    }

    int result = 0;

    if (record->m_autoTranscode)
        result |= JOB_TRANSCODE;
    if (record->m_autoCommFlag)
        result |= JOB_COMMFLAG;
    if (record->m_autoUserJob1)
        result |= JOB_USERJOB1;
    if (record->m_autoUserJob2)
        result |= JOB_USERJOB2;
    if (record->m_autoUserJob3)
        result |= JOB_USERJOB3;
    if (record->m_autoUserJob4)
        result |= JOB_USERJOB4;

    return result;
}

/** \fn RecordingInfo::ApplyRecordRecID(void)
 *  \brief Sets recordid to match RecordingRule recordid
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
        record->MakeOverride();
    record->m_type = newstate;

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
    record->m_recPriority = newrecpriority;
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

    SendUpdateEvent();
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

    SendUpdateEvent();
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

    SendUpdateEvent();
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
    query.bindValue(":START", recstartts);

    if (!query.exec())
        MythDB::DBError("RecTitle update", query);

    title = newTitle;
    subtitle = newSubtitle;

    SendUpdateEvent();
}

/* \fn RecordingInfo::ApplyTranscoderProfileChangeById(int id)
 * \brief Sets the transcoder profile for a recording
 * \param profileid is the 'id' field from recordingprofiles table.
 */
void RecordingInfo::ApplyTranscoderProfileChangeById(int id)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("UPDATE recorded "
            "SET transcoder = :PROFILEID "
            "WHERE chanid = :CHANID "
            "AND starttime = :START");
    query.bindValue(":PROFILEID",  id);
    query.bindValue(":CHANID",  chanid);
    query.bindValue(":START",  recstartts);

    if (!query.exec())
        MythDB::DBError(LOC + "unable to update transcoder "
                "in recorded table", query);
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

/**
 *  \brief Returns the "record" field, creating it if necessary.
 */
RecordingRule* RecordingInfo::GetRecordingRule(void)
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
    recordid = record->m_recordID;
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
        record = new RecordingRule();
        record->LoadByProgram(this);
    }

    hostname = gCoreContext->GetHostName();
    pathname = CreateRecordBasename(ext);

    int count = 0;
    while (!InsertProgram(this, record) && count < 50)
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

    SendAddedEvent();
}

bool RecordingInfo::InsertProgram(const RecordingInfo *pg,
                                  const RecordingRule *rule)
{
    MSqlQuery query(MSqlQuery::InitCon());

    //query.prepare("LOCK TABLES recorded WRITE");
    if (!query.exec("LOCK TABLES recorded WRITE"))
    {
        MythDB::DBError("InsertProgram -- lock", query);
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
            MythDB::DBError("InsertProgram -- select", query);
        else
            VERBOSE(VB_IMPORTANT, "recording already exists...");

        if (!query.exec("UNLOCK TABLES"))
            MythDB::DBError("InsertProgram -- unlock tables", query);
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

    if (pg->originalAirDate.isValid())
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
    query.bindValue(":AUTOEXP",     rule->m_autoExpire);
    query.bindValue(":SERIESID",    pg->seriesid);
    query.bindValue(":PROGRAMID",   pg->programid);
    query.bindValue(":FINDID",      pg->findid);
    query.bindValue(":STARS",       pg->stars);
    query.bindValue(":REPEAT",      pg->IsRepeat());
    query.bindValue(":TRANSCODER",  rule->m_transcoder);
    query.bindValue(":PLAYGROUP",   pg->playgroup);
    query.bindValue(":RECPRIORITY", rule->m_recPriority);
    query.bindValue(":BASENAME",    pg->pathname);
    query.bindValue(":STORGROUP",   pg->storagegroup);
    query.bindValue(":PROGSTART",   pg->startts);
    query.bindValue(":PROGEND",     pg->endts);
    query.bindValue(":PROFILE",     rule->m_recProfile);

    bool ok = query.exec() && (query.numRowsAffected() > 0);
    bool active = query.isActive();

    if (!query.exec("UNLOCK TABLES"))
        MythDB::DBError("InsertProgram -- unlock tables", query);

    if (!ok && !active)
        MythDB::DBError("InsertProgram -- insert", query);

    else if (pg->recordid > 0)
    {
        query.prepare("UPDATE channel SET last_record = NOW() "
                      "WHERE chanid = :CHANID");
        query.bindValue(":CHANID", pg->GetChanID());
        if (!query.exec())
            MythDB::DBError("InsertProgram -- channel last_record", query);

        query.prepare("UPDATE record SET last_record = NOW() "
                      "WHERE recordid = :RECORDID");
        query.bindValue(":RECORDID", pg->recordid);
        if (!query.exec())
            MythDB::DBError("InsertProgram -- record last_record", query);

        if (pg->rectype == kOverrideRecord && pg->parentid > 0)
        {
            query.prepare("UPDATE record SET last_record = NOW() "
                          "WHERE recordid = :PARENTID");
            query.bindValue(":PARENTID", pg->parentid);
            if (!query.exec())
                MythDB::DBError("InsertProgram -- record last_record override",
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
                  "       duplicate = :DUPLICATE "
                  "WHERE chanid = :CHANID AND "
                  "    starttime = :STARTTIME ");
    query.bindValue(":ENDTIME", recendts);
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);
    query.bindValue(":DUPLICATE", !prematurestop);

    if (!query.exec())
        MythDB::DBError("FinishedRecording update", query);

    GetProgramRecordingStatus();
    if (!prematurestop)
    {
        recstatus = rsRecorded;

        QString msg = "Finished recording";
        QString msg_subtitle = subtitle.isEmpty() ? "" :
                                        QString(" \"%1\"").arg(subtitle);
        QString details = QString("%1%2: channel %3")
                                        .arg(title)
                                        .arg(msg_subtitle)
                                        .arg(chanid);

        VERBOSE(VB_GENERAL, QString("%1 %2").arg(msg).arg(details));
        gCoreContext->LogEntry("scheduler", LP_NOTICE, msg, details);
    }

    SendUpdateEvent();
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

    SendUpdateEvent();
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
    bool dup = (GetRecordingStatus() == rsRecorded || forcedup);
    RecStatusType rs = (GetRecordingStatus() == rsCurrentRecording) ?
        rsPreviousRecording : GetRecordingStatus();
    oldrecstatus = GetRecordingStatus();
    if (dup)
        SetReactivated(false);
    uint erecid = parentid ? parentid : recordid;

    MSqlQuery result(MSqlQuery::InitCon());

    result.prepare("REPLACE INTO oldrecorded (chanid,starttime,"
                   "endtime,title,subtitle,description,category,"
                   "seriesid,programid,findid,recordid,station,"
                   "rectype,recstatus,duplicate,reactivate) "
                   "VALUES(:CHANID,:START,:END,:TITLE,:SUBTITLE,:DESC,"
                   ":CATEGORY,:SERIESID,:PROGRAMID,:FINDID,:RECORDID,"
                   ":STATION,:RECTYPE,:RECSTATUS,:DUPLICATE,:REACTIVATE);");
    result.bindValue(":CHANID", chanid);
    result.bindValue(":START", startts);
    result.bindValue(":END", endts);
    result.bindValue(":TITLE", title);
    result.bindValue(":SUBTITLE", subtitle);
    result.bindValue(":DESC", description);
    result.bindValue(":CATEGORY", category);
    result.bindValue(":SERIESID", seriesid);
    result.bindValue(":PROGRAMID", programid);
    result.bindValue(":FINDID", findid);
    result.bindValue(":RECORDID", erecid);
    result.bindValue(":STATION", chansign);
    result.bindValue(":RECTYPE", rectype);
    result.bindValue(":RECSTATUS", rs);
    result.bindValue(":DUPLICATE", dup);
    result.bindValue(":REACTIVATE", IsReactivated());

    if (!result.exec())
        MythDB::DBError("addHistory", result);

    if (dup && findid)
    {
        result.prepare("REPLACE INTO oldfind (recordid, findid) "
                       "VALUES(:RECORDID,:FINDID);");
        result.bindValue(":RECORDID", erecid);
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
    uint erecid = parentid ? parentid : recordid;

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
        result.bindValue(":RECORDID", erecid);
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
    uint erecid = parentid ? parentid : recordid;

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
        result.bindValue(":RECORDID", erecid);
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

/**
 *  \brief Replace %MATCH% vars in the specified string
 *  \param str QString containing matches to be substituted
 */
void RecordingInfo::SubstituteMatches(QString &str)
{
    str.replace("%RECID%", QString::number(getRecordID()));
    str.replace("%PARENTID%", QString::number(parentid));
    str.replace("%FINDID%", QString::number(findid));
    str.replace("%RECSTATUS%", QString::number(recstatus));
    str.replace("%RECTYPE%", QString::number(rectype));
    str.replace("%REACTIVATE%", IsReactivated() ? "1" : "0");

    ProgramInfo::SubstituteMatches(str);
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
