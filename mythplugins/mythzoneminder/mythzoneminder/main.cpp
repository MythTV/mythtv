/* ============================================================
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published bythe Free Software Foundation;
 * either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * ============================================================ */

#include <iostream>
#include <unistd.h>

// qt
#include <qapplication.h>
#include <qsqldatabase.h>

// myth
#include <mythtv/mythcontext.h>
#include <mythtv/mythdialogs.h>
#include <mythtv/mythplugin.h>
#include <mythtv/libmythui/myththemedmenu.h>

//zone minder
#include "zmconsole.h"
#include "zmplayer.h"
#include "zmevents.h"
#include "zmliveplayer.h"
//#include "zmutils.h"
#include "zmsettings.h"
#include "zmclient.h"

using namespace std;

extern "C" {
int mythplugin_init(const char *libversion);
int mythplugin_run(void);
int mythplugin_config(void);
}

void runZMConsole(void);

void setupKeys(void)
{

}

int mythplugin_init(const char *libversion)
{
    if (!gContext->TestPopupVersion("mythzoneminder",
                                    libversion,
                                    MYTH_BINARY_VERSION))
        return -1;

    setupKeys();

    return 0;
}

void runZMConsole(void)
{
    gContext->addCurrentLocation("mythzoneminder");
    ZMConsole console(gContext->GetMainWindow(), "zmconsole",
                      "zoneminder-", "zmconsole");
    console.exec();
    gContext->removeCurrentLocation();
}

void runZMLiveView(void)
{
    ZMLivePlayer player(1, 1, gContext->GetMainWindow(), "zmliveplayer",
                        "zoneminder-", "zmplayer");
    player.exec();
}

void runZMEventView(void)
{
    ZMEvents events(gContext->GetMainWindow(), "zmevents", "zoneminder-", "zmevents");
    events.exec();
}

void ZoneMinderCallback(void *data, QString &selection)
{
    (void) data;

    QString sel = selection.lower();

    if (sel == "zm_console")
        runZMConsole();
    else if (sel == "zm_live_viewer")
        runZMLiveView();
    else if (sel == "zm_event_viewer")
        runZMEventView();
}

void runMenu(QString which_menu)
{
    QString themedir = gContext->GetThemeDir();

    MythThemedMenu *diag = new MythThemedMenu(themedir.ascii(), which_menu, 
                                              GetMythMainWindow()->GetMainStack(),
                                              "zoneminder menu");

    diag->setCallback(ZoneMinderCallback, NULL);
    diag->setKillable();

    if (diag->foundTheme())
    {
        GetMythMainWindow()->GetMainStack()->AddScreen(diag);
    }
    else
    {
        cerr << "Couldn't find theme " << themedir << endl;
    }
}

int mythplugin_run(void)
{
    // setup a connection to the mythzmserver
    if (!ZMClient::setupZMClient())
    {
        return -1;
    }

    runMenu("zonemindermenu.xml");

    return 0;
}

int mythplugin_config(void)
{
    ZMSettings settings;
    settings.exec();

    return 0;
}



