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

#include "proglist.h"
#include "scheduledrecording.h"
#include "infodialog.h"
#include "dialogbox.h"
#include "mythcontext.h"
#include "remoteutil.h"

ProgLister::ProgLister(ProgListType pltype, const QString &view,
                       QSqlDatabase *ldb, MythMainWindow *parent,
                       const char *name)
            : MythDialog(parent, name)
{
    type = pltype;
    db = ldb;
    startTime = QDateTime::currentDateTime();
    timeFormat = gContext->GetSetting("ShortDateFormat") +
	" " + gContext->GetSetting("TimeFormat");

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

    chooseListBox = NULL;
    chooseComboBox = NULL;
    chooseLineEdit = NULL;
    chooseOkButton = NULL;
    chooseDeleteButton = NULL;

    curView = -1;
    fillViewList(view);

    itemList.setAutoDelete(true);
    curItem = -1;
    fillItemList();

    if (curView < 0)
        QApplication::postEvent(this, new MythEvent("CHOOSE_VIEW"));

    updateBackground();

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
        else if (action == "PREVVIEW")
            prevView();
        else if (action == "NEXTVIEW")
            nextView();
        else if (action == "MENU")
            chooseView();
        else if (action == "SELECT" || action == "RIGHT" || action == "LEFT")
            select();
        else if (action == "INFO")
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
		if (name.lower() == "view")
		    viewRect = area;
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
    {
        UITextType *ltype = (UITextType *)container->GetType("sched");
        if (ltype)
        {
            QString value;
            switch (type)
            {
                case plTitle: value = tr("Program Listings"); break;
                case plNewListings: value = tr("New Title Search"); break;
                case plTitleSearch: value = tr("Title Search"); break;
                case plDescSearch: value = tr("Description Search"); break;
                case plChannel: value = tr("Channel Search"); break;
                case plCategory: value = tr("Category Search"); break;
                case plMovies: value = tr("Movie Search"); break;
                default: value = tr("Unknown Search"); break;
            }
            ltype->SetText(value);
        }
	container->Draw(&tmp, 0, 0);
    }

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
 
    if (updateAll || r.intersects(viewRect))
        updateView(&p);
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

void ProgLister::prevView(void)
{
    if (viewCount < 2)
        return;

    curView--;
    if (curView < 0)
        curView = viewCount - 1;

    curItem = -1;
    refillAll = true;
}

void ProgLister::nextView(void)
{
    if (viewCount < 2)
        return;

    curView++;
    if (curView >= viewCount)
        curView = 0;

    curItem = -1;
    refillAll = true;
}

void ProgLister::setViewFromList(void)
{
    if (!chooseListBox)
        return;

    int view = chooseListBox->currentItem();

    if (view < 0 || view >= viewCount)
        return;

    if (view == curView)
        return;

    curView = view;

    curItem = -1;
    refillAll = true;
}

void ProgLister::chooseEditChanged(void)
{
    if (!chooseOkButton || !chooseLineEdit)
        return;

    chooseOkButton->setEnabled(chooseLineEdit->text() != "");
    chooseDeleteButton->setEnabled(false);
}

void ProgLister::chooseComboBoxChanged(void)
{
    if (!chooseComboBox || !chooseLineEdit)
        return;

    chooseLineEdit->setText(chooseComboBox->currentText());
    chooseDeleteButton->setEnabled(chooseComboBox->currentText() != "");
}

void ProgLister::setViewFromEdit(void)
{
    if (!chooseLineEdit)
        return;

    QString view = chooseLineEdit->text();

    if (view <= " ")
        return;

    QString querystr = QString("REPLACE INTO keyword VALUES('%1');").arg(view);
    QSqlQuery query;
    query.exec(querystr);

    fillViewList(chooseComboBox->currentText());

    chooseComboBox->clear();
    chooseComboBox->insertStringList(viewTextList);

    curView = viewList.findIndex(view);

    if (curView < 0)
    {
        viewList << view;
        viewTextList << view;
        curView = viewCount++;
    }

    curItem = -1;
    refillAll = true;
}

void ProgLister::deleteKeyword(void)
{
    if (!chooseDeleteButton || !chooseComboBox || !chooseLineEdit)
        return;

    if (viewCount < 1)
        return;

    QString view = chooseComboBox->currentText();

    QString querystr = QString("DELETE FROM keyword WHERE phrase = '%1';")
                               .arg(view);
    QSqlQuery query;
    query.exec(querystr);

    fillViewList(chooseComboBox->currentText());

    chooseComboBox->clear();
    chooseComboBox->insertStringList(viewTextList);

    chooseLineEdit->setText("");
    chooseDeleteButton->setEnabled(false);
}

void ProgLister::chooseView(void)
{
    if (type == plChannel || type == plCategory)
    {
        if (viewCount < 2)
            return;

        MythPopupBox choosePopup(gContext->GetMainWindow(), "");
        if (type == plChannel)
            choosePopup.addLabel(tr("Select Channel"));
        else if (type == plCategory)
            choosePopup.addLabel(tr("Select Category"));

        chooseListBox = new MythListBox(&choosePopup);
        chooseListBox->setScrollBar(false);
        chooseListBox->setBottomScrollBar(false);
        chooseListBox->insertStringList(viewTextList);
        if (curView < 0)
            chooseListBox->setCurrentItem(0);
        else
            chooseListBox->setCurrentItem(curView);
        choosePopup.addWidget(chooseListBox);

        connect(chooseListBox, SIGNAL(accepted(int)), this, SLOT(setViewFromList()));
        connect(chooseListBox, SIGNAL(accepted(int)), &choosePopup, SLOT(accept()));

        chooseListBox->setFocus();
        choosePopup.ExecPopup();

        delete chooseListBox;
        chooseListBox = NULL;
    }
    else if (type == plTitleSearch || type == plDescSearch)
    {
        MythPopupBox choosePopup(gContext->GetMainWindow(), "");
        choosePopup.addLabel(tr("Enter Search Phrase"));

        chooseLineEdit = new MythRemoteLineEdit(&choosePopup);
        chooseLineEdit->setText("");
        chooseLineEdit->selectAll();
        choosePopup.addWidget(chooseLineEdit);

        chooseOkButton = new MythPushButton(&choosePopup);
        chooseOkButton->setText(tr("OK"));
        chooseOkButton->setEnabled(false);
        choosePopup.addWidget(chooseOkButton);

        chooseComboBox = new MythComboBox(false, &choosePopup);
        chooseComboBox->insertStringList(viewTextList);
        if (curView < 0)
            chooseComboBox->setCurrentItem(0);
        else
            chooseComboBox->setCurrentItem(curView);
        choosePopup.addWidget(chooseComboBox);

        chooseDeleteButton = new MythPushButton(&choosePopup);
        chooseDeleteButton->setText(tr("Delete"));
        chooseDeleteButton->setEnabled(chooseComboBox->currentText() != "");
        choosePopup.addWidget(chooseDeleteButton);

        connect(chooseLineEdit, SIGNAL(textChanged()), this, SLOT(chooseEditChanged()));
        connect(chooseOkButton, SIGNAL(clicked()), this, SLOT(setViewFromEdit()));
        connect(chooseOkButton, SIGNAL(clicked()), &choosePopup, SLOT(accept()));
        connect(chooseComboBox, SIGNAL(activated(int)), this, SLOT(chooseComboBoxChanged()));
        connect(chooseComboBox, SIGNAL(highlighted (int)), this, SLOT(chooseComboBoxChanged()));
        connect(chooseDeleteButton, SIGNAL(clicked()), this, SLOT(deleteKeyword()));

        chooseLineEdit->setFocus();
        choosePopup.ExecPopup();

        delete chooseLineEdit;
        chooseLineEdit = NULL;
        delete chooseOkButton;
        chooseOkButton = NULL;
        delete chooseComboBox;
        chooseComboBox = NULL;
        delete chooseDeleteButton;
        chooseDeleteButton = NULL;
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

void ProgLister::fillViewList(const QString &view)
{
    viewList.clear();
    viewTextList.clear();
    viewCount = 0;

    if (type == plChannel) // list by channel
    {
        QString channelOrdering = 
            gContext->GetSetting("ChannelOrdering", "channum + 0");
        QString querystr = "SELECT channel.chanid, channel.channum, "
            "channel.callsign FROM channel ORDER BY " + channelOrdering + ";";
        QSqlQuery query;
        query.exec(querystr);
        if (query.isActive() && query.numRowsAffected())
        {
            while (query.next())
            {
                QString chanid = query.value(0).toString();
                QString chantext;
                QString channum = query.value(1).toString();
                if (channum != QString::null && channum != "")
                    chantext = channum;
                else
                    chantext = "???";
                QString chansign = query.value(2).toString();
                if (chansign != QString::null && chansign != "")
                    chantext = chantext + " " + chansign;
                viewList << chanid;
                viewTextList << chantext;
                viewCount++;
            }
        }
        if (view != "")
            curView = viewList.findIndex(view);
    }
    else if (type == plCategory) // list by category
    {
        QString querystr = "SELECT category FROM program GROUP BY category;";
        QSqlQuery query;
        query.exec(querystr);
        if (query.isActive() && query.numRowsAffected())
        {
            while (query.next())
            {
                QString category = query.value(0).toString();
                if (category <= " ")
                    continue;
                viewList << category;
                viewTextList << category;
                viewCount++;
            }
        }
        if (view != "")
            curView = viewList.findIndex(view);
    }
    else if (type == plTitleSearch || type == plDescSearch)
    {
        QString querystr = "SELECT phrase FROM keyword;";
        QSqlQuery query;
        query.exec(querystr);
        if (query.isActive() && query.numRowsAffected())
        {
            while (query.next())
            {
                QString phrase = query.value(0).toString();
                if (phrase <= " ")
                    continue;
                viewList << phrase;
                viewTextList << phrase;
                viewCount++;
            }
        }
        if (view != "")
            curView = viewList.findIndex(view);
        else
            curView = -1;
    }
    else if (type == plTitle)
    {
        viewList << view;
        viewTextList << view;
        viewCount++;
        if (view != "")
            curView = 0;
        else
            curView = -1;
    }
    else if (type == plNewListings || type == plMovies)
    {
        viewList << "";
        viewTextList << "";
        viewCount++;
        curView = 0;
    }

    if (curView >= viewCount)
        curView = viewCount - 1;
}

void ProgLister::fillItemList(void)
{
    itemList.clear();
    itemCount = 0;

    if (curView < 0)
        return;

    QString where;

    if (type == plTitle) // per title listings
    {
        where = QString("WHERE program.title = \"%1\" "
                        "AND program.endtime > %2 "
                        "AND program.chanid = channel.chanid "
                        "ORDER BY program.starttime,channel.channum;")
                        .arg(viewList[curView].utf8())
                        .arg(startTime.toString("yyyyMMddhhmm50"));
    }
    else if (type == plNewListings) // what's new list
    {
        where = QString("LEFT JOIN oldprogram ON title=oldtitle "
                        "WHERE oldtitle IS NULL AND program.endtime > %1 "
                        "AND program.chanid = channel.chanid "
                        "AND ( program.airdate = 0 OR "
                        "program.airdate >= YEAR(NOW() - INTERVAL 1 YEAR)) "
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
                        .arg(viewList[curView].utf8())
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
                        .arg(viewList[curView].utf8()).arg(viewList[curView].utf8()).arg(viewList[curView].utf8())
                        .arg(startTime.toString("yyyyMMddhhmm50"));
    }
    else if (type == plChannel) // list by channel
    {
        where = QString("WHERE channel.chanid = \"%1\" "
                        "AND program.endtime > %2 "
                        "AND program.chanid = channel.chanid "
                        "ORDER BY program.starttime;")
                        .arg(viewList[curView])
                        .arg(startTime.toString("yyyyMMddhhmm50"));
    }
    else if (type == plCategory) // list by category
    {
        where = QString("WHERE program.category = \"\%1\" "
                        "AND program.endtime > %2 "
                        "AND program.chanid = channel.chanid "
                        "ORDER BY program.starttime,channel.channum "
                        "LIMIT 500;")
                        .arg(viewList[curView].utf8())
                        .arg(startTime.toString("yyyyMMddhhmm50"));
    }
    else if (type == plMovies) // list movies
    {
        where = QString("WHERE program.category_type LIKE \"\%%1\%\" "
                        "AND program.endtime > %2 "
                        "AND program.chanid = channel.chanid "
                        "ORDER BY program.starttime,channel.channum "
                        "LIMIT 500;")
                        .arg(tr("Movie").utf8())
                        .arg(startTime.toString("yyyyMMddhhmm50"));
    }

    ProgramInfo::GetProgramListByQuery(db, &itemList, where);
    itemCount = itemList.count();

    if (curItem < 0 && itemCount > 0)
        curItem = 0;
    else if (curItem >= itemCount)
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

void ProgLister::updateView(QPainter *p)
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
            QMap<QString, QString> infoMap;
            pi->ToMap(db, infoMap);
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

void ProgLister::customEvent(QCustomEvent *e)
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

