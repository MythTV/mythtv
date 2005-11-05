/* ============================================================
 * File  : main.cpp
 * Author: John Petrocik <john@petrocik.net>
 * Date  : 2005-10-28
 * Description :
 *
 * Copyright 2005 by John Petrocik

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

#include <qapplication.h>
#include <unistd.h>

#include "mythflix.h"
#include "mythflixqueue.h"
#include "mythflixconfig.h"

#include <mythtv/themedmenu.h>
#include <mythtv/mythcontext.h>
#include <mythtv/mythdialogs.h>
#include <mythtv/mythplugin.h>

using namespace std;

extern "C" {
int mythplugin_init(const char *libversion);
int mythplugin_run(void);
int mythplugin_config(void);
}

//void runNews(void);

void browse(void){
        MythFlix flix(gContext->GetMainWindow(), "netflix browse");
        flix.exec();
}

void queue(void){
        MythFlixQueue flix(gContext->GetMainWindow(), "netflix queue");
        flix.exec();
}

void NetFlixCallback(void *data, QString &selection)
{
    cerr << "NetFlixCallback " << selection << endl;

    (void)data;
    QString sel = selection.lower();

    if (sel == "netflix_queue")
    {
        queue();
    }
    if (sel == "netflix_browse")
    {
        browse();
    }
}

void runMenu()
{
    QString themedir = gContext->GetThemeDir();

    ThemedMenu *diag = new ThemedMenu(themedir.ascii(), "netflix_menu.xml", 
                                      gContext->GetMainWindow(), "netflix menu");

    diag->setCallback(NetFlixCallback, NULL);
    diag->setKillable();

    if (diag->foundTheme())
    {
        diag->exec();
    }
    else
    {
        cerr << "Couldn't find theme " << themedir << endl;
    }

    delete diag;
}

void setupKeys(void)
{
    REG_JUMP("Browse", "", "", browse);

    REG_KEY("NetFlix", "RETRIEVENEWS", "Update news items", "I");
    REG_KEY("NetFlix", "FORCERETRIEVE", "Force update news items", "M");
    REG_KEY("NetFlix", "CANCEL", "Cancel news item updating", "C");

    //REG_JUMP("MythFlix", "NetFlix", "", runNews);

}

int mythplugin_run(void)
{
    runMenu();
    return 0;
}

int mythplugin_config(void)
{
    MythFlixConfig config(gContext->GetMainWindow(), "netflix");
    config.exec();

    return 0;
}

int mythplugin_init(const char *libversion)
{
    if (!gContext->TestPopupVersion("mythflix",
                                    libversion,
                                    MYTH_BINARY_VERSION))
        return -1;

    setupKeys();

    return 0;
}



