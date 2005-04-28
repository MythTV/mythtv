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
#include <cassert>
using namespace std;

#include "previousbox.h"
#include "proglist.h"
#include "scheduledrecording.h"
#include "dialogbox.h"
#include "mythcontext.h"
#include "mythdbcon.h"
#include "remoteutil.h"

PreviousBox::PreviousBox(MythMainWindow *parent, const char *name)
            : MythDialog(parent, name)
{
    view = "";
    startTime = QDateTime::currentDateTime();
    searchTime = startTime;

    dayFormat = gContext->GetSetting("DateFormat");
    hourFormat = gContext->GetSetting("TimeFormat");
    timeFormat = gContext->GetSetting("ShortDateFormat") + " " + hourFormat;
    fullDateFormat = dayFormat + " " + hourFormat;
    channelOrdering = gContext->GetSetting("ChannelOrdering", "channum + 0");
    channelFormat = gContext->GetSetting("ChannelFormat", "<num> <sign>");

    allowEvents = true;
    allowUpdates = true;
    updateAll = false;
    refillAll = false;

    fullRect = QRect(0, 0, size().width(), size().height());
    viewRect = QRect(0, 0, 0, 0);
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
    assert(container);
    UIListType *ltype = (UIListType *)container->GetType("proglist");
    if (ltype)
        listsize = ltype->GetItems();

    choosePopup = NULL;
    chooseListBox = NULL;
    chooseLineEdit = NULL;
    chooseOkButton = NULL;
    chooseDeleteButton = NULL;
    chooseRecordButton = NULL;
    chooseDay = NULL;
    chooseHour = NULL;

    curView = -1;
    fillViewList(view);

    curItem = -1;
    fillItemList();

    if (curView < 0)
        QApplication::postEvent(this, new MythEvent("CHOOSE_VIEW"));

    updateBackground();

    setNoErase();

    gContext->addListener(this);
}

PreviousBox::~PreviousBox()
{
    itemList.clear();

    gContext->removeListener(this);
    delete theme;
}

void PreviousBox::keyPressEvent(QKeyEvent *e)
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
        else if (action == "PREVVIEW")
            prevView();
        else if (action == "NEXTVIEW")
            nextView();
        else if (action == "MENU")
            chooseView();
        else if (action == "SELECT" || action == "RIGHT")
            select();
        else if (action == "DELETE")
            deleteItem();
        else if (action == "LEFT")
            accept();
        else if (action == "INFO")
            edit();
        else if (action == "UPCOMING")
            upcoming();
        else if (action == "DETAILS")
            details();
        else if (action == "1")
        {
            if (viewList[curView] == "sort by time")
                curView = viewList.findIndex("reverse time");
            else
                curView = viewList.findIndex("sort by time");

            refillAll = true;
        }
        else if (action == "2")
        {
            if (viewList[curView] == "sort by title")
                curView = viewList.findIndex("reverse title");
            else
                curView = viewList.findIndex("sort by title");

            refillAll = true;
        }
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

void PreviousBox::LoadWindow(QDomElement &element)
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
                if (name.lower() == "view")
                    viewRect = area;
                if (name.lower() == "selector")
                    listRect = area;
                if (name.lower() == "program_info")
                    infoRect = area;
            }
            else
            {
                VERBOSE(VB_IMPORTANT, QString("Unknown element: %1, exiting").arg(e.tagName()));
                exit(-17);
            }
        }
    }
}

void PreviousBox::updateBackground(void)
{
    QPixmap bground(size());
    bground.fill(this, 0, 0);

    QPainter tmp(&bground);

    LayerSet *container = theme->GetSet("background");
    if (container)
    {
        UITextType *ltype = (UITextType *)container->GetType("sched");
        if (ltype)
        {
            QString value = tr("Previously Recorded");
            ltype->SetText(value);
        }
        container->Draw(&tmp, 0, 0);
    }

    tmp.end();

    setPaletteBackgroundPixmap(bground);
}

void PreviousBox::paintEvent(QPaintEvent *e)
{
    if (!allowUpdates)
    {
        updateAll = true;
        return;
    }

    QRect r = e->rect();
    QPainter p(this);
 
    if (updateAll || r.intersects(viewRect))
        updateView(&p);
    if (updateAll || r.intersects(listRect))
        updateList(&p);
    if (updateAll || r.intersects(infoRect))
        updateInfo(&p);

    updateAll = false;
}

void PreviousBox::cursorDown(bool page)
{
    if (curItem < (int)itemList.count() - 1)
    {
        curItem += (page ? listsize : 1);
        if (curItem > (int)itemList.count() - 1)
            curItem = itemList.count() - 1;
        update(fullRect);
    }
}

void PreviousBox::cursorUp(bool page)
{
    if (curItem > 0)
    {
        curItem -= (page ? listsize : 1);
        if (curItem < 0)
            curItem = 0;
        update(fullRect);
    }
}

