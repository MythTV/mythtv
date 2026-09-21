#include <QString>
#include <iostream>

#include "mythexternrecorder_commandlineparser.h"

#include "libmythbase/mythappname.h"
#include "libmythbase/mythlogging.h"

MythExternRecorderCommandLineParser::MythExternRecorderCommandLineParser() :
    MythCommandLineParser(MYTH_APPNAME_MYTHEXTERNRECORDER)
{
    MythExternRecorderCommandLineParser::LoadArguments();
}

QString MythExternRecorderCommandLineParser::GetHelpHeader(void) const
{
    return "mythexternrecorder is a go-between app which interfaces "
        "between a recording device and mythbackend.";
}

void MythExternRecorderCommandLineParser::LoadArguments(void)
{
    allowArgs();
    addHelp();
    addSettingsOverride();
    addVersion();
    addLogging();

    add("--conf", "conf", "", "Path to a configuration file in TOML format.", "")
        ->SetGroup("ExternalRecorder");

    add("--inputid", "inputid", "", "MythTV input this app is attached to.", "")
        ->SetGroup("ExternalRecorder");
}

int MythExternRecorderCommandLineParser::SetupLogging(const QString& logfile)
{
    // Setup the defaults
    verboseString = "";
    verboseMask   = 0;
    verboseArgParse("general");

    if (toBool("verbose"))
    {
        int err = verboseArgParse(toString("verbose"));
        if (err != 0)
            return err;
    }
    else if (toBool("verboseint"))
    {
        verboseMask = static_cast<uint64_t>(toLongLong("verboseint"));
    }

    verboseMask |= VB_STDIO|VB_FLUSH;

    bool loglong = toBool("loglong");

    int facility = GetSyslogFacility();
    LogLevel_t level = GetLogLevel();
    if (level == LOG_UNKNOWN)
        return -1;

    int quiet = toInt("quiet");
    bool propagate = !logfile.isEmpty();

    logStart(logfile, false, quiet, facility, level, propagate, loglong);
    qInstallMessageHandler([](QtMsgType /*unused*/, const QMessageLogContext& /*unused*/, const QString &Msg)
        { LOG(VB_GENERAL, LOG_INFO, "Qt: " + Msg); });

    return 0;
}
