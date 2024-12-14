
#include "mythdialogbox.h"

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <QTime>
#include <utility>

#include "libmythbase/mythdate.h"
#include "libmythbase/mythlogging.h"

#include "mythmainwindow.h"
#include "mythfontproperties.h"
#include "mythuiutils.h"
#include "mythuitext.h"
#include "mythuiimage.h"
#include "mythuibuttonlist.h"
#include "mythuibutton.h"
#include "mythuistatetype.h"
#include "mythuispinbox.h"
#include "mythgesture.h"

const QEvent::Type DialogCompletionEvent::kEventType =
    (QEvent::Type) QEvent::registerEventType();

// Force this class to have a vtable so that dynamic_cast works.
// NOLINTNEXTLINE(modernize-use-equals-default)
DialogCompletionEvent::~DialogCompletionEvent()
{
}

MythMenu::MythMenu(QString text, QObject *retobject, QString resultid) :
    m_text(std::move(text)), m_resultid(std::move(resultid)), m_retObject(retobject)
{
    Init();
}

MythMenu::MythMenu(QString title, QString text, QObject *retobject, QString resultid) :
    m_title(std::move(title)), m_text(std::move(text)), m_resultid(std::move(resultid)),
    m_retObject(retobject)
{
    Init();
}

MythMenu::~MythMenu(void)
{
    while (!m_menuItems.isEmpty())
    {
        MythMenuItem *item = m_menuItems.takeFirst();

        delete item->m_subMenu;
        delete item;
    }
}

void MythMenu::AddItemV(const QString &title, QVariant data, MythMenu *subMenu, bool selected, bool checked)
{
    auto *item = new MythMenuItem(title, checked, subMenu);
    item->SetData(std::move(data));
    AddItem(item, selected, subMenu);
}

// For non-class, static class, or lambda functions.
void MythMenu::AddItem(const QString &title, const MythUICallbackNMF &slot,
                       MythMenu *subMenu, bool selected, bool checked)
{
    auto *item = new MythMenuItem(title, slot, checked, subMenu);
    AddItem(item, selected, subMenu);
}

void MythMenu::AddItem(MythMenuItem *item, bool selected, MythMenu *subMenu)
{
    m_menuItems.append(item);

    if (selected)
        m_selectedItem = m_menuItems.indexOf(item);

    if (subMenu)
        subMenu->SetParent(this);
}

void MythMenu::SetSelectedByTitle(const QString& title)
{
    for (auto *item : std::as_const(m_menuItems))
    {
        if (!item)
            continue;

        if (item->m_text == title)
        {
            m_selectedItem = m_menuItems.indexOf(item);
            break;
        }
    }
}

void MythMenu::SetSelectedByData(const QVariant& data)
{
    for (auto *item : std::as_const(m_menuItems))
    {
        if (!item)
            continue;

        if (item->m_data == data)
        {
            m_selectedItem = m_menuItems.indexOf(item);
            break;
        }
    }
}

/////////////////////////////////////////////////////////////////

MythDialogBox::~MythDialogBox(void)
{
    if (m_menu)
    {
        delete m_menu;
        m_menu = nullptr;
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
    {
        return false;
    }

    bool err = false;
    if (m_fullscreen)
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

    connect(m_buttonList, &MythUIButtonList::itemClicked,
            this, &MythDialogBox::Select);

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
        auto *button = new MythUIButtonListItem(m_buttonList, menuItem->m_text);
        button->SetData(QVariant::fromValue(menuItem));
        button->setDrawArrow((menuItem->m_subMenu != nullptr));

        if (m_currentMenu->m_selectedItem == x)
            m_buttonList->SetItemCurrent(button);
    }
    // GetVisibleCount here makes sure that the dialog size is
    // calculated correctly
    m_buttonList->GetVisibleCount();
    GetMythMainWindow()->GetMainStack()->GetTopScreen()->SetRedraw();
}

