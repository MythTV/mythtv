#include <qlayout.h>
#include <qiconview.h>
#include <qsqldatabase.h>
#include <qwidgetstack.h>
#include <qvbox.h>
#include <qgrid.h>
#include <qregexp.h>
#include <qhostaddress.h>

#include <unistd.h>

#include <iostream>
#include <cerrno>
using namespace std;

#include "config.h"
#include "statusbox.h"
#include "mythcontext.h"
#include "remoteutil.h"
#include "programinfo.h"
#include "tv.h"
#include "jobqueue.h"
#include "util.h"
#include "mythdbcon.h"

/** \class StatusBox
 *  \brief Reports on various status items.
 *
 *  StatusBox reports on the listing status, that is how far
 *  into the future program listings exits. It also reports
 *  on the status of each tuner, the log entries, the status
 *  of the job queue, and the machine status.
 */

StatusBox::StatusBox(MythMainWindow *parent, const char *name)
    : MythDialog(parent, name), errored(false)
{
    // Set this value to the number of items in icon_list
    // to prevent scrolling off the bottom
    int item_count = 0;
    dateFormat = gContext->GetSetting("ShortDateFormat", "M/d");
    timeFormat = gContext->GetSetting("TimeFormat", "h:mm AP");
    timeDateFormat = timeFormat + " " + dateFormat;

    setNoErase();
    LoadTheme();
    if (IsErrored())
        return;
  
    icon_list->SetItemText(item_count++, QObject::tr("Listings Status"));
    icon_list->SetItemText(item_count++, QObject::tr("Tuner Status"));
    icon_list->SetItemText(item_count++, QObject::tr("Log Entries"));
    icon_list->SetItemText(item_count++, QObject::tr("Job Queue"));
    icon_list->SetItemText(item_count++, QObject::tr("Machine Status"));
    icon_list->SetItemText(item_count++, QObject::tr("AutoExpire List"));
    icon_list->SetItemCurrent(0);
    icon_list->SetActive(true);

    QStringList strlist;
    strlist << "QUERY_IS_ACTIVE_BACKEND";
    strlist << gContext->GetHostName();

    gContext->SendReceiveStringList(strlist);

    if (QString(strlist[0]) == "FALSE")
        isBackend = false;
    else if (QString(strlist[0]) == "TRUE")
        isBackend = true;
    else
        isBackend = false;

    VERBOSE(VB_NETWORK, QString("QUERY_IS_ACTIVE_BACKEND=%1").arg(strlist[0]));

    max_icons = item_count;
    inContent = false;
    contentPos = 0;
    contentTotalLines = 0;
    contentSize = 0;
    contentMid = 0;
    min_level = gContext->GetNumSetting("LogDefaultView",1);
    my_parent = parent;
    clicked();

    gContext->addCurrentLocation("StatusBox");
}

StatusBox::~StatusBox(void)
{
    gContext->removeCurrentLocation();
}

void StatusBox::paintEvent(QPaintEvent *e)
{
    QRect r = e->rect();

    if (r.intersects(TopRect))
        updateTopBar();
    if (r.intersects(SelectRect))
        updateSelector();
    if (r.intersects(ContentRect))
        updateContent();
}

void StatusBox::updateContent()
{
    QRect pr = ContentRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);
    QPainter p(this);

    // Normalize the variables here and set the contentMid
    contentSize = list_area->GetItems();
    if (contentSize > contentTotalLines)
        contentSize = contentTotalLines;
    contentMid = contentSize / 2;

    int startPos = 0;
    int highlightPos = 0;

    if (contentPos < contentMid)
    {
        startPos = 0;
        highlightPos = contentPos;
    }
    else if (contentPos >= (contentTotalLines - contentMid))
    {
        startPos = contentTotalLines - contentSize;
        highlightPos = contentSize - (contentTotalLines - contentPos);
    }
    else if (contentPos >= contentMid)
    {
        startPos = contentPos - contentMid;
        highlightPos = contentMid;
    }
 
    if (content  == NULL) return;
    LayerSet *container = content;

    list_area->ResetList();
    for (int x = startPos; (x - startPos) <= contentSize; x++)
    {
        if (contentLines.contains(x))
        {
            list_area->SetItemText(x - startPos, contentLines[x]);
            if (contentFont.contains(x))
                list_area->EnableForcedFont(x - startPos, contentFont[x]);
        }
    }

    list_area->SetItemCurrent(highlightPos);

    if (inContent)
    {
        helptext->SetText(contentDetail[contentPos]);
        update(TopRect);
    }

    list_area->SetUpArrow((startPos > 0) && (contentSize < contentTotalLines));
    list_area->SetDownArrow((startPos + contentSize) < contentTotalLines);

    container->Draw(&tmp, 0, 0);
    container->Draw(&tmp, 1, 0);
    container->Draw(&tmp, 2, 0);
    container->Draw(&tmp, 3, 0);
    container->Draw(&tmp, 4, 0);
    container->Draw(&tmp, 5, 0);
    container->Draw(&tmp, 6, 0);
    container->Draw(&tmp, 7, 0);
    container->Draw(&tmp, 8, 0);
    tmp.end();
    p.drawPixmap(pr.topLeft(), pix);
}

