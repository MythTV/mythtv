#ifndef SCHEDULEDRECORDING_H
#define SCHEDULEDRECORDING_H

#include "settings.h"
#include "recordingtypes.h"
#include <qdatetime.h>
#include <list>


using namespace std;

class ProgramInfo;
class QSqlDatabase;

class RootSRGroup;
class RecOptDialog;


class SRRecordingType;
class SRRecSearchType;
class SRProfileSelector;
class SRDupIn;
class SRDupMethod;
class SRAutoExpire;
class SRStartOffset;
class SREndOffset;
class SRMaxEpisodes;
class SRMaxNewest;
class SRChannel;
class SRStation;
class SRTitle;
class SRSubtitle;
class SRDescription;
class SRStartTime;
class SRStartDate;
class SREndTime;
class SREndDate;
class SRCategory;
class SRRecPriority;
class SRRecGroup;
class SRSeriesid;
class SRProgramid;

class ScheduledRecording: public ConfigurationGroup, public ConfigurationDialog {
    Q_OBJECT
public:
    ScheduledRecording();
    ScheduledRecording(const ScheduledRecording& other);

    virtual void load(QSqlDatabase* db);
    
    virtual MythDialog* dialogWidget(MythMainWindow* parent,
                                     const char* widgetName=0);    
    
    void makeOverride(void);
    ProgramInfo* getProgramInfo() { return m_pginfo; }
    RootSRGroup* getRootGroup() { return rootGroup; }

    RecordingType getRecordingType(void) const;
    void setRecordingType(RecordingType);
    RecSearchType getSearchType(void) const;

    bool GetAutoExpire(void) const;
    void SetAutoExpire(bool expire);

    int GetMaxEpisodes(void) const;
    bool GetMaxNewest(void) const;

    void setStart(const QDateTime& start);
    void setEnd(const QDateTime& end);
    void setRecPriority(int recpriority);
    void setRecGroup(const QString& recgroup);

    
    virtual void save(QSqlDatabase* db);

    virtual void loadByID(QSqlDatabase* db, int id);
    virtual void loadByProgram(QSqlDatabase* db, ProgramInfo* proginfo);
    virtual void loadBySearch(QSqlDatabase *db, RecSearchType lsearch, QString what);

    virtual int exec(QSqlDatabase* db, bool saveOnExec = true, bool doLoad = false)
        { return ConfigurationDialog::exec(db, saveOnExec, doLoad); }    
        
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

    static void fillSelections(QSqlDatabase* db, SelectSetting* setting);

    static void signalChange(QSqlDatabase* db);
    
    void setRecTypeObj(SRRecordingType* val) {type = val;}
    void setSearchTypeObj(SRRecSearchType* val) {search = val;}
    void setProfileObj( SRProfileSelector* val) {profile = val;}
    void setDupInObj(SRDupIn* val) {dupin = val;}
    void setDupMethodObj(SRDupMethod* val) {dupmethod = val;}
    void setAutoExpireObj(SRAutoExpire* val) {autoexpire = val;}
    void setStartOffsetObj(SRStartOffset* val) {startoffset = val;}
    void setEndOffsetObj(SREndOffset* val) {endoffset = val;}
    void setMaxEpisodesObj(SRMaxEpisodes* val) {maxepisodes = val;}
    void setMaxNewestObj(SRMaxNewest* val) {maxnewest = val;}
    void setChannelObj(SRChannel* val) {channel = val;}
    void setStationObj(SRStation* val) {station = val;}
    void setTitleObj(SRTitle* val) {title = val;}
    void setSubTitleObj(SRSubtitle* val) {subtitle = val;}
    void setDescriptionObj(SRDescription* val) {description = val;}
    void setStartTimeObj(SRStartTime* val) {startTime = val;}
    void setStartDateObj(SRStartDate* val) {startDate = val;}
    void setEndTimeObj(SREndTime* val) {endTime = val;}
    void setEndDateObj(SREndDate* val) {endDate = val;}
    void setCategoryObj(SRCategory* val) {category = val;}
    void setRecPriorityObj(SRRecPriority* val) {recpriority = val;}
    void setRecGroupObj(SRRecGroup* val) {recgroup = val;}
    void setSeriesIDObj(SRSeriesid* val) {seriesid = val;}
    void setProgramIDObj(SRProgramid* val) {programid = val;}
    
    void ToMap(QMap<QString, QString>& infoMap);
    
    QString ChannelText(QString format);

protected slots:
    void runProgList();
    void runShowDetails();

protected:
    virtual void setDefault(QSqlDatabase *db, bool haschannel);
    virtual void setProgram(ProgramInfo *proginfo);
    void fetchChannelInfo(QSqlDatabase *db);
    
    class ID: virtual public IntegerSetting,
              public AutoIncrementStorage 
    {
        public:
            ID()
               : AutoIncrementStorage("record", "recordid") 
            {
                setName("RecordID");
                setVisible(false);
            }
    };

    ID* id;
    class SRRecordingType* type;
    class SRRecSearchType* search;
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
    
    ProgramInfo* m_pginfo;
    QGuardedPtr<RootSRGroup> rootGroup;
    QGuardedPtr<RecOptDialog> m_dialog;
    QString chanstr;
    QString chansign;
    QString channame;
    QString searchForWhat;
    QString searchType;
    
    QString channelFormat;
    QString longChannelFormat;
    QString timeFormat;
    QString dateFormat;
    QString shortDateFormat;
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
