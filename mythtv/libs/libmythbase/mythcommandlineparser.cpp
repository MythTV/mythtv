#include <iostream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef _WIN32
#include <pwd.h>
#include <grp.h>
#endif

using namespace std;

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSize>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <QString>
#include <QCoreApplication>
#include <QTextStream>
#include <QDateTime>

#include "mythcommandlineparser.h"
#include "mythcorecontext.h"
#include "exitcodes.h"
#include "mythconfig.h"
#include "mythlogging.h"
#include "mythversion.h"
#include "util.h"

const int kEnd          = 0,
          kEmpty        = 1,
          kOptOnly      = 2,
          kOptVal       = 3,
          kArg          = 4,
          kPassthrough  = 5,
          kInvalid      = 6;

const char* NamedOptType(int type);
bool openPidfile(ofstream &pidfs, const QString &pidfile);
bool setUser(const QString &username);

const char* NamedOptType(int type)
{
    if (type == kEnd)
        return "kEnd";
    else if (type == kEmpty)
        return "kEmpty";
    else if (type == kOptOnly)
        return "kOptOnly";
    else if (type == kOptVal)
        return "kOptVal";
    else if (type == kArg)
        return "kArg";
    else if (type == kPassthrough)
        return "kPassthrough";
    else if (type == kInvalid)
        return "kInvalid";

    return "kUnknown";
}

typedef struct helptmp {
    QString left;
    QString right;
    QStringList arglist;
    CommandLineArg arg;
} HelpTmp;

