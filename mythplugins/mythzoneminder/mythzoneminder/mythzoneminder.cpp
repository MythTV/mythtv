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

// C++
#include <iostream>
#include <unistd.h>

// MythTV
#include <libmyth/mythcontext.h>
#include <libmythbase/mythlogging.h>
#include <libmythbase/mythpluginapi.h>
#include <libmythbase/mythversion.h>
#include <libmythui/mythmainwindow.h>
#include <libmythui/myththemedmenu.h>
#include <libmythui/mythuihelper.h>

//zone minder
#include "zmsettings.h"
#include "zmconsole.h"
#include "zmliveplayer.h"
#include "zmevents.h"
#include "zmclient.h"
#include "zmminiplayer.h"
#include "alarmnotifythread.h"

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

    auto *console = new ZMConsole(mainStack);

    if (console->Create())
        mainStack->AddScreen(console);
}

static void runZMLiveView(void)
{
    if (!checkConnection())
        return;


    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    auto *player = new ZMLivePlayer(mainStack);

    if (player->Create())
        mainStack->AddScreen(player);
}

static void runZMEventView(void)
{
    if (!checkConnection())
        return;

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    auto *events = new ZMEvents(mainStack);

    if (events->Create())
        mainStack->AddScreen(events);
}

static void runZMMiniPlayer(void)
{
    if (!ZMClient::get()->isMiniPlayerEnabled())
        return;

    if (!checkConnection())
        return;

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    auto *miniPlayer = new ZMMiniPlayer(mainStack);

    if (miniPlayer->Create())
        mainStack->AddScreen(miniPlayer);
}

// these point to the the mainmenu callback if found
static void (*m_callback)(void *, QString &) = nullptr;
static void *m_callbackdata = nullptr;

static void ZoneMinderCallback([[maybe_unused]] void *data, QString &selection)
{
    QString sel = selection.toLower();

    if (sel == "zm_console")
        runZMConsole();
    else if (sel == "zm_live_viewer")
        runZMLiveView();
    else if (sel == "zm_event_viewer")
        runZMEventView();
    else
    {
        // if we have found the mainmenu callback
        // pass the selection on to it
        if (m_callback && m_callbackdata)
            m_callback(m_callbackdata, selection);
    }
}

static int runMenu(const QString& which_menu)
{
    QString themedir = GetMythUI()->GetThemeDir();

    // find the 'mainmenu' MythThemedMenu so we can use the callback from it
    MythThemedMenu *mainMenu = nullptr;
    QObject *parentObject = GetMythMainWindow()->GetMainStack()->GetTopScreen();

    while (parentObject)
    {
        mainMenu = qobject_cast<MythThemedMenu *>(parentObject);

        if (mainMenu && mainMenu->objectName() == "mainmenu")
            break;

        parentObject = parentObject->parent();
    }

    auto *diag = new MythThemedMenu(
        themedir, which_menu, GetMythMainWindow()->GetMainStack(),
        "zoneminder menu");

    // save the callback from the main menu
    if (mainMenu)
        mainMenu->getCallback(&m_callback, &m_callbackdata);
    else
    {
        m_callback = nullptr;
        m_callbackdata = nullptr;
    }

    diag->setCallback(ZoneMinderCallback, nullptr);
    diag->setKillable();

    if (diag->foundTheme())
    {
        GetMythMainWindow()->GetMainStack()->AddScreen(diag);
        return 0;
    }
    LOG(VB_GENERAL, LOG_ERR, QString("Couldn't find menu %1 or theme %2")
        .arg(which_menu, themedir));
    delete diag;
    return -1;
}

static void setupKeys(void)
{
    REG_JUMP(QT_TRANSLATE_NOOP("MythControls", "ZoneMinder Console"),
        "", "", runZMConsole);
    REG_JUMP(QT_TRANSLATE_NOOP("MythControls", "ZoneMinder Live View"),
        "", "", runZMLiveView);
    REG_JUMP(QT_TRANSLATE_NOOP("MythControls", "ZoneMinder Events"),
        "", "", runZMEventView);
    REG_JUMPEX(QT_TRANSLATE_NOOP("MythControls", "ZoneMinder Mini Live View"),
        "", "", runZMMiniPlayer, false);
}

int mythplugin_init(const char *libversion)
{
    if (!MythCoreContext::TestPluginVersion("mythzoneminder",
                                            libversion,
                                            MYTH_BINARY_VERSION))
        return -1;

    // setup a connection to the mythzmserver
    (void) checkConnection();

    setupKeys();

    // create the alarm polling thread
    AlarmNotifyThread::get()->start();

    return 0;
}

int mythplugin_run(void)
{

    return runMenu("zonemindermenu.xml");
}

int mythplugin_config(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    auto *ssd = new StandardSettingDialog(mainStack, "zonemindersettings",
                                          new ZMSettings());

    if (ssd->Create())
        mainStack->AddScreen(ssd);
    else
        delete ssd;

    return 0;
}

void mythplugin_destroy(void)
{
    AlarmNotifyThread::get()->stop();
    delete AlarmNotifyThread::get();
    delete ZMClient::get();
}

