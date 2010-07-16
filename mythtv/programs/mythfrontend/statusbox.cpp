
#include "statusbox.h"

using namespace std;

#include <QRegExp>
#include <QHostAddress>

#include "mythcorecontext.h"

#include "mythdb.h"
#include "mythverbose.h"
#include "mythversion.h"
#include "util.h"

#include "config.h"
#include "remoteutil.h"
#include "tv.h"
#include "jobqueue.h"
#include "cardutil.h"
#include "recordinginfo.h"

#include "mythuihelper.h"
#include "mythuibuttonlist.h"
#include "mythuitext.h"
#include "mythuistatetype.h"
#include "mythdialogbox.h"

struct LogLine {
    QString line;
    QString detail;
    QString data;
    QString state;
};

Q_DECLARE_METATYPE(LogLine)

/** \class StatusBox
 *  \brief Reports on various status items.
 *
 *  StatusBox reports on the listing status, that is how far
 *  into the future program listings exists. It also reports
 *  on the status of each tuner, the log entries, the status
 *  of the job queue, and the machine status.
 */

StatusBox::StatusBox(MythScreenStack *parent)
          : MythScreenType(parent, "StatusBox")
{
    m_dateFormat = gCoreContext->GetSetting("ShortDateFormat", "M/d");
    m_timeFormat = gCoreContext->GetSetting("TimeFormat", "h:mm AP");
    m_timeDateFormat = QString("%1 %2").arg(m_timeFormat).arg(m_dateFormat);

    m_minLevel = gCoreContext->GetNumSetting("LogDefaultView",5);

    m_iconState = NULL;
    m_categoryList = m_logList = NULL;
    m_helpText = NULL;

    QStringList strlist;
    strlist << "QUERY_IS_ACTIVE_BACKEND";
    strlist << gCoreContext->GetHostName();

    gCoreContext->SendReceiveStringList(strlist);

    if (QString(strlist[0]) == "TRUE")
        m_isBackendActive = true;
    else
        m_isBackendActive = false;

    m_popupStack = GetMythMainWindow()->GetStack("popup stack");
}

StatusBox::~StatusBox(void)
{
    if (m_logList)
        gCoreContext->SaveSetting("StatusBoxItemCurrent",
                                                    m_logList->GetCurrentPos());
}

bool StatusBox::Create()
{
    if (!LoadWindowFromXML("status-ui.xml", "status", this))
        return false;

    m_categoryList = dynamic_cast<MythUIButtonList *>(GetChild("category"));
    m_logList = dynamic_cast<MythUIButtonList *>(GetChild("log"));

    m_iconState = dynamic_cast<MythUIStateType *>(GetChild("icon"));
    m_helpText = dynamic_cast<MythUIText *>(GetChild("helptext"));

    if (!m_categoryList || !m_logList || !m_helpText)
    {
        VERBOSE(VB_IMPORTANT, "StatusBox, theme is missing "
                              "required elements");
        return false;
    }

    connect(m_categoryList, SIGNAL(itemSelected(MythUIButtonListItem *)),
                SLOT(updateLogList(MythUIButtonListItem *)));
    connect(m_logList, SIGNAL(itemSelected(MythUIButtonListItem *)),
                SLOT(setHelpText(MythUIButtonListItem *)));
    connect(m_logList, SIGNAL(itemClicked(MythUIButtonListItem *)),
                SLOT(clicked(MythUIButtonListItem *)));

    BuildFocusList();
    return true;
}

void StatusBox::Init()
{
    MythUIButtonListItem *item;

    item = new MythUIButtonListItem(m_categoryList, tr("Listings Status"),
                            qVariantFromValue((void*)SLOT(doListingsStatus())));
    item->DisplayState("listings", "icon");

    item = new MythUIButtonListItem(m_categoryList, tr("Schedule Status"),
                            qVariantFromValue((void*)SLOT(doScheduleStatus())));
    item->DisplayState("schedule", "icon");

    item = new MythUIButtonListItem(m_categoryList, tr("Tuner Status"),
                            qVariantFromValue((void*)SLOT(doTunerStatus())));
    item->DisplayState("tuner", "icon");

    item = new MythUIButtonListItem(m_categoryList, tr("Log Entries"),
                            qVariantFromValue((void*)SLOT(doLogEntries())));
    item->DisplayState("log", "icon");

    item = new MythUIButtonListItem(m_categoryList, tr("Job Queue"),
                            qVariantFromValue((void*)SLOT(doJobQueueStatus())));
    item->DisplayState("jobqueue", "icon");

    item = new MythUIButtonListItem(m_categoryList, tr("Machine Status"),
                            qVariantFromValue((void*)SLOT(doMachineStatus())));
    item->DisplayState("machine", "icon");

    item = new MythUIButtonListItem(m_categoryList, tr("AutoExpire List"),
                            qVariantFromValue((void*)SLOT(doAutoExpireList())));
    item->DisplayState("autoexpire", "icon");

    int itemCurrent = gCoreContext->GetNumSetting("StatusBoxItemCurrent", 0);
    m_categoryList->SetItemCurrent(itemCurrent);
}

MythUIButtonListItem* StatusBox::AddLogLine(QString line, QString detail,
                                            QString state, QString data)
{

    LogLine logline;
    logline.line = line;
    logline.detail = detail;
    logline.state = state;
    logline.data = data;

    MythUIButtonListItem *item = new MythUIButtonListItem(m_logList, line,
                                                qVariantFromValue(logline));
    if (!state.isEmpty())
    {
        item->SetFontState(state);
        item->DisplayState(state, "status");
    }

    return item;
}

