#include <qlayout.h>
#include <qiconview.h>
#include <qsqldatabase.h>
#include <qwidgetstack.h>
#include <qvbox.h>
#include <qgrid.h>
#include <qregexp.h>

#include <unistd.h>

#include <iostream>
using namespace std;

#include "statusbox.h"
#include "mythcontext.h"
#include "remoteutil.h"
#include "programinfo.h"
#include "tv.h"

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
#ifdef USING_DVB
    if (gContext->GetNumSetting("DVBMonitorInterval", 0))
        icon_list->SetItemText(item_count++, QObject::tr("DVB Status"));
#endif
    icon_list->SetItemText(item_count++, QObject::tr("Log Entries"));
    icon_list->SetItemCurrent(0);
    icon_list->SetActive(true);

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
                list_area->SetItemText(x - startPos, contentLines[x]);
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

    list_area->SetUpArrow(startPos > 0);
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
                    QString query = QString("UPDATE mythlog "
                                            "SET acknowledged = 1 "
                                            "WHERE priority <= %1")
                                            .arg(min_level);
                    QSqlDatabase *db = QSqlDatabase::database();
                    db->exec(query);
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
        else if ((action == "LEFT") &&
                 (inContent))
        {
            inContent = false;
            contentPos = 0;
            list_area->SetActive(false);
            icon_list->SetActive(true);
            update(SelectRect);
            update(ContentRect);
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
    }
    update(TopRect);
}

void StatusBox::clicked()
{
    if ((inContent) &&
        (icon_list->GetItemText(icon_list->GetCurrentItem()) == QObject::tr("Log Entries")))
    {
        int retval = MythPopupBox::show2ButtonPopup(my_parent,
                                   QString("AckLogEntry"),
                                   QObject::tr("Acknowledge this log entry?"),
                                   QObject::tr("Yes"), QObject::tr("No"), 0);
        if (retval == 0)
        {
            QString query = QString("UPDATE mythlog SET acknowledged = 1 "
                                    "WHERE logid = %1")
                                    .arg(contentData[contentPos]);
            QSqlDatabase *db = QSqlDatabase::database();
            db->exec(query);
            doLogEntries();
        }
    } else {
        // Clear all visible content elements here
        // I'm sure there's a better way to do this but I can't find it
        content->ClearAllText();
        list_area->ResetList();
        contentLines.clear();
        contentDetail.clear();
        contentData.clear();

        QString currentItem;

        currentItem = icon_list->GetItemText(icon_list->GetCurrentItem());

        if (currentItem == QObject::tr("Listings Status"))
            doListingsStatus();

        if (currentItem == QObject::tr("Tuner Status"))
            doTunerStatus();

        if (currentItem == QObject::tr("DVB Status"))
            doDVBStatus();

        if (currentItem == QObject::tr("Log Entries"))
            doLogEntries();
    }
}

