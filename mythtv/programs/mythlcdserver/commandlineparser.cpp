#include <QString>

#include "mythcorecontext.h"
#include "commandlineparser.h"

MythLCDServerCommandLineParser::MythLCDServerCommandLineParser() :
    MythCommandLineParser(MYTH_APPNAME_MYTHLCDSERVER)
{ LoadArguments(); }

void MythLCDServerCommandLineParser::LoadArguments(void)
{
    addHelp();
    addVersion();
    addDaemon();
    addLogging();
    //addPIDFile();

    add(QStringList( QStringList() << "-p" << "--port" ), "port", 6545, "listen port",
            "This is the port MythLCDServer will listen on for events.");
    add(QStringList( QStringList() << "-m" << "--startupmessage" ), "message", "",
            "Message to display on startup.", "");
    add(QStringList( QStringList() << "-t" << "--messagetime"), "messagetime", 30,
            "Message display duration (in seconds)", "");
    add(QStringList( QStringList() << "-x" << "--debuglevel" ), "debug", 0,
            "debug verbosity", "Control debugging verbosity, values from 0-10");

    add("--nodaemon", "nodaemon", false, "", "")
        ->SetRemoved("This is the default behavior. No need for an argument.",
                     "0.25");
}

