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

    dateformat = gContext->GetSetting("ShortDateFormat", "M/d");
    timeformat = gContext->GetSetting("TimeFormat", "h:mm AP");
    displayChanNum = gContext->GetNumSetting("DisplayChanNum");
    showAll = !gContext->GetNumSetting("ViewSchedShowLevel", 0);

    fullRect = QRect(0, 0, size().width(), size().height());
    listRect = QRect(0, 0, 0, 0);
    infoRect = QRect(0, 0, 0, 0);
    conflictRect = QRect(0, 0, 0, 0);
    showLevelRect = QRect(0, 0, 0, 0);

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
            listsize = ltype->GetItems();
    }
    else
    {
        cerr << "MythFrontEnd: ViewSchedule - Failed to get selector object.\n";
        exit(0);
    }

    updateBackground();

    curRec = 0;
    recCount = 0;
    recList.setAutoDelete(true);
    FillList();
 
    setNoErase();

    gContext->addListener(this);
}

ViewScheduled::~ViewScheduled()
{
    gContext->SaveSetting("ViewSchedShowLevel", !showAll);
    gContext->removeListener(this);
    delete theme;
}

void ViewScheduled::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    if (gContext->GetMainWindow()->TranslateKeyPress("TV Frontend", e, actions))
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;

            if (action == "SELECT")
                selected();
            else if (action == "MENU" || action == "INFO")
                edit();
            else if (action == "ESCAPE")
                done(MythDialog::Accepted);
            else if (action == "UP")
                cursorUp();
            else if (action == "DOWN")
                cursorDown();
            else if (action == "PAGEUP")
                pageUp();
            else if (action == "PAGEDOWN")
                pageDown();
            else if (action == "1")
                setShowAll(true);
            else if (action == "2")
                setShowAll(false);
            else
                handled = false;
        }
    }

    if (!handled)
        MythDialog::keyPressEvent(e);
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
                theme->parseFont(e);
            else if (e.tagName() == "container")
                parseContainer(e);
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
    QRect r = e->rect();
    QPainter p(this);
 
    if (r.intersects(listRect))
        updateList(&p);
    if (r.intersects(infoRect))
        updateInfo(&p);
    if (r.intersects(conflictRect))
        updateConflict(&p);
    if (r.intersects(showLevelRect))
        updateShowLevel(&p);
}

void ViewScheduled::cursorDown(bool page)
{
    if (curRec < recCount - 1)
    {
        curRec += (page ? listsize : 1);
        if (curRec > recCount - 1)
            curRec = recCount - 1;
        update(fullRect);
    }
}

void ViewScheduled::cursorUp(bool page)
{
    if (curRec > 0)
    {
        curRec -= (page ? listsize : 1);
        if (curRec < 0)
            curRec = 0;
        update(fullRect);
    }
}

void ViewScheduled::FillList(void)
{
    ProgramInfo *oldInfo = recList.at(curRec);
    if (oldInfo)
        oldInfo = new ProgramInfo(*oldInfo);

    recList.clear();

    vector<ProgramInfo *> schedList;
    conflictBool = RemoteGetAllPendingRecordings(schedList);

    vector<ProgramInfo *>::iterator i;

    for (i = schedList.begin(); i != schedList.end(); i++)
    {
        ProgramInfo *p = *i;
        if (p->recording || showAll || p->norecord > nrOtherShowing)
            recList.append(p);
        else
            delete p;
    }

    recCount = (int)recList.count();

    // The list has changed -- new entries could have been added and
    // old entries (including the old curRec) could have been deleted.
    // Try to reset curRec to the same entry it was on before.  Else,
    // try to set it to the next later entry.  Else, set it to the
    // last entry.  FIXME: optimize this (how?).
    if (!oldInfo)
        curRec = 0;
    else
    {
        curRec = recCount - 1;
        ProgramInfo *p;
        for (p = recList.last(); p; p = recList.prev())
        {
            if (p->recordid == oldInfo->recordid &&
                p->chanid == oldInfo->chanid &&
                p->startts == oldInfo->startts)
            {
                curRec = recList.at();
                break;
            }
            else if (p->recstartts >= oldInfo->recstartts)
                curRec = recList.at();
        }
    }

    if (oldInfo)
        delete oldInfo;
}

