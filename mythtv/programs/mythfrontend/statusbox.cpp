// Qt
#include <QHostAddress>
#include <QNetworkInterface>

// MythTV
#include "libmythbase/filesysteminfo.h"
#include "libmythbase/mythchrono.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythdb.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythmiscutil.h"
#include "libmythbase/mythversion.h"
#include "libmythbase/remoteutil.h"
#include "libmythbase/stringutil.h"
#include "libmythtv/cardutil.h"
#include "libmythtv/decoders/mythcodeccontext.h"
#include "libmythtv/jobqueue.h"
#include "libmythtv/recordinginfo.h"
#include "libmythtv/tv.h"
#include "libmythui/mythdialogbox.h"
#include "libmythui/mythdisplay.h"
#include "libmythui/mythrender_base.h"
#include "libmythui/mythuibuttonlist.h"
#include "libmythui/mythuihelper.h"
#include "libmythui/mythuistatetype.h"
#include "libmythui/mythuitext.h"
#include "libmythui/opengl/mythrenderopengl.h"

// MythFrontend
#include "statusbox.h"

struct LogLine {
    QString m_line;
    QString m_detail;
    QString m_help;
    QString m_helpdetail;
    QString m_data;
    QString m_state;
};

void StatusBoxItem::Start(std::chrono::seconds Interval)
{
    connect(this, &QTimer::timeout, [this]() { emit UpdateRequired(this); });
    start(Interval);
}

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

    connect(m_categoryList, &MythUIButtonList::itemSelected,
            this, &StatusBox::updateLogList);
    connect(m_logList, &MythUIButtonList::itemSelected,
            this, &StatusBox::setHelpText);
    connect(m_logList, &MythUIButtonList::itemClicked,
            this, &StatusBox::clicked);

    BuildFocusList();
    return true;
}

void StatusBox::Init()
{
    auto *item = new MythUIButtonListItem(m_categoryList, tr("Listings Status"),
                                          &StatusBox::doListingsStatus);
    item->DisplayState("listings", "icon");

    item = new MythUIButtonListItem(m_categoryList, tr("Schedule Status"),
                                    &StatusBox::doScheduleStatus);
    item->DisplayState("schedule", "icon");

    item = new MythUIButtonListItem(m_categoryList, tr("Input Status"),
                                    &StatusBox::doTunerStatus);
    item->DisplayState("tuner", "icon");

    item = new MythUIButtonListItem(m_categoryList, tr("Job Queue"),
                                    &StatusBox::doJobQueueStatus);
    item->DisplayState("jobqueue", "icon");

    item = new MythUIButtonListItem(m_categoryList, tr("Video decoders"),
                                    &StatusBox::doDecoderStatus);
    item->DisplayState("decoders", "icon");

    item = new MythUIButtonListItem(m_categoryList, tr("Display"),
                                    &StatusBox::doDisplayStatus);
    item->DisplayState("display", "icon");

    item = new MythUIButtonListItem(m_categoryList, tr("Rendering"),
                                    &StatusBox::doRenderStatus);
    item->DisplayState("render", "icon");

    item = new MythUIButtonListItem(m_categoryList, tr("Machine Status"),
                                    &StatusBox::doMachineStatus);
    item->DisplayState("machine", "icon");

    item = new MythUIButtonListItem(m_categoryList, tr("AutoExpire List"),
                                    qOverload<>(&StatusBox::doAutoExpireList));
    item->DisplayState("autoexpire", "icon");

    int itemCurrent = gCoreContext->GetNumSetting("StatusBoxItemCurrent", 0);
    m_categoryList->SetItemCurrent(itemCurrent);
}

StatusBoxItem *StatusBox::AddLogLine(const QString & line,
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

    auto *item = new StatusBoxItem(m_logList, line, QVariant::fromValue(logline));
    if (logline.m_state.isEmpty())
        logline.m_state = "normal";

    item->SetFontState(logline.m_state);
    item->DisplayState(logline.m_state, "status");
    item->SetText(logline.m_detail, "detail");
    return item;
}

bool StatusBox::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("Status", event, actions);

