#include "scheduledrecording.h"
#include "programinfo.h"
#include "recordingprofile.h"
#include "mythcontext.h"
#include "proglist.h"
#include "previouslist.h"
#include "sr_items.h"
#include "sr_root.h"
#include "sr_dialog.h"
#include "jobqueue.h"
#include "mythdbcon.h"
#include "viewschdiff.h"

#include <qlayout.h>
#include <qlabel.h>
#include <qapplication.h>
#include <qregexp.h>
#include <qsqlquery.h>

// NOTE: if this changes, you _MUST_ update the RecTypePriority function 
// in recordingtypes.cpp.

ScheduledRecording::ScheduledRecording() :
    ConfigurationGroup(true, true, false, false)
{
    m_pginfo = NULL;
    type = NULL;
    search = NULL;
    profile = NULL;
    dupin = NULL;
    dupmethod = NULL;
    autoexpire = NULL;
    autotranscode = NULL;
    transcoder = NULL;
    autocommflag = NULL;
    autouserjob1 = NULL;
    autouserjob2 = NULL;
    autouserjob3 = NULL;
    autouserjob4 = NULL;
    startoffset = NULL;
    endoffset = NULL;
    maxepisodes = NULL;
    maxnewest = NULL;
    m_dialog = NULL;
    recpriority = NULL;
    recgroup = NULL;
    playgroup = NULL;
    prefinput = NULL;
    inactive = NULL;
    searchType = "";
    searchForWhat = "";
        
    longChannelFormat = gContext->GetSetting("LongChannelFormat", "<num> <name>");
    channelFormat = gContext->GetSetting("ChannelFormat", "<num> <sign>");
    timeFormat = gContext->GetSetting("TimeFormat", "h:mm AP");
    dateFormat = gContext->GetSetting("DateFormat", "ddd MMMM d");
    shortDateFormat = gContext->GetSetting("ShortDateFormat", "M/d");
    
    addChild(id = new ID());
    
    channel = new SRChannel(this);
    station = new SRStation(this);
    title = new SRTitle(this);
    subtitle = new SRSubtitle(this);
    description = new SRDescription(this);
    startTime = new SRStartTime(this);
    endTime = new SREndTime(this);
    startDate = new SRStartDate(this);
    endDate = new SREndDate(this);
    category = new SRCategory(this);
    seriesid = new SRSeriesid(this);
    programid = new SRProgramid(this);
    findday = new SRFindDay(this);
    findtime = new SRFindTime(this);
    findid = new SRFindId(this);
    parentid = new SRParentId(this);
    search = new SRRecSearchType(this);
    rootGroup = new RootSRGroup(this);
}

ScheduledRecording::~ScheduledRecording() 
{
    // rootGroup is unique among this class' member variables in
    // that it's not self-managed
    if (!rootGroup.isNull())
        delete rootGroup;
}

void ScheduledRecording::load()
{
    if (getRecordID())
    {
        ConfigurationGroup::load();
        
        QString tmpType = type->getValue();
        type->clearSelections();
        if (tmpType.toInt() == kOverrideRecord ||
            tmpType.toInt() == kDontRecord)
            type->addOverrideSelections();
        else
            type->addNormalSelections(!station->getValue().isEmpty(),
                                      search->intValue() == kManualSearch);
       
        type->setValue(tmpType);
        type->setUnchanged();

        fetchChannelInfo();
    }
}



void ScheduledRecording::loadByProgram(const ProgramInfo* proginfo) 
{
    m_pginfo = proginfo;
    
    if (proginfo->recordid)
        loadByID(proginfo->recordid);
    else
        setDefault(true);

    if (search->intValue() == kNoSearch ||
        search->intValue() == kManualSearch)
    {
        setProgram(proginfo);
        if (!proginfo->recordid)
            playgroup->setValue(PlayGroup::GetInitialName(proginfo));
    }
}

