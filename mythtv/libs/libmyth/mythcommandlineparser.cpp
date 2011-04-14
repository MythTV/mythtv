#include <iostream>
using namespace std;

#include <QFile>
#include <QFileInfo>
#include <QSize>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <QString>
#include <QCoreApplication>

#include "mythcommandlineparser.h"
#include "exitcodes.h"
#include "mythconfig.h"
#include "mythcontext.h"
#include "mythverbose.h"
#include "mythversion.h"

typedef struct helptmp {
    QString left;
    QString right;
    QStringList arglist;
    CommandLineArg arg;
} HelpTmp;

MythCommandLineParser::MythCommandLineParser(QString appname) :
    m_appname(appname), m_allowExtras(false), m_overridesImported(false)
{
    LoadArguments();
}

void MythCommandLineParser::add(QStringList arglist, QString name,
                                QVariant::Type type, QVariant def,
                                QString help, QString longhelp)
{
    CommandLineArg arg;
    arg.name     = name;
    arg.type     = type;
    arg.def      = def;
    arg.help     = help;
    arg.longhelp = longhelp;

    QStringList::const_iterator i;
    for (i = arglist.begin(); i != arglist.end(); ++i)
    {
        if (!m_registeredArgs.contains(*i))
        {
//            cerr << "Adding " << (*i).toLocal8Bit().constData()
//                 << " as taking type '" << QVariant::typeToName(type)
//                 << "'" << endl;
            m_registeredArgs.insert(*i, arg);
        }
    }

    if (!m_defaults.contains(arg.name))
        m_defaults[arg.name] = arg.def;
}

void MythCommandLineParser::PrintVersion(void)
{
    cout << "Please attach all output as a file in bug reports." << endl;
    cout << "MythTV Version : " << MYTH_SOURCE_VERSION << endl;
    cout << "MythTV Branch : " << MYTH_SOURCE_PATH << endl;
    cout << "Network Protocol : " << MYTH_PROTO_VERSION << endl;
    cout << "Library API : " << MYTH_BINARY_VERSION << endl;
    cout << "QT Version : " << QT_VERSION_STR << endl;
#ifdef MYTH_BUILD_CONFIG
    cout << "Options compiled in:" <<endl;
    cout << MYTH_BUILD_CONFIG << endl;
#endif
}

void MythCommandLineParser::PrintHelp(void)
{
    QString help = GetHelpString(true);
    cerr << help.toLocal8Bit().constData();
}