bool StatusBox::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("Status", event, actions);

    for (int i = 0; i < actions.size() && !handled; ++i)
    {
        QString action = actions[i];
        handled = true;

        QRegExp logNumberKeys( "^[12345678]$" );

        MythUIButtonListItem* currentButton = m_categoryList->GetItemCurrent();
        QString currentItem;
        if (currentButton)
            currentItem = currentButton->GetText();

        handled = true;

        if (action == "MENU")
        {
            if (currentItem == tr("Log Entries"))
            {
                QString message = tr("Acknowledge all log entries at "
                                     "this priority level or lower?");

                MythConfirmationDialog *confirmPopup =
                        new MythConfirmationDialog(m_popupStack, message);

                confirmPopup->SetReturnEvent(this, "LogAckAll");

                if (confirmPopup->Create())
                    m_popupStack->AddScreen(confirmPopup, false);
            }
        }
        else if ((currentItem == tr("Log Entries")) &&
                 (logNumberKeys.indexIn(action) == 0))
        {
            m_minLevel = action.toInt();
            m_helpText->SetText(tr("Setting priority level to %1")
                                          .arg(m_minLevel));
            doLogEntries();
        }
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void StatusBox::setHelpText(MythUIButtonListItem *item)
{
    if (!item || GetFocusWidget() != m_logList)
        return;

    LogLine logline = qVariantValue<LogLine>(item->GetData());
    m_helpText->SetText(logline.detail);
}

void StatusBox::updateLogList(MythUIButtonListItem *item)
{
    if (!item)
        return;

    disconnect(this, SIGNAL(updateLog()),0,0);

    const char *slot = (const char *)qVariantValue<void*>(item->GetData());

    connect(this, SIGNAL(updateLog()), slot);
    emit updateLog();
}

void StatusBox::clicked(MythUIButtonListItem *item)
{
    if (!item)
        return;

    LogLine logline = qVariantValue<LogLine>(item->GetData());

    MythUIButtonListItem *currentButton = m_categoryList->GetItemCurrent();
    QString currentItem;
    if (currentButton)
        currentItem = currentButton->GetText();

    // FIXME: Comparisons against strings here is not great, changing names
    //        breaks everything and it's inefficient
    if (currentItem == tr("Log Entries"))
    {
        QString message = tr("Acknowledge this log entry?");

        MythConfirmationDialog *confirmPopup =
                new MythConfirmationDialog(m_popupStack, message);

        confirmPopup->SetReturnEvent(this, "LogAck");
        confirmPopup->SetData(logline.data);

        if (confirmPopup->Create())
            m_popupStack->AddScreen(confirmPopup, false);
    }
    else if (currentItem == tr("Job Queue"))
    {
        QStringList msgs;
        int jobStatus;

        jobStatus = JobQueue::GetJobStatus(logline.data.toInt());

        if (jobStatus == JOB_QUEUED)
        {
            QString message = tr("Delete Job?");

            MythConfirmationDialog *confirmPopup =
                    new MythConfirmationDialog(m_popupStack, message);

            confirmPopup->SetReturnEvent(this, "JobDelete");
            confirmPopup->SetData(logline.data);

            if (confirmPopup->Create())
                m_popupStack->AddScreen(confirmPopup, false);
        }
        else if ((jobStatus == JOB_PENDING) ||
                (jobStatus == JOB_STARTING) ||
                (jobStatus == JOB_RUNNING)  ||
                (jobStatus == JOB_PAUSED))
        {
            QString label = tr("Job Queue Actions:");

            MythDialogBox *menuPopup = new MythDialogBox(label, m_popupStack,
                                                            "statusboxpopup");

            if (menuPopup->Create())
                m_popupStack->AddScreen(menuPopup, false);

            menuPopup->SetReturnEvent(this, "JobModify");

            QVariant data = qVariantFromValue(logline.data);

            if (jobStatus == JOB_PAUSED)
                menuPopup->AddButton(tr("Resume"), data);
            else
                menuPopup->AddButton(tr("Pause"), data);
            menuPopup->AddButton(tr("Stop"), data);
            menuPopup->AddButton(tr("No Change"), data);
        }
        else if (jobStatus & JOB_DONE)
        {
            QString message = tr("Requeue Job?");

            MythConfirmationDialog *confirmPopup =
                    new MythConfirmationDialog(m_popupStack, message);

            confirmPopup->SetReturnEvent(this, "JobRequeue");
            confirmPopup->SetData(logline.data);

            if (confirmPopup->Create())
                m_popupStack->AddScreen(confirmPopup, false);
        }
    }
    else if (currentItem == tr("AutoExpire List"))
    {
        ProgramInfo* rec = m_expList[m_logList->GetCurrentPos()];

        if (rec)
        {
            QString label = tr("AutoExpire Actions:");

            MythDialogBox *menuPopup = new MythDialogBox(label, m_popupStack,
                                                            "statusboxpopup");

            if (menuPopup->Create())
                m_popupStack->AddScreen(menuPopup, false);

            menuPopup->SetReturnEvent(this, "AutoExpireManage");

            menuPopup->AddButton(tr("Delete Now"), qVariantFromValue(rec));
            if ((rec)->GetRecordingGroup() == "LiveTV")
            {
                menuPopup->AddButton(tr("Move to Default group"),
                                                       qVariantFromValue(rec));
            }
            else if ((rec)->GetRecordingGroup() == "Deleted")
                menuPopup->AddButton(tr("Undelete"), qVariantFromValue(rec));
            else
                menuPopup->AddButton(tr("Disable AutoExpire"),
                                                        qVariantFromValue(rec));
            menuPopup->AddButton(tr("No Change"), qVariantFromValue(rec));

        }
    }
}

void StatusBox::customEvent(QEvent *event)
{
    if (event->type() == DialogCompletionEvent::kEventType)
    {
        DialogCompletionEvent *dce = (DialogCompletionEvent*)(event);

        QString resultid  = dce->GetId();
        int     buttonnum = dce->GetResult();

        if (resultid == "LogAck")
        {
            if (buttonnum == 1)
            {
                QString sql = dce->GetData().toString();
                MSqlQuery query(MSqlQuery::InitCon());
                query.prepare("UPDATE mythlog SET acknowledged = 1 "
                            "WHERE logid = :LOGID ;");
                query.bindValue(":LOGID", sql);
                if (!query.exec())
                    MythDB::DBError("StatusBox::customEvent -- LogAck", query);
                doLogEntries();
            }
        }
        else if (resultid == "LogAckAll")
        {
            if (buttonnum == 1)
            {
                MSqlQuery query(MSqlQuery::InitCon());
                query.prepare("UPDATE mythlog SET acknowledged = 1 "
                                "WHERE priority <= :PRIORITY ;");
                query.bindValue(":PRIORITY", m_minLevel);
                if (!query.exec())
                    MythDB::DBError("StatusBox::customEvent -- LogAckAll",
                                    query);
                doLogEntries();
            }
        }
        else if (resultid == "JobDelete")
        {
            if (buttonnum == 1)
            {
                int jobID = dce->GetData().toInt();
                JobQueue::DeleteJob(jobID);
                doJobQueueStatus();
            }
        }
        else if (resultid == "JobRequeue")
        {
            if (buttonnum == 1)
            {
                int jobID = dce->GetData().toInt();
                JobQueue::ChangeJobStatus(jobID, JOB_QUEUED);
                doJobQueueStatus();
            }
        }
        else if (resultid == "JobModify")
        {
            int jobID = dce->GetData().toInt();
            if (buttonnum == 0)
            {
                if (JobQueue::GetJobStatus(jobID) == JOB_PAUSED)
                    JobQueue::ResumeJob(jobID);
                else
                    JobQueue::PauseJob(jobID);
            }
            else if (buttonnum == 1)
            {
                JobQueue::StopJob(jobID);
            }

            doJobQueueStatus();
        }
        else if (resultid == "AutoExpireManage")
        {
            ProgramInfo* rec = qVariantValue<ProgramInfo*>(dce->GetData());
            if (!rec)
                return;

            if ((buttonnum == 0) && rec->QueryIsDeleteCandidate())
            {
                RemoteDeleteRecording(
                    rec->GetChanID(), rec->GetRecordingStartTime(), false);
            }
            else if (buttonnum == 1)
            {
                if ((rec)->GetRecordingGroup() == "Deleted")
                {
                    RemoteUndeleteRecording(
                        rec->GetChanID(), rec->GetRecordingStartTime());
                }
                else
                {
                    rec->SaveAutoExpire(kDisableAutoExpire);

                    if ((rec)->GetRecordingGroup() == "LiveTV")
                    {
                        RecordingInfo ri(*rec);
                        ri.ApplyRecordRecGroupChange("Default");
                        *rec = ri;
                    }
                }
            }
            doAutoExpireList();
        }

    }
}

void StatusBox::doListingsStatus()
{
    if (m_iconState)
        m_iconState->DisplayState("listings");
    m_logList->Reset();
    m_helpText->SetText(tr("Listings Status shows the latest "
                                    "status information from "
                                    "mythfilldatabase"));

    QString mfdLastRunStart, mfdLastRunEnd, mfdLastRunStatus, mfdNextRunStart;
    QString querytext, DataDirectMessage;
    int DaysOfData;
    QDateTime qdtNow, GuideDataThrough;

    qdtNow = QDateTime::currentDateTime();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT max(endtime) FROM program WHERE manualid=0;");

    if (query.exec() && query.next())
        GuideDataThrough = QDateTime::fromString(query.value(0).toString(),
                                                 Qt::ISODate);

    mfdLastRunStart = gCoreContext->GetSetting("mythfilldatabaseLastRunStart");
    mfdLastRunEnd = gCoreContext->GetSetting("mythfilldatabaseLastRunEnd");
    mfdLastRunStatus = gCoreContext->GetSetting("mythfilldatabaseLastRunStatus");
    mfdNextRunStart = gCoreContext->GetSetting("MythFillSuggestedRunTime");
    DataDirectMessage = gCoreContext->GetSetting("DataDirectMessage");

    mfdNextRunStart.replace('T', ' ');

    extern const char *myth_source_version;
    extern const char *myth_source_path;
    AddLogLine(tr("Mythfrontend version: %1 (%2)").arg(myth_source_path)
                                                  .arg(myth_source_version));
    AddLogLine(tr("Last mythfilldatabase guide update:"));
    AddLogLine(tr("Started:   %1").arg(mfdLastRunStart));

    if (mfdLastRunEnd >= mfdLastRunStart)
        AddLogLine(tr("Finished: %1").arg(mfdLastRunEnd));

    AddLogLine(tr("Result: %1").arg(mfdLastRunStatus));


    if (mfdNextRunStart >= mfdLastRunStart)
        AddLogLine(tr("Suggested Next: %1").arg(mfdNextRunStart));

    DaysOfData = qdtNow.daysTo(GuideDataThrough);

    if (GuideDataThrough.isNull())
    {
        AddLogLine(tr("There's no guide data available!"), "", "warning");
        AddLogLine(tr("Have you run mythfilldatabase?"), "", "warning");
    }
    else
    {
        AddLogLine(tr("There is guide data until %1")
                        .arg(QDateTime(GuideDataThrough)
                            .toString("yyyy-MM-dd hh:mm")));

        AddLogLine(QString("(%1).").arg(tr("%n day(s)", "", DaysOfData)));
    }

    if (DaysOfData <= 3)
        AddLogLine(tr("WARNING: is mythfilldatabase running?"), "", "warning");

    if (!DataDirectMessage.isEmpty())
    {
        AddLogLine(tr("DataDirect Status: "));
        AddLogLine(DataDirectMessage);
    }

}

void StatusBox::doScheduleStatus()
{
    if (m_iconState)
        m_iconState->DisplayState("schedule");
    m_logList->Reset();
    m_helpText->SetText(tr("Schedule Status shows current statistics from the "
                           "scheduler."));

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT COUNT(*) FROM record WHERE search = 0");
    if (query.exec() && query.next())
    {
        QString rules = tr("%n standard rule(s) (is) defined", "",
                            query.value(0).toInt());
        AddLogLine(rules, rules);
    }
    else
    {
        MythDB::DBError("StatusBox::doScheduleStatus()", query);
        return;
    }

    query.prepare("SELECT COUNT(*) FROM record WHERE search > 0");
    if (query.exec() && query.next())
    {
        QString rules = tr("%n search rule(s) are defined", "",
                            query.value(0).toInt());
        AddLogLine(rules, rules);
    }
    else
    {
        MythDB::DBError("StatusBox::doScheduleStatus()", query);
        return;
    }

    QMap<RecStatusType, int> statusMatch;
    QMap<RecStatusType, QString> statusText;
    QMap<int, int> sourceMatch;
    QMap<int, QString> sourceText;
    QMap<int, int> inputMatch;
    QMap<int, QString> inputText;
    QString tmpstr;
    int maxSource = 0;
    int maxInput = 0;
    int lowerpriority = 0;
    int hdflag = 0;

    query.prepare("SELECT MAX(sourceid) FROM videosource");
    if (query.exec())
    {
        if (query.next())
            maxSource = query.value(0).toInt();
    }

    query.prepare("SELECT sourceid,name FROM videosource");
    if (query.exec())
    {
        while (query.next())
            sourceText[query.value(0).toInt()] = query.value(1).toString();
    }

    query.prepare("SELECT MAX(cardinputid) FROM cardinput");
    if (query.exec())
    {
        if (query.next())
            maxInput = query.value(0).toInt();
    }

    query.prepare("SELECT cardinputid,cardid,inputname,displayname "
                  "FROM cardinput");
    if (query.exec())
    {
        while (query.next())
        {
            if (!query.value(3).toString().isEmpty())
                inputText[query.value(0).toInt()] = query.value(3).toString();
            else
                inputText[query.value(0).toInt()] = QString("%1: %2")
                                              .arg(query.value(1).toInt())
                                              .arg(query.value(2).toString());
        }
    }

    ProgramList schedList;
    LoadFromScheduler(schedList);

    tmpstr = tr("%n matching showing(s)", "", schedList.size());
    AddLogLine(tmpstr, tmpstr);

    ProgramList::const_iterator it = schedList.begin();
    for (; it != schedList.end(); ++it)
    {
        const ProgramInfo *s = *it;
        const RecStatusType recstatus = s->GetRecordingStatus();

        if (statusMatch[recstatus] < 1)
        {
            statusText[recstatus] = toString(
                recstatus, s->GetRecordingRuleType());
        }

        ++statusMatch[recstatus];

        if (recstatus == rsWillRecord || recstatus == rsRecording)
        {
            ++sourceMatch[s->GetSourceID()];
            ++inputMatch[s->GetInputID()];
            if (s->GetRecordingPriority2() < 0)
                ++lowerpriority;
            if (s->GetVideoProperties() & VID_HDTV)
                ++hdflag;
        }
    }

#define ADD_STATUS_LOG_LINE(rtype, fstate)                      \
    do {                                                        \
        if (statusMatch[rtype] > 0)                             \
        {                                                       \
            tmpstr = QString("%1 %2").arg(statusMatch[rtype])   \
                                     .arg(statusText[rtype]);   \
            AddLogLine(tmpstr, tmpstr, fstate);                 \
        }                                                       \
    } while (0)
    ADD_STATUS_LOG_LINE(rsRecording, "");
    ADD_STATUS_LOG_LINE(rsWillRecord, "");
    ADD_STATUS_LOG_LINE(rsConflict, "error");
    ADD_STATUS_LOG_LINE(rsTooManyRecordings, "warning");
    ADD_STATUS_LOG_LINE(rsLowDiskSpace, "warning");
    ADD_STATUS_LOG_LINE(rsLaterShowing, "warning");
    ADD_STATUS_LOG_LINE(rsNotListed, "warning");

    QString willrec = statusText[rsWillRecord];

    if (lowerpriority > 0)
    {
        tmpstr = QString("%1 %2 %3").arg(lowerpriority).arg(willrec)
                                    .arg(tr("with lower priority"));
        AddLogLine(tmpstr, tmpstr, "warning");
    }
    if (hdflag > 0)
    {
        tmpstr = QString("%1 %2 %3").arg(hdflag).arg(willrec)
                                    .arg(tr("marked as HDTV"));
        AddLogLine(tmpstr, tmpstr);
    }
    int i;
    for (i = 1; i <= maxSource; ++i)
    {
        if (sourceMatch[i] > 0)
        {
            tmpstr = QString("%1 %2 %3 %4 \"%5\"")
                             .arg(sourceMatch[i]).arg(willrec)
                             .arg(tr("from source")).arg(i).arg(sourceText[i]);
            AddLogLine(tmpstr, tmpstr);
        }
    }
    for (i = 1; i <= maxInput; ++i)
    {
        if (inputMatch[i] > 0)
        {
            tmpstr = QString("%1 %2 %3 %4 \"%5\"")
                             .arg(inputMatch[i]).arg(willrec)
                             .arg(tr("on input")).arg(i).arg(inputText[i]);
            AddLogLine(tmpstr, tmpstr);
        }
    }
}

void StatusBox::doTunerStatus()
{
    if (m_iconState)
        m_iconState->DisplayState("tuner");
    m_logList->Reset();
    m_helpText->SetText(tr("Tuner Status shows the current information about "
                           "the state of backend tuner cards"));

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT cardid, cardtype, videodevice "
        "FROM capturecard ORDER BY cardid");

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("StatusBox::doTunerStatus()", query);
        return;
    }

    while (query.next())
    {
        int cardid = query.value(0).toInt();

        QString cmd = QString("QUERY_REMOTEENCODER %1").arg(cardid);
        QStringList strlist( cmd );
        strlist << "GET_STATE";

        gCoreContext->SendReceiveStringList(strlist);
        int state = strlist[0].toInt();

        QString status;
        QString fontstate;
        if (state == kState_Error)
        {
            strlist.clear();
            strlist << QString("QUERY_REMOTEENCODER %1").arg(cardid);
            strlist << "GET_SLEEPSTATUS";

            gCoreContext->SendReceiveStringList(strlist);
            state = strlist[0].toInt();

            if (state == -1)
                status = tr("has an error");
            else if (state == sStatus_Undefined)
                status = tr("is unavailable");
            else
                status = tr("is asleep");

            fontstate = "warning";
        }
        else if (state == kState_WatchingLiveTV)
            status = tr("is watching Live TV");
        else if (state == kState_RecordingOnly ||
                 state == kState_WatchingRecording)
            status = tr("is recording");
        else
            status = tr("is not recording");

        QString tun = tr("Tuner %1 %2 %3");
        QString devlabel = CardUtil::GetDeviceLabel(
            query.value(1).toString(), query.value(2).toString());

        QString shorttuner = tun.arg(cardid).arg("").arg(status);
        QString longtuner = tun.arg(cardid).arg(devlabel).arg(status);

        if (state == kState_RecordingOnly ||
            state == kState_WatchingRecording)
        {
            strlist = QStringList( QString("QUERY_RECORDER %1").arg(cardid));
            strlist << "GET_RECORDING";
            gCoreContext->SendReceiveStringList(strlist);
            ProgramInfo pginfo(strlist);
            if (pginfo.GetChanID())
            {
                status += ' ' + pginfo.GetTitle();
                status += "\n";
                status += pginfo.GetSubtitle();
                longtuner = tun.arg(cardid).arg(devlabel).arg(status);
            }
        }

        AddLogLine(shorttuner, longtuner, fontstate);
    }
}

