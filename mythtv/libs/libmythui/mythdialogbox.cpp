
#include "mythdialogbox.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <QTimer>

#include "mythverbose.h"

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

MythDialogBox::MythDialogBox(const QString &text,
                             MythScreenStack *parent, const char *name,
                             bool fullscreen, bool osd)
         : MythScreenType(parent, name, false)
{
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
        VERBOSE(VB_IMPORTANT, QString("Cannot load screen '%1'")
                                        .arg(windowName));
        return false;
    }

    if (m_titlearea)
        m_titlearea->SetText(m_title);
    m_textarea->SetText(m_text);
    BuildFocusList();

    connect(m_buttonList, SIGNAL(itemClicked(MythUIButtonListItem*)),
            SLOT(Select(MythUIButtonListItem*)));

    return true;
}

void MythDialogBox::Select(MythUIButtonListItem* item)
{
    if (!item)
        return;

    const char *slot = qVariantValue<const char *>(item->GetData());
    if (m_useSlots && slot)
    {
        connect(this, SIGNAL(Selected()), m_retObject, slot,
                Qt::QueuedConnection);
        emit Selected();
    }

    SendEvent(m_buttonList->GetItemPos(item), item->GetText(), item->GetData());
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

void MythDialogBox::ResetButtons(void)
{
    if (m_buttonList)
        m_buttonList->Reset();
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

        if (action == "ESCAPE" )
        {
            SendEvent(-1, m_exittext, m_exitdata);
            if (m_exitdata == 0 && m_exittext.isEmpty())
                Close();
        }
        else if (action == "LEFT")
        {
            SendEvent(-1, m_backtext, m_backdata);
            if (m_backdata == 0 && m_backtext.isEmpty())
                Close();
        }
        else if (action == "MENU")
        {
            SendEvent(-2);
            Close();
        }
        else if (action == "RIGHT")
        {
            MythUIButtonListItem *item = m_buttonList->GetItemCurrent();
            if (item)
                Select(item);
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
    emit Closed(m_id, res);

    if (!m_retObject)
        return;

    DialogCompletionEvent *dce = new DialogCompletionEvent(m_id, res, text, data);
    QCoreApplication::postEvent(m_retObject, dce);
}

/////////////////////////////////////////////////////////////////

MythConfirmationDialog::MythConfirmationDialog(MythScreenStack *parent,
                                               const QString &message,
                                               bool showCancel)
                       : MythScreenType(parent, "mythconfirmpopup")
{
    m_message = message;
    m_showCancel = showCancel;

    m_id = "";
    m_retObject = NULL;
}

bool MythConfirmationDialog::Create(void)
{
    if (!CopyWindowFromBase("MythConfirmationDialog", this))
        return false;

    MythUIText *messageText = NULL;
    MythUIButton *okButton = NULL;
    MythUIButton *cancelButton = NULL;

    bool err = false;
    UIUtilE::Assign(this, messageText, "message", &err);
    UIUtilE::Assign(this, okButton, "ok", &err);
    UIUtilE::Assign(this, cancelButton, "cancel", &err);

    if (err)
    {
        VERBOSE(VB_IMPORTANT, "Cannot load screen 'MythConfirmationDialog'");
        return false;
    }

    if (m_showCancel)
    {
        connect(cancelButton, SIGNAL(Clicked()), SLOT(Cancel()));
    }
    else
        cancelButton->SetVisible(false);

    connect(okButton, SIGNAL(Clicked()), SLOT(Confirm()));

    messageText->SetText(m_message);

    BuildFocusList();

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
    static MythScreenStack  *stk = NULL;


    if (!stk)
    {
        MythMainWindow *win = GetMythMainWindow();

        if (win)
            stk = win->GetStack("popup stack");
        else
        {
            VERBOSE(VB_IMPORTANT, LOC + "no main window?");
            return NULL;
        }

        if (!stk)
        {
            VERBOSE(VB_IMPORTANT, LOC + "no popup stack?\n"
                                        "Is there a MythThemeBase?");
            return NULL;
        }
    }

    pop = new MythConfirmationDialog(stk, message, showCancel);
    if (pop->Create())
    {
        stk->AddScreen(pop);
        if (parent && slot)
            QObject::connect(pop, SIGNAL(haveResult(bool)), parent, slot, Qt::QueuedConnection);
    }
    else
    {
        delete pop;
        pop = NULL;
        VERBOSE(VB_IMPORTANT, LOC + "Couldn't Create() Dialog");
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
        VERBOSE(VB_IMPORTANT, "Cannot load screen 'MythTextInputDialog'");
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
        VERBOSE(VB_IMPORTANT, "Cannot load screen 'MythSearchDialog'");
        return false;
    }

    if (cancelButton)
        connect(cancelButton, SIGNAL(Clicked()), SLOT(Close()));

    connect(okButton, SIGNAL(Clicked()), SLOT(slotSendResult()));

    connect(m_itemList, SIGNAL(itemClicked(MythUIButtonListItem*)), SLOT(slotSendResult()));

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

