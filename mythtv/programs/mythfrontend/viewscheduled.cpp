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
            case Key_I: case Key_M: edit(); return;
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
        if (p->recording || showLevel == showAll ||
            (showLevel == showImportant && p->norecord > nrOtherShowing))
        {
            conflictData[cntStr] = *p;
            dataCount++;
            cnt--;
        }
        delete p;
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
                            tempCard = tempInfo->NoRecordChar();

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
    QMap<QString, QString> regexpMap;

    if (conflictData.count() > 0 && curitem)
    {
        QSqlDatabase *m_db = QSqlDatabase::database();
        curitem->ToMap(m_db, regexpMap);

        LayerSet *container = NULL;
        container = theme->GetSet("program_info");
        if (container)
        {
            container->ClearAllText();
            container->SetTextByRegexp(regexpMap);

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

    allowKeys = false;
    curitem->EditScheduled(db);
    allowKeys = true;

    FillList();
    update(fullRect);

    doingSel = false;
}

void ViewScheduled::selected()
{
    if (doingSel || !curitem)
        return;

    doingSel = true;

    allowKeys = false;
    curitem->EditRecording(db);
    allowKeys = true;

    FillList();
    update(fullRect);

    doingSel = false;
}

