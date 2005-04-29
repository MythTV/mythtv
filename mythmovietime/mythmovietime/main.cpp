/* ============================================================
 * File  : main.cpp
 * Author: J. Donavan Stanley <jdonavan@jdonavan.net>
 *
 * Copyright 2005 by J. Donavan Stanley

 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software Foundation;
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
#include <mythtv/mythdbcon.h>

#include "mainwnd.h"
#include "settings.h"
#include "ddmovie.h"


using namespace std;

extern void UpgradeMovieTimeDatabaseSchema(void);

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
    if (!gContext->TestPopupVersion("mythmovietime",
                                    libversion,
                                    MYTH_BINARY_VERSION))
        return -1;
    
    UpgradeMovieTimeDatabaseSchema();
    
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

bool fetchData()
{
    // Don't fetch data unless it's been at least a day since our last fetch.
    // Note: The last fetch date gets reset every time the user enters the 
    //       config dialogs so that changes in the config can cause us to
    //       fetch data again.    
    QString lastFetch;
    int daysSince = 0;
    lastFetch = gContext->GetSetting("MMT-Last-Fetch");
    
    daysSince = QDate::currentDate().daysTo(QDate::fromString(lastFetch, Qt::ISODate));
    
    
    if (daysSince >= 0)
    {
        return true;
    }

    

    
    QString user;
    QString passwd;
    
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare( "SELECT userid, password FROM videosource WHERE xmltvgrabber = 'mythplus'");
    
    if (query.exec() && query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();
        user = query.value(0).toString();
        passwd = query.value(1).toString();
    }
    else
    {
        user = gContext->GetSetting("MMTUser");
        passwd = gContext->GetSetting("MMTPass");
    }
    
    QString zip = gContext->GetSetting("PostalCode");
    double radius = gContext->GetSetting("Radius", "25").toDouble();
    
    if ( user.isEmpty() || passwd.isEmpty() || zip.isEmpty() )
    {
        MythPopupBox::showOkPopup( gContext->GetMainWindow(),  "",
                                   QObject::tr("Movie data cannot be retrieved without "
                                               "supplying some basic configuration.<br><br>"
                                               "After selecting OK below you will be taken "
                                               "to the configuration screen.<br><br>After you "
                                               "have finished configuration you may run this "
                                               "plugin again."));
                                               
        mythplugin_config();
        return false;
    }

    MythProgressDialog dlg(QObject::tr("Retrieving movie showtime data"), 3);
    
    TMSMovieDirect dd;
    dlg.setProgress(1);
    
    if (!dd.importData( user, passwd, zip, radius ))
    {
        dlg.Close();
        MythPopupBox::showOkPopup( gContext->GetMainWindow(),  "",
                                   QObject::tr("Movie data cannot be retrieved "
                                               "please try again later.") );
        return false;                                               
    }
    dlg.setProgress(2);

    if ( !dd.store() )
    {
        dlg.Close();
        MythPopupBox::showOkPopup( gContext->GetMainWindow(),  "",
                                   QObject::tr("Movie data could not be saved "
                                               "please check the output in the "
                                               "console and try again later.") );
        return false;                                               
    }
    dlg.setProgress(3);
    gContext->SaveSetting("MMT-Last-Fetch", QDate::currentDate().toString(Qt::ISODate));
    dlg.Close();
    return true;    
}

int mythplugin_run(void)
{
    if (fetchData() )
        runMovietime();
    return 0;
}

int mythplugin_config(void)
{
    MMTGeneralSettings settings;
    settings.exec();
    return 0;
}



