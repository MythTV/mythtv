
#include "statusbox.h"

using namespace std;

#include <QRegExp>
#include <QHostAddress>
#include <QNetworkInterface>

#include "mythcorecontext.h"
#include "filesysteminfo.h"
#include "mythmiscutil.h"
#include "mythdb.h"
#include "mythlogging.h"
#include "mythversion.h"
#include "mythdate.h"

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
    QString m_line;
    QString m_detail;
    QString m_help;
    QString m_helpdetail;
    QString m_data;
    QString m_state;
};


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
    m_minLevel = gCoreContext->GetNumSetting("LogDefaultView",5);

    QStringList strlist;
    strlist << "QUERY_IS_ACTIVE_BACKEND";
    strlist << gCoreContext->GetHostName();

    gCoreContext->SendReceiveStringList(strlist);

    m_isBackendActive = (strlist[0] == "TRUE");
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
    m_justHelpText = dynamic_cast<MythUIText *>(GetChild("justhelptext"));

    if (!m_categoryList || !m_logList || (!m_helpText && !m_justHelpText))
    {
        LOG(VB_GENERAL, LOG_ERR, "StatusBox, theme is missing "
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
    auto *item = new MythUIButtonListItem(m_categoryList, tr("Listings Status"),
                            qVariantFromValue((void*)SLOT(doListingsStatus())));
    item->DisplayState("listings", "icon");

    item = new MythUIButtonListItem(m_categoryList, tr("Schedule Status"),
                            qVariantFromValue((void*)SLOT(doScheduleStatus())));
    item->DisplayState("schedule", "icon");

    item = new MythUIButtonListItem(m_categoryList, tr("Input Status"),
                            qVariantFromValue((void*)SLOT(doTunerStatus())));
    item->DisplayState("tuner", "icon");

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

void StatusBox::AddLogLine(const QString & line,
                                            const QString & help,
                                            const QString & detail,
                                            const QString & helpdetail,
                                            const QString & state,
                                            const QString & data)
{
    LogLine logline;
    logline.m_line = line;

    if (detail.isEmpty())
        logline.m_detail = line;
    else
        logline.m_detail = detail;

    if (help.isEmpty())
        logline.m_help = logline.m_detail;
    else
        logline.m_help = help;

    if (helpdetail.isEmpty())
        logline.m_helpdetail = logline.m_detail;
    else
        logline.m_helpdetail = helpdetail;

    logline.m_state = state;
    logline.m_data = data;

    auto *item = new MythUIButtonListItem(m_logList, line,
                                          qVariantFromValue(logline));
    if (logline.m_state.isEmpty())
        logline.m_state = "normal";

    item->SetFontState(logline.m_state);
    item->DisplayState(logline.m_state, "status");
    item->SetText(logline.m_detail, "detail");
}

bool StatusBox::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("Status", event, actions);

    for (int i = 0; i < actions.size() && !handled; ++i)
    {
        QString action = actions[i];
        handled = true;

        QRegExp logNumberKeys( "^[12345678]$" );

        MythUIButtonListItem* currentButton = m_categoryList->GetItemCurrent();
        QString currentItem;
        if (currentButton)
            currentItem = currentButton->GetText();

        if (action == "MENU")
        {
            if (currentItem == tr("Log Entries"))
            {
                QString message = tr("Acknowledge all log entries at "
                                     "this priority level or lower?");

                auto *confirmPopup =
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
            if (m_helpText)
                m_helpText->SetText(tr("Setting priority level to %1")
                                    .arg(m_minLevel));
            if (m_justHelpText)
                m_justHelpText->SetText(tr("Setting priority level to %1")
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

    LogLine logline = item->GetData().value<LogLine>();
    if (m_helpText)
        m_helpText->SetText(logline.m_helpdetail);
    if (m_justHelpText)
        m_justHelpText->SetText(logline.m_help);
}

void StatusBox::updateLogList(MythUIButtonListItem *item)
{
    if (!item)
        return;

    disconnect(this, SIGNAL(updateLog()),nullptr,nullptr);

    const char *slot = (const char *)item->GetData().value<void*>();

    connect(this, SIGNAL(updateLog()), slot);
    emit updateLog();
}

void StatusBox::clicked(MythUIButtonListItem *item)
{
    if (!item)
        return;

    LogLine logline = item->GetData().value<LogLine>();

    MythUIButtonListItem *currentButton = m_categoryList->GetItemCurrent();
    QString currentItem;
    if (currentButton)
        currentItem = currentButton->GetText();

    // FIXME: Comparisons against strings here is not great, changing names
    //        breaks everything and it's inefficient
    if (currentItem == tr("Log Entries"))
    {
        QString message = tr("Acknowledge this log entry?");

        auto *confirmPopup = new MythConfirmationDialog(m_popupStack, message);

        confirmPopup->SetReturnEvent(this, "LogAck");
        confirmPopup->SetData(logline.m_data);

        if (confirmPopup->Create())
            m_popupStack->AddScreen(confirmPopup, false);
    }
    else if (currentItem == tr("Job Queue"))
    {
        QStringList msgs;
        int jobStatus = JobQueue::GetJobStatus(logline.m_data.toInt());

        if (jobStatus == JOB_QUEUED)
        {
            QString message = tr("Delete Job?");

            auto *confirmPopup =
                    new MythConfirmationDialog(m_popupStack, message);

            confirmPopup->SetReturnEvent(this, "JobDelete");
            confirmPopup->SetData(logline.m_data);

            if (confirmPopup->Create())
                m_popupStack->AddScreen(confirmPopup, false);
        }
        else if ((jobStatus == JOB_PENDING) ||
                (jobStatus == JOB_STARTING) ||
                (jobStatus == JOB_RUNNING)  ||
                (jobStatus == JOB_PAUSED))
        {
            QString label = tr("Job Queue Actions:");

            auto *menuPopup = new MythDialogBox(label, m_popupStack,
                                                "statusboxpopup");

            if (menuPopup->Create())
                m_popupStack->AddScreen(menuPopup, false);

            menuPopup->SetReturnEvent(this, "JobModify");

            QVariant data = qVariantFromValue(logline.m_data);

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

            auto *confirmPopup =
                    new MythConfirmationDialog(m_popupStack, message);

            confirmPopup->SetReturnEvent(this, "JobRequeue");
            confirmPopup->SetData(logline.m_data);

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

            auto *menuPopup = new MythDialogBox(label, m_popupStack,
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
        auto *dce = (DialogCompletionEvent*)(event);

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
                m_logList->RemoveItem(m_logList->GetItemCurrent());
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

                m_logList->RemoveItem(m_logList->GetItemCurrent());
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
            auto* rec = dce->GetData().value<ProgramInfo*>();

            // button 2 is "No Change"
            if (!rec || buttonnum == 2)
                return;

            // button 1 is "Delete Now"
            if ((buttonnum == 0) && rec->QueryIsDeleteCandidate())
            {
                if (!RemoteDeleteRecording(rec->GetRecordingID(),
                                           false, false))
                {
                    LOG(VB_GENERAL, LOG_ERR, QString("Failed to delete recording: %1").arg(rec->GetTitle()));
                    return;
                }
            }
            // button 1 is "Move To Default Group" or "UnDelete" or "Disable AutoExpire"
            else if (buttonnum == 1)
            {
                if ((rec)->GetRecordingGroup() == "Deleted")
                {
                    RemoteUndeleteRecording(rec->GetRecordingID());
                }
                else
                {
                    rec->SaveAutoExpire(kDisableAutoExpire);

                    if ((rec)->GetRecordingGroup() == "LiveTV")
                    {
                        RecordingInfo ri(*rec);
                        ri.ApplyRecordRecGroupChange(RecordingInfo::kDefaultRecGroup);
                        *rec = ri;
                    }
                }
            }

            // remove the changed recording from the expire list
            delete m_expList[m_logList->GetCurrentPos()];
            m_expList.erase(m_expList.begin() + m_logList->GetCurrentPos());

            int pos = m_logList->GetCurrentPos();
            int topPos = m_logList->GetTopItemPos();
            doAutoExpireList(false);
            m_logList->SetItemCurrent(pos, topPos);
        }

    }
}

void StatusBox::doListingsStatus()
{
    if (m_iconState)
        m_iconState->DisplayState("listings");
    m_logList->Reset();

    QString helpmsg(tr("Listings Status shows the latest "
                       "status information from "
                       "mythfilldatabase"));
    if (m_helpText)
        m_helpText->SetText(helpmsg);
    if (m_justHelpText)
        m_justHelpText->SetText(helpmsg);

    QDateTime mfdLastRunStart;
    QDateTime mfdLastRunEnd;
    QDateTime mfdNextRunStart;
    QString mfdLastRunStatus;
    QString querytext;
    QDateTime qdtNow;
    QDateTime GuideDataThrough;

    qdtNow = MythDate::current();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT max(endtime) FROM program WHERE manualid=0;");

    if (query.exec() && query.next())
        GuideDataThrough = MythDate::fromString(query.value(0).toString());

    QString tmp = gCoreContext->GetSetting("mythfilldatabaseLastRunStart");
    mfdLastRunStart = MythDate::fromString(tmp);
    tmp = gCoreContext->GetSetting("mythfilldatabaseLastRunEnd");
    mfdLastRunEnd = MythDate::fromString(tmp);
    tmp = gCoreContext->GetSetting("MythFillSuggestedRunTime");
    mfdNextRunStart = MythDate::fromString(tmp);

    mfdLastRunStatus = gCoreContext->GetSetting("mythfilldatabaseLastRunStatus");

    AddLogLine(tr("Mythfrontend version: %1 (%2)").arg(MYTH_SOURCE_PATH)
               .arg(MYTH_SOURCE_VERSION), helpmsg);
    AddLogLine(tr("Last mythfilldatabase guide update:"), helpmsg);
    tmp = tr("Started:   %1").arg(
        MythDate::toString(
            mfdLastRunStart, MythDate::kDateTimeFull | MythDate::kSimplify));
    AddLogLine(tmp, helpmsg);

    if (mfdLastRunEnd >= mfdLastRunStart)
    {
        tmp = tr("Finished: %1")
            .arg(MythDate::toString(
                     mfdLastRunEnd,
                     MythDate::kDateTimeFull | MythDate::kSimplify));
        AddLogLine(tmp, helpmsg);
    }

    AddLogLine(tr("Result: %1").arg(mfdLastRunStatus), helpmsg);


    if (mfdNextRunStart >= mfdLastRunStart)
    {
        tmp = tr("Suggested Next: %1")
            .arg(MythDate::toString(
                     mfdNextRunStart,
                     MythDate::kDateTimeFull | MythDate::kSimplify));
        AddLogLine(tmp, helpmsg);
    }

    int DaysOfData = qdtNow.daysTo(GuideDataThrough);

    if (GuideDataThrough.isNull())
    {
        AddLogLine(tr("There's no guide data available!"), helpmsg,
                   "", "warning");
        AddLogLine(tr("Have you run mythfilldatabase?"), helpmsg,
                   "", "warning");
    }
    else
    {
        AddLogLine(
            tr("There is guide data until %1")
            .arg(MythDate::toString(
                     GuideDataThrough,
                     MythDate::kDateTimeFull | MythDate::kSimplify)), helpmsg);

        AddLogLine(QString("(%1).").arg(tr("%n day(s)", "", DaysOfData)),
                   helpmsg);
    }

    if (DaysOfData <= 3)
        AddLogLine(tr("WARNING: is mythfilldatabase running?"), helpmsg,
                   "", "", "warning");
}

void StatusBox::doScheduleStatus()
{
    if (m_iconState)
        m_iconState->DisplayState("schedule");
    m_logList->Reset();

    QString helpmsg(tr("Schedule Status shows current statistics "
                       "from the scheduler."));
    if (m_helpText)
        m_helpText->SetText(helpmsg);
    if (m_justHelpText)
        m_justHelpText->SetText(helpmsg);

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT COUNT(*) FROM record WHERE type = :TEMPLATE");
    query.bindValue(":TEMPLATE", kTemplateRecord);
    if (query.exec() && query.next())
    {
        QString rules = tr("%n template rule(s) (is) defined", "",
                            query.value(0).toInt());
        AddLogLine(rules, helpmsg);
    }
    else
    {
        MythDB::DBError("StatusBox::doScheduleStatus()", query);
        return;
    }

    query.prepare("SELECT COUNT(*) FROM record "
                  "WHERE type <> :TEMPLATE AND search = :NOSEARCH");
    query.bindValue(":TEMPLATE", kTemplateRecord);
    query.bindValue(":NOSEARCH", kNoSearch);
    if (query.exec() && query.next())
    {
        QString rules = tr("%n standard rule(s) (is) defined", "",
                            query.value(0).toInt());
        AddLogLine(rules, helpmsg);
    }
    else
    {
        MythDB::DBError("StatusBox::doScheduleStatus()", query);
        return;
    }

    query.prepare("SELECT COUNT(*) FROM record WHERE search > :NOSEARCH");
    query.bindValue(":NOSEARCH", kNoSearch);
    if (query.exec() && query.next())
    {
        QString rules = tr("%n search rule(s) are defined", "",
                            query.value(0).toInt());
        AddLogLine(rules, helpmsg);
    }
    else
    {
        MythDB::DBError("StatusBox::doScheduleStatus()", query);
        return;
    }

    QMap<RecStatus::Type, int> statusMatch;
    QMap<RecStatus::Type, QString> statusText;
    QMap<int, int> sourceMatch;
    QMap<int, QString> sourceText;
    QMap<int, int> cardMatch;
    QMap<int, QString> cardText;
    QMap<int, int> cardParent;
    QMap<int, bool> cardSchedGroup;
    QString tmpstr;
    int maxSource = 0;
    int maxCard = 0;
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

    query.prepare("SELECT MAX(cardid) FROM capturecard");
    if (query.exec())
    {
        if (query.next())
            maxCard = query.value(0).toInt();
    }

    query.prepare("SELECT cardid, inputname, displayname, parentid, "
                  "       schedgroup "
                  "FROM capturecard");
    if (query.exec())
    {
        while (query.next())
        {
            int inputid = query.value(0).toInt();
            cardText[inputid] = query.value(2).toString();
            if (cardText[inputid].isEmpty())
                cardText[inputid] = QString::number(query.value(1).toInt());
            cardParent[inputid] = query.value(3).toInt();
            cardSchedGroup[inputid] = query.value(4).toBool();
        }
    }

    ProgramList schedList;
    LoadFromScheduler(schedList);

    tmpstr = tr("%n matching showing(s)", "", schedList.size());
    AddLogLine(tmpstr, helpmsg);

    ProgramList::const_iterator it = schedList.begin();
    for (; it != schedList.end(); ++it)
    {
        const ProgramInfo *s = *it;
        const RecStatus::Type recstatus = s->GetRecordingStatus();

        if (statusMatch[recstatus] < 1)
        {
            statusText[recstatus] = RecStatus::toString(
                recstatus, s->GetRecordingRuleType());
        }

        ++statusMatch[recstatus];

        if (recstatus == RecStatus::WillRecord ||
            recstatus == RecStatus::Pending ||
            recstatus == RecStatus::Recording ||
            recstatus == RecStatus::Tuning ||
            recstatus == RecStatus::Failing)
        {
            ++sourceMatch[s->GetSourceID()];
            int inputid = s->GetInputID();
            // When schedgroup is used, always attribute recordings to
            // the parent inputs.
            if (cardParent[inputid] && cardSchedGroup[cardParent[inputid]])
                inputid = cardParent[inputid];
            ++cardMatch[inputid];
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
            AddLogLine(tmpstr, helpmsg, tmpstr, tmpstr, fstate);\
        }                                                       \
    } while (false)
    ADD_STATUS_LOG_LINE(RecStatus::Recording, "");
    ADD_STATUS_LOG_LINE(RecStatus::Tuning, "");
    ADD_STATUS_LOG_LINE(RecStatus::Failing, "error");
    ADD_STATUS_LOG_LINE(RecStatus::Pending, "");
    ADD_STATUS_LOG_LINE(RecStatus::WillRecord, "");
    ADD_STATUS_LOG_LINE(RecStatus::Conflict, "error");
    ADD_STATUS_LOG_LINE(RecStatus::TooManyRecordings, "warning");
    ADD_STATUS_LOG_LINE(RecStatus::LowDiskSpace, "warning");
    ADD_STATUS_LOG_LINE(RecStatus::LaterShowing, "warning");
    ADD_STATUS_LOG_LINE(RecStatus::NotListed, "warning");

    QString willrec = statusText[RecStatus::WillRecord];

    if (lowerpriority > 0)
    {
        tmpstr = QString("%1 %2 %3").arg(lowerpriority).arg(willrec)
                                    .arg(tr("with lower priority"));
        AddLogLine(tmpstr, helpmsg, tmpstr, tmpstr, "warning");
    }
    if (hdflag > 0)
    {
        tmpstr = QString("%1 %2 %3").arg(hdflag).arg(willrec)
                                    .arg(tr("marked as HDTV"));
        AddLogLine(tmpstr, helpmsg);
    }
    for (int i = 1; i <= maxSource; ++i)
    {
        if (sourceMatch[i] > 0)
        {
            tmpstr = QString("%1 %2 %3 %4 \"%5\"")
                             .arg(sourceMatch[i]).arg(willrec)
                             .arg(tr("from source")).arg(i).arg(sourceText[i]);
            AddLogLine(tmpstr, helpmsg);
        }
    }
    for (int i = 1; i <= maxCard; ++i)
    {
        if (cardMatch[i] > 0)
        {
            tmpstr = QString("%1 %2 %3 %4 \"%5\"")
                             .arg(cardMatch[i]).arg(willrec)
                             .arg(tr("on input")).arg(i).arg(cardText[i]);
            AddLogLine(tmpstr, helpmsg);
        }
    }
}

void StatusBox::doTunerStatus()
{
    struct info
    {
        int         m_inputid;
        bool        m_schedgroup;
        QString     m_displayname;
        int         m_errored;
        int         m_unavailable;
        int         m_sleeping;
        int         m_recording;
        int         m_livetv;
        int         m_available;
        QStringList m_recordings;
    };
    QMap<int, struct info> info;
    QList<int> inputids;
        
    if (m_iconState)
        m_iconState->DisplayState("tuner");
    m_logList->Reset();

    QString helpmsg(tr("Input Status shows the current information "
                       "about the state of backend inputs"));
    if (m_helpText)
        m_helpText->SetText(helpmsg);
    if (m_justHelpText)
        m_justHelpText->SetText(helpmsg);

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT cardid, parentid, schedgroup, displayname "
        "FROM capturecard ORDER BY cardid");

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("StatusBox::doTunerStatus()", query);
        return;
    }

    while (query.next())
    {
        int inputid = query.value(0).toInt();
        int parentid = query.value(1).toInt();

        // If this is a schedgroup child, attribute all status to the
        // parent.
        int infoid = inputid;
        if (parentid && info[parentid].m_schedgroup)
            infoid = parentid;
        else
        {
            info[infoid].m_inputid = inputid;
            info[infoid].m_schedgroup = query.value(2).toBool();
            info[infoid].m_displayname = query.value(3).toString();
            inputids.append(inputid);
        }

        QString cmd = QString("QUERY_REMOTEENCODER %1").arg(inputid);
        QStringList strlist( cmd );
        strlist << "GET_STATE";

        gCoreContext->SendReceiveStringList(strlist);
        int state = strlist[0].toInt();

        QString status;
        QString fontstate;
        if (state == kState_Error)
        {
            strlist.clear();
            strlist << QString("QUERY_REMOTEENCODER %1").arg(inputid);
            strlist << "GET_SLEEPSTATUS";

            gCoreContext->SendReceiveStringList(strlist);
            int sleepState = strlist[0].toInt();

            if (sleepState == -1)
                info[infoid].m_errored += 1;
            else if (sleepState == sStatus_Undefined)
                info[infoid].m_unavailable += 1;
            else
                info[infoid].m_sleeping += 1;
        }
        else if (state == kState_RecordingOnly ||
                 state == kState_WatchingRecording)
            info[infoid].m_recording += 1;
        else if (state == kState_WatchingLiveTV)
            info[infoid].m_livetv += 1;
        else
            info[infoid].m_available += 1;

        if (state == kState_RecordingOnly ||
            state == kState_WatchingRecording ||
            state == kState_WatchingLiveTV)
        {
            strlist = QStringList( QString("QUERY_RECORDER %1").arg(inputid));
            strlist << "GET_RECORDING";
            gCoreContext->SendReceiveStringList(strlist);
            ProgramInfo pginfo(strlist);
            if (pginfo.GetChanID())
            {
                QString titlesub = pginfo.GetTitle();
                if (!pginfo.GetSubtitle().isEmpty())
                    titlesub += QString(" - ") + pginfo.GetSubtitle();
                info[infoid].m_recordings += titlesub;
            }
        }
    }

    QList<int>::iterator it = inputids.begin();
    for ( ; it != inputids.end(); ++it)
    {
        int inputid = *it;

        QStringList statuslist;
        if (info[inputid].m_errored)
            statuslist << tr("%1 errored").arg(info[inputid].m_errored);
        if (info[inputid].m_unavailable)
            statuslist << tr("%1 unavailable").arg(info[inputid].m_unavailable);
        if (info[inputid].m_sleeping)
            statuslist << tr("%1 sleeping").arg(info[inputid].m_sleeping);
        if (info[inputid].m_recording)
            statuslist << tr("%1 recording").arg(info[inputid].m_recording);
        if (info[inputid].m_livetv)
            statuslist << tr("%1 live television").arg(info[inputid].m_livetv);
        if (info[inputid].m_available)
            statuslist << tr("%1 available").arg(info[inputid].m_available);

        QString fontstate;
        if (info[inputid].m_errored)
            fontstate = "error";
        else if (info[inputid].m_unavailable || info[inputid].m_sleeping)
            fontstate = "warning";

        QString shortstatus = tr("Input %1 %2: %3")
            .arg(inputid).arg(info[inputid].m_displayname)
            .arg(statuslist.join(tr(", ")));
        QString longstatus = shortstatus + "\n" +
            info[inputid].m_recordings.join("\n");

        AddLogLine(shortstatus, helpmsg, longstatus, longstatus, fontstate);
    }
}

void StatusBox::doLogEntries(void)
{
    if (m_iconState)
        m_iconState->DisplayState("log");
    m_logList->Reset();

    QString helpmsg(tr("Log Entries shows any unread log entries "
                       "from the system if you have logging enabled"));
    if (m_helpText)
        m_helpText->SetText(helpmsg);
    if (m_justHelpText)
        m_justHelpText->SetText(helpmsg);

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

            detail = tr("On %1 from %2.%3\n%4\n")
                .arg(MythDate::toString(
                         MythDate::as_utc(query.value(3).toDateTime()),
                         MythDate::kDateTimeShort))
                .arg(query.value(4).toString())
                .arg(query.value(1).toString())
                .arg(query.value(5).toString());

            QString tmp = query.value(6).toString();
            if (!tmp.isEmpty())
                detail.append(tmp);
            else
                detail.append(tr("No further details"));

            AddLogLine(line, helpmsg, detail, detail,
                       "", query.value(0).toString());
        }

        if (query.size() == 0)
        {
            AddLogLine(tr("No items found at priority level %1 or lower.")
                       .arg(m_minLevel), helpmsg);
            AddLogLine(tr("Use 1-8 to change priority level."), helpmsg);
        }
    }
}

void StatusBox::doJobQueueStatus()
{
    if (m_iconState)
        m_iconState->DisplayState("jobqueue");
    m_logList->Reset();

    QString helpmsg(tr("Job Queue shows any jobs currently in "
                       "MythTV's Job Queue such as a commercial "
                       "detection job."));
    if (m_helpText)
        m_helpText->SetText(helpmsg);
    if (m_justHelpText)
        m_justHelpText->SetText(helpmsg);

    QMap<int, JobQueueEntry> jobs;
    QMap<int, JobQueueEntry>::Iterator it;

    JobQueue::GetJobsInQueue(jobs,
                             JOB_LIST_NOT_DONE | JOB_LIST_ERROR |
                             JOB_LIST_RECENT);

    if (!jobs.empty())
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
                .arg(MythDate::toString(
                         pginfo.GetRecordingStartTime(),
                         MythDate::kDateTimeFull | MythDate::kSimplify))
                .arg(tr("Job:"))
                .arg(JobQueue::JobText((*it).type))
                .arg(tr("Status: "))
                .arg(JobQueue::StatusText((*it).status));

            if ((*it).status != JOB_QUEUED)
                detail += " (" + (*it).hostname + ')';

            if ((*it).schedruntime > MythDate::current())
                detail += '\n' + tr("Scheduled Run Time:") + ' ' +
                    MythDate::toString(
                        (*it).schedruntime,
                        MythDate::kDateTimeFull | MythDate::kSimplify);
            else
                detail += '\n' + (*it).comment;

            line = QString("%1 @ %2").arg(pginfo.GetTitle())
                .arg(MythDate::toString(
                         pginfo.GetRecordingStartTime(),
                         MythDate::kDateTimeFull | MythDate::kSimplify));

            QString font;
            if ((*it).status == JOB_ERRORED)
                font = "error";
            else if ((*it).status == JOB_ABORTED)
                font = "warning";

            AddLogLine(line, helpmsg, detail, detail, font,
                       QString("%1").arg((*it).id));
        }
    }
    else
        AddLogLine(tr("Job Queue is currently empty."), helpmsg);

}

