#include <iostream>
using namespace std;

#include <QFile>

#include "mythcommandlineparser.h"
#include "exitcodes.h"
#include "mythconfig.h"
#include "mythcontext.h"
#include "mythverbose.h"
#include "mythversion.h"

MythCommandLineParser::MythCommandLineParser(int things_to_parse) :
    parseTypes(things_to_parse),
    display(QString::null), geometry(QString::null),
    wantsToExit(false)
{
}

bool MythCommandLineParser::PreParse(
    int argc, const char * const * argv, int &argpos, bool &err)
{
    err = false;

    if (argpos >= argc)
        return false;

    if ((parseTypes & kCLPDisplay) &&
             (!strcmp(argv[argpos],"-display") ||
              !strcmp(argv[argpos],"--display")))
    {
        if ((argc - 1) > argpos)
        {
            display = argv[argpos+1];
            if (display.startsWith("-"))
            {
                cerr << "Invalid or missing argument to -display option\n";
                err = true;
                return true;
            }
            else
                ++argpos;
        }
        else
        {
            cerr << "Missing argument to -display option\n";
            err = true;
            return true;
        }

        return true;
    }
    else if ((parseTypes & kCLPGeometry) &&
             (!strcmp(argv[argpos],"-geometry") ||
              !strcmp(argv[argpos],"--geometry")))
    {
        if ((argc - 1) > argpos)
        {
            geometry = argv[argpos+1];
            if (geometry.startsWith("-"))
            {
                cerr << "Invalid or missing argument to -geometry option\n";
                err = true;
                return true;
            }
            else
                ++argpos;
        }
        else
        {
            cerr << "Missing argument to -geometry option\n";
            err = true;
            return true;
        }

        return true;
    }
    else if ((parseTypes & kCLPVerbose) &&
             (!strcmp(argv[argpos],"-v") ||
              !strcmp(argv[argpos],"--verbose")))
    {
        if ((argc - 1) > argpos)
        {
            if (parse_verbose_arg(argv[argpos+1]) ==
                GENERIC_EXIT_INVALID_CMDLINE)
            {
                wantsToExit = err = true;
            }
            ++argpos;
        }
        else
        {
            cerr << "Missing argument to -v/--verbose option";
            wantsToExit = err = true;
        }
        return true;
    }
    else if ((parseTypes & kCLPHelp) &&
             (!strcmp(argv[argpos],"-h") ||
              !strcmp(argv[argpos],"--help") ||
              !strcmp(argv[argpos],"--usage")))
    {
        QString help = GetHelpString(false);
        QByteArray ahelp = help.toLocal8Bit();
        cerr << ahelp.constData();
        wantsToExit = true;
        return true;
    }
    else if ((parseTypes & kCLPQueryVersion) &&
             !strcmp(argv[argpos],"--version"))
    {
        extern const char *myth_source_version;
        extern const char *myth_source_path;
        cout << "Please attach all output as a file in bug reports." << endl;
        cout << "MythTV Version   : " << myth_source_version << endl;
        cout << "MythTV Branch    : " << myth_source_path << endl;
        cout << "Network Protocol : " << MYTH_PROTO_VERSION << endl;
        cout << "Library API      : " << MYTH_BINARY_VERSION << endl;
        cout << "QT Version       : " << QT_VERSION_STR << endl;
#ifdef MYTH_BUILD_CONFIG
        cout << "Options compiled in:" <<endl;
        cout << MYTH_BUILD_CONFIG << endl;
#endif
        wantsToExit = true;
        return true;
    }
    else if ((parseTypes & kCLPExtra) &&
             argv[argpos][0] != '-')
        // Though it's allowed (err = false), we didn't handle the arg
        return false;

    return false;
}