void ScheduledRecording::loadBySearch(RecSearchType lsearch,
                                      QString textname,
                                      QString forwhat)
{
    QString from = "";
    loadBySearch(lsearch, textname, from, forwhat);
}
void ScheduledRecording::loadBySearch(RecSearchType lsearch,
                                      QString textname,
                                      QString from,
                                      QString forwhat)
{
    MSqlQuery query(MSqlQuery::InitCon());

    int rid = 0;
    query.prepare("SELECT recordid FROM record WHERE "
                  "search = :SEARCH AND description LIKE :FORWHAT");
    query.bindValue(":SEARCH", lsearch);
    query.bindValue(":FORWHAT", forwhat);

    if (query.exec() && query.isActive())
    {
        if (query.next())
            rid = query.value(0).toInt();
    }
    else
        MythContext::DBError("loadBySearch", query);

    if (rid)
        loadByID(rid);
    else
    {
        setDefault(false);
        search->setValue(lsearch);
        searchForWhat = forwhat;
        
        switch (lsearch)
        {
        case kPowerSearch:
            searchType = "(" +  QObject::tr("Power Search") + ")";
            break;
        case kTitleSearch:
            searchType = "(" +  QObject::tr("Title Search") + ")";
            break;
        case kKeywordSearch:
            searchType = "(" +  QObject::tr("Keyword Search") + ")";
            break;
        case kPeopleSearch:
            searchType = "(" +  QObject::tr("People Search") + ")";
            break;
        default:
            searchType = "(" +  QObject::tr("Unknown Search") + ")";
            break;
        }
        QString ltitle = QString("%1 %2").arg(textname).arg(searchType);
        title->setValue(ltitle);
        subtitle->setValue(from);
        description->setValue(forwhat);
        findday->setValue((startDate->dateValue().dayOfWeek() + 1) % 7);
        QDate epoch = QDate::QDate (1970, 1, 1);
        findid->setValue(epoch.daysTo(startDate->dateValue()) + 719528);
    } 
}

void ScheduledRecording::modifyPowerSearchByID(int rid,
                                               QString textname,
                                               QString forwhat)
{
    QString from = "";
    modifyPowerSearchByID(rid, textname, from, forwhat);
}

void ScheduledRecording::modifyPowerSearchByID(int rid,
                                               QString textname,
                                               QString from,
                                               QString forwhat)
{
    if (rid <= 0)
        return;

    loadByID(rid);
    if (search->intValue() != kPowerSearch)
        return;

    QString ltitle = textname + " (" + QObject::tr("Power Search") + ")";
    title->setValue(ltitle);
    subtitle->setValue(from);
    description->setValue(forwhat);
}

void ScheduledRecording::fetchChannelInfo()
{
    
    if (channel->getValue().toInt() > 0)
    {
        MSqlQuery query(MSqlQuery::InitCon());

        QString queryStr(QString("SELECT channum, callsign, name FROM channel "
                                 "WHERE chanid = '%1';").arg(channel->getValue()));
    
        query.prepare(queryStr);
        
        if (query.exec() && query.isActive() && query.size() > 0)
        {
            query.next();
            chanstr =  query.value(0).toString();
            chansign =  query.value(1).toString();
            channame =  query.value(2).toString();
        }
    }
    else
    {
        chanstr =  "";
        chansign =  "";
        channame =  "";
    }
}



QString ScheduledRecording::ChannelText(QString format)
{
    format.replace("<num>", chanstr)
          .replace("<sign>", chansign)
          .replace("<name>", channame);

    return format;
}


