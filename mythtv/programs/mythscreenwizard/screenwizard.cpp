// Qt
#include <QBrush>
#include <QColor>
#include <QCoreApplication>
#include <QString>
#include <QStringList>

// MythTV
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"
#include "libmythui/mythmainwindow.h"
#include "libmythui/mythrect.h"
#include "libmythui/mythuihelper.h"
#include "libmythui/themeinfo.h"

// MythScreenWizard
#include "screenwizard.h"

static constexpr int kMinWidth  { 160 };
static constexpr int kMinHeight { 160 };

ScreenWizard::ScreenWizard(MythScreenStack *parent, const char *name) :
    MythScreenType(parent, name),
    m_xSize(GetMythMainWindow()->GetUIScreenRect().width()),
    m_ySize(GetMythMainWindow()->GetUIScreenRect().height())
{
}

void ScreenWizard::SetInitialSettings(int _x, int _y, int _w, int _h)
{
    m_xOffset = _x;
    m_yOffset = _y;

    m_screenWidth  = _w ? _w : m_xSize;
    m_screenHeight = _h ? _h : m_ySize;
}

bool ScreenWizard::Create()
{
    // Load the theme for this screen
    bool foundtheme = LoadWindowFromXML("appear-ui.xml", "appearance", this);
    if (!foundtheme)
        return false;

    m_blackout = dynamic_cast<MythUIShape *> (GetChild("blackout"));
    m_preview = dynamic_cast<MythUIImage *> (GetChild("preview"));
    m_size = dynamic_cast<MythUIText *> (GetChild("size"));
    m_offsets = dynamic_cast<MythUIText *> (GetChild("offsets"));
    m_changeAmount = dynamic_cast<MythUIText *> (GetChild("changeamount"));

    if (!m_blackout || !m_preview || !m_size || !m_offsets || !m_changeAmount)
    {
        LOG(VB_GENERAL, LOG_ERR, "ScreenWizard, Error: "
               "Could not instantiate, please check appear-ui.xml for errors");
        return false;
    }

    m_blackout->SetArea(MythRect(0, 0, m_xSize, m_ySize));

    // work out origin co-ordinates for preview corners
    m_topLeftX = m_xOffset;
    m_topLeftY = m_yOffset;
    m_bottomRightX = m_xOffset + m_screenWidth;
    m_bottomRightY = m_yOffset + m_screenHeight;

    updateScreen();

    return true;
}

bool ScreenWizard::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget() && GetFocusWidget()->keyPressEvent(event))
        return true;

    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("Global", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];
        handled = true;
        bool refresh = false;

        if (action == "SELECT")
        {
            m_whichCorner = !m_whichCorner;
            refresh = true;
        }
        else if (action == "UP")
        {
            if (m_whichCorner)
                refresh = moveTLUp();
            else
                refresh = moveBRUp();
        }
        else if (action == "DOWN")
        {
            if (m_whichCorner)
                refresh = moveTLDown();
            else
                refresh = moveBRDown();
        }
        else if (action == "LEFT")
        {
            if (m_whichCorner)
                refresh = moveTLLeft();
            else
                refresh = moveBRLeft();
        }
        else if (action == "RIGHT")
        {
            if (m_whichCorner)
                refresh = moveTLRight();
            else
                refresh = moveBRRight();
        }
        else if (action == "MENU")
        {
            doMenu();
        }
        else if (action == "ESCAPE")
        {
            if (anythingChanged())
                doExit();
            else
                qApp->quit();
        }
        else
        {
            handled = false;
        }

        if (refresh)
            updateScreen();
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

bool ScreenWizard::moveTLUp(void)
{
    if (m_topLeftY < (0 + m_change))
        return false;

    m_topLeftY -= m_change;
    return true;
}

bool ScreenWizard::moveTLDown(void)
{
    if (m_topLeftY > (m_bottomRightY - m_change - kMinHeight))
        if (!moveBRDown())
            return false;

    m_topLeftY += m_change;
    return true;
}

bool ScreenWizard::moveTLLeft(void)
{
    if (m_topLeftX < (0 + m_change))
        return false;

    m_topLeftX -= m_change;
    return true;
}

bool ScreenWizard::moveTLRight(void)
{
    if (m_topLeftX > (m_bottomRightX - m_change - kMinWidth))
        if (!moveBRRight())
            return false;

    m_topLeftX += m_change;
    return true;
}

bool ScreenWizard::moveBRUp(void)
{
    if (m_bottomRightY < (m_topLeftY + m_change + kMinHeight))
        if (!moveTLUp())
            return false;

    m_bottomRightY -= m_change;
    return true;
}

bool ScreenWizard::moveBRDown(void)
{
    if (m_bottomRightY > (m_ySize - m_change))
        return false;

    m_bottomRightY += m_change;
    return true;
}

bool ScreenWizard::moveBRLeft(void)
{
    if (m_bottomRightX < (m_topLeftX + m_change + kMinWidth))
        if (!moveTLLeft())
            return false;

    m_bottomRightX -= m_change;
    return true;
}

bool ScreenWizard::moveBRRight(void)
{
    if (m_bottomRightX > (m_xSize - m_change))
        return false;

    m_bottomRightX += m_change;
    return true;
}

void ScreenWizard::updateScreen()
{
    int width = m_bottomRightX - m_topLeftX;
    int height = m_bottomRightY - m_topLeftY;

    m_preview->SetArea(MythRect(m_topLeftX, m_topLeftY, width, height));
    m_size->SetText(tr("Size: %1 x %2").arg(width).arg(height));
    m_offsets->SetText(tr("Offset: %1 x %2").arg(m_topLeftX).arg(m_topLeftY));
    m_changeAmount->SetText(tr("Change amount: %n pixel(s)", "", m_change));

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
        {
            m_menuPopup->SetReturnEvent(this, "nosave");
        }

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
    if (m_coarseFine)
    {
        m_coarseFine = false;
        m_change = m_fine;
    }
    else
    {
        m_coarseFine = true;
        m_change = m_coarse;
    }

    updateScreen();
}

void ScreenWizard::slotSaveSettings() const
{
    LOG(VB_GENERAL, LOG_ERR, "Updating screen size settings");
    gCoreContext->SaveSetting("GuiOffsetX", m_topLeftX);
    gCoreContext->SaveSetting("GuiOffsetY", m_topLeftY);
    gCoreContext->SaveSetting("GuiWidth", m_bottomRightX - m_topLeftX);
    gCoreContext->SaveSetting("GuiHeight", m_bottomRightY - m_topLeftY);
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

bool ScreenWizard::anythingChanged() const
{
    if (m_screenWidth != m_bottomRightX - m_topLeftX)
        return true;
    if (m_xOffset != m_topLeftX)
        return true;
    if (m_screenHeight != m_bottomRightY - m_topLeftY)
        return true;
    if (m_yOffset != m_topLeftY)
        return true;
    return false;
}

void ScreenWizard::customEvent(QEvent *event)
{

    if (event->type() == DialogCompletionEvent::kEventType)
    {
        auto *dce = (DialogCompletionEvent*)(event);

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

        m_menuPopup = nullptr;
    }

}