QString MythCommandLineParser::GetHelpString(bool with_header) const
{
    QString helpstr;
    QTextStream msg(&helpstr, QIODevice::WriteOnly);

    if (with_header)
    {
        QString versionStr = QString("%1 version: %2 [%3] www.mythtv.org")
            .arg(m_appname).arg(MYTH_SOURCE_PATH).arg(MYTH_SOURCE_VERSION);
        msg << versionStr << endl;
    }

    if (toString("showhelp").isEmpty())
    {
        // build generic help text
        if (with_header)
            msg << "Valid options are: " << endl;

        QMap<QString, HelpTmp> argmap;
        QString argname;
        HelpTmp help;

        QMap<QString, CommandLineArg>::const_iterator i;
        for (i = m_registeredArgs.begin(); i != m_registeredArgs.end(); ++i)
        {
            if (i.value().help.isEmpty())
                // ignore any arguments with no help text
                continue;

            argname = i.value().name;
            if (argmap.contains(argname))
                argmap[argname].arglist << i.key();
            else
            {
                help.arglist = QStringList(i.key());
                help.arg = i.value();
                argmap[argname] = help;
            }
        }

        int len, maxlen = 0;
        QMap<QString, HelpTmp>::iterator i2;
        for (i2 = argmap.begin(); i2 != argmap.end(); ++i2)
        {
            (*i2).left = (*i2).arglist.join(" OR ");
            (*i2).right = (*i2).arg.help;
            len = (*i2).left.length();
            maxlen = max(len, maxlen);
        }

        maxlen += 4;
        QString pad;
        pad.fill(' ', maxlen);
        QStringList rlist;
        QStringList::const_iterator i3;

        for (i2 = argmap.begin(); i2 != argmap.end(); ++i2)
        {
            msg << (*i2).left.leftJustified(maxlen, ' ');

            rlist = (*i2).right.split('\n');
            msg << rlist[0] << endl;

            for (i3 = rlist.begin() + 1; i3 != rlist.end(); ++i3)
                msg << pad << *i3 << endl;
        }
    }
    else
    {
        // build help for a specific argument
        QString optstr = "-" + toString("showhelp");
        if (!m_registeredArgs.contains(optstr))
        {
            optstr = "-" + optstr;
            if (!m_registeredArgs.contains(optstr))
                return QString("Could not find option matching '%s'")
                            .arg(toString("showhelp"));
        }

        if (with_header)
            msg << "Option:      " << optstr << endl << endl;

        // pull option information, and find aliased options
        CommandLineArg option = m_registeredArgs[optstr];
        QMap<QString, CommandLineArg>::const_iterator cmi;
        QStringList aliases;
        for (cmi = m_registeredArgs.begin(); cmi != m_registeredArgs.end(); ++cmi)
        {
            if (cmi.key() == optstr)
                continue;

            if (cmi.value().name == option.name)
                aliases << cmi.key();
        }

        QStringList::const_iterator sli;
        if (!aliases.isEmpty())
        {
            sli = aliases.begin();
            msg <<     "Aliases:     " << *sli << endl;
            while (++sli != aliases.end())
                msg << "             " << *sli << endl;
        }

        msg << "Type:        " << QVariant::typeToName(option.type) << endl;
        if (option.def.canConvert(QVariant::String))
            msg << "Default:     " << option.def.toString() << endl;

        QStringList help;
        if (!option.longhelp.isEmpty())
            help = option.longhelp.split("\n");
        else
            help = option.help.split("\n");

        sli = help.begin();
        msg << "Description: " << *sli << endl;
        while (++sli != help.end())
            msg << "             " << *sli << endl;
    }

    msg.flush();
    return helpstr;
}