void ScheduledRecording::ToMap(QMap<QString, QString>& progMap)
{
    QString searchtitle = "";

    if (m_pginfo && search->intValue() != kNoSearch &&
                    search->intValue() != kManualSearch)
    {
        searchtitle = title->getValue();
        m_pginfo->ToMap(progMap);
    }
    else
    {
        progMap["title"] = title->getValue();
        progMap["subtitle"] = subtitle->getValue();
        progMap["description"] = description->getValue();
        
        progMap["category"] = category->getValue();
        progMap["callsign"] = station->getValue();
        
        progMap["starttime"] = startTime->getValue();
        progMap["startdate"] = startDate->getValue();
        progMap["endtime"] = endTime->getValue();
        progMap["enddate"] = endTime->getValue();
        
        
        if (chanstr.isEmpty())
        {
            progMap["channum"] = QObject::tr("Any");
            progMap["longchannel"] = QObject::tr("Any");
            
        }
        else
        {
            progMap["channum"] = chanstr;
            progMap["longchannel"] = ChannelText(longChannelFormat);
        }
        
        progMap["chanid"] = channel->getValue();
        progMap["channel"] = station->getValue();
        
        QDateTime startts(startDate->dateValue(), startTime->timeValue());
        QDateTime endts(endDate->dateValue(), endTime->timeValue());
        
        QString length;
        int hours, minutes, seconds;
        seconds = startts.secsTo(endts);
        
        minutes = seconds / 60;
        progMap["lenmins"] = QString("%1 %2").arg(minutes).arg(QObject::tr("minutes"));
        hours   = minutes / 60;
        minutes = minutes % 60;
        length.sprintf("%d:%02d", hours, minutes);
        
        progMap["lentime"] = length;
        
        progMap["timedate"] = startts.date().toString(dateFormat) + ", " +
                              startts.time().toString(timeFormat) + " - " +
                              endts.time().toString(timeFormat);

        progMap["shorttimedate"] = startts.date().toString(shortDateFormat) + ", " +
                                   startts.time().toString(timeFormat) + " - " +
                                   endts.time().toString(timeFormat);
    }

    int recType = type->getValue().toInt();

    if (recType == kFindDailyRecord || recType == kFindWeeklyRecord)
    {
        QString findfrom = findtime->timeValue().toString(timeFormat);
        if (recType == kFindWeeklyRecord)
        {
            int daynum = (findday->intValue() + 5) % 7 + 1;
            findfrom = QString("%1, %2")
                               .arg(QDate::shortDayName(daynum))
                               .arg(findfrom);
        }
        progMap["subtitle"] = QString("(%1 %2) %3")
                                      .arg(findfrom).arg(tr("or later"))
                                      .arg(progMap["subtitle"]);
    }

    progMap["searchtype"] = searchType;
    progMap["searchforwhat"] = searchForWhat;

    if (searchtitle != "")
    {
        progMap["banner"] = searchtitle;

        if (progMap["subtitle"] == "")
            progMap["episode"] = progMap["title"];
        else
            progMap["episode"] = QString("%1 - \"%2\"")
                                        .arg(progMap["title"])
                                        .arg(progMap["subtitle"]);
    }
    else
    {
        progMap["banner"] = progMap["title"];
        progMap["episode"] = progMap["subtitle"];
    }
}


void ScheduledRecording::loadByID(int recordID) {
    id->setValue(recordID);
    load();
}

RecordingType ScheduledRecording::getRecordingType(void) const {
    return (RecordingType)(type->getValue().toInt());
}

void ScheduledRecording::setRecordingType(RecordingType newType) {
    type->setValue(QString::number(newType));
}

RecSearchType ScheduledRecording::getSearchType(void) const {
    return (RecSearchType)(search->intValue());
}

void ScheduledRecording::setSearchType(RecSearchType newType) {
    if (type->getValue().toInt() == kOverrideRecord ||
        type->getValue().toInt() == kDontRecord)
    {
        VERBOSE(VB_IMPORTANT, "Attempt to set search type for "
                "override recording");
        return;
    }
    search->setValue(newType);
    type->clearSelections();
    type->addNormalSelections(!station->getValue().isEmpty(),
                              search->intValue() == kManualSearch);
}

int ScheduledRecording::GetAutoExpire(void) const {
    return(autoexpire->getValue().toInt());
}

