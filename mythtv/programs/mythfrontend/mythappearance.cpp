#define MYTHAPPEARANCE_CPP

/* QT includes */
#include <qnamespace.h>
#include <qstringlist.h>
#include <qapplication.h>
#include <qdom.h>
#include <qbuttongroup.h>
#include <qimage.h>
#include <qevent.h>
#include <qdir.h>
#include <qstring.h>

/* MythTV includes */
#include "mythcontext.h"
#include "mythmainwindow.h"
#include "myththemebase.h"

using namespace std;

#include "mythappearance.h"


MythAppearance::MythAppearance(MythScreenStack *parent, const char *name)
    : MythScreenType(parent, name)
{
    gContext->addCurrentLocation("AppearanceWizard");
    // Initialise $stuff
    // Get UI size & offset settings from database

    m_coarsefine = false; // fine adjustments by default
    m_fine = 1; //fine moves arrows by one pixel
    m_coarse = 10; // coarse moves arrows by ten pixels
    m_change = m_fine;
    m_screenheight = 0, m_screenwidth = 0;
    m_arrowsize_x = 30; // maybe get the image size later
    m_arrowsize_y = 30; // maybe get the image size later
    m_topleftarrow_x = 0;
    m_topleftarrow_y = 0;

    m_menuPopup = NULL;
    m_changed = false;

    getSettings();
    getScreenInfo();
    m_whicharrow = true;

    m_xsize = GetMythMainWindow()->GetUIScreenRect().width();
    m_ysize = GetMythMainWindow()->GetUIScreenRect().height();

}

MythAppearance::~MythAppearance() {}


bool MythAppearance::Create()
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("appear-ui.xml", "appearance", this);

    if (!foundtheme)
    {
        VERBOSE(VB_IMPORTANT, "Unable to load window appearance from "
                              "appear-ui.xml");
        return false;
    }

    m_topleftarrow = dynamic_cast<MythUIImage *>
                (GetChild("topleft"));

    m_bottomrightarrow = dynamic_cast<MythUIImage *>
                (GetChild("bottomright"));

    m_size = dynamic_cast<MythUIText *>
                (GetChild("size"));

    m_offsets = dynamic_cast<MythUIText *>
                (GetChild("offsets"));

    m_changeamount = dynamic_cast<MythUIText *>
                (GetChild("changeamount"));

    m_offsets = dynamic_cast<MythUIText *>
                (GetChild("offsets"));

    m_changeamount = dynamic_cast<MythUIText *>
                (GetChild("changeamount"));


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

void MythAppearance::setContext(int context)
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

void MythAppearance::getSettings()
{
    float m_wmult = 0, m_hmult = 0;
    gContext->GetScreenSettings(m_screenwidth, m_wmult, m_screenheight, m_hmult);
}

void MythAppearance::getScreenInfo()
{
    m_xoffset_old = gContext->GetNumSetting("GuiOffsetX", 0);
    m_yoffset_old = gContext->GetNumSetting("GuiOffsetY", 0);
    m_xoffset = m_xoffset_old;
    m_yoffset = m_yoffset_old;
}


bool MythAppearance::keyPressEvent(QKeyEvent *event)
{
    QStringList actions;
    bool handled = false;

    gContext->GetMainWindow()->TranslateKeyPress("Global", event, actions);

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
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
        else if (action == "ESCAPE")
        {
            gContext->removeCurrentLocation();
            GetMythMainWindow()->GetMainStack()->PopScreen();
        }
        else
            handled = false;
    }

    return handled;
}

void MythAppearance::swapArrows()
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

void MythAppearance::moveUp()
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

void MythAppearance::moveLeft()
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

void MythAppearance::moveDown()
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

void MythAppearance::moveRight()
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

void MythAppearance::updateScreen()
{
    m_xsize = (m_bottomrightarrow_x - m_topleftarrow_x + m_arrowsize_x);
    m_ysize = (m_bottomrightarrow_y - m_topleftarrow_y + m_arrowsize_y);
    m_xoffset = m_topleftarrow_x;
    m_yoffset = m_topleftarrow_y;
    m_topleftarrow->SetPosition(QPoint(m_topleftarrow_x, m_topleftarrow_y));
    m_bottomrightarrow->SetPosition(QPoint(m_bottomrightarrow_x, m_bottomrightarrow_y));
    m_size->SetText(QString("Size: %1 x %2").arg(m_xsize).arg(m_ysize));
    m_offsets->SetText(QString("Offset: %1 x %2").arg(m_xoffset).arg(m_yoffset));
    m_changeamount->SetText(QString("Change amount: %1 pixel(s)").arg(m_change));

}

void MythAppearance::doMenu()
{
    if (m_menuPopup)
        return;

    QString label = "MythAppearance Menu";

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    m_menuPopup = new MythDialogBox(label, mainStack, "menuPopup");

    if (m_menuPopup->Create())
        mainStack->AddScreen(m_menuPopup);

    if (m_changed)
    {
        m_menuPopup->SetReturnEvent(this, "save");
        m_menuPopup->AddButton("Save and Quit");
    }
    else
        m_menuPopup->SetReturnEvent(this, "nosave");

    m_menuPopup->AddButton("Reset Screen Size Settings and Quit");
    m_menuPopup->AddButton("Coarse/Fine adjustment");
    m_menuPopup->AddButton("Close Menu");

    updateScreen();
}

void MythAppearance::slotSaveSettings()
{
        VERBOSE(VB_IMPORTANT, "Updating screen size settings");
        updateSettings();
}

void MythAppearance::slotChangeCoarseFine()
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

void MythAppearance::updateSettings()
{
    gContext->SaveSetting("GuiOffsetX", m_xoffset);
    gContext->SaveSetting("GuiOffsetY", m_yoffset);
    gContext->SaveSetting("GuiWidth", m_xsize);
    gContext->SaveSetting("GuiHeight", m_ysize);
    VERBOSE(VB_IMPORTANT, "Updated screen size settings");
    GetMythMainWindow()->JumpTo("Reload Theme");
}

void MythAppearance::slotResetSettings()
{
     gContext->SaveSetting("GuiOffsetX", 0);
     gContext->SaveSetting("GuiOffsetY", 0);
     gContext->SaveSetting("GuiWidth", 0);
     gContext->SaveSetting("GuiHeight", 0);
     m_changed = false;
     GetMythMainWindow()->JumpTo("Reload Theme");
}

void MythAppearance::anythingChanged()
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

void MythAppearance::customEvent(QCustomEvent *event)
{

    if (event->type() == kMythDialogBoxCompletionEventType)
    {
        DialogCompletionEvent *dce =
                                dynamic_cast<DialogCompletionEvent*>(event);

        QString resultid= dce->GetId();
        int buttonnum  = dce->GetResult();

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
