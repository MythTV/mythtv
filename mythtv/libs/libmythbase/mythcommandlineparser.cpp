#include <iostream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <sys/types.h>
#include <unistd.h>

#ifndef _WIN32
#include <sys/ioctl.h>
#include <pwd.h>
#include <grp.h>
#if defined(__linux__) || defined(__LINUX__)
#include <sys/prctl.h>
#endif
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
#include "logging.h"
#include "util.h"

#define TERMWIDTH 79

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

int GetTermWidth(void)
{
#ifdef _WIN32
    return TERMWIDTH;
#else
    struct winsize ws;

    if (ioctl(0, TIOCGWINSZ, &ws) != 0)
        return TERMWIDTH;

    return (int)ws.ws_col;
#endif
}

const char* NamedOptType(int type)
{
    switch (type)
    {
      case kEnd:
        return "kEnd";

      case kEmpty:
        return "kEmpty";

      case kOptOnly:
        return "kOptOnly";

      case kOptVal:
        return "kOptVal";

      case kArg:
        return "kArg";

      case kPassthrough:
        return "kPassthrough";

      case kInvalid:
        return "kInvalid";

      default:
        return "kUnknown";
    }
}

CommandLineArg::CommandLineArg(QString name, QVariant::Type type,
                   QVariant def, QString help, QString longhelp) :
    ReferenceCounter(), m_given(false), m_name(name), m_group(""),
    m_type(type), m_default(def), m_help(help), m_longhelp(longhelp)
{
}

CommandLineArg::CommandLineArg(QString name, QVariant::Type type, QVariant def)
  : ReferenceCounter(), m_given(false), m_name(name), m_group(""),
    m_type(type), m_default(def)
{
}

CommandLineArg::CommandLineArg(QString name) :
    ReferenceCounter(), m_given(false), m_name(name), 
    m_type(QVariant::Invalid)
{
}

QString CommandLineArg::GetKeywordString(void) const
{
    return m_keywords.join(" OR ");
}

int CommandLineArg::GetKeywordLength(void) const
{
    int len = GetKeywordString().length();

    QList<CommandLineArg*>::const_iterator i1;
    for (i1 = m_parents.begin(); i1 != m_parents.end(); ++i1)
        len = max(len, (*i1)->GetKeywordLength()+2);

    return len;
}

QString CommandLineArg::GetHelpString(int off, QString group, bool force) const
{
    QString helpstr;
    QTextStream msg(&helpstr, QIODevice::WriteOnly);
    int termwidth = GetTermWidth();

    if (m_help.isEmpty() && !force)
        // only print if there is a short help to print
        return helpstr;

    if ((m_group != group) && !force)
        // only print if looping over the correct group
        return helpstr;

    if (!m_parents.isEmpty() && !force)
        // only print if an independent option, not subject
        // to a parent option
        return helpstr;

    QString pad;
    pad.fill(' ', off);

    QStringList hlist = m_help.split('\n');
    wrapList(hlist, termwidth-off);
    if (!m_parents.isEmpty())
        msg << "  ";
    msg << GetKeywordString().leftJustified(off, ' ') 
        << hlist[0] << endl;

    QStringList::const_iterator i1;
    for (i1 = hlist.begin() + 1; i1 != hlist.end(); ++i1)
        msg << pad << *i1 << endl;

    QList<CommandLineArg*>::const_iterator i2;
    for (i2 = m_children.begin(); i2 != m_children.end(); ++i2)
        msg << (*i2)->GetHelpString(off, group, true);

    msg.flush();
    return helpstr;
}

QString CommandLineArg::GetLongHelpString(QString keyword) const
{
    QString helpstr;
    QTextStream msg(&helpstr, QIODevice::WriteOnly);
    int termwidth = GetTermWidth();

    if (!m_keywords.contains(keyword))
        return helpstr;

    msg << "Option:      " << keyword << endl << endl;

    bool first = true;

    QStringList::const_iterator i1;
    i1 = m_keywords.begin();
    while (i1 != m_keywords.end())
    {
        if (*i1 != keyword)
        {
            if (first)
            {
                msg << "Aliases:     " << *i1 << endl;
                first = false;
            }
            else
                msg << "             " << *i1 << endl;
        }
        i1++;
    }

    msg << "Type:        " << QVariant::typeToName(m_type) << endl;
    if (m_default.canConvert(QVariant::String))
        msg << "Default:     " << m_default.toString() << endl;

    QStringList help;
    if (m_longhelp.isEmpty())
        help = m_help.split("\n");
    else
        help = m_longhelp.split("\n");
    wrapList(help, termwidth-13);

    msg << "Description: " << help[0] << endl;
    for (i1 = help.begin() + 1; i1 != help.end(); ++i1)
        msg << "             " << *i1 << endl;

    QList<CommandLineArg*>::const_iterator i2;

    if (!m_parents.isEmpty())
    {
        msg << endl << "Can be used in combination with:" << endl;
        for (i2 = m_parents.constBegin(); i2 != m_parents.constEnd(); ++i2)
            msg << " " << (*i2)->GetPreferredKeyword()
                                    .toLocal8Bit().constData();
        msg << endl;
    }

    if (!m_children.isEmpty())
    {
        msg << endl << "Allows the use of:" << endl;
        for (i2 = m_children.constBegin(); i2 != m_children.constEnd(); ++i2)
            msg << " " << (*i2)->GetPreferredKeyword()
                                    .toLocal8Bit().constData();
        msg << endl;
    }

    if (!m_requires.isEmpty())
    {
        msg << endl << "Requires the use of:" << endl;
        for (i2 = m_requires.constBegin(); i2 != m_requires.constEnd(); ++i2)
            msg << " " << (*i2)->GetPreferredKeyword()
                                    .toLocal8Bit().constData();
        msg << endl;
    }

    if (!m_blocks.isEmpty())
    {
        msg << endl << "Prevents the use of:" << endl;
        for (i2 = m_blocks.constBegin(); i2 != m_blocks.constEnd(); ++i2)
            msg << " " << (*i2)->GetPreferredKeyword()
                                    .toLocal8Bit().constData();
        msg << endl;
    }

    msg.flush();
    return helpstr;
}

