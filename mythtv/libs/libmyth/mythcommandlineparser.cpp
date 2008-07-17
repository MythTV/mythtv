#include <QFile>

#include "mythcommandlineparser.h"
#include "exitcodes.h"
#include "mythcontext.h"

MythCommandLineParser::MythCommandLineParser(uint64_t things_to_parse) :
    parseTypes(things_to_parse)
{
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
        if (!f.open(IO_ReadOnly))
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
            QStringList tokens = QStringList::split("=", line);
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
        if (argc - 1 > argpos)
        {
            QString tmpArg = argv[argpos+1];
            if (tmpArg.startsWith("-"))
            {
                cerr << "Invalid or missing argument to "
                     << "-O/--override-setting option\n";
                err = true;
                return true;
            } 
 
            QStringList pairs = QStringList::split(",", tmpArg);
            for (int index = 0; index < pairs.size(); ++index)
            {
                QStringList tokens = QStringList::split("=", pairs[index]);
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
    else
    {
        return false;
    }
}

QString MythCommandLineParser::GetHelpString(bool with_header) const
{
    QString str;
    QTextStream msg(&str, IO_WriteOnly);

    if (with_header)
        msg << "Valid options are: " << endl;

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
            << "File containing KEY=VALUE pairs for settings" << endl;
    }

    msg.flush();

    return str;
}
