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
#include <map>
#include <vector>
#include <algorithm>
using namespace std;

#include "proglist.h"
#include "scheduledrecording.h"
#include "infodialog.h"
#include "dialogbox.h"
#include "mythcontext.h"
#include "remoteutil.h"

ProgLister::ProgLister(ProgListType pltype, const QString &ltitle,
                       QSqlDatabase *ldb, MythMainWindow *parent,
                       const char *name)
            : MythDialog(parent, name)
{
    type = pltype;
    title = ltitle;
    db = ldb;
    startTime = QDateTime::currentDateTime();
    timeFormat = gContext->GetSetting("ShortDateFormat") +
	" " + gContext->GetSetting("TimeFormat");

    allowEvents = true;
    allowUpdates = true;
    updateAll = false;
    refillAll = false;

    fullRect = QRect(0, 0, size().width(), size().height());
    listRect = QRect(0, 0, 0, 0);
    infoRect = QRect(0, 0, 0, 0);
    theme = new XMLParse();
    theme->SetWMult(wmult);
    theme->SetHMult(hmult);

    if (!theme->LoadTheme(xmldata, "programlist"))
    {
        DialogBox diag(gContext->GetMainWindow(), "The theme you are using "
                       "does not contain a 'programlist' element.  Please "
                       "contact the theme creator and ask if they could "
                       "please update it.<br><br>The next screen will be empty."
                       "  Escape out of it to return to the menu.");
        diag.AddButton("OK");
        diag.exec();

        return;
    }

    LoadWindow(xmldata);

    LayerSet *container = theme->GetSet("selector");
    if (container)
    {
        UIListType *ltype = (UIListType *)container->GetType("proglist");
        if (ltype)
            listsize = ltype->GetItems();
    }
    else
    {
        cerr << "MythFrontEnd: ProgLister - Failed to get selector object.\n";
        exit(0);
    }

    updateBackground();

    curItem = 0;
    itemCount = 0;
    itemList.setAutoDelete(true);
    fillItemList();

    setNoErase();

    gContext->addListener(this);
}

ProgLister::~ProgLister()
{
    gContext->removeListener(this);
    delete theme;
}

void ProgLister::keyPressEvent(QKeyEvent *e)
{
    if (!allowEvents)
        return;

    allowEvents = false;
    bool handled = false;

    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("TV Frontend", e, actions);

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "UP")
            cursorUp(false);
        else if (action == "DOWN")
            cursorDown(false);
        else if (action == "PAGEUP")
            cursorUp(true);
        else if (action == "PAGEDOWN")
            cursorDown(true);
        else if (action == "SELECT" || action == "RIGHT" || action == "LEFT")
            select();
        else if (action == "MENU" || action == "INFO")
            edit();
        else if (action == "TOGGLERECORD")
            quickRecord();
        else
            handled = false;
    }

    if (!handled)
        MythDialog::keyPressEvent(e);

    if (refillAll)
    {
        allowUpdates = false;
        do
        {
            refillAll = false;
            fillItemList();
        } while (refillAll);
        allowUpdates = true;
        update(fullRect);
    }

    allowEvents = true;
}

void ProgLister::LoadWindow(QDomElement &element)
{
    QString name;
    int context;
    QRect area;

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement e = child.toElement();
        if (!e.isNull())
        {
            if (e.tagName() == "font")
                theme->parseFont(e);
            else if (e.tagName() == "container")
	    {
		theme->parseContainer(e, name, context, area);
		if (name.lower() == "selector")
		    listRect = area;
		if (name.lower() == "program_info")
		    infoRect = area;
	    }
            else
            {
                cerr << "Unknown element: " << e.tagName() << endl;
                exit(0);
            }
        }
    }
}

void ProgLister::updateBackground(void)
{
    QPixmap bground(size());
    bground.fill(this, 0, 0);

    QPainter tmp(&bground);

    LayerSet *container = theme->GetSet("background");
    if (container)
	container->Draw(&tmp, 0, 0);

    tmp.end();

    setPaletteBackgroundPixmap(bground);
}

void ProgLister::paintEvent(QPaintEvent *e)
{
    if (!allowUpdates)
    {
        updateAll = true;
        return;
    }

    QRect r = e->rect();
    QPainter p(this);
 
    if (updateAll || r.intersects(listRect))
        updateList(&p);
    if (updateAll || r.intersects(infoRect))
        updateInfo(&p);

    updateAll = false;
}

void ProgLister::cursorDown(bool page)
{
    if (curItem < itemCount - 1)
    {
        curItem += (page ? listsize : 1);
        if (curItem > itemCount - 1)
            curItem = itemCount - 1;
        update(fullRect);
    }
}

void ProgLister::cursorUp(bool page)
{
    if (curItem > 0)
    {
        curItem -= (page ? listsize : 1);
        if (curItem < 0)
            curItem = 0;
        update(fullRect);
    }
}

void ProgLister::quickRecord()
{
    ProgramInfo *pi = itemList.at(curItem);

    if (!pi)
        return;

    pi->ToggleRecord(db);
}

void ProgLister::select()
{
    ProgramInfo *pi = itemList.at(curItem);

    if (!pi)
        return;

    pi->EditRecording(db);
}

void ProgLister::edit()
{
    ProgramInfo *pi = itemList.at(curItem);

    if (!pi)
        return;

    pi->EditScheduled(db);
}

