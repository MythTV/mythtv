#include <qlayout.h>
#include <qpushbutton.h>
#include <qbuttongroup.h>
#include <qlabel.h>
#include <qcursor.h>
#include <qsqldatabase.h>
#include <qdatetime.h>
#include <qapplication.h>
#include <qregexp.h>
#include <qheader.h>

#include <iostream>
using namespace std;

#include "viewscheduled.h"
#include "scheduledrecording.h"
#include "infodialog.h"
#include "tv.h"

#include "dialogbox.h"
#include "mythcontext.h"
#include "remoteutil.h"

ViewScheduled::ViewScheduled(QSqlDatabase *ldb, MythMainWindow *parent, 
                             const char *name)
             : MythDialog(parent, name)
{
    db = ldb;

    doingSel = false;
    curitem = NULL;
    conflictBool = false;
    pageDowner = false;

    inList = 0;
    inData = 0;
    listCount = 0;
    dataCount = 0;

    dateformat = gContext->GetSetting("DateFormat", "ddd MMMM d");
    shortdateformat = gContext->GetSetting("ShortDateFormat", "M/d");
    timeformat = gContext->GetSetting("TimeFormat", "h:mm AP");
    displayChanNum = gContext->GetNumSetting("DisplayChanNum");
    showLevel = ShowLevel(gContext->GetNumSetting("ViewSchedShowLevel",
                                                      (int)showAll));

    fullRect = QRect(0, 0, size().width(), size().height());
    listRect = QRect(0, 0, 0, 0);
    infoRect = QRect(0, 0, 0, 0);
    conflictRect = QRect(0, 0, 0, 0);
    showLevelRect = QRect(0, 0, 0, 0);

    allowselect = true;
    allowKeys = true;

    theme = new XMLParse();
    theme->SetWMult(wmult);
    theme->SetHMult(hmult);
    theme->LoadTheme(xmldata, "conflict");
    LoadWindow(xmldata);

    LayerSet *container = theme->GetSet("selector");
    if (container)
    {
        UIListType *ltype = (UIListType *)container->GetType("conflictlist");
        if (ltype)
        {
            listsize = ltype->GetItems();
        }
    }
    else
    {
        cerr << "MythFrontEnd: ViewSchedule - Failed to get selector object.\n";
        exit(0);
    }

    bgTransBackup = gContext->LoadScalePixmap("trans-backup.png");
    if (!bgTransBackup)
        bgTransBackup = new QPixmap();

    updateBackground();

    FillList();
 
    setNoErase();

    gContext->addListener(this);
}

ViewScheduled::~ViewScheduled()
{
    gContext->removeListener(this);
    delete theme;
    delete bgTransBackup;
    if (curitem)
        delete curitem;
}

void ViewScheduled::keyPressEvent(QKeyEvent *e)
{
    if (!allowKeys)
        return;

    if (allowselect)
    {
        switch (e->key())
        {
            case Key_Space: case Key_Enter: case Key_Return: selected(); return;
            case Key_I: edit(); return;
            case Key_Escape:
                gContext->SaveSetting("ViewSchedShowLevel", (int)showLevel);
                done(MythDialog::Accepted);
                break;
            case Key_1: case Key_2: case Key_3:
                if (showLevel != ShowLevel(e->key() - Key_0))
                {
                    showLevel = ShowLevel(e->key() - Key_0);
                    inList = 0;
                    inData = 0;
                    FillList();
                    update(fullRect);
                }
                break;
            default: break;
        }
    }

    switch (e->key())
    {
        case Key_Up: cursorUp(); break;
        case Key_Down: cursorDown(); break;
        case Key_PageUp: case Key_3: pageUp(); break;
        case Key_PageDown: case Key_9: pageDown(); break;
        default: MythDialog::keyPressEvent(e); break;
    }
}

