/*
	mfdctl.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	tiny little program to start, stop, restart, 
	reload the mfd

*/

#include <qapplication.h>
#include <qfile.h>

#include "mfdctl.h"

int start_mfd(int port, int verbosity)
{
    QString mfd_file_location = QString(PREFIX) + "/bin/mfd";
    QFile mfd_file(mfd_file_location);
    if(mfd_file.exists())
    {
        mfd_file_location.append(" -d ");
        if(port != -1)
        {
           QString port_string = QString(" -p %1 ").arg(port);
           mfd_file_location.append(port_string);
        }
        if(verbosity != -1)
        {
           QString verb_string = QString(" -l %1 ").arg(verbosity);
           mfd_file_location.append(verb_string);
        }
        mfd_file_location.append(" &");
        if(system(mfd_file_location) < 0)
        {
            cerr << "problem running your mfd executable like this: "
                 << mfd_file_location
                 << endl;
                 return -1;
        }
        else
        {
            return 0;
        }
    }
    else
    {
        cerr << "Your mfd file does not seem to exist at: " 
             << mfd_file_location
             << endl;
        return -1;
    }
    return -1;
}


int main(int argc, char **argv)
{
    
    QApplication a(argc, argv, false);

    int  special_port = -1;
    QString host = "localhost";
    QString what_to_do = "";
    int  logging_verbosity = -1;

    //
    //  Check command line arguments
    //
    
    for(int argpos = 1; argpos < a.argc(); ++argpos)
    {
        
        if (!strcmp(a.argv()[argpos],"-p") ||
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
                    cerr << "mfdctl: Bad port number" << endl;
                    return -1;
                }
            } 
            else 
            {
                cerr << "mfdctl: Missing argument to -p/--port option" << endl;
                return -1;
            }
        }
        else if (!strcmp(a.argv()[argpos],"-h") ||
            !strcmp(a.argv()[argpos],"--host"))
        {
            if (a.argc() > argpos)
            {
                host = a.argv()[argpos+1];
                ++argpos;
            } 
            else 
            {
                cerr << "which host?" << endl;
            }
        }
        else if (!strcmp(a.argv()[argpos],"-l") ||
            !strcmp(a.argv()[argpos],"--logging-verbosity"))
        {
            if (a.argc() > argpos)
            {
                QString log_verb = a.argv()[argpos+1];
                ++argpos;
                logging_verbosity = log_verb.toInt();
                if(logging_verbosity < 1 ||
                   logging_verbosity > 10)
                {
                    cerr << "mfdftl: Bad logging verbosity" << endl;
                    return -1;
                }
            } 
            else 
            {
                cerr << "which host?" << endl;
            }
        }
        else if (!strcmp(a.argv()[argpos],"start") ||
                 !strcmp(a.argv()[argpos],"stop")  ||
                 !strcmp(a.argv()[argpos],"reload")  ||
                 !strcmp(a.argv()[argpos],"restart") 
                )
        {
            what_to_do = a.argv()[argpos];
        }
        else
        {
            cerr << "Invalid argument: " << a.argv()[argpos] << endl <<
                    "Valid options are: " << endl <<
                    "-p or --port number               A port number to connect to (default is 2342) " << endl <<
                    "-h or --host                      A host to connect to (default is localhost) " << endl <<
                    "-l or --logging-verbosity         Logging verbosity to run mfd at when using start " << endl <<
                    "start | stop | reload | restart   What to do" << endl;
            return -1;
        }
    }

    if(what_to_do.length() < 1)
    {
        cerr << "you don't want to do anything, so I won't" << endl;
        return -1;
    }
    
    
    //
    //  Figure out what we're supposed to do
    //

    if(what_to_do == "start")
    {
        if(host != "localhost")
        {
            cerr << "I can't launch the mfd on another machine." << endl;
            return -1;
        }
        return start_mfd(special_port, logging_verbosity);
    }
    else if(what_to_do == "restart")
    {
        if(host != "localhost")
        {
            cerr << "I can't restart the mfd on another machine." << endl;
            return -1;
        }
        MFDCTL *mfdctl = new MFDCTL(special_port, logging_verbosity, host, "stop");
        while(mfdctl->keepRunning())
        {
            a.processEvents();
        }
        if(mfdctl->anyProblems())
        {
            cerr << "Could not do the stop part of a restart. mfd is probably not running" << endl;
            return -1;
        }
        else
        {   
            return start_mfd(special_port, logging_verbosity);
        }
    }
    else if(what_to_do == "stop")
    {
        MFDCTL *mfdctl = new MFDCTL(special_port, logging_verbosity, host, "stop");
        while(mfdctl->keepRunning())
        {
            a.processEvents();
        }
        if(mfdctl->anyProblems())
        {
            cerr << "Could not stop an mfd. Probably isn't running." << endl;
            return -1;
        }
        return 0;
    }
            
    else if(what_to_do == "reload")
    {
        MFDCTL *mfdctl = new MFDCTL(special_port, logging_verbosity, host, "reload");
        while(mfdctl->keepRunning())
        {
            a.processEvents();
        }
        if(mfdctl->anyProblems())
        {
            cerr << "Some problem trying to reload." << endl;
            return -1;
        }
        return 0;
    }    

    return 0;
}


