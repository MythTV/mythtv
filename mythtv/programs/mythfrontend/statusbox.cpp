#include <qlayout.h>
#include <qiconview.h>
#include <qsqldatabase.h>
#include <qwidgetstack.h>
#include <qvbox.h>
#include <qgrid.h>

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
    max_icons = 3;

    setNoErase();
    LoadTheme();
  
    icon_list->SetItemText(0, "Listings Status");
    icon_list->SetItemText(1, "Tuner Status");
    icon_list->SetItemText(2, "DVB Status");
    // icon_list->SetItemText(3, "Log Entries");
    icon_list->SetItemCurrent(0);
    icon_list->SetActive(true);
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
 
    if (content  == NULL) return;
    LayerSet *container = content;

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

    text_area = (UITextType*)content->GetType("text_area");
    if (!text_area) {
        cerr << "StatusBox: Failed to get text area." << endl;
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
        handled = true;

        if (action == "SELECT")
        {
            clicked();
        }
        else if (action == "UP")
        {
            if (icon_list->GetCurrentItem() > 0)
                icon_list->SetItemCurrent(icon_list->GetCurrentItem()-1);
            setHelpText();
            update(SelectRect);
        }
        else if (action == "DOWN")
        {
            if (icon_list->GetCurrentItem() < (max_icons - 1))
                icon_list->SetItemCurrent(icon_list->GetCurrentItem()+1);
            setHelpText();
            update(SelectRect);
        }
        else
            handled = false;
    }

    if (!handled)
        MythDialog::keyPressEvent(e);
}

void StatusBox::setHelpText()
{
    topbar->ClearAllText();
    switch (icon_list->GetCurrentItem())
    {
        case 0:
            helptext->SetText("Listings Status shows the latest status information from mythfilldatabase");
            break;
        case 1:
            helptext->SetText("Tuner Status shows the current information about the state of backend tuner cards");
            break;
        case 2:
            helptext->SetText("DVB Status shows the quality statistics of all DVB cards, if present");
            break;
        case 3:
            helptext->SetText("Log Entries shows any unread log entries from the system if you have logging enabled");
            break;
    }
    update(TopRect);
}

void StatusBox::clicked()
{
    // Clear all visible content elements here
    // I'm sure there's a better way to do this but I can't find it
    content->ClearAllText();
    list_area->ResetList();

    switch (icon_list->GetCurrentItem())
    {
        case 0:
            doListingsStatus();
            break;
        case 1:
            doTunerStatus();
            break;
        case 2:
            doDVBStatus();
            break;
        case 3:
            doLogEntries();
            break;
    }
}

void StatusBox::doListingsStatus()
{
    QString mfdLastRunStart, mfdLastRunEnd, mfdLastRunStatus, Status;
    QString querytext, DataDirectMessage;
    int DaysOfData;
    QDateTime qdtNow, GuideDataThrough;
    QSqlDatabase *db = QSqlDatabase::database();

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

    Status = QObject::tr("Myth version:") + " " + MYTH_BINARY_VERSION + "\n";

    Status += QObject::tr("Last mythfilldatabase guide update:");
    Status += "\n   ";
    Status += QObject::tr("Started:   ");
    Status += mfdLastRunStart;
    if (mfdLastRunEnd > mfdLastRunStart)  //if end < start, it's still running.
    {
        Status += "\n   ";
        Status += QObject::tr("Finished: ");
        Status += mfdLastRunEnd;
    }

    Status += "\n   ";
    Status += QObject::tr("Result: ");
    Status += mfdLastRunStatus;

    DaysOfData = qdtNow.daysTo(GuideDataThrough);

    if (GuideDataThrough.isNull())
    {
        Status += "\n\n";
        Status += QObject::tr("There's no guide data available! ");
        Status += QObject::tr("Have you run mythfilldatabase?");
        Status += "\n";
    }
    else
    {
        Status += "\n\n";
        Status += QObject::tr("There is guide data until ");
        Status += QDateTime(GuideDataThrough).toString("yyyy-MM-dd hh:mm");

        if (DaysOfData > 0)
        {
            Status += QString("\n(%1 ").arg(DaysOfData);
            if (DaysOfData >1)
                Status += QObject::tr("days");
            else
                Status += QObject::tr("day");
            Status += ").";
        }
        else
            Status += ".";
    }

    if (DaysOfData <= 3)
    {
        Status += "\n";
        Status += QObject::tr("WARNING: is mythfilldatabase running?");
    }

    if (!DataDirectMessage.isNull())
    {
        Status += "\n";
        Status += QObject::tr("DataDirect Status: \n");
        Status += DataDirectMessage;
    }
   
    text_area->SetText(Status);
    update(ContentRect);
}