// Some helper routines for doMachineStatus() that format the output strings

/** \fn sm_str(long long, int)
 *  \brief Returns a short string describing an amount of space, choosing
 *         one of a number of useful units, "TB", "GB", "MB", or "KB".
 *  \param sizeKB Number of kilobytes.
 *  \param prec   Precision to use if we have less than ten of whatever
 *                unit is chosen.
 */
static QString sm_str(long long sizeKB, int prec=1)
{
    if (sizeKB>1024*1024*1024) // Terabytes
    {
        double sizeGB = sizeKB/(1024*1024*1024.0);
        return QObject::tr("%1 TB").arg(sizeGB, 0, 'f', (sizeGB>10)?0:prec);
    }
    if (sizeKB>1024*1024) // Gigabytes
    {
        double sizeGB = sizeKB/(1024*1024.0);
        return QObject::tr("%1 GB").arg(sizeGB, 0, 'f', (sizeGB>10)?0:prec);
    }
    if (sizeKB>1024) // Megabytes
    {
        double sizeMB = sizeKB/1024.0;
        return QObject::tr("%1 MB").arg(sizeMB, 0, 'f', (sizeMB>10)?0:prec);
    }
    // Kilobytes
    return QObject::tr("%1 KB").arg(sizeKB);
}

static QString usage_str_kb(long long total,
                                  long long used,
                                  long long free)
{
    QString ret = QObject::tr("Unknown");
    if (total > 0.0 && free > 0.0)
    {
        double percent = (100.0*free)/total;
        ret = StatusBox::tr("%1 total, %2 used, %3 (or %4%) free.")
            .arg(sm_str(total)).arg(sm_str(used))
            .arg(sm_str(free)).arg(percent, 0, 'f', (percent >= 10.0) ? 0 : 2);
    }
    return ret;
}