bool MythCommandLineParser::Parse(int argc, const char * const * argv)
{
    bool processed;
    QString opt, val, name;
    QVariant::Type type;

    QMap<QString, CommandLineArg>::const_iterator i;
    for (int argpos = 1; argpos < argc; ++argpos)
    {

        opt = QString::fromLocal8Bit(argv[argpos]);
//        cerr << "Processing: " << argv[argpos] << endl;
        if (!opt.startsWith("-"))
        {
            m_remainingArgs << opt;
            continue;
        }
        else
        {
            if (!m_remainingArgs.empty())
            {
                cerr << "Command line arguments received out of sequence"
                     << endl;
                return false;
            }
        }

        processed = false;

        for (i = m_registeredArgs.begin(); i != m_registeredArgs.end(); ++i)
        {
            if (opt == i.key())
            {
                processed = true;
                name = i.value().name;
                type = i.value().type;

                if (type == QVariant::Bool)
                {
//                    cerr << "  bool set to inverse of default" << endl;
                    // if defined, a bool is set to the opposite of default
                    m_parsed[name] = QVariant(!(i.value().def.toBool()));
                    break;
                }

                // try to pull value
                if (++argpos == argc)
                {
                    // end of list
                    if (type == QVariant::String)
                    {
//                        cerr << "  string set to default" << endl;
                        // strings can be undefined, and set to
                        // their default value
                        m_parsed[name] = i.value().def;
                        break;
                    }

                    // all other types expect a value
                    cerr << "Command line option did not receive value"
                         << endl;
                    return false;
                }

//                cerr << "  value found: " << argv[argpos] << endl;

                val = QString::fromLocal8Bit(argv[argpos]);
                if (val.startsWith("-"))
                {
                    // new option reached without value
                    if (type == QVariant::String)
                    {
//                        cerr << "  string set to default" << endl;
                        m_parsed[name] = i.value().def;
                        break;
                    }

                    cerr << "Command line option did not receive value"
                         << endl;
                    return false;
                }

                if (type == QVariant::Int)
                    m_parsed[name] = QVariant(val.toInt());
                else if (type == QVariant::LongLong)
                    m_parsed[name] = QVariant(val.toLongLong());
                else if (type == QVariant::StringList)
                {
                    QStringList slist;
                    if (m_parsed.contains(name))
                        slist = m_parsed[name].toStringList();
                    slist << val;
                    m_parsed[name] = QVariant(slist);
                }
                else if (type == QVariant::Map)
                {
                    // check for missing key/val pair
                    if (!val.contains("="))
                    {
                        cerr << "Command line option did not get expected "
                                "key/value pair" << endl;
                        return false;
                    }

                    QStringList slist = val.split("=");
                    QVariantMap vmap;
                    if (m_parsed.contains(name))
                        vmap = m_parsed[name].toMap();
                    vmap[slist[0]] = QVariant(slist[1]);
                    m_parsed[name] = QVariant(vmap);
                }
                else if (type == QVariant::Size)
                {
                    if (!val.contains("x"))
                    {
                        cerr << "Command line option did not get expected "                                      "XxY pair" << endl;
                        return false;
                    }
                    QStringList slist = val.split("x");
                    m_parsed[name] = QSize(slist[0].toInt(), slist[1].toInt());
                }
                else
                    m_parsed[name] = QVariant(val);
                    // add more type specifics

                break;
            }
        }
#ifdef Q_WS_MACX
        if (opts.startsWith("-psn_"))
            cerr << "Ignoring Process Serial Number from command line"
                 << endl;
        else if (!processed && !m_allowExtras)
#else
        if (!processed && !m_allowExtras)
#endif
        {
            cerr << "Unhandled option given on command line" << endl;
            return false;
        }
    }

    return true;
}

QVariant MythCommandLineParser::operator[](const QString &name)
{
    QVariant res("");
    if (m_parsed.contains(name))
        res = m_parsed[name];
    else if (m_defaults.contains(name))
        res = m_defaults[name];
    return res;
}

QMap<QString,QString> MythCommandLineParser::GetSettingsOverride(void)
{
    QMap<QString,QString> smap;
    if (!m_parsed.contains("overridesettings"))
        return smap;

    QVariantMap vmap = m_parsed["overridesettings"].toMap();

    if (!m_overridesImported)
    {
        if (m_parsed.contains("overridesettingsfile"))
        {
            QString filename = m_parsed["overridesettingsfile"].toString();
            if (!filename.isEmpty())
            {
                QFile f(filename);
                if (f.open(QIODevice::ReadOnly))
                {
                    char buf[1024];
                    int64_t len = f.readLine(buf, sizeof(buf) - 1);
                    while (len != -1)
                    {
                        if (len >= 1 && buf[len-1]=='\n')
                            buf[len-1] = 0;
                        QString line(buf);
                        QStringList tokens = line.split("=",
                                QString::SkipEmptyParts);
                        if (tokens.size() == 2)
                        {
                            tokens[0].replace(QRegExp("^[\"']"), "");
                            tokens[0].replace(QRegExp("[\"']$"), "");
                            tokens[1].replace(QRegExp("^[\"']"), "");
                            tokens[1].replace(QRegExp("[\"']$"), "");
                            if (!tokens[0].isEmpty())
                                vmap[tokens[0]] = QVariant(tokens[1]);
                        }
                        len = f.readLine(buf, sizeof(buf) - 1);
                    }
                    m_parsed["overridesettings"] = QVariant(vmap);
                }
                else
                {
                    QByteArray tmp = filename.toAscii();
                    cerr << "Failed to open the override settings file: '"
                         << tmp.constData() << "'" << endl;
                }
            }
        }
        m_overridesImported = true;
    }

    QVariantMap::const_iterator i;
    for (i = vmap.begin(); i != vmap.end(); ++i)
        smap[i.key()] = i.value().toString();

    // add windowed boolean

    return smap;
}