bool CommandLineArg::Set(QString opt)
{
    m_usedKeyword = opt;

    switch (m_type)
    {
      case QVariant::Bool:
        m_stored = QVariant(!m_default.toBool());
        break;

      case QVariant::Int:
        if (m_stored.isNull())
            m_stored = QVariant(m_default.toInt() + 1);
        else
            m_stored = QVariant(m_stored.toInt() + 1);
        break;

      case QVariant::String:
        m_stored = m_default;
        break;

      default:
        cerr << "Command line option did not receive value:" << endl
             << "    " << opt.toLocal8Bit().constData() << endl;
        return false;
    }

    m_given = true;
    return true;
}

bool CommandLineArg::Set(QString opt, QString val)
{
    QStringList slist;
    QVariantMap vmap;

    m_usedKeyword = opt;

    switch (m_type)
    {
      case QVariant::Bool:
        cerr << "Boolean type options do not accept values:" << endl
             << "    " << opt.toLocal8Bit().constData() << endl;
        return false;

      case QVariant::String:
        m_stored = QVariant(val);
        break;

      case QVariant::Int:
        m_stored = QVariant(val.toInt());
        break;

      case QVariant::UInt:
        m_stored = QVariant(val.toUInt());
        break;

      case QVariant::LongLong:
        m_stored = QVariant(val.toLongLong());
        break;

      case QVariant::Double:
        m_stored = QVariant(val.toDouble());
        break;

      case QVariant::DateTime:
        m_stored = QVariant(myth_dt_from_string(val));
        break;

      case QVariant::StringList:
        if (!m_stored.isNull())
            slist = m_stored.toStringList();
        slist << val;
        m_stored = QVariant(slist);
        break;

      case QVariant::Map:
        if (!val.contains('='))
        {
            cerr << "Command line option did not get expected "
                 << "key/value pair" << endl;
            return false;
        }

        slist = val.split('=');

        if (!m_stored.isNull())
            vmap = m_stored.toMap();
        vmap[slist[0]] = QVariant(slist[1]);
        m_stored = QVariant(vmap);
        break;

      case QVariant::Size:
        if (!val.contains('x'))
        {
            cerr << "Command line option did not get expected "
                 << "XxY pair" << endl;
            return false;
        }

        slist = val.split('x');
        m_stored = QVariant(QSize(slist[0].toInt(), slist[1].toInt()));
        break;

      default:
        m_stored = QVariant(val);
    }

    m_given = true;
    return true;
}

CommandLineArg* CommandLineArg::SetParentOf(QString opt)
{
    m_children << new CommandLineArg(opt);
    return this;
}

CommandLineArg* CommandLineArg::SetParentOf(QStringList opts)
{
    QStringList::const_iterator i = opts.begin();
    for (; i != opts.end(); ++i)
        m_children << new CommandLineArg(*i);
    return this;
}

CommandLineArg* CommandLineArg::SetChildOf(QString opt)
{
    m_parents << new CommandLineArg(opt);
    return this;
}

CommandLineArg* CommandLineArg::SetChildOf(QStringList opts)
{
    QStringList::const_iterator i = opts.begin();
    for (; i != opts.end(); ++i)
        m_parents << new CommandLineArg(*i);
    return this;
}

CommandLineArg* CommandLineArg::SetRequiredChildOf(QString opt)
{
    m_parents << new CommandLineArg(opt);
    m_requiredby << new CommandLineArg(opt);
    return this;
}

CommandLineArg* CommandLineArg::SetRequiredChildOf(QStringList opts)
{
    QStringList::const_iterator i = opts.begin();
    for (; i != opts.end(); ++i)
    {
        m_parents << new CommandLineArg(*i);
        m_requiredby << new CommandLineArg(*i);
    }
    return this;
}

CommandLineArg* CommandLineArg::SetRequires(QString opt)
{
    m_requires << new CommandLineArg(opt);
    return this;
}

CommandLineArg* CommandLineArg::SetRequires(QStringList opts)
{
    QStringList::const_iterator i = opts.begin();
    for (; i != opts.end(); ++i)
        m_requires << new CommandLineArg(*i);
    return this;
}

CommandLineArg* CommandLineArg::SetBlocks(QString opt)
{
    m_blocks << new CommandLineArg(opt);
    return this;
}

CommandLineArg* CommandLineArg::SetBlocks(QStringList opts)
{
    QStringList::const_iterator i = opts.begin();
    for (; i != opts.end(); ++i)
        m_blocks << new CommandLineArg(*i);
    return this;
}

void CommandLineArg::SetParentOf(CommandLineArg *other, bool forward)
{
    int i;
    bool replaced = false;
    other->UpRef();

    for (i = 0; i < m_children.size(); i++)
    {
        if (m_children[i]->m_name == other->m_name)
        {
            m_children[i]->DownRef();
            m_children.replace(i, other);
            replaced = true;
            break;
        }
    }

    if (!replaced)
        m_children << other;

    if (forward)
        other->SetChildOf(this, false);
}