void ViewScheduled::LoadWindow(QDomElement &element)
{
    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement e = child.toElement();
        if (!e.isNull())
        {
            if (e.tagName() == "font")
            {
                theme->parseFont(e);
            }
            else if (e.tagName() == "container")
            {
                parseContainer(e);
            }
            else if (e.tagName() == "popup")
            {
                parsePopup(e);
            }
            else
            {
                cerr << "Unknown element: " << e.tagName() << endl;
                exit(0);
            }
        }
    }
}

void ViewScheduled::parseContainer(QDomElement &element)
{
    QRect area;
    QString name;
    int context;
    theme->parseContainer(element, name, context, area);

    if (name.lower() == "selector")
        listRect = area;
    if (name.lower() == "program_info")
        infoRect = area;
    if (name.lower() == "conflict_info")
        conflictRect = area;
    if (name.lower() == "showlevel_info")
        showLevelRect = area;
}

void ViewScheduled::parsePopup(QDomElement &element)
{
    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        cerr << "Popup needs a name\n";
        exit(0);
    }

    if (name != "selectrec")
    {
        cerr << "Unknown popup name! (try using 'selectrec')\n";
        exit(0);
    }

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "solidbgcolor")
            {
                QString col = theme->getFirstText(info);
                popupBackground = QColor(col);
                graphicPopup = false;
            }
            else if (info.tagName() == "foreground")
            {
                QString col = theme->getFirstText(info);
                popupForeground = QColor(col);
            }
            else if (info.tagName() == "highlight")
            {
                QString col = theme->getFirstText(info);
                popupHighlight = QColor(col);
            }
            else
            {
                cerr << "Unknown popup child: " << info.tagName() << endl;
                exit(0);
            }
        }
    }
}

void ViewScheduled::updateBackground(void)
{
    QPixmap bground(size());
    bground.fill(this, 0, 0);

    QPainter tmp(&bground);

    LayerSet *container = theme->GetSet("background");
    container->Draw(&tmp, 0, 0);

    tmp.end();
    myBackground = bground;

    setPaletteBackgroundPixmap(myBackground);
}

void ViewScheduled::paintEvent(QPaintEvent *e)
{
    if (doingSel || !allowKeys)
        return;

    QRect r = e->rect();
    QPainter p(this);
 
    if (r.intersects(listRect))
    {
        updateList(&p);
    }
    if (r.intersects(infoRect))
    {
        updateInfo(&p);
    }
    if (r.intersects(conflictRect))
    {
        updateConflict(&p);
    }
    if (r.intersects(showLevelRect))
    {
        updateShowLevel(&p);
    }
}

void ViewScheduled::grayOut(QPainter *tmp)
{
    int transparentFlag = gContext->GetNumSetting("PlayBoxShading", 0);
    if (transparentFlag == 0)
        tmp->fillRect(QRect(QPoint(0, 0), size()), QBrush(QColor(10, 10, 10), 
                      Dense4Pattern));
    else if (transparentFlag == 1)
        tmp->drawPixmap(0, 0, *bgTransBackup, 0, 0, (int)(800*wmult), 
                        (int)(600*hmult));
}

void ViewScheduled::cursorDown(bool page)
{
    if (page == false)
    {
        if (inList > (int)((int)(listsize / 2) - 1)
            && ((int)(inData + listsize) <= (int)(dataCount - 1))
            && pageDowner == true)
        {
            inData++;
            inList = (int)(listsize / 2);
        }
        else
        {
            inList++;

            if (inList >= listCount)
                inList = listCount - 1;
        }
    }
    else if (page == true && pageDowner == true)
    {
        if (inList >= (int)(listsize / 2) || inData != 0)
        {
            inData = inData + listsize;
        }
        else if (inList < (int)(listsize / 2) && inData == 0)
        {
            inData = (int)(listsize / 2) + inList;
            inList = (int)(listsize / 2);
        }
    }
    else if (page == true && pageDowner == false)
    {
        inList = listsize - 1;
    }

    if ((int)(inData + inList) >= (int)(dataCount))
    {
        inData = dataCount - listsize;
        inList = listsize - 1;
    }
    else if ((int)(inData + listsize) >= (int)dataCount)
    {
        inData = dataCount - listsize;
    }

    if (inList >= listCount)
        inList = listCount - 1;

    update(fullRect);
}

