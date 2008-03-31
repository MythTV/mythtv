#include <qapplication.h>
#include <QKeyEvent>

#include "mythdialogbox.h"
#include "mythlistbutton.h"
#include "mythmainwindow.h"
#include "mythfontproperties.h"
#include "mythcontext.h"
#include "mythmediamonitor.h"

MythDialogBox::MythDialogBox(const QString &text,
                             MythScreenStack *parent, const char *name) 
         : MythScreenType(parent, name, false)
{
    m_id = "";
    m_retScreen = NULL;
    m_text = text;
    m_textarea = NULL;
    m_buttonList = NULL;
}

bool MythDialogBox::Create(void)
{
    if (!CopyWindowFromBase("MythDialogBox", this))
        return false;

    m_textarea = dynamic_cast<MythUIText *>(GetChild("messagearea"));
    m_buttonList = dynamic_cast<MythListButton *>(GetChild("list"));

    if (!m_textarea || !m_buttonList)
        return false;

    m_textarea->SetText(m_text);
    m_buttonList->SetActive(true);

    connect(m_buttonList, SIGNAL(itemClicked(MythListButtonItem*)),
            this, SLOT(Select(MythListButtonItem*)));

    return true;
}

void MythDialogBox::Select(MythListButtonItem* item)
{
    SendEvent(m_buttonList->GetItemPos(item), item->text(), item->getData());
    m_ScreenStack->PopScreen();
}

void MythDialogBox::SetReturnEvent(MythScreenType *retscreen, 
                               const QString &resultid)
{
    m_retScreen = retscreen;
    m_id = resultid;
}

void MythDialogBox::AddButton(const QString &title, void *data)
{
    MythListButtonItem *button = new MythListButtonItem(m_buttonList, title);
    if (data)
        button->setData(data);
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
                MythListButtonItem *item = m_buttonList->GetItemCurrent();
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