void CommandLineArg::SetChildOf(CommandLineArg *other, bool forward)
{
    int i;
    bool replaced = false;
    other->UpRef();

    for (i = 0; i < m_parents.size(); i++)
    {
        if (m_parents[i]->m_name == other->m_name)
        {
            m_parents[i]->DownRef();
            m_parents.replace(i, other);
            replaced = true;
            break;
        }
    }

    if (!replaced)
        m_parents << other;

    if (forward)
        other->SetParentOf(this, false);
}

void CommandLineArg::SetRequires(CommandLineArg *other, bool forward)
{
    int i;
    bool replaced = false;
    other->UpRef();

    for (i = 0; i < m_requires.size(); i++)
    {
        if (m_requires[i]->m_name == other->m_name)
        {
            m_requires[i]->DownRef();
            m_requires.replace(i, other);
            replaced = true;
            break;
        }
    }

    if (!replaced)
        m_requires << other;

//  requirements need not be reciprocal
//    if (forward)
//        other->SetRequires(this, false);
}

void CommandLineArg::SetBlocks(CommandLineArg *other, bool forward)
{
    int i;
    bool replaced = false;
    other->UpRef();

    for (i = 0; i < m_blocks.size(); i++)
    {
        if (m_blocks[i]->m_name == other->m_name)
        {
            m_blocks[i]->DownRef();
            m_blocks.replace(i, other);
            replaced = true;
            break;
        }
    }

    if (!replaced)
        m_blocks << other;

    if (forward)
        other->SetBlocks(this, false);
}

void CommandLineArg::AllowOneOf(QList<CommandLineArg*> args)
{
    // TODO: blocks do not get set properly if multiple dummy arguments
    //       are provided. since this method will not have access to the
    //       argument list, this issue will have to be resolved later in
    //       ReconcileLinks().
    QList<CommandLineArg*>::const_iterator i1,i2;

    // loop through all but the last entry
    for (i1 = args.begin(); i1 != args.end()-1; ++i1)
    {
        // loop through the next to the last entry
        // and block use with the current
        for (i2 = i1+1; i2 != args.end(); ++i2)
        {
            (*i1)->SetBlocks(*i2);
        }

        if ((*i1)->m_type == QVariant::Invalid)
            (*i1)->DownRef();
    }
}

QString CommandLineArg::GetPreferredKeyword(void) const
{
    QStringList::const_iterator it;
    QString preferred;
    int len = 0, len2;

    for (it = m_keywords.constBegin(); it != m_keywords.constEnd(); ++it)
    {
        len2 = (*it).size();
        if (len2 > len)
        {
            preferred = *it;
            len = len2;
        }
    }

    return preferred;
}

bool CommandLineArg::TestLinks(void) const
{
    if (!m_given)
        return true; // not in use, no need for checks

    QList<CommandLineArg*>::const_iterator i;

    bool passes = false;
    for (i = m_parents.constBegin(); i != m_parents.constEnd(); ++i)
    {
        // one of these must have been defined
        if ((*i)->m_given)
        {
            passes = true;
            break;
        }
    }
    if (!passes && !m_parents.isEmpty())
    {
        cerr << "ERROR: " << m_usedKeyword.toLocal8Bit().constData()
             << " requires at least one of the following arguments" << endl;
        for (i = m_parents.constBegin(); i != m_parents.constEnd(); ++i)
            cerr << " "
                 << (*i)->GetPreferredKeyword().toLocal8Bit().constData();
        cerr << endl << endl;
        return false;
    }

    // we dont care about children

    for (i = m_requires.constBegin(); i != m_requires.constEnd(); ++i)
    {
        // all of these must have been defined
        if (!(*i)->m_given)
        {
            cerr << "ERROR: " << m_usedKeyword.toLocal8Bit().constData()
                 << " requires all of the following be defined as well"
                 << endl;
            for (i = m_requires.constBegin(); i != m_requires.constEnd(); ++i)
                cerr << " "
                     << (*i)->GetPreferredKeyword().toLocal8Bit()
                                                   .constData();
            cerr << endl << endl;
            return false;
        }
    }

    for (i = m_blocks.constBegin(); i != m_blocks.constEnd(); ++i)
    {
        // none of these can be defined
        if ((*i)->m_given)
        {
            cerr << "ERROR: " << m_usedKeyword.toLocal8Bit().constData()
                 << " requires that none of the following be defined" << endl;
            for (i = m_blocks.constBegin(); i != m_blocks.constEnd(); ++i)
                cerr << " "
                     << (*i)->GetPreferredKeyword().toLocal8Bit()
                                                   .constData();
            cerr << endl << endl;
            return false;
        }
    }

    return true;
}

void CommandLineArg::CleanupLinks(void)
{
    // clear out interdependent pointers in preparation for deletion
    while (!m_parents.isEmpty())
        m_parents.takeFirst()->DownRef();

    while (!m_children.isEmpty())
        m_children.takeFirst()->DownRef();

    while (!m_blocks.isEmpty())
        m_blocks.takeFirst()->DownRef();

    while (!m_requires.isEmpty())
        m_requires.takeFirst()->DownRef();

    while (!m_requiredby.isEmpty())
        m_requiredby.takeFirst()->DownRef();
}

