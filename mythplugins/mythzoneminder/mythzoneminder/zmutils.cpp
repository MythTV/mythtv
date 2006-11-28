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

// qt
#include <qsqldatabase.h>

// myth
#include <mythtv/mythcontext.h>

//zm
#include "zmutils.h"

QSqlDatabase *g_ZMDatabase = NULL;
ZMConfig     *g_ZMConfig = NULL;

// based on code from zm_config.cpp
bool loadZMConfig()
{
    QString configFile = gContext->GetSetting("ZoneMinderConfigDir", "/etc") + "/zm.conf";

    if (!g_ZMConfig)
        g_ZMConfig = new ZMConfig;

    FILE *cfg;
    char line[512];
    char *val;
    if ( (cfg = fopen(configFile, "r")) == NULL )
    {
        VERBOSE(VB_IMPORTANT, QString("MythZoneMinder: Can't open ZM config file - %1")
                .arg(configFile));
        delete g_ZMConfig;
        g_ZMConfig = NULL;
        return false;
    }

    while ( fgets( line, sizeof(line), cfg ) != NULL )
    {
        char *line_ptr = line;
        // Trim off any cr/lf line endings
        int chomp_len = strcspn( line_ptr, "\r\n" );
        line_ptr[chomp_len] = '\0';

        // Remove leading white space
        int white_len = strspn( line_ptr, " \t" );
        line_ptr += white_len;

        // Check for comment or empty line
        if ( *line_ptr == '\0' || *line_ptr == '#' )
            continue;
        // Remove trailing white space
        char *temp_ptr = line_ptr+strlen(line_ptr)-1;
        while ( *temp_ptr == ' ' || *temp_ptr == '\t' )
        {
            *temp_ptr-- = '\0';
            temp_ptr--;
        }

        // Now look for the '=' in the middle of the line
        temp_ptr = strchr( line_ptr, '=' );
        if ( !temp_ptr )
        {
            fprintf(stderr,"Invalid data in %s: '%s'\n", configFile.ascii(), line );
            continue;
        }

        // Assign the name and value parts
        char *name_ptr = line_ptr;
        char *val_ptr = temp_ptr+1;

        // Trim trailing space from the name part
        do
        {
            *temp_ptr = '\0';
            temp_ptr--;
        }
        while ( *temp_ptr == ' ' || *temp_ptr == '\t' );

        // Remove leading white space from the value part
        white_len = strspn( val_ptr, " \t" );
        val_ptr += white_len;

        val = (char *)malloc( strlen(val_ptr)+1 );
        strncpy( val, val_ptr, strlen(val_ptr)+1 );

        if ( strcasecmp( name_ptr, "ZM_DB_HOST" ) == 0 ) 
            g_ZMConfig->DBHost = val;
        else if ( strcasecmp( name_ptr, "ZM_DB_NAME" ) == 0 ) 
            g_ZMConfig->DBDatabase = val;
        else if ( strcasecmp( name_ptr, "ZM_DB_USER" ) == 0 ) 
            g_ZMConfig->DBUser = val;
        else if ( strcasecmp( name_ptr, "ZM_DB_PASS" ) == 0 ) 
            g_ZMConfig->DBPassword = val;
        else if ( strcasecmp( name_ptr, "ZM_WEB_USER" ) == 0 ) 
            g_ZMConfig->webUser = val;
        else if ( strcasecmp( name_ptr, "ZM_WEB_GROUP" ) == 0 ) 
            g_ZMConfig->webGroup = val;
        else if ( strcasecmp( name_ptr, "ZM_PATH_WEB" ) == 0 ) 
            g_ZMConfig->webPath = val;
        else if ( strcasecmp( name_ptr, "ZM_PATH_BIN" ) == 0 ) 
            g_ZMConfig->binPath = val;
        else
        {
            // We ignore this now as there may be more parameters than we're
            // bothered about
        }
    }
    fclose(cfg);

    VERBOSE(VB_IMPORTANT, "ZM config loaded OK");

    return true;
}

bool openZMDatabase(void)
{
    // connect to the zm database
    g_ZMDatabase = QSqlDatabase::addDatabase("QMYSQL3", "zm");
    g_ZMDatabase->setDatabaseName(g_ZMConfig->DBDatabase);
    g_ZMDatabase->setUserName(g_ZMConfig->DBUser);
    g_ZMDatabase->setPassword(g_ZMConfig->DBPassword);
    g_ZMDatabase->setHostName(g_ZMConfig->DBHost);

    if (!g_ZMDatabase->open())
    {
        VERBOSE(VB_IMPORTANT, "MythZoneMinder: Failed to opened ZM database");
        return false;
    }

    VERBOSE(VB_IMPORTANT, "ZM database opened OK");
    return true;
}

void deleteEvent(int eventID)
{
    QString sql;
    QSqlQuery query(g_ZMDatabase);

    sql = "DELETE FROM Events WHERE Id = :ID";
    query.prepare(sql);
    query.bindValue(":ID", eventID);
    query.exec();

    sql = "DELETE FROM Stats WHERE Id = :ID";
    query.prepare(sql);
    query.bindValue(":ID", eventID);
    query.exec();

    sql = "DELETE FROM Frames WHERE Id = :ID";
    query.prepare(sql);
    query.bindValue(":ID", eventID);
    query.exec();

    // remove all stored frames for this event
    QString eventDir = "/var/www/localhost/htdocs/zoneminder/events";
    QString cmd = QString("rm -rf %1/*/%2").arg(eventDir).arg(eventID);
    cout << cmd << endl;
    system(cmd);
}