static QString usage_str_mb(float total, float used, float free)
{
    return usage_str_kb((long long)(total*1024), (long long)(used*1024),
                        (long long)(free*1024));
}

static void disk_usage_with_rec_time_kb(QStringList& out, long long total,
                                        long long used, long long free,
                                        const recprof2bps_t& prof2bps)
{
    const QString tail = StatusBox::tr(", using your %1 rate of %2 kb/s");

    out<<usage_str_kb(total, used, free);
    if (free<0)
        return;

    recprof2bps_t::const_iterator it = prof2bps.begin();
    for (; it != prof2bps.end(); ++it)
    {
        const QString pro =
                tail.arg(it.key()).arg((int)((float)(*it) / 1024.0F));

        long long bytesPerMin = ((*it) >> 1) * 15;
        uint minLeft = ((free<<5)/bytesPerMin)<<5;
        minLeft = (minLeft/15)*15;
        uint hoursLeft = minLeft/60;
        QString hourstring = StatusBox::tr("%n hour(s)", "", hoursLeft);
        QString minstring = StatusBox::tr("%n minute(s)", "", minLeft%60);
        QString remainstring = StatusBox::tr("%1 remaining", "time");
        if (minLeft%60 == 0)
            out<<remainstring.arg(hourstring) + pro;
        else if (minLeft > 60)
            out<<StatusBox::tr("%1 and %2 remaining", "time").arg(hourstring)
                                                   .arg(minstring) + pro;
        else
            out<<remainstring.arg(minstring) + pro;
    }
}

