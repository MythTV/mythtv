#ifndef _RECORDING_INFO_H_
#define _RECORDING_INFO_H_

#include "programinfo.h"

class RecordingRule;

/** \class RecordingInfo
 *  \brief Holds information on a %TV Program one might wish to record.
 *
 *  This class exxtends ProgramInfo with additional information about scheduled
 *  recordings and contains helper methods to aid in the scheduling of recordings.
 */

// Note: methods marked with "//pi" could be moved to ProgramInfo without
// breaking linkage or adding new classes to libmyth. For some of them
// RecordingRule::signalChange would need to be moved to remoteutil.{cpp,h},
// but that is a static method which is fairly easy to move.
// These methods are in RecordingInfo because it currently makes sense
// for them to be in this class in terms of related functions being here.

class MPUBLIC RecordingInfo : public ProgramInfo
{
  public:
    RecordingInfo(void) : record(NULL) {}
    RecordingInfo(const RecordingInfo &other) :
        ProgramInfo(other), record(NULL) {}
    RecordingInfo(const ProgramInfo &other) :
        ProgramInfo(other), record(NULL) {}
    RecordingInfo &operator=(const RecordingInfo &other) {return clone(other);}
    RecordingInfo &operator=(const ProgramInfo &other) { return clone(other); }

    virtual RecordingInfo &clone(const ProgramInfo &other);
    virtual void clear(void);

    // Destructor
    virtual ~RecordingInfo();

    // Serializers
    virtual void ToMap(QHash<QString, QString> &progMap,
                       bool showrerecord = false) const;

    // Used to query and set RecordingRule info
    RecordingRule *GetRecordingRule(void);
    int getRecordID(void);
    int GetAutoRunJobs(void) const;
    RecordingType GetProgramRecordingStatus(void);
    QString GetProgramRecordingProfile(void) const;
    void ApplyRecordStateChange(RecordingType newstate, bool save = true);
    void ApplyRecordRecPriorityChange(int);
    void ToggleRecord(void);

    // these five can be moved to programinfo
    void AddHistory(bool resched = true, bool forcedup = false);//pi
    void DeleteHistory(void);//pi
    void ForgetHistory(void);//pi
    void SetDupHistory(void);//pi

    // Used to update database with recording info
    void StartedRecording(QString ext);
    void FinishedRecording(bool prematurestop);
    void UpdateRecordingEnd(void);//pi
    void ReactivateRecording(void);//pi
    void ApplyRecordRecID(void);//pi
    void ApplyRecordRecGroupChange(const QString &newrecgroup);
    void ApplyRecordPlayGroupChange(const QString &newrecgroup);
    void ApplyStorageGroupChange(const QString &newstoragegroup);
    void ApplyRecordRecTitleChange(const QString &newTitle,
                                   const QString &newSubtitle);
    void ApplyTranscoderProfileChange(const QString &profile) const;//pi

    static void signalChange(int recordid);

  private:
    mutable class RecordingRule *record;
};

Q_DECLARE_METATYPE(RecordingInfo*)
Q_DECLARE_METATYPE(RecordingInfo)

#endif // _RECORDING_INFO_H_

/* vim: set expandtab tabstop=4 shiftwidth=4: */
