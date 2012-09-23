
#include "mythdialogbox.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QDateTime>
#include <QDate>
#include <QTime>

#include "mythlogging.h"
#include "mythdate.h"

#include "mythmainwindow.h"
#include "mythfontproperties.h"
#include "mythuiutils.h"
#include "mythuitext.h"
#include "mythuiimage.h"
#include "mythuibuttonlist.h"
#include "mythuibutton.h"
#include "mythuistatetype.h"

QEvent::Type DialogCompletionEvent::kEventType =
    (QEvent::Type) QEvent::registerEventType();


MythMenu::MythMenu(const QString &text, QObject *retobject, const QString &resultid) :
    m_parentMenu(NULL),  m_title(""), m_text(text), m_resultid(resultid), m_retObject(retobject), m_selectedItem(0)
{
    Init();
}

MythMenu::MythMenu(const QString &title, const QString &text, QObject *retobject, const QString &resultid) :
    m_parentMenu(NULL),  m_title(title), m_text(text), m_resultid(resultid), m_retObject(retobject), m_selectedItem(0)
{
    Init();
}

MythMenu::~MythMenu(void)
{
    while (!m_menuItems.isEmpty())
    {
        MythMenuItem *item = m_menuItems.takeFirst();

        if (item->SubMenu)
            delete item->SubMenu;

        delete item;
    }
}

void MythMenu::Init()
{
    m_title.detach();
    m_text.detach();
    m_resultid.detach();
}

void MythMenu::AddItem(const QString& title, const char* slot, MythMenu *subMenu, bool selected, bool checked)
{
    MythMenuItem *item = new MythMenuItem(title, slot, checked, subMenu);

    m_menuItems.append(item);

    if (selected)
        m_selectedItem = m_menuItems.indexOf(item);

    if (subMenu)
        subMenu->SetParent(this);
}

void MythMenu::AddItem(const QString &title, QVariant data, MythMenu *subMenu, bool selected, bool checked)
{
    MythMenuItem *item = new MythMenuItem(title, data, checked, subMenu);

    m_menuItems.append(item);

    if (selected)
        m_selectedItem = m_menuItems.indexOf(item);

    if (subMenu)
        subMenu->SetParent(this);
}

/////////////////////////////////////////////////////////////////

MythDialogBox::MythDialogBox(const QString &text,
                             MythScreenStack *parent, const char *name,
                             bool fullscreen, bool osd)
         : MythScreenType(parent, name, false)
{
    m_menu = NULL;
    m_currentMenu = NULL;
    m_retObject = NULL;
    m_titlearea = NULL;
    m_text = text;
    m_textarea = NULL;
    m_buttonList = NULL;

    m_fullscreen = fullscreen;
    m_osdDialog  = osd;
    m_useSlots = false;

    m_backtext = "";
    m_backdata = 0;
    m_exittext = "";
    m_exitdata = 0;
}

MythDialogBox::MythDialogBox(const QString &title, const QString &text,
                             MythScreenStack *parent, const char *name,
                             bool fullscreen, bool osd)
         : MythScreenType(parent, name, false)
{
    m_menu = NULL;
    m_currentMenu = NULL;
    m_id = "";
    m_retObject = NULL;
    m_title = title;
    m_titlearea = NULL;
    m_text = text;
    m_textarea = NULL;
    m_buttonList = NULL;

    m_fullscreen = fullscreen;
    m_osdDialog  = osd;
    m_useSlots = false;

    m_backtext = "";
    m_backdata = 0;
    m_exittext = "";
    m_exitdata = 0;
}

MythDialogBox::MythDialogBox(MythMenu *menu, MythScreenStack *parent, const char *name,
                               bool fullscreen, bool osd)
         : MythScreenType(parent, name, false)
{
    m_menu = menu;
    m_currentMenu = m_menu;
    m_id = "";
    m_retObject = NULL;
    m_title = "";
    m_titlearea = NULL;
    m_textarea = NULL;
    m_buttonList = NULL;

    m_fullscreen = fullscreen;
    m_osdDialog  = osd;
    m_useSlots = false;

    m_backtext = "";
    m_backdata = 0;
    m_exittext = "";
    m_exitdata = 0;
}

MythDialogBox::~MythDialogBox(void)
{
    if (m_menu)
    {
        delete m_menu;
        m_menu = NULL;
    }
}

