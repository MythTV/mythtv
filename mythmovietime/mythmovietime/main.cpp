/* ============================================================
 * File  : main.cpp
 * Author: J. Donavan Stanley <jdonavan@jdonavan.net>
 *
 * Copyright 2005 by J. Donavan Stanley

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
#include <qsqldatabase.h>
#include <unistd.h>

#include <mythtv/mythcontext.h>
#include <mythtv/mythdialogs.h>
#include <mythtv/mythplugin.h>

#include "mainwnd.h"


using namespace std;



extern "C" {
int mythplugin_init(const char *libversion);
int mythplugin_run(void);
int mythplugin_config(void);
}

void runMovietime(void);

void setupKeys(void)
{
    REG_JUMP("MythMovieTime", "Movie Showtimes", "", runMovietime);

    REG_KEY("MovieTime", "BUYTICKET", "Purchase a ticket for the show", "B");
}

int mythplugin_init(const char *libversion)
{
    cerr << "MythMovieTime init " << endl;
    if (!gContext->TestPopupVersion("mythmovietime",
                                    libversion,
                                    MYTH_BINARY_VERSION))
        return -1;

    setupKeys();

    return 0;
}

void runMovietime(void)
{
    MMTMainWindow mainWnd(gContext->GetMainWindow(),
                          "movietime_main", "movietime-", "movietime main");
    qApp->unlock();
    mainWnd.exec();
    qApp->lock();    
}

int mythplugin_run(void)
{
    runMovietime();
    return 0;
}

int mythplugin_config(void)
{
    return 0;
}