void CommandLineArg::PrintVerbose(void) const
{
    if (!m_given)
        return;

    cerr << "  " << m_name.leftJustified(30).toLocal8Bit().constData();

    QSize tmpsize;
    QMap<QString, QVariant> tmpmap;
    QMap<QString, QVariant>::const_iterator it;
    bool first;

    switch (m_type)
    {
      case QVariant::Bool:
        cerr << (m_stored.toBool() ? "True" : "False") << endl;
        break;

      case QVariant::Int:
        cerr << m_stored.toInt() << endl;
        break;

      case QVariant::UInt:
        cerr << m_stored.toUInt() << endl;
        break;

      case QVariant::LongLong:
        cerr << m_stored.toLongLong() << endl;
        break;

      case QVariant::Double:
        cerr << m_stored.toDouble() << endl;
        break;

      case QVariant::Size:
        tmpsize = m_stored.toSize();
        cerr <<  "x=" << tmpsize.width()
             << " y=" << tmpsize.height()
             << endl;
        break;

      case QVariant::String:
        cerr << '"' << m_stored.toString().toLocal8Bit().constData()
             << '"' << endl;
        break;

      case QVariant::StringList:
        cerr << '"' << m_stored.toStringList().join("\", \"")
                               .toLocal8Bit().constData()
             << '"' << endl;
        break;

      case QVariant::Map:
        tmpmap = m_stored.toMap();
        first = true;

        for (it = tmpmap.begin(); it != tmpmap.end(); it++)
        {
            if (first)
                first = false;
            else
                cerr << QString("").leftJustified(32)
                                   .toLocal8Bit().constData();

            cerr << it.key().toLocal8Bit().constData()
                 << '='
                 << (*it).toString().toLocal8Bit().constData()
                 << endl;
        }

        break;

      case QVariant::DateTime:
        cerr << m_stored.toDateTime().toString(Qt::ISODate)
                        .toLocal8Bit().constData()
             << endl;
        break;

      default:
        cerr << endl;
    }
}

MythCommandLineParser::MythCommandLineParser(QString appname) :
    m_appname(appname), m_passthroughActive(false),
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

MythCommandLineParser::~MythCommandLineParser()
{
    QMap<QString, CommandLineArg*>::iterator i;

    i = m_namedArgs.begin();
    while (i != m_namedArgs.end())
    {
        (*i)->DownRef();
        (*i)->CleanupLinks();
        i = m_namedArgs.erase(i);
    }

    i = m_optionedArgs.begin();
    while (i != m_optionedArgs.end())
    {
        (*i)->DownRef();
        i = m_optionedArgs.erase(i);
    }
}

CommandLineArg* MythCommandLineParser::add(QStringList arglist,
        QString name, QVariant::Type type, QVariant def,
        QString help, QString longhelp)
{
    CommandLineArg *arg;

    if (m_namedArgs.contains(name))
        arg = m_namedArgs[name];
    else
    {
        arg = new CommandLineArg(name, type, def, help, longhelp);
        m_namedArgs.insert(name, arg);
    }

    QStringList::const_iterator i;
    for (i = arglist.begin(); i != arglist.end(); ++i)
    {
        if (!m_optionedArgs.contains(*i))
        {
            arg->AddKeyword(*i);
            if (m_verbose)
                cerr << "Adding " << (*i).toLocal8Bit().constData()
                     << " as taking type '" << QVariant::typeToName(type)
                     << "'" << endl;
            arg->UpRef();
            m_optionedArgs.insert(*i, arg);
        }
    }

    return arg;
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
    QString help = GetHelpString();
    cerr << help.toLocal8Bit().constData();
}

QString MythCommandLineParser::GetHelpString(void) const
{
    QString helpstr;
    QTextStream msg(&helpstr, QIODevice::WriteOnly);

    QString versionStr = QString("%1 version: %2 [%3] www.mythtv.org")
        .arg(m_appname).arg(MYTH_SOURCE_PATH).arg(MYTH_SOURCE_VERSION);
    msg << versionStr << endl;

    QString descr = GetHelpHeader();
    if (descr.size() > 0)
        msg << endl << descr << endl << endl;

    if (toString("showhelp").isEmpty())
    {
        // build generic help text
        //msg << "Valid options are: " << endl;

        QStringList groups("");
        int maxlen = 0;
        QMap<QString, CommandLineArg*>::const_iterator i1;
        for (i1 = m_namedArgs.begin(); i1 != m_namedArgs.end(); ++i1)
        {
            maxlen = max((*i1)->GetKeywordLength(), maxlen);
            if (!groups.contains((*i1)->m_group))
                groups << (*i1)->m_group;
        }

        maxlen += 4;
        QStringList::const_iterator i2;
        for (i2 = groups.begin(); i2 != groups.end(); ++i2)
        {
            if ((*i2).isEmpty())
                msg << "Misc. Options:" << endl;
            else
                msg << (*i2).toLocal8Bit().constData() << " Options:" << endl;

            for (i1 = m_namedArgs.begin(); i1 != m_namedArgs.end(); ++i1)
                msg << (*i1)->GetHelpString(maxlen, *i2);
            msg << endl;
        }
    }
    else
    {
        // build help for a specific argument
        QString optstr = "-" + toString("showhelp");
        if (!m_optionedArgs.contains(optstr))
        {
            optstr = "-" + optstr;
            if (!m_optionedArgs.contains(optstr))
                return QString("Could not find option matching '%1'\n")
                            .arg(toString("showhelp"));
        }

        msg << m_optionedArgs[optstr]->GetLongHelpString(optstr);
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
    int res;
    QString opt, val;
    CommandLineArg *argdef;

    QMap<QString, CommandLineArg>::const_iterator i;
    for (int argpos = 1; argpos < argc; ++argpos)
    {

        res = getOpt(argc, argv, argpos, opt, val);

        if (m_verbose)
            cerr << "res: " << NamedOptType(res) << endl
                 << "opt:  " << opt.toLocal8Bit().constData() << endl
                 << "val:  " << val.toLocal8Bit().constData() << endl << endl;

        if (res == kPassthrough && !m_namedArgs.contains("_passthrough"))
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
            m_namedArgs["_passthrough"]->Set("", val);
            continue;
        }
        else if (res == kArg)
        {
            if (!m_namedArgs.contains("_args"))
            {
                cerr << "Received '"
                     << val.toAscii().constData()
                     << "' but unassociated arguments have not been enabled"
                     << endl;
                return false;        
            }

            m_namedArgs["_args"]->Set("", val);
            continue;
        }

        // this line should not be passed once arguments have started collecting
        if (toBool("_args"))
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

        if (!m_optionedArgs.contains(opt))
        {
            // argument is unhandled, check if parser allows arbitrary input
            if (m_namedArgs.contains("_extra"))
            {
                // arbitrary allowed, specify general collection pool
                argdef = m_namedArgs["_extra"];
                QString tmp = QString("%1=%2").arg(opt).arg(val);
                val = tmp;
                res = kOptVal;
            }
            else
            {
                // arbitrary not allowed, fault out
                cerr << "Unhandled option given on command line:" << endl 
                     << "    " << opt.toLocal8Bit().constData() << endl;
                return false;
            }
        }
        else
            argdef = m_optionedArgs[opt];

        if (m_verbose)
            cerr << "name: " << argdef->GetName().toLocal8Bit().constData()
                 << endl;

        if (res == kOptOnly)
        {
            if (!argdef->Set(opt))
                return false;
        }
        else if (res == kOptVal)
        {
            if (!argdef->Set(opt, val))
                return false;
        }
        else
            return false; // this should not occur

        if (m_verbose)
            cerr << "value: " << argdef->m_stored.toString().toLocal8Bit().constData()
                 << endl;
    }
 
    if (m_verbose)
    {
        cerr << "Processed option list:" << endl;
        QMap<QString, CommandLineArg*>::const_iterator it;
        for (it = m_namedArgs.begin(); it != m_namedArgs.end(); ++it)
            (*it)->PrintVerbose();

        if (m_namedArgs.contains("_args"))
        {
            cerr << endl << "Extra argument list:" << endl;
            QStringList slist = toStringList("_args");
            QStringList::const_iterator it2 = slist.begin();
            for (; it2 != slist.end(); ++it2)
                cerr << "  " << (*it2).toLocal8Bit().constData() << endl;
        }

        if (m_namedArgs.contains("_passthrough"))
        {
            cerr << endl << "Passthrough string:" << endl;
            cerr << "  " << GetPassthrough().toLocal8Bit().constData() << endl;
        }

        cerr << endl;
    }

    return ReconcileLinks();
}