MythCommandLineParser::MythCommandLineParser(QString appname) :
    m_appname(appname), m_allowExtras(false), m_allowArgs(false),
    m_allowPassthrough(false), m_passthroughActive(false),
    m_overridesImported(false), m_verbose(false)
{
    char *verbose = getenv("VERBOSE_PARSER");
    if (verbose != NULL)
    {
        cerr << "MythCommandLineParser is now operating verbosely." << endl;
        m_verbose = true;
    }

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
            if (m_verbose)
                cerr << "Adding " << (*i).toLocal8Bit().constData()
                     << " as taking type '" << QVariant::typeToName(type)
                     << "'" << endl;
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

        QString descr = GetHelpHeader();
        if (descr.size() > 0)
            msg << endl << descr << endl << endl;
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
            wrapList(rlist, 79-maxlen);
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
                return QString("Could not find option matching '%1'\n")
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

int MythCommandLineParser::getOpt(int argc, const char * const * argv,
                                    int &argpos, QString &opt, QString &val)
{
    opt.clear();
    val.clear();

    if (argpos >= argc)
        // this shouldnt happen, return and exit
        return kEnd;

    QString tmp = QString::fromLocal8Bit(argv[argpos]);
    if (tmp.isEmpty())
        // string is empty, return and loop
        return kEmpty;

    if (m_passthroughActive)
    {
        // pass through has been activated
        val = tmp;
        return kArg;
    }

    if (tmp.startsWith("-") && tmp.size() > 1)
    {
        if (tmp == "--")
        {
            // all options beyond this will be passed as a single string
            m_passthroughActive = true;
            return kPassthrough;
        }

        if (tmp.contains("="))
        {
            // option contains '=', split
            QStringList slist = tmp.split("=");

            if (slist.size() != 2)
            {
                // more than one '=' in option, this is not handled
                opt = tmp;
                return kInvalid;
            }

            opt = slist[0];
            val = slist[1];
            return kOptVal;
        }

        opt = tmp;

        if (argpos+1 >= argc)
            // end of input, option only
            return kOptOnly;

        tmp = QString::fromLocal8Bit(argv[++argpos]);
        if (tmp.isEmpty())
            // empty string, option only
            return kOptOnly;

        if (tmp.startsWith("-") && tmp.size() > 1)
        {
            // no value found for option, backtrack
            argpos--;
            return kOptOnly;
        }

        val = tmp;
        return kOptVal;
    }
    else
    {
        // input is not an option string, return as arg
        val = tmp;
        return kArg;
    }

}

bool MythCommandLineParser::Parse(int argc, const char * const * argv)
{
    bool processed;
    int res;
    QString opt, val;
    CommandLineArg argdef;

    if (m_allowExtras)
        m_parsed["extra"] = QVariant(QVariantMap());

    QMap<QString, CommandLineArg>::const_iterator i;
    for (int argpos = 1; argpos < argc; ++argpos)
    {

        res = getOpt(argc, argv, argpos, opt, val);

        if (m_verbose)
            cerr << "res: " << NamedOptType(res) << endl
                 << "opt: " << opt.toLocal8Bit().constData() << endl
                 << "val: " << val.toLocal8Bit().constData() << endl << endl;

        if (res == kPassthrough && !m_allowPassthrough)
        {
            cerr << "Received '--' but passthrough has not been enabled" << endl;
            return false;
        }

        if (res == kEnd)
            break;
        else if (res == kEmpty)
            continue;
        else if (res == kInvalid)
        {
            cerr << "Invalid option received:" << endl << "    "
                 << opt.toLocal8Bit().constData();
            return false;
        }
        else if (m_passthroughActive)
        {
            m_passthrough << val;
            continue;
        }
        else if (res == kArg)
        {
            if (!m_allowArgs)
            {
                cerr << "Received '"
                     << val.toAscii().constData()
                     << "' but unassociated arguments have not been enabled"
                     << endl;
                return false;        
            }

            m_remainingArgs << val;
            continue;
        }

        // this line should not be passed once arguments have started collecting
        if (!m_remainingArgs.empty())
        {
            cerr << "Command line arguments received out of sequence"
                 << endl;
            return false;
        }

#ifdef Q_WS_MACX
        if (opt.startsWith("-psn_"))
        {
            cerr << "Ignoring Process Serial Number from command line"
                 << endl;
            continue;
        }
#endif

        // scan for matching option handler
        processed = false;
        for (i = m_registeredArgs.begin(); i != m_registeredArgs.end(); ++i)
        {
            if (opt == i.key())
            {
                argdef = i.value();
                processed = true;
                break;
            }
        }

        // if unhandled, and extras are allowed, specify general collection pool
        if (!processed)
        {
            if (m_allowExtras)
            {
                argdef.name = "extra";
                argdef.type = QVariant::Map;
                QString tmp = QString("%1=%2").arg(opt).arg(val);
                val = tmp;
                res = kOptVal;
            }
            else
            {
                // else fault
                cerr << "Unhandled option given on command line:" << endl 
                     << "    " << opt.toLocal8Bit().constData() << endl;
                return false;
            }
        }

        if (res == kOptOnly)
        {
            if (argdef.type == QVariant::Bool)
                m_parsed[argdef.name] = QVariant(!(i.value().def.toBool()));
            else if (argdef.type == QVariant::Int)
            {
                if (m_parsed.contains(argdef.name))
                    m_parsed[argdef.name] = m_parsed[argdef.name].toInt() + 1;
                else
                    m_parsed[argdef.name] = QVariant((i.value().def.toInt())+1);
            }
            else if (argdef.type == QVariant::String)
                m_parsed[argdef.name] = i.value().def;
            else
            {
                cerr << "Command line option did not receive value:" << endl
                     << "    " << opt.toLocal8Bit().constData() << endl;
                return false;
            }
        }
        else if (res == kOptVal)
        {
            if (argdef.type == QVariant::Bool)
            {
                cerr << "Boolean type options do not accept values:" << endl
                     << "    " << opt.toLocal8Bit().constData() << endl;
                return false;
            }
            else if (argdef.type == QVariant::String)
                m_parsed[argdef.name] = QVariant(val);
            else if (argdef.type == QVariant::Int)
                m_parsed[argdef.name] = QVariant(val.toInt());
            else if (argdef.type == QVariant::UInt)
                m_parsed[argdef.name] = QVariant(val.toUInt());
            else if (argdef.type == QVariant::LongLong)
                m_parsed[argdef.name] = QVariant(val.toLongLong());
            else if (argdef.type == QVariant::Double)
                m_parsed[argdef.name] = QVariant(val.toDouble());
            else if (argdef.type == QVariant::DateTime)
                m_parsed[argdef.name] = QVariant(myth_dt_from_string(val));
            else if (argdef.type == QVariant::StringList)
            {
                QStringList slist;
                if (m_parsed.contains(argdef.name))
                    slist = m_parsed[argdef.name].toStringList();
                slist << val;
                m_parsed[argdef.name] = QVariant(slist);
            }
            else if (argdef.type == QVariant::Map)
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
                if (m_parsed.contains(argdef.name))
                    vmap = m_parsed[argdef.name].toMap();
                vmap[slist[0]] = QVariant(slist[1]);
                m_parsed[argdef.name] = QVariant(vmap);
            }
            else if (argdef.type == QVariant::Size)
            {
                if (!val.contains("x"))
                {
                    cerr << "Command line option did not get expected "
                            "XxY pair" << endl;
                    return false;
                }
                QStringList slist = val.split("x");
                m_parsed[argdef.name] = QSize(slist[0].toInt(), slist[1].toInt());
            }
            else
                m_parsed[argdef.name] = QVariant(val);
        }
    }

    if (m_verbose)
    {
        cerr << "Processed option list:" << endl;
        QMap<QString, QVariant>::const_iterator it = m_parsed.begin();
        for (; it != m_parsed.end(); it++)
        {
            cerr << "  " << it.key().leftJustified(30)
                              .toLocal8Bit().constData();
            if ((*it).type() == QVariant::Bool)
                cerr << ((*it).toBool() ? "True" : "False");
            else if ((*it).type() == QVariant::Int)
                cerr << (*it).toInt();
            else if ((*it).type() == QVariant::UInt)
                cerr << (*it).toUInt();
            else if ((*it).type() == QVariant::LongLong)
                cerr << (*it).toLongLong();
            else if ((*it).type() == QVariant::Double)
                cerr << (*it).toDouble();
            else if ((*it).type() == QVariant::Size)
            {
                QSize tmpsize = (*it).toSize();
                cerr <<  "x=" << tmpsize.width()
                     << " y=" << tmpsize.height();
            }
            else if ((*it).type() == QVariant::String)
                cerr << '"' << (*it).toString().toLocal8Bit()
                                    .constData()
                     << '"';
            else if ((*it).type() == QVariant::StringList)
                cerr << '"' << (*it).toStringList().join("\", \"")
                                    .toLocal8Bit().constData()
                     << '"';
            else if ((*it).type() == QVariant::Map)
            {
                QMap<QString, QVariant> tmpmap = (*it).toMap();
                bool first = true;
                QMap<QString, QVariant>::const_iterator it2 = tmpmap.begin();
                for (; it2 != tmpmap.end(); it2++)
                {
                    if (first)
                        first = false;
                    else
                        cerr << QString("").leftJustified(32)
                                           .toLocal8Bit().constData();
                    cerr << it2.key().toLocal8Bit().constData()
                         << '='
                         << (*it2).toString().toLocal8Bit().constData()
                         << endl;
                }
                continue;
            }
            else if ((*it).type() == QVariant::DateTime)
                cerr << (*it).toDateTime().toString(Qt::ISODate)
                             .toLocal8Bit().constData();
            cerr << endl;
        }

        cerr << endl << "Extra argument list:" << endl;
        QStringList::const_iterator it3 = m_remainingArgs.begin();
        for (; it3 != m_remainingArgs.end(); it3++)
            cerr << "  " << (*it3).toLocal8Bit().constData() << endl;

        if (m_allowPassthrough)
        {
            cerr << endl << "Passthrough string:" << endl;
            cerr << "  " << GetPassthrough().toLocal8Bit().constData() << endl;
        }

        cerr << endl;
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
    {
        if (m_parsed[key].type() == QVariant::Bool)
            val = m_parsed[key].toBool();
        else
            val = true;
    }
    else if (m_defaults.contains(key))
        if (m_defaults[key].type() == QVariant::Bool)
            val = m_defaults[key].toBool();
    return val;
}

int MythCommandLineParser::toInt(QString key) const
{
    // Return matching value if defined, else use default
    // If key is not registered, return 0
    int val = 0;
    if (m_parsed.contains(key))
    {
        if (m_parsed[key].canConvert(QVariant::Int))
            val = m_parsed[key].toInt();
    }
    else if (m_defaults.contains(key))
    {
        if (m_defaults[key].canConvert(QVariant::Int))
            val = m_defaults[key].toInt();
    }
    return val;
}

uint MythCommandLineParser::toUInt(QString key) const
{
    // Return matching value if defined, else use default
    // If key is not registered, return 0
    uint val = 0;
    if (m_parsed.contains(key))
    {
        if (m_parsed[key].canConvert(QVariant::UInt))
            val = m_parsed[key].toUInt();
    }
    else if (m_defaults.contains(key))
    {
        if (m_defaults[key].canConvert(QVariant::UInt))
            val = m_defaults[key].toUInt();
    }
    return val;
}


long long MythCommandLineParser::toLongLong(QString key) const
{
    // Return matching value if defined, else use default
    // If key is not registered, return 0
    long long val = 0;
    if (m_parsed.contains(key))
    {
        if (m_parsed[key].canConvert(QVariant::LongLong))
            val = m_parsed[key].toLongLong();
    }
    else if (m_defaults.contains(key))
    {
        if (m_defaults[key].canConvert(QVariant::LongLong))
            val = m_defaults[key].toLongLong();
    }
    return val;
}

double MythCommandLineParser::toDouble(QString key) const
{
    // Return matching value if defined, else use default
    // If key is not registered, return 0.0
    double val = 0.0;
    if (m_parsed.contains(key))
    {
        if (m_parsed[key].canConvert(QVariant::Double))
            val = m_parsed[key].toDouble();
    }
    else if (m_defaults.contains(key))
    {
        if (m_defaults[key].canConvert(QVariant::Double))
            val = m_defaults[key].toDouble();
    }
    return val;
}

QSize MythCommandLineParser::toSize(QString key) const
{
    // Return matching value if defined, else use default
    // If key is not registered, return (0,0)
    QSize val(0,0);
    if (m_parsed.contains(key))
    {
        if (m_parsed[key].canConvert(QVariant::Size))
            val = m_parsed[key].toSize();
    }
    else if (m_defaults.contains(key))
    {
        if (m_defaults[key].canConvert(QVariant::Size))
            val = m_defaults[key].toSize();
    }
    return val;
}

QString MythCommandLineParser::toString(QString key) const
{
    // Return matching value if defined, else use default
    // If key is not registered, return empty string
    QString val("");
    if (m_parsed.contains(key))
    {
        if (m_parsed[key].canConvert(QVariant::String))
            val = m_parsed[key].toString();
    }
    else if (m_defaults.contains(key))
    {
        if (m_defaults[key].canConvert(QVariant::String))
            val = m_defaults[key].toString();
    }
    return val;
}

QStringList MythCommandLineParser::toStringList(QString key, QString sep) const
{
    // Return matching value if defined, else use default
    // If key is not registered, return empty stringlist
    QStringList val;
    if (m_parsed.contains(key))
    {
        if (m_parsed[key].type() == QVariant::String && !sep.isEmpty())
            val << m_parsed[key].toString().split(sep);
        else if (m_parsed[key].canConvert(QVariant::StringList))
            val << m_parsed[key].toStringList();
    }
    else if (m_defaults.contains(key))
    {
        if (m_defaults[key].type() == QVariant::String && !sep.isEmpty())
            val << m_defaults[key].toString().split(sep);
        else if (m_defaults[key].canConvert(QVariant::StringList))
            val = m_defaults[key].toStringList();
    }
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
    {
        if (m_parsed[key].canConvert(QVariant::DateTime))
            val = m_parsed[key].toDateTime();
    }
    else if (m_defaults.contains(key))
    {
        if (m_defaults[key].canConvert(QVariant::Int))
            val = m_defaults[key].toDateTime();
    }
    return val;
}

void MythCommandLineParser::addHelp(void)
{
    add(QStringList( QStringList() << "-h" << "--help" << "--usage" ),
            "showhelp", "", "Display this help printout.",
            "Displays a list of all commands available for use with "
            "this application. If another option is provided as an "
            "argument, it will provide detailed information on that "
            "option.");
}

void MythCommandLineParser::addVersion(void)
{
    add("--version", "showversion", false, "Display version information.",
            "Display informtion about build, including:\n"
            " version, branch, protocol, library API, Qt "
            "and compiled options.");
}

void MythCommandLineParser::addWindowed(bool def)
{
    if (def)
        add(QStringList( QStringList() << "-nw" << "--no-windowed" ), false,
            "notwindowed", "Prevent application from running in window.", "");
    else
        add(QStringList( QStringList() << "-w" << "--windowed" ), false,
            "windowed", "Force application to run in a window.", "");
}

void MythCommandLineParser::addDaemon(void)
{
    add(QStringList( QStringList() << "-d" << "--daemon" ), "daemon", false,
            "Fork application into background after startup.",
            "Fork application into background, detatching from "
            "the local terminal.\nOften used with: "
            " --logpath --pidfile --user");
}

void MythCommandLineParser::addSettingsOverride(void)
{
    add(QStringList( QStringList() << "-O" << "--override-setting" ),
            "overridesettings", QVariant::Map,
            "Override a single setting defined by a key=value pair.",
            "Override a single setting from the database using "
            "options defined as one or more key=value pairs\n"
            "Multiple can be defined by multiple uses of the "
            "-O option.");
    add("--override-settings-file", "overridesettingsfile", "", 
            "Define a file of key=value pairs to be "
            "loaded for setting overrides.", "");
}

void MythCommandLineParser::addRecording(void)
{
    add("--chanid", "chanid", 0U,
            "Specify chanid of recording to operate on.", "");
    add("--starttime", "starttime", QDateTime(),
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
    add("--noupnp", "noupnp", false, "Disable use of UPnP.", "");
}

void MythCommandLineParser::addLogging(void)
{
    add(QStringList( QStringList() << "-v" << "--verbose" ), "verbose",
            "general",
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
    add("--loglevel", "loglevel", "info", 
            "Set the logging level.  All log messages at lower levels will be "
            "discarded.\n"
            "In descending order: emerg, alert, crit, err, warning, notice, "
            "info, debug\ndefaults to info", "");
    add("--syslog", "syslog", "none", 
            "Set the syslog logging facility.\nSet to \"none\" to disable, "
            "defaults to none", "");
    add("--nodblog", "nodblog", false, "Disable database logging.", "");
}

void MythCommandLineParser::addPIDFile(void)
{
    add(QStringList( QStringList() << "-p" << "--pidfile" ), "pidfile", "",
            "Write PID of application to filename.",
            "Write the PID of the currently running process as a single "
            "line to this file. Used for init scripts to know what "
            "process to terminate, and with --logfile and log rotaters "
            "to send a HUP signal to process to have it re-open files.");
}

void MythCommandLineParser::addJob(void)
{
    add(QStringList( QStringList() << "-j" << "--jobid" ), "jobid", 0, "",
            "Intended for internal use only, specify the JobID to match "
            "up with in the database for additional information and the "
            "ability to update runtime status in the database.");
}

QString MythCommandLineParser::GetLogFilePath(void)
{
    QString logfile = toString("logpath");
    pid_t   pid = getpid();

    if (logfile.isEmpty())
        return logfile;

    QString logdir;
    QString filepath;

    QFileInfo finfo(logfile);
    if (finfo.isDir())
    {
        m_parsed.insert("islogpath", true);
        logdir  = finfo.filePath();
        logfile = QCoreApplication::applicationName() + "." +
                  QDateTime::currentDateTime().toString("yyyyMMddhhmmss") +
                  QString(".%1").arg(pid) + ".log";
    }
    else
    {
        m_parsed.insert("islogpath", false);
        logdir  = finfo.path();
        logfile = finfo.fileName();
    }

    m_parsed.insert("logdir", logdir);
    m_parsed.insert("logfile", logfile);
    m_parsed.insert("filepath", QFileInfo(QDir(logdir), logfile).filePath());

    return toString("filepath");
}

int MythCommandLineParser::GetSyslogFacility(void)
{
    QString setting = toString("syslog").toLower();
    if (setting == "none")
        return -2;

    return syslogGetFacility(setting);
}

LogLevel_t MythCommandLineParser::GetLogLevel(void)
{
    QString setting = toString("loglevel");
    if (setting.isEmpty())
        return LOG_INFO;

    LogLevel_t level = logLevelGet(setting);
    if (level == LOG_UNKNOWN)
        cerr << "Unknown log level: " << setting.toLocal8Bit().constData() <<
                endl;
     
    return level;
}

bool MythCommandLineParser::SetValue(const QString &key, QVariant value)
{
    if (!m_defaults.contains(key))
        return false;

    if (m_defaults[key].type() != value.type())
        return false;

    m_parsed[key] = value;
    return true;
}

int MythCommandLineParser::ConfigureLogging(QString mask, unsigned int quiet)
{
    int err = 0;

    // Setup the defaults
    verboseString = "";
    verboseMask   = 0;
    verboseArgParse(mask);

    if (toBool("verbose"))
    {
        if ((err = verboseArgParse(toString("verbose"))))
            return err;
    }
    else if (toBool("verboseint"))
        verboseMask = toUInt("verboseint");

    quiet = MAX(quiet, toUInt("quiet"));
    if (quiet > 1)
    {
        verboseMask = VB_NONE;
        verboseArgParse("none");
    }

    int facility = GetSyslogFacility();
    bool dblog = !toBool("nodblog");
    LogLevel_t level = GetLogLevel();
    if (level == LOG_UNKNOWN)
        return GENERIC_EXIT_INVALID_CMDLINE;

    LOG(VB_GENERAL, LOG_CRIT, QString("%1 version: %2 [%3] www.mythtv.org")
            .arg(QCoreApplication::applicationName())
            .arg(MYTH_SOURCE_PATH) .arg(MYTH_SOURCE_VERSION));
    LOG(VB_GENERAL, LOG_CRIT, QString("Enabled verbose msgs: %1")
                                  .arg(verboseString));

    QString logfile = GetLogFilePath();
    bool propogate = toBool("islogpath");
    logStart(logfile, quiet, facility, level, dblog, propogate);

    return GENERIC_EXIT_OK;
}

// WARNING: this must not be called until after MythContext is initialized
void MythCommandLineParser::ApplySettingsOverride(void)
{
    QMap<QString, QString> override = GetSettingsOverride();
    if (override.size())
    {
        QMap<QString, QString>::iterator it;
        for (it = override.begin(); it != override.end(); ++it)
        {
            LOG(VB_GENERAL, LOG_NOTICE,
                 QString("Setting '%1' being forced to '%2'")
                     .arg(it.key()).arg(*it));
            gCoreContext->OverrideSettingForSession(it.key(), *it);
        }
    }
}

bool openPidfile(ofstream &pidfs, const QString &pidfile)
{
    if (!pidfile.isEmpty())
    {
        pidfs.open(pidfile.toAscii().constData());
        if (!pidfs)
        {
            cerr << "Could not open pid file: " << ENO_STR << endl;
            return false;
        }
    }
    return true;
}

bool setUser(const QString &username)
{
    if (username.isEmpty())
        return true;

#ifdef _WIN32
    cerr << "--user option is not supported on Windows" << endl;
    return false;
#else // ! _WIN32
    struct passwd *user_info = getpwnam(username.toLocal8Bit().constData());
    const uid_t user_id = geteuid();

    if (user_id && (!user_info || user_id != user_info->pw_uid))
    {
        cerr << "You must be running as root to use the --user switch." << endl;
        return false;
    }
    else if (user_info && user_id == user_info->pw_uid)
    {
        LOG(VB_GENERAL, LOG_WARNING,
            QString("Already running as '%1'").arg(username));
    }
    else if (!user_id && user_info)
    {
        if (setenv("HOME", user_info->pw_dir,1) == -1)
        {
            cerr << "Error setting home directory." << endl;
            return false;
        }
        if (setgid(user_info->pw_gid) == -1)
        {
            cerr << "Error setting effective group." << endl;
            return false;
        }
        if (initgroups(user_info->pw_name, user_info->pw_gid) == -1)
        {
            cerr << "Error setting groups." << endl;
            return false;
        }
        if (setuid(user_info->pw_uid) == -1)
        {
            cerr << "Error setting effective user." << endl;
            return false;
        }
    }
    else
    {
        cerr << QString("Invalid user '%1' specified with --user")
                    .arg(username).toLocal8Bit().constData() << endl;
        return false;
    }
    return true;
#endif // ! _WIN32
}


int MythCommandLineParser::Daemonize(void)
{
    ofstream pidfs;
    if (!openPidfile(pidfs, toString("pidfile")))
        return GENERIC_EXIT_PERMISSIONS_ERROR;

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        LOG(VB_GENERAL, LOG_WARNING, "Unable to ignore SIGPIPE");

    if (toBool("daemon") && (daemon(0, 1) < 0))
    {
        cerr << "Failed to daemonize: " << ENO_STR << endl;
        return GENERIC_EXIT_DAEMONIZING_ERROR;
    }

    QString username = toString("username");
    if (!username.isEmpty() && !setUser(username))
        return GENERIC_EXIT_PERMISSIONS_ERROR;

    if (pidfs)
    {
        pidfs << getpid() << endl;
        pidfs.close();
    }

    return GENERIC_EXIT_OK;
}
