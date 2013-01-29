
#include "screenwizard.h"

/* QT includes */
#include <QStringList>
#include <QCoreApplication>
#include <QString>
#include <QBrush>
#include <QColor>

/* MythTV includes */
#include "mythcorecontext.h"
#include "mythmainwindow.h"
#include "mythuihelper.h"
#include "mythlogging.h"
#include "themeinfo.h"
#include "mythrect.h"

using namespace std;

#define kMinWidth 160
#define kMinHeight 160

ScreenWizard::ScreenWizard(MythScreenStack *parent, const char *name) :
    MythScreenType(parent, name),
    m_whichcorner(true),
    m_coarsefine(false), // fine adjustments by default
    m_changed(false),
    m_fine(1),           // fine moves corners by one pixel
    m_coarse(10),        // coarse moves corners by ten pixels
    m_change(m_fine),
    m_topleft_x(0),     m_topleft_y(0),
    m_bottomright_x(0), m_bottomright_y(0),
    m_screenwidth(0),        m_screenheight(0),
    m_xsize(GetMythMainWindow()->GetUIScreenRect().width()),
    m_ysize(GetMythMainWindow()->GetUIScreenRect().height()),
    m_xoffset(0),            m_yoffset(0),
    m_menuPopup(NULL)
{
}

ScreenWizard::~ScreenWizard()
{

}

void ScreenWizard::SetInitialSettings(int _x, int _y, int _w, int _h)
{
    m_xoffset = _x;
    m_yoffset = _y;

    m_screenwidth  = _w ? _w : m_xsize;
    m_screenheight = _h ? _h : m_ysize;
}

bool ScreenWizard::Create()
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("appear-ui.xml", "appearance", this);

    if (!foundtheme)
        return false;

    m_blackout = dynamic_cast<MythUIShape *> (GetChild("blackout"));
    m_preview = dynamic_cast<MythUIImage *> (GetChild("preview"));
    m_size = dynamic_cast<MythUIText *> (GetChild("size"));
    m_offsets = dynamic_cast<MythUIText *> (GetChild("offsets"));
    m_changeamount = dynamic_cast<MythUIText *> (GetChild("changeamount"));

    if (!m_blackout || !m_preview || !m_size || !m_offsets || !m_changeamount)
    {
        LOG(VB_GENERAL, LOG_ERR, "ScreenWizard, Error: "
               "Could not instantiate, please check appear-ui.xml for errors");
        return false;
    }

    m_blackout->SetArea(MythRect(0, 0, m_xsize, m_ysize));

    // work out origin co-ordinates for preview corners
    m_topleft_x = m_xoffset;
    m_topleft_y = m_yoffset;
    m_bottomright_x = m_xoffset + m_screenwidth;
    m_bottomright_y = m_yoffset + m_screenheight;

    updateScreen();

    return true;
}