void StatusBox::doLogEntries(void)
{
    if (m_iconState)
        m_iconState->DisplayState("log");
    m_logList->Reset();
    m_helpText->SetText(tr("Log Entries shows any unread log entries from the "
                           "system if you have logging enabled"));

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT logid, module, priority, logdate, host, "
                  "message, details "
                  "FROM mythlog WHERE acknowledged = 0 "
                  "AND priority <= :PRIORITY ORDER BY logdate DESC;");
    query.bindValue(":PRIORITY", m_minLevel);

    if (query.exec())
    {
        QString line;
        QString detail;
        while (query.next())
        {
            line = QString("%1").arg(query.value(5).toString());

            if (!query.value(6).toString().isEmpty())
                detail = tr("On %1 %2 from %3.%4\n%5\n%6")
                               .arg(query.value(3).toDateTime()
                                         .toString(m_dateFormat))
                               .arg(query.value(3).toDateTime()
                                         .toString(m_timeFormat))
                               .arg(query.value(4).toString())
                               .arg(query.value(1).toString())
                               .arg(query.value(5).toString())
                               .arg(query.value(6).toString());
            else
                detail = tr("On %1 %2 from %3.%4\n%5\nNo other details")
                               .arg(query.value(3).toDateTime()
                                         .toString(m_dateFormat))
                               .arg(query.value(3).toDateTime()
                                         .toString(m_timeFormat))
                               .arg(query.value(4).toString())
                               .arg(query.value(1).toString())
                               .arg(query.value(5).toString());
            AddLogLine(line, detail, "", query.value(0).toString());
        }

        if (query.size() == 0)
        {
            AddLogLine(tr("No items found at priority level %1 or lower.")
                                                            .arg(m_minLevel));
            AddLogLine(tr("Use 1-8 to change priority level."));
        }
    }
}

