// POSIX headers
#include <sys/types.h>

// C headers
#include <cstdlib>

// Qt headers
#include <QMap>

// MythTV headers
#include "recordinginfo.h"
#include "recordingrule.h"
#include "scheduledrecording.h"
#include "mythmiscutil.h"
#include "mythdate.h"
#include "mythcorecontext.h"
#include "programinfoupdater.h"
#include "jobqueue.h"
#include "mythdb.h"
#include "mythlogging.h"

#define LOC      QString("RecordingInfo(%1): ").arg(GetBasename())

static inline QString null_to_empty(const QString &str)
{
    return str.isEmpty() ? "" : str;
}

QString RecordingInfo::s_unknownTitle;
// works only for integer divisors of 60
static const uint kUnknownProgramLength = 30;

RecordingInfo::RecordingInfo(
    const QString &_title,
    const QString &_sortTitle,
    const QString &_subtitle,
    const QString &_sortSubtitle,
    const QString &_description,
    uint _season,
    uint _episode,
    uint _totalepisodes,
    const QString &_syndicatedepisode,
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
    uint _partnumber,
    uint _parttotal,

    const QString &_seriesid,
    const QString &_programid,
    const QString &_inetref,
    const CategoryType _catType,

    int _recpriority,

    const QDateTime &_startts,
    const QDateTime &_endts,
    const QDateTime &_recstartts,
    const QDateTime &_recendts,

    float _stars,
    const QDate &_originalAirDate,

    bool _repeat,

    RecStatus::Type _oldrecstatus,
    bool _reactivate,

    uint _recordid,
    uint _parentid,
    RecordingType _rectype,
    RecordingDupInType _dupin,
    RecordingDupMethodType _dupmethod,

    uint _sourceid,
    uint _inputid,

    uint _findid,

    bool _commfree,
    uint _subtitleType,
    uint _videoproperties,
    uint _audioproperties,
    bool _future,
    int _schedorder,
    uint _mplexid,
    uint _sgroupid,
    const QString &_inputname) :
    ProgramInfo(
        _title, _sortTitle, _subtitle, _sortSubtitle,
        _description, _season, _episode, _totalepisodes,
        _category, _chanid, _chanstr, _chansign, _channame,
        QString(), _recgroup, _playgroup,
        _startts, _endts, _recstartts, _recendts,
        _seriesid, _programid, _inetref, _inputname),
    m_oldrecstatus(_oldrecstatus),
    m_future(_future),
    m_schedOrder(_schedorder),
    m_mplexId(_mplexid),
    m_sgroupId(_sgroupid),
    m_desiredRecStartTs(_startts),
    m_desiredRecEndTs(_endts)
{
    m_hostname = _hostname;
    m_storageGroup = _storagegroup;

    m_syndicatedEpisode = _syndicatedepisode;

    m_year = _year;
    m_partNumber = _partnumber;
    m_partTotal = _parttotal;
    m_catType = _catType;

    m_recPriority = _recpriority;

    m_stars = clamp(_stars, 0.0F, 1.0F);
    m_originalAirDate = _originalAirDate;
    if (m_originalAirDate.isValid() && m_originalAirDate < QDate(1895, 12, 28))
        m_originalAirDate = QDate();

    m_programFlags &= ~FL_REPEAT;
    m_programFlags |= _repeat ? FL_REPEAT : 0;
    m_programFlags &= ~FL_REACTIVATE;
    m_programFlags |= _reactivate ? FL_REACTIVATE : 0;
    m_programFlags &= ~FL_CHANCOMMFREE;
    m_programFlags |= _commfree ? FL_CHANCOMMFREE : 0;

    m_recordId = _recordid;
    m_parentId = _parentid;
    m_recType = _rectype;
    m_dupIn = _dupin;
    m_dupMethod = _dupmethod;

    m_sourceId = _sourceid;
    m_inputId = _inputid;
    m_findId = _findid;

    m_properties = ((_subtitleType    << kSubtitlePropertyOffset) |
                  (_videoproperties << kVideoPropertyOffset)  |
                  _audioproperties);

    if (m_recStartTs >= m_recEndTs)
    {
        // start/end-offsets are invalid so ignore
        m_recStartTs = m_startTs;
        m_recEndTs   = m_endTs;
    }

    LoadRecordingFile();
}

RecordingInfo::RecordingInfo(
    const QString &_title,
    const QString &_sortTitle,
    const QString &_subtitle,
    const QString &_sortSubtitle,
    const QString &_description,
    uint _season,
    uint _episode,
    const QString &_category,

    uint _chanid,
    const QString &_chanstr,
    const QString &_chansign,
    const QString &_channame,

    const QString &_recgroup,
    const QString &_playgroup,

    const QString &_seriesid,
    const QString &_programid,
    const QString &_inetref,

    int _recpriority,

    const QDateTime &_startts,
    const QDateTime &_endts,
    const QDateTime &_recstartts,
    const QDateTime &_recendts,

    RecStatus::Type _recstatus,

    uint _recordid,
    RecordingType _rectype,
    RecordingDupInType _dupin,
    RecordingDupMethodType _dupmethod,

    uint _findid,

    bool _commfree) :
    ProgramInfo(
        _title, _sortTitle, _subtitle, _sortSubtitle,
        _description, _season, _episode, 0,
        _category, _chanid, _chanstr, _chansign, _channame,
        QString(), _recgroup, _playgroup,
        _startts, _endts, _recstartts, _recendts,
        _seriesid, _programid, _inetref, ""),
    m_desiredRecStartTs(_startts),
    m_desiredRecEndTs(_endts)
{
    m_recPriority = _recpriority;

    m_recStatus = _recstatus,

    m_recordId = _recordid;
    m_recType = _rectype;
    m_dupIn = _dupin;
    m_dupMethod = _dupmethod;

    m_findId = _findid;

    m_programFlags &= ~FL_CHANCOMMFREE;
    m_programFlags |= _commfree ? FL_CHANCOMMFREE : 0;

    LoadRecordingFile();
}

/** \brief Fills RecordingInfo for the program that airs at
 *         "desiredts" on "chanid".
 *  \param _chanid  %Channel ID on which to search for program.
 *  \param desiredts Date and Time for which we desire the program.
 *  \param genUnknown Generate a full entry for live-tv if unknown
 *  \param maxHours Clamp the maximum time to X hours from dtime.
 *  \param[out] status LoadStatus describing what happened.
 */
