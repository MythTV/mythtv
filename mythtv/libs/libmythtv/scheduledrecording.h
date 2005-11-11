#ifndef SCHEDULEDRECORDING_H
#define SCHEDULEDRECORDING_H

#include "settings.h"
#include "recordingtypes.h"
#include <qdatetime.h>
#include <list>


using namespace std;

class ProgramInfo;

class RootSRGroup;
class RecOptDialog;

class SRInactive;
class SRRecordingType;
class SRRecSearchType;
class SRProfileSelector;
class SRDupIn;
class SRDupMethod;
class SRAutoTranscode;
class SRTranscoderSelector;
class SRAutoCommFlag;
class SRAutoUserJob1;
class SRAutoUserJob2;
class SRAutoUserJob3;
class SRAutoUserJob4;
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
class SRFindDay;
class SRFindTime;
class SRFindId;
class SRTimeStretch;
class SRParentId;

class ScheduledRecording: public ConfigurationGroup, public ConfigurationDialog {
    Q_OBJECT
public:
    ScheduledRecording();
    ~ScheduledRecording();
    ScheduledRecording(const ScheduledRecording& other);

    virtual void load();
    
    virtual MythDialog* dialogWidget(MythMainWindow* parent,
                                     const char* widgetName=0);    
    
    void makeOverride(void);
    const ProgramInfo* getProgramInfo() const { return m_pginfo; }
    RootSRGroup* getRootGroup() { return rootGroup; }

    RecordingType getRecordingType(void) const;
    void setRecordingType(RecordingType);
    RecSearchType getSearchType(void) const;
    void setSearchType(RecSearchType);

    int GetAutoExpire(void) const;
    void SetAutoExpire(int expire);

    int GetMaxEpisodes(void) const;
    bool GetMaxNewest(void) const;

    int GetTranscoder(void) const;

    int GetAutoRunJobs(void) const;

    void setStart(const QDateTime& start);
    void setEnd(const QDateTime& end);
    int getRecPriority(void) const;
    void setRecPriority(int recpriority);
    void setRecGroup(const QString& recgroup);

    
    virtual void save();

    virtual void loadByID(int id);
    virtual void loadByProgram(const ProgramInfo* proginfo);
    virtual void loadBySearch(RecSearchType lsearch,
                              QString textname, QString forwhat);
    virtual void loadBySearch(RecSearchType lsearch, QString textname,
                              QString from, QString forwhat);
    virtual void modifyPowerSearchByID(int rid,
                                       QString textname, QString forwhat);
    virtual void modifyPowerSearchByID(int rid, QString textname,
                                       QString from, QString forwhat);

    virtual int exec(bool saveOnExec = true, bool doLoad = false)
        { return ConfigurationDialog::exec(saveOnExec, doLoad); }    
        
    void remove();
    int getRecordID(void) const { return id->intValue(); };
    QString getProfileName(void) const;

    void findMatchingPrograms(list<ProgramInfo*>& proglist);

    // Do any necessary bookkeeping after a matching program has been
    // recorded
    void doneRecording(ProgramInfo& proginfo);

    static void fillSelections(SelectSetting* setting);

    static void signalChange(int recordid);
    // Use -1 for recordid when all recordids are potentially
    // affected, such as when the program table is updated.  
    // Use 0 for recordid when a reschdule isn't specific to a single
    // recordid, such as when a recording type priority is changed.
    
    void setInactiveObj(SRInactive* val) {inactive = val;}
    void setRecTypeObj(SRRecordingType* val) {type = val;}
    void setSearchTypeObj(SRRecSearchType* val) {search = val;}
    void setProfileObj( SRProfileSelector* val) {profile = val;}
    void setDupInObj(SRDupIn* val) {dupin = val;}
    void setDupMethodObj(SRDupMethod* val) {dupmethod = val;}
    void setAutoTranscodeObj(SRAutoTranscode* val) {autotranscode = val;}
    void setTranscoderObj(SRTranscoderSelector* val) {transcoder = val;}
    void setAutoCommFlagObj(SRAutoCommFlag* val) {autocommflag = val;}
    void setAutoUserJob1Obj(SRAutoUserJob1* val) {autouserjob1 = val;}
    void setAutoUserJob2Obj(SRAutoUserJob2* val) {autouserjob2 = val;}
    void setAutoUserJob3Obj(SRAutoUserJob3* val) {autouserjob3 = val;}
    void setAutoUserJob4Obj(SRAutoUserJob4* val) {autouserjob4 = val;}
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
    void setFindDayObj(SRFindDay* val) {findday = val;}
    void setFindTimeObj(SRFindTime* val) {findtime = val;}
    void setFindIdObj(SRFindId* val) {findid = val;}
    void setParentIdObj(SRParentId* val) {parentid = val;}
    void setTimeStretchIDObj(SRTimeStretch* val) {timestretchid = val;}
    
    void ToMap(QMap<QString, QString>& infoMap);
    
    QString ChannelText(QString format);

public slots:
    void runProgList();

protected slots:
    void runShowDetails();

protected:
    virtual void setDefault(bool haschannel);
    virtual void setProgram(const ProgramInfo *proginfo);
    void fetchChannelInfo();
    
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
    class SRInactive* inactive;
    class SRRecordingType* type;
    class SRRecSearchType* search;
    class SRProfileSelector* profile;
    class SRDupIn* dupin;
    class SRDupMethod* dupmethod;
    class SRAutoTranscode* autotranscode;
    class SRTranscoderSelector* transcoder;
    class SRAutoCommFlag* autocommflag;
    class SRAutoUserJob1* autouserjob1;
    class SRAutoUserJob2* autouserjob2;
    class SRAutoUserJob3* autouserjob3;
    class SRAutoUserJob4* autouserjob4;
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
    class SRFindDay* findday;
    class SRFindTime* findtime;
    class SRFindId* findid;
    SRTimeStretch* timestretchid;
    class SRParentId* parentid;
    
    const ProgramInfo* m_pginfo;
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
    ScheduledRecordingEditor() {};
    virtual ~ScheduledRecordingEditor() {};

    virtual int exec();
    virtual void load();
    virtual void save() { };

protected slots:
    void open(int id);

protected:
};


#endif