void StatusBox::doJobQueueStatus()
{
    if (m_iconState)
        m_iconState->DisplayState("jobqueue");
    m_logList->Reset();
    m_helpText->SetText(tr("Job Queue shows any jobs currently in Myth's Job "
                           "Queue such as a commercial flagging job."));

    QMap<int, JobQueueEntry> jobs;
    QMap<int, JobQueueEntry>::Iterator it;

    JobQueue::GetJobsInQueue(jobs,
                             JOB_LIST_NOT_DONE | JOB_LIST_ERROR |
                             JOB_LIST_RECENT);

    if (jobs.size())
    {
        QString detail;
        QString line;

        for (it = jobs.begin(); it != jobs.end(); ++it)
        {
            ProgramInfo pginfo((*it).chanid, (*it).recstartts);

            if (!pginfo.GetChanID())
                continue;

            detail = QString("%1\n%2 %3 @ %4\n%5 %6     %7 %8")
                .arg(pginfo.GetTitle())
                .arg(pginfo.GetChannelName())
                .arg(pginfo.GetChanNum())
                .arg(pginfo.GetRecordingStartTime()
                     .toString(m_timeDateFormat))
                .arg(tr("Job:"))
                .arg(JobQueue::JobText((*it).type))
                .arg(tr("Status: "))
                .arg(JobQueue::StatusText((*it).status));

            if ((*it).status != JOB_QUEUED)
                detail += " (" + (*it).hostname + ')';

            if ((*it).schedruntime > QDateTime::currentDateTime())
                detail += '\n' + tr("Scheduled Run Time:") + ' ' +
                    (*it).schedruntime.toString(m_timeDateFormat);
            else
                detail += '\n' + (*it).comment;

            line = QString("%1 @ %2").arg(pginfo.GetTitle())
                .arg(pginfo.GetRecordingStartTime()
                     .toString(m_timeDateFormat));

            QString font;
            if ((*it).status == JOB_ERRORED)
                font = "error";
            else if ((*it).status == JOB_ABORTED)
                font = "warning";

            AddLogLine(line, detail, font, QString("%1").arg((*it).id));
        }
    }
    else
        AddLogLine(tr("Job Queue is currently empty."));

}