RecordingInfo::RecordingInfo(
    uint _chanid, const QDateTime &desiredts,
    bool genUnknown, uint maxHours, LoadStatus *status)
{
    ProgramList schedList;
    ProgramList progList;

    MSqlBindings bindings;
    QString querystr = "WHERE program.chanid    = :CHANID   AND "
                       "      program.starttime < :STARTTS1 AND "
                       "      program.endtime   > :STARTTS2 ";
    bindings[":CHANID"] = QString::number(_chanid);
    QDateTime query_startts = desiredts.addSecs(50 - desiredts.time().second());
    bindings[":STARTTS1"] = query_startts;
    bindings[":STARTTS2"] = query_startts;

    ::LoadFromScheduler(schedList);
    LoadFromProgram(progList, querystr, bindings, schedList);

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

    m_recStartTs = m_startTs = desiredts;
    m_recEndTs   = m_endTs   = desiredts;
    m_lastModified           = desiredts;

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

    m_chanId               = query.value(0).toUInt();
    m_chanStr              = query.value(1).toString();
    m_chanSign             = query.value(2).toString();
    m_chanName             = query.value(3).toString();
    m_programFlags &= ~FL_CHANCOMMFREE;
    m_programFlags |= (query.value(4).toInt() == COMM_DETECT_COMMFREE) ?
        FL_CHANCOMMFREE : 0;
    m_chanPlaybackFilters  = query.value(5).toString();

    {
        QMutexLocker locker(&s_staticDataLock);
        if (s_unknownTitle.isEmpty())
            s_unknownTitle = gCoreContext->GetSetting("UnknownTitle");
        m_title = s_unknownTitle;
    }

    if (!genUnknown)
    {
        if (status)
            *status = kFakedZeroMinProgram;
        return;
    }

    // Round endtime up to the next half-hour.
    m_endTs = QDateTime(
        m_endTs.date(),
        QTime(m_endTs.time().hour(),
              m_endTs.time().minute() / kUnknownProgramLength
              * kUnknownProgramLength), Qt::UTC);
    m_endTs = m_endTs.addSecs(kUnknownProgramLength * 60);

    // if under a minute, bump it up to the next half hour
    if (m_startTs.secsTo(m_endTs) < 60)
        m_endTs = m_endTs.addSecs(kUnknownProgramLength * 60);

    m_recEndTs = m_endTs;

    // Find next program starttime
    bindings.clear();
    QDateTime nextstart = m_startTs;
    querystr = "WHERE program.chanid    = :CHANID  AND "
               "      program.starttime > :STARTTS "
               "GROUP BY program.starttime ORDER BY program.starttime LIMIT 1 ";
    bindings[":CHANID"]  = QString::number(_chanid);
    bindings[":STARTTS"] = desiredts.addSecs(50 - desiredts.time().second());

    LoadFromProgram(progList, querystr, bindings, schedList);

    if (!progList.empty())
        nextstart = (*progList.begin())->GetScheduledStartTime();

    if (nextstart > m_startTs && nextstart < m_recEndTs)
        m_recEndTs = m_endTs = nextstart;

    if (status)
        *status = kFakedLiveTVProgram;

    m_desiredRecStartTs = m_startTs;
    m_desiredRecEndTs = m_endTs;

    LoadRecordingFile();
}

/// \brief Copies important fields from other RecordingInfo.
void RecordingInfo::clone(const RecordingInfo &other,
                          bool ignore_non_serialized_data)
{
    bool is_same =
        (m_chanId && m_recStartTs.isValid() && m_startTs.isValid() &&
         m_chanId     == other.GetChanID() &&
         m_recStartTs == other.GetRecordingStartTime() &&
         m_startTs    == other.GetScheduledStartTime());

    ProgramInfo::clone(other, ignore_non_serialized_data);

    if (!is_same)
    {
        delete m_record;
        m_record = nullptr;
    }

    if (!ignore_non_serialized_data)
    {
        m_oldrecstatus      = other.m_oldrecstatus;
        m_savedrecstatus    = other.m_savedrecstatus;
        m_future            = other.m_future;
        m_schedOrder        = other.m_schedOrder;
        m_mplexId           = other.m_mplexId;
        m_sgroupId          = other.m_sgroupId;
        m_desiredRecStartTs = other.m_desiredRecStartTs;
        m_desiredRecEndTs   = other.m_desiredRecEndTs;
    }

    delete m_recordingFile;
    m_recordingFile = nullptr;
    LoadRecordingFile();
}

/// \brief Copies important fields from ProgramInfo
void RecordingInfo::clone(const ProgramInfo &other,
                          bool ignore_non_serialized_data)
{
    bool is_same =
        (m_chanId && m_recStartTs.isValid() && m_startTs.isValid() &&
         m_chanId     == other.GetChanID() &&
         m_recStartTs == other.GetRecordingStartTime() &&
         m_startTs    == other.GetScheduledStartTime());

    ProgramInfo::clone(other, ignore_non_serialized_data);

    if (!is_same)
    {
        delete m_record;
        m_record = nullptr;
    }

    m_oldrecstatus      = RecStatus::Unknown;
    m_savedrecstatus    = RecStatus::Unknown;
    m_future            = false;
    m_schedOrder        = 0;
    m_mplexId           = 0;
    m_sgroupId          = 0;
    m_desiredRecStartTs = QDateTime();
    m_desiredRecEndTs   = QDateTime();

    delete m_recordingFile;
    m_recordingFile = nullptr;
    LoadRecordingFile();
}

void RecordingInfo::clear(void)
{
    ProgramInfo::clear();

    delete m_record;
    m_record = nullptr;

    m_oldrecstatus      = RecStatus::Unknown;
    m_savedrecstatus    = RecStatus::Unknown;
    m_future            = false;
    m_schedOrder        = 0;
    m_mplexId           = 0;
    m_sgroupId          = 0;
    m_desiredRecStartTs = QDateTime();
    m_desiredRecEndTs   = QDateTime();

    delete m_recordingFile;
    m_recordingFile = nullptr;
}


/** \fn RecordingInfo::~RecordingInfo()
 *  \brief Destructor deletes "record" if it exists.
 */
RecordingInfo::~RecordingInfo()
{
    delete m_record;
    m_record = nullptr;

    delete m_recordingFile;
    m_recordingFile = nullptr;
}

/** \fn RecordingInfo::GetProgramRecordingStatus()
 *  \brief Returns the recording type for this RecordingInfo, creating
 *         "record" field if necessary.
 *  \sa RecordingType, RecordingRule
 */
RecordingType RecordingInfo::GetProgramRecordingStatus(void)
{
    if (m_record == nullptr)
    {
        m_record = new RecordingRule();
        m_record->LoadByProgram(this);
    }

    return m_record->m_type;
}

/** \fn RecordingInfo::GetProgramRecordingProfile() const
 *  \brief Returns recording profile name that will be, or was used,
 *         for this program, creating "record" field if necessary.
 *  \sa RecordingRule
 */
QString RecordingInfo::GetProgramRecordingProfile(void) const
{
    if (m_record == nullptr)
    {
        m_record = new RecordingRule();
        m_record->LoadByProgram(this);
    }

    return m_record->m_recProfile;
}

