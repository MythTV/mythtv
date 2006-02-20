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

#include "viewschdiff.h"
#include "scheduledrecording.h"
#include "proglist.h"
#include "tv.h"

#include "exitcodes.h"
#include "dialogbox.h"
#include "mythcontext.h"
#include "remoteutil.h"

ViewScheduleDiff::ViewScheduleDiff(MythMainWindow *parent, const char *name, QString altTbl, int recordidDiff, QString ltitle)
             : MythDialog(parent, name)
{
    dateformat = gContext->GetSetting("ShortDateFormat", "M/d");
    timeformat = gContext->GetSetting("TimeFormat", "h:mm AP");
    channelFormat = gContext->GetSetting("ChannelFormat", "<num> <sign>");

    altTable = altTbl;
    recordid = recordidDiff;
    m_title = ltitle;

    fullRect = QRect(0, 0, size().width(), size().height());
    listRect = QRect(0, 0, 0, 0);
    infoRect = QRect(0, 0, 0, 0);
    showLevelRect = QRect(0, 0, 0, 0);
    recStatusRect = QRect(0, 0, 0, 0);

    theme = new XMLParse();
    theme->SetWMult(wmult);
    theme->SetHMult(hmult);
    if (!theme->LoadTheme(xmldata, "schdiff"))
    {
        DialogBox diag(gContext->GetMainWindow(), tr("The theme you are using "
                       "does not contain a 'schdiff' element. Please contact "
                       "the theme creator and ask if they could please update "
                       "it.<br><br>The next screen will be empty. "
                       "Escape out of it to return to the menu."));
        diag.AddButton(tr("OK"));
        diag.exec();

        return;
    }

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
        VERBOSE(VB_IMPORTANT, "ViewScheduleDiff::ViewScheduleDiff(): "
                "Failed to get selector object.");
        exit(FRONTEND_BUGGY_EXIT_NO_SELECTOR);
    }
    container = theme->GetSet("background");
    if (container)
    {
        UITextType *type = (UITextType *)container->GetType("view");
        if (type)
            type->SetText(m_title);
    }

    updateBackground();

    inEvent = false;
    inFill = false;
    needFill = false;

    listPos = 0;
    FillList();

    setNoErase();

    gContext->addListener(this);

}

ViewScheduleDiff::~ViewScheduleDiff()
{
    gContext->removeListener(this);
    delete theme;
}

void ViewScheduleDiff::keyPressEvent(QKeyEvent *e)
{
    if (inEvent)
        return;

    inEvent = true;

    bool handled = false;
    QStringList actions;
    if (gContext->GetMainWindow()->TranslateKeyPress("TV Frontend", e, actions))
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;

            if (action == "ESCAPE" || action == "LEFT")
                done(MythDialog::Accepted);
            else if (action == "UP")
                cursorUp();
            else if (action == "DOWN")
                cursorDown();
            else if (action == "PAGEUP")
                pageUp();
            else if (action == "PAGEDOWN")
                pageDown();
            else if (action == "INFO")
                edit();
            else if (action == "UPCOMING")
                upcoming();
            else if (action == "DETAILS")
                details();
            else if (action == "SELECT")
                statusDialog();
            else
                handled = false;
        }
    }

    if (!handled)
        MythDialog::keyPressEvent(e);

    inEvent = false;
}

void ViewScheduleDiff::LoadWindow(QDomElement &element)
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
                VERBOSE(VB_IMPORTANT,
                        QString("ViewScheduleDiff: Unknown child element: %1. "
                                "Ignoring.").arg(e.tagName()));
            }
        }
    }
}

void ViewScheduleDiff::parseContainer(QDomElement &element)
{
    QRect area;
    QString name;
    int context;
    theme->parseContainer(element, name, context, area);

    if (name.lower() == "selector")
        listRect = area;
    if (name.lower() == "program_info")
        infoRect = area;
    if (name.lower() == "showlevel_info")
        showLevelRect = area;
    if (name.lower() == "status_info")
        recStatusRect = area;
}

void ViewScheduleDiff::updateBackground(void)
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

void ViewScheduleDiff::paintEvent(QPaintEvent *e)
{
    if (inFill)
        return;

    QRect r = e->rect();
    QPainter p(this);

    if (r.intersects(listRect))
        updateList(&p);
    if (r.intersects(infoRect))
        updateInfo(&p);
    if (r.intersects(showLevelRect))
        updateShowLevel(&p);
    if (r.intersects(recStatusRect))
        updateRecStatus(&p);
}

