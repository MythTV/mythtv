#include "scheduledrecording.h"
#include "programinfo.h"
#include "recordingprofile.h"
#include <qlayout.h>
#include <qlabel.h>
#include <qapplication.h>
#include <qlayout.h>
#include <qregexp.h>

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
            .arg(getValue());
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
        setLabel("When");
        addSelection("Do not record this program",
                     QString::number(ScheduledRecording::NotRecording));
        addSelection("Record only this showing of the program",
                     QString::number(ScheduledRecording::SingleRecord));
        addSelection("Record this program in this timeslot",
                     QString::number(ScheduledRecording::TimeslotRecord));
        addSelection("Record this program whenever it's shown on this channel",
                     QString::number(ScheduledRecording::ChannelRecord));
        addSelection("Record this program whenever it's shown anywhere",
                     QString::number(ScheduledRecording::AllRecord));
    };
};

class SRProfileSelector: public ComboBoxSetting, public SRSetting {
public:
    SRProfileSelector(const ScheduledRecording& parent):
        SRSetting(parent, "profile") {
        setLabel("Profile");
    };

    virtual void load(QSqlDatabase* db) {
        fillSelections(db);
        SRSetting::load(db);
    };

    virtual void fillSelections(QSqlDatabase* db) {
        clearSelections();
        addSelection("(unspecified)", "0");
        RecordingProfile::fillSelections(db, this);
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

ScheduledRecording::ScheduledRecording() {
    addChild(id = new ID());
    addChild(type = new SRRecordingType(*this));
    addChild(profile = new SRProfileSelector(*this));
    addChild(channel = new SRChannel(*this));
    addChild(title = new SRTitle(*this));
    addChild(subtitle = new SRSubtitle(*this));
    addChild(description = new SRDescription(*this));
    addChild(startTime = new SRStartTime(*this));
    addChild(endTime = new SREndTime(*this));
    addChild(startDate = new SRStartDate(*this));
    addChild(endDate = new SREndDate(*this));
}

void ScheduledRecording::fromProgramInfo(const ProgramInfo& proginfo) {
    channel->setValue(proginfo.chanid);
    title->setValue(proginfo.title);
    subtitle->setValue(proginfo.subtitle);
    description->setValue(proginfo.description);
    startTime->setValue(proginfo.startts.time());
    startDate->setValue(proginfo.startts.date());
    endTime->setValue(proginfo.endts.time());
    endDate->setValue(proginfo.endts.date());
}

void ScheduledRecording::findAllProgramsToRecord(QSqlDatabase* db,
                                                 list<ProgramInfo*>& proglist) {
     QString query = QString(
"SELECT channel.chanid, channel.sourceid, "
"program.starttime, program.endtime, "
"program.title, program.subtitle, program.description, "
"channel.channum, channel.callsign, channel.name, "
"record.recordid "
"FROM channel, program, record "
"WHERE "
"channel.chanid = program.chanid "
"AND "
"record.title = program.title "
"AND "

"((record.type = %1) " // allrecord
" OR "
" ((record.chanid = program.chanid) " // channel matches
"  AND "
"  ((record.type = %2) " // channelrecord
"   OR"
"   (((TIME_TO_SEC(record.starttime) = TIME_TO_SEC(program.starttime)) " // timeslot matches
"      AND "
"     (TIME_TO_SEC(record.endtime) = TIME_TO_SEC(program.endtime)) "
"    )"
"    AND "
"    ((record.type = %3) " // timeslotrecord
"     OR"
"     ((TO_DAYS(record.startdate) = TO_DAYS(program.starttime)) " // date matches
"      AND "
"      (TO_DAYS(record.enddate) = TO_DAYS(program.endtime)) "
"     )"
"    )"
"   )"
"  )"
" )"
");")
        .arg(AllRecord)
        .arg(ChannelRecord)
        .arg(TimeslotRecord);

     QSqlQuery result = db->exec(query);
     QDateTime now = QDateTime::currentDateTime();

     if (result.isActive() && result.numRowsAffected() > 0)
         while (result.next()) {
             ProgramInfo *proginfo = new ProgramInfo;
             proginfo->chanid = result.value(0).toString();
             proginfo->sourceid = result.value(1).toInt();
             proginfo->startts = result.value(2).toDateTime();
             proginfo->endts = result.value(3).toDateTime();
             proginfo->title = result.value(4).toString();
             proginfo->subtitle = result.value(5).toString();
             proginfo->description = result.value(6).toString();
             proginfo->chanstr = result.value(7).toString();
             proginfo->chansign = result.value(8).toString();
             proginfo->channame = result.value(9).toString();

             if (proginfo->title == QString::null)
                 proginfo->title = "";
             if (proginfo->subtitle == QString::null)
                 proginfo->subtitle = "";
             if (proginfo->description == QString::null)
                 proginfo->description = "";

             if (proginfo->endts < now)
                 delete proginfo;
             else 
                 proglist.push_back(proginfo);
             
         }
}

bool ScheduledRecording::loadByProgram(QSqlDatabase* db,
                                       const ProgramInfo& proginfo) {
    QString sqltitle(proginfo.title);
    // this doesn't have to be a QRegexp in qt 3.1+
    sqltitle.replace(QRegExp("\'"), "\\'");

    // prevent the SQL from breaking if chanid is null
    QString chanid = proginfo.chanid;
    if (!chanid || chanid == "")
         chanid = "0";

    QString query = QString(
"SELECT "
"recordid "
"FROM record "
"WHERE "
"record.title = '%1' "
"AND "
"((record.type = %2) " // allrecord
" OR "
" ((record.chanid = %3) " // channel matches
"  AND "
"  ((record.type = %4) " // channelrecord
"   OR"
"   (((TIME_TO_SEC(record.starttime) = TIME_TO_SEC('%5')) " // timeslot matches
"      AND "
"     (TIME_TO_SEC(record.endtime) = TIME_TO_SEC('%6')) "
"    )"
"    AND "
"    ((record.type = %7) " // timeslotrecord
"     OR"
"     ((TO_DAYS(record.startdate) = TO_DAYS('%8')) " // date matches
"      AND "
"      (TO_DAYS(record.enddate) = TO_DAYS('%9')) "
"     )"
"    )"
"   )"
"  )"
" )"
");")
        .arg(sqltitle)
        .arg(AllRecord)
        .arg(chanid)
        .arg(ChannelRecord)
        .arg(proginfo.startts.time().toString(Qt::ISODate))
        .arg(proginfo.endts.time().toString(Qt::ISODate))
        .arg(TimeslotRecord)
        .arg(proginfo.startts.date().toString(Qt::ISODate))
        .arg(proginfo.endts.date().toString(Qt::ISODate));

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

ScheduledRecording::RecordingType ScheduledRecording::getRecordingType(void) const {
    return (ScheduledRecording::RecordingType)(type->getValue().toInt());
}

void ScheduledRecording::setRecordingType(ScheduledRecording::RecordingType newType) {
    type->setValue((int)newType);
}

void ScheduledRecording::save(QSqlDatabase* db) {
    if (type->isChanged() && getRecordingType() == NotRecording)
        remove(db);
    else
        ConfigurationGroup::save(db);
    signalChange(db);
}

void ScheduledRecording::remove(QSqlDatabase* db) {
    QString query = QString("DELETE FROM record WHERE recordid = %1").arg(getRecordID());
    db->exec(query);
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
        query = "INSERT settings (value,data) "
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

void ScheduledRecording::doneRecording(QSqlDatabase* db, const ProgramInfo& proginfo) {
    (void)proginfo;

    if (getRecordingType() == SingleRecord)
        remove(db);
}

MythDialog* ScheduledRecording::dialogWidget(MythContext* context,
                                             QWidget* parent, const char* name) {
    float wmult, hmult;
    int screenwidth, screenheight;
    context->GetScreenSettings(screenwidth, wmult, screenheight, hmult);

    int bigfont = context->GetBigFontSize();
    int mediumfont = context->GetMediumFontSize();

    MythDialog* dialog = new ConfigurationDialogWidget(context, parent, name);
    QVBoxLayout* vbox = new QVBoxLayout(dialog);

    QGridLayout *grid = new QGridLayout(vbox, 4, 2, (int)(10*wmult));
    
    QLabel *titlefield = new QLabel(title->getValue(), dialog);
    titlefield->setBackgroundOrigin(QWidget::WindowOrigin);
    titlefield->setFont(QFont("Arial", (int)(bigfont * hmult), QFont::Bold));

    QString dateFormat = context->GetSetting("DateFormat");
    QString timeFormat = context->GetSetting("TimeFormat");
    
    QString dateText = "Date: ";
    switch (getRecordingType()) {
    case SingleRecord:
        dateText += QDateTime(startDate->dateValue(),
                             startTime->timeValue()).toString(dateFormat + " " + timeFormat);
        break;
    case TimeslotRecord:
        dateText += QString("%1 - %2")
            .arg(startTime->timeValue().toString(timeFormat))
            .arg(endTime->timeValue().toString(timeFormat));
        break;
    case ChannelRecord:
    case AllRecord:
    case NotRecording:
        dateText += "(any)";
        break;
    }

    QLabel* date = new QLabel(dateText,dialog);
    date->setBackgroundOrigin(QWidget::WindowOrigin);

    QLabel *subtitlelabel = new QLabel("Episode:", dialog);
    subtitlelabel->setBackgroundOrigin(QWidget::WindowOrigin);
    QLabel *subtitlefield = new QLabel(subtitle->getValue(), dialog);
    subtitlefield->setBackgroundOrigin(QWidget::WindowOrigin);
    subtitlefield->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    QLabel *descriptionlabel = new QLabel("Description:", dialog);
    descriptionlabel->setBackgroundOrigin(QWidget::WindowOrigin);
    QLabel *descriptionfield = new QLabel(description->getValue(), dialog);
    descriptionfield->setBackgroundOrigin(QWidget::WindowOrigin);
    descriptionfield->setAlignment(Qt::WordBreak | Qt::AlignLeft | 
                                   Qt::AlignTop);

    qApp->processEvents();

    int descwidth = (int)(800 * wmult) - descriptionlabel->width() - 100;
    int titlewidth = (int)(760 * wmult);

    titlefield->setMinimumWidth(titlewidth);
    titlefield->setMaximumWidth(titlewidth);

    subtitlefield->setMinimumWidth(descwidth);
    subtitlefield->setMaximumWidth(descwidth);

    descriptionfield->setMinimumWidth(descwidth);
    descriptionfield->setMaximumWidth(descwidth);

    grid->addMultiCellWidget(titlefield, 0, 0, 0, 1, Qt::AlignLeft);
    grid->addMultiCellWidget(date, 1, 1, 0, 1, Qt::AlignLeft);
    grid->addWidget(subtitlelabel, 2, 0, Qt::AlignLeft | Qt::AlignTop);
    grid->addWidget(subtitlefield, 2, 1, Qt::AlignLeft | Qt::AlignTop);
    grid->addWidget(descriptionlabel, 3, 0, Qt::AlignLeft | Qt::AlignTop);
    grid->addWidget(descriptionfield, 3, 1, Qt::AlignLeft | Qt::AlignTop);

    grid->setColStretch(1, 2);
    grid->setRowStretch(3, 1);

    //vbox->addStretch(1);

    QFrame *f = new QFrame(dialog);
    f->setFrameStyle(QFrame::HLine | QFrame::Plain);
    f->setLineWidth((int)(4 * hmult));
    vbox->addWidget(f);    

    vbox->addWidget(type->configWidget(this, dialog));
    vbox->addWidget(profile->configWidget(this, dialog));

    return dialog;
}

void ScheduledRecording::fillSelections(QSqlDatabase* db, SelectSetting* setting) {
    QSqlQuery result = db->exec("SELECT recordid FROM record");
    if (result.isActive() && result.numRowsAffected() > 0)
        while (result.next()) {
            int id = result.value(0).toInt();

            ScheduledRecording sr;
            sr.loadByID(db, id);

            QString label;

            switch (sr.getRecordingType()) {
            case AllRecord:
                label = QString("%1").arg(sr.title->getValue());
                break;
            case ChannelRecord:
                label = QString("%1 on channel %2")
                    .arg(sr.title->getValue())
                    .arg(sr.channel->getSelectionLabel());
                break;
            case TimeslotRecord:
                label = QString("%1 on channel %2 (%3 - %4)")
                    .arg(sr.title->getValue())
                    .arg(sr.channel->getSelectionLabel())
                    .arg(sr.startTime->timeValue().toString())
                    .arg(sr.endTime->timeValue().toString());
                break;
            case SingleRecord:
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


int ScheduledRecordingEditor::exec(MythContext* context, QSqlDatabase* db) {
    m_context = context;
    while (ConfigurationDialog::exec(context, db) == QDialog::Accepted)
        open(getValue().toInt());

    return QDialog::Rejected;
}

void ScheduledRecordingEditor::open(int id) {
    ScheduledRecording* sr = new ScheduledRecording();

    if (id != 0)
        sr->loadByID(db,id);

    if (sr->exec(m_context, db) == QDialog::Accepted)
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

int ScheduledRecording::getProfileID(void) const {
    return profile->getValue().toInt();
}
