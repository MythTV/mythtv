/*
	main.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Starting point for the myth transcoding daemon

*/

#include <qapplication.h>
#include <qsqldatabase.h>
#include <qfile.h>
#include <qtextstream.h>

#include <iostream>
using namespace std;
#include <unistd.h>

#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>
#include <mythtv/langsettings.h>

#include "../mythdvd/config.h"
#include "../mythdvd/dbcheck.h"
#if TRANSCODE_SUPPORT
#include "mtd.h"
#endif

int main(int argc, char **argv)
{
#if TRANSCODE_SUPPORT
    QApplication a(argc, argv, false);
    bool daemon_mode = false;
    int  special_port = -1;    
    //
    //  Check command line arguments
    //
    
    for(int argpos = 1; argpos < a.argc(); ++argpos)
    {
        
        if (!strcmp(a.argv()[argpos],"-d") ||
            !strcmp(a.argv()[argpos],"--daemon"))
        {
            daemon_mode = true;
        }
        else if (!strcmp(a.argv()[argpos],"-n") ||
            !strcmp(a.argv()[argpos],"--nodaemon"))
        {
            daemon_mode = false;
        }
        else if (!strcmp(a.argv()[argpos],"-p") ||
            !strcmp(a.argv()[argpos],"--port"))
        {
            if (a.argc() > argpos)
            {
                QString port_number = a.argv()[argpos+1];
                ++argpos;
                special_port = port_number.toInt();
                if(special_port < 1 ||
                   special_port > 65534)
                {
                    cerr << "mtd: Bad port number" << endl;
                    return -1;
                }
            } 
            else 
            {
                cerr << "mtd: Missing argument to -p/--port option\n";
                return -1;
            }
        }
        else
        {
            cerr << "Invalid argument: " << a.argv()[argpos] << endl <<
                    "Valid options are: " << endl <<
                    "-p or --port number  A port number to listen on (default is 2442) " << endl <<
                    "-d or --daemon       Runs mtd as a daemon " << endl <<
                    "-n or --nodaemon     Does not run mtd as a daemon (default)" << endl;
            return -1;
        }
    }
    
    //
    //  Switch to daemon mode?
    //
    
    if(daemon_mode)
    {
        if(daemon(0, 1) < 0)
        {
            cerr << "mtd: Failed to run as a daemon. Bailing out." << endl ;
            return -1;
        }
        cout << endl;
    }

    //
    //  Get the Myth context and db hooks
    //    
    
    gContext = NULL;
    gContext = new MythContext(MYTH_BINARY_VERSION);
    gContext->Init(false);

    if (!MSqlQuery::testDBConnection())
    {
        cerr << "mtd: Couldn't open database. I go away now. " << endl;
        return -1;
    }

    UpgradeDVDDatabaseSchema();
 
    //
    //  Figure out port to listen on
    //

    int assigned_port = gContext->GetNumSetting("MTDPort", 2442);
    if(special_port > 0)
    {
        assigned_port = special_port;
    }



    //
    //  Where to log
    //
   
    bool log_stdout = false;
    if(gContext->GetNumSetting("MTDLogFlag", 0))
    {
        log_stdout = true;
    } 
    if(!daemon_mode)
    {
        log_stdout = true;
    }

    //
    //  Nice ourself
    //
   
    LanguageSettings::load("mythdvd");
    
    new MTD(db, assigned_port, log_stdout);
    
    a.exec();
                                
    delete db;
    delete gContext;
    return 0;
#else
    argc = argc;
    *argv = *argv; // -Wall
    cerr << "main.o: mtd was built without transcode support. It won't do anything." << endl;
    return -1;
#endif
}

