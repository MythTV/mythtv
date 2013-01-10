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

// myth
#include <mythcontext.h>
#include <mythversion.h>
#include <mythpluginapi.h>
#include <mythmainwindow.h>
#include <myththemedmenu.h>
#include <mythuihelper.h>
#include <mythlogging.h>

//zone minder
#include "zmsettings.h"
#include "zmconsole.h"
#include "zmliveplayer.h"
#include "zmevents.h"
#include "zmclient.h"

using namespace std;



static bool checkConnection(void)
{
    if (!ZMClient::get()->connected())
    {
        if (!ZMClient::setupZMClient())
            return false;
    }

    return true;
}

static void runZMConsole(void)
{
    if (!checkConnection())
        return;

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    ZMConsole *console = new ZMConsole(mainStack);

    if (console->Create())
        mainStack->AddScreen(console);
}

static void runZMLiveView(void)
{
    if (!checkConnection())
        return;


    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    ZMLivePlayer *player = new ZMLivePlayer(mainStack);

    if (player->Create())
        mainStack->AddScreen(player);
}

static void runZMEventView(void)
{
    if (!checkConnection())
        return;

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    ZMEvents *events = new ZMEvents(mainStack);

    if (events->Create())
        mainStack->AddScreen(events);
}

static void ZoneMinderCallback(void *data, QString &selection)
{
    (void) data;

    QString sel = selection.toLower();

    if (sel == "zm_console")
        runZMConsole();
    else if (sel == "zm_live_viewer")
        runZMLiveView();
    else if (sel == "zm_event_viewer")
        runZMEventView();
}

static int runMenu(QString which_menu)
{
    QString themedir = GetMythUI()->GetThemeDir();

    MythThemedMenu *diag = new MythThemedMenu(
        themedir, which_menu, GetMythMainWindow()->GetMainStack(),
        "zoneminder menu");

    diag->setCallback(ZoneMinderCallback, NULL);
    diag->setKillable();

    if (diag->foundTheme())
    {
        GetMythMainWindow()->GetMainStack()->AddScreen(diag);
        return 0;
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Couldn't find menu %1 or theme %2")
                .arg(which_menu).arg(themedir));
        delete diag;
        return -1;
    }
}

static void setupKeys(void)
{
    REG_JUMP(QT_TRANSLATE_NOOP("MythControls", "ZoneMinder Console"),
        "", "", runZMConsole);
    REG_JUMP(QT_TRANSLATE_NOOP("MythControls", "ZoneMinder Live View"),
        "", "", runZMLiveView);
    REG_JUMP(QT_TRANSLATE_NOOP("MythControls", "ZoneMinder Events"),
        "", "", runZMEventView);
}

int mythplugin_init(const char *libversion)
{
    if (!gCoreContext->TestPluginVersion("mythzoneminder",
                                    libversion,
                                    MYTH_BINARY_VERSION))
        return -1;

    setupKeys();

    return 0;
}

int mythplugin_run(void)
{
    // setup a connection to the mythzmserver
    if (!ZMClient::setupZMClient())
    {
        return -1;
    }

    return runMenu("zonemindermenu.xml");
}

int mythplugin_config(void)
{
    ZMSettings settings;
    settings.exec();

    return 0;
}

void mythplugin_destroy(void)
{
    delete ZMClient::get();
}