/** \brief Returns a bitmap of which jobs are attached to this RecordingInfo.
 *  \sa JobTypes, GetProgramFlags()
 */
int RecordingInfo::GetAutoRunJobs(void) const
{
    if (m_record == nullptr)
    {
        m_record = new RecordingRule();
        m_record->LoadByProgram(this);
    }

    int result = 0;

    if (m_record->m_autoTranscode)
        result |= JOB_TRANSCODE;
    if (m_record->m_autoCommFlag)
        result |= JOB_COMMFLAG;
    if (m_record->m_autoMetadataLookup)
        result |= JOB_METADATA;
    if (m_record->m_autoUserJob1)
        result |= JOB_USERJOB1;
    if (m_record->m_autoUserJob2)
        result |= JOB_USERJOB2;
    if (m_record->m_autoUserJob3)
        result |= JOB_USERJOB3;
    if (m_record->m_autoUserJob4)
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
        LOG(VB_GENERAL, LOG_ERR,
            "ProgInfo Error: ApplyRecordRecID(void) needs recordid");
        return;
    }

    query.prepare("UPDATE recorded "
                  "SET recordid = :RECID "
                  "WHERE chanid = :CHANID AND starttime = :START");

    if (m_recType == kOverrideRecord && m_parentId > 0)
        query.bindValue(":RECID", m_parentId);
    else
        query.bindValue(":RECID",  getRecordID());
    query.bindValue(":CHANID", m_chanId);
    query.bindValue(":START",  m_recStartTs);

    if (!query.exec())
        MythDB::DBError(LOC + "RecordID update", query);
}

/**
 *  \brief Sets RecordingType of "record", creating "record" if it
 *         does not exist.
 *  \param newstate State to apply to "record" RecordingType. This
 *                  uses same values as return of GetProgramRecordingState
 *  \param save     If true, save the new state of the recording into the
 *                  database. Note: If the new state is kNotRecording this
 *                  means that the recording will be deleted.
 */
void RecordingInfo::ApplyRecordStateChange(RecordingType newstate, bool save)
{
    GetProgramRecordingStatus();
    if (newstate == kOverrideRecord || newstate == kDontRecord)
        m_record->MakeOverride();
    m_record->m_type = newstate;

    if (save)
    {
        if (newstate == kNotRecording)
            m_record->Delete();
        else
            m_record->Save();
    }
}

/** \fn RecordingInfo::ApplyRecordRecPriorityChange(int)
 *  \brief Sets recording priority of "record", creating "record" if it
 *         does not exist.
 *  \param newrecpriority New recording priority.
 */
void RecordingInfo::ApplyRecordRecPriorityChange(int newrecpriority)
{
    GetProgramRecordingStatus();
    m_record->m_recPriority = newrecpriority;
    m_record->Save();
}

/** \fn RecordingInfo::ApplyRecordRecGroupChange(const QString &newrecgroup)
 *  \brief Sets the recording group, both in this RecordingInfo
 *         and in the database.
 *  \param newrecgroup New recording group.
 */
void RecordingInfo::ApplyRecordRecGroupChange(const QString &newrecgroup)
{
    MSqlQuery query(MSqlQuery::InitCon());

    int newrecgroupid = GetRecgroupID(newrecgroup);

    // Catchall - in the event that the group doesn't exist, then to avoid
    // breakage, we need to create it
    if (newrecgroupid == 0)
    {
        query.prepare("INSERT INTO recgroups SET recgroup = :NAME, "
                      "displayname = :DISPLAYNAME");
        query.bindValue(":NAME", newrecgroup);
        query.bindValue(":DISPLAYNAME", newrecgroup);

        if (query.exec())
            newrecgroupid = query.lastInsertId().toInt();

        if (newrecgroupid <= 0)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Could not create recording group (%1). "
                                            "Does it already exist?").arg(newrecgroup));
            return;
        }
    }

    LOG(VB_GENERAL, LOG_NOTICE,
            QString("ApplyRecordRecGroupChange: %1 to %2 (%3)").arg(m_recGroup)
                                                               .arg(newrecgroup)
                                                               .arg(newrecgroupid));

    query.prepare("UPDATE recorded"
                  " SET recgroup = :RECGROUP, "
                  "     recgroupid = :RECGROUPID "
                  " WHERE chanid = :CHANID"
                  " AND starttime = :START ;");
    query.bindValue(":RECGROUP", null_to_empty(newrecgroup));
    query.bindValue(":RECGROUPID", newrecgroupid);
    query.bindValue(":START", m_recStartTs);
    query.bindValue(":CHANID", m_chanId);

    if (!query.exec())
        MythDB::DBError("RecGroup update", query);

    m_recGroup = newrecgroup; // Deprecate in favour of recgroupid
    //recgroupid = newrecgroupid;

    SendUpdateEvent();
}

void RecordingInfo::ApplyRecordRecGroupChange(int newrecgroupid)
{
    MSqlQuery query(MSqlQuery::InitCon());

    QString newrecgroup;
    if (newrecgroupid > 0)
    {
        newrecgroup = GetRecgroupString(newrecgroupid);

        query.prepare("UPDATE recorded"
                    " SET recgroup = :RECGROUP, "
                    "     recgroupid = :RECGROUPID "
                    " WHERE chanid = :CHANID"
                    " AND starttime = :START ;");
        query.bindValue(":RECGROUP", null_to_empty(newrecgroup));
        query.bindValue(":RECGROUPID", newrecgroupid);
        query.bindValue(":START", m_recStartTs);
        query.bindValue(":CHANID", m_chanId);

        if (!query.exec())
            MythDB::DBError("RecGroup update", query);

        m_recGroup = newrecgroup; // Deprecate in favour of recgroupid
        //recgroupid = newrecgroupid;

        SendUpdateEvent();
    }

    LOG(VB_GENERAL, LOG_NOTICE,
            QString("ApplyRecordRecGroupChange: %1 to %2 (%3)").arg(m_recGroup)
                                                               .arg(newrecgroup)
                                                               .arg(newrecgroupid));
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
    query.bindValue(":PLAYGROUP", null_to_empty(newplaygroup));
    query.bindValue(":START", m_recStartTs);
    query.bindValue(":CHANID", m_chanId);

    if (!query.exec())
        MythDB::DBError("PlayGroup update", query);

    m_playGroup = newplaygroup;

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
    query.bindValue(":STORAGEGROUP", null_to_empty(newstoragegroup));
    query.bindValue(":START", m_recStartTs);
    query.bindValue(":CHANID", m_chanId);

    if (!query.exec())
        MythDB::DBError("StorageGroup update", query);

    m_storageGroup = newstoragegroup;

    SendUpdateEvent();
}