bool MythDialogBox::Create(void)
{
    QString windowName = (m_fullscreen ? "MythDialogBox" : "MythPopupBox");

    if (m_osdDialog)
    {
        if (!XMLParseBase::LoadWindowFromXML("osd.xml", windowName, this))
            return false;
    }
    else if (!CopyWindowFromBase(windowName, this))
        return false;

    bool err = false;
    UIUtilW::Assign(this, m_titlearea, "title");
    UIUtilE::Assign(this, m_textarea, "messagearea", &err);
    UIUtilE::Assign(this, m_buttonList, "list", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Cannot load screen '%1'")
                                        .arg(windowName));
        return false;
    }

    if (m_titlearea)
        m_titlearea->SetText(m_title);
    m_textarea->SetText(m_text);

    BuildFocusList();

    if (m_menu)
        updateMenu();

    connect(m_buttonList, SIGNAL(itemClicked(MythUIButtonListItem*)),
            SLOT(Select(MythUIButtonListItem*)));

    return true;
}

void MythDialogBox::SetMenuItems(MythMenu* menu)
{
    m_menu = menu;
    m_currentMenu = m_menu;
    updateMenu();
}

void MythDialogBox::updateMenu(void)
{
    if (!m_buttonList)
    {
         LOG(VB_GENERAL, LOG_ERR, "UpdateMenu() called before we have a button list to update!");
         return;
    }

    if (!m_currentMenu)
        return;

    if (m_titlearea)
        m_titlearea->SetText(m_currentMenu->m_title);

    m_textarea->SetText(m_currentMenu->m_text);

    m_buttonList->Reset();

    for (int x = 0; x < m_currentMenu->m_menuItems.count(); x++)
    {
        MythMenuItem *menuItem = m_currentMenu->m_menuItems.at(x);
        MythUIButtonListItem *button = new MythUIButtonListItem(m_buttonList, menuItem->Text);
        button->SetData(qVariantFromValue(menuItem));
        button->setDrawArrow((menuItem->SubMenu != NULL));

        if (m_currentMenu->m_selectedItem == x)
            m_buttonList->SetItemCurrent(button);
    }
}

void MythDialogBox::Select(MythUIButtonListItem* item)
{
    if (!item)
        return;

    if (m_currentMenu)
    {
        MythMenuItem *menuItem = qVariantValue<MythMenuItem *>(item->GetData());

        if (menuItem->SubMenu)
        {
            m_currentMenu->m_selectedItem = m_buttonList->GetCurrentPos();
            m_currentMenu = menuItem->SubMenu;
            updateMenu();
            return;
        }

        const char *slot = qVariantValue<const char *>(menuItem->Data);
        if (menuItem->UseSlot && slot)
        {
            connect(this, SIGNAL(Selected()), m_currentMenu->m_retObject, slot,
                    Qt::QueuedConnection);
            emit Selected();
        }

        SendEvent(m_buttonList->GetItemPos(item), item->GetText(), menuItem->Data);
    }
    else
    {
        const char *slot = qVariantValue<const char *>(item->GetData());
        if (m_useSlots && slot)
        {
            connect(this, SIGNAL(Selected()), m_retObject, slot,
                    Qt::QueuedConnection);
            emit Selected();
        }

        SendEvent(m_buttonList->GetItemPos(item), item->GetText(), item->GetData());
    }

    if (m_ScreenStack)
        m_ScreenStack->PopScreen(false);
}

void MythDialogBox::SetReturnEvent(QObject *retobject,
                               const QString &resultid)
{
    m_retObject = retobject;
    m_id = resultid;
}

void MythDialogBox::SetBackAction(const QString &text, QVariant data)
{
    m_backtext = text;
    m_backdata = data;
}

void MythDialogBox::SetExitAction(const QString &text, QVariant data)
{
    m_exittext = text;
    m_exitdata = data;
}

void MythDialogBox::SetText(const QString &text)
{
    if (m_textarea)
        m_textarea->SetText(text);
}

void MythDialogBox::AddButton(const QString &title, QVariant data, bool newMenu,
                              bool setCurrent)
{
    MythUIButtonListItem *button = new MythUIButtonListItem(m_buttonList, title);
    button->SetData(data);
    button->setDrawArrow(newMenu);

    if (setCurrent)
        m_buttonList->SetItemCurrent(button);
}