void StatusBox::updateSelector()
{
    QRect pr = SelectRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);
    QPainter p(this);
 
    if (selector == NULL) return;
    LayerSet *container = selector;

    container->Draw(&tmp, 0, 0);
    container->Draw(&tmp, 1, 0);
    container->Draw(&tmp, 2, 0);
    container->Draw(&tmp, 3, 0);
    container->Draw(&tmp, 4, 0);
    container->Draw(&tmp, 5, 0);
    container->Draw(&tmp, 6, 0);
    container->Draw(&tmp, 7, 0);
    container->Draw(&tmp, 8, 0);
    tmp.end();
    p.drawPixmap(pr.topLeft(), pix);
}

void StatusBox::updateTopBar()
{
    QRect pr = TopRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);
    QPainter p(this);

    if (topbar == NULL) return;
    LayerSet *container = topbar;

    container->Draw(&tmp, 0, 0);
    tmp.end();
    p.drawPixmap(pr.topLeft(), pix);
}

void StatusBox::LoadTheme()
{
    int screenheight = 0, screenwidth = 0;
    float wmult = 0, hmult = 0;

    gContext->GetScreenSettings(screenwidth, wmult, screenheight, hmult);

    theme = new XMLParse();
    theme->SetWMult(wmult);
    theme->SetHMult(hmult);
    if (!theme->LoadTheme(xmldata, "status", "status-"))
    {
        VERBOSE(VB_IMPORTANT, "StatusBox: Unable to load theme.");
        errored = true;
        return;
    }

    for (QDomNode child = xmldata.firstChild(); !child.isNull();
         child = child.nextSibling()) {

        QDomElement e = child.toElement();
        if (!e.isNull()) {
            if (e.tagName() == "font") {
                theme->parseFont(e);
            }
            else if (e.tagName() == "container") {
                QRect area;
                QString name;
                int context;
                theme->parseContainer(e, name, context, area);

                if (name.lower() == "topbar")
                    TopRect = area;
                if (name.lower() == "selector")
                    SelectRect = area;
                if (name.lower() == "content")
                    ContentRect = area;
            }
            else {
                QString msg =
                    QString(tr("The theme you are using contains an "
                               "unknown element ('%1').  It will be ignored"))
                    .arg(e.tagName());
                VERBOSE(VB_IMPORTANT, msg);
                errored = true;
            }
        }
    }

    selector = theme->GetSet("selector");
    if (!selector)
    {
        VERBOSE(VB_IMPORTANT, "StatusBox: Failed to get selector container.");
        errored = true;
    }

    icon_list = (UIListType*)selector->GetType("icon_list");
    if (!icon_list)
    {
        VERBOSE(VB_IMPORTANT, "StatusBox: Failed to get icon list area.");
        errored = true;
    }

    content = theme->GetSet("content");
    if (!content)
    {
        VERBOSE(VB_IMPORTANT, "StatusBox: Failed to get content container.");
        errored = true;
    }

    list_area = (UIListType*)content->GetType("list_area");
    if (!list_area)
    {
        VERBOSE(VB_IMPORTANT, "StatusBox: Failed to get list area.");
        errored = true;
    }

    topbar = theme->GetSet("topbar");
    if (!topbar)
    {
        VERBOSE(VB_IMPORTANT, "StatusBox: Failed to get topbar container.");
        errored = true;
    }

    heading = (UITextType*)topbar->GetType("heading");
    if (!heading)
    {
        VERBOSE(VB_IMPORTANT, "StatusBox: Failed to get heading area.");
        errored = true;
    }

    helptext = (UITextType*)topbar->GetType("helptext");
    if (!helptext)
    {
        VERBOSE(VB_IMPORTANT, "StatusBox: Failed to get helptext area.");
        errored = true;
    }
}