/** \fn RecordingInfo::ApplyRecordRecTitleChange(const QString &newTitle, const QString &newSubtitle, const QString &newDescription)
 *  \brief Sets the recording title, subtitle, and description both in this
 *         RecordingInfo and in the database.
 *  \param newTitle New recording title.
 *  \param newSubtitle New recording subtitle
 *  \param newDescription New recording description
 */
void RecordingInfo::ApplyRecordRecTitleChange(const QString &newTitle,
        const QString &newSubtitle, const QString &newDescription)
{
    MSqlQuery query(MSqlQuery::InitCon());
    QString sql = "UPDATE recorded SET title = :TITLE, subtitle = :SUBTITLE ";
    if (!newDescription.isNull())
        sql += ", description = :DESCRIPTION ";
    sql += " WHERE chanid = :CHANID AND starttime = :START ;";

    query.prepare(sql);
    query.bindValue(":TITLE", newTitle);
    query.bindValue(":SUBTITLE", null_to_empty(newSubtitle));
    if (!newDescription.isNull())
        query.bindValue(":DESCRIPTION", newDescription);
    query.bindValue(":CHANID", m_chanId);
    query.bindValue(":START", m_recStartTs);

    if (!query.exec())
        MythDB::DBError("RecTitle update", query);

    m_title = newTitle;
    m_subtitle = newSubtitle;
    if (!newDescription.isNull())
        m_description = newDescription;

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
    query.bindValue(":CHANID",  m_chanId);
    query.bindValue(":START",  m_recStartTs);

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
        query.bindValue(":CHANID",  m_chanId);
        query.bindValue(":START",  m_recStartTs);

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
            query.bindValue(":CHANID",  m_chanId);
            query.bindValue(":START",  m_recStartTs);

            if (!query.exec())
                MythDB::DBError(LOC + "unable to update transcoder "
                                     "in recorded table", query);
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR,
                "ProgramInfo: unable to query transcoder profile ID");
        }
    }
}

/**
 *  \brief Set this program to never be recorded by inserting 'history' for
 *         it into the database with a status of RecStatus::NeverRecord
 */
void RecordingInfo::ApplyNeverRecord(void)
{
    SetRecordingStatus(RecStatus::NeverRecord);
    SetScheduledStartTime(MythDate::current());
    SetScheduledEndTime(GetRecordingStartTime());
    AddHistory(true, true);
}

/** \fn RecordingInfo::QuickRecord(void)
 *  \brief Create a kSingleRecord if not already scheduled.
 */
void RecordingInfo::QuickRecord(void)
{
    RecordingType curType = GetProgramRecordingStatus();
    if (curType == kNotRecording)
        ApplyRecordStateChange(kSingleRecord);
}

/**
 *  \brief Returns the "record" field, creating it if necessary.
 */
RecordingRule* RecordingInfo::GetRecordingRule(void)
{
    GetProgramRecordingStatus();
    return m_record;
}

/** \fn RecordingInfo::getRecordID(void)
 *  \brief Returns a record id, creating "record" it if necessary.
 */
int RecordingInfo::getRecordID(void)
{
    GetProgramRecordingStatus();
    m_recordId = m_record->m_recordID;
    return m_recordId;
}

bool RecordingInfo::QueryRecordedIdForKey(int & recordedid,
                                          uint chanid, const QDateTime& recstartts)
{
    if (chanid < 1)
    {
        LOG(VB_RECORD, LOG_WARNING,
            QString("QueryRecordedIdFromKey: Invalid chanid %1").arg(chanid));
        return false;
    }
    if (!recstartts.isValid())
    {
        LOG(VB_RECORD, LOG_WARNING,
            QString("QueryRecordedIdFromKey: Invalid start ts %1")
            .arg(recstartts.toString()));
        return false;
    }

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT recordedid FROM recorded "
        "WHERE chanid = :CHANID AND starttime = :RECSTARTTS");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":RECSTARTTS", recstartts);
    if (query.exec() && query.next())
    {
        recordedid = query.value(0).toUInt();
        return true;
    }

    return false;
}

/**
 *  \brief Inserts this RecordingInfo into the database as an existing recording.
 *
 *  This method, of course, only works if a recording has been scheduled
 *  and started.
 *
 *  \param ext    File extension for recording
 */
void RecordingInfo::StartedRecording(const QString& ext)
{
    QString dirname = m_pathname;

    if (!m_record)
    {
        m_record = new RecordingRule();
        m_record->LoadByProgram(this);
    }

    m_hostname = gCoreContext->GetHostName();
    m_pathname = CreateRecordBasename(ext);

    int count = 0;
    while (!InsertProgram(this, m_record) && count < 50)
    {
        m_recStartTs = m_recStartTs.addSecs(1);
        m_pathname = CreateRecordBasename(ext);
        count++;
    }

    if (count >= 50)
    {
        LOG(VB_GENERAL, LOG_ERR, "Couldn't insert program");
        return;
    }

    m_pathname = dirname + "/" + m_pathname;

    LOG(VB_FILE, LOG_INFO, LOC + QString("StartedRecording: Recording to '%1'")
                             .arg(m_pathname));


    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("DELETE FROM recordedseek WHERE chanid = :CHANID"
                  " AND starttime = :START;");
    query.bindValue(":CHANID", m_chanId);
    query.bindValue(":START", m_recStartTs);

    if (!query.exec() || !query.isActive())
        MythDB::DBError("Clear seek info on record", query);

    query.prepare("DELETE FROM recordedmarkup WHERE chanid = :CHANID"
                  " AND starttime = :START;");
    query.bindValue(":CHANID", m_chanId);
    query.bindValue(":START", m_recStartTs);

    if (!query.exec() || !query.isActive())
        MythDB::DBError("Clear markup on record", query);

    query.prepare("REPLACE INTO recordedcredits"
                 " SELECT * FROM credits"
                 " WHERE chanid = :CHANID AND starttime = :START;");
    query.bindValue(":CHANID", m_chanId);
    query.bindValue(":START", m_startTs);
    if (!query.exec() || !query.isActive())
        MythDB::DBError("Copy program credits on record", query);

    query.prepare("REPLACE INTO recordedprogram"
                 " SELECT * from program"
                 " WHERE chanid = :CHANID AND starttime = :START"
                 " AND title = :TITLE;");
    query.bindValue(":CHANID", m_chanId);
    query.bindValue(":START", m_startTs);
    query.bindValue(":TITLE", m_title);
    if (!query.exec() || !query.isActive())
        MythDB::DBError("Copy program data on record", query);

    query.prepare("REPLACE INTO recordedrating"
                 " SELECT * from programrating"
                 " WHERE chanid = :CHANID AND starttime = :START;");
    query.bindValue(":CHANID", m_chanId);
    query.bindValue(":START", m_startTs);
    if (!query.exec() || !query.isActive())
        MythDB::DBError("Copy program ratings on record", query);

    // File
    if (!GetRecordingFile())
        LoadRecordingFile();
    RecordingFile *recFile = GetRecordingFile();
    recFile->m_fileName = GetBasename();
    recFile->m_storageDeviceID = GetHostname();
    recFile->m_storageGroup = GetStorageGroup();
    recFile->Save();

    SendAddedEvent();
}

