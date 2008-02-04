#include <qapplication.h>

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
    buttonList = NULL;
}

bool MythDialogBox::Create(void)
{
    if (!CopyWindowFromBase("MythDialogBox", this))
        return false;

    SetFullscreen(false);

    MythUIText *textarea = dynamic_cast<MythUIText *>(GetChild("messagearea"));
    buttonList = dynamic_cast<MythListButton *>(GetChild("list"));

    if (!textarea || !buttonList)
        return false;

    textarea->SetText(m_text);
    buttonList->SetActive(true);

    connect(buttonList, SIGNAL(itemClicked(MythListButtonItem*)),
            this, SLOT(Select(MythListButtonItem*)));

    return true;
}

void MythDialogBox::Select(MythListButtonItem* item)
{
    SendEvent(buttonList->GetItemPos(item));
    m_ScreenStack->PopScreen();
}

void MythDialogBox::SetReturnEvent(MythScreenType *retscreen, 
                               const QString &resultid)
{
    m_retScreen = retscreen;
    m_id = resultid;
}

void MythDialogBox::AddButton(const QString &title)
{
    new MythListButtonItem(buttonList, title);
}

bool MythDialogBox::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    if (GetMythMainWindow()->TranslateKeyPress("qt", event, actions))
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
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
                MythListButtonItem *item = buttonList->GetItemCurrent();
                Select(item);
            }
            else
                handled = false;
        }
    }

    return handled;
}

void MythDialogBox::SendEvent(int res)
{
    if (!m_retScreen)
        return;

    DialogCompletionEvent *dce = new DialogCompletionEvent(m_id, res);
    QApplication::postEvent(m_retScreen, dce);
}
