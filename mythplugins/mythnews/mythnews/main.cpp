/* ============================================================
 * File  : main.cpp
 * Author: Renchi Raju <renchi@pooh.tam.uiuc.edu>
 * Date  : 2003-08-30
 * Description :
 *
 * Copyright 2003 by Renchi Raju

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

#include <unistd.h>

// qt
#include <qapplication.h>

// myth
#include <mythtv/mythcontext.h>
#include <mythtv/libmythui/mythmainwindow.h>
#include <mythtv/mythplugin.h>
#include <mythtv/mythpluginapi.h>

// mythnews
#include "dbcheck.h"
#include "mythnews.h"
#include "mythnewsconfig.h"

using namespace std;

void runNews(void);

void setupKeys(void)
{
    REG_JUMP("MythNews", "RSS News feed reader", "", runNews);

    REG_KEY("News", "RETRIEVENEWS", "Update news items", "I");
    REG_KEY("News", "FORCERETRIEVE", "Force update news items", "M");
    REG_KEY("News", "CANCEL", "Cancel news item updating", "C");
}

int mythplugin_init(const char *libversion)
{
    if (!gContext->TestPopupVersion("mythnews",
                                    libversion,
                                    MYTH_BINARY_VERSION))
        return -1;

    gContext->ActivateSettingsCache(false);
    if (!UpgradeNewsDatabaseSchema())
    {
        VERBOSE(VB_IMPORTANT,
                "Couldn't upgrade database to new schema, exiting.");
        return -1;
    }
    gContext->ActivateSettingsCache(false);

    setupKeys();

    return 0;
}

void runNews(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    MythNews *mythnews = new MythNews(mainStack, "mythnews");

    if (mythnews->Create())
        mainStack->AddScreen(mythnews);
}

int mythplugin_run(void)
{
    runNews();
    return 0;
}

int mythplugin_config(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    MythNewsConfig *mythnewsconfig = new MythNewsConfig(mainStack, "mythnewsconfig");

    if (mythnewsconfig->Create())
        mainStack->AddScreen(mythnewsconfig);

    return 0;
}



