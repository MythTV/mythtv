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
    cout << "   --file <filename>" << endl;
    cout << "       Import data from the given file." << endl << endl;
    cout << "   --help" << endl;
    cout << "       This text." << endl << endl;
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv, false);
    QString filename;
    int argpos = 1;
    
    while (argpos < a.argc())
    {
        if (!strcmp(a.argv()[argpos], "--file"))
        {
            filename = a.argv()[++argpos];
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

            
    
    
    
    gContext = NULL;
    gContext = new MythContext(MYTH_BINARY_VERSION);
    if(!gContext->Init(false))
    {
        VERBOSE(VB_IMPORTANT, "Failed to init MythContext, exiting.");
        return -1;
    }
    
    if (!filename.isEmpty() )
    {
        TMSMovieDirect dd;
        dd.importFile(filename);
        dd.store();
    }
    else
    {
        showUsage();
    }
    
    return 0;
}




// EOF
