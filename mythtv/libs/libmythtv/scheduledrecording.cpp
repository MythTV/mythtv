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
    case kSingleRecord:   return 1; break;
    case kFindOneRecord:  return 2; break;
    case kWeekslotRecord: return 3; break;
    case kTimeslotRecord: return 4; break;
    case kChannelRecord:  return 5; break;
    case kAllRecord:      return 6; break;
    default: return 7;
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

class SRRecordingType: public ComboBoxSetting, public SRSetting {
public:
    SRRecordingType(const ScheduledRecording& parent):
        SRSetting(parent, "type") {
        setLabel(QObject::tr("Schedule"));
        addSelection(QObject::tr("Do not record this program"),
                     QString::number(kNotRecording));
        addSelection(QObject::tr("Record only this showing"),
                     QString::number(kSingleRecord));
        addSelection(QObject::tr("Record one showing of this program"),
                     QString::number(kFindOneRecord));
        addSelection(QObject::tr("Record in this timeslot every week"),
                     QString::number(kWeekslotRecord));
        addSelection(QObject::tr("Record in this timeslot every day"),
                     QString::number(kTimeslotRecord));
        addSelection(QObject::tr("Record at any time on this channel"),
                     QString::number(kChannelRecord));
        addSelection(QObject::tr("Record at any time on any channel"),
                     QString::number(kAllRecord));
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
        addSelection(QObject::tr("Even if Empty"),
                 QString::number(kDupEmptySubDesc));
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

class SRPreRoll: public SpinBoxSetting, public SRSetting {
public:
    SRPreRoll(const ScheduledRecording& parent):
        SpinBoxSetting(-120, 120, 10, true),
        SRSetting(parent, "preroll") {
        setLabel(QObject::tr("Start Early (minutes)"));
    };
};

class SRPostRoll: public SpinBoxSetting, public SRSetting {
public:
    SRPostRoll(const ScheduledRecording& parent):
        SpinBoxSetting(-120, 240, 10, true),
        SRSetting(parent, "postroll") {
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
        SpinBoxSetting(-99, 99, 1),
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
        addSelection(QObject::tr("Default"), QObject::tr("Default"));

        QSqlDatabase *m_db = QSqlDatabase::database();
        QString thequery = QString("SELECT DISTINCT recgroup from recorded "
                                   "WHERE recgroup <> '%1'")
                                   .arg(QObject::tr("Default"));
        QSqlQuery query = m_db->exec(thequery);

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

ScheduledRecording::ScheduledRecording() {
    addChild(id = new ID());
    addChild(type = new SRRecordingType(*this));
    addChild(profile = new SRProfileSelector(*this));
    addChild(dupin = new SRDupIn(*this));
    addChild(dupmethod = new SRDupMethod(*this));
    addChild(autoexpire = new SRAutoExpire(*this));
    addChild(maxepisodes = new SRMaxEpisodes(*this));
    addChild(preroll = new SRPreRoll(*this));
    addChild(postroll = new SRPostRoll(*this));
    addChild(maxnewest = new SRMaxNewest(*this));
    addChild(channel = new SRChannel(*this));
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

    m_pginfo = NULL;
}

void ScheduledRecording::fromProgramInfo(ProgramInfo* proginfo)
{
    channel->setValue(proginfo->chanid);
    title->setValue(proginfo->title);
    subtitle->setValue(proginfo->subtitle);
    description->setValue(proginfo->description);
    startTime->setValue(proginfo->startts.time());
    startDate->setValue(proginfo->startts.date());
    endTime->setValue(proginfo->endts.time());
    endDate->setValue(proginfo->endts.date());
    category->setValue(proginfo->category);
    recpriority->setValue(proginfo->recpriority);
    recgroup->setValue(proginfo->recgroup);
    autoexpire->setValue(gContext->GetNumSetting("AutoExpireDefault", 0));
}

bool ScheduledRecording::loadByProgram(QSqlDatabase* db,
                                       ProgramInfo* proginfo) {
    m_pginfo = proginfo;

    if (proginfo->recordid)
    {
        loadByID(db, proginfo->recordid);
        return true;
    }

    QString sqltitle(proginfo->title);

    // this doesn't have to be a QRegexp in qt 3.1+
    sqltitle.replace(QRegExp("\'"), "\\'");
    sqltitle.replace(QRegExp("\""), "\\\"");

    // prevent the SQL from breaking if chanid is null
    QString chanid = proginfo->chanid;
    if (!chanid || chanid == "")
         chanid = "0";

    // XXX - this should set proginfo.duplicate as in findAllProgramsToRecord
    // Have to split up into parts since .arg() only supports %1-%9
    QString query = QString(
"SELECT "
"recordid "
"FROM record "
"WHERE "
"record.title = \"%1\" "
"AND "
"((record.type = %2 " // allrecord
"OR record.type = %3) " // findonerecord
" OR "
" ((record.chanid = %4) " // channel matches
"  AND "
"  ((record.type = %5) " // channelrecord
"   OR"
"   (((TIME_TO_SEC(record.starttime) = TIME_TO_SEC('%6')) " // timeslot matches
"      AND "
"     (TIME_TO_SEC(record.endtime) = TIME_TO_SEC('%7')) "
"    )"
"    AND ")
        .arg(sqltitle.utf8())
        .arg(kAllRecord)
        .arg(kFindOneRecord)
        .arg(chanid)
        .arg(kChannelRecord)
        .arg(proginfo->startts.time().toString(Qt::ISODate))
        .arg(proginfo->endts.time().toString(Qt::ISODate));

query += QString(
"    ((record.type = %1) " // timeslotrecord
"     OR"
"     ((DAYOFWEEK(record.startdate) = DAYOFWEEK('%2') "
"      AND "
"      ((record.type = %3) " // weekslotrecord
"       OR "
"       ((TO_DAYS(record.startdate) = TO_DAYS('%4')) " // date matches
"        AND "
"        (TO_DAYS(record.enddate) = TO_DAYS('%5')) "
"        )"
"       )"
"      )"
"     )"
"    )"
"   )"
"  )"
" )"
");")
        .arg(kTimeslotRecord)
        .arg(proginfo->startts.date().toString(Qt::ISODate))
        .arg(kWeekslotRecord)
        .arg(proginfo->startts.date().toString(Qt::ISODate))
        .arg(proginfo->endts.date().toString(Qt::ISODate));

    QSqlQuery result = db->exec(query);
    if (result.isActive()) {
        if (result.numRowsAffected() > 0) {
            result.next();
            loadByID(db, result.value(0).toInt());
            return true;
        } else {
            fromProgramInfo(proginfo);
        }
    } else {
        MythContext::DBError("loadByProgram", result);
    }

    return false;
}

void ScheduledRecording::loadByID(QSqlDatabase* db, int recordID) {
    id->setValue(recordID);
    load(db);
}

RecordingType ScheduledRecording::getRecordingType(void) const {
    return (RecordingType)(type->getValue().toInt());
}

void ScheduledRecording::setRecordingType(RecordingType newType) {
    type->setValue((int)newType);
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
    if (getRecordingType() == kSingleRecord ||
        getRecordingType() == kFindOneRecord)
        remove(db);

    QString sqltitle = proginfo.title;
    QString sqlsubtitle = proginfo.subtitle;
    QString sqldescription = proginfo.description;
    QString sqlcategory = proginfo.category;

    sqltitle.replace(QRegExp("\""), QString("\\\""));
    sqlsubtitle.replace(QRegExp("\""), QString("\\\""));
    sqldescription.replace(QRegExp("\""), QString("\\\""));
    sqlcategory.replace(QRegExp("\""), QString("\\\""));

    QString query = QString("INSERT INTO oldrecorded (chanid,starttime,endtime,title,"
                            "subtitle,description,category) "
                            "VALUES(%1,\"%2\",\"%3\",\"%4\",\"%5\",\"%6\","
                            "\"%7\");")
        .arg(proginfo.chanid)
        .arg(proginfo.recstartts.toString(Qt::ISODate))
        .arg(proginfo.recendts.toString(Qt::ISODate))
        .arg(sqltitle.utf8()) 
        .arg(sqlsubtitle.utf8())
        .arg(sqldescription.utf8())
        .arg(sqlcategory.utf8());

    QSqlQuery result = db->exec(query);
    if (!result.isActive())
    {
        MythContext::DBError("doneRecording", result);
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

    vbox->addWidget(type->configWidget(this, dialog));

    QHBoxLayout* hbox = new QHBoxLayout(vbox, (int)(20 * wmult));
    QVBoxLayout* vbox1 = new QVBoxLayout(hbox, (int)(10 * wmult));
    QVBoxLayout* vbox2 = new QVBoxLayout(hbox, (int)(10 * wmult));

    vbox1->addWidget(profile->configWidget(this, dialog));
    vbox1->addWidget(recpriority->configWidget(this, dialog));
    vbox1->addWidget(autoexpire->configWidget(this, dialog));
    vbox1->addWidget(maxepisodes->configWidget(this, dialog));
    vbox1->addWidget(maxnewest->configWidget(this, dialog));

    vbox2->addWidget(recgroup->configWidget(this, dialog));
    vbox2->addWidget(preroll->configWidget(this, dialog));
    vbox2->addWidget(postroll->configWidget(this, dialog));
    vbox2->addWidget(dupmethod->configWidget(this, dialog));
    vbox2->addWidget(dupin->configWidget(this, dialog));

    MythPushButton *button = new MythPushButton(tr("See a list of all up-coming"
                                                   " episodes/playtimes."),
                                                dialog);
    connect(button, SIGNAL(clicked()), this, SLOT(runProgList()));
    vbox->addWidget(button);

    return dialog;
}

void ScheduledRecording::runProgList(void)
{
    ProgLister *pl = new ProgLister(plTitle, title->getValue(),
                                    QSqlDatabase::database(),
                                    gContext->GetMainWindow(), "proglist");
    pl->exec();
    delete pl;
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