void StatusBox::doTunerStatus()
{
    int count = 0;

    QString querytext = QString("SELECT cardid FROM capturecard;");
    QSqlDatabase *db = QSqlDatabase::database();
    QSqlQuery query = db->exec(querytext);
    if (query.isActive() && query.numRowsAffected())
    {
        list_area->ResetList();
        while(query.next())
        {
            int cardid = query.value(0).toInt();

            QString cmd = QString("QUERY_REMOTEENCODER %1").arg(cardid);
            QStringList strlist = cmd;
            strlist << "GET_STATE";

            gContext->SendReceiveStringList(strlist);
  
            QString Status = QString("Tuner %1 ").arg(cardid);
            if (strlist[0].toInt()==kState_WatchingLiveTV)
                Status += "is watching live TV";
            else if (strlist[0].toInt()==kState_RecordingOnly ||
                     strlist[0].toInt()==kState_WatchingRecording)
                Status += "is recording";
            else 
                Status += "is not recording";

            list_area->SetItemText(count, Status);
            count++;

            if (strlist[0].toInt()==kState_RecordingOnly)
            {
                strlist = QString("QUERY_RECORDER %1").arg(cardid);
                strlist << "GET_RECORDING";
                gContext->SendReceiveStringList(strlist);
                ProgramInfo *proginfo = new ProgramInfo;
                proginfo->FromStringList(strlist, 0);
   
                Status = "   ";
                Status += proginfo->title;
                list_area->SetItemText(count++, Status);

                Status = "   ";
                Status += proginfo->subtitle;
                if (Status != "   ")
                    list_area->SetItemText(count++, Status);
            }
        }
    }
    update(ContentRect);
}

void StatusBox::doDVBStatus(void)
{
    QString querytext;

    bool doneAnything = false;
    
    QString Status = QString("Details of DVB error statistics for last 48 hours:\n");

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
                Status += QString("Recording period from %1 to %2\n").arg(t_start.toString()).arg(t_end.toString());
                
                while (query.next())
                {
		    Status += QString("Encoder %1 Min SNR: %2 Avg SNR: %3 Min BER %4 Avg BER %5 Cont Errs: %6 Overflows: %7\n")
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
        Status += QString("There is no DVB signal quality data available to display.\n");
    }

    text_area->SetText(Status);
    update(ContentRect);
}

void StatusBox::doLogEntries(void)
{
/*
    // int minlevel = gContext->GetNumSetting("LogDefaultView",0);
    int minlevel = 8;

    log_list->clear();
    QSqlDatabase *db = QSqlDatabase::database();
    QString thequery = QString("SELECT logid, module, priority, logdate, host, message, "
                               "details FROM mythlog WHERE acknowledged = 0 and priority <= %1 "
                               "order by logdate").arg(minlevel);
    QSqlQuery query = db->exec(thequery);
    if (query.isActive())
        while (query.next())
            log_list->insertItem(QString("%1 %2").arg(query.value(3).toString()).arg(query.value(5).toString()));
*/
}

StatusBox::~StatusBox(void)
{
}

