#include <unistd.h>

#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <cstdlib>

// qt
#include <qdir.h>
#include <qdom.h>

// myth
#include <mythtv/mythcontext.h>
#include <mythtv/mythwidgets.h>

// mytharchive
#include "advancedoptions.h"

AdvancedOptions::AdvancedOptions(MythMainWindow *parent, QString window_name,
                                 QString theme_filename, const char *name)
                : MythThemedDialog(parent, window_name, theme_filename, name, true)
{
    wireUpTheme();
    assignFirstFocus();
    updateForeground();

    bCreateISO = false;
    bDoBurn = false;
    bEraseDvdRw = false;
    loadConfiguration();
}

AdvancedOptions::~AdvancedOptions(void)
{
    saveConfiguration();
}

void AdvancedOptions::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Global", e, actions);

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "ESCAPE")
        {
            done(0);
        }
        else if (action == "DOWN")
        {
            nextPrevWidgetFocus(true);
        }
        else if (action == "UP")
        {
            nextPrevWidgetFocus(false);
        }
        else if (action == "PAGEDOWN")
        {
        }
        else if (action == "PAGEUP")
        {
        }
        else if (action == "LEFT")
        {
            nextPrevWidgetFocus(false);
        }
        else if (action == "RIGHT")
        {
            nextPrevWidgetFocus(true);
        }
        else if (action == "SELECT")
        {
            activateCurrent();
        }
        else
            handled = false;
    }

    if (!handled)
            MythThemedDialog::keyPressEvent(e);
}

void AdvancedOptions::wireUpTheme()
{
    // make iso image checkbox
    createISO_check = getUICheckBoxType("makeisoimage");
    if (createISO_check)
    {
        connect(createISO_check, SIGNAL(pushed(bool)),
                this, SLOT(toggleCreateISO(bool)));
    }

    // burn dvdr checkbox
    doBurn_check = getUICheckBoxType("burntodvdr");
    if (doBurn_check)
    {
        connect(doBurn_check, SIGNAL(pushed(bool)),
                this, SLOT(toggleDoBurn(bool)));
    }

    // erase DVD RW checkbox
    eraseDvdRw_check = getUICheckBoxType("erasedvdrw");
    if (eraseDvdRw_check)
    {
        connect(eraseDvdRw_check, SIGNAL(pushed(bool)),
                this, SLOT(toggleEraseDvdRw(bool)));
    }

    // ok button
    ok_button = getUITextButtonType("ok_button");
    if (ok_button)
    {
        ok_button->setText(tr("OK"));
        connect(ok_button, SIGNAL(pushed()), this, SLOT(OKPressed()));
    }

    // cancel button
    cancel_button = getUITextButtonType("cancel_button");
    if (cancel_button)
    {
        cancel_button->setText(tr("Cancel"));
        connect(cancel_button, SIGNAL(pushed()), this, SLOT(cancelPressed()));
    }

    buildFocusList();
}

void AdvancedOptions::OKPressed()
{
    done(Accepted);
}

void AdvancedOptions::cancelPressed()
{
    reject();
}

void AdvancedOptions::loadConfiguration(void)
{
    bCreateISO = (gContext->GetSetting("MythArchiveCreateISO", "0") == "1");
    createISO_check->setState(bCreateISO);

    bDoBurn = (gContext->GetSetting("MythArchiveBurnDVDr", "1") == "1");
    doBurn_check->setState(bDoBurn);

    bEraseDvdRw = (gContext->GetSetting("MythArchiveEraseDvdRw", "0") == "1");
    eraseDvdRw_check->setState(bEraseDvdRw);
}

void AdvancedOptions::saveConfiguration(void)
{
    gContext->SaveSetting("MythArchiveCreateISO", 
                (createISO_check->getState() ? "1" : "0"));
    gContext->SaveSetting("MythArchiveBurnDVDr", 
                (doBurn_check->getState() ? "1" : "0"));
    gContext->SaveSetting("MythArchiveEraseDvdRw", 
                (eraseDvdRw_check->getState() ? "1" : "0"));
}