void ViewScheduled::cursorUp(bool page)
{
    if (page == false)
    {
        if (inList < ((int)(listsize / 2) + 1) && inData > 0)
        {
            inList = (int)(listsize / 2);
            inData--;
            if (inData < 0)
            {
                inData = 0;
                inList--;
            }
         }
         else
         {
             inList--;
         }
     }
     else if (page == true && inData > 0)
     {
         inData = inData - listsize;
         if (inData < 0)
         {
             inList = inList + inData;
             inData = 0;
             if (inList < 0)
                 inList = 0;
         }

         if (inList > (int)(listsize / 2))
         {
             inList = (int)(listsize / 2);
             inData = inData + (int)(listsize / 2) - 1;
         }
     }
     else if (page == true)
     {
         inData = 0;
         inList = 0;
     }

     if (inList > -1)
     {
         update(fullRect);
     }
     else
         inList = 0;
}

void ViewScheduled::FillList(void)
{
    QString chanid = "";
    int cnt = 999;
    conflictBool = false;

    conflictData.clear();

    bool conflicts = false;
    vector<ProgramInfo *> recordinglist;
    QString cntStr = "";

    allowKeys = false;
    conflicts = RemoteGetAllPendingRecordings(recordinglist);
    allowKeys = true;

    vector<ProgramInfo *>::reverse_iterator pgiter = recordinglist.rbegin();

    for (; pgiter != recordinglist.rend(); pgiter++)
    {
        ProgramInfo *p = *pgiter;
        cntStr.sprintf("%d", cnt);
        if (p->conflicting)
            conflictBool = true;
        if (!p->recording && showLevel != showAll &&
              (showLevel != showImportant ||
               p->norecord <= nrOtherShowing))
            continue;
        conflictData[cntStr] = *p;
        delete p;
        cnt--;
        dataCount++;
    }
}

void ViewScheduled::updateList(QPainter *p)
{
    QRect pr = listRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);
    
    int pastSkip = (int)inData;
    pageDowner = false;
    listCount = 0;

    typedef QMap<QString, ProgramInfo> ConflictData;
    ProgramInfo *tempInfo;
  
    QString tempSubTitle = "";
    QString tempDate = "";
    QString tempTime = "";
    QString tempChan = "";
    QString tempCard = "";

    ConflictData::Iterator it;
    ConflictData::Iterator start;
    ConflictData::Iterator end;

    start = conflictData.begin();
    end = conflictData.end();

    LayerSet *container = NULL;
    container = theme->GetSet("selector");
    if (container)
    {
        UIListType *ltype = (UIListType *)container->GetType("conflictlist");
        if (ltype)
        {
            int cnt = 0;
            ltype->ResetList();
            ltype->SetActive(true);

            for (it = start; it != end; ++it)
            {
                if (cnt < listsize)
                {
                    if (pastSkip <= 0)
                    {
                        tempInfo = &(it.data());

                        tempSubTitle = tempInfo->title;
                        if ((tempInfo->subtitle).stripWhiteSpace().length() > 0)
                            tempSubTitle = tempSubTitle + " - \"" + 
                                           tempInfo->subtitle + "\"";

                        tempDate = (tempInfo->recstartts).toString(shortdateformat);
                        tempTime = (tempInfo->recstartts).toString(timeformat);

                        if (displayChanNum)
                            tempChan = tempInfo->chansign;
                        else
                            tempChan = tempInfo->chanstr;

                        if (tempInfo->recording)
                            tempCard = QString::number(tempInfo->cardid);
                        else
                        {
                            switch (tempInfo->norecord)
                            {
                            case nrManualOverride:    tempCard = "X"; break;
                            case nrPreviousRecording: tempCard = "P"; break;
                            case nrCurrentRecording:  tempCard = "C"; break;
                            case nrOtherShowing:      tempCard = "O"; break;
                            case nrTooManyRecordings: tempCard = "T"; break;
                            case nrDontRecordList:    tempCard = "D"; break;
                            case nrLowerRanking:      tempCard = "R"; break;
                            case nrManualConflict:    tempCard = "M"; break;
                            case nrAutoConflict:      tempCard = "A"; break;
                            default:                  tempCard = "-"; break;
                            }
                        }

                        if (cnt == inList)
                        {
                            if (curitem)
                                delete curitem;
                            curitem = new ProgramInfo(*tempInfo);
                            ltype->SetItemCurrent(cnt);
                        }

                        ltype->SetItemText(cnt, 1, tempChan);
                        ltype->SetItemText(cnt, 2, tempDate + " " + tempTime);
                        ltype->SetItemText(cnt, 3, tempSubTitle);
                        ltype->SetItemText(cnt, 4, tempCard);

                        if (tempInfo->conflicting)
                        {
                            ltype->EnableForcedFont(cnt, 
                                                    "conflictingrecording");
                        }

                        if (!tempInfo->recording)
                        {
                            ltype->EnableForcedFont(cnt, "disabledrecording");
                        }

                        cnt++;
                        listCount++;
                    }
                    pastSkip--;
                }
                else
                    pageDowner = true;
            }
        }

        ltype->SetDownArrow(pageDowner);
        if (inData > 0)
            ltype->SetUpArrow(true);
        else
            ltype->SetUpArrow(false);
    }

    if (conflictData.count() <= 0)
        container = theme->GetSet("norecordings_list");

    if (container)
    {
       container->Draw(&tmp, 0, 0);
       container->Draw(&tmp, 1, 0);
       container->Draw(&tmp, 2, 0);
       container->Draw(&tmp, 3, 0);
       container->Draw(&tmp, 4, 0);
       container->Draw(&tmp, 5, 0);
       container->Draw(&tmp, 6, 0);
       container->Draw(&tmp, 7, 0);
       container->Draw(&tmp, 8, 0);
    }

    tmp.end();
    p->drawPixmap(pr.topLeft(), pix);
}