void ViewScheduleDiff::cursorDown(bool page)
{
    if (recList.count() == 0) return;
    if (listPos < recList.count() - 1)
    {
        listPos += (page ? listsize : 1);
        if (listPos > recList.count() - 1)
            listPos = recList.count() - 1;
        update(fullRect);
    }
}

void ViewScheduleDiff::cursorUp(bool page)
{
    if (listPos > 0)
    {
        unsigned int move = (page ? listsize : 1);
        if (move > listPos) listPos = 0;
        else listPos -= move;
        update(fullRect);
    }
}

void ViewScheduleDiff::edit()
{
    ProgramInfo *pi = CurrentProgram();;

    if (!pi)
        return;

    pi->EditScheduled();
}

void ViewScheduleDiff::upcoming()
{
    ProgramInfo *pi = CurrentProgram();

    ProgLister *pl = new ProgLister(plTitle, pi->title, "",
                                   gContext->GetMainWindow(), "proglist");
    pl->exec();
    delete pl;
}

void ViewScheduleDiff::details()
{
    ProgramInfo *pi = CurrentProgram();

    if (pi)
        pi->showDetails();
}

void ViewScheduleDiff::statusDialog()
{
    ProgramInfo *pi = CurrentProgram();
    if (!pi)
        return;

    QString timeFormat = gContext->GetSetting("TimeFormat", "h:mm AP");

    QString message = pi->title;

    if (pi->subtitle != "")
        message += QString(" - \"%1\"").arg(pi->subtitle);

    message += "\n\n";
    message += pi->RecStatusDesc();

    if (pi->recstatus == rsConflict || pi->recstatus == rsLaterShowing)
    {
        message += " " + QObject::tr("The following programs will be recorded "
                                     "instead:") + "\n\n";
        ProgramInfo *pa = recListAfter.first();
        while (pa)
        {
            if (pa->recstartts >= pi->recendts)
                break;
            if (pa->recendts > pi->recstartts &&
                (pa->recstatus == rsWillRecord ||
                 pa->recstatus == rsRecording))
            {
                message += QString("%1 - %2  %3")
                    .arg(pa->recstartts.toString(timeFormat))
                    .arg(pa->recendts.toString(timeFormat)).arg(pa->title);
                if (pa->subtitle != "")
                    message += QString(" - \"%1\"").arg(pa->subtitle);
                message += "\n";
            }
            pa = recListAfter.next();
        }
    }
    DialogBox diag(gContext->GetMainWindow(), message);
    diag.AddButton(QObject::tr("OK"));
    diag.exec();
    return;
}

static int comp_recstart(ProgramInfo *a, ProgramInfo *b)
{
    if (a->recstartts != b->recstartts)
    {
        if (a->recstartts > b->recstartts)
            return 1;
        else
            return -1;
    }
    if (a->recendts != b->recendts)
    {
        if (a->recendts > b->recendts)
            return 1;
        else
            return -1;
    }
    if (a->chansign != b->chansign)
    {
        if (a->chansign < b->chansign)
            return 1;
        else
            return -1;
    }
    return 0;
}

void ViewScheduleDiff::FillList(void)
{
    inFill = true;

    QString callsign;
    QDateTime startts, recstartts;

    if (listPos < recList.count())
    {
        struct ProgramStruct s = recList[listPos];
        ProgramInfo *p = s.after;
        if (!p) p = s.before;

        if (p) {
            callsign = p->chansign;
            startts = p->startts;
            recstartts = p->recstartts;
        }
    }

    recListBefore.FromScheduler(conflictBool);
    recListAfter.FromScheduler(conflictBool, altTable, recordid);

    recListBefore.Sort(comp_recstart);
    recListAfter.Sort(comp_recstart);

    QDateTime now = QDateTime::currentDateTime();

    ProgramInfo *p = recListBefore.first();
    while (p)
    {
        if (p->recendts >= now || p->endts >= now)
            p = recListBefore.next();
        else
        {
            recListBefore.remove();
            p = recListBefore.current();
        }
    }

    p = recListAfter.first();
    while (p)
    {
        if (p->recendts >= now || p->endts >= now)
            p = recListAfter.next();
        else
        {
            recListAfter.remove();
            p = recListAfter.current();
        }
    }

    ProgramInfo *pb = recListBefore.first();
    ProgramInfo *pa = recListAfter.first();
    ProgramStruct s;

    recList.clear();
    while (pa || pb) {
           s.before = pb;
           s.after = pa;

           if (!pa) {
                pb = recListBefore.next();
           } else if (!pb) {
               pa = recListAfter.next();
           } else switch (comp_recstart(pb, pa)) {
               case 0:
                   pb = recListBefore.next();
                   pa = recListAfter.next();
                   break;
               case -1: // pb BEFORE pa
                   pb = recListBefore.next();
                   s.after = NULL;
                   break;
               case 1: // pa BEFORE pb
                   s.before = NULL;
                   pa = recListAfter.next();
                   break;
           }
           if (s.before && s.after && (s.before->cardid == s.after->cardid) &&
               (s.before->recstatus == s.after->recstatus))
           {
               continue;
           }
           recList.push_back(s);
    }

    if (!callsign.isNull())
    {
        listPos = recList.count() - 1;
        int i;
        for (i = listPos; i >= 0; i--)
        {
            struct ProgramStruct s = recList[listPos];
            ProgramInfo *p = s.after;
            if (!p) p = s.before;

            if (callsign == p->chansign &&
                startts == p->startts)
            {
                listPos = i;
                break;
            }
            else if (recstartts <= p->recstartts)
                listPos = i;
        }
    }

    inFill = false;
}

