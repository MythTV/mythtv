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
#include "ddmovie.h"

void showUsage()
{
    cout << "Usage:" << endl;
    cout << "   --file <string>" << endl;
    cout << "       Import data from the given file." << endl << endl;
    cout << "   --user <string>" << endl;
    cout << "       The user ID to authenticate on the web server with." << endl << endl;
    cout << "   --zip <string>" << endl;
    cout << "       The zipcode that's the center of the radius." << endl << endl;
    cout << "   --radius <number>" << endl;
    cout << "       The how far out from <zipcode> to look for theaters. Defaults to 25 miles." << endl << endl;
    cout << "   --help" << endl;
    cout << "       This text." << endl << endl;
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv, false);
    
    
    gContext = new MythContext(MYTH_BINARY_VERSION);
    if(!gContext->Init(false))
    {
        VERBOSE(VB_IMPORTANT, "Failed to init MythContext, exiting.");
        return -1;
    }
    
    QString filename;
    QString user = gContext->GetSetting("MMTUser");
    QString passwd = gContext->GetSetting("MMTPass");
    QString zip = gContext->GetSetting("PostalCode");
    double radius = gContext->GetSetting("Radius", "25").toDouble();
    
    int argpos = 1;
    
    while (argpos < a.argc())
    {
        if (!strcmp(a.argv()[argpos], "--file"))
        {
            filename = a.argv()[++argpos];
        }
        else if (!strcmp(a.argv()[argpos], "--user"))
        {
            user = a.argv()[++argpos];
        }
        else if (!strcmp(a.argv()[argpos], "--zip"))
        {
            zip = a.argv()[++argpos];
        }
        else if (!strcmp(a.argv()[argpos], "--radius"))
        {
            radius = QString(a.argv()[++argpos]).toDouble();
        }
        else if (!strcmp(a.argv()[argpos], "--pass"))
        {
            passwd = a.argv()[++argpos];
        }
        else if (!strcmp(a.argv()[argpos], "-h") ||
                 !strcmp(a.argv()[argpos], "--help"))
        {
            showUsage();
            return -1;
        }
        else
        {
            fprintf(stderr, "illegal option: '%s' (use --help)\n",
                    a.argv()[argpos]);
            return -1;
        }
        
        ++argpos;
    }

            
    
    
    
    
    
    TMSMovieDirect dd;
    if (!filename.isEmpty() )
    {
        dd.importFile(filename);
    }
    else if (!user.isEmpty() && !passwd.isEmpty() && !zip.isEmpty())
    {
        dd.importData( user, passwd, zip, radius );
    }
    else
    {
        showUsage();
        return 0;
    }
    
    if( dd.store() )
    {   
        gContext->SaveSetting("MMT-Last-Fetch", QDate::currentDate().toString(Qt::ISODate));
    }
    return 0;
}




// EOF