void ViewScheduled::updateConflict(QPainter *p)
{
    QRect pr = conflictRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);

    LayerSet *container = NULL;
    container = theme->GetSet("conflict_info");
    if (conflictBool == true)
    {
        if (container)
        {
           UITextType *type = (UITextType *)container->GetType("status");
           if (type)
               type->SetText(tr("Time Conflict"));
        }
    }
    else
    {
        if (container)
        {
           UITextType *type = (UITextType *)container->GetType("status");
           if (type)
               type->SetText(tr("No Conflicts"));
        }
    }

    if (container)
    {
        container->Draw(&tmp, 4, 0);
        container->Draw(&tmp, 5, 0);
        container->Draw(&tmp, 6, 0);
        container->Draw(&tmp, 7, 0);
        container->Draw(&tmp, 8, 0);
    }

    tmp.end();
    p->drawPixmap(pr.topLeft(), pix);
}

void ViewScheduled::updateShowLevel(QPainter *p)
{
    QRect pr = showLevelRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);

    LayerSet *container = NULL;
    container = theme->GetSet("showlevel_info");
    if (container)
    {
        UITextType *type = (UITextType *)container->GetType("showlevel");
        if (type)
            switch (showLevel)
            {
                case showAll: type->SetText(tr("All")); break;
                case showImportant: type->SetText(tr("Important")); break;
                case showRecording: type->SetText(tr("Recording")); break;
            }
    }

    if (container)
    {
        container->Draw(&tmp, 4, 0);
        container->Draw(&tmp, 5, 0);
        container->Draw(&tmp, 6, 0);
        container->Draw(&tmp, 7, 0);
        container->Draw(&tmp, 8, 0);
    }

    tmp.end();
    p->drawPixmap(pr.topLeft(), pix);
}