void ViewScheduled::updateList(QPainter *p)
{
    QRect pr = listRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);
    
    LayerSet *container = theme->GetSet("selector");
    if (container)
    {
        UIListType *ltype = (UIListType *)container->GetType("conflictlist");
        if (ltype)
        {
            ltype->ResetList();
            ltype->SetActive(true);

            int skip;
            if (recCount <= listsize || curRec <= listsize / 2)
                skip = 0;
            else if (curRec >= recCount - listsize + listsize / 2)
                skip = recCount - listsize;
            else
                skip = curRec - listsize / 2;
            ltype->SetUpArrow(skip > 0);
            ltype->SetDownArrow(skip + listsize < recCount);

            int i;
            for (i = 0; i < listsize; i++)
            {
                if (i + skip >= recCount)
                    break;

                ProgramInfo *p = recList.at(i+skip);

                QString temp;

                if (displayChanNum)
                    temp = p->chansign;
                else
                    temp = p->chanstr;
                ltype->SetItemText(i, 1, temp);

                temp = (p->recstartts).toString(dateformat);
                temp += " " + (p->recstartts).toString(timeformat);
                ltype->SetItemText(i, 2, temp);

                temp = p->title;
                if ((p->subtitle).stripWhiteSpace().length() > 0)
                    temp += " - \"" + p->subtitle + "\"";
                ltype->SetItemText(i, 3, temp);

                if (p->recording)
                    temp = QString::number(p->cardid);
                else
                    temp = p->NoRecordChar();
                ltype->SetItemText(i, 4, temp);

                if (i + skip == curRec)
                    ltype->SetItemCurrent(i);

                if (p->conflicting)
                    ltype->EnableForcedFont(i, "conflictingrecording");
                else if (!p->recording)
                    ltype->EnableForcedFont(i, "disabledrecording");
            }
        }
    }

    if (recCount == 0)
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

    LayerSet *container = theme->GetSet("conflict_info");
    if (container)
    {
        UITextType *type = (UITextType *)container->GetType("status");
        if (type)
        {
            if (conflictBool)
                type->SetText(tr("Time Conflict"));
            else
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

    LayerSet *container = theme->GetSet("showlevel_info");
    if (container)
    {
        UITextType *type = (UITextType *)container->GetType("showlevel");
        if (type)
        {
            if (showAll)
                type->SetText(tr("All"));
            else
                type->SetText(tr("Important"));
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

    LayerSet *container = theme->GetSet("program_info");
    if (container)
    {
        ProgramInfo *p = recList.at(curRec);
        if (p)
        {
            QSqlDatabase *m_db = QSqlDatabase::database();
            p->ToMap(m_db, regexpMap);
            container->ClearAllText();
            container->SetTextByRegexp(regexpMap);
        }
    }

    if (recCount == 0)
        container = theme->GetSet("norecordings_info");

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

void ViewScheduled::edit()
{
    ProgramInfo *curitem = recList.at(curRec);

    if (!curitem)
        return;

    curitem->EditScheduled(db);
}

void ViewScheduled::selected()
{
    ProgramInfo *curitem = recList.at(curRec);

    if (!curitem)
        return;

    curitem->EditRecording(db);
}

void ViewScheduled::customEvent(QCustomEvent *e)
{
    if ((MythEvent::Type)(e->type()) != MythEvent::MythEventMessage)
        return;

    MythEvent *me = (MythEvent *)e;
    QString message = me->Message();
    if (message != "SCHEDULE_CHANGE")
        return;

    FillList();
    update(fullRect);
}

void ViewScheduled::setShowAll(bool all)
{
    showAll = all;

    FillList();
    update(fullRect);
}
