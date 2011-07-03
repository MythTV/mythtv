
using namespace std;

#include <QString>

#include "mythcorecontext.h"
#include "commandlineparser.h"

MythMessageCommandLineParser::MythMessageCommandLineParser() :
    MythCommandLineParser(MYTH_APPNAME_MYTHMESSAGE)
{ LoadArguments(); }

void MythMessageCommandLineParser::LoadArguments(void)
{
    addHelp();
    addVersion();
    allowExtras();

    add("--udpport", "port", 6948, "(optional) UDP Port to send to", "");
    add("--bcastaddr", "addr", "127.0.0.1",
            "(optional) IP address to send to", "");
    add("--print-template", "printtemplate", false,
            "Print the template to be sent to the frontend", "");
    add(QStringList( QStringList() << "-v" << "--verbose" ), "verbose",
            false, "Turn on verbose debugging", "");
}

