
#include <QApplication>

#include "mythverbose.h"

#include "mythdialogbox.h"
#include "mythmainwindow.h"
#include "mythfontproperties.h"

MythDialogBox::MythDialogBox(const QString &text,
                             MythScreenStack *parent, const char *name)
         : MythScreenType(parent, name, false)
{
    m_id = "";
    m_retScreen = NULL;
    m_text = text;
    m_textarea = NULL;
    m_buttonList = NULL;

    m_useSlots = false;
}

bool MythDialogBox::Create(void)
{
    if (!CopyWindowFromBase("MythDialogBox", this))
        return false;

    m_textarea = dynamic_cast<MythUIText *>(GetChild("messagearea"));
    m_buttonList = dynamic_cast<MythUIButtonList *>(GetChild("list"));

    if (!m_textarea || !m_buttonList)
        return false;

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
        connect(this, SIGNAL(Selected()), m_retScreen, slot,
                Qt::QueuedConnection);
        emit Selected();
    }

    SendEvent(m_buttonList->GetItemPos(item), item->text(), item->getData());
    m_ScreenStack->PopScreen(false);
}

void MythDialogBox::SetReturnEvent(MythScreenType *retscreen,
                               const QString &resultid)
{
    m_retScreen = retscreen;
    m_id = resultid;
}

void MythDialogBox::AddButton(const QString &title, void *data)
{
    MythUIButtonListItem *button = new MythUIButtonListItem(m_buttonList, title);
    if (data)
        button->setData(data);
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

    return handled;
}

void MythDialogBox::SendEvent(int res, QString text, void *data)
{
    if (!m_retScreen)
        return;

    DialogCompletionEvent *dce = new DialogCompletionEvent(m_id, res, text, data);
    QApplication::postEvent(m_retScreen, dce);
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
    m_retScreen = NULL;
}

bool MythConfirmationDialog::Create(void)
{
    if (!CopyWindowFromBase("MythConfirmationDialog", this))
        return false;

    MythUIText *messageText = dynamic_cast<MythUIText *>
                                            (GetChild("message"));
    MythUIButton *okButton = dynamic_cast<MythUIButton *>
                                         (GetChild("ok"));
    MythUIButton *cancelButton = dynamic_cast<MythUIButton *>
                                         (GetChild("cancel"));

    if (!messageText || !okButton || !cancelButton)
        return false;

    if (m_showCancel)
    {
        connect(cancelButton, SIGNAL(buttonPressed()), SLOT(Cancel()));
    }
    else
        cancelButton->SetVisible(false);

    connect(okButton, SIGNAL(buttonPressed()), SLOT(Confirm()));

    okButton->SetText(tr("Ok"));
    cancelButton->SetText(tr("Cancel"));

    messageText->SetText(m_message);

    BuildFocusList();

    return true;
}

void MythConfirmationDialog::SetReturnEvent(MythScreenType *retscreen,
                                            const QString &resultid)
{
    m_retScreen = retscreen;
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

    if (m_retScreen)
    {
        int res = 0;
        if (ok)
            res = 1;

        DialogCompletionEvent *dce = new DialogCompletionEvent(m_id, res, "",
                                                               NULL);
        QApplication::postEvent(m_retScreen, dce);
    }

    Close();
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
    m_retScreen = NULL;
}

bool MythTextInputDialog::Create(void)
{
    if (!CopyWindowFromBase("MythTextInputDialog", this))
        return false;

    m_textEdit = dynamic_cast<MythUITextEdit *> (GetChild("input"));
    MythUIText *messageText = dynamic_cast<MythUIText *>
                                            (GetChild("message"));
    MythUIButton *okButton = dynamic_cast<MythUIButton *>
                                         (GetChild("ok"));
    MythUIButton *cancelButton = dynamic_cast<MythUIButton *>
                                         (GetChild("cancel"));

    if (!m_textEdit || !messageText || !okButton)
        return false;

    if (cancelButton)
        connect(cancelButton, SIGNAL(buttonPressed()), SLOT(Close()));
    connect(okButton, SIGNAL(buttonPressed()), SLOT(sendResult()));

    m_textEdit->SetFilter(m_filter);
    m_textEdit->SetText(m_defaultValue);
    //m_textEdit->SetIsPassword(m_isPassword);

    messageText->SetText(m_message);
    okButton->SetText(tr("Ok"));

    m_textEdit->SetText("");

    BuildFocusList();

    return true;
}

void MythTextInputDialog::SetReturnEvent(MythScreenType *retscreen,
                                         const QString &resultid)
{
    m_retScreen = retscreen;
    m_id = resultid;
}

void MythTextInputDialog::sendResult()
{
    QString inputString = m_textEdit->GetText();
    emit haveResult(inputString);

    if (m_retScreen)
    {
        DialogCompletionEvent *dce = new DialogCompletionEvent(m_id, 0,
                                                            inputString, NULL);
        QApplication::postEvent(m_retScreen, dce);
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
    m_retScreen = NULL;
}

bool MythUISearchDialog::Create(void)
{
    if (!CopyWindowFromBase("MythSearchDialog", this))
        return false;

    m_textEdit = dynamic_cast<MythUITextEdit *> (GetChild("input"));

    m_titleText = dynamic_cast<MythUIText *>(GetChild("title"));
    m_matchesText = dynamic_cast<MythUIText *>(GetChild("matches"));

    m_itemList =  dynamic_cast<MythUIButtonList *>(GetChild("itemlist"));

    MythUIButton *okButton = dynamic_cast<MythUIButton *>(GetChild("ok"));
    MythUIButton *cancelButton = dynamic_cast<MythUIButton *>(GetChild("cancel"));

    if (!m_textEdit || !m_titleText || !okButton || !m_itemList)
    {
        VERBOSE(VB_IMPORTANT, "Theme is missing critical theme elements.");
        return false;
    }

    if (cancelButton)
    {
        cancelButton->SetText(tr("Cancel"));
        connect(cancelButton, SIGNAL(buttonPressed()), SLOT(Close()));
    }

    connect(okButton, SIGNAL(buttonPressed()), SLOT(slotSendResult()));

    connect(m_itemList, SIGNAL(itemClicked(MythUIButtonListItem*)), SLOT(slotSendResult()));

    m_textEdit->SetText(m_defaultValue);
    connect(m_textEdit, SIGNAL(valueChanged()), SLOT(slotUpdateList()));

    m_titleText->SetText(m_title);
    if (m_matchesText)
        m_matchesText->SetText(tr("0 matches"));

    okButton->SetText(tr("Ok"));

    BuildFocusList();

    slotUpdateList();

    return true;
}

void MythUISearchDialog::SetReturnEvent(MythScreenType *retscreen,
                                         const QString &resultid)
{
    m_retScreen = retscreen;
    m_id = resultid;
}

void MythUISearchDialog::slotSendResult()
{
    if (!m_itemList->GetItemCurrent())
        return;

    QString result = m_itemList->GetValue();

    emit haveResult(result);

    if (m_retScreen)
    {
        DialogCompletionEvent *dce = new DialogCompletionEvent(m_id, 0,
                                                            result, NULL);
        QApplication::postEvent(m_retScreen, dce);
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