void MythDialogBox::Select(MythUIButtonListItem* item)
{
    if (!item)
        return;

    if (m_currentMenu)
    {
        auto *menuItem = item->GetData().value< MythMenuItem * >();
        if (menuItem == nullptr)
            return;
        if (menuItem->m_subMenu)
        {
            m_currentMenu->m_selectedItem = m_buttonList->GetCurrentPos();
            m_currentMenu = menuItem->m_subMenu;
            updateMenu();
            return;
        }

        if (menuItem->m_useSlot)
        {
            const char *slot = menuItem->m_data.value < const char * >();
            if (slot)
            {
                connect(this, SIGNAL(Selected()), m_currentMenu->m_retObject, slot,
                        Qt::QueuedConnection);
                emit Selected();
            }
            else if (menuItem->m_data.value<MythUICallbackNMF>())
            {
                connect(this, &MythDialogBox::Selected, m_currentMenu->m_retObject,
                        menuItem->m_data.value<MythUICallbackNMF>(),
                        Qt::QueuedConnection);
                emit Selected();
            }
            else if (menuItem->m_data.value<MythUICallbackMF>())
            {
                connect(this, &MythDialogBox::Selected, m_currentMenu->m_retObject,
                        menuItem->m_data.value<MythUICallbackMF>(),
                        Qt::QueuedConnection);
                emit Selected();
            }
            else if (menuItem->m_data.value<MythUICallbackMFc>())
            {
                connect(this, &MythDialogBox::Selected, m_currentMenu->m_retObject,
                        menuItem->m_data.value<MythUICallbackMFc>(),
                        Qt::QueuedConnection);
                emit Selected();
            }
        }

        SendEvent(m_buttonList->GetItemPos(item), item->GetText(), menuItem->m_data);
    }
    else
    {
        if (m_useSlots)
        {
            const char *slot = item->GetData().value<const char *>();
            if (slot)
            {
                connect(this, SIGNAL(Selected()), m_retObject, slot,
                        Qt::QueuedConnection);
                emit Selected();
            }
            else if (item->GetData().value<MythUICallbackNMF>())
            {
                connect(this, &MythDialogBox::Selected, m_retObject,
                        item->GetData().value<MythUICallbackNMF>(),
                        Qt::QueuedConnection);
                emit Selected();
            }
            else if (item->GetData().value<MythUICallbackMF>())
            {
                connect(this, &MythDialogBox::Selected, m_retObject,
                        item->GetData().value<MythUICallbackMF>(),
                        Qt::QueuedConnection);
                emit Selected();
            }
            else if (item->GetData().value<MythUICallbackMFc>())
            {
                connect(this, &MythDialogBox::Selected, m_retObject,
                        item->GetData().value<MythUICallbackMFc>(),
                        Qt::QueuedConnection);
                emit Selected();
            }
        }

        SendEvent(m_buttonList->GetItemPos(item), item->GetText(), item->GetData());
    }

    if (m_screenStack)
        m_screenStack->PopScreen(nullptr, false);
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
    m_backdata = std::move(data);
}

void MythDialogBox::SetExitAction(const QString &text, QVariant data)
{
    m_exittext = text;
    m_exitdata = std::move(data);
}

void MythDialogBox::SetText(const QString &text)
{
    if (m_textarea)
        m_textarea->SetText(text);
}

void MythDialogBox::AddButtonV(const QString &title, QVariant data, bool newMenu,
                              bool setCurrent)
{
    auto *button = new MythUIButtonListItem(m_buttonList, title);
    button->SetData(std::move(data));
    button->setDrawArrow(newMenu);

    if (setCurrent)
        m_buttonList->SetItemCurrent(button);
    // GetVisibleCount here makes sure that the dialog size is
    // calculated correctly
    m_buttonList->GetVisibleCount();
}