static QString uptimeStr(time_t uptime)
{
    QString str = "   " + StatusBox::tr("Uptime") + ": ";

    if (uptime == 0)
        return str + StatusBox::tr("unknown", "unknown uptime");

    int days = uptime/(60*60*24);
    uptime -= days*60*60*24;
    int hours = uptime/(60*60);
    uptime -= hours*60*60;
    int min  = uptime/60;
    int secs = uptime%60;

    if (days > 0)
    {
        char    buff[6];
        QString dayLabel = StatusBox::tr("%n day(s)", "", days);

        sprintf(buff, "%d:%02d", hours, min);

        return str + QString("%1, %2").arg(dayLabel).arg(buff);
    }

    char  buff[9];
    sprintf(buff, "%d:%02d:%02d", hours, min, secs);
    return str + QString( buff );
}

/** \fn StatusBox::getActualRecordedBPS(QString hostnames)
 *  \brief Fills in recordingProfilesBPS w/ average bitrate from recorded table
 */
void StatusBox::getActualRecordedBPS(const QString& hostnames)
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
        QString rateStr = tr("average", "average rate");

        // Don't user a tr() directly here as the Qt tools will
        // not be able to extract the string for translation.
        recordingProfilesBPS[rateStr] =
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
        QString rateStr = tr("maximum", "maximum rate");

        // Don't user a tr() directly here as the Qt tools will
        // not be able to extract the string for translation.
        recordingProfilesBPS[rateStr] =
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
                            "statistics of this machine and the MythTV "
                            "server.");

    if (m_helpText)
        m_helpText->SetText(machineStr);
    if (m_justHelpText)
        m_justHelpText->SetText(machineStr);

    int    totalM = 0;    // Physical memory
    int    usedM = 0;
    int    freeM = 0;
    int    totalS = 0;    // Virtual  memory (swap)
    int    usedS = 0;
    int    freeS = 0;
    time_t uptime = 0;

    QString line;
    if (m_isBackendActive)
        line = tr("System:");
    else
        line = tr("This machine:");
    AddLogLine(line, machineStr);

    // Hostname & IP
    line = "   " + tr("Hostname") + ": " + gCoreContext->GetHostName();
    line.append(", " + tr("IP") + ": ");
    QString sep = "";
    foreach(QNetworkInterface iface, QNetworkInterface::allInterfaces())
    {
        QNetworkInterface::InterfaceFlags f = iface.flags();
        if (!(f & QNetworkInterface::IsUp))
            continue;
        if (!(f & QNetworkInterface::IsRunning))
            continue;
        if (f & QNetworkInterface::IsLoopBack)
            continue;

        foreach(QNetworkAddressEntry addr, iface.addressEntries())
        {
            if (addr.ip().protocol() == QAbstractSocket::IPv4Protocol ||
                addr.ip().protocol() == QAbstractSocket::IPv6Protocol)
            {
                line += sep + addr.ip().toString();
                if (sep.isEmpty())
                    sep = ", ";
            }
        }
        line += "";
    }
    AddLogLine(line, machineStr);

    // uptime
    if (!getUptime(uptime))
        uptime = 0;
    line = uptimeStr(uptime);

    // weighted average loads
    line.append(", " + tr("Load") + ": ");