bool MythCommandLineParser::toBool(QString key) const
{
    // If value is of type boolean, return its value
    // If value is of other type, return whether
    //      it was defined, of if it will return
    //      its default value

    bool val = false;
    if (m_parsed.contains(key))
        if (m_parsed[key].type() == QVariant::Bool)
            val = m_parsed[key].toBool();
        else
            val = true;
    return val;
}

int MythCommandLineParser::toInt(QString key) const
{
    // Return matching value if defined, else use default
    // If key is not registered, return 0
    int val = 0;
    if (m_parsed.contains(key))
        if (m_parsed[key].canConvert(QVariant::Int))
            val = m_parsed[key].toInt();
    else if (m_defaults.contains(key))
        if (m_defaults[key].canConvert(QVariant::Int))
            val = m_defaults[key].toInt();
    return val;
}

uint MythCommandLineParser::toUInt(QString key) const
{
    // Return matching value if defined, else use default
    // If key is not registered, return 0
    uint val = 0;
    if (m_parsed.contains(key))
        if (m_parsed[key].canConvert(QVariant::UInt))
            val = m_parsed[key].toUInt();
    else if (m_defaults.contains(key))
        if (m_defaults[key].canConvert(QVariant::UInt))
            val = m_defaults[key].toUInt();
    return val;
}


long long MythCommandLineParser::toLongLong(QString key) const
{
    // Return matching value if defined, else use default
    // If key is not registered, return 0
    long long val = 0;
    if (m_parsed.contains(key))
        if (m_parsed[key].canConvert(QVariant::LongLong))
            val = m_parsed[key].toLongLong();
    else if (m_defaults.contains(key))
        if (m_defaults[key].canConvert(QVariant::LongLong))
            val = m_defaults[key].toLongLong();
    return val;
}

double MythCommandLineParser::toDouble(QString key) const
{
    // Return matching value if defined, else use default
    // If key is not registered, return 0.0
    double val = 0.0;
    if (m_parsed.contains(key))
        if (m_parsed[key].canConvert(QVariant::Double))
            val = m_parsed[key].toDouble();
    else if (m_defaults.contains(key))
        if (m_defaults[key].canConvert(QVariant::Double))
            val = m_defaults[key].toDouble();
    return val;
}

QSize MythCommandLineParser::toSize(QString key) const
{
    // Return matching value if defined, else use default
    // If key is not registered, return (0,0)
    QSize val(0,0);
    if (m_parsed.contains(key))
        if (m_parsed[key].canConvert(QVariant::Size))
            val = m_parsed[key].toSize();
    else if (m_defaults.contains(key))
        if (m_defaults[key].canConvert(QVariant::Size))
            val = m_defaults[key].toSize();
    return val;
}

QString MythCommandLineParser::toString(QString key) const
{
    // Return matching value if defined, else use default
    // If key is not registered, return empty string
    QString val("");
    if (m_parsed.contains(key))
        if (m_parsed[key].canConvert(QVariant::String))
            val = m_parsed[key].toString();
    else if (m_defaults.contains(key))
        if (m_defaults[key].canConvert(QVariant::String))
            val = m_defaults[key].toString();
    return val;
}

QStringList MythCommandLineParser::toStringList(QString key) const
{
    // Return matching value if defined, else use default
    // If key is not registered, return empty stringlist
    QStringList val;
    if (m_parsed.contains(key))
        if (m_parsed[key].canConvert(QVariant::StringList))
            val << m_parsed[key].toStringList();
    else if (m_defaults.contains(key))
        if (m_defaults[key].canConvert(QVariant::StringList))
            val = m_defaults[key].toStringList();
    return val;
}

