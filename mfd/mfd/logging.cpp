/*
	logging.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Methods for logging object of the myth transcoding daemon

*/
#include "logging.h"

#include <qdatetime.h>

#include <mythtv/mythcontext.h>

MFDLogger::MFDLogger(bool log_stdout, int verbosity_level)
          :QObject()
{
    //
    //  startup defaults
    //
    
    log_to_stdout = log_stdout;
    verbosity = verbosity_level;
    if(verbosity < 1 ||
       verbosity > 10)
    {
        verbosity = 1;
    }
    
    //
    //  Where to log
    //

    if(!log_to_stdout)
    {
        QString fallback_log_location = QString(PREFIX) + "/share/mythtv/";
        QString logfile_name = gContext->GetSetting("MFDLogLocation", fallback_log_location);
        logfile_name.append("/mfd.log");
        logging_file.setName(logfile_name);
        if(!logging_file.open(IO_WriteOnly))
        {
            cerr << "logging.o: Problem opening logfile. Does this look openable to you: " << logfile_name << endl;
            cerr << "logging.o: Reverting to logging on stdout. " << endl;
            log_to_stdout = true;
        }
    }
}

void MFDLogger::addEntry(const QString &log_entry)
{
    addEntry(1, log_entry);
}

void MFDLogger::addEntry(int verbosity_level, const QString &log_entry)
{
    writeStampedString(verbosity_level, log_entry);
}

void MFDLogger::addStartup()
{
    char hostname[1024];
    QString startup_message = "######################################################################";
    writeString(startup_message);

    startup_message = "mfd started at " + QDateTime(QDateTime::currentDateTime()).toString();
    writeString(startup_message);
    
    gethostname(hostname, 1024);
    
    startup_message = hostname;
    startup_message.prepend("mfd is running on a host called ");
                    
    writeString(startup_message);

    startup_message = "######################################################################";
    writeString(startup_message);
}

void MFDLogger::addShutdown()
{
    QString shutdown_message = "######################################################################";
    writeString(shutdown_message);

    shutdown_message = "mfd shutting down at " + QDateTime(QDateTime::currentDateTime()).toString();
    writeString(shutdown_message);

    shutdown_message = "######################################################################";
    writeString(shutdown_message);

    shutdown_message = "";
    writeString(shutdown_message);
}

void MFDLogger::writeString(const QString &log_entry)
{
    if(log_to_stdout)
    {
        cout << log_entry << endl;
    }
    else
    {
        QTextStream stream(&logging_file);
        stream << log_entry << endl << flush;
    }
}

void MFDLogger::writeStampedString(int verbosity_level, const QString &log_entry)
{
    if(verbosity_level <= verbosity)
    {
        QString stamped_log_entry = log_entry;
        stamped_log_entry.prepend(": ");
        stamped_log_entry.prepend(QTime(QTime::currentTime()).toString());
        writeString(stamped_log_entry);
    }
}

MFDLogger::~MFDLogger()
{
    if(!log_to_stdout)
    {
        logging_file.close();
    }
}