void ScheduledRecording::SetAutoExpire(int expire) {
    autoexpire->setValue(expire);
}

int ScheduledRecording::GetTranscoder(void) const {
    return transcoder->getValue().toInt();
}

int ScheduledRecording::GetAutoRunJobs(void) const {
    int result = 0;

    if (autotranscode->getValue().toInt())
        result |= JOB_TRANSCODE;
    if (autocommflag->getValue().toInt())
        result |= JOB_COMMFLAG;
    if (autouserjob1->getValue().toInt())
        result |= JOB_USERJOB1;
    if (autouserjob2->getValue().toInt())
        result |= JOB_USERJOB2;
    if (autouserjob3->getValue().toInt())
        result |= JOB_USERJOB3;
    if (autouserjob4->getValue().toInt())
        result |= JOB_USERJOB4;

    return result;
}

int ScheduledRecording::GetMaxEpisodes(void) const {
    return(maxepisodes->getValue().toInt());
}

bool ScheduledRecording::GetMaxNewest(void) const {
    return(maxnewest->getValue().toInt());
}

void ScheduledRecording::save(void)
{
    // NOTE: we can not use a default value for send(bool) because
    // anyone calling save on a parent type pointer will then not 
    // use our version of the virtual funcation.
    save(true);
}

void ScheduledRecording::save(bool sendSig)
{
    if (type->isChanged() && getRecordingType() == kNotRecording)
    {
        remove();
    }
    else
    {
        ConfigurationGroup::save();

        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare(
            "UPDATE recorded "
                "SET recpriority = :RECPRIORITY, "
                    "transcoder = :TRANSCODER, playgroup = :PLAYGROUP "
                "WHERE recordid = :RECORDID ;");
        query.bindValue(":RECPRIORITY", getRecPriority());
        query.bindValue(":TRANSCODER", transcoder->getValue().toInt());
        query.bindValue(":PLAYGROUP", playgroup->getValue());
        query.bindValue(":RECORDID", getRecordID());

        if (!query.exec())
            MythContext::DBError("UPDATE recorded", query);
    }
    if (sendSig)
        signalChange(getRecordID());
}

void ScheduledRecording::save(QString destination) 
{
    if (type->isChanged() && getRecordingType() == kNotRecording)
    {
        remove();
    }
    else
    {
        ConfigurationGroup::save(destination);
    }
}

void ScheduledRecording::remove() 
{
    int rid = getRecordID();

    if (!rid)
        return;

    QString querystr;
    MSqlQuery query(MSqlQuery::InitCon());
    querystr = QString("DELETE FROM record WHERE recordid = %1").arg(rid);
    query.prepare(querystr);
    query.exec();

    querystr = QString("DELETE FROM oldfind WHERE recordid = %1").arg(rid);
    query.prepare(querystr);
    query.exec();
}

void ScheduledRecording::signalChange(int recordid) 
{
    if (gContext->IsBackend())
    {
        MythEvent me(QString("RESCHEDULE_RECORDINGS %1").arg(recordid));
        gContext->dispatch(me);
    }
    else
    {
        QStringList slist;
        slist << QString("RESCHEDULE_RECORDINGS %1").arg(recordid);
        if (!gContext->SendReceiveStringList(slist))
            VERBOSE(VB_IMPORTANT, QString("Error rescheduling id %1 "
                                    "in ScheduledRecording::signalChange")
                                    .arg(recordid));
    }
}

void ScheduledRecording::doneRecording(ProgramInfo& proginfo) 
{
    proginfo.recstatus = rsRecorded;

    QString msg = "Finished recording";
    QString subtitle = proginfo.subtitle.isEmpty() ? "" :
                                    QString(" \"%1\"").arg(proginfo.subtitle);
    QString details = QString("%1%2: channel %3")
                                    .arg(proginfo.title)
                                    .arg(subtitle)
                                    .arg(proginfo.chanid);

    VERBOSE(VB_GENERAL, QString("%1 %2").arg(msg).arg(details).local8Bit());
    gContext->LogEntry("scheduler", LP_NOTICE, msg, details);
}

