#ifndef SCHEDULEDRECORDING_H
#define SCHEDULEDRECORDING_H

#include "libmyth/settings.h"
#include <qdatetime.h>

class ProgramInfo;
class ScheduledRecording: public ConfigurationGroup, public ConfigurationDialog {
public:
    enum RecordingType
    {
        NotRecording = 0,
        SingleRecord = 1,
        TimeslotRecord,
        ChannelRecord,
        AllRecord
    };

    ScheduledRecording();
    //ScheduledRecording(const ScheduledRecording& other);

    void fromProgramInfo(const ProgramInfo& proginfo);

    RecordingType getRecordingType(void) const;
    void setRecordingType(RecordingType);

    void setStart(const QDateTime& start);
    void setEnd(const QDateTime& end);

    virtual MythDialog* dialogWidget(MythContext* context,
                                  QWidget* parent=NULL, const char* name=0);
    virtual void save(QSqlDatabase* db);

    void loadByID(QSqlDatabase* db, int id);
    bool loadByProgram(QSqlDatabase* db, const ProgramInfo& proginfo);

    void remove(QSqlDatabase* db);
    int getRecordID(void) const { return id->intValue(); };
    int getProfileID(void) const;

    static void findAllProgramsToRecord(QSqlDatabase* db, list<ProgramInfo*>& proglist);
    void findMatchingPrograms(QSqlDatabase* db, list<ProgramInfo*>& proglist);

    // Do any necessary bookkeeping after a matching program has been
    // recorded
    void doneRecording(QSqlDatabase* db, const ProgramInfo& proginfo);

    static bool hasChanged(QSqlDatabase* db);

    static void ScheduledRecording::fillSelections(QSqlDatabase* db, SelectSetting* setting);

protected:
    static void signalChange(QSqlDatabase* db);

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
    class SRChannel* channel;
    class SRTitle* title;
    class SRSubtitle* subtitle;
    class SRDescription* description;
    class SRStartTime* startTime;
    class SRStartDate* startDate;
    class SREndTime* endTime;
    class SREndDate* endDate;
};

class ScheduledRecordingEditor: public ListBoxSetting, public ConfigurationDialog {
    Q_OBJECT
public:
    ScheduledRecordingEditor(QSqlDatabase* _db):
        db(_db) {};
    virtual ~ScheduledRecordingEditor() {};

    virtual int exec(MythContext* context, QSqlDatabase* db);
    virtual void load(QSqlDatabase* db);
    virtual void save(QSqlDatabase* db) { (void)db; };

protected slots:
    void open(int id);

protected:
    MythContext* m_context;
    QSqlDatabase* db;
};

#endif
