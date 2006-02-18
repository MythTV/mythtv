/*
	logging.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Methods for logging object of the myth transcoding daemon

*/
#include "logging.h"

#include <unistd.h>
#include <qdatetime.h>

#include <mythtv/mythcontext.h>

MTDLogger::MTDLogger(bool log_stdout)
          :QObject()
{
    log_to_stdout = log_stdout;
    
    //
    //  Where to log
    //

    QString logfile_name = gContext->GetSetting("DVDRipLocation");
    if(logfile_name.length() < 1)
    {
        cerr << "MTDLogger.o: You do not have a DVD rip directory set. Run Setup." << endl;
        exit(0);
    }    
    logfile_name.append("/mtd.log");
    if(!log_to_stdout)
    {
        logging_file.setName(logfile_name);
        if(!logging_file.open(IO_WriteOnly))
        {
            cerr << "MTDLogger.o: Problem opening logfile. Does this look openable to you: " << logfile_name << endl;
            exit(0);
        }
    }
}

void MTDLogger::addEntry(const QString &log_entry)
{
    writeStampedString(log_entry);
}

void MTDLogger::addStartup()
{
    char hostname[1024];
    QString startup_message = "mtd started at " 
                            + QDateTime(QDateTime::currentDateTime()).toString();
    writeString(startup_message);
    
    gethostname(hostname, 1024);
    
    startup_message = hostname;
    startup_message.prepend("mtd is running on a host called ");
                    
    writeString(startup_message);
    writeStampedString("Waiting for connections/jobs");    
}

void MTDLogger::addShutdown()
{
    QString shutdown_message = "mtd shutting down at " 
                            + QDateTime(QDateTime::currentDateTime()).toString();
    writeString(shutdown_message);
}

void MTDLogger::socketOpened()
{
    writeStampedString("a client socket has been opened");
}

void MTDLogger::socketClosed()
{
    writeStampedString("a client socket has been closed");
}

void MTDLogger::writeString(const QString &log_entry)
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

void MTDLogger::writeStampedString(const QString &log_entry)
{
    QString stamped_log_entry = log_entry;
    stamped_log_entry.prepend(": ");
    stamped_log_entry.prepend(QTime(QTime::currentTime()).toString());
    writeString(stamped_log_entry);
}

MTDLogger::~MTDLogger()
{
    logging_file.close();
}

