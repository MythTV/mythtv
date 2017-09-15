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

typedef AutoDeleteDeque<RecordingInfo*> RecordingList;

class MTV_PUBLIC RecordingInfo : public ProgramInfo
{
  public:
    RecordingInfo(void) : 
        oldrecstatus(RecStatus::Unknown),
        savedrecstatus(RecStatus::Unknown),
        future(false),
        schedorder(0),
        mplexid(0),
        sgroupid(0),
        desiredrecstartts(),
        desiredrecendts(),
        record(NULL),
        m_recordingFile(NULL) { LoadRecordingFile(); }
    RecordingInfo(const RecordingInfo &other) :
        ProgramInfo(other),
        oldrecstatus(other.oldrecstatus),
        savedrecstatus(other.savedrecstatus),
        future(other.future),
        schedorder(other.schedorder),
        mplexid(other.mplexid),
        sgroupid(other.sgroupid),
        desiredrecstartts(other.desiredrecstartts),
        desiredrecendts(other.desiredrecendts),
        record(NULL),
        m_recordingFile(NULL)  { LoadRecordingFile(); }
    RecordingInfo(const ProgramInfo &other) :
        ProgramInfo(other),
        oldrecstatus(RecStatus::Unknown),
        savedrecstatus(RecStatus::Unknown),
        future(false),
        schedorder(0),
        mplexid(0),
        sgroupid(0),
        desiredrecstartts(startts),
        desiredrecendts(endts),
        record(NULL),
        m_recordingFile(NULL)  { LoadRecordingFile(); }
    RecordingInfo(uint _recordedid) :
        ProgramInfo(_recordedid),
        oldrecstatus(RecStatus::Unknown),
        savedrecstatus(RecStatus::Unknown),
        future(false),
        schedorder(0),
        mplexid(0),
        sgroupid(0),
        desiredrecstartts(startts),
        desiredrecendts(endts),
        record(NULL),
        m_recordingFile(NULL)  { LoadRecordingFile(); }
    RecordingInfo(uint _chanid, const QDateTime &_recstartts) : /// DEPRECATED
        ProgramInfo(_chanid, _recstartts),
        oldrecstatus(RecStatus::Unknown),
        savedrecstatus(RecStatus::Unknown),
        future(false),
        schedorder(0),
        mplexid(0),
        sgroupid(0),
        desiredrecstartts(startts),
        desiredrecendts(endts),
        record(NULL),
        m_recordingFile(NULL)  { LoadRecordingFile(); }
    RecordingInfo(QStringList::const_iterator &it,
                  QStringList::const_iterator  end) :
        ProgramInfo(it, end),
        oldrecstatus(RecStatus::Unknown),
        savedrecstatus(RecStatus::Unknown),
        future(false),
        schedorder(0),
        mplexid(0),
        sgroupid(0),
        desiredrecstartts(startts),
        desiredrecendts(endts),
        record(NULL),
        m_recordingFile(NULL)  { LoadRecordingFile(); }
    /// Create RecordingInfo from 'program'+'record'+'channel' tables,
    /// used in scheduler.cpp @ ~ 3296
    RecordingInfo(
        const QString &title,
        const QString &subtitle,
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
        uint cardid,

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
        const QString &subtitle,
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
    typedef enum {
        kNoProgram           = 0,
        kFoundProgram        = 1,
        kFakedLiveTVProgram  = 2,
        kFakedZeroMinProgram = 3,
    } LoadStatus;
    RecordingInfo(uint _chanid, const QDateTime &desiredts,
                  bool genUnknown, uint maxHours = 0,
                  LoadStatus *status = NULL);

    typedef enum {
        kDefaultRecGroup     = 1, // Auto-increment columns start at one
        kLiveTVRecGroup      = 2,
        kDeletedRecGroup     = 3,
    } SpecialRecordingGroups;

  public:
    RecordingInfo &operator=(const RecordingInfo &other)
        { clone(other); return *this; }
    RecordingInfo &operator=(const ProgramInfo &other)
        { clone(other); return *this; }
    virtual void clone(const RecordingInfo &other,
                       bool ignore_non_serialized_data = false);
    virtual void clone(const ProgramInfo &other,
                       bool ignore_non_serialized_data = false);

    virtual void clear(void);

    // Destructor
    virtual ~RecordingInfo();

    // Serializers
    virtual void SubstituteMatches(QString &str);

    void SetRecordingID(uint _recordedid) {  recordedid = _recordedid;
                                             m_recordingFile->m_recordingId = _recordedid; }

    // Quick gets
    /// Creates a unique string that can be used to identify a
    /// scheduled recording.
    QString MakeUniqueSchedulerKey(void) const
        { return MakeUniqueKey(chanid, startts); }

    // Used to query and set RecordingRule info
    RecordingRule *GetRecordingRule(void);
    int getRecordID(void);
    int GetAutoRunJobs(void) const;
    RecordingType GetProgramRecordingStatus(void);
    QString GetProgramRecordingProfile(void) const;
    void ApplyRecordStateChange(RecordingType newstate, bool save = true);
    void ApplyRecordRecPriorityChange(int);
    void QuickRecord(void);

    // Used in determining start and end for RecordingQuality determination
    void SetDesiredStartTime(const QDateTime &dt) { desiredrecstartts = dt; }
    void SetDesiredEndTime(const QDateTime &dt) { desiredrecendts = dt; }
    QDateTime GetDesiredStartTime(void) const { return desiredrecstartts; }
    QDateTime GetDesiredEndTime(void) const { return desiredrecendts; }

    // these five can be moved to programinfo
    void AddHistory(bool resched = true, bool forcedup = false, 
                    bool future = false);//pi
    void DeleteHistory(void);//pi
    void ForgetHistory(void);//pi
    void SetDupHistory(void);//pi

    // Used to update database with recording info
    void StartedRecording(QString ext);
    void FinishedRecording(bool allowReRecord);
    void UpdateRecordingEnd(void);//pi
    void ReactivateRecording(void);//pi
    void ApplyRecordRecID(void);//pi
    void ApplyRecordRecGroupChange(const QString &newrecgroup);
    void ApplyRecordRecGroupChange(int newrecgroupid);
    void ApplyRecordPlayGroupChange(const QString &newrecgroup);
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
    void SaveFilesize(uint64_t fsize);   /// Will replace the one in ProgramInfo
    void SetFilesize( uint64_t sz );     /// Will replace the one in ProgramInfo
    uint64_t GetFilesize(void) const; /// Will replace the one in ProgramInfo

    RecStatus::Type oldrecstatus;
    RecStatus::Type savedrecstatus;
    bool future;
    int schedorder;
    uint mplexid; // Only valid within the scheduler
    uint sgroupid; // Only valid within the scheduler
    QDateTime desiredrecstartts;
    QDateTime desiredrecendts;

  private:
    mutable class RecordingRule *record;
    RecordingFile *m_recordingFile;

  protected:
    static bool InsertProgram(RecordingInfo *pg,
                              const RecordingRule *rule);

    static QString unknownTitle;
};

Q_DECLARE_METATYPE(RecordingInfo*)
Q_DECLARE_METATYPE(RecordingInfo)

#endif // _RECORDING_INFO_H_

/* vim: set expandtab tabstop=4 shiftwidth=4: */
