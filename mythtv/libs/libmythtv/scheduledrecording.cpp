#include "scheduledrecording.h"
#include "programinfo.h"
#include "recordingprofile.h"
#include "mythcontext.h"
#include "proglist.h"

#include <qlayout.h>
#include <qlabel.h>
#include <qapplication.h>
#include <qregexp.h>

// Convert a RecordingType to a simple integer so it's specificity can
// be compared to another.  Lower number means more specific.
int RecTypePriority(RecordingType rectype)
{
    switch (rectype) {
    case kNotRecording:   return 0; break;
    case kDontRecord:     return 1; break;
    case kOverrideRecord: return 2; break;
    case kSingleRecord:   return 3; break;
    case kFindOneRecord:  return 4; break;
    case kWeekslotRecord: return 5; break;
    case kTimeslotRecord: return 6; break;
    case kChannelRecord:  return 7; break;
    case kAllRecord:      return 9; break;
    default: return 9;
    }
}

class SRSetting: public SimpleDBStorage {
protected:
    SRSetting(const ScheduledRecording& _parent, QString name):
        SimpleDBStorage("record", name),
        parent(_parent) {
        setName(name);
    };

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
    };

    const ScheduledRecording& parent;
};

// NOTE: if this changes, you _MUST_ update the RecTypePriority function above.
class SRRecordingType: public ComboBoxSetting, public SRSetting {
public:
    SRRecordingType(const ScheduledRecording& parent):
        SRSetting(parent, "type") {
        setLabel(QObject::tr("Schedule"));
        addSelection(QString::number(kNotRecording));
        addSelection(QString::number(kSingleRecord));
        addSelection(QString::number(kFindOneRecord));
        addSelection(QString::number(kWeekslotRecord));
        addSelection(QString::number(kTimeslotRecord));
        addSelection(QString::number(kChannelRecord));
        addSelection(QString::number(kAllRecord));
        addSelection(QString::number(kOverrideRecord));
        addSelection(QString::number(kDontRecord));
    };
    void addNormalSelections(bool haschannel) {
        addSelection(QObject::tr("Do not record this program"),
                     QString::number(kNotRecording));
        if (haschannel)
            addSelection(QObject::tr("Record only this showing"),
                         QString::number(kSingleRecord));
        addSelection(QObject::tr("Record one showing of this program"),
                     QString::number(kFindOneRecord));
        if (haschannel)
        {
            addSelection(QObject::tr("Record in this timeslot every week"),
                         QString::number(kWeekslotRecord));
            addSelection(QObject::tr("Record in this timeslot every day"),
                         QString::number(kTimeslotRecord));
            addSelection(QObject::tr("Record at any time on this channel"),
                         QString::number(kChannelRecord));
        }
        addSelection(QObject::tr("Record at any time on any channel"),
                     QString::number(kAllRecord));
    };
    void addOverrideSelections(void) {
        addSelection(QObject::tr("Record this showing with normal options"),
                     QString::number(kNotRecording));
        addSelection(QObject::tr("Record this showing with override options"),
                     QString::number(kOverrideRecord));
        addSelection(QObject::tr("Do not record this showing"),
                     QString::number(kDontRecord));
    };
};

class SRRecSearchType: public IntegerSetting, public SRSetting {
public:
    SRRecSearchType(const ScheduledRecording& parent):
        SRSetting(parent, "search") {
        setVisible(false);
    };
};

class SRProfileSelector: public ComboBoxSetting, public SRSetting {
public:
    SRProfileSelector(const ScheduledRecording& parent):
        SRSetting(parent, "profile") {
        setLabel(QObject::tr("Profile"));
    };

    virtual void load(QSqlDatabase* db) {
        fillSelections(db);
        SRSetting::load(db);
    };

    virtual void fillSelections(QSqlDatabase* db) {
        clearSelections();
        RecordingProfile::fillSelections(db, this, 0);
    };
};

class SRDupIn: public ComboBoxSetting, public SRSetting {
public:
    SRDupIn(const ScheduledRecording& parent):
        SRSetting(parent, "dupin") {
        setLabel(QObject::tr("Duplicate Location"));
        addSelection(QObject::tr("All places"),
                 QString::number(kDupsInAll));
        addSelection(QObject::tr("Current Recs"),
                 QString::number(kDupsInRecorded));
        addSelection(QObject::tr("Previous Recs"),
                 QString::number(kDupsInOldRecorded));
    };
};

