#ifndef SR_ITEMS_H
#define SR_ITEMS_H
#include <qsqldatabase.h>
#include "mythcontext.h"
#include "managedlist.h"
#include "scheduledrecording.h"
#include "recordingprofile.h"


class SimpleSRSetting: public SimpleDBStorage 
{
    protected:
        SimpleSRSetting(ScheduledRecording& _parent, QString name)
                      : SimpleDBStorage("record", name),
                        parent(_parent) 
       {
            parent.addChild(this);
            setName(name);
        }
        
        virtual QString setClause(void) 
        {
            QString value = getValue();
            value.replace(QRegExp("\""), QString("\\\""));
    
            return QString("recordid = %1, %2 = \"%3\"")
                .arg(parent.getRecordID())
                .arg(getColumn())
                .arg(value.utf8());
        }
    
        virtual QString whereClause(void) 
        {
            return QString("recordid = %1").arg(parent.getRecordID());
        }
        
        ScheduledRecording& parent;
};


class SRSetting: public ManagedListSetting 
{
    protected:
        SRSetting(ScheduledRecording& _parent, QString name, ManagedList* _parentList=NULL)
            : ManagedListSetting("record", name, _parentList),
              parent(_parent) 
        {
            _parent.addChild(this);
            setName(name);
        }
        
        virtual QString setClause(void) {
            QString value = getValue();
            value.replace(QRegExp("\""), QString("\\\""));
        
            return QString("recordid = %1, %2 = \"%3\"")
                .arg(parent.getRecordID())
                .arg(getColumn())
                .arg(value.utf8());
        };
        
        virtual QString whereClause(void) {
            return QString("recordid = %1").arg(parent.getRecordID());
            }
        
        ScheduledRecording& parent;
};



class SRSelectSetting : public SelectManagedListSetting 
{
    protected:
        SRSelectSetting(ScheduledRecording& _parent, const QString& listName,  const QString& listText, 
                        ManagedListGroup* _group, QString _column, ManagedList* _parentList=NULL)
            : SelectManagedListSetting(listName, listText, _group, "record", _column, _parentList),
              parent(_parent) 
        {
            _parent.addChild(this);
            setName(_column);
        }
        virtual QString setClause(void) {
            QString value = getValue();
            value.replace(QRegExp("\""), QString("\\\""));
        
            return QString("recordid = %1, %2 = \"%3\"")
                .arg(parent.getRecordID())
                .arg(getColumn())
                .arg(value.utf8());
        };
        
        virtual QString whereClause(void) {
            return QString("recordid = %1").arg(parent.getRecordID());
            }
        ScheduledRecording& parent;
};

class SRBoolSetting : public BoolManagedListSetting
{
    public:
        SRBoolSetting(ScheduledRecording& _parent, const QString& trueText, const QString& falseText, 
                      const QString& ItemName, QString _column, ManagedList* _parentList=NULL)
            : BoolManagedListSetting(trueText, falseText, ItemName, "record", _column, _parentList),
              parent(_parent) 
            
        {
            _parent.addChild(this);
            setName(_column);
        }
        
        virtual QString setClause(void) {
            QString value = getValue();
            value.replace(QRegExp("\""), QString("\\\""));
        
            return QString("recordid = %1, %2 = \"%3\"")
                .arg(parent.getRecordID())
                .arg(getColumn())
                .arg(value.utf8());
        };
        
        virtual QString whereClause(void) {
            return QString("recordid = %1").arg(parent.getRecordID());
            }
        ScheduledRecording& parent;
};


class SRBoundedIntegerSetting : public BoundedIntegerManagedListSetting
{
    public:
        SRBoundedIntegerSetting(int _min, int _max, int _step, ScheduledRecording& _parent,
                                const QString& ItemName, QString _column, ManagedList* _parentList=NULL)
            : BoundedIntegerManagedListSetting(_min, _max, _step, ItemName, "record", _column, _parentList),
              parent(_parent) 
            
