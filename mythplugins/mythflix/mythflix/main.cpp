// C++ headers
#include <iostream>
#include <unistd.h>

// QT headers
#include <QApplication>

// MythTV headers
#include <mythtv/mythcontext.h>
#include <mythtv/mythversion.h>
#include <mythtv/mythplugin.h>
#include <mythtv/mythpluginapi.h>

// MythUI headers
#include <mythtv/libmythui/myththemedmenu.h>
#include <mythtv/libmythui/mythmainwindow.h>
#include <mythtv/libmythui/mythuihelper.h>

// MythFlix headers
#include "mythflix.h"
#include "mythflixqueue.h"
#include "mythflixconfig.h"
#include "flixutil.h"
#include "dbcheck.h"

using namespace std;

void browse(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    MythFlix *mythflix = new MythFlix(mainStack, "flixbrowse");

    if (mythflix->Create())
        mainStack->AddScreen(mythflix);
}

void queue(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    MythFlixQueue *mythflix = new MythFlixQueue(mainStack, "flixqueue");

    if (mythflix->Create())
        mainStack->AddScreen(mythflix);
}

void history(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    MythFlixQueue *mythflix = new MythFlixQueue(mainStack, "flixhistory");

    if (mythflix->Create())
        mainStack->AddScreen(mythflix);
}

void NetFlixCallback(void *data, QString &selection)
{
    (void)data;
    QString sel = selection.lower();

    if (sel == "netflix_queue")
    {
        queue();
    }
    if (sel == "netflix_history")
    {
        history();
    }
    if (sel == "netflix_browse")
    {
        browse();
    }
}

int runMenu()
{
    QString menuname = "netflix_menu.xml";
    QString themedir = GetMythUI()->GetThemeDir();

    MythThemedMenu *diag = new MythThemedMenu(
        themedir, menuname, GetMythMainWindow()->GetMainStack(),
        "netflix menu");

    diag->setCallback(NetFlixCallback, NULL);
    diag->setKillable();

    if (diag->foundTheme())
    {
        GetMythMainWindow()->GetMainStack()->AddScreen(diag);
        return 0;
    }
    else
    {
        VERBOSE(VB_IMPORTANT, QString("Couldn't find menu %1 or theme %2")
                              .arg(menuname).arg(themedir));
        delete diag;
        return -1;
    }
}

void setupKeys(void)
{
    REG_JUMP("Netflix Browser", QT_TRANSLATE_NOOP("MythControls",
        "Browse Netflix titles"), "", browse);
    REG_JUMP("Netflix Queue", QT_TRANSLATE_NOOP("MythControls",
        "Administer Netflix Queue"), "", queue);
    REG_JUMP("Netflix History", QT_TRANSLATE_NOOP("MythControls",
        "View Netflix History"), "", history);

    REG_KEY("NetFlix", "MOVETOTOP", QT_TRANSLATE_NOOP("MythControls",
        "Moves movie to top of queue"), "1");

    //REG_JUMP("MythFlix", "NetFlix", "", runNews);

}

int mythplugin_run(void)
{
    return runMenu();
}

int mythplugin_config(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    MythFlixConfig *mythflixconfig = new MythFlixConfig(mainStack,
                                                        "mythflixconfig");

    if (mythflixconfig->Create())
    {
        mainStack->AddScreen(mythflixconfig);
        return 0;
    }
    else
    {
        delete mythflixconfig;
        return -1;
    }
}

int mythplugin_init(const char *libversion)
{
    if (!gContext->TestPopupVersion("mythflix",
                                    libversion,
                                    MYTH_BINARY_VERSION))
        return -1;

    gContext->ActivateSettingsCache(false);
    if (!UpgradeFlixDatabaseSchema())
    {
        VERBOSE(VB_IMPORTANT,
                "Couldn't upgrade database to new schema, exiting.");
        return -1;
    }
    gContext->ActivateSettingsCache(true);

    setupKeys();

    return 0;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