class SRDupMethod: public ComboBoxSetting, public SRSetting {
public:
    SRDupMethod(const ScheduledRecording& parent):
        SRSetting(parent, "dupmethod") {
        setLabel(QObject::tr("Duplicate Check"));
        addSelection(QObject::tr("Sub & Desc"),
                 QString::number(kDupCheckSubDesc));
        addSelection(QObject::tr("Subtitle"),
                 QString::number(kDupCheckSub));
        addSelection(QObject::tr("Description"),
                 QString::number(kDupCheckDesc));
        addSelection(QObject::tr("None"),
                 QString::number(kDupCheckNone));
    };
};

class SRAutoExpire: public CheckBoxSetting, public SRSetting {
public:
    SRAutoExpire(const ScheduledRecording& parent):
        SRSetting(parent, "autoexpire") {
        setLabel(QObject::tr("Auto Expire"));
    };
};

class SRStartOffset: public SpinBoxSetting, public SRSetting {
public:
    SRStartOffset(const ScheduledRecording& parent):
        SpinBoxSetting(-120, 120, 10, true),
        SRSetting(parent, "startoffset") {
        setLabel(QObject::tr("Start Early (minutes)"));
    };
};

class SREndOffset: public SpinBoxSetting, public SRSetting {
public:
    SREndOffset(const ScheduledRecording& parent):
        SpinBoxSetting(-120, 240, 10, true),
        SRSetting(parent, "endoffset") {
        setLabel(QObject::tr("End Late   (minutes)"));
    };
};

class SRMaxEpisodes: public SpinBoxSetting, public SRSetting {
public:
    SRMaxEpisodes(const ScheduledRecording& parent):
        SpinBoxSetting(0, 60, 1),
        SRSetting(parent, "maxepisodes") {
        setLabel(QObject::tr("Max episodes"));
    };
};

class SRMaxNewest: public CheckBoxSetting, public SRSetting {
public:
    SRMaxNewest(const ScheduledRecording& parent):
        SRSetting(parent, "maxnewest") {
        setLabel(QObject::tr("Delete oldest over Max"));
        setValue(false);
    };
};

class SRChannel: public ChannelSetting, public SRSetting {
public:
    SRChannel(const ScheduledRecording& parent): SRSetting(parent, "chanid") {
        setVisible(false);
    };
};

class SRStation: public LineEditSetting, public SRSetting {
public:
    SRStation(const ScheduledRecording& parent): SRSetting(parent, "station") {
        setVisible(false);
    };
};

class SRTitle: public LineEditSetting, public SRSetting {
public:
    SRTitle(const ScheduledRecording& parent):
        SRSetting(parent, "title") {
        setVisible(false);
    };
};

class SRSubtitle: public LineEditSetting, public SRSetting {
public:
    SRSubtitle(const ScheduledRecording& parent):
        SRSetting(parent, "subtitle") {
        setVisible(false);
    };
};

class SRDescription: public LineEditSetting, public SRSetting {
public:
    SRDescription(const ScheduledRecording& parent):
        SRSetting(parent, "description") {
        setVisible(false);
    };
};

class SRStartTime: public TimeSetting, public SRSetting {
public:
    SRStartTime(const ScheduledRecording& parent):
        SRSetting(parent, "starttime") {
        setVisible(false);
    };
};

class SRStartDate: public DateSetting, public SRSetting {
public:
    SRStartDate(const ScheduledRecording& parent):
        SRSetting(parent, "startdate") {
        setVisible(false);
    };
};

class SREndTime: public TimeSetting, public SRSetting {
public:
    SREndTime(const ScheduledRecording& parent):
        SRSetting(parent, "endtime") {
        setVisible(false);
    };
};

class SREndDate: public DateSetting, public SRSetting {
public:
    SREndDate(const ScheduledRecording& parent):
        SRSetting(parent, "enddate") {
        setVisible(false);
    };
};

class SRRecPriority: public SpinBoxSetting, public SRSetting {
public:
    SRRecPriority(const ScheduledRecording& parent): 
        SpinBoxSetting(-99, 99, 5, true),
        SRSetting(parent, "recpriority") {
        setLabel(QObject::tr("Priority"));
        setValue(0);
    };
};