QMap<QString,QString> MythCommandLineParser::toMap(QString key) const
{
    // Return matching value if defined, else use default
    // If key is not registered, return empty stringmap
    QMap<QString,QString> val;
    if (m_parsed.contains(key))
    {
        if (m_parsed[key].canConvert(QVariant::Map))
        {
            QMap<QString,QVariant> tmp = m_parsed[key].toMap();
            QMap<QString,QVariant>::const_iterator i;
            for (i = tmp.begin(); i != tmp.end(); ++i)
                val[i.key()] = i.value().toString();
        }
    }
    else if (m_defaults.contains(key))
    {
        if (m_defaults[key].canConvert(QVariant::Map))
        {
            QMap<QString,QVariant> tmp = m_defaults[key].toMap();
            QMap<QString,QVariant>::const_iterator i;
            for (i = tmp.begin(); i != tmp.end(); ++i)
                val[i.key()] = i.value().toString();
        }
    }
    return val;
}

QDateTime MythCommandLineParser::toDateTime(QString key) const
{
    // Return matching value if defined, else use default
    // If key is not registered, return empty datetime
    QDateTime val;
    if (m_parsed.contains(key))
        if (m_parsed[key].canConvert(QVariant::DateTime))
            val = m_parsed[key].toDateTime();
    else if (m_defaults.contains(key))
        if (m_defaults[key].canConvert(QVariant::Int))
            val = m_defaults[key].toDateTime();
    return val;
}

void MythCommandLineParser::addHelp(void)
{
    add(QStringList( QStringList() << "-h" << "--help" ), "showhelp", "",
            "Display this help printout.",
            "Displays a list of all commands available for use with\n"
            "this application. If another option is provided as an\n"
            "argument, it will provide detailed information on that\n"
            "option.");
}

void MythCommandLineParser::addVersion(void)
{
    add("--version", "showversion", "Display version information.",
            "Display informtion about build, including:\n"
            "  version, branch, protocol, library API, Qt\n"
            "  and compiled options.");
}

void MythCommandLineParser::addWindowed(bool def)
{
    if (def)
        add(QStringList( QStringList() << "-nw" << "--no-windowed" ),
            "notwindowed", "Prevent application from running in window.", "");
    else
        add(QStringList( QStringList() << "-w" << "--windowed" ), "windowed",
            "Force application to run in a window.", "");
}

void MythCommandLineParser::addDaemon(void)
{
    add(QStringList( QStringList() << "-d" << "--daemon" ), "daemonize",
            "Fork application into background after startup.",
            "Fork application into background, detatching from\n"
            "the local terminal. Often used with:\n"
            "   --logfile       --pidfile\n"
            "   --user");
}

void MythCommandLineParser::addSettingsOverride(void)
{
    add(QStringList( QStringList() << "-O" << "--override-setting" ),
            "overridesettings", QVariant::Map,
            "Override a single setting defined by a key=value pair.",
            "Override a single setting from the database using\n"
            "options defined as one or more key=value pairs\n"
            "Multiple can be defined by multiple uses of the\n"
            "-O option.");
    add("--override-settings-file", "overridesettingsfile", "", 
            "Define a file of key=value pairs to be\n"
            "loaded for setting overrides.", "");
}

void MythCommandLineParser::addVerbose(void)
{
    add(QStringList( QStringList() << "-v" << "--verbose" ), "verbose",
            "important,general",
            "Specify log filtering. Use '-v help' for level info.", "");
    add("-V", "verboseint", 0U, "",
            "This option is intended for internal use only.\n"
            "This option takes an unsigned value corresponding\n"
            "to the bitwise log verbosity operator.");
}

void MythCommandLineParser::addRecording(void)
{
    add("--chanid", "chanid", 0U,
            "Specify chanid of recording to operate on.", "");
    add("--starttime", "starttime", "",
            "Specify start time of recording to operate on.", "");
}

