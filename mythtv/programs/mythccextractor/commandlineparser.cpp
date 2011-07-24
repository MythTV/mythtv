#include "commandlineparser.h"
#include "mythcorecontext.h"

MythCCExtractorCommandLineParser::MythCCExtractorCommandLineParser() :
    MythCommandLineParser(MYTH_APPNAME_MYTHCCEXTRACTOR)
{
    LoadArguments();
}

void MythCCExtractorCommandLineParser::LoadArguments(void)
{
    addHelp();
    addSettingsOverride();
    addVersion();

    add(QStringList( QStringList() << "-v" << "--verbose" ), "verbose",
            "none",
            "Specify log filtering. Use '-v help' for level info.", "");
    add("-V", "verboseint", 0U, "",
            "This option is intended for internal use only.\n"
            "This option takes an unsigned value corresponding "
            "to the bitwise log verbosity operator.");
    add(QStringList( QStringList() << "-l" << "--logfile" << "--logpath" ), 
            "logpath", "",
            "Writes logging messages to a file at logpath.\n"
            "If a directory is given, a logfile will be created in that "
            "directory with a filename of applicationName.date.pid.log.\n"
            "If a full filename is given, that file will be used.\n"
            "This is typically used in combination with --daemon, and if used "
            "in combination with --pidfile, this can be used with log "
            "rotaters, using the HUP call to inform MythTV to reload the "
            "file (currently disabled).", "");
    add(QStringList( QStringList() << "-q" << "--quiet"), "quiet", 0,
            "Don't log to the console (-q).  Don't log anywhere (-q -q)", "");
    add("--loglevel", "loglevel", "err", 
            "Set the logging level.  All log messages at lower levels will be "
            "discarded.\n"
            "In descending order: emerg, alert, crit, err, warning, notice, "
            "info, debug\ndefaults to err", "");
    add("--syslog", "syslog", "none", 
            "Set the syslog logging facility.\nSet to \"none\" to disable, "
            "defaults to none", "");
    add("--nodblog", "nodblog", false, "Disable database logging.", "");

    add(QStringList( QStringList() << "-i" << "--infile" ), "inputfile", "",
            "input file", "");
}

QString MythCCExtractorCommandLineParser::GetHelpHeader(void) const
{
    return
        "This is a command for generating srt files for\n"
        "DVB and ATSC recordings.";
}