#if defined(_WIN32) || defined(Q_OS_ANDROID)
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

    AddLogLine(line, machineStr);

    // memory usage
    if (getMemStats(totalM, freeM, totalS, freeS))
    {
        usedM = totalM - freeM;
        if (totalM > 0)
        {
            line = "   " + tr("RAM") + ": " + usage_str_mb(totalM, usedM, freeM);
            AddLogLine(line, machineStr);
        }
        usedS = totalS - freeS;
        if (totalS > 0)
        {
            line = "   " + tr("Swap") +
                                  ": "  + usage_str_mb(totalS, usedS, freeS);
            AddLogLine(line, machineStr);
        }
    }

    if (!m_isBackendActive)
    {
        line = tr("MythTV server") + ':';
        AddLogLine(line, machineStr);

        // Hostname & IP
        line = "   " + tr("Hostname") + ": " + gCoreContext->GetSetting("MasterServerName");
        line.append(", " + tr("IP") + ": " + gCoreContext->GetSetting("MasterServerIP"));
        AddLogLine(line, machineStr);

        // uptime
        if (!RemoteGetUptime(uptime))
            uptime = 0;
        line = uptimeStr(uptime);

        // weighted average loads
        line.append(", " + tr("Load") + ": ");
        float floads[3];
        if (RemoteGetLoad(floads))
        {
            char buff[30];

            sprintf(buff, "%0.2f, %0.2f, %0.2f",
                    static_cast<double>(floads[0]),
                    static_cast<double>(floads[1]),
                    static_cast<double>(floads[2]));
            line.append(QString(buff));
        }
        else
            line.append(tr("unknown"));

        AddLogLine(line, machineStr);

        // memory usage
        if (RemoteGetMemStats(totalM, freeM, totalS, freeS))
        {
            usedM = totalM - freeM;
            if (totalM > 0)
            {
                line = "   " + tr("RAM") +
                                     ": "  + usage_str_mb(totalM, usedM, freeM);
                AddLogLine(line, machineStr);
            }

            usedS = totalS - freeS;
            if (totalS > 0)
            {
                line = "   " + tr("Swap") +
                                     ": "  + usage_str_mb(totalS, usedS, freeS);
                AddLogLine(line, machineStr);
            }
        }
    }

    // get free disk space
    QString hostnames;

    QList<FileSystemInfo> fsInfos = FileSystemInfo::RemoteGetInfo();
    for (int i = 0; i < fsInfos.size(); ++i)
    {
        // For a single-directory installation just display the totals
        if ((fsInfos.size() == 2) && (i == 0) &&
            (fsInfos[i].getPath() != "TotalDiskSpace") &&
            (fsInfos[i+1].getPath() == "TotalDiskSpace"))
            i++;

        hostnames = QString("\"%1\"").arg(fsInfos[i].getHostname());
        hostnames.replace(' ', "");
        hostnames.replace(',', "\",\"");

        getActualRecordedBPS(hostnames);

        QStringList list;
        disk_usage_with_rec_time_kb(list,
            fsInfos[i].getTotalSpace(), fsInfos[i].getUsedSpace(),
            fsInfos[i].getTotalSpace() - fsInfos[i].getUsedSpace(),
            recordingProfilesBPS);

        if (fsInfos[i].getPath() == "TotalDiskSpace")
        {
            line = tr("Total Disk Space:");
            AddLogLine(line, machineStr);
        }
        else
        {
            line = tr("MythTV Drive #%1:").arg(fsInfos[i].getFSysID());
            AddLogLine(line, machineStr);

            QStringList tokens = fsInfos[i].getPath().split(',');

            if (tokens.size() > 1)
            {
                AddLogLine(QString("   ") + tr("Directories:"), machineStr);

                int curToken = 0;
                while (curToken < tokens.size())
                    AddLogLine(QString("      ") +
                               tokens[curToken++], machineStr);
            }
            else
            {
                AddLogLine(QString("   " ) + tr("Directory:") + ' ' +
                           fsInfos[i].getPath(), machineStr);
            }
        }

        QStringList::iterator it = list.begin();
        for (;it != list.end(); ++it)
        {
            line = QString("   ") + (*it);
            AddLogLine(line, machineStr);
        }
    }

}