void MythCommandLineParser::addGeometry(void)
{
    add(QStringList( QStringList() << "-geometry" << "--geometry" ), "geometry",
            "", "Specify window size and position (WxH[+X+Y])", "");
}

void MythCommandLineParser::addDisplay(void)
{
#ifdef USING_X11
    add("-display", "display", "", "Specify X server to use.", "");
#endif
}

void MythCommandLineParser::addUPnP(void)
{
    add("--noupnp", "noupnp", "Disable use of UPnP.", "");
}

void MythCommandLineParser::addLogFile(void)
{
    add(QStringList( QStringList() << "-l" << "--logfile" ), "logfile", "",
            "Writes STDERR and STDOUT messages to filename.",
            "Redirects messages typically sent to STDERR and STDOUT\n"
            "to this log file. This is typically used in combination\n"
            "with --daemon, and if used in combination with --pidfile,\n"
            "this can be used with log rotaters, using the HUP call\n"
            "to inform MythTV to reload the file.");
}

void MythCommandLineParser::addPIDFile(void)
{
    add(QStringList( QStringList() << "-p" << "--pidfile" ), "pidfile", "",
            "Write PID of application to filename.",
            "Write the PID of the currently running process as a single\n"
            "line to this file. Used for init scripts to know what\n"
            "process to terminate, and with --logfile and log rotaters\n"
            "to send a HUP signal to process to have it re-open files.");
}

void MythCommandLineParser::addJob(void)
{
    add(QStringList( QStringList() << "-j" << "--jobid" ), "jobid", 0, "",
            "Intended for internal use only, specify the JobID to match\n"
            "up with in the database for additional information and the\n"
            "ability to update runtime status in the database.");
}

MythBackendCommandLineParser::MythBackendCommandLineParser(void) :
    MythCommandLineParser(MYTH_APPNAME_MYTHBACKEND)
{ LoadArguments(); }

void MythBackendCommandLineParser::LoadArguments(void)
{
    addHelp();
    addVersion();
    addDaemon();
    addSettingsOverride();
    addVerbose();
    addUPnP();
    addLogFile();
    addPIDFile();

    add("--printsched", "printsched",
            "Print upcoming list of scheduled recordings.", "");
    add("--testsched", "testsched", "do some scheduler testing.", "");
    add("--resched", "resched",
            "Trigger a run of the recording scheduler on the existing\n"
            "master backend.",
            "This command will connect to the master backend and trigger\n"
            "a run of the recording scheduler. The call will return\n"
            "immediately, however the scheduler run may take several\n"
            "seconds to a minute or longer to complete.");
    add("--nosched", "nosched", "",
            "Intended for debugging use only, disable the scheduler\n"
            "on this backend if it is the master backend, preventing\n"
            "any recordings from occuring until the backend is\n"
            "restarted without this option.");
    add("--scanvideos", "scanvideos",
            "Trigger a rescan of media content in MythVideo.",
            "This command will connect to the master backend and trigger\n"
            "a run of the Video scanner. The call will return\n"
            "immediately, however the scanner may take several seconds\n"
            "to tens of minutes, depending on how much new or moved\n"
            "content it has to hash, and how quickly the scanner can\n"
            "access those files to do so. If enabled, this will also\n"
            "trigger the bulk metadata scanner upon completion.");
    add("--nojobqueue", "nojobqueue", "",
            "Intended for debugging use only, disable the jobqueue\n"
            "on this backend. As each jobqueue independently selects\n"
            "jobs, this will only have any affect on this local\n"
            "backend.");
    add("--nohousekeeper", "nohousekeeper", "",
            "Intended for debugging use only, disable the housekeeper\n"
            "on this backend if it is the master backend, preventing\n"
            "any guide processing, recording cleanup, or any other\n"
            "task performed by the housekeeper.");
    add("--noautoexpire", "noautoexpire", "",
            "Intended for debugging use only, disable the autoexpirer\n"
            "on this backend if it is the master backend, preventing\n"
            "recordings from being expired to clear room for new\n"
            "recordings.");
    add("--event", "event", "", "Send a backend event test message.", "");
    add("--systemevent", "systemevent", "",
            "Send a backend SYSTEM_EVENT test message.", "");
    add("--clearcache", "clearcache",
            "Trigger a cache clear on all connected MythTV systems.",
            "This command will connect to the master backend and trigger\n"
            "a cache clear event, which will subsequently be pushed to\n"
            "all other connected programs. This event will clear the\n"
            "local database settings cache used by each program, causing\n"
            "options to be re-read from the database upon next use.");
    add("--printexpire", "printexpire", "ALL",
            "Print upcoming list of recordings to be expired.", "");
    add("--setverbose", "setverbose", "",
            "Change debug level of the existing master backend.", "");
    add("--user", "username", "",
            "Drop permissions to username after starting.", "");
}