bool MythCommandLineParser::ReconcileLinks(void)
{
    QList<CommandLineArg*> links;
    QMap<QString,CommandLineArg*>::iterator i1;
    QList<CommandLineArg*>::iterator i2;

    if (m_verbose)
        cerr << "Reconciling links for option interdependencies." << endl;

    for (i1 = m_namedArgs.begin(); i1 != m_namedArgs.end(); ++i1)
    {
        links = (*i1)->m_parents;
        for (i2 = links.begin(); i2 != links.end(); ++i2)
        {
            if ((*i2)->m_type != QVariant::Invalid)
                continue; // already handled

            if (!m_namedArgs.contains((*i2)->m_name))
            {
                // not found
                cerr << "ERROR: could not reconcile linked argument." << endl
                     << "  '" << (*i1)->m_name.toLocal8Bit().constData()
                     << "' could not find '"
                     << (*i2)->m_name.toLocal8Bit().constData() << "'." << endl
                     << "  Please resolve dependency and recompile." << endl;
                return false;
            }

            // replace linked argument
            if (m_verbose)
                cerr << QString("  Setting %1 as child of %2")
                            .arg((*i1)->m_name).arg((*i2)->m_name)
                            .toLocal8Bit().constData()
                     << endl;
            (*i1)->SetChildOf(m_namedArgs[(*i2)->m_name]);
        }

        links = (*i1)->m_children;
        for (i2 = links.begin(); i2 != links.end(); ++i2)
        {
            if ((*i2)->m_type != QVariant::Invalid)
                continue; // already handled

            if (!m_namedArgs.contains((*i2)->m_name))
            {
                // not found
                cerr << "ERROR: could not reconcile linked argument." << endl
                     << "  '" << (*i1)->m_name.toLocal8Bit().constData()
                     << "' could not find '"
                     << (*i2)->m_name.toLocal8Bit().constData() << "'." << endl
                     << "  Please resolve dependency and recompile." << endl;
                return false;
            }

            // replace linked argument
            if (m_verbose)
                cerr << QString("  Setting %1 as parent of %2")
                            .arg((*i1)->m_name).arg((*i2)->m_name)
                            .toLocal8Bit().constData()
                     << endl;
            (*i1)->SetParentOf(m_namedArgs[(*i2)->m_name]);
        }

        links = (*i1)->m_requires;
        for (i2 = links.begin(); i2 != links.end(); ++i2)
        {
            if ((*i2)->m_type != QVariant::Invalid)
                continue; // already handled

            if (!m_namedArgs.contains((*i2)->m_name))
            {
                // not found
                cerr << "ERROR: could not reconcile linked argument." << endl
                     << "  '" << (*i1)->m_name.toLocal8Bit().constData()
                     << "' could not find '"
                     << (*i2)->m_name.toLocal8Bit().constData() << "'." << endl
                     << "  Please resolve dependency and recompile." << endl;
                return false;
            }

            // replace linked argument
            if (m_verbose)
                cerr << QString("  Setting %1 as requiring %2")
                            .arg((*i1)->m_name).arg((*i2)->m_name)
                            .toLocal8Bit().constData()
                     << endl;
            (*i1)->SetRequires(m_namedArgs[(*i2)->m_name]);
        }

        i2 = (*i1)->m_requiredby.begin();
        while (i2 != (*i1)->m_requiredby.end())
        {
            if ((*i2)->m_type == QVariant::Invalid)
            {
                // if its not an invalid, it shouldnt be here anyway
                if (m_namedArgs.contains((*i2)->m_name))
                {
                    m_namedArgs[(*i2)->m_name]->SetRequires(*i1);
                    if (m_verbose)
                        cerr << QString("  Setting %1 as blocking %2")
                                    .arg((*i1)->m_name).arg((*i2)->m_name)
                                    .toLocal8Bit().constData()
                             << endl;
                }
            }

            (*i2)->DownRef();
            i2 = (*i1)->m_requiredby.erase(i2);
        }

        i2 = (*i1)->m_blocks.begin();
        while (i2 != (*i1)->m_blocks.end())
        {
            if ((*i2)->m_type != QVariant::Invalid)
            {
                i2++;
                continue; // already handled
            }

            if (!m_namedArgs.contains((*i2)->m_name))
            {
                (*i2)->DownRef();
                i2 = (*i1)->m_blocks.erase(i2);
                continue; // if it doesnt exist, it cant block this command
            }

            // replace linked argument
            if (m_verbose)
                cerr << QString("  Setting %1 as blocking %2")
                            .arg((*i1)->m_name).arg((*i2)->m_name)
                            .toLocal8Bit().constData()
                     << endl;
            (*i1)->SetBlocks(m_namedArgs[(*i2)->m_name]);
            i2++;
        }
    }

    for (i1 = m_namedArgs.begin(); i1 != m_namedArgs.end(); ++i1)
    {
        if (!(*i1)->TestLinks())
            return false;
    }

    return true;
}