void StatusBox::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Status", e, actions);

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        QString currentItem;
        QRegExp logNumberKeys( "^[12345678]$" );

        currentItem = icon_list->GetItemText(icon_list->GetCurrentItem());
        handled = true;

        if (action == "SELECT")
        {
            clicked();
        }
        else if (action == "MENU")
        {
            if ((inContent) &&
                (currentItem == QObject::tr("Log Entries")))
            {
                int retval = MythPopupBox::show2ButtonPopup(my_parent,
                                 QString("AckLogEntry"),
                                 QObject::tr("Acknowledge all log entries at "
                                             "this priority level or lower?"),
                                 QObject::tr("Yes"), QObject::tr("No"), 0);
                if (retval == 0)
                {
                    MSqlQuery query(MSqlQuery::InitCon());
                    query.prepare("UPDATE mythlog SET acknowledged = 1 "
                                  "WHERE priority <= :PRIORITY ;");
                    query.bindValue(":PRIORITY", min_level);
                    query.exec();
                    doLogEntries();
                }
            }
            else if ((inContent) &&
                     (currentItem == QObject::tr("Job Queue")))
            {
                clicked();
            }
        }
        else if (action == "UP")
        {
            if (inContent)
            {
                if (contentPos > 0)
                    contentPos--;
                update(ContentRect);
            }
            else
            {
                if (icon_list->GetCurrentItem() > 0)
                    icon_list->SetItemCurrent(icon_list->GetCurrentItem()-1);
                clicked();
                setHelpText();
                update(SelectRect);
            }

        }
        else if (action == "DOWN")
        {
            if (inContent)
            {
                if (contentPos < (contentTotalLines - 1))
                    contentPos++;
                update(ContentRect);
            }
            else
            {
                if (icon_list->GetCurrentItem() < (max_icons - 1))
                    icon_list->SetItemCurrent(icon_list->GetCurrentItem()+1);
                clicked();
                setHelpText();
                update(SelectRect);
            }
        }
        else if (action == "PAGEUP" && inContent) 
        {
            contentPos -= contentSize;
            if (contentPos < 0)
                contentPos = 0;
            update(ContentRect);
        }
        else if (action == "PAGEDOWN" && inContent)
        {
            contentPos += contentSize;
            if (contentPos > (contentTotalLines - 1))
                contentPos = contentTotalLines - 1;
            update(ContentRect);
        }
        else if ((action == "RIGHT") &&
                 (!inContent) &&
                 ((contentTotalLines > contentSize) ||
                  (doScroll)))
        {
            clicked();
            inContent = true;
            contentPos = 0;
            icon_list->SetActive(false);
            list_area->SetActive(true);
            update(SelectRect);
            update(ContentRect);
        }
        else if (action == "LEFT")
        {
            if (inContent)
            {
                inContent = false;
                contentPos = 0;
                list_area->SetActive(false);
                icon_list->SetActive(true);
                setHelpText();
                update(SelectRect);
                update(ContentRect);
            }
            else
            {
                if (gContext->GetNumSetting("UseArrowAccels", 1))
                    accept();
            }
        }
        else if ((currentItem == QObject::tr("Log Entries")) &&
                 (logNumberKeys.search(action) == 0))
        {
            min_level = action.toInt();
            helptext->SetText(QObject::tr("Setting priority level to %1")
                                          .arg(min_level));
            update(TopRect);
            doLogEntries();
        }
        else
            handled = false;
    }

    if (!handled)
        MythDialog::keyPressEvent(e);
}

void StatusBox::setHelpText()
{
    if (inContent)
    {
        helptext->SetText(contentDetail[contentPos]);
    } else {
        topbar->ClearAllText();
        QString currentItem;

        currentItem = icon_list->GetItemText(icon_list->GetCurrentItem());

        if (currentItem == QObject::tr("Listings Status"))
            helptext->SetText(QObject::tr("Listings Status shows the latest "
                                          "status information from "
                                          "mythfilldatabase"));

        if (currentItem == QObject::tr("Tuner Status"))
            helptext->SetText(QObject::tr("Tuner Status shows the current "
                                          "information about the state of "
                                          "backend tuner cards"));

        if (currentItem == QObject::tr("DVB Status"))
            helptext->SetText(QObject::tr("DVB Status shows the quality "
                                          "statistics of all DVB cards, if "
                                          "present"));

        if (currentItem == QObject::tr("Log Entries"))
            helptext->SetText(QObject::tr("Log Entries shows any unread log "
                                          "entries from the system if you "
                                          "have logging enabled"));
        if (currentItem == QObject::tr("Job Queue"))
            helptext->SetText(QObject::tr("Job Queue shows any jobs currently "
                                          "in Myth's Job Queue such as a "
                                          "commercial flagging job."));
        if (currentItem == QObject::tr("Machine Status"))
        {
            QString machineStr = QObject::tr("Machine Status shows "
                                             "some operating system "
                                             "statistics of this machine");
            if (!isBackend)
                machineStr.append(" " + QObject::tr("and the MythTV server"));

            helptext->SetText(machineStr);
        }

        if (currentItem == QObject::tr("AutoExpire List"))
            helptext->SetText(QObject::tr("The AutoExpire List shows all "
                "recordings which may be expired and the order of their "
                "expiration. Recordings at the top of the list will be "
                "expired first."));
    }
    update(TopRect);
}