void MythDialogBox::AddButton(const QString &title, const char *slot,
                              bool newMenu, bool setCurrent)
{
    MythUIButtonListItem *button = new MythUIButtonListItem(m_buttonList, title);

    m_useSlots = true;

    if (slot)
        button->SetData(qVariantFromValue(slot));
    button->setDrawArrow(newMenu);

    if (setCurrent)
        m_buttonList->SetItemCurrent(button);
}

bool MythDialogBox::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("qt", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "ESCAPE")
        {
            SendEvent(-1, m_exittext, m_exitdata);
            if (m_exitdata == 0 && m_exittext.isEmpty())
                Close();
        }
        else if ((action == "LEFT" &&
             m_buttonList->GetLayout() == MythUIButtonList::LayoutVertical) ||
            (action == "UP" &&
             m_buttonList->GetLayout() == MythUIButtonList::LayoutHorizontal))
        {
            if (m_currentMenu && m_currentMenu->m_parentMenu)
            {
                m_currentMenu = m_currentMenu->m_parentMenu;
                updateMenu();
                return true;
            }

            SendEvent(-1, m_backtext, m_backdata);
            Close();
        }
        else if (action == "MENU")
        {
            SendEvent(-2);
            Close();
        }
        else if ((action == "RIGHT" &&
                  m_buttonList->GetLayout() == MythUIButtonList::LayoutVertical) ||
                 (action == "DOWN" &&
                  m_buttonList->GetLayout() == MythUIButtonList::LayoutHorizontal))
        {
            Select(m_buttonList->GetItemCurrent());
        }
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

bool MythDialogBox::gestureEvent(MythGestureEvent *event)
{
    bool handled = false;
    if (event->gesture() == MythGestureEvent::Click)
    {
        switch (event->GetButton())
        {
            case MythGestureEvent::RightButton :
                SendEvent(-2);
                Close();
                handled = true;
                break;
            default :
                break;
        }

    }

    if (!handled && MythScreenType::gestureEvent(event))
        handled = true;

    return handled;
}

void MythDialogBox::SendEvent(int res, QString text, QVariant data)
{
    if (m_currentMenu)
    {
        emit Closed(m_currentMenu->m_resultid, res);

        if (!m_currentMenu->m_retObject)
            return;

        DialogCompletionEvent *dce = new DialogCompletionEvent(m_currentMenu->m_resultid, res, text, data);
        QCoreApplication::postEvent(m_currentMenu->m_retObject, dce);
    }
    else
    {
        emit Closed(m_id, res);

        if (!m_retObject)
            return;

        DialogCompletionEvent *dce = new DialogCompletionEvent(m_id, res, text, data);
        QCoreApplication::postEvent(m_retObject, dce);
    }
}

/////////////////////////////////////////////////////////////////

MythConfirmationDialog::MythConfirmationDialog(MythScreenStack *parent,
                                               const QString &message,
                                               bool showCancel)
                       : MythScreenType(parent, "mythconfirmpopup")
{
    m_messageText = NULL;
    m_message = message;
    m_showCancel = showCancel;

    m_id = "";
    m_retObject = NULL;
}

bool MythConfirmationDialog::Create(void)
{
    if (!CopyWindowFromBase("MythConfirmationDialog", this))
        return false;

    MythUIButton *okButton = NULL;
    MythUIButton *cancelButton = NULL;

    bool err = false;
    UIUtilE::Assign(this, m_messageText, "message", &err);
    UIUtilE::Assign(this, okButton, "ok", &err);
    UIUtilE::Assign(this, cancelButton, "cancel", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'MythConfirmationDialog'");
        return false;
    }

    if (m_showCancel)
    {
        connect(cancelButton, SIGNAL(Clicked()), SLOT(Cancel()));
    }
    else
        cancelButton->SetVisible(false);

    connect(okButton, SIGNAL(Clicked()), SLOT(Confirm()));

    m_messageText->SetText(m_message);

    BuildFocusList();

    if (m_showCancel)
        SetFocusWidget(cancelButton);
    else
        SetFocusWidget(okButton);

    return true;
}