bool RecordingInfo::InsertProgram(RecordingInfo *pg,
                                  const RecordingRule *rule)
{
    QString inputname = pg->GetInputName();
    int recgroupid = GetRecgroupID(pg->m_recGroup);

    MSqlQuery query(MSqlQuery::InitCon());

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
    query.bindValue(":CHANID", pg->m_chanId);
    query.bindValue(":STARTS", pg->m_recStartTs);

    bool err = true;
    if (!query.exec())
    {
        MythDB::DBError("InsertProgram -- select", query);
    }
    else if (query.next())
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("RecordingInfo::InsertProgram(%1): ")
                .arg(pg->toString()) + "recording already exists...");
    }
    else
    {
        err = false;
    }

    if (err)
    {
        if (!query.exec("UNLOCK TABLES"))
            MythDB::DBError("InsertProgram -- unlock tables", query);
        return false;
    }

    query.prepare(
        "INSERT INTO recorded "
        "   (chanid,    starttime,   endtime,         title,            "
        "    subtitle,  description, season,          episode,          "
        "    hostname,  category,    recgroup,        autoexpire,       "
        "    recordid,  seriesid,    programid,       inetref,          "
        "    stars,     previouslyshown,              originalairdate,  "
        "    findid,    transcoder,  playgroup,       recpriority,      "
        "    basename,  progstart,   progend,         profile,          "
        "    duplicate, storagegroup, inputname,      recgroupid) "
        "VALUES"
        "  (:CHANID,   :STARTS,     :ENDS,           :TITLE,            "
        "   :SUBTITLE, :DESC,       :SEASON,         :EPISODE,          "
        "   :HOSTNAME, :CATEGORY,   :RECGROUP,       :AUTOEXP,          "
        "   :RECORDID, :SERIESID,   :PROGRAMID,      :INETREF,          "
        "   :STARS,    :REPEAT,                      :ORIGAIRDATE,      "
        "   :FINDID,   :TRANSCODER, :PLAYGROUP,      :RECPRIORITY,      "
        "   :BASENAME, :PROGSTART,  :PROGEND,        :PROFILE,          "
        "   0,         :STORGROUP,  :INPUTNAME,      :RECGROUPID) "
        );

    if (pg->m_recType == kOverrideRecord)
        query.bindValue(":RECORDID",    pg->m_parentId);
    else
        query.bindValue(":RECORDID",    pg->m_recordId);

    if (pg->m_originalAirDate.isValid())
    {
        query.bindValue(":ORIGAIRDATE", pg->m_originalAirDate);
    // If there is no originalairdate use "year"
    }
    else if (pg->m_year >= 1895)
    {
        query.bindValue(":ORIGAIRDATE", QDate(pg->m_year,1,1));
    }
    else
    {
        query.bindValue(":ORIGAIRDATE", "0000-00-00");
    }

    query.bindValue(":CHANID",      pg->m_chanId);
    query.bindValue(":STARTS",      pg->m_recStartTs);
    query.bindValue(":ENDS",        pg->m_recEndTs);
    query.bindValue(":TITLE",       pg->m_title);
    query.bindValue(":SUBTITLE",    null_to_empty(pg->m_subtitle));
    query.bindValue(":DESC",        null_to_empty(pg->m_description));
    query.bindValue(":SEASON",      pg->m_season);
    query.bindValue(":EPISODE",     pg->m_episode);
    query.bindValue(":HOSTNAME",    pg->m_hostname);
    query.bindValue(":CATEGORY",    null_to_empty(pg->m_category));
    query.bindValue(":RECGROUP",    null_to_empty(pg->m_recGroup));
    query.bindValue(":AUTOEXP",     rule->m_autoExpire);
    query.bindValue(":SERIESID",    null_to_empty(pg->m_seriesId));
    query.bindValue(":PROGRAMID",   null_to_empty(pg->m_programId));
    query.bindValue(":INETREF",     null_to_empty(pg->m_inetRef));
    query.bindValue(":FINDID",      pg->m_findId);
    query.bindValue(":STARS",       pg->m_stars);
    query.bindValue(":REPEAT",      pg->IsRepeat());
    query.bindValue(":TRANSCODER",  rule->m_transcoder);
    query.bindValue(":PLAYGROUP",   pg->m_playGroup);
    query.bindValue(":RECPRIORITY", rule->m_recPriority);
    query.bindValue(":BASENAME",    pg->m_pathname);
    query.bindValue(":STORGROUP",   null_to_empty(pg->m_storageGroup));
    query.bindValue(":PROGSTART",   pg->m_startTs);
    query.bindValue(":PROGEND",     pg->m_endTs);
    query.bindValue(":PROFILE",     null_to_empty(rule->m_recProfile));
    query.bindValue(":INPUTNAME",   inputname);
    query.bindValue(":RECGROUPID",  recgroupid);

    bool ok = query.exec() && (query.numRowsAffected() > 0);
    if (ok)
    {
        pg->SetRecordingID(query.lastInsertId().toUInt());
    }
    bool active = query.isActive();

    if (!query.exec("UNLOCK TABLES"))
        MythDB::DBError("InsertProgram -- unlock tables", query);

    if (!ok && !active)
    {
        MythDB::DBError("InsertProgram -- insert", query);

    }
    else if (pg->m_recordId > 0)
    {
        query.prepare("UPDATE channel SET last_record = NOW() "
                      "WHERE chanid = :CHANID");
        query.bindValue(":CHANID", pg->GetChanID());
        if (!query.exec())
            MythDB::DBError("InsertProgram -- channel last_record", query);

        query.prepare("UPDATE record SET last_record = NOW() "
                      "WHERE recordid = :RECORDID");
        query.bindValue(":RECORDID", pg->m_recordId);
        if (!query.exec())
            MythDB::DBError("InsertProgram -- record last_record", query);

        if (pg->m_recType == kOverrideRecord && pg->m_parentId > 0)
        {
            query.prepare("UPDATE record SET last_record = NOW() "
                          "WHERE recordid = :PARENTID");
            query.bindValue(":PARENTID", pg->m_parentId);
            if (!query.exec())
                MythDB::DBError("InsertProgram -- record last_record override",
                                query);
        }
    }

    return ok;
}

