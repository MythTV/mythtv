// -*- Mode: c++ -*-

#include <QLayout>
#include <QPushButton>
#include <QLabel>
#include <QCursor>
#include <QDateTime>
#include <QApplication>
#include <QRegExp>
#include <QPaintEvent>
#include <QPixmap>
#include <QKeyEvent>
#include <QPainter>

#include "viewschdiff.h"
#include "scheduledrecording.h"
#include "tv.h"

#include "exitcodes.h"
#include "dialogbox.h"
#include "mythcontext.h"
#include "mythverbose.h"
#include "remoteutil.h"
#include "recordinginfo.h"

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
        DialogBox *dlg = new DialogBox(
            gContext->GetMainWindow(),
            QObject::tr(
                "The theme you are using does not contain the "
                "%1 element. Please contact the theme creator "
                "and ask if they could please update it.<br><br>"
                "The next screen will be empty. "
                "Escape out of it to return to the menu.")
            .arg("'schdiff'"));

        dlg->AddButton(tr("OK"));
        dlg->exec();
        dlg->deleteLater();

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

    handled = gContext->GetMainWindow()->TranslateKeyPress("TV Frontend", e, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
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
//         else if (action == "INFO")
//             edit();
//         else if (action == "UPCOMING")
//             upcoming();
//         else if (action == "DETAILS")
//             details();
        else if (action == "SELECT")
            statusDialog();
        else
            handled = false;
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

    if (name.toLower() == "selector")
        listRect = area;
    if (name.toLower() == "program_info")
        infoRect = area;
    if (name.toLower() == "showlevel_info")
        showLevelRect = area;
    if (name.toLower() == "status_info")
        recStatusRect = area;
}

void ViewScheduleDiff::updateBackground(void)
{
    QPixmap bground(size());
    bground.fill(this, 0, 0);

    QPainter tmp(&bground);

    LayerSet *container = theme->GetSet("background");
    if (!container)
        return;

    container->Draw(&tmp, 0, 0);

    tmp.end();
    myBackground = bground;

    QPalette p = palette();
    p.setBrush(backgroundRole(), QBrush(myBackground));
    setPalette(p);
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
    if (recList.empty())
        return;

    if (listPos < (int)recList.size() - 1)
    {
        listPos += (page ? listsize : 1);
        if (listPos > (int)recList.size() - 1)
            listPos = (int)recList.size() - 1;
        update(fullRect);
    }
}

void ViewScheduleDiff::cursorUp(bool page)
{
    if (listPos > 0)
    {
        int move = (page ? listsize : 1);
        if (move > listPos) listPos = 0;
        else listPos -= move;
        update(fullRect);
    }
}

void ViewScheduleDiff::edit()
{
//     ProgramInfo *pi = CurrentProgram();
// 
//     if (!pi)
//         return;
// 
//     RecordingInfo ri(*pi);
//     ri.EditScheduled();
//     *pi = ri;
}

void ViewScheduleDiff::upcoming()
{
//     ProgramInfo *pi = CurrentProgram();
//     if (!pi)
//         return;
// 
//     ProgListerQt *pl = new ProgListerQt(plTitle, pi->title, "",
//                                    gContext->GetMainWindow(), "proglist");
//     pl->exec();
//     delete pl;
}

void ViewScheduleDiff::details()
{
//     ProgramInfo *pi = CurrentProgram();
// 
//     if (pi)
//     {
//         const RecordingInfo ri(*pi);
//         ri.showDetails();
//     }
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

        ProgramList::const_iterator it = recListAfter.begin();
        for (; it != recListAfter.end(); ++it)
        {
            const ProgramInfo *pa = *it;
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
        }
    }

    DialogBox *dlg = new DialogBox(gContext->GetMainWindow(), message);
    dlg->AddButton(QObject::tr("OK"));
    dlg->exec();
    dlg->deleteLater();

    return;
}

static int comp_recstart(const ProgramInfo *a, const ProgramInfo *b)
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
    if (a->recpriority != b->recpriority &&
        (a->recstatus == rsWillRecord || b->recstatus == rsWillRecord))
    {
        if (a->recpriority < b->recpriority)
            return 1;
        else
            return -1;
    }
    return 0;
}

