#include <qlayout.h>
#include <qiconview.h>
#include <qsqldatabase.h>
#include <qwidgetstack.h>
#include <qvbox.h>
#include <qgrid.h>
#include <qregexp.h>
#include <qhostaddress.h>

#include <unistd.h>

#ifdef __linux__
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#endif
#if defined(__FreeBSD__) || defined(CONFIG_DARWIN)
#include <sys/sysctl.h>
#include <sys/param.h>
#include <sys/mount.h>
#endif
#ifdef CONFIG_DARWIN
#include <mach/mach.h>
#endif

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

StatusBox::StatusBox(MythMainWindow *parent, const char *name)
         : MythDialog(parent, name)
{
    // Set this value to the number of items in icon_list
    // to prevent scrolling off the bottom
    int item_count = 0;
    dateFormat = gContext->GetSetting("ShortDateFormat", "M/d");
    timeFormat = gContext->GetSetting("TimeFormat", "h:mm AP");
    timeDateFormat = timeFormat + " " + dateFormat;

    setNoErase();
    LoadTheme();
  
    icon_list->SetItemText(item_count++, QObject::tr("Listings Status"));
    icon_list->SetItemText(item_count++, QObject::tr("Tuner Status"));
    icon_list->SetItemText(item_count++, QObject::tr("Log Entries"));
    icon_list->SetItemText(item_count++, QObject::tr("Job Queue"));
    icon_list->SetItemText(item_count++, QObject::tr("Machine Status"));
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
}

StatusBox::~StatusBox(void)
{
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

    // In order to maintain a center scrolling highlight, we need to determine
    // the point at which to start the content list so that the current item
    // is in the center
    int startPos = 0; 
    if (contentPos >= contentMid)
        startPos = contentPos - contentMid;
 
    if (content  == NULL) return;
    LayerSet *container = content;

    // If the content position is beyond the midpoint and before the end
    // or if the content is first being displayed, write the content using
    // the offset we determined above.  If we are before the midpoint or 
    // close to the end, we stop moving the content up or down to let the 
    // hightlight move up and down with fixed content
    if (((contentPos >= contentMid) &&
         (contentPos < (contentTotalLines - contentMid))) ||
        (contentPos == 0))
    {
        list_area->ResetList();
        for (int x = startPos; (x - startPos) <= contentSize; x++)
            if (contentLines.contains(x))
            {
                list_area->SetItemText(x - startPos, contentLines[x]);
                if (contentFont.contains(x))
                    list_area->EnableForcedFont(x - startPos, contentFont[x]);
            }
    }

    // If we are scrolling, the determine the item to highlight
    if (doScroll)
    {
        int newPos = 0;

        if (contentPos < contentMid)
            newPos = contentPos;
        else if (contentPos >= (contentTotalLines - contentMid))
            newPos = contentSize - (contentTotalLines - contentPos);
        else
            newPos = contentMid;

        list_area->SetItemCurrent(newPos);

        if (inContent)
        {
            helptext->SetText(contentDetail[contentPos]);
            update(TopRect);
        }
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
    theme->LoadTheme(xmldata, "status", "status-");

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
                cerr << "Unknown element: " << e.tagName()
                          << endl;
                exit(-1);
            }
        }
    }

    selector = theme->GetSet("selector");
    if (!selector) {
        cerr << "StatusBox: Failed to get selector container." << endl;
        exit(-1);
    }

    icon_list = (UIListType*)selector->GetType("icon_list");
    if (!icon_list) {
        cerr << "StatusBox: Failed to get icon list area." << endl;
        exit(-1);
    }

    content = theme->GetSet("content");
    if (!content) {
        cerr << "StatusBox: Failed to get content container." << endl;
        exit(-1);
    }

    list_area = (UIListType*)content->GetType("list_area");
    if (!list_area) {
        cerr << "StatusBox: Failed to get list area." << endl;
        exit(-1);
    }

    topbar = theme->GetSet("topbar");
    if (!topbar) {
        cerr << "StatusBox: Failed to get topbar container." << endl;
        exit(-1);
    }

    heading = (UITextType*)topbar->GetType("heading");
    if (!heading) {
        cerr << "StatusBox: Failed to get heading area." << endl;
        exit(-1);
    }

    helptext = (UITextType*)topbar->GetType("helptext");
    if (!helptext) {
        cerr << "StatusBox: Failed to get helptext area." << endl;
        exit(-1);
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
}