QVariant MythCommandLineParser::operator[](const QString &name)
{
    QVariant var("");
    if (!m_namedArgs.contains(name))
        return var;

    CommandLineArg *arg = m_namedArgs[name];

    if (arg->m_given)
        var = arg->m_stored;
    else
        var = arg->m_default;

    return var;
}

QStringList MythCommandLineParser::GetArgs(void) const
{
    return toStringList("_args");
}

QString MythCommandLineParser::GetPassthrough(void) const
{
    return toStringList("_passthrough").join(" ");
}

QMap<QString,QString> MythCommandLineParser::GetSettingsOverride(void)
{
    QMap<QString,QString> smap = toMap("overridesettings");

    if (!m_overridesImported)
    {
        if (toBool("overridesettingsfile"))
        {
            QString filename = toString("overridesettingsfile");
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
                                smap[tokens[0]] = tokens[1];
                        }
                        len = f.readLine(buf, sizeof(buf) - 1);
                    }
                }
                else
                {
                    QByteArray tmp = filename.toAscii();
                    cerr << "Failed to open the override settings file: '"
                         << tmp.constData() << "'" << endl;
                }
            }
        }

        if (toBool("windowed"))
            smap["RunFrontendInWindow"] = "1";
        else if (toBool("notwindowed"))
            smap["RunFrontendInWindow"] = "0";

        if (toBool("mousecursor"))
            smap["HideMouseCursor"] = "0";
        else if (toBool("nomousecursor"))
            smap["HideMouseCursor"] = "1";

        m_overridesImported = true;

        if (!smap.isEmpty())
        {
            QVariantMap vmap;
            QMap<QString, QString>::const_iterator it;
            for (it = smap.begin(); it != smap.end(); ++it)
                vmap[it.key()] = QVariant(it.value());

            m_namedArgs["overridesettings"]->Set(QVariant(vmap));
        }
    }

    if (m_verbose)
    {
        cerr << "Option Overrides:" << endl;
        QMap<QString, QString>::const_iterator it;
        for (it = smap.constBegin(); it != smap.constEnd(); ++it)
            cerr << QString("    %1 - %2").arg(it.key(), 30).arg(*it)
                        .toLocal8Bit().constData() << endl;
    }

    return smap;
}

bool MythCommandLineParser::toBool(QString key) const
{
    if (!m_namedArgs.contains(key))
        return false;

    CommandLineArg *arg = m_namedArgs[key];

    if (arg->m_type == QVariant::Bool)
    {
        if (arg->m_given)
            return arg->m_stored.toBool();
        return arg->m_default.toBool();
    }

    if (arg->m_given)
        return true;

    return false;
}

int MythCommandLineParser::toInt(QString key) const
{
    int val = 0;
    if (!m_namedArgs.contains(key))
        return val;

    CommandLineArg *arg = m_namedArgs[key];

    if (arg->m_given)
    {
        if (arg->m_stored.canConvert(QVariant::Int))
            val = arg->m_stored.toInt();
    }
    else
    {
        if (arg->m_default.canConvert(QVariant::Int))
            val = arg->m_default.toInt();
    }

    return val;
}

uint MythCommandLineParser::toUInt(QString key) const
{
    uint val = 0;
    if (!m_namedArgs.contains(key))
        return val;

    CommandLineArg *arg = m_namedArgs[key];

    if (arg->m_given)
    {
        if (arg->m_stored.canConvert(QVariant::UInt))
            val = arg->m_stored.toUInt();
    }
    else
    {
        if (arg->m_default.canConvert(QVariant::UInt))
            val = arg->m_default.toUInt();
    }

    return val;
}

long long MythCommandLineParser::toLongLong(QString key) const
{
    long long val = 0;
    if (!m_namedArgs.contains(key))
        return val;

    CommandLineArg *arg = m_namedArgs[key];

    if (arg->m_given)
    {
        if (arg->m_stored.canConvert(QVariant::LongLong))
            val = arg->m_stored.toLongLong();
    }
    else
    {
        if (arg->m_default.canConvert(QVariant::LongLong))
            val = arg->m_default.toLongLong();
    }

    return val;
}