/** \fn StatusBox::doAutoExpireList()
 *  \brief Show list of recordings which may AutoExpire
 */
void StatusBox::doAutoExpireList(bool updateExpList)
{
    if (m_iconState)
        m_iconState->DisplayState("autoexpire");
    m_logList->Reset();

    QString helpmsg(tr("The AutoExpire List shows all recordings "
                       "which may be expired and the order of "
                       "their expiration. Recordings at the top "
                       "of the list will be expired first."));
    if (m_helpText)
        m_helpText->SetText(helpmsg);
    if (m_justHelpText)
        m_justHelpText->SetText(helpmsg);

    QString               contentLine;
    QString               detailInfo;
    QString               staticInfo;
    long long             totalSize(0);
    long long             liveTVSize(0);
    int                   liveTVCount(0);
    long long             deletedGroupSize(0);
    int                   deletedGroupCount(0);

    vector<ProgramInfo *>::iterator it;

    if (updateExpList)
    {
        for (it = m_expList.begin(); it != m_expList.end(); ++it)
            delete *it;
        m_expList.clear();

        RemoteGetAllExpiringRecordings(m_expList);
    }

    for (it = m_expList.begin(); it != m_expList.end(); ++it)
    {
        ProgramInfo *pginfo = *it;

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
        ProgramInfo *pginfo = *it;
        QDateTime starttime = pginfo->GetRecordingStartTime();
        QDateTime endtime = pginfo->GetRecordingEndTime();
        contentLine =
            MythDate::toString(
                starttime, MythDate::kDateFull | MythDate::kSimplify) + " - ";

        contentLine +=
            "(" + ProgramInfo::i18n(pginfo->GetRecordingGroup()) + ") ";

        contentLine += pginfo->GetTitle() +
            " (" + sm_str(pginfo->GetFilesize() / 1024) + ")";

        detailInfo =
            MythDate::toString(
                starttime, MythDate::kDateTimeFull | MythDate::kSimplify) +
            " - " +
            MythDate::toString(
                endtime, MythDate::kDateTimeFull | MythDate::kSimplify);

        detailInfo += " (" + sm_str(pginfo->GetFilesize() / 1024) + ")";

        detailInfo += " (" + ProgramInfo::i18n(pginfo->GetRecordingGroup()) + ")";

        detailInfo += "\n" + pginfo->toString(ProgramInfo::kTitleSubtitle, " - ");

        AddLogLine(contentLine, staticInfo, detailInfo,
                   staticInfo + detailInfo);
    }
}

Q_DECLARE_METATYPE(LogLine)

/* vim: set expandtab tabstop=4 shiftwidth=4: */