// Some helper routines for doMachineStatus() that format the output strings

/** \fn sm_str(long long, int)
 *  \brief Returns a short string describing an amount of space, choosing
 *         one of a number of useful units, "TB", "GB", "MB", or "KB".
 *  \param sizeKB Number of kilobytes.
 *  \param prec   Precision to use if we have less than ten of whatever
 *                unit is chosen.
 */
static const QString sm_str(long long sizeKB, int prec=1)
{
    if (sizeKB>1024*1024*1024) // Terabytes
    {
        double sizeGB = sizeKB/(1024*1024*1024.0);
        return QString("%1 TB").arg(sizeGB, 0, 'f', (sizeGB>10)?0:prec);
    }
    else if (sizeKB>1024*1024) // Gigabytes
    {
        double sizeGB = sizeKB/(1024*1024.0);
        return QString("%1 GB").arg(sizeGB, 0, 'f', (sizeGB>10)?0:prec);
    }
    else if (sizeKB>1024) // Megabytes
    {
        double sizeMB = sizeKB/1024.0;
        return QString("%1 MB").arg(sizeMB, 0, 'f', (sizeMB>10)?0:prec);
    }
    // Kilobytes
    return QString("%1 KB").arg(sizeKB);
}

static const QString usage_str_kb(long long total,
                                  long long used,
                                  long long free)
{
    QString ret = QObject::tr("Unknown");
    if (total > 0.0 && free > 0.0)
    {
        double percent = (100.0*free)/total;
        ret = QObject::tr("%1 total, %2 used, %3 (or %4%) free.")
            .arg(sm_str(total)).arg(sm_str(used))
            .arg(sm_str(free)).arg(percent, 0, 'f', (percent >= 10.0) ? 0 : 2);
    }
    return ret;
}

