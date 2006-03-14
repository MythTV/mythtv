#include <qapplication.h>

#include "mythdialogbox.h"
#include "mythlistbutton.h"
#include "mythmainwindow.h"
#include "mythfontproperties.h"
#include "mythcontext.h"
#include "mythmediamonitor.h"

MythDialogBox::MythDialogBox(const QString &text,
                     MythScreenStack *parent, const char *name) 
         : MythScreenType(parent, name)
{
    m_id = "";

    MythFontProperties fontProp;
    fontProp.SetFace(CreateFont("Arial", 24, QFont::Bold));
    fontProp.SetColor(Qt::white);
    fontProp.SetShadow(true, NormPoint(QPoint(4, 4)), QColor(Qt::black), 64);

    QRect fullRect = GetMythMainWindow()->GetUIScreenRect();
    int xpad = NormX(60);
    int ypad = NormY(60);

    QRect textRect = QRect(xpad, ypad, fullRect.width() - xpad * 2, 
                           fullRect.height() / 2 - ypad);

    MythUIText *label = new MythUIText(text, fontProp, textRect, textRect, 
                                       this, "label");
    label->SetJustification(Qt::WordBreak | Qt::AlignLeft | Qt::AlignTop);

    ypad = NormY(40);
    QRect listarea = QRect(xpad, fullRect.height() / 2 + ypad, 
                           fullRect.width() - xpad * 2,
                           fullRect.height() / 3);

    buttonList = new MythListButton(this, "listbutton", listarea, true, true);

    buttonList->SetFontActive(fontProp);
    fontProp.SetColor(QColor(qRgb(128, 128, 128)));
    buttonList->SetFontInactive(fontProp);

    buttonList->SetSpacing(NormX(10));
    buttonList->SetMargin(NormX(6));
    buttonList->SetDrawFromBottom(true);
    buttonList->SetTextFlags(Qt::AlignCenter);

    buttonList->SetActive(true);
    m_retScreen = NULL;
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

bool MythDialogBox::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    if (GetMythMainWindow()->TranslateKeyPress("qt", e, actions))
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;

            if (action == "UP")
                buttonList->MoveUp();
            else if (action == "DOWN")
                buttonList->MoveDown();
            else if (action == "ESCAPE" || action == "LEFT")
            {
                SendEvent(-1);
                m_ScreenStack->PopScreen();
            }
            else if (action == "SELECT" || action == "RIGHT")
            {
                MythListButtonItem *item = buttonList->GetItemCurrent();
                SendEvent(buttonList->GetItemPos(item));
                m_ScreenStack->PopScreen();
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