        {
            _parent.addChild(this);
            setName(_column);
        }
        virtual QString setClause(void) 
        {
            QString value = getValue();
            value.replace(QRegExp("\""), QString("\\\""));
        
            return QString("recordid = %1, %2 = \"%3\"")
                .arg(parent.getRecordID())
                .arg(getColumn())
                .arg(value.utf8());
        };
        
        virtual QString whereClause(void) 
        {
            return QString("recordid = %1").arg(parent.getRecordID());
        }
        
        ScheduledRecording& parent;
};


class SRRecordingType : public SRSelectSetting
{
    public:
        SRRecordingType(ScheduledRecording& _parent, ManagedList* _list, ManagedListGroup* _group)
            : SRSelectSetting(_parent, "typeList", "Select recording schedule...", _group, "type", _list) 
        {
            _parent.setRecTypeObj(this);
        }
    
    
        void addNormalSelections(bool haschannel) 
        {
            addSelection("Do not record this program", kNotRecording);
            
            if (haschannel)
                addSelection("Record only this showing", kSingleRecord);
                
            addSelection("Record one showing of this program", kFindOneRecord);
            
            if (haschannel)
            {
                addSelection("Record in this timeslot every week", kWeekslotRecord);
                addSelection("Record in this timeslot every day",  kTimeslotRecord);
                addSelection("Record at any time on this channel", kChannelRecord);
            }
            
           addSelection("Record at any time on any channel", kAllRecord);
        }
    
        void addOverrideSelections(void) 
        {
            addSelection("Record this showing with normal options", kNotRecording);
            addSelection("Record this showing with override options", kOverrideRecord);
            addSelection("Do not record this showing", kDontRecord);
        }
        
        
       
};

class SRStartOffset : public SRBoundedIntegerSetting 
{
    public:
        SRStartOffset(ScheduledRecording& _parent, ManagedList* _list)
                     : SRBoundedIntegerSetting( -120, 120, 10, _parent, "startoffsetList", "startoffset", _list) 
        {
            setTemplates("Start recording %1 minutes late", "Start recording %1 minute late", "Start recording on time", 
                         "Start recording %1 minute early", "Start recording %1 minutes early");
            setShortTemplates(" %1 minutes late", " %1 minute late", "on time", " %1 minute early", " %1 minutes early");
        }
};



class SREndOffset : public SRBoundedIntegerSetting 
{
    public:
        SREndOffset(ScheduledRecording& _parent, ManagedList* _list)
                     : SRBoundedIntegerSetting( -120, 240, 10, _parent, "endoffsetList", "endoffset", _list) 
        {
            setTemplates("End recording %1 minutes early", "End recording %1 minute early", "End recording on time", 
                         "End recording %1 minute late", "End recording %1 minutes late");
            setShortTemplates("%1 minutes early", "%1 minute early", "on time", "%1 minute late", "%1 minutes late");
        }
};


class SRDupIn : public SRSelectSetting
{
    public:
        SRDupIn(ScheduledRecording& _parent, ManagedList* _list, ManagedListGroup* _group)
            : SRSelectSetting(_parent, "dupInList", "Check for duplicates in...", _group, "dupin", _list ) 
        {
            addSelection("Look for duplicates in current and previous recordings", kDupsInAll);
            addSelection("Look for duplicates in current recordings only", kDupsInRecorded);
            addSelection("Look for duplicates in previous recordings only", kDupsInOldRecorded);
            setValue(kDupsInAll);
        }
};


class SRDupMethod: public SRSelectSetting 
{
    public:
        SRDupMethod(ScheduledRecording& _parent, ManagedList* _list, ManagedListGroup* _group)
            : SRSelectSetting(_parent, "dupMethodList", "Match duplicates with...", _group, "dupmethod", _list) 
        {
            addSelection("Match duplicates using subtitle & description", kDupCheckSubDesc);
            addSelection("Match duplicates using subtitle", kDupCheckSub);
            addSelection("Match duplicates using description", kDupCheckDesc);
            addSelection("Don't match duplicates", kDupCheckNone);
            setValue(kDupCheckSubDesc);
        }
};