class SRRecGroup: public ComboBoxSetting, public SRSetting {
public:
    SRRecGroup(const ScheduledRecording& parent):
        ComboBoxSetting(true),
        SRSetting(parent, "recgroup") {
        setLabel(QObject::tr("Recording Group"));
    };

    virtual void load(QSqlDatabase *db) {
        fillSelections(db);
        SRSetting::load(db);
    };

    virtual void fillSelections(QSqlDatabase *db) {
        addSelection(QString("Default"), QString("Default"));

        QString thequery = QString("SELECT DISTINCT recgroup from recorded "
                                   "WHERE recgroup <> '%1'")
                                   .arg(QString("Default"));
        QSqlQuery query = db->exec(thequery);

        if (query.isActive() && query.numRowsAffected() > 0)
            while (query.next())
                addSelection(query.value(0).toString(),
                             query.value(0).toString());

        thequery = QString("SELECT DISTINCT recgroup from record "
                           "WHERE recgroup <> '%1'")
                           .arg(QString("Default"));
        query = db->exec(thequery);

        if (query.isActive() && query.numRowsAffected() > 0)
            while (query.next())
                addSelection(query.value(0).toString(),
                             query.value(0).toString());
    };
};

class SRCategory: public LineEditSetting, public SRSetting {
public:
    SRCategory(const ScheduledRecording& parent):
        SRSetting(parent, "category") {
        setVisible(false);
    };
};

class SRSeriesid: public LineEditSetting, public SRSetting {
public:
    SRSeriesid(const ScheduledRecording& parent):
        SRSetting(parent, "seriesid") {
        setVisible(false);
    };
};

class SRProgramid: public LineEditSetting, public SRSetting {
public:
    SRProgramid(const ScheduledRecording& parent):
        SRSetting(parent, "programid") {
        setVisible(false);
    };
};

ScheduledRecording::ScheduledRecording() {
    addChild(id = new ID());
    addChild(type = new SRRecordingType(*this));
    addChild(search = new SRRecSearchType(*this));
    addChild(profile = new SRProfileSelector(*this));
    addChild(dupin = new SRDupIn(*this));
    addChild(dupmethod = new SRDupMethod(*this));
    addChild(autoexpire = new SRAutoExpire(*this));
    addChild(maxepisodes = new SRMaxEpisodes(*this));
    addChild(startoffset = new SRStartOffset(*this));
    addChild(endoffset = new SREndOffset(*this));
    addChild(maxnewest = new SRMaxNewest(*this));
    addChild(channel = new SRChannel(*this));
    addChild(station = new SRStation(*this));
    addChild(title = new SRTitle(*this));
    addChild(subtitle = new SRSubtitle(*this));
    addChild(description = new SRDescription(*this));
    addChild(startTime = new SRStartTime(*this));
    addChild(endTime = new SREndTime(*this));
    addChild(startDate = new SRStartDate(*this));
    addChild(endDate = new SREndDate(*this));
    addChild(category = new SRCategory(*this));
    addChild(recpriority = new SRRecPriority(*this));
    addChild(recgroup = new SRRecGroup(*this));
    addChild(seriesid = new SRSeriesid(*this));
    addChild(programid = new SRProgramid(*this));

    m_pginfo = NULL;
}

void ScheduledRecording::load(QSqlDatabase *db)
{
    if (getRecordID())
    {
        ConfigurationGroup::load(db);

        QString tmpType = type->getValue();
        type->clearSelections();
        if (tmpType.toInt() == kOverrideRecord ||
            tmpType.toInt() == kDontRecord)
            type->addOverrideSelections();
        else
            type->addNormalSelections(!station->getValue().isEmpty());
        type->setValue(type->getValueIndex(tmpType));
        type->setUnchanged();
    }
}

void ScheduledRecording::setDefault(QSqlDatabase *db, bool haschannel)
{
    id->setValue(0);
    type->clearSelections();
    type->addNormalSelections(haschannel);
    search->setValue(kNoSearch);

    title->setValue("");
    subtitle->setValue("");
    description->setValue("");
    channel->setValue("");
    station->setValue("");
    QDate date = QDate::currentDate();
    QTime time = QTime::currentTime();
    startDate->setValue(date);
    startTime->setValue(time);
    endDate->setValue(date);
    endTime->setValue(time);
    seriesid->setValue("");
    programid->setValue("");
    category->setValue("");

    profile->fillSelections(db);
    dupin->setValue(0);
    dupmethod->setValue(0);
    autoexpire->setValue(gContext->GetNumSetting("AutoExpireDefault", 0));
    maxepisodes->setValue(0);
    maxnewest->setValue(0);
    startoffset->setValue(0);
    endoffset->setValue(0);
    recpriority->setValue(0);
    recgroup->fillSelections(db);
    recgroup->setValue("Default");
}

