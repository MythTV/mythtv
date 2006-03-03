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

#include <mythtv/mythcontext.h>
#include <mythtv/mythdialogs.h>
#include <mythtv/mythplugin.h>

#include <mythtv/libmythui/myththemedmenu.h>

using namespace std;

extern "C" {
int mythplugin_init(const char *libversion);
int mythplugin_run(void);
int mythplugin_config(void);
}

//void runNews(void);

void browse(void){
        gContext->addCurrentLocation("flixbrowse");
        MythFlix flix(gContext->GetMainWindow(), "netflix browse");
        flix.exec();
        gContext->removeCurrentLocation();
}

void queue(void){
        gContext->addCurrentLocation("flixqueue");
        MythFlixQueue flix(gContext->GetMainWindow(), "netflix queue");
        flix.exec();
        gContext->removeCurrentLocation();
}

void history(void){
        gContext->addCurrentLocation("flixhistory");
        MythFlixQueue flix(gContext->GetMainWindow(), "netflix history");
        flix.exec();
        gContext->removeCurrentLocation();
}

void NetFlixCallback(void *data, QString &selection)
{
    VERBOSE(VB_NONE, QString("MythFlix: NetFlixCallback %1").arg(selection));

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

void runMenu()
{
    QString themedir = gContext->GetThemeDir();

    MythThemedMenu *diag = new MythThemedMenu(themedir.ascii(), 
                                              "netflix_menu.xml", 
                                              GetMythMainWindow()->GetMainStack(), 
                                              "netflix menu");

    diag->setCallback(NetFlixCallback, NULL);
    diag->setKillable();

    if (diag->foundTheme())
    {
        GetMythMainWindow()->GetMainStack()->AddScreen(diag);
    }
    else
    {
        VERBOSE(VB_IMPORTANT, QString("MythFlix: Couldn't find theme %1").arg(themedir));
        delete diag;
    }
}

void setupKeys(void)
{
    REG_JUMP("Netflix Browser", "Browse Netflix titles", "", browse);
    REG_JUMP("Netflix Queue", "Administer Netflix Queue", "", queue);
    REG_JUMP("Netflix History", "View Netflix History", "", history);

    REG_KEY("NetFlix", "MOVETOTOP", "Moves movie to top of queue", "1");
    REG_KEY("NetFlix", "REMOVE", "Removes movie from queue", "D");

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