bool MythConfirmationDialog::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("qt", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "ESCAPE")
            sendResult(false);
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void MythConfirmationDialog::SetMessage(const QString &message)
{
    m_message = message;
    if (m_messageText)
        m_messageText->SetText(m_message);
}

void MythConfirmationDialog::SetReturnEvent(QObject *retobject,
                                            const QString &resultid)
{
    m_retObject = retobject;
    m_id = resultid;
}

void MythConfirmationDialog::Confirm()
{
    sendResult(true);
}

void MythConfirmationDialog::Cancel()
{
    sendResult(false);
}

void MythConfirmationDialog::sendResult(bool ok)
{
    emit haveResult(ok);

    if (m_retObject)
    {
        int res = 0;
        if (ok)
            res = 1;

        DialogCompletionEvent *dce = new DialogCompletionEvent(m_id, res, "",
                                                               m_resultData);
        QCoreApplication::postEvent(m_retObject, dce);
    }

    Close();
}

/**
 * Non-blocking version of MythPopupBox::showOkPopup()
 */
MythConfirmationDialog  *ShowOkPopup(const QString &message, QObject *parent,
                                     const char *slot, bool showCancel)
{
    QString                  LOC = "ShowOkPopup('" + message + "') - ";
    MythConfirmationDialog  *pop;
    MythScreenStack         *stk = NULL;

    MythMainWindow *win = GetMythMainWindow();

    if (win)
        stk = win->GetStack("popup stack");
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "no main window?");
        return NULL;
    }

    if (!stk)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "no popup stack? "
                                       "Is there a MythThemeBase?");
        return NULL;
    }

    pop = new MythConfirmationDialog(stk, message, showCancel);
    if (pop->Create())
    {
        stk->AddScreen(pop);
        if (parent && slot)
            QObject::connect(pop, SIGNAL(haveResult(bool)), parent, slot,
                             Qt::QueuedConnection);
    }
    else
    {
        delete pop;
        pop = NULL;
        LOG(VB_GENERAL, LOG_ERR, LOC + "Couldn't Create() Dialog");
    }

    return pop;
}

/////////////////////////////////////////////////////////////////

MythTextInputDialog::MythTextInputDialog(MythScreenStack *parent,
                                         const QString &message,
                                         InputFilter filter,
                                         bool isPassword,
                                         const QString &defaultValue)
           : MythScreenType(parent, "mythtextinputpopup")
{
    m_filter = filter;
    m_isPassword = isPassword;
    m_message = message;
    m_defaultValue = defaultValue;
    m_textEdit = NULL;

    m_id = "";
    m_retObject = NULL;
}

bool MythTextInputDialog::Create(void)
{
    if (!CopyWindowFromBase("MythTextInputDialog", this))
        return false;

    MythUIText *messageText = NULL;
    MythUIButton *okButton = NULL;
    MythUIButton *cancelButton = NULL;

    bool err = false;
    UIUtilE::Assign(this, m_textEdit, "input", &err);
    UIUtilE::Assign(this, messageText, "message", &err);
    UIUtilE::Assign(this, okButton, "ok", &err);
    UIUtilW::Assign(this, cancelButton, "cancel");

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'MythTextInputDialog'");
        return false;
    }

    if (cancelButton)
        connect(cancelButton, SIGNAL(Clicked()), SLOT(Close()));
    connect(okButton, SIGNAL(Clicked()), SLOT(sendResult()));

    m_textEdit->SetFilter(m_filter);
    m_textEdit->SetText(m_defaultValue);
    m_textEdit->SetPassword(m_isPassword);

    messageText->SetText(m_message);

    BuildFocusList();

    return true;
}

void MythTextInputDialog::SetReturnEvent(QObject *retobject,
                                         const QString &resultid)
{
    m_retObject = retobject;
    m_id = resultid;
}

void MythTextInputDialog::sendResult()
{
    QString inputString = m_textEdit->GetText();
    emit haveResult(inputString);

    if (m_retObject)
    {
        DialogCompletionEvent *dce = new DialogCompletionEvent(m_id, 0,
                                                            inputString, "");
        QCoreApplication::postEvent(m_retObject, dce);
    }

    Close();
}

/////////////////////////////////////////////////////////////////////