void StatusBox::clicked()
{
    QString currentItem = icon_list->GetItemText(icon_list->GetCurrentItem());

    if (inContent)
    {
        if (currentItem == QObject::tr("Log Entries"))
        {
            int retval;

            retval = MythPopupBox::show2ButtonPopup(my_parent,
                                   QString("AckLogEntry"),
                                   QObject::tr("Acknowledge this log entry?"),
                                   QObject::tr("Yes"), QObject::tr("No"), 0);
            if (retval == 0)
            {
                MSqlQuery query(MSqlQuery::InitCon());
                query.prepare("UPDATE mythlog SET acknowledged = 1 "
                              "WHERE logid = :LOGID ;");
                query.bindValue(":LOGID", contentData[contentPos]);
                query.exec();
                doLogEntries();
            }
        }
        else if (currentItem == QObject::tr("Job Queue"))
        {
            QStringList msgs;
            int jobStatus;
            int retval;

            jobStatus = JobQueue::GetJobStatus(
                                contentData[contentPos].toInt());

            if (jobStatus == JOB_QUEUED)
            {
                retval = MythPopupBox::show2ButtonPopup(my_parent,
                                       QString("JobQueuePopup"),
                                       QObject::tr("Delete Job?"),
                                       QObject::tr("Yes"),
                                       QObject::tr("No"), 1);
                if (retval == 0)
                {
                    JobQueue::DeleteJob(contentData[contentPos].toInt());
                    doJobQueueStatus();
                }
            }
            else if ((jobStatus == JOB_PENDING) ||
                     (jobStatus == JOB_STARTING) ||
                     (jobStatus == JOB_RUNNING))
            {
                msgs << QObject::tr("Pause");
                msgs << QObject::tr("Stop");
                msgs << QObject::tr("No Change");
                retval = MythPopupBox::showButtonPopup(my_parent,
                                       QString("JobQueuePopup"),
                                       QObject::tr("Job Queue Actions:"),
                                       msgs, 2);
                if (retval == 0)
                {
                    JobQueue::PauseJob(contentData[contentPos].toInt());
                    doJobQueueStatus();
                }
                else if (retval == 1)
                {
                    JobQueue::StopJob(contentData[contentPos].toInt());
                    doJobQueueStatus();
                }
            }
            else if (jobStatus == JOB_PAUSED)
            {
                msgs << QObject::tr("Resume");
                msgs << QObject::tr("Stop");
                msgs << QObject::tr("No Change");
                retval = MythPopupBox::showButtonPopup(my_parent,
                                       QString("JobQueuePopup"),
                                       QObject::tr("Job Queue Actions:"),
                                       msgs, 2);
                if (retval == 0)
                {
                    JobQueue::ResumeJob(contentData[contentPos].toInt());
                    doJobQueueStatus();
                }
                else if (retval == 1)
                {
                    JobQueue::StopJob(contentData[contentPos].toInt());
                    doJobQueueStatus();
                }
            }
            else if (jobStatus & JOB_DONE)
            {
                retval = MythPopupBox::show2ButtonPopup(my_parent,
                                       QString("JobQueuePopup"),
                                       QObject::tr("Requeue Job?"),
                                       QObject::tr("Yes"),
                                       QObject::tr("No"), 1);
                if (retval == 0)
                {
                    JobQueue::ChangeJobStatus(contentData[contentPos].toInt(),
                                              JOB_QUEUED);
                    doJobQueueStatus();
                }
            }
        }

        return;
    }
    
    // Clear all visible content elements here
    // I'm sure there's a better way to do this but I can't find it
    content->ClearAllText();
    list_area->ResetList();
    contentLines.clear();
    contentDetail.clear();
    contentFont.clear();
    contentData.clear();

    if (currentItem == QObject::tr("Listings Status"))
        doListingsStatus();
    else if (currentItem == QObject::tr("Tuner Status"))
        doTunerStatus();
    else if (currentItem == QObject::tr("Log Entries"))
        doLogEntries();
    else if (currentItem == QObject::tr("Job Queue"))
        doJobQueueStatus();
    else if (currentItem == QObject::tr("Machine Status"))
        doMachineStatus();
    else if (currentItem == QObject::tr("AutoExpire List"))
        doAutoExpireList();
}