bool ScreenWizard::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget() && GetFocusWidget()->keyPressEvent(event))
        return true;

    QStringList actions;
    bool handled = false;

    handled = GetMythMainWindow()->TranslateKeyPress("Global", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;
        bool refresh = false;

        if (action == "SELECT")
        {
            m_whichcorner = !m_whichcorner;
            refresh = true;
        }
        else if (action == "UP")
        {
            if (m_whichcorner)
                refresh = moveTLUp();
            else
                refresh = moveBRUp();
        }
        else if (action == "DOWN")
        {
            if (m_whichcorner)
                refresh = moveTLDown();
            else
                refresh = moveBRDown();
        }
        else if (action == "LEFT")
        {
            if (m_whichcorner)
                refresh = moveTLLeft();
            else
                refresh = moveBRLeft();
        }
        else if (action == "RIGHT")
        {
            if (m_whichcorner)
                refresh = moveTLRight();
            else
                refresh = moveBRRight();
        }
        else if (action == "MENU")
            doMenu();
        else if (action == "ESCAPE")
        {
            if (anythingChanged())
                doExit();
            else
                qApp->quit();
        }
        else
            handled = false;

        if (refresh)
            updateScreen();
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

bool ScreenWizard::moveTLUp(void)
{
    if (m_topleft_y < (0 + m_change))
        return false;

    m_topleft_y -= m_change;
    return true;
}

bool ScreenWizard::moveTLDown(void)
{
    if (m_topleft_y > (m_bottomright_y - m_change - kMinHeight))
        if (!moveBRDown())
            return false;

    m_topleft_y += m_change;
    return true;
}

bool ScreenWizard::moveTLLeft(void)
{
    if (m_topleft_x < (0 + m_change))
        return false;

    m_topleft_x -= m_change;
    return true;
}

bool ScreenWizard::moveTLRight(void)
{
    if (m_topleft_x > (m_bottomright_x - m_change - kMinWidth))
        if (!moveBRRight())
            return false;

    m_topleft_x += m_change;
    return true;
}

bool ScreenWizard::moveBRUp(void)
{
    if (m_bottomright_y < (m_topleft_y + m_change + kMinHeight))
        if (!moveTLUp())
            return false;

    m_bottomright_y -= m_change;
    return true;
}

bool ScreenWizard::moveBRDown(void)
{
    if (m_bottomright_y > (m_ysize - m_change))
        return false;

    m_bottomright_y += m_change;
    return true;
}

bool ScreenWizard::moveBRLeft(void)
{
    if (m_bottomright_x < (m_topleft_x + m_change + kMinWidth))
        if (!moveTLLeft())
            return false;

    m_bottomright_x -= m_change;
    return true;
}

bool ScreenWizard::moveBRRight(void)
{
    if (m_bottomright_x > (m_xsize - m_change))
        return false;

    m_bottomright_x += m_change;
    return true;
}

void ScreenWizard::updateScreen()
{
    int width = m_bottomright_x - m_topleft_x;
    int height = m_bottomright_y - m_topleft_y;

    m_preview->SetArea(MythRect(m_topleft_x, m_topleft_y, width, height));
    m_size->SetText(tr("Size: %1 x %2").arg(width).arg(height));
    m_offsets->SetText(tr("Offset: %1 x %2").arg(m_topleft_x).arg(m_topleft_y));
    m_changeamount->SetText(tr("Change amount: %n pixel(s)", "", m_change));

}

void ScreenWizard::doMenu()
{
    if (m_menuPopup)
        return;

    QString label = tr("Options");

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    m_menuPopup = new MythDialogBox(label, popupStack, "menuPopup");

    if (m_menuPopup->Create())
    {
        if (anythingChanged())
        {
            m_menuPopup->SetReturnEvent(this, "save");
            m_menuPopup->AddButton(tr("Save and Quit"));
        }
        else
            m_menuPopup->SetReturnEvent(this, "nosave");

        m_menuPopup->AddButton(tr("Reset Changes and Quit"));
        m_menuPopup->AddButton(tr("Coarse/Fine adjustment"));
        m_menuPopup->AddButton(tr("Close Menu"));

        popupStack->AddScreen(m_menuPopup);
    }
    else
    {
        delete m_menuPopup;
    }
}

void ScreenWizard::doExit()
{
    if (m_menuPopup)
        return;

    QString label = tr("Exit Options");

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    m_menuPopup = new MythDialogBox(label, popupStack, "menuPopup");

    if (m_menuPopup->Create())
    {
        m_menuPopup->SetReturnEvent(this, "quit");
        m_menuPopup->AddButton(tr("Save and Quit"));
        m_menuPopup->AddButton(tr("Discard and Quit"));
        m_menuPopup->AddButton(tr("Reset and Quit"));
        m_menuPopup->AddButton(tr("Close Menu"));

        popupStack->AddScreen(m_menuPopup);
    }
    else
    {
        delete m_menuPopup;
    }
}

void ScreenWizard::slotChangeCoarseFine()
{
    if (m_coarsefine)
    {
        m_coarsefine = false;
        m_change = m_fine;
    }
    else
    {
        m_coarsefine = true;
        m_change = m_coarse;
    }

    updateScreen();
}

void ScreenWizard::slotSaveSettings()
{
    LOG(VB_GENERAL, LOG_ERR, "Updating screen size settings");
    gCoreContext->SaveSetting("GuiOffsetX", m_topleft_x);
    gCoreContext->SaveSetting("GuiOffsetY", m_topleft_y);
    gCoreContext->SaveSetting("GuiWidth", m_bottomright_x - m_topleft_x);
    gCoreContext->SaveSetting("GuiHeight", m_bottomright_y - m_topleft_y);
    qApp->quit();
}

void ScreenWizard::slotResetSettings()
{
    gCoreContext->SaveSetting("GuiOffsetX", 0);
    gCoreContext->SaveSetting("GuiOffsetY", 0);
    gCoreContext->SaveSetting("GuiWidth", 0);
    gCoreContext->SaveSetting("GuiHeight", 0);
    qApp->quit();
}

bool ScreenWizard::anythingChanged()
{
    if (m_screenwidth != m_bottomright_x - m_topleft_x)
        return true;
    else if (m_xoffset != m_topleft_x)
        return true;
    else if (m_screenheight != m_bottomright_y - m_topleft_y)
        return true;
    else if (m_yoffset != m_topleft_y)
        return true;
    else
        return false;
}

void ScreenWizard::customEvent(QEvent *event)
{

    if (event->type() == DialogCompletionEvent::kEventType)
    {
        DialogCompletionEvent *dce = (DialogCompletionEvent*)(event);

        QString resultid  = dce->GetId();
        int     buttonnum = dce->GetResult();

        if (resultid == "save")
        {
            if (buttonnum == 0)
                slotSaveSettings();
            else if (buttonnum == 1)
                slotResetSettings();
            else if (buttonnum == 2)
                slotChangeCoarseFine();
        }
        else if (resultid == "nosave")
        {
            if (buttonnum == 0)
                slotResetSettings();
            if (buttonnum == 1)
                slotChangeCoarseFine();
        }
        else if (resultid == "quit")
        {
            if (buttonnum == 0)
                slotSaveSettings();
            if (buttonnum == 1)
                qApp->quit();
            if (buttonnum == 2)
                slotResetSettings();
        }

        m_menuPopup = NULL;
    }

}