void ViewScheduled::updateInfo(QPainter *p)
{
    QRect pr = infoRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);

    if (conflictData.count() > 0 && curitem)
    {  

        QDateTime startts = curitem->recstartts;
        QDateTime endts = curitem->recendts;

        QString timedate = startts.date().toString(dateformat) + ", " +
                           startts.time().toString(timeformat) + " - " +
                           endts.time().toString(timeformat);

        QString subtitle = "";
        QString chantext = "";
        QString description = "";

        if (gContext->GetNumSetting("DisplayChanNum") != 0)
            chantext = curitem->channame + " [" + curitem->chansign + "]";
        else
            chantext = curitem->chanstr;

        if (curitem->subtitle != "(null)")
            subtitle = curitem->subtitle;
        else
            subtitle = "";

        if (curitem->description != "(null)")
            description = curitem->description;
        else
            description = "";

        LayerSet *container = NULL;
        container = theme->GetSet("program_info");
        if (container)
        {
            UITextType *type = (UITextType *)container->GetType("title");
            if (type)
                type->SetText(curitem->title);
 
            type = (UITextType *)container->GetType("subtitle");
            if (type)
                type->SetText(subtitle);

            type = (UITextType *)container->GetType("timedate");
            if (type)
                type->SetText(timedate);

            type = (UITextType *)container->GetType("description");
            if (type)
                type->SetText(curitem->description);

            type = (UITextType *)container->GetType("channel");
            if (type)
                type->SetText(chantext);

        }
       
        if (container)
        {
            container->Draw(&tmp, 4, 0);
            container->Draw(&tmp, 5, 0);
            container->Draw(&tmp, 6, 0);
            container->Draw(&tmp, 7, 0);
            container->Draw(&tmp, 8, 0);
        }
    }
    else
    {
        LayerSet *norec = theme->GetSet("norecordings_info");
        if (norec)
        {
            norec->Draw(&tmp, 4, 0);
            norec->Draw(&tmp, 5, 0);
            norec->Draw(&tmp, 6, 0);
            norec->Draw(&tmp, 7, 0);
            norec->Draw(&tmp, 8, 0);
        }

        allowselect = false;
    }

    tmp.end();
    p->drawPixmap(pr.topLeft(), pix);
}

void ViewScheduled::edit()
{
    if (doingSel || !curitem)
        return;

    doingSel = true;

    ProgramInfo *rec = curitem;

    MythContext::KickDatabase(db);

    if ((gContext->GetNumSetting("AdvancedRecord", 0)) ||
        (rec->GetProgramRecordingStatus(db) > kAllRecord))
    {
        ScheduledRecording record;
        record.loadByProgram(db, rec);
        record.exec(db);
    }
    else
    {
        InfoDialog diag(rec, gContext->GetMainWindow(), "Program Info");
        diag.exec();
    }

    ScheduledRecording::signalChange(db);

    FillList();
    update(fullRect);

    doingSel = false;
}

void ViewScheduled::selected()
{
    if (doingSel || !curitem)
        return;

    doingSel = true;

    ProgramInfo *rec = curitem;

    MythContext::KickDatabase(db);

    if (rec->conflicting)
        handleConflicting(rec);
    else if (rec->recording)
        handleRecording(rec);
    else
        handleNotRecording(rec);

    doingSel = false;
}