void ScheduledRecording::setProgram(ProgramInfo *proginfo)
{
    title->setValue(proginfo->title);
    subtitle->setValue(proginfo->subtitle);
    description->setValue(proginfo->description);
    channel->setValue(proginfo->chanid);
    station->setValue(proginfo->chansign);
    startDate->setValue(proginfo->startts.date());
    startTime->setValue(proginfo->startts.time());
    endDate->setValue(proginfo->endts.date());
    endTime->setValue(proginfo->endts.time());
    seriesid->setValue(proginfo->seriesid);
    programid->setValue(proginfo->programid);
    category->setValue(proginfo->category);
}

void ScheduledRecording::makeOverride(void)
{
    if (type->getValue().toInt() == kOverrideRecord ||
        type->getValue().toInt() == kDontRecord)
        return;

    id->setValue(0);
    type->clearSelections();
    type->addOverrideSelections();
    search->setValue(kNoSearch);

    setProgram(m_pginfo);

    profile->setChanged();
    dupin->setChanged();
    dupmethod->setChanged();
    autoexpire->setChanged();
    maxepisodes->setChanged();
    maxnewest->setChanged();
    startoffset->setChanged();
    endoffset->setChanged();
    recpriority->setChanged();
    recgroup->setChanged();
}

void ScheduledRecording::loadByProgram(QSqlDatabase* db,
                                       ProgramInfo* proginfo) {
    m_pginfo = proginfo;

    if (proginfo->recordid)
        loadByID(db, proginfo->recordid);
    else {
        setDefault(db, true);
        search->setValue(kNoSearch);
    }

    if (search->intValue() == kNoSearch)
        setProgram(proginfo);
}

void ScheduledRecording::loadBySearch(QSqlDatabase *db,
                                      RecSearchType lsearch,
                                      QString forwhat)
{
    int rid = 0;

    QString thequery = QString("SELECT recordid FROM record WHERE "
                               "search = %1 AND description LIKE '%2'")
        .arg(lsearch).arg(forwhat);
    QSqlQuery query = db->exec(thequery);

    if (query.isActive())
    {
        if (query.next())
            rid = query.value(0).toInt();
    }
    else
        MythContext::DBError("loadBySearch", query);

    if (rid)
        loadByID(db, rid);
    else
    {
        setDefault(db, false);
        search->setValue(lsearch);
        QString ltitle = forwhat;
        switch (lsearch)
        {
        case kTitleSearch:
            ltitle += " (" +  QObject::tr("Title Search") + ")";
            break;
        case kKeywordSearch:
            ltitle += " (" +  QObject::tr("Keyword Search") + ")";
            break;
        case kPeopleSearch:
            ltitle += " (" +  QObject::tr("People Search") + ")";
            break;
        default:
            ltitle += " (" +  QObject::tr("Unknown Search") + ")";
            break;
        }
        title->setValue(ltitle);
        description->setValue(forwhat);
    } 
}

void ScheduledRecording::loadByID(QSqlDatabase* db, int recordID) {
    id->setValue(recordID);
    load(db);
}

RecordingType ScheduledRecording::getRecordingType(void) const {
    return (RecordingType)(type->getValue().toInt());
}

void ScheduledRecording::setRecordingType(RecordingType newType) {
    type->setValue(type->getValueIndex(QString::number(newType)));
}

RecSearchType ScheduledRecording::getSearchType(void) const {
    return (RecSearchType)(search->intValue());
}

bool ScheduledRecording::GetAutoExpire(void) const {
    return(autoexpire->getValue().toInt());
}

void ScheduledRecording::SetAutoExpire(bool expire) {
    autoexpire->setValue(expire);
}

int ScheduledRecording::GetMaxEpisodes(void) const {
    return(maxepisodes->getValue().toInt());
}

bool ScheduledRecording::GetMaxNewest(void) const {
    return(maxnewest->getValue().toInt());
}

void ScheduledRecording::save(QSqlDatabase* db) {
    if (type->isChanged() && getRecordingType() == kNotRecording)
        remove(db);
    else
        ConfigurationGroup::save(db);
    signalChange(db);
}