#if 0
    for (int i = 0; i < actions.size() && !handled; ++i)
    {
        QString action = actions[i];
        handled = true;

        if (action == "MENU")
        {
        }
        else
            handled = false;
    }
#endif

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void StatusBox::setHelpText(MythUIButtonListItem *item)
{
    if (!item || GetFocusWidget() != m_logList)
        return;

    auto logline = item->GetData().value<LogLine>();
    if (m_helpText)
        m_helpText->SetText(logline.m_helpdetail);
    if (m_justHelpText)
        m_justHelpText->SetText(logline.m_help);
}

void StatusBox::updateLogList(MythUIButtonListItem *item)
{
    if (!item)
        return;

    disconnect(this, &StatusBox::updateLog,nullptr,nullptr);

    if (item->GetData().value<MythUICallbackMF>())
    {
        connect(this, &StatusBox::updateLog,
                item->GetData().value<MythUICallbackMF>());
        emit updateLog();
    }
    else if (item->GetData().value<MythUICallbackMFc>())
    {
        connect(this, &StatusBox::updateLog,
                item->GetData().value<MythUICallbackMFc>());
        emit updateLog();
    }
}

void StatusBox::clicked(MythUIButtonListItem *item)
{
    if (!item)
        return;

    auto logline = item->GetData().value<LogLine>();

    MythUIButtonListItem *currentButton = m_categoryList->GetItemCurrent();
    QString currentItem;
    if (currentButton)
        currentItem = currentButton->GetText();

    // FIXME: Comparisons against strings here is not great, changing names
    //        breaks everything and it's inefficient
    if (currentItem == tr("Job Queue"))
    {
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

            QVariant data = QVariant::fromValue(logline.m_data);

            if (jobStatus == JOB_PAUSED)
                menuPopup->AddButtonV(tr("Resume"), data);
            else
                menuPopup->AddButtonV(tr("Pause"), data);
            menuPopup->AddButtonV(tr("Stop"), data);
            menuPopup->AddButtonV(tr("No Change"), data);
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

            menuPopup->AddButtonV(tr("Delete Now"), QVariant::fromValue(rec));
            if ((rec)->GetRecordingGroup() == "LiveTV")
            {
                menuPopup->AddButtonV(tr("Move to Default group"),
                                                       QVariant::fromValue(rec));
            }
            else if ((rec)->GetRecordingGroup() == "Deleted")
            {
                menuPopup->AddButtonV(tr("Undelete"), QVariant::fromValue(rec));
            }
            else
            {
                menuPopup->AddButtonV(tr("Disable AutoExpire"),
                                                        QVariant::fromValue(rec));
            }
            menuPopup->AddButtonV(tr("No Change"), QVariant::fromValue(rec));

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

        if (resultid == "JobDelete")
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

    AddLogLine(tr("Mythfrontend version: %1 (%2)")
               .arg(GetMythSourcePath(), GetMythSourceVersion()),
               helpmsg);
    AddLogLine(tr("Database schema version: %1").arg(gCoreContext->GetSetting("DBSchemaVer")));
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

    for (auto *s : schedList)
    {
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

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
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
        tmpstr = QString("%1 %2 %3").arg(QString::number(lowerpriority),
                                         willrec, tr("with lower priority"));
        AddLogLine(tmpstr, helpmsg, tmpstr, tmpstr, "warning");
    }
    if (hdflag > 0)
    {
        tmpstr = QString("%1 %2 %3").arg(QString::number(hdflag),
                                         willrec, tr("marked as HDTV"));
        AddLogLine(tmpstr, helpmsg);
    }
    for (int i = 1; i <= maxSource; ++i)
    {
        if (sourceMatch[i] > 0)
        {
            tmpstr = QString("%1 %2 %3 %4 \"%5\"")
                             .arg(QString::number(sourceMatch[i]), willrec,
                                  tr("from source"), QString::number(i), sourceText[i]);
            AddLogLine(tmpstr, helpmsg);
        }
    }
    for (int i = 1; i <= maxCard; ++i)
    {
        if (cardMatch[i] > 0)
        {
            tmpstr = QString("%1 %2 %3 %4 \"%5\"")
                             .arg(QString::number(cardMatch[i]), willrec,
                                  tr("on input"), QString::number(i), cardText[i]);
            AddLogLine(tmpstr, helpmsg);
        }
    }
}

void StatusBox::doTunerStatus()
{
    struct info
    {
        int         m_errored     {0};
        int         m_unavailable {0};
        int         m_sleeping    {0};
        int         m_recording   {0};
        int         m_livetv      {0};
        int         m_available   {0};
        QStringList m_recordings;
    };
    QMap<QString, struct info> info;
    QStringList inputnames;
        
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
        "SELECT cardid, displayname "
        "FROM capturecard ORDER BY displayname, cardid");

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("StatusBox::doTunerStatus()", query);
        return;
    }

    while (query.next())
    {
        int inputid = query.value(0).toInt();
	QString inputname = query.value(1).toString();
	if (!info.contains(inputname))
	    inputnames.append(inputname);

        QString cmd = QString("QUERY_REMOTEENCODER %1").arg(inputid);
        QStringList strlist( cmd );
        strlist << "GET_STATE";

        gCoreContext->SendReceiveStringList(strlist);
        int state = strlist[0].toInt();

        if (state == kState_Error)
        {
            strlist.clear();
            strlist << QString("QUERY_REMOTEENCODER %1").arg(inputid);
            strlist << "GET_SLEEPSTATUS";

            gCoreContext->SendReceiveStringList(strlist);
            int sleepState = strlist[0].toInt();

            if (sleepState == -1)
                info[inputname].m_errored += 1;
            else if (sleepState == sStatus_Undefined)
                info[inputname].m_unavailable += 1;
            else
                info[inputname].m_sleeping += 1;
        }
        else if (state == kState_RecordingOnly ||
                 state == kState_WatchingRecording)
        {
            info[inputname].m_recording += 1;
        }
        else if (state == kState_WatchingLiveTV)
        {
            info[inputname].m_livetv += 1;
        }
        else
        {
            info[inputname].m_available += 1;
        }

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
                info[inputname].m_recordings += titlesub;
            }
        }
    }

    for (const QString& inputname : std::as_const(inputnames))
    {
        QStringList statuslist;
        if (info[inputname].m_errored)
            statuslist << tr("%1 errored").arg(info[inputname].m_errored);
        if (info[inputname].m_unavailable)
            statuslist << tr("%1 unavailable").arg(info[inputname].m_unavailable);
        if (info[inputname].m_sleeping)
            statuslist << tr("%1 sleeping").arg(info[inputname].m_sleeping);
        if (info[inputname].m_recording)
            statuslist << tr("%1 recording").arg(info[inputname].m_recording);
        if (info[inputname].m_livetv)
            statuslist << tr("%1 live television").arg(info[inputname].m_livetv);
        if (info[inputname].m_available)
            statuslist << tr("%1 available").arg(info[inputname].m_available);

        QString fontstate;
        if (info[inputname].m_errored)
            fontstate = "error";
        else if (info[inputname].m_unavailable || info[inputname].m_sleeping)
            fontstate = "warning";

        QString shortstatus = tr("Input %1: %2")
            .arg(inputname, statuslist.join(tr(", ")));
        QString longstatus = shortstatus + "\n" +
            info[inputname].m_recordings.join("\n");

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
                         MythDate::kDateTimeShort),
                     query.value(4).toString(),
                     query.value(1).toString(),
                     query.value(5).toString());

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
                .arg(pginfo.GetTitle(),
                     pginfo.GetChannelName(),
                     pginfo.GetChanNum(),
                     MythDate::toString(
                         pginfo.GetRecordingStartTime(),
                         MythDate::kDateTimeFull | MythDate::kSimplify),
                     tr("Job:"),
                     JobQueue::JobText((*it).type),
                     tr("Status: "),
                     JobQueue::StatusText((*it).status));

            if ((*it).status != JOB_QUEUED)
                detail += " (" + (*it).hostname + ')';

            if ((*it).schedruntime > MythDate::current())
            {
                detail += '\n' + tr("Scheduled Run Time:") + ' ' +
                    MythDate::toString(
                        (*it).schedruntime,
                        MythDate::kDateTimeFull | MythDate::kSimplify);
            }
            else
            {
                detail += '\n' + (*it).comment;
            }

            line = QString("%1 @ %2")
                .arg(pginfo.GetTitle(),
                     MythDate::toString(
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
    {
        AddLogLine(tr("Job Queue is currently empty."), helpmsg);
    }

}

// Some helper routines for doMachineStatus() that format the output strings

static QString usage_str_kb(long long total,
                                  long long used,
                                  long long free)
{
    QString ret = QObject::tr("Unknown");
    if (total > 0.0 && free > 0.0)
    {
        double percent = (100.0*free)/total;
        ret = StatusBox::tr("%1 total, %2 used, %3 (or %4%) free.")
            .arg(StringUtil::formatKBytes(total),
                 StringUtil::formatKBytes(used),
                 StringUtil::formatKBytes(free))
            .arg(percent, 0, 'f', (percent >= 10.0) ? 0 : 2);
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

    // NOLINTNEXTLINE(modernize-loop-convert)
    for (auto it = prof2bps.begin(); it != prof2bps.end(); ++it)
    {
        const QString pro =
                tail.arg(it.key()).arg((int)((float)(*it) / 1024.0F));

        long long bytesPerMin = ((*it) >> 1) * 15LL;
        uint minLeft = ((free<<5)/bytesPerMin)<<5;
        minLeft = (minLeft/15)*15;
        uint hoursLeft = minLeft/60;
        QString hourstring = StatusBox::tr("%n hour(s)", "", hoursLeft);
        QString minstring = StatusBox::tr("%n minute(s)", "", minLeft%60);
        QString remainstring = StatusBox::tr("%1 remaining", "time");
        if (minLeft%60 == 0)
            out<<remainstring.arg(hourstring) + pro;
        else if (minLeft > 60)
        {
            out<<StatusBox::tr("%1 and %2 remaining", "time")
                .arg(hourstring, minstring) + pro;
        }
        else
        {
            out<<remainstring.arg(minstring) + pro;
        }
    }
}

static QString uptimeStr(std::chrono::seconds uptime)
{
    QString str = "   " + StatusBox::tr("Uptime") + ": ";

    if (uptime == 0s)
        return str + StatusBox::tr("unknown", "unknown uptime");

    auto days = duration_cast<std::chrono::days>(uptime);
    auto secs = uptime % 24h;

    QString astext;
    if (days.count() > 0)
    {
        astext = QString("%1, %2")
            .arg(StatusBox::tr("%n day(s)", "", days.count()),
                 MythDate::formatTime(secs, "H:mm"));
    } else {
        astext = MythDate::formatTime(secs, "H:mm:ss");
    }
    return str + astext;
}

/** \fn StatusBox::getActualRecordedBPS(QString hostnames)
 *  \brief Fills in m_recordingProfilesBps w/ average bitrate from recorded table
 */
void StatusBox::getActualRecordedBPS(const QString& hostnames)
{
    m_recordingProfilesBps.clear();

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
        m_recordingProfilesBps[rateStr] =
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
        m_recordingProfilesBps[rateStr] =
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

    QString line;
    if (m_isBackendActive)
        line = tr("System:");
    else
        line = tr("This machine:");
    AddLogLine(line, machineStr);

    // Time
    StatusBoxItem *timebox = AddLogLine("");
    auto UpdateTime = [](StatusBoxItem* Item)
    {
        Item->SetText("   " + tr("System time") + ": " + QDateTime::currentDateTime().toString());
    };
    UpdateTime(timebox);
    connect(timebox, &StatusBoxItem::UpdateRequired, UpdateTime);
    timebox->Start();

    // Hostname & IP
    AddLogLine("   " + tr("Hostname") + ": " + gCoreContext->GetHostName());
    AddLogLine("   " + tr("OS") + QString(": %1 (%2)").arg(QSysInfo::prettyProductName(),
                                                           QSysInfo::currentCpuArchitecture()));
    AddLogLine("   " + tr("Qt version") + QString(": %1").arg(qVersion()));

    QList allInterfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface & iface : std::as_const(allInterfaces))
    {
        QNetworkInterface::InterfaceFlags f = iface.flags();
        if (!(f & QNetworkInterface::IsUp))
            continue;
        if (!(f & QNetworkInterface::IsRunning))
            continue;
        if ((f & QNetworkInterface::IsLoopBack) != 0U)
            continue;

        QNetworkInterface::InterfaceType type = iface.type();
        QString name = type == QNetworkInterface::Wifi ? tr("WiFi") : tr("Ethernet");
        AddLogLine("   " + name + QString(" (%1): ").arg(iface.humanReadableName()));
        AddLogLine("        " + tr("MAC Address") + ": " + iface.hardwareAddress());
        QList addresses = iface.addressEntries();
        for (const QNetworkAddressEntry & addr : std::as_const(addresses))
        {
            if (addr.ip().protocol() == QAbstractSocket::IPv4Protocol ||
                addr.ip().protocol() == QAbstractSocket::IPv6Protocol)
            {
                AddLogLine("        " + tr("IP Address") + ": " + addr.ip().toString());
            }
        }
    }
    AddLogLine(line, machineStr);

    // uptime
    std::chrono::seconds uptime = 0s;
    if (getUptime(uptime))
    {
        auto UpdateUptime = [](StatusBoxItem* Item)
        {
            std::chrono::seconds time = 0s;
            if (getUptime(time))
                Item->SetText(uptimeStr(time));
        };
        StatusBoxItem *uptimeitem = AddLogLine(uptimeStr(uptime));
        connect(uptimeitem, &StatusBoxItem::UpdateRequired, UpdateUptime);
        uptimeitem->Start(1min);
    }

    // weighted average loads
#if !defined(_WIN32) && !defined(Q_OS_ANDROID)
    auto UpdateLoad = [](StatusBoxItem* Item)
    {
        loadArray loads = getLoadAvgs();
        Item->SetText(QString("   %1: %2 %3 %4").arg(tr("Load")).arg(loads[0], 1, 'f', 2)
                .arg(loads[1], 1, 'f', 2).arg(loads[2], 1, 'f', 2));
    };
    StatusBoxItem* loaditem = AddLogLine("");
    UpdateLoad(loaditem);
    connect(loaditem, &StatusBoxItem::UpdateRequired, UpdateLoad);
    loaditem->Start();
#endif

    // memory usage
    int totalM = 0; // Physical memory
    int freeM = 0;
    int totalS = 0; // Virtual  memory (swap)
    int freeS = 0;
    if (getMemStats(totalM, freeM, totalS, freeS))
    {
        auto UpdateMem = [](StatusBoxItem* Item)
        {
            int totm = 0;
            int freem = 0;
            int tots = 0;
            int frees = 0;
            if (getMemStats(totm, freem, tots, frees))
                Item->SetText(QString("   " + tr("RAM") + ": " + usage_str_mb(totm, totm - freem, freem)));
        };

        auto UpdateSwap = [](StatusBoxItem* Item)
        {
            int totm = 0;
            int freem = 0;
            int tots = 0;
            int frees = 0;
            if (getMemStats(totm, freem, tots, frees))
                Item->SetText(QString("   " + tr("Swap") + ": " + usage_str_mb(tots, tots - frees, frees)));
        };
        StatusBoxItem* mem  = AddLogLine("", machineStr);
        StatusBoxItem* swap = AddLogLine("", machineStr);
        UpdateMem(mem);
        UpdateSwap(swap);
        connect(mem,  &StatusBoxItem::UpdateRequired, UpdateMem);
        connect(swap, &StatusBoxItem::UpdateRequired, UpdateSwap);
        mem->Start(3s);
        swap->Start(3s);
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
        if (RemoteGetUptime(uptime))
        {
            auto UpdateRemoteUptime = [](StatusBoxItem* Item)
            {
                std::chrono::seconds time = 0s;
                RemoteGetUptime(time);
                Item->SetText(uptimeStr(time));
            };
            StatusBoxItem *remoteuptime = AddLogLine(uptimeStr(uptime));
            connect(remoteuptime, &StatusBoxItem::UpdateRequired, UpdateRemoteUptime);
            remoteuptime->Start(1min);
        }

        // weighted average loads
        system_load_array floads;
        if (RemoteGetLoad(floads))
        {
            auto UpdateRemoteLoad = [](StatusBoxItem* Item)
            {
                system_load_array loads = { 0.0, 0.0, 0.0 };
                RemoteGetLoad(loads);
                Item->SetText(QString("   %1: %2 %3 %4").arg(tr("Load")).arg(loads[0], 1, 'f', 2)
                        .arg(loads[1], 1, 'f', 2).arg(loads[2], 1, 'f', 2));
            };
            StatusBoxItem* remoteloaditem = AddLogLine("");
            UpdateRemoteLoad(remoteloaditem);
            connect(remoteloaditem, &StatusBoxItem::UpdateRequired, UpdateRemoteLoad);
            remoteloaditem->Start();
        }

        // memory usage
        if (RemoteGetMemStats(totalM, freeM, totalS, freeS))
        {
            auto UpdateRemoteMem = [](StatusBoxItem* Item)
            {
                int totm = 0;
                int freem = 0;
                int tots = 0;
                int frees = 0;
                if (RemoteGetMemStats(totm, freem, tots, frees))
                    Item->SetText(QString("   " + tr("RAM") + ": " + usage_str_mb(totm, totm - freem, freem)));
            };

            auto UpdateRemoteSwap = [](StatusBoxItem* Item)
            {
                int totm = 0;
                int freem = 0;
                int tots = 0;
                int frees = 0;
                if (RemoteGetMemStats(totm, freem, tots, frees))
                    Item->SetText(QString("   " + tr("Swap") + ": " + usage_str_mb(tots, tots - frees, frees)));
            };
            StatusBoxItem* rmem  = AddLogLine("", machineStr);
            StatusBoxItem* rswap = AddLogLine("", machineStr);
            UpdateRemoteMem(rmem);
            UpdateRemoteSwap(rswap);
            connect(rmem,  &StatusBoxItem::UpdateRequired, UpdateRemoteMem);
            connect(rswap, &StatusBoxItem::UpdateRequired, UpdateRemoteSwap);
            rmem->Start(10s);
            rswap->Start(11s);
        }
    }

    // get free disk space
    QString hostnames;

    FileSystemInfoList fsInfos = FileSystemInfoManager::GetInfoList();
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
            m_recordingProfilesBps);

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

        for (auto & diskinfo : list)
        {
            line = QString("   ") + diskinfo;
            AddLogLine(line, machineStr);
        }
    }

}