void StatusBox::doListingsStatus()
{
    QString mfdLastRunStart, mfdLastRunEnd, mfdLastRunStatus, Status;
    QString querytext, DataDirectMessage;
    int DaysOfData;
    QDateTime qdtNow, GuideDataThrough;
    QSqlDatabase *db = QSqlDatabase::database();
    int count = 0;

    contentLines.clear();
    contentDetail.clear();
    doScroll = false;

    qdtNow = QDateTime::currentDateTime();
    querytext = QString("SELECT max(endtime) FROM program;");

    QSqlQuery query = db->exec(querytext);

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

    QString querytext = QString("SELECT cardid FROM capturecard;");
    QSqlDatabase *db = QSqlDatabase::database();
    QSqlQuery query = db->exec(querytext);

    contentLines.clear();
    contentDetail.clear();

    if (query.isActive() && query.numRowsAffected())
    {
        while(query.next())
        {
            int cardid = query.value(0).toInt();

            QString cmd = QString("QUERY_REMOTEENCODER %1").arg(cardid);
            QStringList strlist = cmd;
            strlist << "GET_STATE";

            gContext->SendReceiveStringList(strlist);
  
            QString Status = QString(tr("Tuner %1 ")).arg(cardid);
            if (strlist[0].toInt()==kState_WatchingLiveTV)
                Status += tr("is watching live TV");
            else if (strlist[0].toInt()==kState_RecordingOnly ||
                     strlist[0].toInt()==kState_WatchingRecording)
                Status += tr("is recording");
            else 
                Status += tr("is not recording");

            contentLines[count] = Status;
            contentDetail[count] = Status;

            if (strlist[0].toInt()==kState_RecordingOnly)
            {
                strlist = QString("QUERY_RECORDER %1").arg(cardid);
                strlist << "GET_RECORDING";
                gContext->SendReceiveStringList(strlist);
                ProgramInfo *proginfo = new ProgramInfo;
                proginfo->FromStringList(strlist, 0);
   
                Status = proginfo->title;
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

void StatusBox::doDVBStatus(void)
{
    QString querytext;
    bool doneAnything = false;
  
    doScroll = false;
    int count = 0;
  
    contentLines.clear();
    contentDetail.clear();
 
    QString Status = QObject::tr("Details of DVB error statistics for last 48 "
                                 "hours:\n");

    QString outerqry =
        "SELECT starttime,endtime FROM recorded "
        "WHERE starttime >= DATE_SUB(NOW(), INTERVAL 48 HOUR) "
        "ORDER BY starttime;";

    QSqlDatabase *db = QSqlDatabase::database();
    QSqlQuery oquery = db->exec(outerqry);

    if (oquery.isActive() && oquery.numRowsAffected())
    {
        querytext = QString("SELECT cardid,"
                            "max(fe_ss),min(fe_ss),avg(fe_ss),"
                            "max(fe_snr),min(fe_snr),avg(fe_snr),"
                            "max(fe_ber),min(fe_ber),avg(fe_ber),"
                            "max(fe_unc),min(fe_unc),avg(fe_unc),"
                            "max(myth_cont),max(myth_over),max(myth_pkts) "
                            "FROM dvb_signal_quality "
                            "WHERE sampletime BETWEEN ? AND ? "
                            "GROUP BY cardid");

        QSqlQuery query = db->exec(NULL);
        query.prepare(querytext);
        
        while (oquery.next())
        {
            QDateTime t_start = oquery.value(0).toDateTime();
            QDateTime t_end = oquery.value(1).toDateTime();

            query.bindValue(0, t_start);
            query.bindValue(1, t_end);

            if (!query.exec())
                cout << query.lastError().databaseText() << "\r\n" 
                     << query.lastError().driverText() << "\r\n";
            
            if (query.isActive() && query.numRowsAffected())
            {
                contentLines[count++] =
                       QObject::tr("Recording period from %1 to %2")
                                   .arg(t_start.toString())
                                   .arg(t_end.toString());
                
                while (query.next())
                {
                    contentLines[count++] =
                           QObject::tr("Encoder %1 Min SNR: %2 Avg SNR: %3 Min "
                                       "BER %4 Avg BER %5 Cont Errs: %6 "
                                       "Overflows: %7")
                                       .arg(query.value(0).toInt())
                                       .arg(query.value(5).toInt())
                                       .arg(query.value(6).toInt())
                                       .arg(query.value(8).toInt())
                                       .arg(query.value(9).toInt())
                                       .arg(query.value(13).toInt())
                                       .arg(query.value(14).toInt());

                    doneAnything = true;
                }
            }
        }
    }

    if (!doneAnything)
    {
        contentLines[count++] = QObject::tr("There is no DVB signal quality "
                                            "data available to display.");
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
    contentData.clear();

    QSqlDatabase *db = QSqlDatabase::database();
    QString thequery;

    thequery = QString("SELECT logid, module, priority, logdate, host, "
                       "message, details "
                       "FROM mythlog WHERE acknowledged = 0 AND priority <= %1 "
                       "ORDER BY logdate DESC;").arg(min_level);
    QSqlQuery query = db->exec(thequery);
    if (query.isActive())
    {
        while (query.next())
        {
            line = QString("%1").arg(query.value(5).toString());
            contentLines[count] = line;

            if (query.value(6).toString() != "")
                line = QString("On %1 %2 from %3.%4\n%5\n%6")
                               .arg(query.value(3).toDateTime()
                                         .toString(dateFormat))
                               .arg(query.value(3).toDateTime()
                                         .toString(timeFormat))
                               .arg(query.value(4).toString())
                               .arg(query.value(1).toString())
                               .arg(query.value(5).toString())
                               .arg(query.value(6).toString());
            else
                line = QString("On %1 %2 from %3.%4\n%5\nNo other details")
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