void ScheduledRecording::remove(QSqlDatabase* db) {
    if (!getRecordID())
        return;

    QString query = QString("DELETE FROM record WHERE recordid = %1")
                            .arg(getRecordID());
    db->exec(query);
    query = QString("DELETE FROM recordoverride WHERE recordid = %1")
                    .arg(getRecordID());
    db->exec(query);
    query = QString("UPDATE recordid FROM recorded SET recordid = NULL "
                    "WHERE recordid = %1")
                    .arg(getRecordID());
}

void ScheduledRecording::signalChange(QSqlDatabase* db) {
    MythContext::KickDatabase(db);

    QSqlQuery result = db->exec("SELECT NULL FROM settings WHERE value = \"RecordChanged\";");

    QString query;
    if (result.isActive() && result.numRowsAffected() > 0)
    {
        query = "UPDATE settings SET data = \"yes\" WHERE "
            "value = \"RecordChanged\";";
    }
    else
    {
        query = "INSERT INTO settings (value,data) "
            "VALUES(\"RecordChanged\", \"yes\");";
    }
    db->exec(query);

}

bool ScheduledRecording::hasChanged(QSqlDatabase* db) {
    QString thequery;
    
    bool retval = false;

    thequery = "SELECT data FROM settings WHERE value = \"RecordChanged\";";
    QSqlQuery query = db->exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        QString value = query.value(0).toString();
        if (value == "yes")
        {
            thequery = "UPDATE settings SET data = \"no\" WHERE value = "
                       "\"RecordChanged\";";
            query = db->exec(thequery);

            retval = true;
        }
    }

    return retval;
}

void ScheduledRecording::doneRecording(QSqlDatabase* db, 
                                       const ProgramInfo& proginfo) 
{
    if (getRecordingType() == kFindOneRecord)
        remove(db);

    addHistory(db, proginfo);
}

void ScheduledRecording::addHistory(QSqlDatabase* db, 
                                    const ProgramInfo& proginfo) 
{
    QString sqltitle = proginfo.title;
    QString sqlsubtitle = proginfo.subtitle;
    QString sqldescription = proginfo.description;
    QString sqlcategory = proginfo.category;

    sqltitle.replace(QRegExp("\""), QString("\\\""));
    sqlsubtitle.replace(QRegExp("\""), QString("\\\""));
    sqldescription.replace(QRegExp("\""), QString("\\\""));
    sqlcategory.replace(QRegExp("\""), QString("\\\""));

    QString query = QString("INSERT INTO oldrecorded (chanid,starttime,"
                            "endtime,title,subtitle,description,category,"
                            "seriesid,programid) "
                            "VALUES(%1,\"%2\",\"%3\",\"%4\",\"%5\",\"%6\","
                            "\"%7\",\"%8\",\"%9\");")
        .arg(proginfo.chanid)
        .arg(proginfo.recstartts.toString(Qt::ISODate))
        .arg(proginfo.recendts.toString(Qt::ISODate))
        .arg(sqltitle.utf8()) 
        .arg(sqlsubtitle.utf8())
        .arg(sqldescription.utf8())
        .arg(sqlcategory.utf8())
        .arg(proginfo.seriesid)
        .arg(proginfo.programid);

    QSqlQuery result = db->exec(query);
    if (!result.isActive())
    {
        MythContext::DBError("addHistory", result);
    }
    else
    {
        // The addition of an entry to oldrecorded may affect near-future
        // scheduling decisions, so recalculate
        signalChange(db);
    }
}

void ScheduledRecording::forgetHistory(QSqlDatabase* db,
                                       const ProgramInfo& proginfo) {
    QString sqltitle = proginfo.title;
    QString sqlsubtitle = proginfo.subtitle;
    QString sqldescription = proginfo.description;

    sqltitle.replace(QRegExp("'"), QString("\\'"));
    sqlsubtitle.replace(QRegExp("'"), QString("\\'"));
    sqldescription.replace(QRegExp("'"), QString("\\'"));

    QString query = QString("DELETE FROM oldrecorded WHERE title = '%1' AND subtitle = '%2' AND description = '%3'")
        .arg(sqltitle.utf8()) 
        .arg(sqlsubtitle.utf8())
        .arg(sqldescription.utf8());
    QSqlQuery result = db->exec(query);
    if (!result.isActive())
    {
        MythContext::DBError("forgetHistory", result);
    }
    else
    {
        // The removal of an entry from oldrecorded may affect near-future
        // scheduling decisions, so recalculate
        signalChange(db);
    }
}