void ProgLister::fillItemList(void)
{
    QString where;

    if (type == plTitle) // per title listings
    {
        where = QString("WHERE program.title = \"%1\" "
                        "AND program.endtime > %2 "
                        "AND program.chanid = channel.chanid "
                        "ORDER BY program.starttime,channel.channum;")
                        .arg(title.utf8())
                        .arg(startTime.toString("yyyyMMddhhmm50"));
    }
    else if (type == plNewListings) // what's new list
    {
        where = QString("LEFT JOIN oldprogram ON title=oldtitle "
                        "WHERE oldtitle IS NULL AND program.endtime > %1 "
                        "AND  program.chanid = channel.chanid "
                        "GROUP BY title ORDER BY starttime LIMIT 500;")
                        .arg(startTime.toString("yyyyMMddhhmm50"));
    }
    else if (type == plTitleSearch) // keyword search
    {
        where = QString("WHERE program.title LIKE \"\%%1\%\" "
                        "AND program.endtime > %2 "
                        "AND program.chanid = channel.chanid "
                        "ORDER BY program.starttime,channel.channum "
                        "LIMIT 500;")
                        .arg(title.utf8())
                        .arg(startTime.toString("yyyyMMddhhmm50"));
    }
    else if (type == plDescSearch) // description search
    {
        where = QString("WHERE (program.title LIKE \"\%%1\%\" "
                        "OR program.subtitle LIKE \"\%%2\%\" "
                        "OR program.description LIKE \"\%%3\%\") "
                        "AND program.endtime > %4 "
                        "AND program.chanid = channel.chanid "
                        "ORDER BY program.starttime,channel.channum "
                        "LIMIT 500;")
                        .arg(title.utf8()).arg(title.utf8()).arg(title.utf8())
                        .arg(startTime.toString("yyyyMMddhhmm50"));
    }
    else if (type == plChannel) // list by channel
    {
        where = QString("WHERE channel.name = \"%1\" "
                        "AND program.endtime > %2 "
                        "AND program.chanid = channel.chanid "
                        "ORDER BY program.starttime;")
                        .arg(title.utf8())
                        .arg(startTime.toString("yyyyMMddhhmm50"));
    }
    else if (type == plCategory) // list by category
    {
        where = QString("WHERE program.category = \"\%1\" "
                        "AND program.endtime > %2 "
                        "AND program.chanid = channel.chanid "
                        "ORDER BY program.starttime,channel.channum "
                        "LIMIT 500;")
                        .arg(title.utf8())
                        .arg(startTime.toString("yyyyMMddhhmm50"));
    }

    itemList.clear();
    ProgramInfo::GetProgramListByQuery(db, &itemList, where);
    itemCount = itemList.count();

    if (curItem >= itemCount - 1)
        curItem = itemCount - 1;

    vector<ProgramInfo *> recList;
    RemoteGetAllPendingRecordings(recList);

    ProgramInfo *pi;

    for (pi = itemList.first(); pi; pi = itemList.next())
        pi->FillInRecordInfo(recList);

    while (!recList.empty())
    {
        pi = recList.back();
        delete pi;
        recList.pop_back();
    }
}

void ProgLister::updateList(QPainter *p)
{
    QRect pr = listRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);

    QString tmptitle;
    
    LayerSet *container = theme->GetSet("selector");
    if (container)
    {
        UIListType *ltype = (UIListType *)container->GetType("proglist");
        if (ltype)
        {
            ltype->ResetList();
            ltype->SetActive(true);

            int skip;
            if (itemCount <= listsize || curItem <= listsize / 2)
                skip = 0;
            else if (curItem >= itemCount - listsize + listsize / 2)
                skip = itemCount - listsize;
            else
                skip = curItem - listsize / 2;
            ltype->SetUpArrow(skip > 0);
            ltype->SetDownArrow(skip + listsize < itemCount);

            int i;
            for (i = 0; i < listsize; i++)
            {
                if (i + skip >= itemCount)
                    break;

                ProgramInfo *pi = itemList.at(i+skip);

                ltype->SetItemText(i, 1, pi->startts.toString(timeFormat));
                ltype->SetItemText(i, 2, pi->chanstr + " " + pi->chansign);

                if (pi->subtitle == "")
                    tmptitle = pi->title;
                else
                {
                    if (type == plTitle)
                        tmptitle = pi->subtitle;
                    else
                        tmptitle = QString("%1 - \"%2\"")
                                           .arg(pi->title)
                                           .arg(pi->subtitle);
                }

                ltype->SetItemText(i, 3, tmptitle);

                if (pi->conflicting)
                    ltype->EnableForcedFont(i, "conflicting");
                else if (pi->recording)
                    ltype->EnableForcedFont(i, "recording");

                if (i + skip == curItem)
                    ltype->SetItemCurrent(i);
            }
        }
    }

    if (itemCount == 0)
        container = theme->GetSet("noprograms_list");

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

void ProgLister::updateInfo(QPainter *p)
{
    QRect pr = infoRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);

    LayerSet *container = NULL;
    ProgramInfo *pi = itemList.at(curItem);

    if (pi)
    {
        container = theme->GetSet("program_info");
        if (container)
        {  
            QMap<QString, QString> regexpMap;
            pi->ToMap(db, regexpMap);
            container->ClearAllText();
            container->SetTextByRegexp(regexpMap);
        }
    }
    else
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

void ProgLister::customEvent(QCustomEvent *e)
{
    if ((MythEvent::Type)(e->type()) != MythEvent::MythEventMessage)
        return;

    MythEvent *me = (MythEvent *)e;
    QString message = me->Message();
    if (message != "SCHEDULE_CHANGE")
        return;

    refillAll = true;

    if (!allowEvents)
        return;

    allowEvents = false;

    allowUpdates = false;
    do
    {
        refillAll = false;
        fillItemList();
    } while (refillAll);
    allowUpdates = true;
    update(fullRect);

    allowEvents = true;
}

