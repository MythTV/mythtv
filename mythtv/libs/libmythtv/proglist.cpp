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
#include "customedit.h"
#include "dialogbox.h"
#include "mythcontext.h"
#include "remoteutil.h"
#include "mythdbcon.h"

ProgLister::ProgLister(ProgListType pltype,
                       const QString &view, const QString &from,
                       MythMainWindow *parent,
                       const char *name)
            : MythDialog(parent, name)
{
    type = pltype;
    addTables = from;
    startTime = QDateTime::currentDateTime();
    searchTime = startTime;

    dayFormat = gContext->GetSetting("DateFormat");
    hourFormat = gContext->GetSetting("TimeFormat");
    timeFormat = gContext->GetSetting("ShortDateFormat") + " " + hourFormat;
    fullDateFormat = dayFormat + " " + hourFormat;
    channelOrdering = gContext->GetSetting("ChannelOrdering", "channum + 0");
    channelFormat = gContext->GetSetting("ChannelFormat", "<num> <sign>");

    switch (pltype)
    {
        case plTitleSearch:   searchtype = kTitleSearch;   break;
        case plKeywordSearch: searchtype = kKeywordSearch; break;
        case plPeopleSearch:  searchtype = kPeopleSearch;  break;
        case plPowerSearch:   searchtype = kPowerSearch;   break;
        case plSQLSearch:     searchtype = kPowerSearch;   break;
        default:              searchtype = kNoSearch;      break;
    }

    allowEvents = true;
    allowUpdates = true;
    updateAll = false;
    refillAll = false;
    titleSort = false;
    reverseSort = false;

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
    chooseEditButton = NULL;
    chooseOkButton = NULL;
    chooseDeleteButton = NULL;
    chooseRecordButton = NULL;
    chooseDay = NULL;
    chooseHour = NULL;

    powerPopup = NULL;
    powerTitleEdit = NULL;
    powerSubtitleEdit = NULL;
    powerDescEdit = NULL;
    powerCatType = NULL;
    powerCategory = NULL;
    powerStation = NULL;

    curView = -1;
    fillViewList(view);

    curItem = -1;
    fillItemList();

    if (curView < 0)
        QApplication::postEvent(this, new MythEvent("CHOOSE_VIEW"));

    updateBackground();

    setNoErase();

    gContext->addListener(this);
    gContext->addCurrentLocation("ProgLister");
}