static const QString usage_str_mb(float total, float used, float free)
{
    return usage_str_kb((long long)(total*1024), (long long)(used*1024),
                        (long long)(free*1024));
}

static void disk_usage_with_rec_time_kb(QStringList& out, long long total,
                                        long long used, long long free,
                                        const recprof2bps_t& prof2bps)
{
    const QString tail = QObject::tr(", using your %1 rate of %2 kb/s");

    out<<usage_str_kb(total, used, free);
    if (free<0)
        return;

    recprof2bps_t::const_iterator it = prof2bps.begin();
    for (; it != prof2bps.end(); ++it)
    {
        const QString pro =
                tail.arg(it.key()).arg((int)((float)(*it) / 1024.0));

        long long bytesPerMin = ((*it) >> 1) * 15;
        uint minLeft = ((free<<5)/bytesPerMin)<<5;
        minLeft = (minLeft/15)*15;
        uint hoursLeft = minLeft/60;
        QString hourstring = QObject::tr("%n hour(s)", "", hoursLeft);
        QString minstring = QObject::tr("%n minute(s)", "", minLeft%60);
        QString remainstring = QObject::tr("%1 remaining", "time");
        if (minLeft%60 == 0)
            out<<remainstring.arg(hourstring) + pro;
        else if (minLeft > 60)
            out<<QObject::tr("%1 and %2 remaining", "time").arg(hourstring)
                                                   .arg(minstring) + pro;
        else
            out<<remainstring.arg(minstring) + pro;
    }
}

