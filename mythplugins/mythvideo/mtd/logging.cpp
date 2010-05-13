/*
    logging.cpp

    (c) 2003 Thor Sigvaldason and Isaac Richards
    Part of the mythTV project

    Methods for logging object of the myth transcoding daemon

*/
#include "logging.h"

#include <unistd.h>
#include <cstdlib>
#include <QDateTime>

#include "compat.h"
#include <mythcontext.h>

#define LOC      QString("MTDLogger: ")
#define LOC_WARN QString("MTDLogger, Warning: ")
#define LOC_ERR  QString("MTDLogger, Error: ")

MTDLogger::MTDLogger(bool log_stdout) :
    QObject(),
    log_to_stdout(log_stdout)
{
}

bool MTDLogger::Init(void)
{
    QString logfile_name = gCoreContext->GetSetting("DVDRipLocation");
    if (logfile_name.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "You do not have a DVD rip directory set. Run Setup.");
        return false;
    }

    logfile_name += "/mtd.log";

    if (!log_to_stdout)
    {
        logging_file.setFileName(logfile_name);
        if (!logging_file.open(QIODevice::WriteOnly))
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "\n\t\t\t" +
                    QString("Could not open logfile '%1' for writing")
                    .arg(logfile_name) +
                    "\n\t\t\tSending log output to stdout instead");
            log_to_stdout = true;
        }
    }

    return true;
}

void MTDLogger::addEntry(const QString &log_entry)
{
    writeStampedString(log_entry);
}

void MTDLogger::addStartup()
{
    char hostname[1024];
    QString startup_message = "mtd started at " +
        QDateTime(QDateTime::currentDateTime()).toString();
    writeString(startup_message);
    
    gethostname(hostname, 1024);
    
    startup_message = hostname;
    startup_message.prepend("mtd is running on a host called ");
                    
    writeString(startup_message);
    writeStampedString("Waiting for connections/jobs");    
}

void MTDLogger::addShutdown()
{
    QString shutdown_message = "mtd shutting down at " +
      QDateTime(QDateTime::currentDateTime()).toString();
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
    if (log_to_stdout)
    {
        VERBOSE(VB_IMPORTANT, log_entry);
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