MythDialog* ScheduledRecording::dialogWidget(MythMainWindow *parent, 
                                             const char *name)
{
    float wmult, hmult;
    gContext->GetScreenSettings(wmult, hmult);

    MythDialog* dialog = new ConfigurationDialogWidget(parent, name);
    QVBoxLayout* vbox = new QVBoxLayout(dialog, (int)(20 * wmult),
                                                (int)(10 * wmult));

    if (!m_pginfo || search->intValue() != kNoSearch)
    {
        QLabel *searchlabel = new QLabel(title->getValue(), dialog);
        searchlabel->setBackgroundOrigin(QWidget::WindowOrigin);
        searchlabel->setFont(gContext->GetBigFont());
        searchlabel->setMinimumWidth(int(760 * wmult));
        searchlabel->setMaximumWidth(int(760 * wmult));
        vbox->addWidget(searchlabel);
    }

    if (m_pginfo)
    {
        QGridLayout *grid = m_pginfo->DisplayWidget(dialog);
        vbox->addLayout(grid);
    }

    QFrame *f;
    f = new QFrame(dialog);
    f->setFrameStyle(QFrame::HLine | QFrame::Plain);
    f->setLineWidth((int)(4 * hmult));
    vbox->addWidget(f);    

    typeWidget = type->configWidget(this, dialog);
    vbox->addWidget(typeWidget);

    QHBoxLayout* hbox = new QHBoxLayout(vbox, (int)(20 * wmult));
    QVBoxLayout* vbox1 = new QVBoxLayout(hbox, (int)(10 * wmult));
    QVBoxLayout* vbox2 = new QVBoxLayout(hbox, (int)(10 * wmult));

    profileWidget = profile->configWidget(this, dialog);
    vbox1->addWidget(profileWidget);
    recpriorityWidget = recpriority->configWidget(this, dialog);
    vbox1->addWidget(recpriorityWidget);
    autoexpireWidget = autoexpire->configWidget(this, dialog);
    vbox1->addWidget(autoexpireWidget);
    maxepisodesWidget = maxepisodes->configWidget(this, dialog);
    vbox1->addWidget(maxepisodesWidget);
    maxnewestWidget = maxnewest->configWidget(this, dialog);
    vbox1->addWidget(maxnewestWidget);

    recgroupWidget = recgroup->configWidget(this, dialog);
    vbox2->addWidget(recgroupWidget);
    startoffsetWidget = startoffset->configWidget(this, dialog);
    vbox2->addWidget(startoffsetWidget);
    endoffsetWidget = endoffset->configWidget(this, dialog);
    vbox2->addWidget(endoffsetWidget);
    dupmethodWidget = dupmethod->configWidget(this, dialog);
    vbox2->addWidget(dupmethodWidget);
    dupinWidget = dupin->configWidget(this, dialog);
    vbox2->addWidget(dupinWidget);

    hbox = new QHBoxLayout(vbox, (int)(20 * wmult));
    proglistButton = new MythPushButton(tr("List upcoming episodes"),
                                                dialog);
    if (!m_pginfo)
        proglistButton->setEnabled(false);
    hbox->addWidget(proglistButton);
    hbox->addStretch( 42 );
    MythPushButton *cancel = new MythPushButton(tr("&Cancel"), dialog);
    hbox->addWidget(cancel);
    hbox->addSpacing( 6 );
    MythPushButton *finish = new MythPushButton(tr("&Finish"), dialog);
    hbox->addWidget(finish);

    connect(type, SIGNAL(valueChanged(const QString&)), this,
            SLOT(setAvailableOptions(const QString&)));
    connect(maxepisodes, SIGNAL(valueChanged(int)), this,
            SLOT(setAvailableOptions(int)));
    connect(proglistButton, SIGNAL(clicked()), this, SLOT(runProgList()));
    connect(cancel, SIGNAL(clicked()), dialog, SLOT(reject()));
    connect(finish, SIGNAL(clicked()), dialog, SLOT(accept()));

    setAvailableOptions();
    type->setFocus();

    return dialog;
}