MythFrontendCommandLineParser::MythFrontendCommandLineParser(void) :
    MythCommandLineParser(MYTH_APPNAME_MYTHFRONTEND)
{ LoadArguments(); }

void MythFrontendCommandLineParser::LoadArguments(void)
{
    addHelp();
    addVersion();
    addWindowed(false);
    addSettingsOverride();
    addVerbose();
    addGeometry();
    addDisplay();
    addUPnP();
    addLogFile();

    add(QStringList( QStringList() << "-r" << "--reset" ), "reset",
        "Resets appearance, settings, and language.", "");
    add(QStringList( QStringList() << "-p" << "--prompt" ), "prompt",
        "Always prompt for backend selection.", "");
    add(QStringList( QStringList() << "-d" << "--disable-autodiscovery" ),
        "noautodiscovery", "Prevent frontend from using UPnP autodiscovery.", "");
}

MythPreviewGeneratorCommandLineParser::MythPreviewGeneratorCommandLineParser(void) :
    MythCommandLineParser(MYTH_APPNAME_MYTHPREVIEWGEN)
{ LoadArguments(); }

void MythPreviewGeneratorCommandLineParser::LoadArguments(void)
{
    addHelp();
    addVersion();
    addVerbose();
    addRecording();

    add("--seconds", "seconds", 0LL, "Number of seconds into video to take preview image.", "");
    add("--frame", "frame", 0LL, "Number of frames into video to take preview image.", "");
    add("--size", "size", QSize(0,0), "Dimensions of preview image.", "");
    add("--infile", "inputfile", "", "Input video for preview generation.", "");
    add("--outfile" "outputfile", "", "Optional output file for preview generation.", "");
}

MythWelcomeCommandLineParser::MythWelcomeCommandLineParser(void) :
    MythCommandLineParser(MYTH_APPNAME_MYTHWELCOME)
{ LoadArguments(); }

void MythWelcomeCommandLineParser::LoadArguments(void)
{
    addHelp();
    addSettingsOverride();
    addVersion();
    addVerbose();
    addLogFile();

    add(QStringList( QStringList() << "-s" << "--setup" ), "setup",
            "Run setup for mythshutdown.", "");
}

MythAVTestCommandLineParser::MythAVTestCommandLineParser(void) :
    MythCommandLineParser(MYTH_APPNAME_MYTHAVTEST)
{ LoadArguments(); }

void MythAVTestCommandLineParser::LoadArguments(void)
{
    addHelp();
    addSettingsOverride();
    addVersion();
    addWindowed(false);
    addVerbose();
    addGeometry();
    addDisplay();
}

MythCommFlagCommandLineParser::MythCommFlagCommandLineParser(void) :
    MythCommandLineParser(MYTH_APPNAME_MYTHCOMMFLAG)
{ LoadArguments(); }