/**
 *  \brief If not a premature stop, adds program to history of recorded
 *         programs.
 *  \param allowReRecord This flag goes into the entry in the recorded
 *                       programs. It also determines whether the
 *                       recordedmarkup database table is updated with the
 *                       program length.
 */
void RecordingInfo::FinishedRecording(bool allowReRecord)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("UPDATE recorded SET endtime = :ENDTIME, "
                  "       duplicate = :DUPLICATE "
                  "WHERE chanid = :CHANID AND "
                  "    starttime = :STARTTIME ");
    query.bindValue(":ENDTIME", m_recEndTs);
    query.bindValue(":CHANID", m_chanId);
    query.bindValue(":STARTTIME", m_recStartTs);
    query.bindValue(":DUPLICATE", !allowReRecord);

    if (!query.exec())
        MythDB::DBError("FinishedRecording update", query);

    GetProgramRecordingStatus();
    if (!allowReRecord)
    {
        m_recStatus = RecStatus::Recorded;

#if QT_VERSION < QT_VERSION_CHECK(5,8,0)
        uint starttime = m_recStartTs.toTime_t();
        uint endtime   = m_recEndTs.toTime_t();
        int64_t duration = ((int64_t)endtime - (int64_t)starttime) * 1000000;
#else
        qint64 starttime = m_recStartTs.toSecsSinceEpoch();
        qint64 endtime   = m_recEndTs.toSecsSinceEpoch();
        int64_t duration = (endtime - starttime) * 1000000;
#endif
        SaveTotalDuration(duration);

        QString msg = "Finished recording";
        QString msg_subtitle = m_subtitle.isEmpty() ? "" :
                                        QString(" \"%1\"").arg(m_subtitle);
        QString details = QString("%1%2: channel %3")
                                        .arg(m_title)
                                        .arg(msg_subtitle)
                                        .arg(m_chanId);

        LOG(VB_GENERAL, LOG_INFO, QString("%1 %2").arg(msg).arg(details));
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
    query.bindValue(":ENDTIME", m_recEndTs);

    query.bindValue(":CHANID", m_chanId);
    query.bindValue(":STARTTIME", m_recStartTs);

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
    result.bindValue(":STARTTIME", m_startTs);
    result.bindValue(":TITLE", m_title);
    result.bindValue(":STATION", m_chanSign);

    if (!result.exec())
        MythDB::DBError("ReactivateRecording", result);

    ScheduledRecording::ReschedulePlace("Reactivate");
}

/**
 *  \brief Adds recording history, creating "record" it if necessary.
 */
void RecordingInfo::AddHistory(bool resched, bool forcedup, bool future)
{
    bool dup = (GetRecordingStatus() == RecStatus::Recorded || forcedup);
    RecStatus::Type rs = (GetRecordingStatus() == RecStatus::CurrentRecording &&
                        !future) ? RecStatus::PreviousRecording : GetRecordingStatus();
    LOG(VB_SCHEDULE, LOG_INFO, QString("AddHistory: %1/%2, %3, %4, %5/%6")
        .arg(int(rs)).arg(int(m_oldrecstatus)).arg(future).arg(dup)
        .arg(GetScheduledStartTime(MythDate::ISODate)).arg(GetTitle()));
    if (!future)
        m_oldrecstatus = GetRecordingStatus();
    if (dup)
        SetReactivated(false);
    uint erecid = m_parentId ? m_parentId : m_recordId;

    MSqlQuery result(MSqlQuery::InitCon());

    result.prepare("REPLACE INTO oldrecorded (chanid,starttime,"
                   "endtime,title,subtitle,description,season,episode,"
                   "category,seriesid,programid,inetref,findid,recordid,"
                   "station,rectype,recstatus,duplicate,reactivate,generic,"
                   "future) "
                   "VALUES(:CHANID,:START,:END,:TITLE,:SUBTITLE,:DESC,:SEASON,"
                   ":EPISODE,:CATEGORY,:SERIESID,:PROGRAMID,:INETREF,"
                   ":FINDID,:RECORDID,:STATION,:RECTYPE,:RECSTATUS,:DUPLICATE,"
                   ":REACTIVATE,:GENERIC,:FUTURE);");
    result.bindValue(":CHANID", m_chanId);
    result.bindValue(":START", m_startTs);
    result.bindValue(":END", m_endTs);
    result.bindValue(":TITLE", m_title);
    result.bindValue(":SUBTITLE", null_to_empty(m_subtitle));
    result.bindValue(":DESC", null_to_empty(m_description));
    result.bindValue(":SEASON", m_season);
    result.bindValue(":EPISODE", m_episode);
    result.bindValue(":CATEGORY", null_to_empty(m_category));
    result.bindValue(":SERIESID", null_to_empty(m_seriesId));
    result.bindValue(":PROGRAMID", null_to_empty(m_programId));
    result.bindValue(":INETREF", null_to_empty(m_inetRef));
    result.bindValue(":FINDID", m_findId);
    result.bindValue(":RECORDID", erecid);
    result.bindValue(":STATION", null_to_empty(m_chanSign));
    result.bindValue(":RECTYPE", m_recType);
    result.bindValue(":RECSTATUS", rs);
    result.bindValue(":DUPLICATE", dup);
    result.bindValue(":REACTIVATE", 0);
    result.bindValue(":GENERIC", IsGeneric());
    result.bindValue(":FUTURE", future);

    if (!result.exec())
        MythDB::DBError("addHistory", result);

    if (dup && m_findId)
    {
        result.prepare("REPLACE INTO oldfind (recordid, findid) "
                       "VALUES(:RECORDID,:FINDID);");
        result.bindValue(":RECORDID", erecid);
        result.bindValue(":FINDID", m_findId);

        if (!result.exec())
            MythDB::DBError("addFindHistory", result);
    }

    // The adding of an entry to oldrecorded may affect near-future
    // scheduling decisions, so recalculate if told
    if (resched)
        ScheduledRecording::RescheduleCheck(*this, "AddHistory");
}

/** \fn RecordingInfo::DeleteHistory(void)
 *  \brief Deletes recording history, creating "record" it if necessary.
 */
