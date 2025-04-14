#ifndef RECORDING_RULE_H
#define RECORDING_RULE_H

// QT
#include <QDateTime>
#include <QDate>
#include <QTime>
#include <QStringList>
#include <QCoreApplication>

// MythTV
#include "libmythbase/programinfo.h"
#include "libmythbase/programtypes.h"
#include "libmythbase/recordingtypes.h"
#include "libmythtv/mythtvexp.h"
#include "libmythtv/recordinginfo.h"
#include "libmythtv/recordingprofile.h"

/** \class RecordingRule
 *  \brief Internal representation of a recording rule, mirrors the record
 *         table
 *
 *  Please keep clean and tidy, don't let this class become a dumping ground
 *  for single use utility functions, UI related stuff or anything which has a
 *  tenuous link to recording rules. See RecordingInfo or ProgramInfo instead.
 *
 */

class MTV_PUBLIC RecordingRule
{
    Q_DECLARE_TR_FUNCTIONS(RecordingRule);

  public:
    static const int kNumFilters = 16;

    RecordingRule();
   ~RecordingRule() = default;

    void ensureSortFields(void);
    bool Load(bool asTemplate = false);
    bool LoadByProgram(const ProgramInfo* proginfo);
    bool LoadBySearch(RecSearchType lsearch, const QString& textname, const QString& forwhat,
                      QString joininfo = "", ProgramInfo *pginfo = nullptr);
    bool LoadTemplate(const QString& title,
                      const QString& category = "Default",
                      const QString& categoryType = "Default");

    bool ModifyPowerSearchByID(int rid, const QString& textname, QString forwhat,
                               QString joininfo = "");
    bool MakeOverride(void);
    bool MakeTemplate(QString category);

    bool Save(bool sendSig = true);
    bool Delete(bool sendSig = true);

    bool IsLoaded() const { return m_loaded; }
    void UseTempTable(bool usetemp, const QString& table = "record_tmp");
    static unsigned GetDefaultFilter(void);

    void ToMap(InfoMap &infoMap, uint date_format = 0) const;

    AutoExpireType GetAutoExpire(void) const
        { return m_autoExpire ? kNormalAutoExpire : kDisableAutoExpire; }

    bool IsValid(QString &msg) const;

    static QString SearchTypeToString(RecSearchType searchType);
    static QStringList GetTemplateNames(void);

    /// Unique Recording Rule ID
    int                    m_recordID           {-1};
    int                    m_parentRecID        {0};

    /// Recording rule is enabled?
    bool                   m_isInactive         {false};

    // Recording metadata
    QString                m_title;
    QString                m_sortTitle;
    QString                m_subtitle;
    QString                m_sortSubtitle;
    QString                m_description;
    QString                m_category;

    QString                m_seriesid;
    QString                m_programid;
    // String could be null when we trying to insert into DB
    QString                m_inetref;

    QDate                  m_startdate;
    QDate                  m_enddate;
    QTime                  m_starttime;
    QTime                  m_endtime;

    uint                   m_season             {0};
    uint                   m_episode            {0};

    // Associated data for rule types
    QString                m_station;     /// callsign?
    int                    m_channelid          {0};
    /// Time for timeslot rules
    QTime                  m_findtime;
    int                    m_findid;
    /// Day of the week for once per week etc
    int                    m_findday            {0};

    // Scheduling Options
    int                    m_recPriority        {0};
    int                    m_prefInput          {0};
    int                    m_startOffset        {0};
    int                    m_endOffset          {0};
    RecordingType          m_type               {kNotRecording};
    RecSearchType          m_searchType         {kNoSearch};
    RecordingDupMethodType m_dupMethod          {kDupCheckSubThenDesc};
    RecordingDupInType     m_dupIn              {kDupsInAll};
    unsigned               m_filter             {0};
    AutoExtendType         m_autoExtend         {AutoExtendType::None};

    // Storage Options
    // TODO: These should all be converted to integer IDs instead
    QString                m_recProfile         {tr("Default")};
    QString                m_storageGroup       {tr("Default")};
    QString                m_playGroup          {tr("Default")};
    uint                   m_recGroupID         {RecordingInfo::kDefaultRecGroup};

    int                    m_maxEpisodes        {0};
    bool                   m_autoExpire         {false};
    bool                   m_maxNewest          {false};

    // Post Processing Options
    int                    m_transcoder{RecordingProfile::kTranscoderAutodetect};
    bool                   m_autoCommFlag       {true};
    bool                   m_autoTranscode      {false};
    bool                   m_autoUserJob1       {false};
    bool                   m_autoUserJob2       {false};
    bool                   m_autoUserJob3       {false};
    bool                   m_autoUserJob4       {false};
    bool                   m_autoMetadataLookup {true};

    // Statistic fields - Used to generate statistics about particular rules
    // and influence watch list weighting
    int                    m_averageDelay       {100};
    QDateTime              m_nextRecording;
    QDateTime              m_lastRecorded;
    QDateTime              m_lastDeleted;

    // Temporary table related - Used for schedule previews
    QString                m_recordTable        {"record"};
    int                    m_tempID             {0};

    // Is this an override rule? Purely for the benefit of the UI, we display
    // different options for override rules
    bool                   m_isOverride         {false};

    // Is this a template rule?  Purely for the benefit of the UI, we
    // display all options for template rules
    bool                   m_isTemplate         {false};

  private:
    // Populate variables from a ProgramInfo object
    void AssignProgramInfo();

    // Name of template used for new rule.
    QString                m_template;

    // Pointer for ProgramInfo, exists only if we loaded from ProgramInfo in
    // the first place
    const ProgramInfo     *m_progInfo           {nullptr};

    // Indicate that a rule has been loaded, for new rules this is still true
    // after any of the Load* methods is called
    bool                   m_loaded             {false};
};

Q_DECLARE_METATYPE(RecordingRule*)

#endif