void MythCommFlagCommandLineParser::LoadArguments(void)
{
    addHelp();
    addSettingsOverride();
    addVersion();
    addJob();

    add(QStringList( QStringList() << "-f" << "--file" ), "file", "",
            "Specify file to operate on.");
    add("--video", "video", "", "Rebuild the seek table for a video (non-recording) file.", "");
    add("--method", "commmethod", "", "Commercial flagging method[s] to employ:\n"
                                      "off, blank, scene, blankscene, logo, all\n"
                                      "d2, d2_logo, d2_blank, d2_scene, d2_all\n", "");
    add("--outputmethod", "outputmethod", "",
            "Format of output written to outputfile, essentials, full.", "");
    add("--gencutlist", "gencutlist", "Copy the commercial skip list to the cutlist.", "");
    add("--clearcutlist", "clearcutlist", "Clear the cutlist.", "");
    add("--clearskiplist", "clearskiplist", "Clear the commercial skip list.", "");
    add("--getcutlist", "getcutlist", "Display the current cutlist.", "");
    add("--getskiplist", "getskiplist", "Display the current commercial skip list.", "");
    add("--setcutlist", "setcutlist", "", "Set a new cutlist in the form:\n"
                                          "#-#[,#-#]... (ie, 1-100,1520-3012,4091-5094)", "");
    add("--skipdb", "skipdb", "", "Intended for external 3rd party use.");
    add("--quiet", "quiet", "Don't display commercial flagging progress.", "");
    add("--very-quiet", "vquiet", "Only display output.", "");
    add("--queue", "queue", "Insert flagging job into the JobQueue, rather than\n"
                            "running flagging in the foreground.", "");
    add("--nopercentage", "nopercent", "Don't print percentage done.", "");
    add("--rebuild", "rebuild", "Do not flag commercials, just rebuild the seektable.", "");
    add("--force", "force", "Force operation, even if program appears to be in use.", "");
    add("--dontwritetodb", "dontwritedb", "", "Intended for external 3rd party use.");
    add("--onlydumpdb", "dumpdb", "", "?");
    add("--outputfile", "outputfile", "", "File to write commercial flagging output [debug].", "");
}

MythJobQueueCommandLineParser::MythJobQueueCommandLineParser(void) :
    MythCommandLineParser(MYTH_APPNAME_MYTHJOBQUEUE)
{ LoadArguments(); }

void MythJobQueueCommandLineParser::LoadArguments(void)
{
    addHelp();
    addSettingsOverride();
    addVersion();
    addVerbose();
    addLogFile();
    addPIDFile();
    addDaemon();
}

MythFillDatabaseCommandLineParser::MythFillDatabaseCommandLineParser(void) :
    MythCommandLineParser(MYTH_APPNAME_MYTHFILLDATABASE)
{ LoadArguments(); }

void MythFillDatabaseCommandLineParser::LoadArguments(void)
{
}

MythLCDServerCommandLineParser::MythLCDServerCommandLineParser(void) :
    MythCommandLineParser(MYTH_APPNAME_MYTHLCDSERVER)
{ LoadArguments(); }

void MythLCDServerCommandLineParser::LoadArguments(void)
{
}

MythMessageCommandLineParser::MythMessageCommandLineParser(void) :
    MythCommandLineParser(MYTH_APPNAME_MYTHMESSAGE)
{ LoadArguments(); }

void MythMessageCommandLineParser::LoadArguments(void)
{
}

MythShutdownCommandLineParser::MythShutdownCommandLineParser(void) :
    MythCommandLineParser(MYTH_APPNAME_MYTHSHUTDOWN)
{ LoadArguments(); }

void MythShutdownCommandLineParser::LoadArguments(void)
{
}

MythTVSetupCommandLineParser::MythTVSetupCommandLineParser(void) :
    MythCommandLineParser(MYTH_APPNAME_MYTHTV_SETUP)
{ LoadArguments(); }

void MythTVSetupCommandLineParser::LoadArguments(void)
{
}

MythTranscodeCommandLineParser::MythTranscodeCommandLineParser(void) :
    MythCommandLineParser(MYTH_APPNAME_MYTHTRANSCODE)
{ LoadArguments(); }

void MythTranscodeCommandLineParser::LoadArguments(void)
{
}

