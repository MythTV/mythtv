/*
	main.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Starting point for the myth transcoding daemon

*/

#include <signal.h>

#include <qapplication.h>
#include <qfile.h>
#include <qtextstream.h>

#include <iostream>
using namespace std;
#include <unistd.h>

#include "mfd.h"
#include "settings.h"

#include "dbcheck.h"

//
//  This is a global object that anyone can ask for a setting value. If
//  everything was compiled with myth lib support, it basicly wraps calls to
//  gContext
//

Settings *mfdContext;

//
//  This is the center of the universe
//

MFD *the_mfd;

//
//  main() just figures out a few
//  resources (e.g. the database) and
//  some options (e.g. daemon mode) and
//  then launches the MFD object.
//


int main(int argc, char **argv)
{

    //
    //  Set up to ignore all signals. A seperate thread (signalthread) will
    //  handle signals (ie. user hitting CTRL-C, kill pid, etc) and shut
    //  things down cleanly
    //
    
    sigset_t signal_mask;
    sigemptyset(&signal_mask);
    sigprocmask(0, 0, &signal_mask);
    sigaddset(&signal_mask, SIGINT);
    sigaddset(&signal_mask, SIGTERM);
    sigprocmask(SIG_SETMASK, &signal_mask, 0);

    QApplication a(argc, argv, false);

    bool daemon_mode = false;
    int  special_port = -1;    
    int  logging_verbosity = 1;

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
                    cerr << "mfd: Bad port number" << endl;
                    return -1;
                }
            } 
            else 
            {
                cerr << "mfd: Missing argument to -p/--port option\n";
                return -1;
            }
        }
        else if (!strcmp(a.argv()[argpos],"-l") ||
            !strcmp(a.argv()[argpos],"--logging-verbosity"))
        {
            if (a.argc() > argpos)
            {
                QString log_verbose = a.argv()[argpos+1];
                ++argpos;
                logging_verbosity = log_verbose.toInt();
                if(logging_verbosity < 1 ||
                   logging_verbosity > 10)
                {
                    cerr << "mfd: Bad logging verbosity setting, setting to 1" << endl;
                    logging_verbosity = 1;
                }
            } 
            else 
            {
                cerr << "mfd: Missing argument to -l/--logging-verbosity option. Setting to 1.\n";
                logging_verbosity = 1;
            }
        }
        else
        {
            cerr << "Invalid argument: " << a.argv()[argpos] << endl <<
                    "Valid options are: " << endl <<
                    "-p or --port number               A port number to listen on (default is 2342) " << endl <<
                    "-l or --logging-verbosity number  Amount of info to log (1 = least, 10 = most) " << endl <<
                    "-d or --daemon                    Runs mfd as a daemon " << endl <<
                    "-n or --nodaemon                  Does not run mfd as a daemon " << endl;
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
            cerr << "mfd: Failed to run as a daemon. Bailing out." << endl ;
            return -1;
        }
    }

    //
    //  Create an object to get settings from
    //
    
    mfdContext = new Settings();

    //
    //  Figure out port to listen on
    //

    int assigned_port = 2342;
    assigned_port = gContext->GetNumSetting("MFDPort", 2342);

    if(special_port > 0)
    {
        assigned_port = special_port;
    }

    if(assigned_port < 0)
    {
        cerr << "cannot start mfd with port number of " << assigned_port << endl;
        return 0;
    }


    //
    //  Where to log
    //
   
    bool log_stdout = false;
    if(mfdContext->getNumSetting("MFDLogFlag", 0))
    {
        log_stdout = true;
    }

    if(!daemon_mode)
    {
        log_stdout = true;
    }

    //
    //  Make sure database are all up to whack
    //

    UpgradeMusicDatabaseSchema();

    the_mfd = new MFD((uint) assigned_port, log_stdout, logging_verbosity);
    a.exec();
                                
    if(mfdContext)
    {
        delete mfdContext;
    }

    return 0;
}