static const QString uptimeStr(time_t uptime)
{
    int     days, hours, min, secs;
    QString str;

    str = QString("   " + QObject::tr("Uptime") + ": ");

    if (uptime == 0)
        return str + "unknown";

    days = uptime/(60*60*24);
    uptime -= days*60*60*24;
    hours = uptime/(60*60);
    uptime -= hours*60*60;
    min  = uptime/60;
    secs = uptime%60;

    if (days > 0)
    {
        char    buff[6];
        QString dayLabel = QObject::tr("%n day(s)", "", days);

        sprintf(buff, "%d:%02d", hours, min);

        return str + QString("%1, %2").arg(dayLabel).arg(buff);
    }
    else
    {
        char  buff[9];

        sprintf(buff, "%d:%02d:%02d", hours, min, secs);

        return str + QString( buff );
    }
}

/** \fn StatusBox::getActualRecordedBPS(QString hostnames)
 *  \brief Fills in recordingProfilesBPS w/ average bitrate from recorded table
 */
void StatusBox::getActualRecordedBPS(QString hostnames)
{
    recordingProfilesBPS.clear();

    QString querystr;
    MSqlQuery query(MSqlQuery::InitCon());

    querystr =
        "SELECT sum(filesize) * 8 / "
            "sum(((unix_timestamp(endtime) - unix_timestamp(starttime)))) "
            "AS avg_bitrate "
        "FROM recorded WHERE hostname in (%1) "
            "AND (unix_timestamp(endtime) - unix_timestamp(starttime)) > 300;";

    query.prepare(querystr.arg(hostnames));

    if (query.exec() && query.next() &&
        query.value(0).toDouble() > 0)
    {
        recordingProfilesBPS[tr("average")] =
            (int)(query.value(0).toDouble());
    }

    querystr =
        "SELECT max(filesize * 8 / "
            "(unix_timestamp(endtime) - unix_timestamp(starttime))) "
            "AS max_bitrate "
        "FROM recorded WHERE hostname in (%1) "
            "AND (unix_timestamp(endtime) - unix_timestamp(starttime)) > 300;";

    query.prepare(querystr.arg(hostnames));

    if (query.exec() && query.next() &&
        query.value(0).toDouble() > 0)
    {
        recordingProfilesBPS[tr("maximum")] =
            (int)(query.value(0).toDouble());
    }
}

/** \fn StatusBox::doMachineStatus()
 *  \brief Show machine status.
 *
 *   This returns statisics for master backend when using
 *   a frontend only machine. And returns info on the current
 *   system if frontend is running on a backend machine.
 *  \bug We should report on all backends and the current frontend.
 */
void StatusBox::doMachineStatus()
{
    if (m_iconState)
        m_iconState->DisplayState("machine");
    m_logList->Reset();
    QString machineStr = tr("Machine Status shows some operating system "
                            "statistics of this machine");
    if (!m_isBackendActive)
        machineStr.append(" " + tr("and the MythTV server"));

    m_helpText->SetText(machineStr);

    int           totalM, usedM, freeM;    // Physical memory
    int           totalS, usedS, freeS;    // Virtual  memory (swap)
    time_t        uptime;

    QString line;
    if (m_isBackendActive)
        line = tr("System:");
    else
        line = tr("This machine:");
    AddLogLine(line, QString("%1\n").arg(line));

    // uptime
    if (!getUptime(uptime))
        uptime = 0;
    line = uptimeStr(uptime);

    // weighted average loads
    line.append(".   " + tr("Load") + ": ");

#ifdef _WIN32
    line.append(tr("unknown") + " - getloadavg() " + tr("failed"));
#else // if !_WIN32
    double loads[3];
    if (getloadavg(loads,3) == -1)
        line.append(tr("unknown") + " - getloadavg() " + tr("failed"));
    else
    {
        char buff[30];

        sprintf(buff, "%0.2lf, %0.2lf, %0.2lf", loads[0], loads[1], loads[2]);
        line.append(QString(buff));
    }
#endif // _WIN32

    AddLogLine(line, QString("%1\n").arg(line));

    // memory usage
    if (getMemStats(totalM, freeM, totalS, freeS))
    {
        usedM = totalM - freeM;
        if (totalM > 0)
        {
            line = "   " + tr("RAM") + ": " + usage_str_mb(totalM, usedM, freeM);
            AddLogLine(line, QString("%1\n").arg(line));
        }
        usedS = totalS - freeS;
        if (totalS > 0)
        {
            line = "   " + tr("Swap") +
                                  ": "  + usage_str_mb(totalS, usedS, freeS);
            AddLogLine(line, QString("%1\n").arg(line));
        }
    }

    if (!m_isBackendActive)
    {
        line = tr("MythTV server") + ':';
        AddLogLine(line, QString("%1\n").arg(line));

        // uptime
        if (!RemoteGetUptime(uptime))
            uptime = 0;
        line = uptimeStr(uptime);

        // weighted average loads
        line.append(".   " + tr("Load") + ": ");
        float loads[3];
        if (RemoteGetLoad(loads))
        {
            char buff[30];

            sprintf(buff, "%0.2f, %0.2f, %0.2f", loads[0], loads[1], loads[2]);
            line.append(QString(buff));
        }
        else
            line.append(tr("unknown"));

        AddLogLine(line, QString("%1\n").arg(line));

        // memory usage
        if (RemoteGetMemStats(totalM, freeM, totalS, freeS))
        {
            usedM = totalM - freeM;
            if (totalM > 0)
            {
                line = "   " + tr("RAM") +
                                     ": "  + usage_str_mb(totalM, usedM, freeM);
                AddLogLine(line, QString("%1\n").arg(line));
            }

            usedS = totalS - freeS;
            if (totalS > 0)
            {
                line = "   " + tr("Swap") +
                                     ": "  + usage_str_mb(totalS, usedS, freeS);
                AddLogLine(line, QString("%1\n").arg(line));
            }
        }
    }

    // get free disk space
    QString hostnames;

    vector<FileSystemInfo> fsInfos = RemoteGetFreeSpace();
    for (uint i=0; i<fsInfos.size(); ++i)
    {
        // For a single-directory installation just display the totals
        if ((fsInfos.size() == 2) && (i == 0) &&
            (fsInfos[i].directory != "TotalDiskSpace") &&
            (fsInfos[i+1].directory == "TotalDiskSpace"))
            i++;

        hostnames = QString("\"%1\"").arg(fsInfos[i].hostname);
        hostnames.replace(' ', "");
        hostnames.replace(',', "\",\"");

        getActualRecordedBPS(hostnames);

        QStringList list;
        disk_usage_with_rec_time_kb(list,
            fsInfos[i].totalSpaceKB, fsInfos[i].usedSpaceKB,
            fsInfos[i].totalSpaceKB - fsInfos[i].usedSpaceKB,
            recordingProfilesBPS);

        if (fsInfos[i].directory == "TotalDiskSpace")
        {
            line = tr("Total Disk Space:");
            AddLogLine(line, QString("%1\n").arg(line));
        }
        else
        {
            line = tr("MythTV Drive #%1:").arg(fsInfos[i].fsID);
            AddLogLine(line, QString("%1\n").arg(line));

            QStringList tokens = fsInfos[i].directory.split(',');

            if (tokens.size() > 1)
            {
                AddLogLine(QString("   ") + tr("Directories:"));

                int curToken = 0;
                while (curToken < tokens.size())
                    AddLogLine(QString("      ") + tokens[curToken++]);
            }
            else
            {
                AddLogLine(QString("   " ) + tr("Directory:") + ' ' +
                            fsInfos[i].directory);
            }
        }

        QStringList::iterator it = list.begin();
        for (;it != list.end(); ++it)
        {
            line = QString("   ") + (*it);
            AddLogLine(line, QString("%1\n").arg(line));
        }
    }

}