bool MythDialogBox::inputMethodEvent(QInputMethodEvent *event)
{
    return GetFocusWidget()->inputMethodEvent(event);
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
        const QString& action = actions[i];
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
        {
            handled = false;
        }
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

bool MythDialogBox::gestureEvent(MythGestureEvent *event)
{
    bool handled = false;
    if (event->GetGesture() == MythGestureEvent::Click)
    {
        switch (event->GetButton())
        {
            case Qt::RightButton:
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

void MythDialogBox::SendEvent(int res, const QString& text, const QVariant& data)
{
    if (m_currentMenu)
    {
        emit Closed(m_currentMenu->m_resultid, res);

        if (!m_currentMenu->m_retObject)
            return;

        auto *dce = new DialogCompletionEvent(m_currentMenu->m_resultid, res, text, data);
        QCoreApplication::postEvent(m_currentMenu->m_retObject, dce);
    }
    else
    {
        emit Closed(m_id, res);

        if (!m_retObject)
            return;

        auto *dce = new DialogCompletionEvent(m_id, res, text, data);
        QCoreApplication::postEvent(m_retObject, dce);
    }
}

/////////////////////////////////////////////////////////////////

bool MythConfirmationDialog::Create(void)
{
    if (!CopyWindowFromBase("MythConfirmationDialog", this))
        return false;

    MythUIButton *okButton = nullptr;
    MythUIButton *cancelButton = nullptr;

    bool err = false;
    UIUtilE::Assign(this, m_messageText, "message", &err);
    UIUtilE::Assign(this, okButton, "ok", &err);
    UIUtilE::Assign(this, cancelButton, "cancel", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'MythConfirmationDialog'");
        return false;
    }

    if (cancelButton && m_showCancel)
        connect(cancelButton, &MythUIButton::Clicked, this, &MythConfirmationDialog::Cancel);
    else if (cancelButton)
        cancelButton->SetVisible(false);

    if (okButton)
        connect(okButton, &MythUIButton::Clicked, this, &MythConfirmationDialog::Confirm);

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
        const QString& action = actions[i];
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

        auto *dce = new DialogCompletionEvent(m_id, res, "", m_resultData);
        QCoreApplication::postEvent(m_retObject, dce);
        m_retObject = nullptr;
    }

    Close();
}

/**
 * Non-blocking version of MythPopupBox::showOkPopup()
 */
MythConfirmationDialog  *ShowOkPopup(const QString &message, bool showCancel)
{
    QString                  LOC = "ShowOkPopup('" + message + "') - ";
    MythScreenStack         *stk = nullptr;

    MythMainWindow *win = GetMythMainWindow();

    if (win)
        stk = win->GetStack("popup stack");
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "no main window?");
        return nullptr;
    }

    if (!stk)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "no popup stack? "
                                       "Is there a MythThemeBase?");
        return nullptr;
    }

    auto *pop = new MythConfirmationDialog(stk, message, showCancel);
    if (pop->Create())
    {
        stk->AddScreen(pop);
    }
    else
    {
        delete pop;
        pop = nullptr;
        LOG(VB_GENERAL, LOG_ERR, LOC + "Couldn't Create() Dialog");
    }

    return pop;
}

bool WaitFor(MythConfirmationDialog *dialog)
{
    if (!dialog)
        return true; // No dialog is treated as user pressing OK

    // Local event loop processes events whilst we wait
    QEventLoop block;

    // Quit when dialog exits
    QObject::connect(dialog, &MythConfirmationDialog::haveResult,
                     &block, [&block](bool result)
    { block.exit(result ? 1 : 0); });

    // Block and return dialog result
    return block.exec() != 0;
}

/////////////////////////////////////////////////////////////////

bool MythTextInputDialog::Create(void)
{
    if (!CopyWindowFromBase("MythTextInputDialog", this))
        return false;

    MythUIText *messageText = nullptr;
    MythUIButton *okButton = nullptr;
    MythUIButton *cancelButton = nullptr;

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
        connect(cancelButton, &MythUIButton::Clicked, this, &MythScreenType::Close);
    if (okButton)
        connect(okButton, &MythUIButton::Clicked, this, &MythTextInputDialog::sendResult);

    m_textEdit->SetFilter(m_filter);
    m_textEdit->SetText(m_defaultValue);
    m_textEdit->SetPassword(m_isPassword);

    if (messageText)
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
        auto *dce = new DialogCompletionEvent(m_id, 0, inputString, "");
        QCoreApplication::postEvent(m_retObject, dce);
    }

    Close();
}

/////////////////////////////////////////////////////////////////

MythSpinBoxDialog::MythSpinBoxDialog(MythScreenStack *parent,
                                     QString message)
    : MythScreenType(parent, "mythspinboxpopup"),
      m_message(std::move(message)),
      m_id("")
{
}

bool MythSpinBoxDialog::Create(void)
{
    if (!CopyWindowFromBase("MythSpinBoxDialog", this))
        return false;

    MythUIText *messageText = nullptr;
    MythUIButton *okButton = nullptr;
    MythUIButton *cancelButton = nullptr;

    bool err = false;
    UIUtilE::Assign(this, m_spinBox, "input", &err);
    UIUtilE::Assign(this, messageText, "message", &err);
    UIUtilE::Assign(this, okButton, "ok", &err);
    UIUtilW::Assign(this, cancelButton, "cancel");

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'MythSpinBoxDialog'");
        return false;
    }

    if (cancelButton)
        connect(cancelButton, &MythUIButton::Clicked, this, &MythScreenType::Close);
    if (okButton)
        connect(okButton, &MythUIButton::Clicked, this, &MythSpinBoxDialog::sendResult);

    if (messageText)
        messageText->SetText(m_message);
    BuildFocusList();

    return true;
}

/**
 * \copydoc MythUISpinBox::SetRange()
 * Can be called only after MythSpinBoxDialog::Create() return successfully
 */
void MythSpinBoxDialog::SetRange(int low, int high, int step, uint pageMultiple)
{
    m_spinBox->SetRange(low, high, step, pageMultiple);
}

/**
 *
 */
