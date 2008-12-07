
#include <QApplication>

#include "mythverbose.h"

#include "mythdialogbox.h"
#include "mythmainwindow.h"
#include "mythfontproperties.h"

MythDialogBox::MythDialogBox(const QString &text,
                             MythScreenStack *parent, const char *name,
                             bool fullscreen)
         : MythScreenType(parent, name, false)
{
    m_id = "";
    m_retObject = NULL;
    m_titlearea = NULL;
    m_title = "";
    m_text = text;
    m_textarea = NULL;
    m_buttonList = NULL;

    m_fullscreen = fullscreen;
    m_useSlots = false;
}

MythDialogBox::MythDialogBox(const QString &title, const QString &text,
                             MythScreenStack *parent, const char *name,
                             bool fullscreen)
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
    m_useSlots = false;
}

bool MythDialogBox::Create(void)
{
    QString windowName = (m_fullscreen ? "MythDialogBox" : "MythPopupBox");

    if (!CopyWindowFromBase(windowName, this))
        return false;

    try
    {
        m_titlearea = GetMythUIText("title", true);
        m_textarea = GetMythUIText("messagearea");
        m_buttonList = GetMythUIButtonList("list");
    }
    catch (QString &error)
    {
        VERBOSE(VB_IMPORTANT, "Cannot load screen '" + windowName + "'\n\t\t\t"
                              "Error was: " + error);
        return false;
    }

    if (m_titlearea)
        m_titlearea->SetText(m_title);
    m_textarea->SetText(m_text);
    m_buttonList->SetActive(true);

    connect(m_buttonList, SIGNAL(itemClicked(MythUIButtonListItem*)),
            this, SLOT(Select(MythUIButtonListItem*)));

    return true;
}

void MythDialogBox::Select(MythUIButtonListItem* item)
{
    const char *slot = (const char *)item->getData();
    if (m_useSlots && slot)
    {
        const char *slot = (const char *)item->getData();
        connect(this, SIGNAL(Selected()), m_retObject, slot,
                Qt::QueuedConnection);
        emit Selected();
    }

    SendEvent(m_buttonList->GetItemPos(item), item->text(), item->GetData());
    m_ScreenStack->PopScreen(false);
}

void MythDialogBox::SetReturnEvent(QObject *retobject,
                               const QString &resultid)
{
    m_retObject = retobject;
    m_id = resultid;
}

void MythDialogBox::AddButton(const QString &title, QVariant data)
{
    MythUIButtonListItem *button = new MythUIButtonListItem(m_buttonList, title);
    button->SetData(data);
}

void MythDialogBox::AddButton(const QString &title, const char *slot)
{
    MythUIButtonListItem *button = new MythUIButtonListItem(m_buttonList, title);

    m_useSlots = true;

    if (slot)
        button->setData(const_cast<char*>(slot));
}

bool MythDialogBox::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    if (GetMythMainWindow()->TranslateKeyPress("qt", event, actions))
    {
        for (int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;

            if (action == "ESCAPE" || action == "LEFT" || action == "MENU")
            {
                SendEvent(-1);
                m_ScreenStack->PopScreen();
            }
            else if (action == "RIGHT")
            {
                MythUIButtonListItem *item = m_buttonList->GetItemCurrent();
                Select(item);
            }
            else
                handled = false;
        }
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void MythDialogBox::SendEvent(int res, QString text, QVariant data)
{
    if (!m_retObject)
        return;

    DialogCompletionEvent *dce = new DialogCompletionEvent(m_id, res, text, data);
    QApplication::postEvent(m_retObject, dce);
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

    try
    {
        messageText = GetMythUIText("message");
        okButton = GetMythUIButton("ok");
        cancelButton = GetMythUIButton("cancel");
    }
    catch (QString &error)
    {
        VERBOSE(VB_IMPORTANT, "Cannot load screen 'MythConfirmationDialog'\n\t\t\t"
                              "Error was: " + error);
        return false;
    }

    if (m_showCancel)
    {
        connect(cancelButton, SIGNAL(Clicked()), SLOT(Cancel()));
    }
    else
        cancelButton->SetVisible(false);

    connect(okButton, SIGNAL(Clicked()), SLOT(Confirm()));

    okButton->SetText(tr("OK"));
    cancelButton->SetText(tr("Cancel"));

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
    if (GetMythMainWindow()->TranslateKeyPress("qt", event, actions))
    {
        for (int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;

            if (action == "ESCAPE")
                sendResult(false);
            else
                handled = false;
        }
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
        QApplication::postEvent(m_retObject, dce);
    }

    Close();
}

/**
 * Non-blocking version of MythPopupBox::showOkPopup()
 */
void ShowOkPopup(const QString &message)
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
            return;
        }

        if (!stk)
        {
            VERBOSE(VB_IMPORTANT, LOC + "no popup stack?\n"
                                        "Is there a MythThemeBase?");
            return;
        }
    }

    pop = new MythConfirmationDialog(stk, message, false);  // No cancel button
    if (pop->Create())
        stk->AddScreen(pop);
    else
    {
        delete pop;
        VERBOSE(VB_IMPORTANT, LOC + "Couldn't Create() Dialog");
    }
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

    try
    {
        m_textEdit = GetMythUITextEdit("input");
        messageText = GetMythUIText("message");
        okButton = GetMythUIButton("ok");
        cancelButton = GetMythUIButton("cancel", true);
    }
    catch (QString &error)
    {
        VERBOSE(VB_IMPORTANT, "Cannot load screen 'MythTextInputDialog'\n\t\t\t"
                              "Error was: " + error);
        return false;
    }

    if (cancelButton)
        connect(cancelButton, SIGNAL(Clicked()), SLOT(Close()));
    connect(okButton, SIGNAL(Clicked()), SLOT(sendResult()));

    m_textEdit->SetFilter(m_filter);
    m_textEdit->SetText(m_defaultValue);
    m_textEdit->SetPassword(m_isPassword);

    messageText->SetText(m_message);
    okButton->SetText(tr("OK"));

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
        QApplication::postEvent(m_retObject, dce);
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

    try
    {
        m_textEdit = GetMythUITextEdit("input");
        m_titleText = GetMythUIText("title");
        m_matchesText = GetMythUIText("matches", true);
        m_itemList = GetMythUIButtonList("itemlist");
        okButton = GetMythUIButton("ok");
        cancelButton = GetMythUIButton("cancel", true);
    }
    catch (QString &error)
    {
        VERBOSE(VB_IMPORTANT, "Cannot load screen 'MythSearchDialog'\n\t\t\t"
                              "Error was: " + error);
        return false;
    }

    if (cancelButton)
    {
        cancelButton->SetText(tr("Cancel"));
        connect(cancelButton, SIGNAL(Clicked()), SLOT(Close()));
    }

    connect(okButton, SIGNAL(Clicked()), SLOT(slotSendResult()));

    connect(m_itemList, SIGNAL(itemClicked(MythUIButtonListItem*)), SLOT(slotSendResult()));

    m_textEdit->SetText(m_defaultValue);
    connect(m_textEdit, SIGNAL(valueChanged()), SLOT(slotUpdateList()));

    m_titleText->SetText(m_title);
    if (m_matchesText)
        m_matchesText->SetText(tr("0 matches"));

    okButton->SetText(tr("OK"));

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
        QApplication::postEvent(m_retObject, dce);
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
        new MythUIButtonListItem(m_itemList, item, NULL, true, MythUIButtonListItem::NotChecked);
    }

    m_itemList->SetItemCurrent(0);

    if (m_matchesText)
        m_matchesText->SetText(tr("%1 matches").arg(m_list.size()));
}