void ViewScheduleDiff::updateList(QPainter *p)
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

            int listCount = (int)recList.count();

            int skip;
            if (listCount <= listsize || (int)listPos <= listsize / 2)
                skip = 0;
            else if ((int)listPos >= listCount - listsize + listsize / 2)
                skip = listCount - listsize;
            else
                skip = (int)listPos - listsize / 2;

            ltype->SetUpArrow(skip > 0);
            ltype->SetDownArrow(skip + listsize < listCount);

            int i;
            for (i = 0; i < listsize; i++)
            {
                if (i + skip >= listCount)
                    break;

                struct ProgramStruct s = recList[skip+i];
                struct ProgramInfo *p = s.after;
                if (!p) p = s.before;

                QString temp;

                temp = (p->recstartts).toString(dateformat);
                temp += " " + (p->recstartts).toString(timeformat);
                ltype->SetItemText(i, 1, temp);

                ltype->SetItemText(i, 2, p->ChannelText(channelFormat));

                temp = p->title;
                if ((p->subtitle).stripWhiteSpace().length() > 0)
                    temp += " - \"" + p->subtitle + "\"";
                ltype->SetItemText(i, 3, temp);

                if (s.before) temp = s.before->RecStatusChar();
                else temp = "-";
                ltype->SetItemText(i, 4, temp);

                if (s.after) temp = s.after->RecStatusChar();
                else temp = "-";
                ltype->SetItemText(i, 5, temp);

                if (i + skip == (int)listPos)
                    ltype->SetItemCurrent(i);

                if (!s.after)
                    ltype->EnableForcedFont(i, "disabledrecording");
                else if (p->recstatus == rsRecording)
                    ltype->EnableForcedFont(i, "recording");
                else if (p->recstatus == rsConflict ||
                         p->recstatus == rsOffLine ||
                         p->recstatus == rsAborted)
                    ltype->EnableForcedFont(i, "conflictingrecording");
                else if (p->recstatus == rsWillRecord)
                    ltype->EnableForcedFont(i, "record");
                else if (p->recstatus == rsRepeat ||
                         (p->recstatus != rsDontRecord &&
                          p->recstatus <= rsEarlierShowing))
                    ltype->EnableForcedFont(i, "disabledrecording"); 
            }
        }
    }

    if (recList.count() == 0)
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

void ViewScheduleDiff::updateShowLevel(QPainter *p)
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
            type->SetText(tr("All"));
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

void ViewScheduleDiff::updateInfo(QPainter *p)
{
    QRect pr = infoRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);
    QMap<QString, QString> infoMap;

    LayerSet *container = theme->GetSet("program_info");
    if (container)
    {
        ProgramInfo *p = CurrentProgram();

        if (p)
        {
            p->ToMap(infoMap);
            container->ClearAllText();
            container->SetText(infoMap);
        }
    }
    if (recList.count() == 0)
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

void ViewScheduleDiff::updateRecStatus(QPainter *p)
{
    QRect pr = recStatusRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);
    QMap<QString, QString> infoMap;

    LayerSet *container = theme->GetSet("status_info");
    if (container)
    {
        ProgramInfo *p = CurrentProgram();
        if (p)
        {
            p->ToMap(infoMap);
            container->ClearAllText();
            container->SetText(infoMap);
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

ProgramInfo *ViewScheduleDiff::CurrentProgram() {
    if (listPos >= recList.count())
        return NULL;
    ProgramStruct s = recList[listPos];
    if (s.after) return s.after;
    else return s.before;
}