ProgLister::~ProgLister()
{
    itemList.clear();
    gContext->removeListener(this);
    gContext->removeCurrentLocation();
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
        else if (action == "SELECT" || action == "RIGHT")
            select();
        else if (action == "LEFT")
            accept();
        else if (action == "INFO")
            edit();
        else if (action == "CUSTOMEDIT")
            customEdit();
        else if (action == "UPCOMING")
            upcoming();
        else if (action == "DETAILS")
            details();
        else if (action == "TOGGLERECORD")
            quickRecord();
        else if (action == "1")
        {
            if (titleSort == true)
            {
                titleSort = false;
                reverseSort = false;
            }
            else
            {
                reverseSort = !reverseSort;
            }
            refillAll = true;
        }
        else if (action == "2")
        {
            if (titleSort == false)
            {
                titleSort = true;
                reverseSort = false;
            }
            else
            {
                reverseSort = !reverseSort;
            }
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
                VERBOSE(VB_IMPORTANT,
                        QString("ProgLister::LoadWindow(): Error, unknown "
                                "element '%1'. Ignoring.").arg(e.tagName()));
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
                case plKeywordSearch: value = tr("Keyword Search"); break;
                case plPeopleSearch: value = tr("People Search"); break;
                case plPowerSearch: value = tr("Power Search"); break;
                case plSQLSearch: value = tr("Power Search"); break;
                case plCategory: value = tr("Category Search"); break;
                case plChannel: value = tr("Channel Search"); break;
                case plMovies: value = tr("Movie Search"); break;
                case plTime: value = tr("Time Search"); break;
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
 
    
    if (updateAll || r.intersects(listRect))
        updateList(&p);
    if (updateAll || r.intersects(infoRect))
        updateInfo(&p);
    if (updateAll || r.intersects(viewRect))
        updateView(&p);
    
    updateAll = false;
}

void ProgLister::cursorDown(bool page)
{
    if (curItem < (int)itemList.count() - 1)
    {
        curItem += (page ? listsize : 1);
        if (curItem > (int)itemList.count() - 1)
            curItem = itemList.count() - 1;
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
    if (type == plTime)
    {
        searchTime = searchTime.addSecs(-3600);
        curView = 0;
        viewList[curView] = searchTime.toString(fullDateFormat);
        viewTextList[curView] = viewList[curView];
        refillAll = true;
        return;
    }

    if (viewList.count() < 2)
        return;

    curView--;
    if (curView < 0)
        curView = viewList.count() - 1;

    curItem = -1;
    refillAll = true;
}

void ProgLister::nextView(void)
{
    if (type == plTime)
    {
        searchTime = searchTime.addSecs(3600);
        curView = 0;
        viewList[curView] = searchTime.toString(fullDateFormat);
        viewTextList[curView] = viewList[curView];
        refillAll = true;
        return;
    }
    if (viewList.count() < 2)
        return;

    curView++;
    if (curView >= (int)viewList.count())
        curView = 0;

    curItem = -1;
    refillAll = true;
}

void ProgLister::setViewFromList(void)
{
    if (!choosePopup || (!chooseListBox && !chooseEditButton))
        return;

    int view = chooseListBox->currentItem();

    if (type == plTitleSearch || type == plKeywordSearch || 
        type == plPeopleSearch)
    {
        view--;
        if (view < 0)
        {
            if (chooseLineEdit)
                chooseLineEdit->setFocus();
            return;
        }
    }
    if (type == plPowerSearch)
    {
        view--;
        if (view < 0)
        {
            if (chooseEditButton)
                powerEdit();
            return;
        }
    }

    choosePopup->done(0);

    if (view == curView)
        return;

    curView = view;

    curItem = -1;
    refillAll = true;
}

void ProgLister::chooseEditChanged(void)
{
    if (!chooseOkButton || !chooseRecordButton || !chooseLineEdit)
        return;

    chooseOkButton->setEnabled(chooseLineEdit->text().
                               stripWhiteSpace().length() > 0);
    chooseRecordButton->setEnabled(chooseLineEdit->text().
                                   stripWhiteSpace().length() > 0);
}

void ProgLister::chooseListBoxChanged(void)
{
    if (!chooseListBox)
        return;

    int view = chooseListBox->currentItem() - 1;

    if (chooseLineEdit)
    {
        if (view < 0)
            chooseLineEdit->setText("");
        else
            chooseLineEdit->setText(viewList[view]);

        chooseDeleteButton->setEnabled(view >= 0);
    }
    else if (chooseEditButton)
    {
        chooseDeleteButton->setEnabled(view >= 0);
        chooseRecordButton->setEnabled(view >= 0);
    }
}

void ProgLister::updateKeywordInDB(const QString &text)
{
    int oldview = chooseListBox->currentItem() - 1;
    int newview = viewList.findIndex(text);

    QString qphrase = NULL;

    if (newview < 0 || newview != oldview)
    {
        if (oldview >= 0)
        {
            qphrase = viewList[oldview].utf8();

            MSqlQuery query(MSqlQuery::InitCon());
            query.prepare("DELETE FROM keyword "
                          "WHERE phrase = :PHRASE AND searchtype = :TYPE;");
            query.bindValue(":PHRASE", qphrase);
            query.bindValue(":TYPE", searchtype);
            query.exec();
        }
        if (newview < 0)
        {
            qphrase = text.utf8();

            MSqlQuery query(MSqlQuery::InitCon());
            query.prepare("REPLACE INTO keyword (phrase, searchtype)"
                          "VALUES(:PHRASE, :TYPE );");
            query.bindValue(":PHRASE", qphrase);
            query.bindValue(":TYPE", searchtype);
            query.exec();
        }
    }
}

void ProgLister::setViewFromEdit(void)
{
    if (!choosePopup || !chooseListBox || !chooseLineEdit)
        return;

    QString text = chooseLineEdit->text();

    if (text.stripWhiteSpace().length() == 0)
        return;

    updateKeywordInDB(text);
  
    choosePopup->done(0);

    fillViewList(text);

    curItem = -1;
    refillAll = true;
}

void ProgLister::setViewFromPowerEdit()
{
    if (!powerPopup || !choosePopup || !chooseListBox)
        return;

    QString text = "";
    text =     powerTitleEdit->text().replace(":","%").replace("*","%") + ":";
    text += powerSubtitleEdit->text().replace(":","%").replace("*","%") + ":";
    text +=     powerDescEdit->text().replace(":","%").replace("*","%") + ":";

    if (powerCatType->currentItem() > 0)
        text += typeList[powerCatType->currentItem()];
    text += ":";
    if (powerCategory->currentItem() > 0)
        text += categoryList[powerCategory->currentItem()];
    text += ":";
    if (powerStation->currentItem() > 0)
        text += stationList[powerStation->currentItem()];

    if (text == ":::::")
        return;

    updateKeywordInDB(text);

    powerPopup->done(0);

    fillViewList(text);

    curView = viewList.findIndex(text);

    curItem = -1;
    refillAll = true;
}

void ProgLister::addSearchRecord(void)
{
    if (!choosePopup || !chooseListBox)
        return;

    QString text = "";

    if (chooseLineEdit)
        text = chooseLineEdit->text();
    else if (chooseEditButton)
        text = chooseListBox->currentText();
    else
        return;

    QString what = text;

    if (text.stripWhiteSpace().length() == 0)
        return;

    if (searchtype == kNoSearch)
    {
        VERBOSE(VB_IMPORTANT, "Unknown search in ProgLister");
        return;
    }

    if (searchtype == kPowerSearch)
    {
        if (text == "" || text == ":::::")
            return;

        MSqlBindings bindings;
        powerStringToSQL(text.utf8(), what, bindings);

        if (what == "")
            return;

        MSqlEscapeAsAQuery(what, bindings);
    }


    ScheduledRecording record;
    record.loadBySearch(searchtype, text, what);
    record.exec();

    chooseListBox->setFocus();
    setViewFromEdit();
}

void ProgLister::deleteKeyword(void)
{
    if (!chooseDeleteButton || !chooseListBox)
        return;

    int view = chooseListBox->currentItem() - 1;

    if (view < 0)
        return;

    QString text = viewList[view];
    QString qphrase = text.utf8();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM keyword "
                  "WHERE phrase = :PHRASE AND searchtype = :TYPE;");
    query.bindValue(":PHRASE", qphrase);
    query.bindValue(":TYPE", searchtype);
    query.exec();

    chooseListBox->removeItem(view + 1);
    viewList.remove(text);
    viewTextList.remove(text);

    if (view < curView)
        curView--;
    else if (view == curView)
        curView = -1;

    if (view >= (int)chooseListBox->count() - 1)
        view = chooseListBox->count() - 2;

    chooseListBox->setSelected(view + 1, true);

    if (viewList.count() < 1 && chooseLineEdit)
        chooseLineEdit->setFocus();
    else
        chooseListBox->setFocus();
}

void ProgLister::setViewFromTime(void)
{
    if (!choosePopup || !chooseDay || !chooseHour)
        return;

    int dayOffset = chooseDay->currentItem() - 1;
    searchTime.setDate(startTime.addDays(dayOffset).date());

    QTime m_hr;
    m_hr.setHMS(chooseHour->currentItem(), 0, 0);
    searchTime.setTime(m_hr);

    curView = 0;
    viewList[curView] = searchTime.toString(fullDateFormat);
    viewTextList[curView] = viewList[curView];

    choosePopup->done(0);

    curItem = -1;
    refillAll = true;
}

void ProgLister::chooseView(void)
{
    if (type == plChannel || type == plCategory || 
        type == plMovies || type == plNewListings)
    {
        if (viewList.count() < 2)
            return;

        choosePopup = new MythPopupBox(gContext->GetMainWindow(), "");

        QString msg;
        switch (type)
        {
            case plMovies: msg = tr("Select Rating"); break;
            case plChannel: msg = tr("Select Channel"); break;
            case plCategory: msg = tr("Select Category"); break;
            case plNewListings: msg = tr("Select List"); break;
            default: msg = tr("Select"); break;
        }
        choosePopup->addLabel(msg);

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
    else if (type == plTitleSearch || type == plKeywordSearch ||
             type == plPeopleSearch)
    {
        int oldView = curView;

        choosePopup = new MythPopupBox(gContext->GetMainWindow(), "");
        choosePopup->addLabel(tr("Select Phrase"));

        chooseListBox = new MythListBox(choosePopup);
        chooseListBox->setScrollBar(false);
        chooseListBox->setBottomScrollBar(false);
        chooseListBox->insertItem(tr("<New Phrase>"));
        chooseListBox->insertStringList(viewTextList);
        if (curView < 0)
            chooseListBox->setCurrentItem(0);
        else
            chooseListBox->setCurrentItem(curView + 1);
        choosePopup->addWidget(chooseListBox);

        chooseLineEdit = new MythRemoteLineEdit(choosePopup);
        if (curView < 0)
            chooseLineEdit->setText("");
        else
            chooseLineEdit->setText(viewList[curView]);
        choosePopup->addWidget(chooseLineEdit);

        chooseOkButton = new MythPushButton(choosePopup);
        chooseOkButton->setText(tr("OK"));
        choosePopup->addWidget(chooseOkButton);

        chooseDeleteButton = new MythPushButton(choosePopup);
        chooseDeleteButton->setText(tr("Delete"));
        choosePopup->addWidget(chooseDeleteButton);

        chooseRecordButton = new MythPushButton(choosePopup);
        chooseRecordButton->setText(tr("Record"));
        choosePopup->addWidget(chooseRecordButton);

        chooseOkButton->setEnabled(chooseLineEdit->text()
                                   .stripWhiteSpace().length() > 0);
        chooseDeleteButton->setEnabled(curView >= 0);
        chooseRecordButton->setEnabled(chooseLineEdit->text()
                                       .stripWhiteSpace().length() > 0);

        connect(chooseListBox, SIGNAL(accepted(int)), this, SLOT(setViewFromList()));
        connect(chooseListBox, SIGNAL(menuButtonPressed(int)), chooseLineEdit, SLOT(setFocus()));
        connect(chooseListBox, SIGNAL(selectionChanged()), this, SLOT(chooseListBoxChanged()));
        connect(chooseLineEdit, SIGNAL(textChanged()), this, SLOT(chooseEditChanged()));
        connect(chooseOkButton, SIGNAL(clicked()), this, SLOT(setViewFromEdit()));
        connect(chooseDeleteButton, SIGNAL(clicked()), this, SLOT(deleteKeyword()));
        connect(chooseRecordButton, SIGNAL(clicked()), this, SLOT(addSearchRecord()));

        if (viewList.count() < 1)
            chooseLineEdit->setFocus();
        else
            chooseListBox->setFocus();
        choosePopup->ExecPopup();

        delete chooseLineEdit;
        chooseLineEdit = NULL;
        delete chooseOkButton;
        chooseOkButton = NULL;
        delete chooseDeleteButton;
        chooseDeleteButton = NULL;
        delete chooseRecordButton;
        chooseRecordButton = NULL;
        delete chooseListBox;
        chooseListBox = NULL;
        delete choosePopup;
        choosePopup = NULL;

        if (viewList.count() < 1 || (oldView < 0 && curView < 0))
            reject();
        else if (curView < 0)
        {
            curView = 0;
            curItem = -1;
            refillAll = true;
        }
    }
    else if (type == plPowerSearch)
    {
        int oldView = curView;

        choosePopup = new MythPopupBox(gContext->GetMainWindow(), "");
        choosePopup->addLabel(tr("Select Search"));

        chooseListBox = new MythListBox(choosePopup);
        chooseListBox->setScrollBar(false);
        chooseListBox->setBottomScrollBar(false);
        chooseListBox->insertItem(tr("<New Search>"));
        chooseListBox->insertStringList(viewTextList);
        if (curView < 0)
            chooseListBox->setCurrentItem(0);
        else
            chooseListBox->setCurrentItem(curView + 1);
        choosePopup->addWidget(chooseListBox);

        chooseEditButton = new MythPushButton(choosePopup);
        chooseEditButton->setText(tr("Edit"));
        choosePopup->addWidget(chooseEditButton);

        chooseDeleteButton = new MythPushButton(choosePopup);
        chooseDeleteButton->setText(tr("Delete"));
        choosePopup->addWidget(chooseDeleteButton);

        chooseRecordButton = new MythPushButton(choosePopup);
        chooseRecordButton->setText(tr("Record"));
        choosePopup->addWidget(chooseRecordButton);

        chooseDeleteButton->setEnabled(curView >= 0);
        chooseRecordButton->setEnabled(curView >= 0);

        connect(chooseListBox, SIGNAL(accepted(int)), this,
                               SLOT(setViewFromList()));
        connect(chooseListBox, SIGNAL(menuButtonPressed(int)),chooseEditButton,
                               SLOT(setFocus()));
        connect(chooseListBox, SIGNAL(selectionChanged()), this,
                               SLOT(chooseListBoxChanged()));
        connect(chooseEditButton, SIGNAL(clicked()), this, 
                                  SLOT(powerEdit()));
        connect(chooseDeleteButton, SIGNAL(clicked()), this,
                                    SLOT(deleteKeyword()));
        connect(chooseRecordButton, SIGNAL(clicked()), this,
                                    SLOT(addSearchRecord()));

        if (viewList.count() < 1)
            chooseEditButton->setFocus();
        else
            chooseListBox->setFocus();
        choosePopup->ExecPopup();

        delete chooseEditButton;
        chooseEditButton = NULL;
        delete chooseDeleteButton;
        chooseDeleteButton = NULL;
        delete chooseRecordButton;
        chooseRecordButton = NULL;
        delete chooseListBox;
        chooseListBox = NULL;
        delete choosePopup;
        choosePopup = NULL;

        if (viewList.count() < 1 || (oldView < 0 && curView < 0))
            reject();
        else if (curView < 0)
        {
            curView = 0;
            curItem = -1;
            refillAll = true;
        }
    }
    else if (type == plTime)
    {
        choosePopup = new MythPopupBox(gContext->GetMainWindow(), "");
        choosePopup->addLabel(tr("Select Time"));

        chooseDay = new MythComboBox(false, choosePopup);

        for(int m_index = -1; m_index <= 14; m_index++)
        {
            chooseDay->insertItem(startTime.addDays(m_index)
                                  .toString(dayFormat));
            if (startTime.addDays(m_index).toString("MMdd") ==
                                searchTime.toString("MMdd"))
                chooseDay->setCurrentItem(chooseDay->count() - 1);
        }
        choosePopup->addWidget(chooseDay);

        chooseHour = new MythComboBox(false, choosePopup);

        QTime m_hr;
        for(int m_index = 0; m_index < 24; m_index++)
        {
            m_hr.setHMS(m_index, 0, 0);
            chooseHour->insertItem(m_hr.toString(hourFormat));
            if (m_hr.toString("hh") == searchTime.toString("hh"))
                chooseHour->setCurrentItem(m_index);
        }
        choosePopup->addWidget(chooseHour);

        chooseOkButton = new MythPushButton(choosePopup);
        chooseOkButton->setText(tr("OK"));
        choosePopup->addWidget(chooseOkButton);

        connect(chooseOkButton,
                SIGNAL(clicked()), this, SLOT(setViewFromTime()));

        chooseOkButton->setFocus();
        choosePopup->ExecPopup();

        delete chooseDay;
        chooseDay = NULL;
        delete chooseHour;
        chooseHour = NULL;
        delete chooseOkButton;
        chooseOkButton = NULL;
        delete choosePopup;
        choosePopup = NULL;
    }
}

void ProgLister::powerEdit()
{
    int view = chooseListBox->currentItem() - 1;
    QString text = ":::::";

    if (view >= 0)
        text = viewList[view];

    QStringList field = QStringList::split( ":", text, true);

    if (field.count() != 6)
    {
        VERBOSE(VB_IMPORTANT, QString("Error. PowerSearch %1 has %2 fields")
                .arg(text).arg(field.count()));
    }

    powerPopup = new MythPopupBox(gContext->GetMainWindow(), "");
    powerPopup->addLabel(tr("Edit Power Search Fields"));

    powerPopup->addLabel(tr("Optional title phrase:"));
    powerTitleEdit = new MythRemoteLineEdit(powerPopup);
    powerPopup->addWidget(powerTitleEdit);

    powerPopup->addLabel(tr("Optional subtitle phrase:"));
    powerSubtitleEdit = new MythRemoteLineEdit(powerPopup);
    powerPopup->addWidget(powerSubtitleEdit);

    powerPopup->addLabel(tr("Optional description phrase:"));
    powerDescEdit = new MythRemoteLineEdit(powerPopup);
    powerPopup->addWidget(powerDescEdit);

    powerCatType = new MythComboBox(false, powerPopup);
    powerCatType->insertItem(tr("(Any Program Type)"));
    typeList.clear();
    typeList << "";
    powerCatType->insertItem(tr("Movies"));
    typeList << "movie";
    powerCatType->insertItem(tr("Series"));
    typeList << "series";
    powerCatType->insertItem(tr("Show"));
    typeList << "tvshow";
    powerCatType->insertItem(tr("Sports"));
    typeList << "sports";
    powerCatType->setCurrentItem(typeList.findIndex(field[3]));
    powerPopup->addWidget(powerCatType);

    powerCategory = new MythComboBox(false, powerPopup);
    powerCategory->insertItem(tr("(Any Category)"));
    categoryList.clear();
    categoryList << "";

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT category FROM program GROUP BY category;");
    query.exec();

    if (query.isActive() && query.size())
    {
        while (query.next())
        {
            QString category = query.value(0).toString();
            if (category <= " " || category == NULL)
                continue;
            category = QString::fromUtf8(query.value(0).toString());
            powerCategory->insertItem(category);
            categoryList << category;
            if (category == field[4])
                powerCategory->setCurrentItem(powerCategory->count() - 1);
        }
    }
    powerPopup->addWidget(powerCategory);

    powerStation = new MythComboBox(false, powerPopup);
    powerStation->insertItem(tr("(Any Station)"));
    stationList.clear();
    stationList << "";

    query.prepare(QString("SELECT channel.chanid, channel.channum, "
                  "channel.callsign, channel.name FROM channel "
                  "WHERE channel.visible = 1 "
                  "GROUP BY callsign "
                  "ORDER BY ") + channelOrdering + ";");
    query.exec();

    if (query.isActive() && query.size())
    {
        while (query.next())
        {
            QString chanid = query.value(0).toString();
            QString channum = query.value(1).toString();
            QString chansign = QString::fromUtf8(query.value(2).toString());
            QString channame = QString::fromUtf8(query.value(3).toString());

            QString chantext = channelFormat;
            chantext.replace("<num>", channum)
                .replace("<sign>", chansign)
                .replace("<name>", channame);

            viewList << chanid;
            viewTextList << chantext;

            powerStation->insertItem(chantext);
            stationList << chansign;
            if (chansign == field[5])
                powerStation->setCurrentItem(powerStation->count() - 1);
        }
    }
    powerPopup->addWidget(powerStation);

    powerOkButton = new MythPushButton(powerPopup);
    powerOkButton->setText(tr("OK"));
    powerPopup->addWidget(powerOkButton);

    connect(powerOkButton, SIGNAL(clicked()), this, 
                           SLOT(setViewFromPowerEdit()));

    powerTitleEdit->setText(field[0]);
    powerSubtitleEdit->setText(field[1]);
    powerDescEdit->setText(field[2]);

    powerTitleEdit->setFocus();
    choosePopup->done(0);
    powerPopup->ExecPopup();

    delete powerTitleEdit;
    powerTitleEdit = NULL;
    delete powerSubtitleEdit;
    powerSubtitleEdit = NULL;
    delete powerDescEdit;
    powerDescEdit = NULL;

    delete powerCatType;
    powerCatType = NULL;
    delete powerCategory;
    powerCategory = NULL;
    delete powerStation;
    powerStation = NULL;

    delete powerOkButton;
    powerOkButton = NULL;
    delete powerPopup;
    powerPopup = NULL;
}

void ProgLister::powerStringToSQL(const QString &qphrase, QString &output,
                                  MSqlBindings &bindings)
{
    output = "";
    QString curfield;

    QStringList field = QStringList::split(":", qphrase, true);

    if (field.count() != 6)
    {
        VERBOSE(VB_IMPORTANT, QString("Error. PowerSearch %1 has %2 fields")
                .arg(qphrase).arg(field.count()));
        return;
    }

    if (field[0])
    {
        curfield = "%" + field[0] + "%";
        output += "program.title LIKE :POWERTITLE ";
        bindings[":POWERTITLE"] = curfield;
    }

    if (field[1])
    {
        if (output > "")
            output += "\nAND ";

        curfield = "%" + field[1] + "%";
        output += "program.subtitle LIKE :POWERSUB ";
        bindings[":POWERSUB"] = curfield;
    }

    if (field[2])
    {
        if (output > "")
            output += "\nAND ";

        curfield = "%" + field[2] + "%";
        output += "program.description LIKE :POWERDESC ";
        bindings[":POWERDESC"] = curfield;
    }

    if (field[3])
    {
        if (output > "")
            output += "\nAND ";

        output += "program.category_type = :POWERCATTYPE ";
        bindings[":POWERCATTYPE"] = field[3];
    }

    if (field[4])
    {
        if (output > "")
            output += "\nAND ";

        output += "program.category = :POWERCAT ";
        bindings[":POWERCAT"] = field[4];
    }

    if (field[5])
    {
        if (output > "")
            output += "\nAND ";

        output += "channel.callsign = :POWERCALLSIGN ";
        bindings[":POWERCALLSIGN"] = field[5];
    }
}

void ProgLister::quickRecord()
{
    ProgramInfo *pi = itemList.at(curItem);

    if (!pi)
        return;

    pi->ToggleRecord();
}

void ProgLister::select()
{
    ProgramInfo *pi = itemList.at(curItem);

    if (!pi)
        return;

    pi->EditRecording();
}

void ProgLister::edit()
{
    ProgramInfo *pi = itemList.at(curItem);

    if (!pi)
        return;

    pi->EditScheduled();
}

void ProgLister::customEdit()
{
    ProgramInfo *pi = itemList.at(curItem);

    if (!pi)
        return;

    CustomEdit *ce = new CustomEdit(gContext->GetMainWindow(), "customedit",
                                    pi->getRecordID(), pi->title);
    ce->exec();
    delete ce;
}

void ProgLister::upcoming()
{
    ProgramInfo *pi = itemList.at(curItem);

    if (!pi || type == plTitle)
        return;

    ProgLister *pl = new ProgLister(plTitle, pi->title, "",
                                   gContext->GetMainWindow(), "proglist");
    pl->exec();
    delete pl;
}

void ProgLister::details()
{
    ProgramInfo *pi = itemList.at(curItem);

    if (pi)
        pi->showDetails();
}

void ProgLister::fillViewList(const QString &view)
{
    viewList.clear();
    viewTextList.clear();

    if (type == plChannel) // list by channel
    {
        MSqlQuery query(MSqlQuery::InitCon()); 
        query.prepare(QString("SELECT channel.chanid, channel.channum, "
                      "channel.callsign, channel.name FROM channel "
                      "WHERE channel.visible = 1 "
                      "GROUP BY channum, callsign "
                      "ORDER BY ") + channelOrdering + ";");
        query.exec();

        if (query.isActive() && query.size())
        {
            while (query.next())
            {
                QString chanid = query.value(0).toString();
                QString channum = query.value(1).toString();
                QString chansign = QString::fromUtf8(query.value(2).toString());
                QString channame = QString::fromUtf8(query.value(3).toString());

                QString chantext = channelFormat;
                chantext.replace("<num>", channum)
                    .replace("<sign>", chansign)
                    .replace("<name>", channame);

                viewList << chanid;
                viewTextList << chantext;
            }
        }
        if (view != "")
            curView = viewList.findIndex(view);
    }
    else if (type == plCategory) // list by category
    {
        MSqlQuery query(MSqlQuery::InitCon()); 
        query.prepare("SELECT category FROM program GROUP BY category;");
        query.exec();

        if (query.isActive() && query.size())
        {
            while (query.next())
            {
                QString category = query.value(0).toString();
                if (category <= " " || category == NULL)
                    continue;
                category = QString::fromUtf8(query.value(0).toString());
                viewList << category;
                viewTextList << category;
            }
        }
        if (view != "")
            curView = viewList.findIndex(view);
    }
    else if (type == plTitleSearch || type == plKeywordSearch ||
             type == plPeopleSearch || type == plPowerSearch)
    {
        MSqlQuery query(MSqlQuery::InitCon()); 
        query.prepare("SELECT phrase FROM keyword "
                      "WHERE searchtype = :SEARCHTYPE;");
        query.bindValue(":SEARCHTYPE", searchtype);
        query.exec();

        if (query.isActive() && query.size())
        {
            while (query.next())
            {
                QString phrase = query.value(0).toString();
                if (phrase <= " ")
                    continue;
                phrase = QString::fromUtf8(query.value(0).toString());
                viewList << phrase;
                viewTextList << phrase;
            }
        }
        if (view != "")
        {
            curView = viewList.findIndex(view);

            if (curView < 0)
            {
                QString qphrase = view.utf8();

                MSqlQuery query(MSqlQuery::InitCon()); 
                query.prepare("REPLACE INTO keyword (phrase, searchtype)"
                              "VALUES(:VIEW, :SEARCHTYPE );");
                query.bindValue(":VIEW", qphrase);
                query.bindValue(":SEARCHTYPE", searchtype);
                query.exec();

                viewList << qphrase;
                viewTextList << qphrase;

                curView = viewList.count() - 1;
            }
        }
        else
            curView = -1;
    }
    else if (type == plTitle)
    {
        if (view != "")
        {
            viewList << view;
            viewTextList << view;
            curView = 0;
        }
        else
            curView = -1;
    }
    else if (type == plNewListings)
    {
        viewList << "all";
        viewTextList << tr("All");

        viewList << "premieres";
        viewTextList << tr("Premieres");

        viewList << "movies";
        viewTextList << tr("Movies");

        viewList << "series";
        viewTextList << tr("Series");

        viewList << "specials";
        viewTextList << tr("Specials");
        curView = 0;
    }
    else if (type == plMovies)
    {
        viewList << "0.0";
        viewTextList << tr("All");
        viewList << "1.0";
        viewTextList << tr("4 stars");
        viewList << "0.875";
        viewTextList << tr("At least 3 1/2 stars");
        viewList << "0.75";
        viewTextList << tr("At least 3 stars");
        viewList << "0.5";
        viewTextList << tr("At least 2 stars");
        curView = 0;
    }
    else if (type == plTime)
    {
        curView = 0;
        viewList[curView] = searchTime.toString(fullDateFormat);
        viewTextList[curView] = viewList[curView];
    }
    else if (type == plSQLSearch)
    {
        curView = 0;
        viewList << view;
        viewTextList << tr("Power Recording Rule");
    }
    if (curView >= (int)viewList.count())
        curView = viewList.count() - 1;
}

class plTitleSort
{
    public:
        plTitleSort(void) {;}

        bool operator()(const ProgramInfo *a, const ProgramInfo *b) 
        {
            if (a->sortTitle != b->sortTitle)
                    return (a->sortTitle < b->sortTitle);

            if (a->recstatus == b->recstatus)
                return a->startts < b->startts;

            if (a->recstatus == rsRecording) return true;
            if (b->recstatus == rsRecording) return false;

            if (a->recstatus == rsWillRecord) return true;
            if (b->recstatus == rsWillRecord) return false;

            return a->startts < b->startts;
        }
};

class plTimeSort
{
    public:
        plTimeSort(void) {;}

        bool operator()(const ProgramInfo *a, const ProgramInfo *b) 
        {
            if (a->startts == b->startts)
                return (a->chanid < b->chanid);

            return (a->startts < b->startts);
        }
};

void ProgLister::fillItemList(void)
{
    if (curView < 0)
         return;

    QString where;
    QString startstr = startTime.toString("yyyy-MM-ddThh:mm:50");
    QString qphrase = viewList[curView].utf8();

    MSqlBindings bindings;
    bindings[":PGILSTART"] = startstr;
    bindings[":PGILPHRASE"] = qphrase;
    bindings[":PGILLIKEPHRASE"] = QString("%") + qphrase + "%";

    if (type == plTitle) // per title listings
    {
        where = "WHERE channel.visible = 1 "
                "  AND program.endtime > :PGILSTART "
                "  AND program.title = :PGILPHRASE ";
    }
    else if (type == plNewListings) // what's new list
    {
        where = "LEFT JOIN oldprogram ON "
                "  oldprogram.oldtitle = program.title "
                "WHERE channel.visible = 1 "
                "  AND program.endtime > :PGILSTART "
                "  AND oldprogram.oldtitle IS NULL "
                "  AND program.manualid = 0 ";

        if (qphrase == "premieres")
        {
            where += "  AND ( ";
            where += "    ( program.previouslyshown = 0 ";
            where += "      AND (program.category = 'Special' ";
            where += "        OR program.programid LIKE 'EP%0001') ";
            where += "      AND DAYOFYEAR(program.originalairdate) = "; 
            where += "          DAYOFYEAR(program.starttime)) ";
            where += "    OR (program.category_type='movie' ";
            where += "      AND program.stars > 0.5 ";
            where += "      AND program.airdate >= YEAR(NOW()) - 2) ";
            where += "  ) ";
        }
        else if (qphrase == "movies")
        {
            where += "  AND program.category_type = 'movie' ";
        }
        else if (qphrase == "series")
        {
            where += "  AND program.category_type = 'series' ";
        }
        else if (qphrase == "specials")
        {
            where += "  AND program.category_type = 'tvshow' ";
        }
        else
        {
            where += "  AND (program.category_type <> 'movie' ";
            where += "  OR program.airdate >= YEAR(NOW()) - 3) ";
        }
    }
    else if (type == plTitleSearch) // keyword search
    {
        where = "WHERE channel.visible = 1 "
                "  AND program.endtime > :PGILSTART "
                "  AND program.title LIKE :PGILLIKEPHRASE ";
    }
    else if (type == plKeywordSearch) // keyword search
    {
        where = "WHERE channel.visible = 1 "
                "  AND program.endtime > :PGILSTART "
                "  AND (program.title LIKE :PGILLIKEPHRASE "
                "    OR program.subtitle LIKE :PGILLIKEPHRASE "
                "    OR program.description LIKE :PGILLIKEPHRASE ) ";
    }
    else if (type == plPeopleSearch) // people search
    {
        where = ", people, credits WHERE channel.visible = 1 "
                "  AND program.endtime > :PGILSTART "
                "  AND people.name LIKE :PGILPHRASE "
                "  AND credits.person = people.person "
                "  AND program.chanid = credits.chanid "
                "  AND program.starttime = credits.starttime";
    }
    else if (type == plPowerSearch) // complex search
    {
        QString powerWhere;
        MSqlBindings powerBindings;

        powerStringToSQL(qphrase, powerWhere, powerBindings);

        if (powerWhere != "")
        {
            where = QString("WHERE channel.visible = 1 "
                    "  AND program.endtime > :PGILSTART "
                    "  AND ( ") + powerWhere + " ) ";
            MSqlAddMoreBindings(bindings, powerBindings);
        }
    }
    else if (type == plSQLSearch) // complex search
    {
        qphrase.remove(QRegExp("^\\s*AND\\s+", false));
        where = QString("WHERE channel.visible = 1 "
                        "  AND program.endtime > :PGILSTART "
                        "  AND ( %1 ) ").arg(qphrase);
        if (addTables > "")
            where = addTables + " " + where;
    }
    else if (type == plChannel) // list by channel
    {
        where = "WHERE channel.visible = 1 "
                "  AND program.endtime > :PGILSTART "
                "  AND channel.chanid = :PGILPHRASE ";
    }
    else if (type == plCategory) // list by category
    {
        where = "WHERE channel.visible = 1 "
                "  AND program.endtime > :PGILSTART "
                "  AND program.category = :PGILPHRASE ";
    }
    else if (type == plMovies) // list movies
    {
        where = "WHERE channel.visible = 1 "
                "  AND program.endtime > :PGILSTART "
                "  AND program.category_type = 'movie' "
                "  AND program.stars >= :PGILPHRASE ";
    }
    else if (type == plTime) // list by time
    {
        bindings[":PGILSEARCHTIME"] = searchTime.toString("yyyy-MM-dd hh:00:00");
        where = "WHERE channel.visible = 1 "
                "  AND program.starttime >= :PGILSEARCHTIME ";
        if (titleSort)
            where += "  AND program.starttime < DATE_ADD(:PGILSEARCHTIME, "
                     "INTERVAL '1' HOUR) ";
    }

    schedList.FromScheduler();
    itemList.FromProgram(where, bindings, schedList);

    ProgramInfo *s;
    vector<ProgramInfo *> sortedList;

    while (itemList.count())
    {
        s = itemList.take();
        if (type == plTitle)
            s->sortTitle = s->subtitle;
        else
            s->sortTitle = s->title;

        s->sortTitle.remove(QRegExp("^(The |A |An )"));
        sortedList.push_back(s);
    }

    if (type == plNewListings || titleSort)
    {
        // Prune to one per title
        sort(sortedList.begin(), sortedList.end(), plTitleSort());

        QString curtitle = "";
        vector<ProgramInfo *>::iterator i = sortedList.begin();
        while (i != sortedList.end())
        {
            ProgramInfo *p = *i;
            if (p->sortTitle != curtitle)
            {
                curtitle = p->sortTitle;
                i++;
            }
            else
            {
                delete p;
                i = sortedList.erase(i);
            }
        }
    }
    if (!titleSort)
        sort(sortedList.begin(), sortedList.end(), plTimeSort());

    if (reverseSort)
    {
        vector<ProgramInfo *>::reverse_iterator r = sortedList.rbegin();
        for (; r != sortedList.rend(); r++)
            itemList.append(*r);
    }
    else
    {
        vector<ProgramInfo *>::iterator i = sortedList.begin();
        for (; i != sortedList.end(); i++)
            itemList.append(*i);
    }

    if (curItem < 0 && itemList.count() > 0)
        curItem = 0;
    else if (curItem >= (int)itemList.count())
        curItem = itemList.count() - 1;
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
        UITextType *uitype = (UITextType *)container->GetType("curview");
        if (uitype && curView >= 0)
            uitype->SetText(viewTextList[curView]);

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

            QStringList starMap;
            QString starstr = "";
            for (int i = 0; i <= 4; i++)
            {
                starMap << starstr;
                starMap << starstr + "/";
                starstr += "*";
            }

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

                if (pi->stars > 0.0)
                    tmptitle = QString("%1 (%2, %3 )")
                                       .arg(pi->title).arg(pi->year)
                                       .arg(starMap[(int) (pi->stars * 8)]);
                else if (pi->subtitle == "")
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

                if (pi->recstatus == rsConflict ||
                    pi->recstatus == rsOffLine)
                    ltype->EnableForcedFont(i, "conflicting");
                else if (pi->recstatus == rsRecording)
                    ltype->EnableForcedFont(i, "recording");
                else if (pi->recstatus == rsWillRecord)
                    ltype->EnableForcedFont(i, "record");

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