void StatusBox::doDecoderStatus()
{
    if (m_iconState)
        m_iconState->DisplayState("decoders");
    m_logList->Reset();
    QString displayhelp = tr("Available hardware decoders for video playback.");
    if (m_helpText)
        m_helpText->SetText(displayhelp);
    if (m_justHelpText)
        m_justHelpText->SetText(displayhelp);

    QStringList decoders = MythCodecContext::GetDecoderDescription();
    if (decoders.isEmpty())
    {
        AddLogLine(tr("None"));
    }
    else
    {
        for (const QString & decoder : std::as_const(decoders))
            AddLogLine(decoder);
    }
}

void StatusBox::doDisplayStatus()
{
    if (m_iconState)
        m_iconState->DisplayState("display");
    m_logList->Reset();
    auto displayhelp = tr("Display information.");
    if (m_helpText)
        m_helpText->SetText(displayhelp);
    if (m_justHelpText)
        m_justHelpText->SetText(displayhelp);

    auto desc = GetMythMainWindow()->GetDisplay()->GetDescription();
    for (const auto & line : std::as_const(desc))
        AddLogLine(line);
}

void StatusBox::doRenderStatus()
{
    if (m_iconState)
        m_iconState->DisplayState("render");
    m_logList->Reset();
    auto displayhelp = tr("Render information.");
    if (m_helpText)
        m_helpText->SetText(displayhelp);
    if (m_justHelpText)
        m_justHelpText->SetText(displayhelp);

    auto * render = GetMythMainWindow()->GetRenderDevice();
    if (render && render->Type() == kRenderOpenGL)
    {
        auto * opengl = dynamic_cast<MythRenderOpenGL*>(render);

        if (opengl)
        {
            auto UpdateFPS = [](StatusBoxItem* Item)
            {
                uint64_t swapcount = 0;
                auto * rend = GetMythMainWindow()->GetRenderDevice();
                if (auto * gl = dynamic_cast<MythRenderOpenGL*>(rend); gl != nullptr)
                    swapcount = gl->GetSwapCount();
                Item->SetText(tr("Current fps\t: %1").arg(swapcount));
            };

            auto * fps = AddLogLine("");
            // Reset the frame counter
            (void)opengl->GetSwapCount();
            UpdateFPS(fps);
            connect(fps, &StatusBoxItem::UpdateRequired, UpdateFPS);
            fps->Start();
        }

        if (opengl && (opengl->GetExtraFeatures() & kGLNVMemory))
        {
            auto GetGPUMem = []()
            {
                auto * rend = GetMythMainWindow()->GetRenderDevice();
                if (auto * gl = dynamic_cast<MythRenderOpenGL*>(rend); gl != nullptr)
                    return gl->GetGPUMemory();
                return std::tuple<int,int,int> { 0, 0, 0 };
            };
            auto UpdateUsed = [&GetGPUMem](StatusBoxItem* Item)
            {
                auto mem = GetGPUMem();
                int total = std::get<0>(mem);
                if (total > 0)
                {
                    int avail = std::get<2>(mem);
                    Item->SetText(tr("GPU memory used     : %1MB").arg(total - avail));
                }
            };

            auto UpdateFree = [&GetGPUMem](StatusBoxItem* Item)
            {
                auto mem = GetGPUMem();
                int total = std::get<0>(mem);
                if (total > 0)
                {
                    int avail = std::get<2>(mem);
                    int percent = static_cast<int>((avail / static_cast<float>(total) * 100.0F));
                    Item->SetText(tr("GPU memory free     : %1MB (or %2%)").arg(avail).arg(percent));
                }
            };

            auto current = GetGPUMem();
            // Total and dedicated will not change
            AddLogLine(tr("GPU memory total    : %1MB").arg(std::get<0>(current)));
            AddLogLine(tr("GPU memory dedicated: %1MB").arg(std::get<1>(current)));
            auto * used = AddLogLine("");
            auto * freemem = AddLogLine("");
            UpdateUsed(used);
            UpdateFree(freemem);
            connect(used, &StatusBoxItem::UpdateRequired, UpdateUsed);
            connect(freemem, &StatusBoxItem::UpdateRequired, UpdateFree);
            used->Start();
            freemem->Start();
        }

        auto desc = render->GetDescription();
        for (const auto & line : std::as_const(desc))
            AddLogLine(line);
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

    std::vector<ProgramInfo *>::iterator it;

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
                     m_expList.size()).arg(StringUtil::formatKBytes(totalSize / 1024));

    if (liveTVCount)
        staticInfo += tr("%n (is) LiveTV and consume(s) %1\n", "", liveTVCount)
                            .arg(StringUtil::formatKBytes(liveTVSize / 1024));

    if (deletedGroupCount)
    {
        staticInfo += tr("%n (is) Deleted and consume(s) %1\n", "",
                        deletedGroupCount)
                        .arg(StringUtil::formatKBytes(deletedGroupSize / 1024));
    }

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
            " (" + StringUtil::formatKBytes(pginfo->GetFilesize() / 1024) + ")";

        detailInfo =
            MythDate::toString(
                starttime, MythDate::kDateTimeFull | MythDate::kSimplify) +
            " - " +
            MythDate::toString(
                endtime, MythDate::kDateTimeFull | MythDate::kSimplify);

        detailInfo += " (" + StringUtil::formatKBytes(pginfo->GetFilesize() / 1024) + ")";

        detailInfo += " (" + ProgramInfo::i18n(pginfo->GetRecordingGroup()) + ")";

        detailInfo += "\n" + pginfo->toString(ProgramInfo::kTitleSubtitle, " - ");

        AddLogLine(contentLine, staticInfo, detailInfo,
                   staticInfo + detailInfo);
    }
}

Q_DECLARE_METATYPE(LogLine)

/* vim: set expandtab tabstop=4 shiftwidth=4: */
