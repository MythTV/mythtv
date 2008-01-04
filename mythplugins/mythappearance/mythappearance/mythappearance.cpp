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
#include <mythtv/mythcontext.h>
#include <mythtv/mythdialogs.h>
#include <mythtv/uitypes.h>
#include <mythtv/uilistbtntype.h>
#include <mythtv/xmlparse.h>

using namespace std;

#include "mythappearance.h"


MythAppearance::MythAppearance(MythMainWindow *parent, QString windowName,
                        QString themeFilename, const char *name)
        : MythThemedDialog(parent, windowName, themeFilename, name)
{


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

    menuPopup = NULL;
    m_changed = false;

    getSettings();
    getScreenInfo();
    wireUpTheme();
    setContext(1);
    m_whicharrow = true;
    updateScreen();
    m_xsize = screenwidth;
    m_ysize = screenheight;
    // work out origin co-ordinates for arrows

    m_topleftarrow_x = m_xoffset;
    m_topleftarrow_y = m_yoffset;
    m_bottomrightarrow_x = m_screenwidth - m_xoffset - m_arrowsize_x;
    m_bottomrightarrow_y = m_screenheight - m_yoffset - m_arrowsize_y;
    updateScreen();

}

MythAppearance::~MythAppearance() {}

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


void MythAppearance::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Game", e, actions);

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
        else
            handled = false;
    }
    if (!handled)
        MythThemedDialog::keyPressEvent(e);
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
        if (m_topleftarrow_x > int (screenwidth * 0.25))
            m_topleftarrow_x = int (screenwidth * 0.25);
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

void MythAppearance::wireUpTheme()
{
    m_topleftarrow = getUIImageType("topleft");
    m_arrowsize_x = m_topleftarrow->GetSize(true).width();
    m_arrowsize_y = m_topleftarrow->GetSize(true).height();
    m_bottomrightarrow = getUIImageType("bottomright");
    m_size = getUITextType("size");
    m_offsets = getUITextType("offsets");
    m_changeamount = getUITextType("changeamount");
    updateScreen();
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
    updateForeground();
}

void MythAppearance::doMenu()
{

    if (menuPopup)
        return;
    menuPopup = new MythPopupBox(gContext->GetMainWindow(), "menuPopup");
    menuPopup->addLabel("MythAppearance Menu");
    if (m_changed)
    {
        updateButton = menuPopup->addButton("Reset Screen Size Settings and Quit", this, SLOT(slotResetSettings()));
        updateButton = menuPopup->addButton("Save and Quit", this, SLOT(slotSaveSettings()));
    }
    updateButton = menuPopup->addButton("Coarse/Fine adjustment", this, SLOT(slotChangeCoarseFine()));
    OKButton = menuPopup->addButton("Close Menu", this, SLOT(closeMenu()));
    OKButton->setFocus();
    menuPopup->ShowPopup(this, SLOT(closeMenu()));
    updateScreen();
}

void MythAppearance::closeMenu(void)
{
    if (menuPopup)
    {
        menuPopup->deleteLater();
        menuPopup = NULL;
    }
}

void MythAppearance::slotSaveSettings()
{
        VERBOSE(VB_IMPORTANT, "Updating screen size settings");
        updateSettings();
        updateScreen();
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

    menuPopup->hide();
    menuPopup->deleteLater();
    menuPopup = NULL;
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
    updateScreen();
}

void MythAppearance::slotResetSettings()
{
     gContext->SaveSetting("GuiOffsetX", 0);
     gContext->SaveSetting("GuiOffsetY", 0);
     gContext->SaveSetting("GuiWidth", 0);
     gContext->SaveSetting("GuiHeight", 0);
     m_changed = false;
     GetMythMainWindow()->JumpTo("Reload Theme");
     getSettings();
     getScreenInfo();
     updateScreen();
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