void PreviousBox::prevView(void)
{
    if (viewList.count() < 2)
        return;

    curView--;
    if (curView < 0)
        curView = viewList.count() - 1;

    curItem = -1;
    refillAll = true;
}

void PreviousBox::nextView(void)
{
    if (viewList.count() < 2)
        return;

    curView++;
    if (curView >= (int)viewList.count())
        curView = 0;

    curItem = -1;
    refillAll = true;
}

void PreviousBox::setViewFromList(void)
{
    if (!choosePopup || !chooseListBox)
        return;

    int view = chooseListBox->currentItem();

    choosePopup->done(0);

    if (view == curView)
        return;

    curView = view;

    curItem = -1;
    refillAll = true;
}

void PreviousBox::chooseView(void)
{
    if (viewList.count() < 2)
        return;

    choosePopup = new MythPopupBox(gContext->GetMainWindow(), "");
    choosePopup->addLabel(tr("Select Sort Order"));

    chooseListBox = new MythListBox(choosePopup);
    chooseListBox->setScrollBar(false);
    chooseListBox->setBottomScrollBar(false);
    chooseListBox->insertStringList(viewTextList);
    if (curView < 0)
        chooseListBox->setCurrentItem(0);
    else
        chooseListBox->setCurrentItem(curView);
    choosePopup->addWidget(chooseListBox);

    connect(chooseListBox, SIGNAL(accepted(int)), this, SLOT(setViewFromList()));

    chooseListBox->setFocus();
    choosePopup->ExecPopup();

    delete chooseListBox;
    chooseListBox = NULL;
    delete choosePopup;
    choosePopup = NULL;
}

void PreviousBox::select()
{
    removalDialog();
}

void PreviousBox::edit()
{
    ProgramInfo *pi = itemList.at(curItem);

    if (!pi)
        return;

    pi->EditScheduled();
}

void PreviousBox::upcoming()
{
    ProgramInfo *pi = itemList.at(curItem);

    ProgLister *pl = new ProgLister(plTitle, pi->title,
                                   gContext->GetMainWindow(), "proglist");
    pl->exec();
    delete pl;
}

void PreviousBox::details()
{
    ProgramInfo *pi = itemList.at(curItem);

    if (pi)
        pi->showDetails();
}

void PreviousBox::fillViewList(const QString &view)
{
    viewList.clear();
    viewTextList.clear();

    viewList << "sort by title";
    viewTextList << tr("Sort by Title");

    viewList << "reverse title";
    viewTextList << tr("Reverse Title");

    viewList << "sort by time";
    viewTextList << tr("Sort by Time");

    viewList << "reverse time";
    viewTextList << tr("Reverse Time");

    curView = viewList.findIndex(view);

    if (curView < 0)
        curView = 0;
}

class pbTitleSort
{
    public:
        pbTitleSort(bool reverseSort = false) {m_reverse = reverseSort;}

        bool operator()(const ProgramInfo *a, const ProgramInfo *b) 
        {
            if (a->sortTitle == b->sortTitle)
            {
                if (a->programid == b->programid)
                    return (a->startts < b->startts);
                else
                    return (a->programid < b->programid);
            }
            else if (m_reverse)
                return (a->sortTitle > b->sortTitle);
            else
                return (a->sortTitle < b->sortTitle);
        }

    private:
        bool m_reverse;
};

class pbTimeSort
{
    public:
        pbTimeSort(bool reverseSort = false) {m_reverse = reverseSort;}

        bool operator()(const ProgramInfo *a, const ProgramInfo *b) 
        {
            if (m_reverse)
                return (a->startts > b->startts);
            else
                return (a->startts < b->startts);
        }

    private:
        bool m_reverse;
};

void PreviousBox::fillItemList(void)
{
    if (curView < 0)
        return;

    ProgramInfo *s;
    itemList.FromOldRecorded("");

    vector<ProgramInfo *> sortedList;
    while (itemList.count())
    {
        s = itemList.take();
        s->sortTitle = s->title;
        s->sortTitle.remove(QRegExp("^(The |A |An )"));
        sortedList.push_back(s);
    }

    if (viewList[curView] == "reverse time")
        sort(sortedList.begin(), sortedList.end(), pbTimeSort(true));
    else if (viewList[curView] == "sort by time")
        sort(sortedList.begin(), sortedList.end(), pbTimeSort(false));
    else if (viewList[curView] == "reverse title")
        sort(sortedList.begin(), sortedList.end(), pbTitleSort(true));
    else
        sort(sortedList.begin(), sortedList.end(), pbTitleSort(false));

    vector<ProgramInfo *>::iterator i = sortedList.begin();
    for (; i != sortedList.end(); i++)
        itemList.append(*i);

    if (curItem < 0 && itemList.count() > 0)
        curItem = 0;
    else if (curItem >= (int)itemList.count())
        curItem = itemList.count() - 1;
}