/** \fn MythUISearchDialog::MythUISearchDialog(MythScreenStack*,
                                   const QString&,
                                   const QStringList&,
                                   bool  matchAnywhere,
                                   const QString&)
 *  \brief the classes constructor
 *  \param parent the MythScreenStack this widget belongs to
 *  \param title  the text to show as the title
 *  \param list   the list of text strings to search
 *  \param matchAnywhere if true will match the input text anywhere in the string.
                         if false will match only strings that start with the input text.
                         Default is false.
 *  \param defaultValue  The initial value for the input text. Default is ""
 */
MythUISearchDialog::MythUISearchDialog(MythScreenStack *parent,
                                   const QString &title,
                                   const QStringList &list,
                                   bool  matchAnywhere,
                                   const QString &defaultValue)
                : MythScreenType(parent, "mythsearchdialogpopup")
{
    m_list = list;
    m_matchAnywhere = matchAnywhere;
    m_title = title;
    m_defaultValue = defaultValue;

    m_titleText = NULL;
    m_matchesText = NULL;
    m_textEdit = NULL;
    m_itemList = NULL;

    m_id = "";
    m_retObject = NULL;
}

bool MythUISearchDialog::Create(void)
{
    if (!CopyWindowFromBase("MythSearchDialog", this))
        return false;

    MythUIButton *okButton = NULL;
    MythUIButton *cancelButton = NULL;

    bool err = false;
    UIUtilE::Assign(this, m_textEdit, "input", &err);
    UIUtilE::Assign(this, m_titleText, "title", &err);
    UIUtilW::Assign(this, m_matchesText, "matches");
    UIUtilE::Assign(this, m_itemList, "itemlist", &err);
    UIUtilE::Assign(this, okButton, "ok", &err);
    UIUtilW::Assign(this, cancelButton, "cancel");

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'MythSearchDialog'");
        return false;
    }

    if (cancelButton)
        connect(cancelButton, SIGNAL(Clicked()), SLOT(Close()));

    connect(okButton, SIGNAL(Clicked()), SLOT(slotSendResult()));

    connect(m_itemList, SIGNAL(itemClicked(MythUIButtonListItem*)),
            SLOT(slotSendResult()));

    m_textEdit->SetText(m_defaultValue);
    connect(m_textEdit, SIGNAL(valueChanged()), SLOT(slotUpdateList()));

    m_titleText->SetText(m_title);
    if (m_matchesText)
        m_matchesText->SetText(tr("%n match(es)", "", 0));

    BuildFocusList();

    slotUpdateList();

    return true;
}

void MythUISearchDialog::SetReturnEvent(QObject *retobject,
                                        const QString &resultid)
{
    m_retObject = retobject;
    m_id = resultid;
}

void MythUISearchDialog::slotSendResult()
{
    if (!m_itemList->GetItemCurrent())
        return;

    QString result = m_itemList->GetValue();

    emit haveResult(result);

    if (m_retObject)
    {
        DialogCompletionEvent *dce = new DialogCompletionEvent(m_id, 0,
                                                            result, "");
        QCoreApplication::postEvent(m_retObject, dce);
    }

    Close();
}

void MythUISearchDialog::slotUpdateList(void)
{
    m_itemList->Reset();

    for (int x = 0; x < m_list.size(); x++)
    {
        QString item = m_list.at(x);

        if (m_matchAnywhere)
        {
            if (!item.contains(m_textEdit->GetText(), Qt::CaseInsensitive))
                continue;
        }
        else
        {
            if (!item.startsWith(m_textEdit->GetText(), Qt::CaseInsensitive))
                continue;
        }

        // add item to list
        new MythUIButtonListItem(m_itemList, item, NULL, false);
    }

    m_itemList->SetItemCurrent(0);

    if (m_matchesText)
        m_matchesText->SetText(tr("%n match(es)", "", 0));
}

////////////////////////////////////////////////////////////////////////

MythTimeInputDialog::MythTimeInputDialog(MythScreenStack *parent,
                                         const QString &message,
                                         int resolutionFlags,
                                         QDateTime startTime,
                                         int rangeLimit)
    : MythScreenType(parent, "timepopup"),
        m_message(message), m_startTime(startTime),
        m_resolution(resolutionFlags), m_rangeLimit(rangeLimit),
        m_dateList(NULL), m_timeList(NULL), m_retObject(NULL)
{
}