void StatusBox::doListingsStatus()
{
    QString mfdLastRunStart, mfdLastRunEnd, mfdLastRunStatus, Status;
    QString querytext, DataDirectMessage;
    int DaysOfData;
    QDateTime qdtNow, GuideDataThrough;
    int count = 0;

    contentLines.clear();
    contentDetail.clear();
    contentFont.clear();
    doScroll = false;

    qdtNow = QDateTime::currentDateTime();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT max(endtime) FROM program;");
    query.exec();

    if (query.isActive() && query.numRowsAffected())
    {
        query.next();
        GuideDataThrough = QDateTime::fromString(query.value(0).toString(),
                                                 Qt::ISODate);
    }

    mfdLastRunStart = gContext->GetSetting("mythfilldatabaseLastRunStart");
    mfdLastRunEnd = gContext->GetSetting("mythfilldatabaseLastRunEnd");
    mfdLastRunStatus = gContext->GetSetting("mythfilldatabaseLastRunStatus");
    DataDirectMessage = gContext->GetSetting("DataDirectMessage");

    contentLines[count++] = QObject::tr("Myth version:") + " " +
                                        MYTH_BINARY_VERSION;
    contentLines[count++] = QObject::tr("Last mythfilldatabase guide update:");
    contentLines[count++] = QObject::tr("Started:   ") + mfdLastRunStart;

    if (mfdLastRunEnd >= mfdLastRunStart) //if end < start, it's still running.
        contentLines[count++] = QObject::tr("Finished: ") + mfdLastRunEnd;

    contentLines[count++] = QObject::tr("Result: ") + mfdLastRunStatus;

    DaysOfData = qdtNow.daysTo(GuideDataThrough);

    if (GuideDataThrough.isNull())
    {
        contentLines[count++] = "";
        contentLines[count++] = QObject::tr("There's no guide data available!");
        contentLines[count++] = QObject::tr("Have you run mythfilldatabase?");
    }
    else
    {
        contentLines[count++] = "";
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

    if (query.isActive() && query.numRowsAffected())
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

#define MB (1024*1024)

static const QString usageStr(float total, float used, float free)
{
    char  buff1[10];       // QString::arg(double) doesn't allow %0.2f,
    char  buff2[10];       // so we need to use sprintf() with a buffer
    char *units  = "MB";
    char *format = "%0.0f";

    if (total == 0.0)
        return "unknown";

    if (total > 1024)
        units = "GB", format = "%0.2f",
        total /= 1024, used /= 1024, free /= 1024;

    sprintf(buff1, format, total);

    QString s = QString("%1 %2").arg(buff1).arg(units);
    //QString s = QString("%1 %2 %3")
                //.arg(buff1).arg(units).arg(QObject::tr("total"));


    // If, some some reason, we didn't get the usage stats, shorten output:

    if (used == 0 && free == 0)
        return s;


    sprintf(buff1, format, used);
    sprintf(buff2, format, free);

    return s + ", " +
           QString("%1 %2 %3")
           .arg(buff1).arg(units).arg(QObject::tr("used"))
           + ", " +
           QString("%1 %2 %3 (%4%)")
           .arg(buff2).arg(units).arg(QObject::tr("free"))
           .arg((int)(100*free/total));
}

static const QString diskUsageStr(const char *path)
{
    double total, free, used;

    if (diskUsage(path, total, used, free))
        return usageStr (total, used, free);
    else
        return QString("%1 - %2").arg(path).arg(strerror(errno));
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

void StatusBox::doMachineStatus()
{
    int           count(0);
    int           totalM, usedM, freeM;    // Physical memory
    int           totalS, usedS, freeS;    // Virtual  memory (swap)
    time_t        uptime;

    contentLines.clear();
    contentDetail.clear();
    contentFont.clear();
    //doScroll = false;??

    if (isBackend)
        contentLines[count++] = QObject::tr("System") + ":";
    else
        contentLines[count++] = QObject::tr("This machine") + ":";

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
    count++;


    // memory usage
    if (getMemStats(totalM, freeM, totalS, freeS))
    {
        usedM = totalM - freeM;
        if (totalM > 0)
            contentLines[count++] = "   " + QObject::tr("RAM") +
                                    ": "  + usageStr(totalM, usedM, freeM);
        usedS = totalS - freeS;
        if (totalS > 0)
            contentLines[count++] = "   " +
                                    QObject::tr("Swap") +
                                    ": "  + usageStr(totalS, usedS, freeS);
    }

    if (!isBackend)
    {
        contentLines[count++] = QObject::tr("MythTV server") + ":";

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
            contentLines[count++].append(QObject::tr("unknown"));

        // memory usage
        if (RemoteGetMemStats(totalM, freeM, totalS, freeS))
        {
            usedM = totalM - freeM;
            if (totalM > 0)
                contentLines[count++] = "   " + QObject::tr("RAM") +
                                        ": "  + usageStr(totalM, usedM, freeM);
            usedS = totalS - freeS;
            if (totalS > 0)
                contentLines[count++] = "   " + QObject::tr("Swap") +
                                        ": "  + usageStr(totalS, usedS, freeS);
        }
    }

    // get free disk space
    contentLines[count++] = QString(QObject::tr("Disk space:"));

    if (isBackend)
    {
        QString str;

        contentLines[count] = "   " + QString(QObject::tr("Recordings")) + ": ";

        // Perform the database queries
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT * FROM settings "
                      "WHERE value=\"RecordFilePrefix\" "
                      "AND hostname = :HOSTNAME ;");
        query.bindValue(":HOSTNAME", gContext->GetHostName());
        query.exec();

        // did we get results?
        if (query.isActive() && query.numRowsAffected())
        {
            // skip to first result
            query.next();
            str = diskUsageStr(query.value(1).toCString());
        }
        else
            str = QString(QObject::tr("DB error - RecordFilePrefix unknown"));
        contentLines[count++].append(str);

        // Ditto for the second path
        contentLines[count] = "   " + QString(QObject::tr("TV buffer")) + ": ";

        query.prepare("SELECT * FROM settings "
                      "WHERE value=\"LiveBufferDir\" "
                      "AND hostname = :HOSTNAME ;");
        query.bindValue(":HOSTNAME", gContext->GetHostName());
        query.exec();
        if (query.isActive() && query.numRowsAffected())
        {
            query.next();
            str = diskUsageStr(query.value(1).toCString());
        }
        else
            str = QString(QObject::tr("DB error - LiveBufferDir unknown"));
        contentLines[count++].append(str);
    }
    else
    {
        int  total, used;
        RemoteGetFreeSpace(total, used);
        if (total)
            contentLines[count++] = "   " + usageStr(total, used, total - used);
        else
            contentLines[count++] = "   " + QString(QObject::tr("Unknown"));
    }

    contentTotalLines = count;
    update(ContentRect);
}