bool MythCommandLineParser::Parse(
    int argc, const char * const * argv, int &argpos, bool &err)
{
    err = false;

    if (argpos >= argc)
        return false;

    if ((parseTypes & kCLPWindowed) &&
        (!strcmp(argv[argpos],"-w") ||
         !strcmp(argv[argpos],"--windowed")))
    {
        settingsOverride["RunFrontendInWindow"] = "1";
        return true;
    }
    else if ((parseTypes & kCLPNoWindowed) &&
             (!strcmp(argv[argpos],"-nw") ||
              !strcmp(argv[argpos],"--no-windowed")))
    {
        settingsOverride["RunFrontendInWindow"] = "0";
        return true;
    }
    else if ((parseTypes & kCLPOverrideSettingsFile) &&
             (!strcmp(argv[argpos],"--override-settings-file")))
    {
        if (argc <= argpos)
        {
            cerr << "Missing argument to --override-settings-file option\n";
            err = true;
            return true;
        }

        QString filename = QString::fromLocal8Bit(argv[++argpos]);
        QFile f(filename);
        if (!f.open(QIODevice::ReadOnly))
        {
            QByteArray tmp = filename.toAscii();
            cerr << "Failed to open the override settings file: '"
                 << tmp.constData() << "'" << endl;
            err = true;
            return true;
        }

        char buf[1024];
        int64_t len = f.readLine(buf, sizeof(buf) - 1);
        while (len != -1)
        {
            if (len >= 1 && buf[len-1]=='\n')
                buf[len-1] = 0;
            QString line(buf);
            QStringList tokens = line.split("=", QString::SkipEmptyParts);
            if (tokens.size() == 1)
                tokens.push_back("");
            if (tokens.size() >= 2)
            {
                tokens[0].replace(QRegExp("^[\"']"), "");
                tokens[0].replace(QRegExp("[\"']$"), "");
                tokens[1].replace(QRegExp("^[\"']"), "");
                tokens[1].replace(QRegExp("[\"']$"), "");
                if (!tokens[0].isEmpty())
                    settingsOverride[tokens[0]] = tokens[1];
            }
            len = f.readLine(buf, sizeof(buf) - 1);
        }
        return true;
    }
    else if ((parseTypes & kCLPOverrideSettings) &&
             (!strcmp(argv[argpos],"-O") ||
              !strcmp(argv[argpos],"--override-setting")))
    {
        if ((argc - 1) > argpos)
        {
            QString tmpArg = argv[argpos+1];
            if (tmpArg.startsWith("-"))
            {
                cerr << "Invalid or missing argument to "
                     << "-O/--override-setting option\n";
                err = true;
                return true;
            }

            QStringList pairs = tmpArg.split(",", QString::SkipEmptyParts);
            for (int index = 0; index < pairs.size(); ++index)
            {
                QStringList tokens = pairs[index].split(
                    "=", QString::SkipEmptyParts);
                if (tokens.size() == 1)
                    tokens.push_back("");
                if (tokens.size() >= 2)
                {
                    tokens[0].replace(QRegExp("^[\"']"), "");
                    tokens[0].replace(QRegExp("[\"']$"), "");
                    tokens[1].replace(QRegExp("^[\"']"), "");
                    tokens[1].replace(QRegExp("[\"']$"), "");
                    if (!tokens[0].isEmpty())
                        settingsOverride[tokens[0]] = tokens[1];
                }
            }
        }
        else
        {
            cerr << "Invalid or missing argument to "
                 << "-O/--override-setting option\n";
            err = true;
            return true;
        }

        ++argpos;
        return true;
    }
    else if ((parseTypes & kCLPGetSettings) && gContext &&
             (!strcmp(argv[argpos],"-G") ||
              !strcmp(argv[argpos],"--get-setting") ||
              !strcmp(argv[argpos],"--get-settings")))
    {
        if ((argc - 1) > argpos)
        {
            QString tmpArg = argv[argpos+1];
            if (tmpArg.startsWith("-"))
            {
                cerr << "Invalid or missing argument to "
                     << "-G/--get-setting option\n";
                err = true;
                return true;
            }

            settingsQuery = tmpArg.split(",", QString::SkipEmptyParts);
        }
        else
        {
            cerr << "Invalid or missing argument to "
                 << "-G/--get-setting option\n";
            err = true;
            return true;
        }

        ++argpos;
        return true;
    }
    else
    {
        return PreParse(argc, argv, argpos, err);
    }
}

QString MythCommandLineParser::GetHelpString(bool with_header) const
{
    QString str;
    QTextStream msg(&str, QIODevice::WriteOnly);

    if (with_header)
        msg << "Valid options are: " << endl;

    if (parseTypes & kCLPDisplay)
    {
        msg << "-display X-server              "
            << "Create GUI on X-server, not localhost" << endl;
    }

    if (parseTypes & kCLPGeometry)
    {
        msg << "-geometry or --geometry WxH    "
            << "Override window size settings" << endl;
        msg << "-geometry WxH+X+Y              "
            << "Override window size and position" << endl;
    }

    if (parseTypes & kCLPWindowed)
    {
        msg << "-w or --windowed               Run in windowed mode" << endl;
    }

    if (parseTypes & kCLPNoWindowed)
    {
        msg << "-nw or --no-windowed           Run in non-windowed mode "
            << endl;
    }

    if (parseTypes & kCLPOverrideSettings)
    {
        msg << "-O or --override-setting KEY=VALUE " << endl
            << "                               "
            << "Force the setting named 'KEY' to value 'VALUE'" << endl
            << "                               "
            << "This option may be repeated multiple times" << endl;
    }

    if (parseTypes & kCLPOverrideSettingsFile)
    {
        msg << "--override-settings-file <file> " << endl
            << "                               "
            << "File containing KEY=VALUE pairs for settings" << endl
            << "                               Use a comma seperated list to return multiple values"
            << endl;
    }

    if (parseTypes & kCLPGetSettings)
    {
        msg << "-G or --get-setting KEY[,KEY2,etc] " << endl
            << "                               "
            << "Returns the current database setting for 'KEY'" << endl;
    }

    if (parseTypes & kCLPQueryVersion)
    {
        msg << "--version                      Version information" << endl;
    }

    if (parseTypes & kCLPVerbose)
    {
        msg << "-v or --verbose debug-level    Use '-v help' for level info" << endl;
    }

    msg.flush();

    return str;
}