class SRRecSearchType: public IntegerSetting, public SimpleSRSetting 
{
    public:
        SRRecSearchType(ScheduledRecording& parent):
            SimpleSRSetting(parent, "search") {
            setVisible(false);
        }
};


class SRProfileSelector: public SRSelectSetting {
    public:
        SRProfileSelector(ScheduledRecording& _parent, ManagedList* _list, ManagedListGroup* _group)
            : SRSelectSetting(_parent, "profileList", "Select recording Profile...", _group, "profile", _list ) 
        {
            _parent.setProfileObj(this);
        }
        
        
        virtual void load(QSqlDatabase* db) {
            fillSelections(db);
            SRSelectSetting::load(db);
        }
        
        virtual void fillSelections(QSqlDatabase* db) {
            clearSelections();
            RecordingProfile::fillSelections(db, selectItem, 0);
        }
};


class SRDupSettingsGroup : public ManagedListGroup
{
    Q_OBJECT
    public:
        SRDupSettingsGroup(ScheduledRecording& _rec, ManagedList* _list, ManagedListGroup* _group, QObject* _parent);
        virtual void doGoBack();    
        void syncText();
    
    public slots:
        void itemChanged(ManagedListItem*);
        
    protected:
        SRDupIn* dupLocItem;
        SRDupMethod* dupMethItem;
};


class SROffsetGroup : public ManagedListGroup
{
    Q_OBJECT
    
    public:
        SROffsetGroup(ScheduledRecording& _rec, ManagedList* pList, ManagedListGroup* pGroup);
        
        virtual void doGoBack()
        {
            syncText();
            ManagedListGroup::doGoBack();
        }

    
        void syncText();
    
    public slots:
        void itemChanged(ManagedListItem*);        
        
    protected:
        SRStartOffset* startOffset;
        SREndOffset* endOffset;
};



class SRChannel: public ChannelSetting, public SimpleSRSetting 
{
    public:
        SRChannel(ScheduledRecording& parent): SimpleSRSetting(parent, "chanid") {
            setVisible(false);
        }
};

class SRStation: public LineEditSetting, public SimpleSRSetting 
{
    public:
        SRStation(ScheduledRecording& parent): SimpleSRSetting(parent, "station") {
            setVisible(false);
        }
};

class SRTitle: public LineEditSetting, public SimpleSRSetting 
{
    public:
        SRTitle(ScheduledRecording& parent)
              : SimpleSRSetting(parent, "title") 
        {
            setVisible(false);
        }
};

class SRSubtitle: public LineEditSetting, public SimpleSRSetting 
{
    public:
        SRSubtitle(ScheduledRecording& parent)
                 : SimpleSRSetting(parent, "subtitle") 
        {
            setVisible(false);
        }
};

class SRDescription: public LineEditSetting, public SimpleSRSetting 
{
    public:
        SRDescription(ScheduledRecording& parent)
                    : SimpleSRSetting(parent, "description") 
        {
            setVisible(false);
        }
};

class SRStartTime: public TimeSetting, public SimpleSRSetting 
{
    public:
        SRStartTime(ScheduledRecording& parent)
                  : SimpleSRSetting(parent, "starttime") 
        {
            setVisible(false);
        }
};

class SRStartDate: public DateSetting, public SimpleSRSetting 
{
    public:
        SRStartDate(ScheduledRecording& parent)
                  : SimpleSRSetting(parent, "startdate") 
        {
            setVisible(false);
        }
};

class SREndTime: public TimeSetting, public SimpleSRSetting 
{
    public:
        SREndTime(ScheduledRecording& parent)
                : SimpleSRSetting(parent, "endtime") 
        {
            setVisible(false);
        }
};

class SREndDate: public DateSetting, public SimpleSRSetting 
{
    public:
        SREndDate(ScheduledRecording& parent)
                : SimpleSRSetting(parent, "enddate") 
        {
            setVisible(false);
        }
};