void StatusBox::doListingsStatus()
{
    QString mfdLastRunStart, mfdLastRunEnd, mfdLastRunStatus, mfdNextRunStart;
    QString querytext, Status, DataDirectMessage;
    int DaysOfData;
    QDateTime qdtNow, GuideDataThrough;
    int count = 0;

    contentLines.clear();
    contentDetail.clear();
    contentFont.clear();
    doScroll = false;

    qdtNow = QDateTime::currentDateTime();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT max(endtime) FROM program WHERE manualid=0;");
    query.exec();

    if (query.isActive() && query.size())
    {
        query.next();
        GuideDataThrough = QDateTime::fromString(query.value(0).toString(),
                                                 Qt::ISODate);
    }

    mfdLastRunStart = gContext->GetSetting("mythfilldatabaseLastRunStart");
    mfdLastRunEnd = gContext->GetSetting("mythfilldatabaseLastRunEnd");
    mfdLastRunStatus = gContext->GetSetting("mythfilldatabaseLastRunStatus");
    mfdNextRunStart = gContext->GetSetting("MythFillSuggestedRunTime");
    DataDirectMessage = gContext->GetSetting("DataDirectMessage");

    mfdNextRunStart.replace("T", " ");

    contentLines[count++] = QObject::tr("Myth version:") + " " +
                                        MYTH_BINARY_VERSION;
    contentLines[count++] = QObject::tr("Last mythfilldatabase guide update:");
    contentLines[count++] = QObject::tr("Started:   ") + mfdLastRunStart;

    if (mfdLastRunEnd >= mfdLastRunStart) //if end < start, it's still running.
        contentLines[count++] = QObject::tr("Finished: ") + mfdLastRunEnd;

    contentLines[count++] = QObject::tr("Result: ") + mfdLastRunStatus;


    if (mfdNextRunStart >= mfdLastRunStart) 
        contentLines[count++] = QObject::tr("Suggested Next: ") + 
                                mfdNextRunStart;

    DaysOfData = qdtNow.daysTo(GuideDataThrough);

    if (GuideDataThrough.isNull())
    {
        contentLines[count++] = "";
        contentLines[count++] = QObject::tr("There's no guide data available!");
        contentLines[count++] = QObject::tr("Have you run mythfilldatabase?");
    }
    else
    {
        contentLines[count++] = QObject::tr("There is guide data until ") + 
                                QDateTime(GuideDataThrough)
                                          .toString("yyyy-MM-dd hh:mm");

        if (DaysOfData > 0)
        {
            Status = QString("(%1 ").arg(DaysOfData);
            if (DaysOfData >1)
                Status += QObject::tr("days");
            else
                Status += QObject::tr("day");
            Status += ").";
            contentLines[count++] = Status;
        }
    }

    if (DaysOfData <= 3)
    {
        contentLines[count++] = QObject::tr("WARNING: is mythfilldatabase "
                                            "running?");
    }

    if (!DataDirectMessage.isNull())
    {
        contentLines[count++] = QObject::tr("DataDirect Status: "); 
        contentLines[count++] = DataDirectMessage;
    }
   
    contentTotalLines = count;
    update(ContentRect);
}

void StatusBox::doTunerStatus()
{
    int count = 0;
    doScroll = true;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT cardid FROM capturecard;");
    query.exec();

    contentLines.clear();
    contentDetail.clear();
    contentFont.clear();

    if (query.isActive() && query.size())
    {
        while(query.next())
        {
            int cardid = query.value(0).toInt();

            QString cmd = QString("QUERY_REMOTEENCODER %1").arg(cardid);
            QStringList strlist = cmd;
            strlist << "GET_STATE";

            gContext->SendReceiveStringList(strlist);
            int state = strlist[0].toInt();
  
            QString Status = QString(tr("Tuner %1 ")).arg(cardid);
            if (state==kState_Error)
                Status += tr("is not available");
            else if (state==kState_WatchingLiveTV)
                Status += tr("is watching live TV");
            else if (state==kState_RecordingOnly ||
                     state==kState_WatchingRecording)
                Status += tr("is recording");
            else 
                Status += tr("is not recording");

            contentLines[count] = Status;
            contentDetail[count] = Status;

            if (state==kState_RecordingOnly ||
                state==kState_WatchingRecording)
            {
                strlist = QString("QUERY_RECORDER %1").arg(cardid);
                strlist << "GET_RECORDING";
                gContext->SendReceiveStringList(strlist);
                ProgramInfo *proginfo = new ProgramInfo;
                proginfo->FromStringList(strlist, 0);
   
                Status += " " + proginfo->title;
                Status += "\n";
                Status += proginfo->subtitle;
                contentDetail[count] = Status;
            }
            count++;
        }
    }
    contentTotalLines = count;
    update(ContentRect);
}

