#ifndef _RECORDING_INFO_H_
#define _RECORDING_INFO_H_

#include <QDateTime>
#include <QString>

#include "mythtvexp.h"
#include "programinfo.h"
#include "recordingfile.h"
#include "enums/recStatus.h"

class RecordingRule;

/** \class RecordingInfo
 *  \brief Holds information on a %TV Program one might wish to record.
 *
 *  This class exxtends ProgramInfo with additional information about
 *  scheduled recordings and contains helper methods to aid in the
 *  scheduling of recordings.
 */

// Note: methods marked with "//pi" could be moved to ProgramInfo without
// breaking linkage or adding new classes to libmyth. For some of them
// RecordingRule::signalChange would need to be moved to remoteutil.{cpp,h},
// but that is a static method which is fairly easy to move.
// These methods are in RecordingInfo because it currently makes sense
// for them to be in this class in terms of related functions being here.

class RecordingInfo;
class RecordingRule;

using RecordingList = AutoDeleteDeque<RecordingInfo*>;

class MTV_PUBLIC RecordingInfo : public ProgramInfo
{
  public:
    RecordingInfo(void) { LoadRecordingFile(); }
    RecordingInfo(const RecordingInfo &other) :
        ProgramInfo(other),
        m_oldrecstatus(other.m_oldrecstatus),
        m_savedrecstatus(other.m_savedrecstatus),
        m_future(other.m_future),
        m_schedorder(other.m_schedorder),
        m_mplexid(other.m_mplexid),
        m_sgroupid(other.m_sgroupid),
        m_desiredrecstartts(other.m_desiredrecstartts),
        m_desiredrecendts(other.m_desiredrecendts)  { LoadRecordingFile(); }
    explicit RecordingInfo(const ProgramInfo &other) :
        ProgramInfo(other),
        m_desiredrecstartts(m_startts),
        m_desiredrecendts(m_endts)  { LoadRecordingFile(); }
    explicit RecordingInfo(uint _recordedid) :
        ProgramInfo(_recordedid),
        m_desiredrecstartts(m_startts),
        m_desiredrecendts(m_endts)  { LoadRecordingFile(); }
    RecordingInfo(uint _chanid, const QDateTime &_recstartts) : /// DEPRECATED
        ProgramInfo(_chanid, _recstartts),
        m_desiredrecstartts(m_startts),
        m_desiredrecendts(m_endts)  { LoadRecordingFile(); }
    RecordingInfo(QStringList::const_iterator &it,
                  QStringList::const_iterator  end) :
        ProgramInfo(it, end),
        m_desiredrecstartts(m_startts),
        m_desiredrecendts(m_endts)  { LoadRecordingFile(); }
    /// Create RecordingInfo from 'program'+'record'+'channel' tables,
    /// used in scheduler.cpp @ ~ 3296
    RecordingInfo(
        const QString &title,
        const QString &sortTitle,
        const QString &subtitle,
        const QString &sortSubtitle,
        const QString &description,
        uint season,
        uint episode,
        uint totalepisodes,
        const QString &syndicatedepisode,
        const QString &category,

        uint chanid,
        const QString &chanstr,
        const QString &chansign,
        const QString &channame,

        const QString &recgroup,
        const QString &playgroup,

        const QString &hostname,
        const QString &storagegroup,

        uint year,
        uint partnumber,
        uint parttotal,

        const QString &seriesid,
        const QString &programid,
        const QString &inetref,
        const CategoryType catType,

        int recpriority,

        const QDateTime &startts,
        const QDateTime &endts,
        const QDateTime &recstartts,
        const QDateTime &recendts,

        float stars,
        const QDate &originalAirDate,

        bool repeat,

        RecStatus::Type oldrecstatus,
        bool reactivate,

        uint recordid,
        uint parentid,
        RecordingType rectype,
        RecordingDupInType dupin,
        RecordingDupMethodType dupmethod,

        uint sourceid,
        uint inputid,

        uint findid,

        bool commfree,
        uint subtitleType,
        uint videoproperties,
        uint audioproperties,
        bool future,
        int schedorder,
        uint mplexid,
        uint sgroupid,
        const QString &inputname);

    /// Create RecordingInfo from 'record'+'channel' tables,
    /// user in scheduler.cpp  @ ~ 3566 & ~ 3643
    RecordingInfo(
        const QString &title,
        const QString &sortTitle,
        const QString &subtitle,
        const QString &sortSubtitle,
        const QString &description,
        uint season,
        uint episode,
        const QString &category,

        uint chanid,
        const QString &chanstr,
        const QString &chansign,
        const QString &channame,

        const QString &recgroup,
        const QString &playgroup,

        const QString &seriesid,
        const QString &programid,
        const QString &inetref,

        int recpriority,

        const QDateTime &startts,
        const QDateTime &endts,
        const QDateTime &recstartts,
        const QDateTime &recendts,

        RecStatus::Type recstatus,

        uint recordid,
        RecordingType rectype,
        RecordingDupInType dupin,
        RecordingDupMethodType dupmethod,

        uint findid,

        bool commfree);