void RecordingInfo::DeleteHistory(void)
{
    uint erecid = m_parentId ? m_parentId : m_recordId;

    MSqlQuery result(MSqlQuery::InitCon());

    result.prepare("DELETE FROM oldrecorded WHERE title = :TITLE AND "
                   "starttime = :START AND station = :STATION");
    result.bindValue(":TITLE", m_title);
    result.bindValue(":START", m_recStartTs);
    result.bindValue(":STATION", m_chanSign);

    if (!result.exec())
        MythDB::DBError("deleteHistory", result);

    if (/*m_duplicate &&*/ m_findId)
    {
        result.prepare("DELETE FROM oldfind WHERE "
                       "recordid = :RECORDID AND findid = :FINDID");
        result.bindValue(":RECORDID", erecid);
        result.bindValue(":FINDID", m_findId);

        if (!result.exec())
            MythDB::DBError("deleteFindHistory", result);
    }

    // The removal of an entry from oldrecorded may affect near-future
    // scheduling decisions, so recalculate
    ScheduledRecording::RescheduleCheck(*this, "DeleteHistory");
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
    uint erecid = m_parentId ? m_parentId : m_recordId;
    uint din = m_dupIn;
    uint dmeth = m_dupMethod;

    if (din == kDupsUnset)
        din = kDupsInAll;
    if (dmeth == kDupCheckUnset)
        dmeth = kDupCheckSubThenDesc;

    MSqlQuery result(MSqlQuery::InitCon());

    // Handle this specific entry in recorded.
    result.prepare("UPDATE recorded SET duplicate = 0 "
                   "WHERE chanid = :CHANID "
                       "AND starttime = :STARTTIME "
                       "AND title = :TITLE;");
    result.bindValue(":STARTTIME", m_recStartTs);
    result.bindValue(":TITLE", m_title);
    result.bindValue(":CHANID", m_chanId);

    if (!result.exec())
        MythDB::DBError("forgetRecorded1", result);

    // Handle other matching entries in recorded.
    if (din & kDupsInRecorded)
    {
        result.prepare(
            "UPDATE recorded SET duplicate = 0 "
            "WHERE duplicate = 1 AND "
            "      title = :TITLE AND "
            "      ( "
            "        (:PROGRAMID1 <> '' AND "
            "         :PROGRAMID2 = recorded.programid) "
            "        OR "
            "        ( "
            "          (:PROGRAMID3 = '' OR recorded.programid = '' OR "
            "           LEFT(:PROGRAMID4, LOCATE('/', :PROGRAMID5)) <> "
            "             LEFT(recorded.programid, "
            "                  LOCATE('/', recorded.programid))) "
            "          AND "
            "          (((:DUPMETHOD1 & 0x02) = 0) OR (:SUBTITLE1 <> '' "
            "            AND :SUBTITLE2 = recorded.subtitle)) "
            "          AND "
            "          (((:DUPMETHOD2 & 0x04) = 0) OR (:DESCRIPTION1 <> '' "
            "            AND :DESCRIPTION2 = recorded.description)) "
            "          AND "
            "          (((:DUPMETHOD3 & 0x08) = 0) OR "
            "            (:SUBTITLE3 <> '' AND "
            "             (:SUBTITLE4 = recorded.subtitle OR "
            "              (recorded.subtitle = '' AND "
            "               :SUBTITLE5 = recorded.description))) OR "
            "            (:SUBTITLE6 = '' AND :DESCRIPTION3 <> '' AND "
            "             (:DESCRIPTION4 = recorded.subtitle OR "
            "              (recorded.subtitle = '' AND "
            "               :DESCRIPTION5 = recorded.description)))) "
            "        ) "
            "      )" );
        result.bindValue(":TITLE", m_title);
        result.bindValue(":SUBTITLE1", null_to_empty(m_subtitle));
        result.bindValue(":SUBTITLE2", null_to_empty(m_subtitle));
        result.bindValue(":SUBTITLE3", null_to_empty(m_subtitle));
        result.bindValue(":SUBTITLE4", null_to_empty(m_subtitle));
        result.bindValue(":SUBTITLE5", null_to_empty(m_subtitle));
        result.bindValue(":SUBTITLE6", null_to_empty(m_subtitle));
        result.bindValue(":DESCRIPTION1", null_to_empty(m_description));
        result.bindValue(":DESCRIPTION2", null_to_empty(m_description));
        result.bindValue(":DESCRIPTION3", null_to_empty(m_description));
        result.bindValue(":DESCRIPTION4", null_to_empty(m_description));
        result.bindValue(":DESCRIPTION5", null_to_empty(m_description));
        result.bindValue(":PROGRAMID1", null_to_empty(m_programId));
        result.bindValue(":PROGRAMID2", null_to_empty(m_programId));
        result.bindValue(":PROGRAMID3", null_to_empty(m_programId));
        result.bindValue(":PROGRAMID4", null_to_empty(m_programId));
        result.bindValue(":PROGRAMID5", null_to_empty(m_programId));
        result.bindValue(":DUPMETHOD1", dmeth);
        result.bindValue(":DUPMETHOD2", dmeth);
        result.bindValue(":DUPMETHOD3", dmeth);

        if (!result.exec())
            MythDB::DBError("forgetRecorded2", result);
    }

    // Handle this specific entry in oldrecorded.
    result.prepare("UPDATE oldrecorded SET duplicate = 0 "
                   "WHERE station = :STATION "
                       "AND starttime = :STARTTIME "
                       "AND title = :TITLE;");
    result.bindValue(":STARTTIME", m_startTs);
    result.bindValue(":TITLE", m_title);
    result.bindValue(":STATION", m_chanSign);

    if (!result.exec())
        MythDB::DBError("forgetOldRecorded1", result);

    // Handle other matching entries in oldrecorded.
    if (din & kDupsInOldRecorded)
    {
        result.prepare(
            "UPDATE oldrecorded SET duplicate = 0 "
            "WHERE duplicate = 1 AND "
            "      title = :TITLE AND "
            "      ( "
            "        (:PROGRAMID1 <> '' AND "
            "         :PROGRAMID2 = oldrecorded.programid) "
            "        OR "
            "        ( "
            "          (:PROGRAMID3 = '' OR oldrecorded.programid = '' OR "
            "           LEFT(:PROGRAMID4, LOCATE('/', :PROGRAMID5)) <> "
            "             LEFT(oldrecorded.programid, "
            "                  LOCATE('/', oldrecorded.programid))) "
            "          AND "
            "          (((:DUPMETHOD1 & 0x02) = 0) OR (:SUBTITLE1 <> '' "
            "            AND :SUBTITLE2 = oldrecorded.subtitle)) "
            "          AND "
            "          (((:DUPMETHOD2 & 0x04) = 0) OR (:DESCRIPTION1 <> '' "
            "            AND :DESCRIPTION2 = oldrecorded.description)) "
            "          AND "
            "          (((:DUPMETHOD3 & 0x08) = 0) OR "
            "            (:SUBTITLE3 <> '' AND "
            "             (:SUBTITLE4 = oldrecorded.subtitle OR "
            "              (oldrecorded.subtitle = '' AND "
            "               :SUBTITLE5 = oldrecorded.description))) OR "
            "            (:SUBTITLE6 = '' AND :DESCRIPTION3 <> '' AND "
            "             (:DESCRIPTION4 = oldrecorded.subtitle OR "
            "              (oldrecorded.subtitle = '' AND "
            "               :DESCRIPTION5 = oldrecorded.description)))) "
            "        ) "
            "      )" );
        result.bindValue(":TITLE", m_title);
        result.bindValue(":SUBTITLE1", null_to_empty(m_subtitle));
        result.bindValue(":SUBTITLE2", null_to_empty(m_subtitle));
        result.bindValue(":SUBTITLE3", null_to_empty(m_subtitle));
        result.bindValue(":SUBTITLE4", null_to_empty(m_subtitle));
        result.bindValue(":SUBTITLE5", null_to_empty(m_subtitle));
        result.bindValue(":SUBTITLE6", null_to_empty(m_subtitle));
        result.bindValue(":DESCRIPTION1", null_to_empty(m_description));
        result.bindValue(":DESCRIPTION2", null_to_empty(m_description));
        result.bindValue(":DESCRIPTION3", null_to_empty(m_description));
        result.bindValue(":DESCRIPTION4", null_to_empty(m_description));
        result.bindValue(":DESCRIPTION5", null_to_empty(m_description));
        result.bindValue(":PROGRAMID1", null_to_empty(m_programId));
        result.bindValue(":PROGRAMID2", null_to_empty(m_programId));
        result.bindValue(":PROGRAMID3", null_to_empty(m_programId));
        result.bindValue(":PROGRAMID4", null_to_empty(m_programId));
        result.bindValue(":PROGRAMID5", null_to_empty(m_programId));
        result.bindValue(":DUPMETHOD1", dmeth);
        result.bindValue(":DUPMETHOD2", dmeth);
        result.bindValue(":DUPMETHOD3", dmeth);

        if (!result.exec())
            MythDB::DBError("forgetOldRecorded2", result);
    }

    // Remove any never records which aren't need anymore.
    result.prepare("DELETE FROM oldrecorded "
                   "WHERE recstatus = :NEVER AND duplicate = 0");
    result.bindValue(":NEVER", RecStatus::NeverRecord);

    if (!result.exec())
        MythDB::DBError("forgetNeverHistory", result);

    // Handle matching entries in oldfind.
    if (m_findId)
    {
        result.prepare("DELETE FROM oldfind WHERE "
                       "recordid = :RECORDID AND findid = :FINDID");
        result.bindValue(":RECORDID", erecid);
        result.bindValue(":FINDID", m_findId);

        if (!result.exec())
            MythDB::DBError("forgetFindHistory", result);
    }

    // The removal of an entry from oldrecorded may affect near-future
    // scheduling decisions, so recalculate
    ScheduledRecording::RescheduleCheck(*this, "ForgetHistory");
}