static bool comp_recstart_less_than(const ProgramInfo *a, const ProgramInfo *b)
{
    return comp_recstart(a,b) < 0;
}

void ViewScheduleDiff::FillList(void)
{
    inFill = true;

    QString callsign;
    QDateTime startts, recstartts;

    if (listPos < (int)recList.size())
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

    recListBefore.sort(comp_recstart_less_than);
    recListAfter.sort(comp_recstart_less_than);

    QDateTime now = QDateTime::currentDateTime();

    ProgramList::iterator it = recListBefore.begin();
    while (it != recListBefore.end())
    {
        if ((*it)->recendts >= now || (*it)->endts >= now)
        {
            ++it;
        }
        else
        {
            it = recListBefore.erase(it);
        }
    }

    it = recListAfter.begin();
    while (it != recListAfter.end())
    {
        if ((*it)->recendts >= now || (*it)->endts >= now)
        {
            ++it;
        }
        else
        {
            it = recListAfter.erase(it);
        }
    }

    ProgramList::iterator pb = recListBefore.begin();
    ProgramList::iterator pa = recListAfter.begin();
    ProgramStruct s;

    recList.clear();
    while (pa != recListAfter.end() || pb != recListBefore.end())
    {
        s.before = (pb != recListBefore.end()) ? *pb : NULL;
        s.after  = (pa != recListAfter.end())  ? *pa : NULL;

        if (pa == recListAfter.end())
        {
            ++pb;
        }
        else if (pb == recListBefore.end())
        {
            ++pa;
        }
        else
        {
            switch (comp_recstart(*pb, *pa))
            {
                case 0:
                    ++pb;
                    ++pa;
                    break;
                case -1: // pb BEFORE pa
                    ++pb;
                    s.after = NULL;
                    break;
                case 1: // pa BEFORE pb
                    s.before = NULL;
                    ++pa;
                    break;
            }
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
        listPos = recList.size() - 1;
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

            int listCount = (int)recList.size();

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
                struct ProgramInfo *pginfo = s.after;
                if (!pginfo)
                    pginfo = s.before;

                QString temp;

                temp = (pginfo->recstartts).toString(dateformat);
                temp += " " + (pginfo->recstartts).toString(timeformat);
                ltype->SetItemText(i, 1, temp);

                ltype->SetItemText(i, 2, pginfo->ChannelText(channelFormat));

                temp = pginfo->title;
                if ((pginfo->subtitle).trimmed().length() > 0)
                    temp += " - \"" + pginfo->subtitle + "\"";
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
                else if (pginfo->recstatus == rsRecording)
                    ltype->EnableForcedFont(i, "recording");
                else if (pginfo->recstatus == rsConflict ||
                         pginfo->recstatus == rsOffLine ||
                         pginfo->recstatus == rsAborted)
                    ltype->EnableForcedFont(i, "conflictingrecording");
                else if (pginfo->recstatus == rsWillRecord)
                    ltype->EnableForcedFont(i, "record");
                else if (pginfo->recstatus == rsRepeat ||
                         pginfo->recstatus == rsOtherShowing ||
                         (pginfo->recstatus != rsDontRecord &&
                          pginfo->recstatus <= rsEarlierShowing))
                    ltype->EnableForcedFont(i, "disabledrecording"); 
            }
        }
    }

    if (recList.empty())
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
    InfoMap infoMap;

    LayerSet *container = theme->GetSet("program_info");
    if (container)
    {
        ProgramInfo *pginfo = CurrentProgram();

        if (pginfo)
        {
            pginfo->ToMap(infoMap);
            container->ClearAllText();
            container->SetText(infoMap);
        }
    }
    if (recList.empty())
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
    InfoMap infoMap;

    LayerSet *container = theme->GetSet("status_info");
    if (container)
    {
        ProgramInfo *pginfo = CurrentProgram();
        if (pginfo)
        {
            pginfo->ToMap(infoMap);
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
    if (listPos >= (int)recList.size())
        return NULL;
    ProgramStruct s = recList[listPos];
    if (s.after) return s.after;
    else return s.before;
}