void ScheduledRecording::setAvailableOptions(void)
{
    bool multiEpisode = true;
    bool isScheduled = true;
    bool maxIsSet = false;

    switch(type->getValue().toInt())
    {
        case kNotRecording:
        case kDontRecord:
                isScheduled = false;
                break;

        case kSingleRecord:
        case kFindOneRecord:
        case kOverrideRecord:
                multiEpisode = false;
                break;
    }

    if (maxepisodes->getValue().toInt() > 0)
        maxIsSet = true;

    profileWidget->setEnabled(isScheduled);
    recpriorityWidget->setEnabled(isScheduled);
    autoexpireWidget->setEnabled(isScheduled);
    recgroupWidget->setEnabled(isScheduled);
    startoffsetWidget->setEnabled(isScheduled);
    endoffsetWidget->setEnabled(isScheduled);

    maxepisodesWidget->setEnabled(isScheduled && multiEpisode);
    maxnewestWidget->setEnabled(isScheduled && multiEpisode && maxIsSet);
    dupinWidget->setEnabled(isScheduled && multiEpisode);
    dupmethodWidget->setEnabled(isScheduled && multiEpisode);
}

void ScheduledRecording::runProgList(void)
{
    ProgLister *pl = new ProgLister(plTitle, title->getValue(),
                                    QSqlDatabase::database(),
                                    gContext->GetMainWindow(), "proglist");
    pl->exec();
    delete pl;
    proglistButton->setFocus();
}

void ScheduledRecording::fillSelections(QSqlDatabase* db, SelectSetting* setting) {
    QSqlQuery result = db->exec("SELECT recordid FROM record");
    if (result.isActive() && result.numRowsAffected() > 0)
        while (result.next()) {
            int id = result.value(0).toInt();

            ScheduledRecording sr;
            sr.loadByID(db, id);

            QString label;
            QString weekly = "";

            switch (sr.getRecordingType()) {
            case kAllRecord:
                label = QString("%1").arg(sr.title->getValue());
                break;
            case kFindOneRecord:
                label = QString("%1").arg(sr.title->getValue());
                break;
            case kChannelRecord:
                label = QString("%1 on channel %2")
                    .arg(sr.title->getValue())
                    .arg(sr.channel->getSelectionLabel());
                break;
            case kWeekslotRecord:
                weekly = QDate(sr.startDate->dateValue()).toString("dddd")+"s ";
            case kTimeslotRecord:
                label = QString("%1 on channel %2 (%3%4 - %5)")
                    .arg(sr.title->getValue())
                    .arg(sr.channel->getSelectionLabel())
                    .arg(weekly)
                    .arg(sr.startTime->timeValue().toString())
                    .arg(sr.endTime->timeValue().toString());
                break;
            case kSingleRecord:
            case kOverrideRecord:
            case kDontRecord:
                label = QString("%1 on channel %2 (%3 %4 - %5)")
                    .arg(sr.title->getValue())
                    .arg(sr.channel->getSelectionLabel())
                    .arg(sr.startDate->dateValue().toString())
                    .arg(sr.startTime->timeValue().toString())
                    .arg(sr.endTime->timeValue().toString());
                break;
            default:
                label = "You should not see this";
            }

            setting->addSelection(label, QString::number(id));
        }
}

void ScheduledRecordingEditor::load(QSqlDatabase* db) {
    clearSelections();
    ScheduledRecording::fillSelections(db, this);
}


int ScheduledRecordingEditor::exec(QSqlDatabase* db) {
    while (ConfigurationDialog::exec(db) == QDialog::Accepted)
        open(getValue().toInt());

    return QDialog::Rejected;
}

void ScheduledRecordingEditor::open(int id) {
    ScheduledRecording* sr = new ScheduledRecording();

    if (id != 0)
        sr->loadByID(db,id);

    if (sr->exec(db) == QDialog::Accepted)
        sr->save(db);
    delete sr;
}

void ScheduledRecording::setStart(const QDateTime& start) {
    startTime->setValue(start.time());
    startDate->setValue(start.date());
}

void ScheduledRecording::setEnd(const QDateTime& end) {
    endTime->setValue(end.time());
    endDate->setValue(end.date());
}

void ScheduledRecording::setRecPriority(int newrecpriority) {
    recpriority->setValue(newrecpriority);
}

void ScheduledRecording::setRecGroup(const QString& newrecgroup) {
    recgroup->setValue(newrecgroup);
}

QString ScheduledRecording::getProfileName(void) const {
    return profile->getValue();
}