void StatusBox::doLogEntries(void)
{
    QString line;
    int count = 0;
 
    doScroll = true;

    contentLines.clear();
    contentDetail.clear();
    contentFont.clear();
    contentData.clear();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT logid, module, priority, logdate, host, "
                  "message, details "
                  "FROM mythlog WHERE acknowledged = 0 "
                  "AND priority <= :PRIORITY ORDER BY logdate DESC;");
    query.bindValue(":PRIORITY", min_level);
    query.exec();

    if (query.isActive())
    {
        while (query.next())
        {
            line = QString("%1").arg(query.value(5).toString());
            contentLines[count] = line;

            if (query.value(6).toString() != "")
                line = tr("On %1 %2 from %3.%4\n%5\n%6")
                               .arg(query.value(3).toDateTime()
                                         .toString(dateFormat))
                               .arg(query.value(3).toDateTime()
                                         .toString(timeFormat))
                               .arg(query.value(4).toString())
                               .arg(query.value(1).toString())
                               .arg(query.value(5).toString())
                               .arg(QString::fromUtf8(query.value(6).toString()));
            else
                line = tr("On %1 %2 from %3.%4\n%5\nNo other details")
                               .arg(query.value(3).toDateTime()
                                         .toString(dateFormat))
                               .arg(query.value(3).toDateTime()
                                         .toString(timeFormat))
                               .arg(query.value(4).toString())
                               .arg(query.value(1).toString())
                               .arg(query.value(5).toString());
            contentDetail[count] = line;
            contentData[count++] = query.value(0).toString();
        }
    }

    if (!count)
    {
        doScroll = false;
        contentLines[count++] = QObject::tr("No items found at priority "
                                            "level %1 or lower.")
                                            .arg(min_level);
        contentLines[count++] = QObject::tr("Use 1-8 to change priority "
                                            "level.");
    }
      
    contentTotalLines = count;
    update(ContentRect);
}