void ViewScheduled::handleNotRecording(ProgramInfo *rec)
{
    QString message = QString("This showing will not be recorded because %1.")
        .arg(rec->NoRecordText());

    DialogBox diag(gContext->GetMainWindow(), tr(message));
    diag.AddButton(tr("OK"));

    if (rec->norecord != nrTooManyRecordings &&
        rec->norecord != nrDontRecordList &&
        rec->norecord != nrLowerRanking)
        diag.AddButton(tr("Record it anyway"));

    int ret = diag.exec();

    if (ret <= 1)
        return;

    if (rec->norecord != nrManualConflict &&
        rec->norecord != nrAutoConflict)
    {
        rec->SetOverride(db, 1);
        FillList();
        update(fullRect);
        return;
    }

    QString pstart = rec->startts.toString("yyyyMMddhhmm");
    pstart += "00";
    QString pend = rec->endts.toString("yyyyMMddhhmm");
    pend += "00";

    QString thequery;
    thequery = QString("INSERT INTO conflictresolutionoverride (chanid,"
                       "starttime, endtime) VALUES (%1, %2, %3);")
                       .arg(rec->chanid).arg(pstart).arg(pend);

    QSqlQuery qquery = db->exec(thequery);
    if (!qquery.isActive())
    {
        cerr << "DB Error: conflict resolution insertion failed, SQL query "
             << "was:" << endl;
        cerr << thequery << endl;
    }

    allowKeys = false;
    vector<ProgramInfo *> *conflictlist = RemoteGetConflictList(rec, false);
    allowKeys = true;

    if (!conflictlist)
        return;

    QString dstart, dend;
    vector<ProgramInfo *>::iterator i;
    for (i = conflictlist->begin(); i != conflictlist->end(); i++)
    {
        dstart = (*i)->startts.toString("yyyyMMddhhmm");
        dstart += "00";
        dend = (*i)->endts.toString("yyyyMMddhhmm");
        dend += "00";

        thequery = QString("DELETE FROM conflictresolutionoverride WHERE "
                           "chanid = %1 AND starttime = %2 AND "
                           "endtime = %3;")
                           .arg((*i)->chanid).arg(dstart).arg(dend);

        qquery = db->exec(thequery);
        if (!qquery.isActive())
        {
            cerr << "DB Error: conflict resolution deletion failed, SQL query "
                 << "was:" << endl;
            cerr << thequery << endl;
        }
    }

    vector<ProgramInfo *>::iterator iter = conflictlist->begin();
    for (; iter != conflictlist->end(); iter++)
    {
        delete (*iter);
    }
    delete conflictlist;

    ScheduledRecording::signalChange(db);

    FillList();
}

void ViewScheduled::handleConflicting(ProgramInfo *rec)
{
    chooseConflictingProgram(rec);
    return;
}

void ViewScheduled::handleRecording(ProgramInfo *rec)
{
    QString message = "This showing will be recorded.";

    DialogBox diag(gContext->GetMainWindow(), tr(message));
    diag.AddButton(tr("OK"));
    diag.AddButton(tr("Don't record it"));

    int ret = diag.exec();

    if (ret <= 1)
      return;

    rec->SetOverride(db, 2);
    FillList();
    update(fullRect);
}