double MythCommandLineParser::toDouble(QString key) const
{
    double val = 0.0;
    if (!m_namedArgs.contains(key))
        return val;

    CommandLineArg *arg = m_namedArgs[key];

    if (arg->m_given)
    {
        if (arg->m_stored.canConvert(QVariant::Double))
            val = arg->m_stored.toDouble();
    }
    else
    {
        if (arg->m_default.canConvert(QVariant::Double))
            val = arg->m_default.toDouble();
    }

    return val;
}

QSize MythCommandLineParser::toSize(QString key) const
{
    QSize val(0,0);
    if (!m_namedArgs.contains(key))
        return val;

    CommandLineArg *arg = m_namedArgs[key];

    if (arg->m_given)
    {
        if (arg->m_stored.canConvert(QVariant::Size))
            val = arg->m_stored.toSize();
    }
    else
    {
        if (arg->m_default.canConvert(QVariant::Size))
            val = arg->m_default.toSize();
    }

    return val;
}

QString MythCommandLineParser::toString(QString key) const
{
    QString val("");
    if (!m_namedArgs.contains(key))
        return val;

    CommandLineArg *arg = m_namedArgs[key];

    if (arg->m_given)
    {
        if (arg->m_stored.canConvert(QVariant::String))
            val = arg->m_stored.toString();
    }
    else
    {
        if (arg->m_default.canConvert(QVariant::String))
            val = arg->m_default.toString();
    }

    return val;
}

QStringList MythCommandLineParser::toStringList(QString key, QString sep) const
{
    QVariant varval;
    QStringList val;
    if (!m_namedArgs.contains(key))
        return val;

    CommandLineArg *arg = m_namedArgs[key];

    if (arg->m_given)
        varval = arg->m_stored;
    else
        varval = arg->m_default;

    if (arg->m_type == QVariant::String && !sep.isEmpty())
        val = varval.toString().split(sep);
    else if (varval.canConvert(QVariant::StringList))
        val = varval.toStringList();

    return val;
}

QMap<QString,QString> MythCommandLineParser::toMap(QString key) const
{
    QMap<QString, QString> val;
    QMap<QString, QVariant> tmp;
    if (!m_namedArgs.contains(key))
        return val;

    CommandLineArg *arg = m_namedArgs[key];

    if (arg->m_given)
    {
        if (arg->m_stored.canConvert(QVariant::Map))
            tmp = arg->m_stored.toMap();
    }
    else
    {
        if (arg->m_default.canConvert(QVariant::Map))
            tmp = arg->m_default.toMap();
    }

    QMap<QString, QVariant>::const_iterator i;
    for (i = tmp.begin(); i != tmp.end(); ++i)
        val[i.key()] = i.value().toString();

    return val;
}

QDateTime MythCommandLineParser::toDateTime(QString key) const
{
    QDateTime val;
    if (!m_namedArgs.contains(key))
        return val;

    CommandLineArg *arg = m_namedArgs[key];

    if (arg->m_given)
    {
        if (arg->m_stored.canConvert(QVariant::DateTime))
            val = arg->m_stored.toDateTime();
    }
    else
    {
        if (arg->m_default.canConvert(QVariant::DateTime))
            val = arg->m_default.toDateTime();
    }

    return val;
}

void MythCommandLineParser::allowArgs(bool allow)
{
    if (m_namedArgs.contains("_args"))
    {
        if (!allow)
            m_namedArgs.remove("_args");
    }
    else if (!allow)
        return;

    CommandLineArg *arg = new CommandLineArg("_args", QVariant::StringList,
                                             QStringList());
    m_namedArgs["_args"] = arg;
}

void MythCommandLineParser::allowExtras(bool allow)
{
    if (m_namedArgs.contains("_extra"))
    {
        if (!allow)
            m_namedArgs.remove("_extra");
    }
    else if (!allow)
        return;

    QMap<QString,QVariant> vmap;
    CommandLineArg *arg = new CommandLineArg("_extra", QVariant::Map, vmap);

    m_namedArgs["_extra"] = arg;
}

void MythCommandLineParser::allowPassthrough(bool allow)
{
    if (m_namedArgs.contains("_passthrough"))
    {
        if (!allow)
            m_namedArgs.remove("_passthrough");
    }
    else if (!allow)
        return;

    CommandLineArg *arg = new CommandLineArg("_passthrough",
                                    QVariant::StringList, QStringList());
    m_namedArgs["_passthrough"] = arg;
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

void MythCommandLineParser::addWindowed(void)
{
    add(QStringList( QStringList() << "-nw" << "--no-windowed" ),
            "notwindowed", false, 
            "Prevent application from running in window.", "")
        ->SetBlocks("windowed")
        ->SetGroup("User Interface");

    add(QStringList( QStringList() << "-w" << "--windowed" ), "windowed", 
            false, "Force application to run in a window.", "")
        ->SetGroup("User Interface");
}

void MythCommandLineParser::addMouse(void)
{
    add("--mouse-cursor", "mousecursor", false,
            "Force visibility of the mouse cursor.", "")
        ->SetBlocks("nomousecursor")
        ->SetGroup("User Interface");

    add("--no-mouse-cursor", "nomousecursor", false,
            "Force the mouse cursor to be hidden.", "")
        ->SetGroup("User Interface");
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
            "Specify chanid of recording to operate on.", "")
        ->SetRequires("starttime");

    add("--starttime", "starttime", QDateTime(),
            "Specify start time of recording to operate on.", "");
}

void MythCommandLineParser::addGeometry(void)
{
    add(QStringList( QStringList() << "-geometry" << "--geometry" ), "geometry",
            "", "Specify window size and position (WxH[+X+Y])", "")
        ->SetGroup("User Interface");
}

