
#include "screenwizard.h"

/* QT includes */
#include <QStringList>
#include <QCoreApplication>
#include <QString>

/* MythTV includes */
#include "mythcorecontext.h"
#include "mythmainwindow.h"
#include "mythuihelper.h"
#include "mythverbose.h"

using namespace std;


ScreenWizard::ScreenWizard(MythScreenStack *parent, const char *name) :
    MythScreenType(parent, name),
    m_x_offset(0),           m_y_offset(0),
    m_whicharrow(true),
    m_coarsefine(false), // fine adjustments by default
    m_changed(false),
    m_fine(1),           // fine moves arrows by one pixel
    m_coarse(10),        // coarse moves arrows by ten pixels
    m_change(m_fine),
    m_topleftarrow_x(0),     m_topleftarrow_y(0),
    m_bottomrightarrow_x(0), m_bottomrightarrow_y(0),
    m_arrowsize_x(30),   // maybe get the image size later
    m_arrowsize_y(30),   // maybe get the image size later
    m_screenwidth(0),        m_screenheight(0),
    m_xsize(GetMythMainWindow()->GetUIScreenRect().width()),
    m_ysize(GetMythMainWindow()->GetUIScreenRect().height()),
    m_menuPopup(NULL)
{
    // Initialise $stuff
    // Get UI size & offset settings from database
    getSettings();
    getScreenInfo();
}

ScreenWizard::~ScreenWizard() {}


bool ScreenWizard::Create()
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("appear-ui.xml", "appearance", this);

    if (!foundtheme)
        return false;

    m_topleftarrow = dynamic_cast<MythUIImage *> (GetChild("topleft"));
    m_bottomrightarrow = dynamic_cast<MythUIImage *> (GetChild("bottomright"));

    m_size = dynamic_cast<MythUIText *> (GetChild("size"));
    m_offsets = dynamic_cast<MythUIText *> (GetChild("offsets"));
    m_changeamount = dynamic_cast<MythUIText *> (GetChild("changeamount"));
    if (!m_topleftarrow || !m_bottomrightarrow || !m_size || !m_offsets ||
        !m_changeamount)
    {
        VERBOSE(VB_IMPORTANT, "ScreenWizard, Error: "
                "Could not instantiate, please check appear-ui.xml for errors");
        return false;
    }

    m_arrowsize_x = m_topleftarrow->GetArea().width();
    m_arrowsize_y = m_topleftarrow->GetArea().height();

    // work out origin co-ordinates for arrows
    m_topleftarrow_x = m_xoffset;
    m_topleftarrow_y = m_yoffset;
    m_bottomrightarrow_x = m_screenwidth - m_xoffset - m_arrowsize_x;
    m_bottomrightarrow_y = m_screenheight - m_yoffset - m_arrowsize_y;

    setContext(1);
    updateScreen();

    return true;
}

void ScreenWizard::setContext(int context)
{

    if (context == 1)
    {
        m_topleftarrow->SetVisible(true);
        m_bottomrightarrow->SetVisible(false);
    }
    else if (context == 2)
    {
        m_bottomrightarrow->SetVisible(true);
        m_topleftarrow->SetVisible(false);
    }

}

void ScreenWizard::getSettings()
{
    float m_wmult = 0, m_hmult = 0;
    GetMythUI()->GetScreenSettings(m_screenwidth, m_wmult, m_screenheight, m_hmult);
}