void ScheduledRecording::runTitleList(void)
{
    ProgLister *pl = NULL;

    if (search->intValue())
    {
        if (m_pginfo)
        {
            pl = new ProgLister(plTitle, m_pginfo->title, "",
                                gContext->GetMainWindow(), "proglist");
        }
        else
        {
            QString trimTitle = title->getValue();
            trimTitle.remove(QRegExp(" \\(.*\\)$"));
            pl = new ProgLister(plTitle, trimTitle, "",
                                gContext->GetMainWindow(), "proglist");
        }
    }
    else
    {
        pl = new ProgLister(plTitle, title->getValue(), "",
                            gContext->GetMainWindow(), "proglist");
    }
    pl->exec();
    delete pl;
}

void ScheduledRecording::runRuleList(void)
{
    ProgLister *pl = NULL;

    if (getRecordID())
    {
        pl = new ProgLister(plRecordid,
                            QString("%1").arg(getRecordID()), "",
                            gContext->GetMainWindow(), "proglist");
    }
    else
    {
        pl = new ProgLister(plTitle, title->getValue(), "",
                            gContext->GetMainWindow(), "proglist");
    }
    pl->exec();
    delete pl;
}

void ScheduledRecording::runPrevList(void)
{
    PreviousList *pb = NULL;

    pb = new PreviousList(gContext->GetMainWindow(), "proglist",
                          getRecordID(), title->getValue());
    pb->exec();
    delete pb;
}

void ScheduledRecording::runShowDetails(void)
{
    if (m_pginfo)
        m_pginfo->showDetails();
}