/** \fn StatusBox::doAutoExpireList()
 *  \brief Show list of recordings which may AutoExpire
 */
void StatusBox::doAutoExpireList()
{
    if (m_iconState)
        m_iconState->DisplayState("autoexpire");
    m_logList->Reset();
    m_helpText->SetText(tr("The AutoExpire List shows all recordings which "
                           "may be expired and the order of their expiration. "
                           "Recordings at the top of the list will be expired "
                           "first."));

    ProgramInfo*          pginfo;
    QString               contentLine;
    QString               detailInfo;
    QString               staticInfo;
    long long             totalSize(0);
    long long             liveTVSize(0);
    int                   liveTVCount(0);
    long long             deletedGroupSize(0);
    int                   deletedGroupCount(0);

    vector<ProgramInfo *>::iterator it;
    for (it = m_expList.begin(); it != m_expList.end(); ++it)
        delete *it;
    m_expList.clear();

    RemoteGetAllExpiringRecordings(m_expList);

    for (it = m_expList.begin(); it != m_expList.end(); ++it)
    {
        pginfo = *it;

        totalSize += pginfo->GetFilesize();
        if (pginfo->GetRecordingGroup() == "LiveTV")
        {
            liveTVSize += pginfo->GetFilesize();
            liveTVCount++;
        }
        else if (pginfo->GetRecordingGroup() == "Deleted")
        {
            deletedGroupSize += pginfo->GetFilesize();
            deletedGroupCount++;
        }
    }

    staticInfo = tr("%n recording(s) consuming %1 (is) allowed to expire\n", "",
                     m_expList.size()).arg(sm_str(totalSize / 1024));

    if (liveTVCount)
        staticInfo += tr("%n (is) LiveTV and consume(s) %1\n", "", liveTVCount)
                            .arg(sm_str(liveTVSize / 1024));

    if (deletedGroupCount)
        staticInfo += tr("%n (is) Deleted and consume(s) %1\n", "",
                        deletedGroupCount)
                        .arg(sm_str(deletedGroupSize / 1024));

    for (it = m_expList.begin(); it != m_expList.end(); ++it)
    {
        pginfo = *it;
        contentLine = pginfo->GetRecordingStartTime()
            .toString(m_dateFormat) + " - ";

        contentLine += "(" + ProgramInfo::i18n(pginfo->GetRecordingGroup()) + ") ";

        contentLine += pginfo->GetTitle() +
            " (" + sm_str(pginfo->GetFilesize() / 1024) + ")";

        detailInfo = staticInfo;
        detailInfo +=
            pginfo->GetRecordingStartTime().toString(m_timeDateFormat) + " - " +
            pginfo->GetRecordingEndTime().toString(m_timeDateFormat);

        detailInfo += " (" + sm_str(pginfo->GetFilesize() / 1024) + ")";

        detailInfo += " (" + ProgramInfo::i18n(pginfo->GetRecordingGroup()) + ")";

        detailInfo += "\n" + pginfo->toString(ProgramInfo::kTitleSubtitle, " - ");

        AddLogLine(contentLine, detailInfo);
    }

}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