void MythCommandLineParser::addDisplay(void)
{
#ifdef CONFIG_X11
    add("-display", "display", "", "Specify X server to use.", "")
        ->SetGroup("User Interface");
#endif
}

void MythCommandLineParser::addUPnP(void)
{
    add("--noupnp", "noupnp", false, "Disable use of UPnP.", "");
}

void MythCommandLineParser::addLogging(
    const QString &defaultVerbosity, LogLevel_t defaultLogLevel)
{
    defaultLogLevel =
        ((defaultLogLevel >= LOG_UNKNOWN) || (defaultLogLevel <= LOG_ANY)) ?
        LOG_INFO : defaultLogLevel;

    QString logLevelStr = logLevelGetName(defaultLogLevel);

    add(QStringList( QStringList() << "-v" << "--verbose" ), "verbose",
        defaultVerbosity,
        "Specify log filtering. Use '-v help' for level info.", "")
                ->SetGroup("Logging");
    add("-V", "verboseint", 0U, "",
        "This option is intended for internal use only.\n"
        "This option takes an unsigned value corresponding "
        "to the bitwise log verbosity operator.")
                ->SetGroup("Logging");
    add(QStringList( QStringList() << "-l" << "--logfile" << "--logpath" ), 
        "logpath", "",
        "Writes logging messages to a file at logpath.\n"
        "If a directory is given, a logfile will be created in that "
        "directory with a filename of applicationName.date.pid.log.\n"
        "If a full filename is given, that file will be used.\n"
        "This is typically used in combination with --daemon, and if used "
        "in combination with --pidfile, this can be used with log "
        "rotators, using the HUP call to inform MythTV to reload the "
        "file (currently disabled).", "")
                ->SetGroup("Logging");
    add(QStringList( QStringList() << "-q" << "--quiet"), "quiet", 0,
        "Don't log to the console (-q).  Don't log anywhere (-q -q)", "")
                ->SetGroup("Logging");
    add("--loglevel", "loglevel", logLevelStr, 
        QString(
            "Set the logging level.  All log messages at lower levels will be "
            "discarded.\n"
            "In descending order: emerg, alert, crit, err, warning, notice, "
            "info, debug\ndefaults to ") + logLevelStr, "")
                ->SetGroup("Logging");
    add("--syslog", "syslog", "none", 
        "Set the syslog logging facility.\nSet to \"none\" to disable, "
        "defaults to none", "")
                ->SetGroup("Logging");
    add("--nodblog", "nodblog", false, "Disable database logging.", "")
                ->SetGroup("Logging");
}

void MythCommandLineParser::addPIDFile(void)
{
    add(QStringList( QStringList() << "-p" << "--pidfile" ), "pidfile", "",
            "Write PID of application to filename.",
            "Write the PID of the currently running process as a single "
            "line to this file. Used for init scripts to know what "
            "process to terminate, and with --logfile and log rotators "
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
        SetValue("islogpath", true);
        logdir  = finfo.filePath();
        logfile = QCoreApplication::applicationName() + "." +
                  QDateTime::currentDateTime().toString("yyyyMMddhhmmss") +
                  QString(".%1").arg(pid) + ".log";
    }
    else
    {
        SetValue("islogpath", false);
        logdir  = finfo.path();
        logfile = finfo.fileName();
    }

    SetValue("logdir", logdir);
    SetValue("logfile", logfile);
    SetValue("filepath", QFileInfo(QDir(logdir), logfile).filePath());

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
    CommandLineArg *arg;

    if (!m_namedArgs.contains(key))
    {
        QVariant val(value);
        arg = new CommandLineArg(key, val.type(), val);
        m_namedArgs.insert(key, arg);
    }
    else
    {
        arg = m_namedArgs[key];
        if (arg->m_type != value.type())
            return false;
    }

    arg->Set(value);
    return true;
}

int MythCommandLineParser::ConfigureLogging(QString mask, unsigned int progress)
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

    int quiet = toUInt("quiet");
    if (max(quiet, (int)progress) > 1)
    {
        verboseMask = VB_NONE;
        verboseArgParse("none");
    }

    int facility = GetSyslogFacility();
    bool dblog = !toBool("nodblog");
    LogLevel_t level = GetLogLevel();
    if (level == LOG_UNKNOWN)
        return GENERIC_EXIT_INVALID_CMDLINE;

    LOG(VB_GENERAL, LOG_CRIT,
        QString("%1 version: %2 [%3] www.mythtv.org")
        .arg(QCoreApplication::applicationName())
        .arg(MYTH_SOURCE_PATH).arg(MYTH_SOURCE_VERSION));
    LOG(VB_GENERAL, LOG_NOTICE,
        QString("Enabled verbose msgs: %1").arg(verboseString));

    QString logfile = GetLogFilePath();
    bool propagate = toBool("islogpath");

    if (toBool("daemon"))
        quiet = max(quiet, 1);

    logStart(logfile, progress, quiet, facility, level, dblog, propagate);

    return GENERIC_EXIT_OK;
}

// WARNING: this must not be called until after MythContext is initialized
void MythCommandLineParser::ApplySettingsOverride(void)
{
    cerr << "Applying settings override" << endl;

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
#if defined(__linux__) || defined(__LINUX__)
    // Check the current dumpability of core dumps, which will be disabled
    // by setuid, so we can re-enable, if appropriate
    int dumpability = prctl(PR_GET_DUMPABLE);
#endif
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
#if defined(__linux__) || defined(__LINUX__)
        if (dumpability && (prctl(PR_SET_DUMPABLE, dumpability) == -1))
            LOG(VB_GENERAL, LOG_WARNING, "Unable to re-enable core file "
                    "creation. Run without the --user argument to use "
                    "shell-specified limits.");
#endif
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