void ScheduledRecording::fillSelections(SelectSetting* setting) 
{
    MSqlQuery result(MSqlQuery::InitCon());
    result.prepare("SELECT recordid FROM record");
    if (result.exec() && result.isActive() && result.size() > 0)
    {
        while (result.next()) 
        {
            int recid = result.value(0).toInt();

            ScheduledRecording sr;
            sr.loadByID(recid);

            QString label;
            QString weekly = "";

            switch (sr.getRecordingType()) {
            case kAllRecord:
                label = QString("%1").arg(sr.title->getValue());
                break;
            case kFindOneRecord:
                label = QString("%1").arg(sr.title->getValue());
                break;
            case kFindWeeklyRecord:
                weekly = QDate(sr.startDate->dateValue()).toString("dddd")+"s ";
            case kFindDailyRecord:
                label = QString("%1 (after %2%3")
                    .arg(sr.title->getValue())
                    .arg(weekly)
                    .arg(sr.findtime->timeValue().toString());
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

            setting->addSelection(label, QString::number(recid));
        }
    }
}

void ScheduledRecordingEditor::load() 
{
    clearSelections();
    ScheduledRecording::fillSelections(this);
}


int ScheduledRecordingEditor::exec() 
{
    while (ConfigurationDialog::exec() == QDialog::Accepted)
        open(getValue().toInt());

    return QDialog::Rejected;
}

void ScheduledRecordingEditor::open(int id) {
    ScheduledRecording* sr = new ScheduledRecording();

    if (id != 0)
        sr->loadByID(id);

    if (sr->exec() == QDialog::Accepted)
        sr->save();
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

int ScheduledRecording::getRecPriority(void) const {
    return recpriority->getValue().toInt();
}

void ScheduledRecording::setRecPriority(int newrecpriority) {
    recpriority->setValue(newrecpriority);
}

void ScheduledRecording::setRecGroup(const QString& newrecgroup) {
    recgroup->setValue(newrecgroup);
}

QString ScheduledRecording::GetRecGroup(void) const {
    return recgroup->getValue();
}

void ScheduledRecording::setPlayGroup(const QString& newplaygroup) {
    playgroup->setValue(newplaygroup);
}

QString ScheduledRecording::getProfileName(void) const {
    return profile->getValue();
}

QString ScheduledRecording::getRecordTitle(void) const {
    return title->getValue();
}

QString ScheduledRecording::getRecordSubTitle(void) const {
    return subtitle->getValue();
}

QString ScheduledRecording::getRecordDescription(void) const {
    return description->getValue();
}

MythDialog* ScheduledRecording::dialogWidget(MythMainWindow *parent,
                                             const char *name)
{
    (void) parent;
    (void) name;
#ifdef USING_FRONTEND
    MythDialog* dlg = new RecOptDialog(this, parent, name);
    rootGroup->setDialog(dlg);
    return dlg;
#else
    VERBOSE(VB_IMPORTANT, "You must compile the frontend "
            "to use a dialogWidget.");
    return NULL;
#endif
}



void ScheduledRecording::setDefault(bool haschannel)
{
    id->setValue(0);
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
    findday->setValue(-1);
    findtime->setValue(QTime::fromString("00:00:00", Qt::ISODate));
    findid->setValue(0);
    parentid->setValue(0);
    category->setValue("");
    search->setValue(kNoSearch);

     
    
    if (!type)   
    {
        cerr << "No type object" << endl;
        return;
    }
    
    type->clearSelections();
    type->addNormalSelections(haschannel, search->intValue() == kManualSearch);
    type->setValue(kNotRecording);
    
    profile->fillSelections();
    profile->setValue(QObject::tr("Default"));
    
    dupin->setValue(kDupsInAll);
    dupmethod->setValue(kDupCheckSubDesc);
    maxepisodes->setValue(0);
    startoffset->setValue(gContext->GetNumSetting("DefaultStartOffset", 0));
    endoffset->setValue(gContext->GetNumSetting("DefaultEndOffset", 0));
    maxnewest->setValue(0);
    recpriority->setValue(0);
    
    autoexpire->setValue(gContext->GetNumSetting("AutoExpireDefault", 0));

    autotranscode->setValue(gContext->GetNumSetting("AutoTranscode", 0));
    transcoder->fillSelections();
    transcoder->setValue(gContext->GetNumSetting("DefaultTranscoder",
                         RecordingProfile::TranscoderAutodetect));
    autocommflag->setValue(gContext->GetNumSetting("AutoCommercialFlag", 1));
    autouserjob1->setValue(gContext->GetNumSetting("AutoRunUserJob1", 0));
    autouserjob2->setValue(gContext->GetNumSetting("AutoRunUserJob2", 0));
    autouserjob3->setValue(gContext->GetNumSetting("AutoRunUserJob3", 0));
    autouserjob4->setValue(gContext->GetNumSetting("AutoRunUserJob4", 0));

    recgroup->fillSelections();    
    recgroup->setValue("Default");
    playgroup->fillSelections();    
    playgroup->setValue("Default");
    prefinput->fillSelections();    
    prefinput->setValue(0);

    inactive->setValue(0);
}

void ScheduledRecording::setProgram(const ProgramInfo *proginfo)
{
    m_pginfo = proginfo;
    if (proginfo)
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
        if (findday->intValue() < 0)
        {
            findday->setValue((proginfo->startts.date().dayOfWeek() + 1) % 7);
            findtime->setValue(proginfo->startts.time());

            QDate epoch = QDate::QDate (1970, 1, 1);
            findid->setValue(epoch.daysTo(proginfo->startts.date()) + 719528);
        }
        else
        {
            if (findid->intValue() > 0)
                findid->setValue(proginfo->findid);
            else
            {
                QDate epoch = QDate::QDate (1970, 1, 1);
                findid->setValue(epoch.daysTo(proginfo->startts.date()) +
                                              719528);
            }
        }
        category->setValue(proginfo->category);
        
        fetchChannelInfo();
    }
}

void ScheduledRecording::makeOverride(void)
{
    if (type->getValue().toInt() == kOverrideRecord ||
        type->getValue().toInt() == kDontRecord)
        return;

    id->setValue(0);
    type->clearSelections();
    type->addOverrideSelections();
    type->setValue(kNotRecording);
    inactive->setValue(0);

    if (search->intValue() == kManualSearch)
        search->setChanged();
    else
        search->setValue(kNoSearch);

    setProgram(m_pginfo);

    parentid->setValue(m_pginfo->recordid);

    profile->setChanged();
    dupin->setChanged();
    dupmethod->setChanged();
    autoexpire->setChanged();
    autotranscode->setChanged();
    autocommflag->setChanged();
    autouserjob1->setChanged();
    autouserjob2->setChanged();
    autouserjob3->setChanged();
    autouserjob4->setChanged();
    maxepisodes->setChanged();
    maxnewest->setChanged();
    startoffset->setChanged();
    endoffset->setChanged();
    recpriority->setChanged();
    recgroup->setChanged();
    playgroup->setChanged();
}

void
ScheduledRecording::testRecording()
{

    QString ttable = "record_tmp";

    MSqlQueryInfo dbcon = MSqlQuery::SchedCon();
    MSqlQuery query(dbcon);
    QString thequery;

    thequery ="SELECT GET_LOCK(:LOCK, 2);";
    query.prepare(thequery);
    query.bindValue(":LOCK", "DiffSchedule");
    query.exec();
    if (query.lastError().type() != QSqlError::None)
    {
        QString msg =
            QString("DB Error (Obtaining lock in testRecording): \n"
                    "Query was: %1 \nError was: %2 \n")
            .arg(thequery)
            .arg(MythContext::DBErrorMessage(query.lastError()));
        VERBOSE(VB_IMPORTANT, msg);
        return;
    }


    thequery = QString("DROP TABLE IF EXISTS %1;").arg(ttable);
    query.prepare(thequery);
    query.exec();
    if (query.lastError().type() != QSqlError::None)
    {
        QString msg =
            QString("DB Error (deleting old table in testRecording): \n"
                    "Query was: %1 \nError was: %2 \n")
            .arg(thequery)
            .arg(MythContext::DBErrorMessage(query.lastError()));
        VERBOSE(VB_IMPORTANT, msg);
        return;
    }

    thequery = QString("CREATE TABLE %1 SELECT * FROM record;").arg(ttable);
    query.prepare(thequery);
    query.exec();
    if (query.lastError().type() != QSqlError::None)
    {
        QString msg =
            QString("DB Error (create new table): \n"
                    "Query was: %1 \nError was: %2 \n")
            .arg(thequery)
            .arg(MythContext::DBErrorMessage(query.lastError()));
        VERBOSE(VB_IMPORTANT, msg);
        return;
    }

    bool resetID = false;
    if (getRecordID() == 0) {
        thequery = QString("SELECT MAX(recordid) FROM %1 ORDER BY recordid;")
                           .arg(ttable);
        query.prepare(thequery);
        query.exec();
        if (query.isActive() && query.next())
            id->setValue(query.value(0).toInt() + 1);
        else
            id->setValue(100000);
    }
    save(ttable);

    ViewScheduleDiff vsd(gContext->GetMainWindow(), "Preview Schedule Changes",
                         ttable, getRecordID(), title->getValue());

    thequery = "SELECT RELEASE_LOCK(:LOCK);";
    query.prepare(thequery);
    query.bindValue(":LOCK", "DiffSchedule");
    query.exec();
    if (query.lastError().type() != QSqlError::None)
    {
        QString msg =
            QString("DB Error (free lock): \n"
                    "Query was: %1 \nError was: %2 \n")
            .arg(thequery)
            .arg(MythContext::DBErrorMessage(query.lastError()));
        VERBOSE(VB_IMPORTANT, msg);
        return;
    }

    if (resetID)
        id->setValue(0);

    vsd.exec();
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