bool MythTimeInputDialog::Create()
{
    if (!CopyWindowFromBase("MythTimeInputDialog", this))
        return false;

    MythUIText *messageText = NULL;
    MythUIButton *okButton = NULL;

    bool err = false;
    UIUtilE::Assign(this, messageText, "message", &err);
    UIUtilE::Assign(this, m_dateList, "dates", &err);
    UIUtilE::Assign(this, m_timeList, "times", &err);
    UIUtilE::Assign(this, okButton, "ok", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'MythTimeInputDialog'");
        return false;
    }

    m_dateList->SetVisible(false);
    m_timeList->SetVisible(false);

    MythUIButtonListItem *item;
    // Date
    if (kNoDate != (m_resolution & 0xF))
    {
        const QDate startdate(m_startTime.toLocalTime().date());
        QDate date(startdate);

        int limit = 0;
        if (m_resolution & kFutureDates)
        {
            limit += m_rangeLimit;
        }
        if (m_resolution & kPastDates)
        {
            limit += m_rangeLimit;
            date = date.addDays(0-m_rangeLimit);
        }

        QString text;
        int flags;
        bool selected = false;
        for (int x = 0; x <= limit; x++)
        {
            selected = false;
            if (m_resolution & kDay)
            {
                date = date.addDays(1);
                flags = MythDate::kDateFull | MythDate::kSimplify;
                if (m_rangeLimit >= 356)
                    flags |= MythDate::kAddYear;
                text = MythDate::toString(date, flags);

                if (date == startdate)
                    selected = true;
            }
            else if (m_resolution & kMonth)
            {
                date = date.addMonths(1);
                text = date.toString("MMM yyyy");

                if ((date.month() == startdate.month()) &&
                    (date.year() == startdate.year()))
                    selected = true;
            }
            else if (m_resolution & kYear)
            {
                date = date.addYears(1);
                text = date.toString("yyyy");
                if (date.year() == startdate.year())
                    selected = true;
            }

            item = new MythUIButtonListItem(m_dateList, text, NULL, false);
            item->SetData(QVariant(date));

            if (selected)
                m_dateList->SetItemCurrent(item);
        }
        m_dateList->SetVisible(true);
    }

    // Time
    if (kNoTime != (m_resolution & 0xF0))
    {
        QDate startdate(m_startTime.toLocalTime().date());
        QTime starttime(m_startTime.toLocalTime().time());
        QTime time(0,0,0);
        QString text;
        bool selected = false;

        int limit = (m_resolution & kMinutes) ? (60 * 24) : 24;

        for (int x = 0; x < limit; x++)
        {
            selected = false;
            if (m_resolution & kMinutes)
            {
                time = time.addSecs(60);
                QDateTime dt = QDateTime(startdate, time, Qt::LocalTime);
                text = MythDate::toString(dt, MythDate::kTime);

                if (time == starttime)
                    selected = true;
            }
            else if (m_resolution & kHours)
            {
                time = time.addSecs(60*60);
                text = time.toString("hh:00");

                if (time.hour() == starttime.hour())
                    selected = true;
            }

            item = new MythUIButtonListItem(m_timeList, text, NULL, false);
            item->SetData(QVariant(time));

            if (selected)
                m_timeList->SetItemCurrent(item);
        }
        m_timeList->SetVisible(true);
    }

    if (messageText && !m_message.isEmpty())
        messageText->SetText(m_message);

    connect(okButton, SIGNAL(Clicked()), SLOT(okClicked()));

    BuildFocusList();

    return true;
}

void MythTimeInputDialog::SetReturnEvent(QObject* retobject,
                                         const QString& resultid)
{
    m_retObject = retobject;
    m_id = resultid;
}

void MythTimeInputDialog::okClicked(void)
{
    QDate date = m_dateList->GetDataValue().toDate();
    QTime time = m_timeList->GetDataValue().toTime();

    QDateTime dateTime = QDateTime(date, time, Qt::LocalTime).toUTC();

    emit haveResult(dateTime);

    if (m_retObject)
    {
        QVariant data(dateTime);
        DialogCompletionEvent *dce = new DialogCompletionEvent(m_id, 0, "",
                                                               data);
        QCoreApplication::postEvent(m_retObject, dce);
    }

    Close();
}
