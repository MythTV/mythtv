#ifndef SCHEDULEDRECORDING_H
#define SCHEDULEDRECORDING_H

#include "settings.h"
#include <qdatetime.h>

#include <list>
using namespace std;

enum RecordingType
{
    kNotRecording = 0,
    kSingleRecord = 1,
    kTimeslotRecord,
    kChannelRecord,
    kAllRecord,
    kWeekslotRecord,
    kFindOneRecord,
    kOverrideRecord,
    kDontRecord
};

int RecTypePriority(RecordingType rectype);

enum RecordingDupInType
{
    kDupsInRecorded     = 0x01,
    kDupsInOldRecorded  = 0x02,
    kDupsInAll          = 0x0F
};


enum RecordingDupMethodType
{
    kDupCheckNone     = 0x01,
    kDupCheckSub      = 0x02,
    kDupCheckDesc     = 0x04,
    kDupCheckSubDesc  = 0x06,
    kDupCheckIdOnly   = 0x08
};

class ProgramInfo;
class QSqlDatabase;
class ScheduledRecording: public ConfigurationGroup, public ConfigurationDialog {
    Q_OBJECT
public:
    ScheduledRecording();
    //ScheduledRecording(const ScheduledRecording& other);

    virtual void load(QSqlDatabase* db);
    void fromProgramInfo(ProgramInfo* proginfo, QSqlDatabase *db);
    void makeOverride(ProgramInfo* proginfo);

    RecordingType getRecordingType(void) const;
    void setRecordingType(RecordingType);

    bool GetAutoExpire(void) const;
    void SetAutoExpire(bool expire);

    int GetMaxEpisodes(void) const;
    bool GetMaxNewest(void) const;

    void setStart(const QDateTime& start);
    void setEnd(const QDateTime& end);
    void setRecPriority(int recpriority);
    void setRecGroup(const QString& recgroup);

    virtual MythDialog* dialogWidget(MythMainWindow *parent, 
                                     const char *name = 0);
    virtual void save(QSqlDatabase* db);

    void loadByID(QSqlDatabase* db, int id);
    bool loadByProgram(QSqlDatabase* db, ProgramInfo* proginfo);

    void remove(QSqlDatabase* db);
    int getRecordID(void) const { return id->intValue(); };
    QString getProfileName(void) const;

    void findMatchingPrograms(QSqlDatabase* db, list<ProgramInfo*>& proglist);

    // Do any necessary bookkeeping after a matching program has been
    // recorded
    void doneRecording(QSqlDatabase* db, const ProgramInfo& proginfo);

    // Remember a program has been reccorded before (means it won't
    // be recorded again)
    void addHistory(QSqlDatabase* db, const ProgramInfo& proginfo);

    // Forget whether a program has been recorded before (allows it to
    // be recorded again)
    void forgetHistory(QSqlDatabase* db, const ProgramInfo& proginfo);

    static bool hasChanged(QSqlDatabase* db);

    static void ScheduledRecording::fillSelections(QSqlDatabase* db, SelectSetting* setting);

    static void signalChange(QSqlDatabase* db);

protected slots:
    void setAvailableOptions(const QString& newValue)
                             { setAvailableOptions(); };
    void setAvailableOptions(int newValue)
                             { setAvailableOptions(); };
    void setAvailableOptions(void);
    void runProgList();

private:
    class ID: virtual public IntegerSetting,
              public AutoIncrementStorage {
    public:
        ID():
            AutoIncrementStorage("record", "recordid") {
            setName("RecordID");
            setVisible(false);
        };
    };

    ID* id;
    class SRRecordingType* type;
    class SRProfileSelector* profile;
    class SRDupIn* dupin;
    class SRDupMethod* dupmethod;
    class SRAutoExpire* autoexpire;
    class SRStartOffset* startoffset;
    class SREndOffset* endoffset;
    class SRMaxEpisodes* maxepisodes;
    class SRMaxNewest* maxnewest;
    class SRChannel* channel;
    class SRStation* station;
    class SRTitle* title;
    class SRSubtitle* subtitle;
    class SRDescription* description;
    class SRStartTime* startTime;
    class SRStartDate* startDate;
    class SREndTime* endTime;
    class SREndDate* endDate;
    class SRCategory* category;
    class SRRecPriority* recpriority;
    class SRRecGroup* recgroup;
    class SRSeriesid* seriesid;
    class SRProgramid* programid;

    QWidget *typeWidget;
    QWidget *profileWidget;
    QWidget *recpriorityWidget;
    QWidget *autoexpireWidget;
    QWidget *recgroupWidget;
    QWidget *startoffsetWidget;
    QWidget *endoffsetWidget;
    QWidget *maxepisodesWidget;
    QWidget *maxnewestWidget;
    QWidget *dupinWidget;
    QWidget *dupmethodWidget;
    MythPushButton *proglistButton;

    ProgramInfo* m_pginfo;
};

class ScheduledRecordingEditor: public ListBoxSetting, public ConfigurationDialog {
    Q_OBJECT
public:
    ScheduledRecordingEditor(QSqlDatabase* _db):
        db(_db) {};
    virtual ~ScheduledRecordingEditor() {};

    virtual int exec(QSqlDatabase* db);
    virtual void load(QSqlDatabase* db);
    virtual void save(QSqlDatabase* db) { (void)db; };

protected slots:
    void open(int id);

protected:
    QSqlDatabase* db;
};

#endif