    // Create ProgramInfo that overlaps the desired time on the
    // specified channel id.
    enum LoadStatus {
        kNoProgram           = 0,
        kFoundProgram        = 1,
        kFakedLiveTVProgram  = 2,
        kFakedZeroMinProgram = 3,
    };
    RecordingInfo(uint _chanid, const QDateTime &desiredts,
                  bool genUnknown, uint maxHours = 0,
                  LoadStatus *status = nullptr);

    enum SpecialRecordingGroups {
        kDefaultRecGroup     = 1, // Auto-increment columns start at one
        kLiveTVRecGroup      = 2,
        kDeletedRecGroup     = 3,
    };

  public:
    RecordingInfo &operator=(const RecordingInfo &other)
        { RecordingInfo::clone(other); return *this; }
    RecordingInfo &operator=(const ProgramInfo &other)
        { RecordingInfo::clone(other); return *this; }
    virtual void clone(const RecordingInfo &other,
                       bool ignore_non_serialized_data = false);
    void clone(const ProgramInfo &other,
               bool ignore_non_serialized_data = false) override; // ProgramInfo

    void clear(void) override; // ProgramInfo

    // Destructor
    virtual ~RecordingInfo();

    // Serializers
    void SubstituteMatches(QString &str) override; // ProgramInfo

    void SetRecordingID(uint _recordedid) override // ProgramInfo
        {  m_recordedid = _recordedid;
            m_recordingFile->m_recordingId = _recordedid; }

    // Quick gets
    /// Creates a unique string that can be used to identify a
    /// scheduled recording.
    QString MakeUniqueSchedulerKey(void) const
        { return MakeUniqueKey(m_chanid, m_startts); }

    // Used to query and set RecordingRule info
    RecordingRule *GetRecordingRule(void);
    int getRecordID(void);
    static bool QueryRecordedIdForKey(int & recordedid,
                                      uint chanid, const QDateTime& recstartts);
    int GetAutoRunJobs(void) const;
    RecordingType GetProgramRecordingStatus(void);
    QString GetProgramRecordingProfile(void) const;
    void ApplyRecordStateChange(RecordingType newstate, bool save = true);
    void ApplyRecordRecPriorityChange(int);
    void QuickRecord(void);

    // Used in determining start and end for RecordingQuality determination
    void SetDesiredStartTime(const QDateTime &dt) { m_desiredrecstartts = dt; }
    void SetDesiredEndTime(const QDateTime &dt) { m_desiredrecendts = dt; }
    QDateTime GetDesiredStartTime(void) const { return m_desiredrecstartts; }
    QDateTime GetDesiredEndTime(void) const { return m_desiredrecendts; }

    // these five can be moved to programinfo
    void AddHistory(bool resched = true, bool forcedup = false,
                    bool future = false);//pi
    void DeleteHistory(void);//pi
    void ForgetHistory(void);//pi
    void SetDupHistory(void);//pi

    // Used to update database with recording info
    void StartedRecording(const QString& ext);
    void FinishedRecording(bool allowReRecord);
    void UpdateRecordingEnd(void);//pi
    void ReactivateRecording(void);//pi
    void ApplyRecordRecID(void);//pi
    void ApplyRecordRecGroupChange(const QString &newrecgroup);
    void ApplyRecordRecGroupChange(int newrecgroupid);
    void ApplyRecordPlayGroupChange(const QString &newplaygroup);
    void ApplyStorageGroupChange(const QString &newstoragegroup);
    void ApplyRecordRecTitleChange(const QString &newTitle,
                                   const QString &newSubtitle,
                                   const QString &newDescription);
    void ApplyTranscoderProfileChange(const QString &profile) const;//pi
    void ApplyTranscoderProfileChangeById(int);
    void ApplyNeverRecord(void);

    // Temporary while we transition from string to integer
    static QString GetRecgroupString(uint recGroupID);
    static uint GetRecgroupID(const QString &recGroup);

    // File specific metdata
    void LoadRecordingFile();
    RecordingFile *GetRecordingFile() const { return m_recordingFile; }
    void SaveFilesize(uint64_t fsize) override; // ProgramInfo
    void SetFilesize( uint64_t fsize ) override; // ProgramInfo
    uint64_t GetFilesize(void) const override; // ProgramInfo

    RecStatus::Type m_oldrecstatus      {RecStatus::Unknown};
    RecStatus::Type m_savedrecstatus    {RecStatus::Unknown};
    bool            m_future            {false};
    int             m_schedorder        {0};
    uint            m_mplexid           {0}; // Only valid within the scheduler
    uint            m_sgroupid          {0}; // Only valid within the scheduler
    QDateTime       m_desiredrecstartts;
    QDateTime       m_desiredrecendts;

  private:
    mutable class RecordingRule *m_record        {nullptr};
    RecordingFile               *m_recordingFile {nullptr};

  protected:
    static bool InsertProgram(RecordingInfo *pg,
                              const RecordingRule *rule);

    static QString s_unknownTitle;
};

Q_DECLARE_METATYPE(RecordingInfo*)
Q_DECLARE_METATYPE(RecordingInfo)

#endif // _RECORDING_INFO_H_

/* vim: set expandtab tabstop=4 shiftwidth=4: */