void ScreenWizard::getScreenInfo()
{
    m_xoffset_old = gCoreContext->GetNumSetting("GuiOffsetX", 0);
    m_yoffset_old = gCoreContext->GetNumSetting("GuiOffsetY", 0);
    m_xoffset = m_xoffset_old;
    m_yoffset = m_yoffset_old;
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

        if (action == "SELECT")
            swapArrows();
        else if (action == "UP")
            moveUp();
        else if (action == "DOWN")
            moveDown();
        else if (action == "LEFT")
            moveLeft();
        else if (action == "RIGHT")
            moveRight();
        else if (action == "MENU")
            doMenu();
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void ScreenWizard::swapArrows()
{
    if (m_whicharrow)
    {
        setContext(2);
        m_whicharrow = false;
    }

    else
    {
        setContext(1);
        m_whicharrow = true;
    }

// now update the screen
    updateScreen();
}

void ScreenWizard::moveUp()
{
    if (m_whicharrow) // do the top left arrow
    {
        m_topleftarrow_y -= m_change;
        if (m_topleftarrow_y < 0)
            m_topleftarrow_y = 0;
    }
    else // do the bottom right arrow
    {
            m_bottomrightarrow_y -= m_change;
            if (m_bottomrightarrow_y < int(m_screenheight * 0.75))
                m_bottomrightarrow_y = int(m_screenheight * 0.75);
    }

    updateScreen(); // now update the screen
    anythingChanged();
}

void ScreenWizard::moveLeft()
{
    if (m_whicharrow) // do the top left arrow
    {
        m_topleftarrow_x -= m_change;
        if (m_topleftarrow_x < 0)
            m_topleftarrow_x = 0;
    }
    else // do the bottom right arrow
    {
        m_bottomrightarrow_x -= m_change;
        if (m_bottomrightarrow_x < int(m_screenwidth * 0.75))
            m_bottomrightarrow_x = int(m_screenwidth * 0.75);
    }

    updateScreen(); // now update the screen
    anythingChanged();
}

void ScreenWizard::moveDown()
{
    if (m_whicharrow) // do the top left arrow
    {
        {
            m_topleftarrow_y += m_change;
            if (m_topleftarrow_y > int(m_screenheight * 0.25))
                m_topleftarrow_y = int(m_screenheight * 0.25);
        }
    }
    else // do the bottom right arrow
    {
        m_bottomrightarrow_y += m_change;
        if (m_bottomrightarrow_y > m_screenheight - m_arrowsize_y)
            m_bottomrightarrow_y = m_screenheight - m_arrowsize_y;
    }

    updateScreen(); // now update the screen
    anythingChanged();
}

void ScreenWizard::moveRight()
{
    if (m_whicharrow) // do the top left arrow
    {
        m_topleftarrow_x += m_change;
        if (m_topleftarrow_x > int (m_screenwidth * 0.25))
            m_topleftarrow_x = int (m_screenwidth * 0.25);
    }
    else // do the bottom right arrow
    {
        m_bottomrightarrow_x += m_change;
        if (m_bottomrightarrow_x > m_screenwidth - m_arrowsize_x)
            m_bottomrightarrow_x = m_screenwidth - m_arrowsize_x;
    }

    updateScreen(); // now update the screen
    anythingChanged();
}

void ScreenWizard::updateScreen()
{
    m_xsize = (m_bottomrightarrow_x - m_topleftarrow_x + m_arrowsize_x);
    m_ysize = (m_bottomrightarrow_y - m_topleftarrow_y + m_arrowsize_y);
    m_xoffset = m_topleftarrow_x;
    m_yoffset = m_topleftarrow_y;
    m_topleftarrow->SetPosition(QPoint(m_topleftarrow_x, m_topleftarrow_y));
    m_bottomrightarrow->SetPosition(QPoint(m_bottomrightarrow_x, m_bottomrightarrow_y));
    m_size->SetText(tr("Size: %1 x %2").arg(m_xsize).arg(m_ysize));
    m_offsets->SetText(tr("Offset: %1 x %2").arg(m_xoffset).arg(m_yoffset));
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
        if (m_changed)
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

void ScreenWizard::slotSaveSettings()
{
        VERBOSE(VB_IMPORTANT, "Updating screen size settings");
        updateSettings();
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

void ScreenWizard::updateSettings()
{
    gCoreContext->SaveSetting("GuiOffsetX", m_xoffset);
    gCoreContext->SaveSetting("GuiOffsetY", m_yoffset);
    gCoreContext->SaveSetting("GuiWidth", m_xsize);
    gCoreContext->SaveSetting("GuiHeight", m_ysize);
    VERBOSE(VB_IMPORTANT, "Updated screen size settings");
    GetMythMainWindow()->JumpTo("Reload Theme");
}

void ScreenWizard::slotResetSettings()
{
     gCoreContext->SaveSetting("GuiOffsetX", 0);
     gCoreContext->SaveSetting("GuiOffsetY", 0);
     gCoreContext->SaveSetting("GuiWidth", 0);
     gCoreContext->SaveSetting("GuiHeight", 0);
     m_changed = false;
     GetMythMainWindow()->JumpTo("Reload Theme");
}

void ScreenWizard::anythingChanged()
{
    if (m_xsize != m_screenwidth)
        m_changed = true;
    else if (m_xoffset != m_xoffset_old)
            m_changed = true;
    else if (m_ysize != m_screenheight)
            m_changed = true;
    else if (m_yoffset != m_yoffset_old)
            m_changed = true;
    else m_changed = false;
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

        m_menuPopup = NULL;
    }

}
