#ifndef RECORDING_RULE_H
#define RECORDING_RULE_H

// QT
#include <QDateTime>
#include <QDate>
#include <QTime>

// libmythdb
#include "mythexp.h"

// libmyth
#include "programinfo.h"

// libmythtv
#include "programtypes.h"
#include "recordingtypes.h"
#include "recordingprofile.h"

/** \class RecordingRule
 *  \brief Internal representation of a recording rule, mirrors the record
 *         table
 *
 *  Please keep clean and tidy, don't let this class become a dumping ground
 *  for single use utility functions, UI related stuff or anything which has a
 *  tenuous link to recording rules. See RecordingInfo or ProgramInfo instead.
 *
 */

class MPUBLIC RecordingRule
{

  public:
    RecordingRule();
   ~RecordingRule() {};

    bool Load(void);
    bool LoadByProgram(const ProgramInfo* proginfo);
    bool LoadBySearch(RecSearchType lsearch, QString textname, QString forwhat,
                      QString from = "");

    bool ModifyPowerSearchByID(int rid, QString textname, QString forwhat,
                               QString from = "");
    bool MakeOverride(void);

    bool Save(bool sendSig = true);
    bool Delete(bool sendSig = true);

    bool IsLoaded() const { return m_loaded; }
    void UseTempTable(bool usetemp, QString table = "record_tmp");

    void ToMap(InfoMap &infoMap) const;

    AutoExpireType GetAutoExpire(void) const
        { return m_autoExpire ? kNormalAutoExpire : kDisableAutoExpire; }

    int m_recordID; /// Unique Recording Rule ID
    int m_parentRecID;

    bool m_isInactive; /// Recording rule is enabled?

    // Recording metadata
    QString m_title;
    QString m_subtitle;
    QString m_description;
    QString m_category;

    QTime m_starttime;
    QDate m_startdate;
    QTime m_endtime;
    QDate m_enddate;

    QString m_seriesid;
    QString m_programid;

    // Associated data for rule types
    int m_channelid;
    QString m_station; /// callsign?
    int m_findday; /// Day of the week for once per week etc
    QTime m_findtime; /// Time for timeslot rules
    int m_findid;

    // Scheduling Options
    RecordingType m_type;
    RecSearchType m_searchType;
    int m_recPriority;
    int m_prefInput;
    int m_startOffset;
    int m_endOffset;
    RecordingDupMethodType m_dupMethod;
    RecordingDupInType m_dupIn;

    // Storage Options
    // TODO: These should all be converted to integer IDs instead
    QString m_recProfile;
    QString m_recGroup;
    QString m_storageGroup;
    QString m_playGroup;

    bool m_autoExpire;
    int  m_maxEpisodes;
    bool m_maxNewest;

    // Post Processing Options
    bool m_autoCommFlag;
    bool m_autoTranscode;
    int m_transcoder;
    bool m_autoUserJob1;
    bool m_autoUserJob2;
    bool m_autoUserJob3;
    bool m_autoUserJob4;

    // Statistic fields - Used to generate statistics about particular rules
    // and influence watch list weighting
    QDateTime m_nextRecording;
    QDateTime m_lastRecorded;
    QDateTime m_lastDeleted;
    int m_averageDelay;

    // Temporary table related - Used for schedule previews
    QString m_recordTable;
    int m_tempID;

    // Is this an override rule? Purely for the benefit of the UI, we display
    // different options for override rules
    bool m_isOverride;

  private:
    // Populate variables from a ProgramInfo object
    void AssignProgramInfo();

    // Pointer for ProgramInfo, exists only if we loaded from ProgramInfo in
    // the first place
    const ProgramInfo *m_progInfo;

    // Internal strings for insertion into the RuleMap and display in the UI
    QString m_searchTypeString;
    QString m_searchFor;

    // Indicate that a rule has been loaded, for new rules this is still true
    // after any of the Load* methods is called
    bool m_loaded;
};

Q_DECLARE_METATYPE(RecordingRule*)

#endif