/** \fn RecordingInfo::SetDupHistory(void)
 *  \brief Set the duplicate flag in oldrecorded.
 */
void RecordingInfo::SetDupHistory(void)
{
    MSqlQuery result(MSqlQuery::InitCon());

    result.prepare("UPDATE oldrecorded SET duplicate = 1 "
                   "WHERE future = 0 AND duplicate = 0 "
                   "AND title = :TITLE AND "
                   "((programid = '' AND subtitle = :SUBTITLE"
                   "  AND description = :DESC) OR "
                   " (programid <> '' AND programid = :PROGRAMID) OR "
                   " (findid <> 0 AND findid = :FINDID))");
    result.bindValue(":TITLE", m_title);
    result.bindValue(":SUBTITLE", null_to_empty(m_subtitle));
    result.bindValue(":DESC", null_to_empty(m_description));
    result.bindValue(":PROGRAMID", null_to_empty(m_programId));
    result.bindValue(":FINDID", m_findId);

    if (!result.exec())
        MythDB::DBError("setDupHistory", result);

    ScheduledRecording::RescheduleCheck(*this, "SetHistory");
}

/**
 *  \brief Replace %MATCH% vars in the specified string
 *  \param str QString containing matches to be substituted
 */
void RecordingInfo::SubstituteMatches(QString &str)
{
    str.replace("%RECID%", QString::number(getRecordID()));
    str.replace("%PARENTID%", QString::number(m_parentId));
    str.replace("%FINDID%", QString::number(m_findId));
    str.replace("%RECSTATUS%", QString::number(m_recStatus));
    str.replace("%RECTYPE%", QString::number(m_recType));
    str.replace("%REACTIVATE%", IsReactivated() ? "1" : "0");
    str.replace("%INPUTNAME%", GetInputName());
    str.replace("%CHANNUM%", GetChanNum());

    ProgramInfo::SubstituteMatches(str);
}

/**
 *  \brief Temporary helper during transition from string to ID
 */
uint RecordingInfo::GetRecgroupID(const QString& recGroup)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT recgroupid FROM recgroups WHERE recgroup = :RECGROUP");
    query.bindValue(":RECGROUP", null_to_empty(recGroup));

    if (!query.exec())
        MythDB::DBError("RecGroup update", query);

    if (!query.next())
        return 0;

    return query.value(0).toUInt();
}

/**
 *  \brief Temporary helper during transition from string to ID
 */
QString RecordingInfo::GetRecgroupString(uint recGroupID)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT recgroup FROM recgroups WHERE recgroupid = :RECGROUPID");
    query.bindValue(":RECGROUPID", recGroupID);
    if (!query.exec() || !query.next())
    {
        MythDB::DBError("GetRecgroupString()", query);
        return QString();
    }
    return query.value(0).toString();
}

void RecordingInfo::LoadRecordingFile()
{
    if (!m_recordingFile)
    {
        m_recordingFile = new RecordingFile();
        if (m_recordedId > 0)
        {
            m_recordingFile->m_recordingId = m_recordedId;
            m_recordingFile->Load();
        }
    }
}

void RecordingInfo::SaveFilesize(uint64_t fsize)
{
    if (!GetRecordingFile())
        LoadRecordingFile();
    GetRecordingFile()->m_fileSize = fsize;
    GetRecordingFile()->Save(); // Ideally this would be called just the once when all metadata is gathered

    s_updater->insert(m_recordedId, kPIUpdateFileSize, fsize);

    ProgramInfo::SaveFilesize(fsize); // Temporary
}

void RecordingInfo::SetFilesize(uint64_t fsize)
{
    if (!GetRecordingFile())
        LoadRecordingFile();
    GetRecordingFile()->m_fileSize = fsize;
    s_updater->insert(m_recordedId, kPIUpdateFileSize, fsize);

    // Make sure the old storage location is updated for now
    ProgramInfo::SetFilesize(fsize);
}

uint64_t RecordingInfo::GetFilesize(void) const
{
    if (GetRecordingFile() && GetRecordingFile()->m_fileSize > 0)
        return GetRecordingFile()->m_fileSize;

    // Temporary fallback to reading from old storage location
    return ProgramInfo::GetFilesize();
}


/* vim: set expandtab tabstop=4 shiftwidth=4: */