void ViewScheduled::chooseConflictingProgram(ProgramInfo *rec)
{
    allowKeys = false;
    vector<ProgramInfo *> *conflictlist = RemoteGetConflictList(rec, true);
    allowKeys = true;

    if (!conflictlist)
        return;

    QString message = tr("The follow scheduled recordings conflict with each "
                         "other.  Which would you like to record?");

    DialogBox diag(gContext->GetMainWindow(), message, 
                   tr("Remember this choice and use it automatically "
                      "in the future"));
 
    QString button; 
    button = rec->title + QString("\n");
    button += rec->recstartts.toString(dateformat + " " + timeformat);
    if (gContext->GetNumSetting("DisplayChanNum") != 0)
        button += " on " + rec->channame + " [" + rec->chansign + "]";
    else
        button += QString(" on channel ") + rec->chanstr;

    diag.AddButton(button);

    vector<ProgramInfo *>::iterator i = conflictlist->begin();
    for (; i != conflictlist->end(); i++)
    {
        ProgramInfo *info = (*i);

        button = info->title + QString("\n");
        button += info->recstartts.toString(dateformat + " " + timeformat);
        if (gContext->GetNumSetting("DisplayChanNum") != 0)
            button += " on " + info->channame + " [" + info->chansign + "]";
        else
            button += QString(" on channel ") + info->chanstr;

        diag.AddButton(button);
    }

    int ret = diag.exec();
    int boxstatus = diag.getCheckBoxState();

    if (ret == 0)
    {
        vector<ProgramInfo *>::iterator iter = conflictlist->begin();
        for (; iter != conflictlist->end(); iter++)
        {
            delete (*iter);
        }

        delete conflictlist;
        return;
    }

    ProgramInfo *prefer = NULL;
    vector<ProgramInfo *> *dislike = new vector<ProgramInfo *>;
    if (ret == 2)
    {
        prefer = rec;
        for (i = conflictlist->begin(); i != conflictlist->end(); i++)
            dislike->push_back(*i);
    }
    else
    {
        dislike->push_back(rec);
        int counter = 3;
        for (i = conflictlist->begin(); i != conflictlist->end(); i++) 
        {
            if (counter == ret)
                prefer = (*i);
            else
                dislike->push_back(*i);
            counter++;
        }
    }

    if (!prefer)
    {
        printf("Ack, no preferred recording\n");
        delete dislike;
        vector<ProgramInfo *>::iterator iter = conflictlist->begin();
        for (; iter != conflictlist->end(); iter++)
        {
            delete (*iter);
        }

        delete conflictlist;
        return;
    }

    QString thequery;

    if (boxstatus == 1)
    {
        for (i = dislike->begin(); i != dislike->end(); i++)
        {
            QString sqltitle1 = prefer->title;
            QString sqltitle2 = (*i)->title;

            sqltitle1.replace(QRegExp("\""), QString("\\\""));
            sqltitle2.replace(QRegExp("\""), QString("\\\""));

            thequery = QString("INSERT INTO conflictresolutionany "
                               "(prefertitle, disliketitle) VALUES "
                               "(\"%1\", \"%2\");").arg(sqltitle1.utf8())
                               .arg(sqltitle2.utf8());
            QSqlQuery qquery = db->exec(thequery);
            if (!qquery.isActive())
            {
                cerr << "DB Error: conflict resolution insertion failed, SQL "
                     << "query was:" << endl;
                cerr << thequery << endl;
            }
        }
    } 
    else
    {
        QString pstart = prefer->startts.toString("yyyyMMddhhmm");
        pstart += "00";
        QString pend = prefer->endts.toString("yyyyMMddhhmm");
        pend += "00";

        QString dstart, dend;

        for (i = dislike->begin(); i != dislike->end(); i++)
        {
            dstart = (*i)->startts.toString("yyyyMMddhhmm");
            dstart += "00";
            dend = (*i)->endts.toString("yyyyMMddhhmm");
            dend += "00";

            thequery = QString("INSERT INTO conflictresolutionsingle "
                               "(preferchanid, preferstarttime, "
                               "preferendtime, dislikechanid, "
                               "dislikestarttime, dislikeendtime) VALUES "
                               "(%1, %2, %3, %4, %5, %6);") 
                               .arg(prefer->chanid).arg(pstart).arg(pend)
                               .arg((*i)->chanid).arg(dstart).arg(dend);

            QSqlQuery qquery = db->exec(thequery);
            if (!qquery.isActive())
            {
                cerr << "DB Error: conflict resolution insertion failed, SQL "
                     << "query was:" << endl;
                cerr << thequery << endl;
            }
        }
    }  

    QString dstart, dend;
    for (i = dislike->begin(); i != dislike->end(); i++)
    {
        dstart = (*i)->startts.toString("yyyyMMddhhmm");
        dstart += "00";
        dend = (*i)->endts.toString("yyyyMMddhhmm");
        dend += "00";

        thequery = QString("DELETE FROM conflictresolutionoverride WHERE "
                           "chanid = %1 AND starttime = %2 AND "
                           "endtime = %3;").arg((*i)->chanid).arg(dstart)
                           .arg(dend);

        QSqlQuery qquery = db->exec(thequery);
        if (!qquery.isActive())
        {
            cerr << "DB Error: conflict resolution deletion failed, SQL query "
                 << "was:" << endl;
            cerr << thequery << endl;
        }
    }

    delete dislike;
    vector<ProgramInfo *>::iterator iter = conflictlist->begin();
    for (; iter != conflictlist->end(); iter++)
    {
        delete (*iter);
    }
    delete conflictlist;

    ScheduledRecording::signalChange(db);

    FillList();
}