void StatusBox::doJobQueueStatus()
{
    QMap<int, JobQueueEntry> jobs;
    QMap<int, JobQueueEntry>::Iterator it;
    int count = 0;

    QString detail;

    JobQueue::GetJobsInQueue(jobs,
                             JOB_LIST_NOT_DONE | JOB_LIST_ERROR |
                             JOB_LIST_RECENT);

    doScroll = true;

    contentLines.clear();
    contentDetail.clear();
    contentFont.clear();
    contentData.clear();

    if (jobs.size())
    {
        for (it = jobs.begin(); it != jobs.end(); ++it)
        {
            QString chanid = it.data().chanid;
            QDateTime starttime = it.data().starttime;
            ProgramInfo *pginfo;

            pginfo = ProgramInfo::GetProgramFromRecorded(chanid, starttime);

            if (!pginfo)
                continue;

            detail = pginfo->title + "\n" +
                     pginfo->channame + " " + pginfo->chanstr +
                         " @ " + starttime.toString(timeDateFormat) + "\n" +
                     tr("Job:") + " " + JobQueue::JobText(it.data().type) +
                     "     " + tr("Status: ") +
                     JobQueue::StatusText(it.data().status);

            if (it.data().status != JOB_QUEUED)
                detail += " (" + it.data().hostname + ")";

            detail += "\n" + it.data().comment;

            contentLines[count] = pginfo->title + " @ " +
                                  starttime.toString(timeDateFormat);

            contentDetail[count] = detail;
            contentData[count] = QString("%1").arg(it.data().id);

            if (it.data().status == JOB_ERRORED)
                contentFont[count] = "error";
            else if (it.data().status == JOB_ABORTED)
                contentFont[count] = "warning";

            count++;

            delete pginfo;
        }
    }
    else
    {
        contentLines[count++] = QObject::tr("Job Queue is currently empty.");
        doScroll = false;
    }

    contentTotalLines = count;
    update(ContentRect);
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
    const QString tail = QObject::tr(", using your %1 rate of %2 Kb/sec");

    out<<usage_str_kb(total, used, free);
    if (free<0)
        return;

    recprof2bps_t::const_iterator it = prof2bps.begin();
    for (; it != prof2bps.end(); ++it)
    {
        const QString pro =
                tail.arg(it.key()).arg((int)((float)it.data() / 1024.0));
        
        long long bytesPerMin = (it.data() >> 1) * 15;
        uint minLeft = ((free<<5)/bytesPerMin)<<5;
        minLeft = (minLeft/15)*15;
        uint hoursLeft = minLeft/60;
        if (hoursLeft > 3)
            out<<QObject::tr("%1 hours left").arg(hoursLeft) + pro;
        else if (minLeft > 90)
            out<<QObject::tr("%1 hours and %2 minutes left")
                .arg(hoursLeft).arg(minLeft%60) + pro;
        else
            out<<QObject::tr("%1 minutes left").arg(minLeft) + pro;
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
        QString dayLabel;

        if (days == 1)
            dayLabel = QObject::tr("day");
        else
            dayLabel = QObject::tr("days");

        sprintf(buff, "%d:%02d", hours, min);

        return str + QString("%1 %2, %3").arg(days).arg(dayLabel).arg(buff);
    }
    else
    {
        char  buff[9];

        sprintf(buff, "%d:%02d:%02d", hours, min, secs);

        return str + buff;
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

    if (query.exec() && query.isActive() && query.size() > 0 && query.next() &&
        query.value(0).toDouble() > 0)
    {
        recordingProfilesBPS[QObject::tr("average")] =
            (int)(query.value(0).toDouble());
    }

    querystr =
        "SELECT max(filesize * 8 / "
            "(unix_timestamp(endtime) - unix_timestamp(starttime))) "
            "AS max_bitrate "
        "FROM recorded WHERE hostname in (%1) "
            "AND (unix_timestamp(endtime) - unix_timestamp(starttime)) > 300;";

    query.prepare(querystr.arg(hostnames));

    if (query.exec() && query.isActive() && query.size() > 0 && query.next() &&
        query.value(0).toDouble() > 0)
    {
        recordingProfilesBPS[QObject::tr("maximum")] =
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
    int           count(0);
    int           totalM, usedM, freeM;    // Physical memory
    int           totalS, usedS, freeS;    // Virtual  memory (swap)
    time_t        uptime;
    int           detailBegin;
    QString       detailString;
    int           detailLoop;

    contentLines.clear();
    contentDetail.clear();
    contentFont.clear();
    doScroll = true;

    detailBegin = count;
    detailString = "";

    if (isBackend)
        contentLines[count] = QObject::tr("System") + ":";
    else
        contentLines[count] = QObject::tr("This machine") + ":";
    detailString += contentLines[count] + "\n";
    count++;

    // uptime
    if (!getUptime(uptime))
        uptime = 0;
    contentLines[count] = uptimeStr(uptime);

    // weighted average loads
    contentLines[count].append(".   " + QObject::tr("Load") + ": ");

    double loads[3];
    if (getloadavg(loads,3) == -1)
        contentLines[count].append(QObject::tr("unknown") +
                                   " - getloadavg() " + QObject::tr("failed"));
    else
    {
        char buff[30];

        sprintf(buff, "%0.2lf, %0.2lf, %0.2lf", loads[0], loads[1], loads[2]);
        contentLines[count].append(QString(buff));
    }
    detailString += contentLines[count] + "\n";
    count++;


    // memory usage
    if (getMemStats(totalM, freeM, totalS, freeS))
    {
        usedM = totalM - freeM;
        if (totalM > 0)
        {
            contentLines[count] = "   " + QObject::tr("RAM") +
                                  ": "  + usage_str_mb(totalM, usedM, freeM);
            detailString += contentLines[count] + "\n";
            count++;
        }
        usedS = totalS - freeS;
        if (totalS > 0)
        {
            contentLines[count] = "   " + QObject::tr("Swap") +
                                  ": "  + usage_str_mb(totalS, usedS, freeS);
            detailString += contentLines[count] + "\n";
            count++;
        }
    }

    for (detailLoop = detailBegin; detailLoop < count; detailLoop++)
        contentDetail[detailLoop] = detailString;

    detailBegin = count;
    detailString = "";

    if (!isBackend)
    {
        contentLines[count] = QObject::tr("MythTV server") + ":";
        detailString += contentLines[count] + "\n";
        count++;

        // uptime
        if (!RemoteGetUptime(uptime))
            uptime = 0;
        contentLines[count] = uptimeStr(uptime);

        // weighted average loads
        contentLines[count].append(".   " + QObject::tr("Load") + ": ");
        float loads[3];
        if (RemoteGetLoad(loads))
        {
            char buff[30];

            sprintf(buff, "%0.2f, %0.2f, %0.2f", loads[0], loads[1], loads[2]);
            contentLines[count].append(QString(buff));
        }
        else
            contentLines[count].append(QObject::tr("unknown"));

        detailString += contentLines[count] + "\n";
        count++;

        // memory usage
        if (RemoteGetMemStats(totalM, freeM, totalS, freeS))
        {
            usedM = totalM - freeM;
            if (totalM > 0)
            {
                contentLines[count] = "   " + QObject::tr("RAM") +
                                      ": "  + usage_str_mb(totalM, usedM, freeM);
                detailString += contentLines[count] + "\n";
                count++;
            }

            usedS = totalS - freeS;
            if (totalS > 0)
            {
                contentLines[count] = "   " + QObject::tr("Swap") +
                                      ": "  + usage_str_mb(totalS, usedS, freeS);
                detailString += contentLines[count] + "\n";
                count++;
            }
        }
    }

    for (detailLoop = detailBegin; detailLoop < count; detailLoop++)
        contentDetail[detailLoop] = detailString;

    detailBegin = count;
    detailString = "";

    // get free disk space
    QString hostnames;
    
    vector<FileSystemInfo> fsInfos = RemoteGetFreeSpace();
    for (uint i=0; i<fsInfos.size(); i++)
    {
        hostnames = QString("\"%1\"").arg(fsInfos[i].hostname);
        hostnames.replace(QRegExp(" "), "");
        hostnames.replace(QRegExp(","), "\",\"");

        getActualRecordedBPS(hostnames);

        QStringList list;
        disk_usage_with_rec_time_kb(list,
            fsInfos[i].totalSpaceKB, fsInfos[i].usedSpaceKB,
            fsInfos[i].totalSpaceKB - fsInfos[i].usedSpaceKB,
            recordingProfilesBPS);

        contentLines[count] = 
            QObject::tr("Disk usage on %1:").arg(fsInfos[i].hostname);
        detailString += contentLines[count] + "\n";
        count++;

        QStringList::iterator it = list.begin();
        for (;it != list.end(); ++it)
        {
            contentLines[count] =  QString("   ") + (*it);
            detailString += contentLines[count] + "\n";
            count++;
        }

        for (detailLoop = detailBegin; detailLoop < count; detailLoop++)
            contentDetail[detailLoop] = detailString;

        detailBegin = count;
        detailString = "";
    }

    contentTotalLines = count;
    update(ContentRect);
}

/** \fn StatusBox::doAutoExpireList()
 *  \brief Show list of recordings which may AutoExpire
 */
void StatusBox::doAutoExpireList()
{
    int                   count(0);
    vector<ProgramInfo *> expList;
    ProgramInfo*          pginfo;
    QString               contentLine;
    QString               detailInfo;
    QString               staticInfo;
    long long             totalSize(0);
    long long             liveTVSize(0);
    int                   liveTVCount(0);

    contentLines.clear();
    contentDetail.clear();
    contentFont.clear();
    doScroll = true;

    RemoteGetAllExpiringRecordings(expList);

    vector<ProgramInfo *>::iterator it;
    for (it = expList.begin(); it != expList.end(); it++)
    {
        pginfo = *it;
       
        totalSize += pginfo->filesize;
        if (pginfo->recgroup == "LiveTV")
        {
            liveTVSize += pginfo->filesize;
            liveTVCount++;
        }
    }

    staticInfo = tr("%1 recordings consuming %2 are allowed to expire")
                    .arg(expList.size()).arg(sm_str(totalSize / 1024)) + "\n";

    if (liveTVCount)
        staticInfo += tr("%1 of these are LiveTV and consume %2")
                        .arg(liveTVCount).arg(sm_str(liveTVSize / 1024)) + "\n";
    else
        staticInfo += "\n";

    for (it = expList.begin(); it != expList.end(); it++)
    {
        pginfo = *it;
        contentLine = pginfo->recstartts.toString(dateFormat) + " - " +
                      pginfo->title + " (" + sm_str(pginfo->filesize / 1024) +
                      ")";
        detailInfo = staticInfo + pginfo->title;

        if (pginfo->subtitle != "")
            detailInfo += " - " + pginfo->subtitle + "";

        detailInfo += " (" + sm_str(pginfo->filesize / 1024) + ")\n";

        if (pginfo->recgroup == "LiveTV")
            contentLine += " (" + tr("LiveTV") + ")";

        detailInfo += pginfo->recstartts.toString(timeDateFormat) + " - " +
                      pginfo->recendts.toString(timeDateFormat) + "\n";

        contentLines[count] = contentLine;
        contentDetail[count] = detailInfo;
        count++;
    }

    contentTotalLines = count;
    update(ContentRect);
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