void PreviousBox::updateView(QPainter *p)
{
    QRect pr = viewRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);

    LayerSet *container = NULL;

    container = theme->GetSet("view");
    if (container)
    {  
        UITextType *type = (UITextType *)container->GetType("curview");
        if (type && curView >= 0)
            type->SetText(viewTextList[curView]);

        container->Draw(&tmp, 4, 0);
        container->Draw(&tmp, 5, 0);
        container->Draw(&tmp, 6, 0);
        container->Draw(&tmp, 7, 0);
        container->Draw(&tmp, 8, 0);
    }

    tmp.end();
    p->drawPixmap(pr.topLeft(), pix);
}

void PreviousBox::updateList(QPainter *p)
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
            if ((int)itemList.count() <= listsize || curItem <= listsize/2)
                skip = 0;
            else if (curItem >= (int)itemList.count() - listsize + listsize/2)
                skip = itemList.count() - listsize;
            else
                skip = curItem - listsize / 2;
            ltype->SetUpArrow(skip > 0);
            ltype->SetDownArrow(skip + listsize < (int)itemList.count());

            int i;
            for (i = 0; i < listsize; i++)
            {
                if (i + skip >= (int)itemList.count())
                    break;

                ProgramInfo *pi = itemList.at(i+skip);

                ltype->SetItemText(i, 1, pi->startts.toString(timeFormat));
                ltype->SetItemText(i, 2, pi->ChannelText(channelFormat));

                if (pi->subtitle == "")
                    tmptitle = pi->title;
                else
                {
                     tmptitle = QString("%1 - \"%2\"")
                                       .arg(pi->title)
                                       .arg(pi->subtitle);
                }

                ltype->SetItemText(i, 3, tmptitle);

                //if ((pi->catType == "series" &&
                //     pi->programid.contains(QRegExp(".*0000$"))) ||
                //    (pi->programid == "" && pi->subtitle == "" &&
                //      pi->description == ""))
                //    ltype->EnableForcedFont(i, "inactive");

                if (i + skip == curItem)
                    ltype->SetItemCurrent(i);
            }
        }
    }

    if (itemList.count() == 0)
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

void PreviousBox::updateInfo(QPainter *p)
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
            QMap<QString, QString> infoMap;
            pi->ToMap(infoMap);
            container->ClearAllText();
            container->SetText(infoMap);
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

void PreviousBox::removalDialog()
{
    ProgramInfo *pi = itemList.at(curItem);

    if (!pi)
        return;

    QString message = pi->title;

    if (pi->subtitle != "")
        message += QString(" - \"%1\"").arg(pi->subtitle);

    if (pi->description != "")
        message += "\n\n" + pi->description;

    message += "\n\n\n" + tr("NOTE: removing items from this list will not "
                             "delete any recordings.");
    
    DialogBox diag(gContext->GetMainWindow(), message);
    int button = 1, ok = -1, rm_episode = -1, rm_title = -1;
    // int rm_generics = -1;

    diag.AddButton(tr("Keep this episode in the list"));
    ok = button++;

    diag.AddButton(tr("Remove this episode from the list"));
    rm_episode = button++;

    diag.AddButton(tr("Remove all episodes for this title"));
    rm_title = button++;

    // diag.AddButton(tr("Remove all that cannot be used for duplicate matching"));
    // rm_generics = button++;

    int ret = diag.exec();

    if (ret == rm_episode)
    {
        deleteItem();
    }
    else if (ret == rm_title)
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("DELETE FROM oldrecorded WHERE title = :TITLE ;");
        query.bindValue(":TITLE", pi->title.utf8());
        query.exec();

        ScheduledRecording::signalChange(0);
        fillItemList();
    }
}

void PreviousBox::deleteItem()
{
    ProgramInfo *pi = itemList.at(curItem);

    if (!pi)
        return;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM oldrecorded "
                  "WHERE chanid = :CHANID AND starttime = :STARTTIME ;");
    query.bindValue(":CHANID", pi->chanid);
    query.bindValue(":STARTTIME", pi->startts.toString(Qt::ISODate));
    query.exec();
    ScheduledRecording::signalChange(0);
    fillItemList();
}

void PreviousBox::customEvent(QCustomEvent *e)
{
    if ((MythEvent::Type)(e->type()) != MythEvent::MythEventMessage)
        return;

    MythEvent *me = (MythEvent *)e;
    QString message = me->Message();
    if (message != "SCHEDULE_CHANGE" && message != "CHOOSE_VIEW")
        return;

    if (message == "CHOOSE_VIEW")
    {
        chooseView();
        if (curView < 0)
        {
            reject();
            return;
        }
    }

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