class SRCategory: public LineEditSetting, public SimpleSRSetting 
{
    public:
        SRCategory(ScheduledRecording& parent)
                 : SimpleSRSetting(parent, "category") 
        {
            setVisible(false);
        }
};

class SRSeriesid: public LineEditSetting, public SimpleSRSetting 
{
    public:
        SRSeriesid(ScheduledRecording& parent)
                 : SimpleSRSetting(parent, "seriesid") 
        {
            setVisible(false);
        }
};

class SRProgramid: public LineEditSetting, public SimpleSRSetting 
{
    public:
        SRProgramid(ScheduledRecording& parent)
                  : SimpleSRSetting(parent, "programid") 
        {
            setVisible(false);
        };
};

class SRAutoExpire: public SRBoolSetting 
{
    public:
        SRAutoExpire(ScheduledRecording& _parent, ManagedList* _list)
                    : SRBoolSetting(_parent, "Allow auto expire", "Don't allow auto expire", 
                                    "autoExpireItem", "autoexpire", _list )
        {
            _parent.setAutoExpireObj(this);
        }
};

class SRMaxNewest: public SRBoolSetting 
{
    public:
        SRMaxNewest(ScheduledRecording& _parent, ManagedList* _list)
                   : SRBoolSetting(_parent, "Delete oldest if this recording exceedes the max episodes",
                                   "Don't record if this would exceed the max episodes", "maxnewestItem", "maxnewest", _list )
        {
            setValue(false);
            _parent.setMaxNewestObj(this);
        }
};




class SRMaxEpisodes : public SRBoundedIntegerSetting 
{
    public:
    SRMaxEpisodes(ScheduledRecording& _parent, ManagedList* _list)
                 : SRBoundedIntegerSetting( 0, 100, 10, _parent, "maxepisodesList", "maxepisodes", _list) 
    {
        setTemplates("", "", "No episode limit", "Keep only one episode.", "Keep at most %1 episodes");
        setShortTemplates("", "", "No episode limit", "one episode", "%1 episodes");
        _parent.setMaxEpisodesObj(this);
    }
};



class SRRecPriority: public SRBoundedIntegerSetting 
{
    public:
        SRRecPriority(ScheduledRecording& _parent, ManagedList* _list)
                    : SRBoundedIntegerSetting( -99, 99, 5, _parent, "recpriorityList", "recpriority", _list) 
        {
            setTemplates( "Reduce priority by %1", "Reduce priority by %1", "No recording priority", 
                          "Raise priority by %1", "Raise priority by %1" );
            setValue(0);
            _parent.setRecPriorityObj(this);
        }
};

class SRRecGroup: public SRSelectSetting {
public:
    SRRecGroup(ScheduledRecording& _parent, ManagedListGroup* _group, ManagedList* _list)
        : SRSelectSetting(_parent, "recgroupList", "Select recording group...", _group, "recgroup", _list ) 
    {
    }

    virtual void load(QSqlDatabase *db) {
        fillSelections(db);
        SRSelectSetting::load(db);
    }

    virtual void fillSelections(QSqlDatabase *db) {
        addSelection("Store in the default recording group", QString("Default"), false);

        QString thequery = QString("SELECT DISTINCT recgroup from recorded "
                                   "WHERE recgroup <> '%1'")
                                   .arg(QString("Default"));
        QSqlQuery query = db->exec(thequery);

        if (query.isActive() && query.numRowsAffected() > 0)
            while (query.next())
                addSelection(QString(QObject::tr("Store in the '%1' group")).arg(query.value(0).toString()),
                             query.value(0).toString(), false);

        thequery = QString("SELECT DISTINCT recgroup from record "
                           "WHERE recgroup <> '%1'")
                           .arg(QString("Default"));
        query = db->exec(thequery);

        if (query.isActive() && query.numRowsAffected() > 0)
            while (query.next())
                addSelection(QString(QObject::tr("Store in the '%1' group")).arg(query.value(0).toString()),
                             query.value(0).toString(), false);
    }
};
#endif

