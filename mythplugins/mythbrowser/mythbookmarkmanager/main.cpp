/*
 * main.cpp
 *
 * Copyright (C) 2003
 *
 * Author:
 * - Philippe Cattin <cattin@vision.ee.ethz.ch>
 *
 * Bugfixes from:
 *
 * Translations by:
 *
 */

#include <iostream>

#include <qapplication.h>
#include <unistd.h>

#include "bookmarkmanager.h"

#include <mythtv/mythcontext.h>
#include <mythtv/mythdialogs.h>
#include <mythtv/mythplugin.h>
#include <mythtv/langsettings.h>

using namespace std;

extern "C" {
int mythplugin_init(const char *libversion);
int mythplugin_run(void);
int mythplugin_config(void);
}

int mythplugin_init(const char *libversion)
{
    if (!gContext->TestPopupVersion("mythbookmarks", libversion, MYTH_BINARY_VERSION))
        return -1;
    LanguageSettings::load("mythbrowser");
    return 0;
}

int mythplugin_run(void)
{
    Bookmarks bookmarks(gContext->GetMainWindow(), "bookmarks");
    bookmarks.exec();

    return 0;
}

int mythplugin_config(void)
{
    BookmarksConfig config(gContext->GetMainWindow(),"bookmarks");
    config.exec();

    return 0;
}