void MythSpinBoxDialog::AddSelection(const QString& label, int value)
{
    m_spinBox->AddSelection(value, label);
}

/**
 * Can be called only after MythSpinBoxDialog::Create() return successfully
 * The range need to be set before we can set the value
 */
void MythSpinBoxDialog::SetValue (const QString & value)
{
    m_spinBox->SetValue(value);
}

/**
 * Can be called only after MythSpinBoxDialog::Create() return successfully
 */
void MythSpinBoxDialog::SetValue(int value)
{
    m_spinBox->SetValue(value);
}

void MythSpinBoxDialog::SetReturnEvent(QObject *retobject,
                                       const QString &resultid)
{
    m_retObject = retobject;
    m_id = resultid;
}

void MythSpinBoxDialog::sendResult()
{
    QString inputString = m_spinBox->GetValue();
    emit haveResult(inputString);

    if (m_retObject)
    {
        auto *dce = new DialogCompletionEvent(m_id, 0, inputString, "");
        QCoreApplication::postEvent(m_retObject, dce);
    }

    Close();
}

/////////////////////////////////////////////////////////////////////

bool MythUISearchDialog::Create(void)
{
    if (!CopyWindowFromBase("MythSearchDialog", this))
        return false;

    MythUIButton *okButton = nullptr;
    MythUIButton *cancelButton = nullptr;

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
        connect(cancelButton, &MythUIButton::Clicked, this, &MythScreenType::Close);

    if (okButton)
        connect(okButton, &MythUIButton::Clicked, this, &MythUISearchDialog::slotSendResult);

    connect(m_itemList, &MythUIButtonList::itemClicked,
            this, &MythUISearchDialog::slotSendResult);

    m_textEdit->SetText(m_defaultValue);
    connect(m_textEdit, &MythUITextEdit::valueChanged, this, &MythUISearchDialog::slotUpdateList);

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
        auto *dce = new DialogCompletionEvent(m_id, 0, result, "");
        QCoreApplication::postEvent(m_retObject, dce);
    }

    Close();
}

void MythUISearchDialog::slotUpdateList(void)
{
    m_itemList->Reset();

    for (const QString& item : std::as_const(m_list))
    {
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
        new MythUIButtonListItem(m_itemList, item, nullptr, false);
    }

    m_itemList->SetItemCurrent(0);

    if (m_matchesText)
        m_matchesText->SetText(tr("%n match(es)", "", 0));
}

////////////////////////////////////////////////////////////////////////

MythTimeInputDialog::MythTimeInputDialog(MythScreenStack *parent,
                                         QString message,
                                         int resolutionFlags,
                                         QDateTime startTime,
                                         int rangeLimit)
    : MythScreenType(parent, "timepopup"),
        m_message(std::move(message)), m_startTime(std::move(startTime)),
        m_resolution(resolutionFlags), m_rangeLimit(rangeLimit)
{
}

bool MythTimeInputDialog::Create()
{
    if (!CopyWindowFromBase("MythTimeInputDialog", this))
        return false;

    MythUIText *messageText = nullptr;
    MythUIButton *okButton = nullptr;

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
        bool selected = false;
        for (int x = 0; x <= limit; x++)
        {
            selected = false;
            if (m_resolution & kDay)
            {
                date = date.addDays(1);
                int flags = MythDate::kDateFull | MythDate::kSimplify;
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

            auto *item = new MythUIButtonListItem(m_dateList, text, nullptr, false);
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
#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
                QDateTime dt = QDateTime(startdate, time, Qt::LocalTime);
#else
                QDateTime dt = QDateTime(startdate, time,
                                         QTimeZone(QTimeZone::LocalTime));
#endif
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

            auto *item = new MythUIButtonListItem(m_timeList, text, nullptr, false);
            item->SetData(QVariant(time));

            if (selected)
                m_timeList->SetItemCurrent(item);
        }
        m_timeList->SetVisible(true);
    }

    if (messageText && !m_message.isEmpty())
        messageText->SetText(m_message);

    if (okButton)
        connect(okButton, &MythUIButton::Clicked, this, &MythTimeInputDialog::okClicked);

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

#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
    QDateTime dateTime = QDateTime(date, time, Qt::LocalTime).toUTC();
#else
    QDateTime dateTime = QDateTime(date, time,
                                   QTimeZone(QTimeZone::LocalTime)).toUTC();
#endif

    emit haveResult(dateTime);

    if (m_retObject)
    {
        QVariant data(dateTime);
        auto *dce = new DialogCompletionEvent(m_id, 0, "", data);
        QCoreApplication::postEvent(m_retObject, dce);
    }

    Close();
}
