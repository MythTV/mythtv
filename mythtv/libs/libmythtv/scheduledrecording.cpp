#include "scheduledrecording.h"
#include "programinfo.h"
#include "recordingprofile.h"
#include "mythcontext.h"
#include "proglist.h"

#include <qlayout.h>
#include <qlabel.h>
#include <qapplication.h>
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
        addSelection(QObject::tr("Record in this timeslot every day"),
                     QString::number(kTimeslotRecord));
        addSelection(QObject::tr("Record at any time on this channel"),
                     QString::number(kChannelRecord));
        addSelection(QObject::tr("Record at any time on any channel"),
                     QString::number(kAllRecord));
        addSelection(QObject::tr("Record in this timeslot every week"),
                     QString::number(kWeekslotRecord));
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
        addSelection(QObject::tr("(unspecified)"), "0");
        RecordingProfile::fillSelections(db, this, 0);
    };
};

class SRRecordDups: public ComboBoxSetting, public SRSetting {
public:
    SRRecordDups(const ScheduledRecording& parent):
        SRSetting(parent, "recorddups") {
        setLabel(QObject::tr("Record duplicates"));
        addSelection(QObject::tr("Never"), QString::number(kRecordDupsNever));
        addSelection(QObject::tr("If deleted"),
                     QString::number(kRecordDupsIfDeleted));
        addSelection(QObject::tr("Always"), QString::number(kRecordDupsAlways));
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
        SpinBoxSetting(-60, 60, 1),
        SRSetting(parent, "preroll") {
        setLabel(QObject::tr("Pre-Roll Minutes"));
    };
};

class SRPostRoll: public SpinBoxSetting, public SRSetting {
public:
    SRPostRoll(const ScheduledRecording& parent):
        SpinBoxSetting(-60, 60, 1),
        SRSetting(parent, "postroll") {
        setLabel(QObject::tr("Post-Roll Minutes"));
    };
};

class SRMaxEpisodes: public SpinBoxSetting, public SRSetting {
public:
    SRMaxEpisodes(const ScheduledRecording& parent):
        SpinBoxSetting(0, 60, 1),
        SRSetting(parent, "maxepisodes") {
        setLabel(QObject::tr("Maximum number of episodes to keep "
                             "(set to 0 for ALL)"));
    };
};

class SRMaxNewest: public CheckBoxSetting, public SRSetting {
public:
    SRMaxNewest(const ScheduledRecording& parent):
        SRSetting(parent, "maxnewest") {
        setLabel(QObject::tr("Record new episodes and delete oldest once "
                             "maximum count is reached"));
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

class SRRank: public SpinBoxSetting, public SRSetting {
public:
    SRRank(const ScheduledRecording& parent): 
        SpinBoxSetting(-99, 99, 1),
        SRSetting(parent, "rank") {
        setLabel(QObject::tr("Rank"));
        setValue(0);
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
    addChild(recorddups = new SRRecordDups(*this));
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
    addChild(rank = new SRRank(*this));

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
    rank->setValue(proginfo->rank.toInt());
    autoexpire->setValue(gContext->GetNumSetting("AutoExpireDefault", 0));
}

void ScheduledRecording::findAllProgramsToRecord(QSqlDatabase* db,
                                                 list<ProgramInfo*>& proglist) {
     QString query = QString(
"SELECT DISTINCT channel.chanid, channel.sourceid, "
"program.starttime, program.endtime, "
"program.title, program.subtitle, program.description, "
"channel.channum, channel.callsign, channel.name, "
"oldrecorded.starttime IS NOT NULL AS oldrecduplicate, program.category, "
"record.rank, record.recorddups, "
"recorded.starttime IS NOT NULL as recduplicate, record.type, "
"record.recordid, recordoverride.type, "
"program.starttime - INTERVAL record.preroll minute, "
"program.endtime + INTERVAL record.postroll minute "
"FROM record "
" INNER JOIN channel ON (channel.chanid = program.chanid) "
" INNER JOIN program ON (program.title = record.title) "
" LEFT JOIN oldrecorded ON "
"  ( "
"    oldrecorded.title IS NOT NULL AND oldrecorded.title <> '' AND program.title = oldrecorded.title "
"     AND "
"    oldrecorded.subtitle IS NOT NULL AND oldrecorded.subtitle <> '' AND program.subtitle = oldrecorded.subtitle "
"     AND "
"    oldrecorded.description IS NOT NULL AND oldrecorded.description <> '' AND program.description = oldrecorded.description"
"  ) "
" LEFT JOIN recorded ON "
"  ( "
"    recorded.title IS NOT NULL AND recorded.title <> '' AND program.title = recorded.title "
"     AND "
"    recorded.subtitle IS NOT NULL AND recorded.subtitle <> '' AND program.subtitle = recorded.subtitle "
"     AND "
"    recorded.description IS NOT NULL AND recorded.description <> '' AND program.description = recorded.description"
"  ) "
" LEFT JOIN recordoverride ON "
"  ( "
"    record.recordid = recordoverride.recordid "
"    AND program.chanid = recordoverride.chanid "
"    AND program.starttime = recordoverride.starttime "
"    AND program.endtime = recordoverride.endtime "
"    AND program.title = recordoverride.title "
"    AND program.subtitle = recordoverride.subtitle "
"    AND program.description = recordoverride.description "
"  ) "
"WHERE "
"((record.type = %1) " // allrecord
" OR "
" ((record.chanid = program.chanid) " // channel matches
"  AND "
"  ((record.type = %2) " // channelrecord
"   OR"
"   ((TIME_TO_SEC(record.starttime) = TIME_TO_SEC(program.starttime)) " // timeslot matches
"    AND "
"    ((record.type = %3) " // timeslotrecord
"     OR"
"     ((DAYOFWEEK(record.startdate) = DAYOFWEEK(program.starttime) "
"      AND "
"      ((record.type = %4) " // weekslotrecord
"       OR"
"       ((TO_DAYS(record.startdate) = TO_DAYS(program.starttime)) " // date matches
"        AND "
"        (TIME_TO_SEC(record.endtime) = TIME_TO_SEC(program.endtime)) "
"        AND "
"        (TO_DAYS(record.enddate) = TO_DAYS(program.endtime)) "
"        )"
"       )"
"      )"
"     )"
"    )"
"   )"
"  )"
" )"
");")
        .arg(kAllRecord)
        .arg(kChannelRecord)
        .arg(kTimeslotRecord)
        .arg(kWeekslotRecord);

     QSqlQuery result = db->exec(query);
     QDateTime now = QDateTime::currentDateTime();

     if (result.isActive() && result.numRowsAffected() > 0)
     {
         while (result.next()) {
             ProgramInfo *proginfo = new ProgramInfo;
             proginfo->recording = true;
             proginfo->chanid = result.value(0).toString();
             proginfo->sourceid = result.value(1).toInt();
             proginfo->startts = result.value(2).toDateTime();
             proginfo->endts = result.value(3).toDateTime();
             proginfo->title = QString::fromUtf8(result.value(4).toString());
             proginfo->subtitle = QString::fromUtf8(result.value(5).toString());
             proginfo->description = QString::fromUtf8(result.value(6).toString());
             proginfo->chanstr = result.value(7).toString();
             proginfo->chansign = result.value(8).toString();
             proginfo->channame = result.value(9).toString();
             proginfo->category = QString::fromUtf8(result.value(11).toString());
             proginfo->rank = result.value(12).toString();
             proginfo->recdups = RecordingDupsType(result.value(13).toInt());
             proginfo->rectype = RecordingType(result.value(15).toInt());
             proginfo->recordid = result.value(16).toInt();
             proginfo->override = result.value(17).toInt();
             proginfo->recstartts = result.value(18).toDateTime();
             proginfo->recendts = result.value(19).toDateTime();

             if (proginfo->override == 2)
             {
                 proginfo->recording = false;
                 proginfo->norecord = nrManualOverride;
             }
             else if (proginfo->rectype != kSingleRecord &&
                      proginfo->override != 1 &&
                      proginfo->recdups != kRecordDupsAlways)
             {
                 if (proginfo->recdups == kRecordDupsIfDeleted)
                 {
                     if (result.value(14).toInt())
                     {
                         proginfo->recording = false;
                         proginfo->norecord = nrCurrentRecording;
                     }
                 }
                 else
                 {
                     if (result.value(10).toInt())
                     {
                         proginfo->recording = false;
                         proginfo->norecord = nrPreviousRecording;
                     }
                 }
             }

             // would save many queries to create and populate a
             // ScheduledRecording and put it in the proginfo at the
             // same time, since it will be loaded later anyway with
             // multiple queries

             if (proginfo->title == QString::null)
                 proginfo->title = "";
             if (proginfo->subtitle == QString::null)
                 proginfo->subtitle = "";
             if (proginfo->description == QString::null)
                 proginfo->description = "";
             if (proginfo->category == QString::null)
                 proginfo->category = "";

             if (proginfo->endts < now)
                 delete proginfo;
             else 
                 proglist.push_back(proginfo);
         }
    }
    else if (!result.isActive())
        MythContext::DBError("findAllProgramsToRecord", result);
}

void ScheduledRecording::findAllScheduledPrograms(QSqlDatabase* db,
                                                  list<ProgramInfo*>& proglist)
{
    QString temptime, tempdate;
    QString query = QString("SELECT record.chanid, record.starttime, "
"record.startdate, record.endtime, record.enddate, record.title, "
"record.subtitle, record.description, record.rank, record.type, "
"channel.name, record.recordid FROM record "
"LEFT JOIN channel ON (channel.chanid = record.chanid) "
"ORDER BY title ASC;");

    QSqlQuery result = db->exec(query);

    if (result.isActive() && result.numRowsAffected() > 0)
        while (result.next()) 
        {
            ProgramInfo *proginfo = new ProgramInfo;
            proginfo->chanid = result.value(0).toString();
            proginfo->rectype = RecordingType(result.value(9).toInt());
            proginfo->recordid = result.value(11).toInt();

            if (proginfo->rectype == kSingleRecord || 
                proginfo->rectype == kTimeslotRecord ||
                proginfo->rectype == kWeekslotRecord) 
            {
                temptime = result.value(1).toString();
                tempdate = result.value(2).toString();
                proginfo->startts = QDateTime::fromString(tempdate+":"+temptime,
                                                          Qt::ISODate);
                temptime = result.value(3).toString();
                tempdate = result.value(4).toString();
                proginfo->endts = QDateTime::fromString(tempdate+":"+temptime,
                                                        Qt::ISODate);
            }
            else 
            {
                // put currentDateTime() in time fields to prevent
                // Invalid date/time warnings later
                proginfo->startts = QDateTime::currentDateTime();
                proginfo->startts.setTime(QTime(0,0));
                proginfo->endts = QDateTime::currentDateTime();
                proginfo->endts.setTime(QTime(0,0));
            }

            proginfo->title = QString::fromUtf8(result.value(5).toString());
            if (proginfo->rectype == kSingleRecord)
                proginfo->subtitle = QString::fromUtf8(result.value(6).toString());
            proginfo->description = QString::fromUtf8(result.value(7).toString());
            proginfo->rank = result.value(8).toString();
            proginfo->channame = result.value(10).toString();
            proginfo->recstartts = proginfo->startts;
            proginfo->recendts = proginfo->endts;

            if (proginfo->title == QString::null)
                proginfo->title = "";
            if (proginfo->subtitle == QString::null)
                proginfo->subtitle = "";
            if (proginfo->description == QString::null)
                proginfo->description = "";

            proglist.push_back(proginfo);
        }
}

bool ScheduledRecording::loadByProgram(QSqlDatabase* db,
                                       ProgramInfo* proginfo) {
    QString sqltitle(proginfo->title);

    m_pginfo = proginfo;

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
"    AND ")
        .arg(sqltitle.utf8())
        .arg(kAllRecord)
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
    QString query = QString("DELETE FROM record WHERE recordid = %1").arg(getRecordID());
    db->exec(query);
    query = QString("DELETE FROM recordoverride WHERE recordid = %1").arg(getRecordID());
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
    if (getRecordingType() == kSingleRecord)
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
        MythContext::DBError("doneRecording", result);

    // The addition of an entry to oldrecorded may affect near-future
    // scheduling decisions, so recalculate
    signalChange(db);
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
        MythContext::DBError("forgetHistory", result);
}

MythDialog* ScheduledRecording::dialogWidget(MythMainWindow *parent, 
                                             const char *name)
{
    float wmult, hmult;
    gContext->GetScreenSettings(wmult, hmult);

    MythDialog* dialog = new ConfigurationDialogWidget(parent, name);
    QVBoxLayout* vbox = new QVBoxLayout(dialog, (int)(10 * wmult));

    if (m_pginfo)
    {
        QGridLayout *grid = m_pginfo->DisplayWidget(this, dialog);
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
    vbox1->addWidget(rank->configWidget(this, dialog));
    vbox1->addWidget(autoexpire->configWidget(this, dialog));

    vbox2->addWidget(recorddups->configWidget(this, dialog));
    vbox2->addWidget(preroll->configWidget(this, dialog));
    vbox2->addWidget(postroll->configWidget(this, dialog));

    vbox->addWidget(maxepisodes->configWidget(this, dialog));
    vbox->addWidget(maxnewest->configWidget(this, dialog));

    MythPushButton *button = new MythPushButton(tr("See a list of all up-coming"
                                                   " episodes/playtimes."),
                                                dialog);
    connect(button, SIGNAL(clicked()), this, SLOT(runProgList()));
    vbox->addWidget(button);

    return dialog;
}

void ScheduledRecording::runProgList(void)
{
    ProgLister *pl = new ProgLister(title->getValue(),
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

void ScheduledRecording::setRank(const QString& newrank) {
    rank->setValue(newrank.toInt());
}

QString ScheduledRecording::getProfileName(void) const {
    return profile->getValue();
}
