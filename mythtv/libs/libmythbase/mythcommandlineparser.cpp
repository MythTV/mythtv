/* -*- Mode: c++ -*-
*
* Class CommandLineArg
* Class MythCommandLineParser
*
* Copyright (C) Raymond Wagner 2011
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
*/

// C headers
#include <stdlib.h>
#include <stdio.h>

// POSIX headers
#include <unistd.h>
#include <signal.h>

// System headers
#include <sys/types.h>
#ifndef _WIN32
#include <sys/ioctl.h>
#include <pwd.h>
#include <grp.h>
#if defined(__linux__) || defined(__LINUX__)
#include <sys/prctl.h>
#endif
#endif

// C++ headers
#include <algorithm>
#include <iostream>
#include <fstream>
using namespace std;

// Qt headers
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
#include "mythmiscutil.h"
#include "mythdate.h"

#define TERMWIDTH 79

const int kEnd          = 0,
          kEmpty        = 1,
          kOptOnly      = 2,
          kOptVal       = 3,
          kCombOptVal   = 4,
          kArg          = 5,
          kPassthrough  = 6,
          kInvalid      = 7;

const char* NamedOptType(int type);
bool openPidfile(ofstream &pidfs, const QString &pidfile);
bool setUser(const QString &username);
int GetTermWidth(void);

/** \fn GetTermWidth(void)
 *  \brief returns terminal width, or 79 on error
 */
int GetTermWidth(void)
{
#if defined(_WIN32) || defined(Q_OS_ANDROID)
    return TERMWIDTH;
#else
    struct winsize ws;

    if (ioctl(0, TIOCGWINSZ, &ws) != 0)
        return TERMWIDTH;

    return (int)ws.ws_col;
#endif
}

/** \fn NamedOptType
 *  \brief Return character string describing type of result from parser pass
 */
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

      case kCombOptVal:
        return "kCombOptVal";

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

/** \defgroup commandlineparser Command Line Processing
 *  \ingroup libmythbase
 *  \brief Utility responsible for processing arguments from the command line
 *
 *  This fundamental design for this utility is a class that can be modularly
 *  configured with different optional arguments and behaviors, let process
 *  the received input arguments, and then persist for the results to be read
 *  out as needed.
 *
 *  In typical use, one will subclass MythCommandLineParser() and overwrite
 *  the LoadArguments() and GetHelpHeader() virtual functions. LoadArguments()
 *  is a convenient place to define default behaviors and accepted arguments.
 *  GetHelpHeader() is called for text describing the application, used when
 *  calling the '--help' argument. This utility will automatically handle help
 *  output, as well as check relationships between arguments.
 */

/** \class CommandLineArg
 *  \ingroup commandlineparser
 *  \brief Definition for a single command line option
 *
 *  This class contains instructions for the command line parser about what
 *  options to process from the command line. Each instance can correspond
 *  to multiple argument keywords, and stores a default value, whether it
 *  has been supplied, help text, and optional interdependencies with other
 *  CommandLineArgs.
 */

/** \brief Default constructor for CommandLineArg class
 *
 *  This constructor is for use with command line parser, defining an option
 *  that can be used on the command line, and should be reported in --help
 *  printouts
 */
CommandLineArg::CommandLineArg(QString name, QVariant::Type type,
                   QVariant def, QString help, QString longhelp) :
    ReferenceCounter(QString("CommandLineArg:%1").arg(name)),
    m_given(false), m_converted(false), m_name(name),
    m_group(""), m_deprecated(""), m_removed(""), m_removedversion(""),
    m_type(type), m_default(def), m_help(help), m_longhelp(longhelp)
{
    if ((m_type != QVariant::String) && (m_type != QVariant::StringList) &&
            (m_type != QVariant::Map))
        m_converted = true;
}

/** \brief Reduced constructor for CommandLineArg class
 *
 *  This constructor is for internal use within the command line parser. It
 *  is intended for use in supplementary data storage for information not
 *  supplied directly on the command line.
 */
CommandLineArg::CommandLineArg(QString name, QVariant::Type type, QVariant def)
  : ReferenceCounter(QString("CommandLineArg:%1").arg(name)),
    m_given(false), m_converted(false), m_name(name),
    m_group(""), m_deprecated(""), m_removed(""), m_removedversion(""),
    m_type(type), m_default(def)
{
    if ((m_type != QVariant::String) && (m_type != QVariant::StringList) &&
            (m_type != QVariant::Map))
        m_converted = true;
}

/** \brief Dummy constructor for CommandLineArg class
 *
 *  This constructor is for internal use within the command line parser. It
 *  is used as a placeholder for defining relations between different command
 *  line arguments, and is reconciled with the proper argument of the same
 *  name prior to parsing inputs.
 */
CommandLineArg::CommandLineArg(QString name) :
    ReferenceCounter(QString("CommandLineArg:%1").arg(name)),
    m_given(false), m_converted(false), m_name(name),
    m_deprecated(""), m_removed(""), m_removedversion(""),
    m_type(QVariant::Invalid)
{
}

/** \brief Return string containing all possible keyword triggers for this
 *         argument
 */
QString CommandLineArg::GetKeywordString(void) const
{
    // this may cause problems if the terminal is too narrow, or if too
    // many keywords for the same argument are used
    return m_keywords.join(" OR ");
}

/** \brief Return length of full keyword string for use in determining indent
 *         of help text
 */
int CommandLineArg::GetKeywordLength(void) const
{
    int len = GetKeywordString().length();

    QList<CommandLineArg*>::const_iterator i1;
    for (i1 = m_parents.begin(); i1 != m_parents.end(); ++i1)
        len = max(len, (*i1)->GetKeywordLength()+2);

    return len;
}

/** \brief Return string containing help text with desired offset
 *
 *  This function returns a string containing all usable keywords and the
 *  shortened help text, for use with the general help printout showing all
 *  options. It automatically accounts for terminal width, and wraps the text
 *  accordingly.
 *
 *  The group option acts as a filter, only returning text if the argument is
 *  part of the group the parser is currently printing options for.
 *
 *  Child arguments will not produce help text on their own, but only indented
 *  beneath each of the marked parent arguments. The force option specifies
 *  that the function is being called by the parent argument, and help should
 *  be output.
 */
QString CommandLineArg::GetHelpString(int off, QString group, bool force) const
{
    QString helpstr;
    QTextStream msg(&helpstr, QIODevice::WriteOnly);
    int termwidth = GetTermWidth();
    if (termwidth < off)
    {
        if (off > 70)
            // developer has configured some absurdly long command line
            // arguments, but we still need to do something
            termwidth = off+40;
        else
            // user is running uselessly narrow console, use a sane console
            // width instead
            termwidth = 79;
    }

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

    if (!m_deprecated.isEmpty())
        // option is marked as deprecated, do not show
        return helpstr;

    if (!m_removed.isEmpty())
        // option is marked as removed, do not show
        return helpstr;

    QString pad;
    pad.fill(' ', off);

    // print the first line with the available keywords
    QStringList hlist = m_help.split('\n');
    wrapList(hlist, termwidth-off);
    if (!m_parents.isEmpty())
        msg << "  ";
    msg << GetKeywordString().leftJustified(off, ' ')
        << hlist[0] << endl;

    // print remaining lines with necessary padding
    QStringList::const_iterator i1;
    for (i1 = hlist.begin() + 1; i1 != hlist.end(); ++i1)
        msg << pad << *i1 << endl;

    // loop through any child arguments to print underneath
    QList<CommandLineArg*>::const_iterator i2;
    for (i2 = m_children.begin(); i2 != m_children.end(); ++i2)
        msg << (*i2)->GetHelpString(off, group, true);

    msg.flush();
    return helpstr;
}

/** \brief Return string containing extended help text
 *
 *  This function returns a longer version of the help text than that provided
 *  with the list of arguments, intended for more detailed, specific
 *  information. This also documents the type of argument it takes, default
 *  value, and any relational dependencies with other arguments it might have.
 */
QString CommandLineArg::GetLongHelpString(QString keyword) const
{
    QString helpstr;
    QTextStream msg(&helpstr, QIODevice::WriteOnly);
    int termwidth = GetTermWidth();

    // help called for an argument that is not me, this should not happen
    if (!m_keywords.contains(keyword))
        return helpstr;

    // argument has been marked as removed, so warn user of such
    if (!m_removed.isEmpty())
        PrintRemovedWarning(keyword);
    // argument has been marked as deprecated, so warn user of such
    else if (!m_deprecated.isEmpty())
        PrintDeprecatedWarning(keyword);

    msg << "Option:      " << keyword << endl << endl;

    bool first = true;

    // print all related keywords, padding for multiples
    QStringList::const_iterator i1;
    for (i1 = m_keywords.begin(); i1 != m_keywords.end(); ++i1)
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
    }

    // print type and default for the stored value
    msg << "Type:        " << QVariant::typeToName(m_type) << endl;
    if (m_default.canConvert(QVariant::String))
        msg << "Default:     " << m_default.toString() << endl;

    QStringList help;
    if (m_longhelp.isEmpty())
        help = m_help.split("\n");
    else
        help = m_longhelp.split("\n");
    wrapList(help, termwidth-13);

    // print description, wrapping and padding as necessary
    msg << "Description: " << help[0] << endl;
    for (i1 = help.begin() + 1; i1 != help.end(); ++i1)
        msg << "             " << *i1 << endl;

    QList<CommandLineArg*>::const_iterator i2;

    // loop through the four relation types and print
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

/** \brief Set option as provided on command line with no value
 *
 *  This specifies that an option is given, but there is no corresponding
 *  value, meaning this can only be used on a boolean, integer, and string
 *  arguments. All other will return false.
 */
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
            m_stored = QVariant(1);
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

/** \brief Set option as provided on command line with value
 */
bool CommandLineArg::Set(QString opt, QByteArray val)
{
    QVariantList vlist;
    QList<QByteArray> blist;
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
        m_stored = QVariant(MythDate::fromString(QString(val)));
        break;

      case QVariant::StringList:
        if (!m_stored.isNull())
            vlist = m_stored.toList();
        vlist << val;
        m_stored = QVariant(vlist);
        break;

      case QVariant::Map:
        if (!val.contains('='))
        {
            cerr << "Command line option did not get expected "
                 << "key/value pair" << endl;
            return false;
        }

        blist = val.split('=');

        if (!m_stored.isNull())
            vmap = m_stored.toMap();
        vmap[QString(blist[0])] = QVariant(blist[1]);
        m_stored = QVariant(vmap);
        break;

      case QVariant::Size:
        if (!val.contains('x'))
        {
            cerr << "Command line option did not get expected "
                 << "XxY pair" << endl;
            return false;
        }

        blist = val.split('x');
        m_stored = QVariant(QSize(blist[0].toInt(), blist[1].toInt()));
        break;

      default:
        m_stored = QVariant(val);
    }

    m_given = true;
    return true;
}

/** \brief Set argument as parent of given child
 */
CommandLineArg* CommandLineArg::SetParentOf(QString opt)
{
    m_children << new CommandLineArg(opt);
    return this;
}

/** \brief Set argument as parent of multiple children
 */
CommandLineArg* CommandLineArg::SetParentOf(QStringList opts)
{
    QStringList::const_iterator i = opts.begin();
    for (; i != opts.end(); ++i)
        m_children << new CommandLineArg(*i);
    return this;
}

/** \brief Set argument as child of given parent
 */
CommandLineArg* CommandLineArg::SetParent(QString opt)
{
    m_parents << new CommandLineArg(opt);
    return this;
}

/** \brief Set argument as child of multiple parents
 */
CommandLineArg* CommandLineArg::SetParent(QStringList opts)
{
    QStringList::const_iterator i = opts.begin();
    for (; i != opts.end(); ++i)
        m_parents << new CommandLineArg(*i);
    return this;
}

/** \brief Set argument as child of given parent
 */
CommandLineArg* CommandLineArg::SetChildOf(QString opt)
{
    m_parents << new CommandLineArg(opt);
    return this;
}

/** \brief Set argument as child of multiple parents
 */
CommandLineArg* CommandLineArg::SetChildOf(QStringList opts)
{
    QStringList::const_iterator i = opts.begin();
    for (; i != opts.end(); ++i)
        m_parents << new CommandLineArg(*i);
    return this;
}

/** \brief Set argument as parent of given child
 */
CommandLineArg* CommandLineArg::SetChild(QString opt)
{
    m_children << new CommandLineArg(opt);
    return this;
}

/** \brief Set argument as parent of multiple children
 */
CommandLineArg* CommandLineArg::SetChild(QStringList opts)
{
    QStringList::const_iterator i = opts.begin();
    for (; i != opts.end(); ++i)
        m_children << new CommandLineArg(*i);
    return this;
}

/** \brief Set argument as parent of given child and mark as required
 */
CommandLineArg* CommandLineArg::SetRequiredChild(QString opt)
{
    m_children << new CommandLineArg(opt);
    m_requires << new CommandLineArg(opt);
    return this;
}

/** \brief Set argument as parent of multiple children and mark as required
 */
CommandLineArg* CommandLineArg::SetRequiredChild(QStringList opts)
{
    QStringList::const_iterator i = opts.begin();
    for (; i != opts.end(); ++i)
    {
        m_children << new CommandLineArg(*i);
        m_requires << new CommandLineArg(*i);
    }
    return this;
}

/** \brief Set argument as child required by given parent
 */
CommandLineArg* CommandLineArg::SetRequiredChildOf(QString opt)
{
    m_parents << new CommandLineArg(opt);
    m_requiredby << new CommandLineArg(opt);
    return this;
}

/** \brief Set argument as child required by multiple parents
 */
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

/** \brief Set argument as requiring given option
 */
CommandLineArg* CommandLineArg::SetRequires(QString opt)
{
    m_requires << new CommandLineArg(opt);
    return this;
}

/** \brief Set argument as requiring multiple options
 */
CommandLineArg* CommandLineArg::SetRequires(QStringList opts)
{
    QStringList::const_iterator i = opts.begin();
    for (; i != opts.end(); ++i)
        m_requires << new CommandLineArg(*i);
    return this;
}

/** \brief Set argument as incompatible with given option
 */
CommandLineArg* CommandLineArg::SetBlocks(QString opt)
{
    m_blocks << new CommandLineArg(opt);
    return this;
}

/** \brief Set argument as incompatible with multiple options
 */
CommandLineArg* CommandLineArg::SetBlocks(QStringList opts)
{
    QStringList::const_iterator i = opts.begin();
    for (; i != opts.end(); ++i)
        m_blocks << new CommandLineArg(*i);
    return this;
}

/** \brief Set option as deprecated
 */
CommandLineArg* CommandLineArg::SetDeprecated(QString depstr)
{
    if (depstr.isEmpty())
        depstr = "and will be removed in a future version.";
    m_deprecated = depstr;
    return this;
}

/** \brief Set option as removed
 */
CommandLineArg* CommandLineArg::SetRemoved(QString remstr, QString remver)
{
    if (remstr.isEmpty())
        remstr = "and is no longer available in this version.";
    m_removed = remstr;
    m_removedversion = remver;
    return this;
}

/** \brief Internal use, set argument as parent of given child
 *
 *  This option is intended for internal use only, as part of reconciling
 *  dummy options with their matched real counterparts.
 */
void CommandLineArg::SetParentOf(CommandLineArg *other, bool forward)
{
    int i;
    bool replaced = false;
    other->IncrRef();

    for (i = 0; i < m_children.size(); i++)
    {
        if (m_children[i]->m_name == other->m_name)
        {
            m_children[i]->DecrRef();
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

/** \brief Internal use, set argument as child of given parent
 *
 *  This option is intended for internal use only, as part of reconciling
 *  dummy options with their matched real counterparts.
 */
void CommandLineArg::SetChildOf(CommandLineArg *other, bool forward)
{
    int i;
    bool replaced = false;
    other->IncrRef();

    for (i = 0; i < m_parents.size(); i++)
    {
        if (m_parents[i]->m_name == other->m_name)
        {
            m_parents[i]->DecrRef();
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

/** \brief Internal use, set argument as requiring given option
 *
 *  This option is intended for internal use only, as part of reconciling
 *  dummy options with their matched real counterparts.
 */
void CommandLineArg::SetRequires(CommandLineArg *other, bool /*forward*/)
{
    int i;
    bool replaced = false;
    other->IncrRef();

    for (i = 0; i < m_requires.size(); i++)
    {
        if (m_requires[i]->m_name == other->m_name)
        {
            m_requires[i]->DecrRef();
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

/** \brief Internal use, set argument as incompatible with given option
 *
 *  This option is intended for internal use only, as part of reconciling
 *  dummy options with their matched real counterparts.
 */
void CommandLineArg::SetBlocks(CommandLineArg *other, bool forward)
{
    int i;
    bool replaced = false;
    other->IncrRef();

    for (i = 0; i < m_blocks.size(); i++)
    {
        if (m_blocks[i]->m_name == other->m_name)
        {
            m_blocks[i]->DecrRef();
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

/** \brief Mark a list of arguments as mutually exclusive
 */
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
            (*i1)->DecrRef();
    }
}

/** \brief Convert stored string value from QByteArray to QString
 *
 *  This is a work around to delay string processing until after QApplication
 *  has been initialized, to allow the locale to be configured and unicode
 *  handling to work properly
 */
void CommandLineArg::Convert(void)
{
    if (!QCoreApplication::instance())
        // QApplication not available, no sense doing anything yet
        return;

    if (m_converted)
        // already run, abort
        return;

    if (!m_given)
    {
        // nothing to work on, abort
        m_converted = true;
        return;
    }

    if (m_type == QVariant::String)
    {
        if (m_stored.type() == QVariant::ByteArray)
        {
            m_stored = QString::fromLocal8Bit(m_stored.toByteArray());
        }
        // else
        //      not sure why this isnt a bytearray, but ignore it and
        //      set it as converted
    }
    else if (m_type == QVariant::StringList)
    {
        if (m_stored.type() == QVariant::List)
        {
            QVariantList vlist = m_stored.toList();
            QVariantList::const_iterator iter = vlist.begin();
            QStringList slist;
            for (; iter != vlist.end(); ++iter)
                slist << QString::fromLocal8Bit(iter->toByteArray());
            m_stored = QVariant(slist);
        }
    }
    else if (m_type == QVariant::Map)
    {
        QVariantMap vmap = m_stored.toMap();
        QVariantMap::iterator iter = vmap.begin();
        for (; iter != vmap.end(); ++iter)
            (*iter) = QString::fromLocal8Bit(iter->toByteArray());
    }
    else
        return;

    m_converted = true;
}


/** \brief Return the longest keyword for the argument
 *
 *  This is used to determine which keyword to use when listing relations to
 *  other options. The longest keyword is presumed to be the most descriptive.
 */
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

/** \brief Test all related arguments to make sure specified requirements are
 *         fulfilled
 */
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

/** \brief Clear out references to other arguments in preparation for deletion
 */
void CommandLineArg::CleanupLinks(void)
{
    // clear out interdependent pointers in preparation for deletion
    while (!m_parents.isEmpty())
        m_parents.takeFirst()->DecrRef();

    while (!m_children.isEmpty())
        m_children.takeFirst()->DecrRef();

    while (!m_blocks.isEmpty())
        m_blocks.takeFirst()->DecrRef();

    while (!m_requires.isEmpty())
        m_requires.takeFirst()->DecrRef();

    while (!m_requiredby.isEmpty())
        m_requiredby.takeFirst()->DecrRef();
}

/** \brief Internal use. Print processed input in verbose mode.
 */
void CommandLineArg::PrintVerbose(void) const
{
    if (!m_given)
        return;

    cerr << "  " << m_name.leftJustified(30).toLocal8Bit().constData();

    QSize tmpsize;
    QMap<QString, QVariant> tmpmap;
    QMap<QString, QVariant>::const_iterator it;
    QVariantList vlist;
    QVariantList::const_iterator it2;
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
        cerr << '"' << m_stored.toByteArray().constData()
             << '"' << endl;
        break;

      case QVariant::StringList:
        vlist = m_stored.toList();
        it2 = vlist.begin();
        cerr << '"' << it2->toByteArray().constData() << '"';
        ++it2;
        for (; it2 != vlist.end(); ++it2)
            cerr << ", \""
                 << it2->constData()
                 << '"';
        cerr << endl;
        break;

      case QVariant::Map:
        tmpmap = m_stored.toMap();
        first = true;

        for (it = tmpmap.begin(); it != tmpmap.end(); ++it)
        {
            if (first)
                first = false;
            else
                cerr << QString("").leftJustified(32)
                                   .toLocal8Bit().constData();

            cerr << it.key().toLocal8Bit().constData()
                 << '='
                 << it->toByteArray().constData()
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

/** \brief Internal use. Print warning for removed option.
 */
void CommandLineArg::PrintRemovedWarning(QString &keyword) const
{
    QString warn = QString("%1 has been removed").arg(keyword);
    if (!m_removedversion.isEmpty())
        warn += QString(" as of MythTV %1").arg(m_removedversion);

    cerr << QString("****************************************************\n"
                    " WARNING: %1\n"
                    "          %2\n"
                    "****************************************************\n\n")
                .arg(warn).arg(m_removed)
                .toLocal8Bit().constData();
}

/** \brief Internal use. Print warning for deprecated option.
 */
void CommandLineArg::PrintDeprecatedWarning(QString &keyword) const
{
    cerr << QString("****************************************************\n"
                    " WARNING: %1 has been deprecated\n"
                    "          %2\n"
                    "****************************************************\n\n")
                .arg(keyword).arg(m_deprecated)
                .toLocal8Bit().constData();
}

/** \class MythCommandLineParser
 *  \ingroup commandlineparser
 *  \brief Parent class for defining application command line parsers
 *
 *  This class provides a generic interface for defining and parsing available
 *  command line options. Options can be provided manually using the add()
 *  method, or one of several canned add*() methods. Once defined, the command
 *  line is parsed using the Parse() method, and results are available through
 *  Qt standard to<Type>() methods.
 */

/** \brief Default constructor for MythCommandLineArg class
 */
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
        (*i)->CleanupLinks();
        (*i)->DecrRef();
        i = m_namedArgs.erase(i);
    }

    i = m_optionedArgs.begin();
    while (i != m_optionedArgs.end())
    {
        (*i)->DecrRef();
        i = m_optionedArgs.erase(i);
    }
}

/** \brief Add a new command line argument
 *
 *  This is the primary method for adding new arguments for processing. There
 *  are several overloaded convenience methods that tie into this, allowing
 *  it to be called with fewer inputs.
 *
 *  \param arglist  list of arguments to allow use of on the command line
 *  \param name     internal name to be used when pulling processed data out
 *  \param type     type of variable to be processed.  The allowed types are
 *                  listed below.
 *  \param def      default value to provide if one is not supplied or option
 *                  is not used
 *  \param help     short help text, displayed when printing all available
 *                  options with '--help'.  If this is empty, the argument
 *                  will not be shown
 *  \param longhelp Extended help text, displayed when help about a specific
 *                  option is requested using '--help \<option\>'
 *
 * <table>
 *   <tr><th> Type <th> Description
 *   <tr><td> Bool <td> set to value, or default if value is not provided
 *   <tr><td> String      <td> set to value, or default if value is not provided
 *   <tr><td> Int         <td> set to value, or behaves as counter for multiple uses if
 *                             value is not provided
 *   <tr><td> UInt        <td>
 *   <tr><td> LongLong    <td>
 *   <tr><td> Double      <td>
 *   <tr><td> DateTime    <td> accepts ISO8601 and %Myth's flattened version
 *   <tr><td> StringList  <td> accepts multiple uses, appended as individual strings
 *   <tr><td> Map         <td> accepts multiple pairs, in the syntax "key=value"
 *   <tr><td> Size        <td> accepts size in the syntax "XxY"
 * </table>
 */
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
            arg->IncrRef();
            m_optionedArgs.insert(*i, arg);
        }
    }

    return arg;
}

/** \brief Print application version information
 */
void MythCommandLineParser::PrintVersion(void) const
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

/** \brief Print command line option help
 */
void MythCommandLineParser::PrintHelp(void) const
{
    QString help = GetHelpString();
    cerr << help.toLocal8Bit().constData();
}

/** \brief Generate command line option help text
 *
 *  Generates generic help or specific help, depending on whether a value
 *  was provided to the --help option
 */
QString MythCommandLineParser::GetHelpString(void) const
{
    QString helpstr;
    QTextStream msg(&helpstr, QIODevice::WriteOnly);

    QString versionStr = QString("%1 version: %2 [%3] www.mythtv.org")
        .arg(m_appname).arg(MYTH_SOURCE_PATH).arg(MYTH_SOURCE_VERSION);
    msg << versionStr << endl;

    if (toString("showhelp").isEmpty())
    {
        // build generic help text

        QString descr = GetHelpHeader();
        if (descr.size() > 0)
            msg << endl << descr << endl << endl;

        // loop through registered arguments to populate list of groups
        QStringList groups("");
        int maxlen = 0;
        QMap<QString, CommandLineArg*>::const_iterator i1;
        for (i1 = m_namedArgs.begin(); i1 != m_namedArgs.end(); ++i1)
        {
            maxlen = max((*i1)->GetKeywordLength(), maxlen);
            if (!groups.contains((*i1)->m_group))
                groups << (*i1)->m_group;
        }

        // loop through list of groups and print help string for each
        // arguments will filter themselves if they are not in the group
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

/** \brief Internal use. Pull next key/value pair from argv.
 */
int MythCommandLineParser::getOpt(int argc, const char * const * argv,
                                  int &argpos, QString &opt, QByteArray &val)
{
    opt.clear();
    val.clear();

    if (argpos >= argc)
        // this shouldnt happen, return and exit
        return kEnd;

    QByteArray tmp(argv[argpos]);
    if (tmp.isEmpty())
        // string is empty, return and loop
        return kEmpty;

    if (m_passthroughActive)
    {
        // pass through has been activated
        val = tmp;
        return kArg;
    }

    if (tmp.startsWith('-') && tmp.size() > 1)
    {
        if (tmp == "--")
        {
            // all options beyond this will be passed as a single string
            m_passthroughActive = true;
            return kPassthrough;
        }

        if (tmp.contains('='))
        {
            // option contains '=', split
            QList<QByteArray> blist = tmp.split('=');

            if (blist.size() != 2)
            {
                // more than one '=' in option, this is not handled
                opt = QString(tmp);
                return kInvalid;
            }

            opt = QString(blist[0]);
            val = blist[1];
            return kCombOptVal;
        }

        opt = QString(tmp);

        if (argpos+1 >= argc)
            // end of input, option only
            return kOptOnly;

        tmp = QByteArray(argv[++argpos]);
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

/** \brief Loop through argv and populate arguments with values
 *
 *  This should not be called until all arguments are added to the parser.
 *  This returns false if the parser hits an argument it is not designed
 *  to handle.
 */
bool MythCommandLineParser::Parse(int argc, const char * const * argv)
{
    int res;
    QString opt;
    QByteArray val;
    CommandLineArg *argdef;

    // reconnect interdependencies between command line options
    if (!ReconcileLinks())
        return false;

    // loop through command line arguments until all are spent
    for (int argpos = 1; argpos < argc; ++argpos)
    {

        // pull next option
        res = getOpt(argc, argv, argpos, opt, val);

        if (m_verbose)
            cerr << "res: " << NamedOptType(res) << endl
                 << "opt:  " << opt.toLocal8Bit().constData() << endl
                 << "val:  " << val.constData() << endl << endl;

        // '--' found on command line, enable passthrough mode
        if (res == kPassthrough && !m_namedArgs.contains("_passthrough"))
        {
            cerr << "Received '--' but passthrough has not been enabled" << endl;
            SetValue("showhelp", "");
            return false;
        }

        // end of options found, terminate loop
        if (res == kEnd)
            break;

        // GetOpt pulled an empty option, this shouldnt happen by ignore
        // it and continue
        else if (res == kEmpty)
            continue;

        // more than one equal found in key/value pair, fault out
        else if (res == kInvalid)
        {
            cerr << "Invalid option received:" << endl << "    "
                 << opt.toLocal8Bit().constData();
            SetValue("showhelp", "");
            return false;
        }

        // passthrough is active, so add the data to the stringlist
        else if (m_passthroughActive)
        {
            m_namedArgs["_passthrough"]->Set("", val);
            continue;
        }

        // argument with no preceeding '-' encountered, add to stringlist
        else if (res == kArg)
        {
            if (!m_namedArgs.contains("_args"))
            {
                cerr << "Received '"
                     << val.constData()
                     << "' but unassociated arguments have not been enabled"
                     << endl;
                SetValue("showhelp", "");
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
            SetValue("showhelp", "");
            return false;
        }

#ifdef Q_OS_MAC
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
                QByteArray tmp = opt.toLocal8Bit();
                tmp += '=';
                tmp += val;
                val = tmp;
                res = kOptVal;
            }
            else
            {
                // arbitrary not allowed, fault out
                cerr << "Unhandled option given on command line:" << endl
                     << "    " << opt.toLocal8Bit().constData() << endl;
                SetValue("showhelp", "");
                return false;
            }
        }
        else
            argdef = m_optionedArgs[opt];

        // argument has been marked as removed, warn user and fail
        if (!argdef->m_removed.isEmpty())
        {
            argdef->PrintRemovedWarning(opt);
            SetValue("showhelp", "");
            return false;
        }

        // argument has been marked as deprecated, warn user
        if (!argdef->m_deprecated.isEmpty())
            argdef->PrintDeprecatedWarning(opt);

        if (m_verbose)
            cerr << "name: " << argdef->GetName().toLocal8Bit().constData()
                 << endl;

        // argument is keyword only, no value
        if (res == kOptOnly)
        {
            if (!argdef->Set(opt))
            {
                SetValue("showhelp", "");
                return false;
            }
        }
        // argument has keyword and value
        else if ((res == kOptVal) || (res == kCombOptVal))
        {
            if (!argdef->Set(opt, val))
            {
                // if option and value were combined with a '=', abort directly
                // otherwise, attempt processing them independenly
                if ((res == kCombOptVal) || !argdef->Set(opt))
                {
                    SetValue("showhelp", "");
                    return false;
                }
                // drop back an iteration so the unused value will get
                // processed a second time as a keyword-less argument
                --argpos;
            }
        }
        else
        {
            SetValue("showhelp", "");
            return false; // this should not occur
        }

        if (m_verbose)
            cerr << "value: " << argdef->m_stored.toString().toLocal8Bit().constData()
                 << endl;
    }

    QMap<QString, CommandLineArg*>::const_iterator it;

    if (m_verbose)
    {
        cerr << "Processed option list:" << endl;
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

    // make sure all interdependencies are fulfilled
    for (it = m_namedArgs.begin(); it != m_namedArgs.end(); ++it)
    {
        if (!(*it)->TestLinks())
        {
            QString keyword = (*it)->m_usedKeyword;
            if (keyword.startsWith('-'))
            {
                if (keyword.startsWith("--"))
                    keyword.remove(0,2);
                else
                    keyword.remove(0,1);
            }

            SetValue("showhelp", keyword);
            return false;
        }
    }

    return true;
}

/** \brief Replace dummy arguments used to define interdependency with pointers
 *  to their real counterparts.
 */
bool MythCommandLineParser::ReconcileLinks(void)
{
    if (m_verbose)
        cerr << "Reconciling links for option interdependencies." << endl;

    QMap<QString,CommandLineArg*>::iterator args_it;
    for (args_it = m_namedArgs.begin(); args_it != m_namedArgs.end(); ++args_it)
    {
        QList<CommandLineArg*> links = (*args_it)->m_parents;
        QList<CommandLineArg*>::iterator links_it;
        for (links_it = links.begin(); links_it != links.end(); ++links_it)
        {
            if ((*links_it)->m_type != QVariant::Invalid)
                continue; // already handled

            if (!m_namedArgs.contains((*links_it)->m_name))
            {
                // not found
                cerr << "ERROR: could not reconcile linked argument." << endl
                     << "  '" << (*args_it)->m_name.toLocal8Bit().constData()
                     << "' could not find '"
                     << (*links_it)->m_name.toLocal8Bit().constData()
                     << "'." << endl
                     << "  Please resolve dependency and recompile." << endl;
                return false;
            }

            // replace linked argument
            if (m_verbose)
                cerr << QString("  Setting %1 as child of %2")
                            .arg((*args_it)->m_name).arg((*links_it)->m_name)
                            .toLocal8Bit().constData()
                     << endl;
            (*args_it)->SetChildOf(m_namedArgs[(*links_it)->m_name]);
        }

        links = (*args_it)->m_children;
        for (links_it = links.begin(); links_it != links.end(); ++links_it)
        {
            if ((*links_it)->m_type != QVariant::Invalid)
                continue; // already handled

            if (!m_namedArgs.contains((*links_it)->m_name))
            {
                // not found
                cerr << "ERROR: could not reconcile linked argument." << endl
                     << "  '" << (*args_it)->m_name.toLocal8Bit().constData()
                     << "' could not find '"
                     << (*links_it)->m_name.toLocal8Bit().constData()
                     << "'." << endl
                     << "  Please resolve dependency and recompile." << endl;
                return false;
            }

            // replace linked argument
            if (m_verbose)
                cerr << QString("  Setting %1 as parent of %2")
                            .arg((*args_it)->m_name).arg((*links_it)->m_name)
                            .toLocal8Bit().constData()
                     << endl;
            (*args_it)->SetParentOf(m_namedArgs[(*links_it)->m_name]);
        }

        links = (*args_it)->m_requires;
        for (links_it = links.begin(); links_it != links.end(); ++links_it)
        {
            if ((*links_it)->m_type != QVariant::Invalid)
                continue; // already handled

            if (!m_namedArgs.contains((*links_it)->m_name))
            {
                // not found
                cerr << "ERROR: could not reconcile linked argument." << endl
                     << "  '" << (*args_it)->m_name.toLocal8Bit().constData()
                     << "' could not find '"
                     << (*links_it)->m_name.toLocal8Bit().constData()
                     << "'." << endl
                     << "  Please resolve dependency and recompile." << endl;
                return false;
            }

            // replace linked argument
            if (m_verbose)
                cerr << QString("  Setting %1 as requiring %2")
                            .arg((*args_it)->m_name).arg((*links_it)->m_name)
                            .toLocal8Bit().constData()
                     << endl;
            (*args_it)->SetRequires(m_namedArgs[(*links_it)->m_name]);
        }

        QList<CommandLineArg*>::iterator req_it =
            (*args_it)->m_requiredby.begin();
        while (req_it != (*args_it)->m_requiredby.end())
        {
            if ((*req_it)->m_type == QVariant::Invalid)
            {
                // if its not an invalid, it shouldnt be here anyway
                if (m_namedArgs.contains((*req_it)->m_name))
                {
                    m_namedArgs[(*req_it)->m_name]->SetRequires(*args_it);
                    if (m_verbose)
                    {
                        cerr << QString("  Setting %1 as blocking %2")
                                    .arg((*args_it)->m_name)
                                    .arg((*req_it)->m_name)
                                    .toLocal8Bit().constData()
                             << endl;
                    }
                }
            }

            (*req_it)->DecrRef();
            req_it = (*args_it)->m_requiredby.erase(req_it);
        }

        QList<CommandLineArg*>::iterator block_it =
            (*args_it)->m_blocks.begin();
        while (block_it != (*args_it)->m_blocks.end())
        {
            if ((*block_it)->m_type != QVariant::Invalid)
            {
                ++block_it;
                continue; // already handled
            }

            if (!m_namedArgs.contains((*block_it)->m_name))
            {
                (*block_it)->DecrRef();
                block_it = (*args_it)->m_blocks.erase(block_it);
                continue; // if it doesnt exist, it cant block this command
            }

            // replace linked argument
            if (m_verbose)
            {
                cerr << QString("  Setting %1 as blocking %2")
                            .arg((*args_it)->m_name).arg((*block_it)->m_name)
                            .toLocal8Bit().constData()
                     << endl;
            }
            (*args_it)->SetBlocks(m_namedArgs[(*block_it)->m_name]);
            ++block_it;
        }
    }

    return true;
}

/** \brief Returned stored QVariant for given argument, or default value
 *  if not used
 */
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

/** \brief Return list of additional values provided on the command line
 *  independent of any keyword.
 */
QStringList MythCommandLineParser::GetArgs(void) const
{
    return toStringList("_args");
}

/** \brief Return map of additional key/value pairs provided on the command
 *  line independent of any registered argument.
 */
QMap<QString,QString> MythCommandLineParser::GetExtra(void) const
{
    return toMap("_extra");
}

/** \brief Return any text supplied on the command line after a bare '--'
 */
QString MythCommandLineParser::GetPassthrough(void) const
{
    return toStringList("_passthrough").join(" ");
}

/** \brief Return map of key/value pairs provided to override database options
 *
 *  This method is used for the -O/--override-setting options, as well as the
 *  specific arguments to override the window border and mouse cursor. On its
 *  first use, this method will also read in any addition settings provided in
 *  the --override-settings-file
 */
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
                    QByteArray tmp = filename.toLatin1();
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

/** \brief Returns stored QVariant as a boolean
 *
 *  If the stored value is of type boolean, this will return the actual
 *  stored or default value. For all other types, this will return whether
 *  the argument was supplied on the command line or not.
 */
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

/** \brief Returns stored QVariant as an integer, falling to default
 *  if not provided
 */
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

/** \brief Returns stored QVariant as an unsigned integer, falling to
 *  default if not provided
 */
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

/** \brief Returns stored QVariant as a long integer, falling to
 *  default if not provided
 */
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

/** \brief Returns stored QVariant as double floating point value, falling
 *  to default if not provided
 */
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

/** \brief Returns stored QVariant as a QSize value, falling
 *  to default if not provided
 */
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

/** \brief Returns stored QVariant as a QString, falling
 *  to default if not provided
 */
QString MythCommandLineParser::toString(QString key) const
{
    QString val("");
    if (!m_namedArgs.contains(key))
        return val;

    CommandLineArg *arg = m_namedArgs[key];

    if (arg->m_given)
    {
        if (!arg->m_converted)
            arg->Convert();

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

/** \brief Returns stored QVariant as a QStringList, falling to default
 *  if not provided. Optional separator can be specified to split result
 *  if stored value is a QString.
 */
QStringList MythCommandLineParser::toStringList(QString key, QString sep) const
{
    QVariant varval;
    QStringList val;
    if (!m_namedArgs.contains(key))
        return val;

    CommandLineArg *arg = m_namedArgs[key];

    if (arg->m_given)
    {
        if (!arg->m_converted)
            arg->Convert();

        varval = arg->m_stored;
    }
    else
        varval = arg->m_default;

    if (arg->m_type == QVariant::String && !sep.isEmpty())
        val = varval.toString().split(sep);
    else if (varval.canConvert(QVariant::StringList))
        val = varval.toStringList();

    return val;
}

/** \brief Returns stored QVariant as a QMap, falling
 *  to default if not provided
 */
QMap<QString,QString> MythCommandLineParser::toMap(QString key) const
{
    QMap<QString, QString> val;
    QMap<QString, QVariant> tmp;
    if (!m_namedArgs.contains(key))
        return val;

    CommandLineArg *arg = m_namedArgs[key];

    if (arg->m_given)
    {
        if (!arg->m_converted)
            arg->Convert();

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

/** \brief Returns stored QVariant as a QDateTime, falling
 *  to default if not provided
 */
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

/** \brief Specify that parser should allow and collect values provided
 *  independent of any keyword
 */
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

/** \brief Specify that parser should allow and collect additional key/value
 *  pairs not explicitly defined for processing
 */
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

/** \brief Specify that parser should allow a bare '--', and collect all
 *  subsequent text as a QString
 */
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

/** \brief Canned argument definition for --help
 */
void MythCommandLineParser::addHelp(void)
{
    add(QStringList( QStringList() << "-h" << "--help" << "--usage" ),
            "showhelp", "", "Display this help printout, or give detailed "
                            "information of selected option.",
            "Displays a list of all commands available for use with "
            "this application. If another option is provided as an "
            "argument, it will provide detailed information on that "
            "option.");
}

/** \brief Canned argument definition for --version
 */
void MythCommandLineParser::addVersion(void)
{
    add("--version", "showversion", false, "Display version information.",
            "Display informtion about build, including:\n"
            " version, branch, protocol, library API, Qt "
            "and compiled options.");
}

/** \brief Canned argument definition for --windowed and -no-windowed
 */
void MythCommandLineParser::addWindowed(void)
{
    add(QStringList( QStringList() << "-nw" << "--no-windowed" ),
            "notwindowed", false,
            "Prevent application from running in a window.", "")
        ->SetBlocks("windowed")
        ->SetGroup("User Interface");

    add(QStringList( QStringList() << "-w" << "--windowed" ), "windowed",
            false, "Force application to run in a window.", "")
        ->SetGroup("User Interface");
}

/** \brief Canned argument definition for --mouse-cursor and --no-mouse-cursor
 */
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

/** \brief Canned argument definition for --daemon
 */
void MythCommandLineParser::addDaemon(void)
{
    add(QStringList( QStringList() << "-d" << "--daemon" ), "daemon", false,
            "Fork application into background after startup.",
            "Fork application into background, detatching from "
            "the local terminal.\nOften used with: "
            " --logpath --pidfile --user");
}

/** \brief Canned argument definition for --override-setting and
 *  --override-settings-file
 */
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

/** \brief Canned argument definition for --chanid and --starttime
 */
void MythCommandLineParser::addRecording(void)
{
    add("--chanid", "chanid", 0U,
            "Specify chanid of recording to operate on.", "")
        ->SetRequires("starttime");

    add("--starttime", "starttime", QDateTime(),
            "Specify start time of recording to operate on.", "")
        ->SetRequires("chanid");
}

/** \brief Canned argument definition for --geometry
 */
void MythCommandLineParser::addGeometry(void)
{
    add(QStringList( QStringList() << "-geometry" << "--geometry" ), "geometry",
            "", "Specify window size and position (WxH[+X+Y])", "")
        ->SetGroup("User Interface");
}

/** \brief Canned argument definition for -display. Only works on X11 systems.
 */
void MythCommandLineParser::addDisplay(void)
{
#ifdef USING_X11
    add("-display", "display", "", "Specify X server to use.", "")
        ->SetGroup("User Interface");
#endif
}

/** \brief Canned argument definition for --noupnp
 */
void MythCommandLineParser::addUPnP(void)
{
    add("--noupnp", "noupnp", false, "Disable use of UPnP.", "");
}

/** \brief Canned argument definition for all logging options, including
 *  --verbose, --logpath, --quiet, --loglevel, --syslog, --enable-dblog
 *  and --disable-mythlogserver
 */
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
    add("-V", "verboseint", 0LL, "",
        "This option is intended for internal use only.\n"
        "This option takes an unsigned value corresponding "
        "to the bitwise log verbosity operator.")
                ->SetGroup("Logging");
    add("--logpath", "logpath", "",
        "Writes logging messages to a file in the directory logpath with "
        "filenames in the format: applicationName.date.pid.log.\n"
        "This is typically used in combination with --daemon, and if used "
        "in combination with --pidfile, this can be used with log "
        "rotators, using the HUP call to inform MythTV to reload the "
        "file", "")
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
        "defaults to none.", "")
                ->SetGroup("Logging");
#if CONFIG_SYSTEMD_JOURNAL
    add("--systemd-journal", "systemd-journal", "false",
        "Use systemd-journal instead of syslog.", "")
                ->SetBlocks(QStringList()
                            << "syslog"
#if CONFIG_MYTHLOGSERVER
                            << "disablemythlogserver"
#endif
                )
                ->SetGroup("Logging");
#endif
    add("--nodblog", "nodblog", false, "Disable database logging.", "")
                ->SetGroup("Logging")
                ->SetDeprecated("this is now the default, see --enable-dblog");
    add("--enable-dblog", "enabledblog", false, "Enable logging to database.", "")
                ->SetGroup("Logging");

    add(QStringList( QStringList() << "-l" << "--logfile" ),
        "logfile", "", "", "")
                ->SetGroup("Logging")
                ->SetRemoved("This option has been removed as part of "
            "rewrite of the logging interface. Please update your init "
            "scripts to use --syslog to interface with your system's "
            "existing system logging daemon, or --logpath to specify a "
            "dirctory for MythTV to write its logs to.", "0.25");
#if CONFIG_MYTHLOGSERVER
    add("--disable-mythlogserver", "disablemythlogserver", false,
                "Disable all logging but console.", "")
                ->SetBlocks(QStringList() << "logpath" << "syslog" << "enabledblog")
                ->SetGroup("Logging");
#endif
}

/** \brief Canned argument definition for --pidfile
 */
void MythCommandLineParser::addPIDFile(void)
{
    add(QStringList( QStringList() << "-p" << "--pidfile" ), "pidfile", "",
            "Write PID of application to filename.",
            "Write the PID of the currently running process as a single "
            "line to this file. Used for init scripts to know what "
            "process to terminate, and with log rotators "
            "to send a HUP signal to process to have it re-open files.");
}

/** \brief Canned argument definition for --jobid
 */
void MythCommandLineParser::addJob(void)
{
    add(QStringList( QStringList() << "-j" << "--jobid" ), "jobid", 0, "",
            "Intended for internal use only, specify the JobID to match "
            "up with in the database for additional information and the "
            "ability to update runtime status in the database.");
}

/** \brief Canned argument definition for --infile and --outfile
 */
void MythCommandLineParser::addInFile(bool addOutFile)
{
    add("--infile", "infile", "", "Input file URI", "");
    if (addOutFile)
        add("--outfile", "outfile", "", "Output file URI", "");
}

/** \brief Helper utility for logging interface to pull path from --logpath
 */
QString MythCommandLineParser::GetLogFilePath(void)
{
    QString logfile = toString("logpath");
    pid_t   pid = getpid();

    if (logfile.isEmpty())
        return logfile;

    QString logdir;
    QString filepath;

    QFileInfo finfo(logfile);
    if (!finfo.isDir())
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("%1 is not a directory, disabling logfiles")
            .arg(logfile));
        return QString();
    }

    logdir  = finfo.filePath();
    logfile = QCoreApplication::applicationName() + "." +
        MythDate::toString(MythDate::current(), MythDate::kFilename) +
        QString(".%1").arg(pid) + ".log";

    SetValue("logdir", logdir);
    SetValue("logfile", logfile);
    SetValue("filepath", QFileInfo(QDir(logdir), logfile).filePath());

    return toString("filepath");
}

/** \brief Helper utility for logging interface to return syslog facility
 */
int MythCommandLineParser::GetSyslogFacility(void)
{
    QString setting = toString("syslog").toLower();
    if (setting == "none")
        return -2;

    return syslogGetFacility(setting);
}

/** \brief Helper utility for logging interface to filtering level
 */
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

/** \brief Set a new stored value for an existing argument definition, or
 *  spawn a new definition store value in. Argument is subsequently marked
 *  as being provided on the command line.
 */
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

/** \brief Read in logging options and initialize the logging interface
 */
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
        verboseMask = toLongLong("verboseint");

    verboseMask |= VB_STDIO|VB_FLUSH;

    int quiet = toUInt("quiet");
    if (max(quiet, (int)progress) > 1)
    {
        verboseMask = VB_NONE|VB_FLUSH;
        verboseArgParse("none");
    }

    int facility = GetSyslogFacility();
#if CONFIG_SYSTEMD_JOURNAL
    bool journal = toBool("systemd-journal");
    if (journal)
    {
        if (facility >= 0)
	    return GENERIC_EXIT_INVALID_CMDLINE;
	facility = SYSTEMD_JOURNAL_FACILITY;
    }
#endif
    bool dblog = toBool("enabledblog");
    bool noserver = toBool("disablemythlogserver");
    LogLevel_t level = GetLogLevel();
    if (level == LOG_UNKNOWN)
        return GENERIC_EXIT_INVALID_CMDLINE;

    LOG(VB_GENERAL, LOG_CRIT,
        QString("%1 version: %2 [%3] www.mythtv.org")
        .arg(QCoreApplication::applicationName())
        .arg(MYTH_SOURCE_PATH).arg(MYTH_SOURCE_VERSION));
    LOG(VB_GENERAL, LOG_CRIT, QString("Qt version: compile: %1, runtime: %2")
        .arg(QT_VERSION_STR).arg(qVersion()));
    LOG(VB_GENERAL, LOG_NOTICE,
        QString("Enabled verbose msgs: %1").arg(verboseString));

    QString logfile = GetLogFilePath();
    bool propagate = !logfile.isEmpty();

    if (toBool("daemon"))
        quiet = max(quiet, 1);

    logStart(logfile, progress, quiet, facility, level, dblog, propagate, noserver);

    return GENERIC_EXIT_OK;
}

/** \brief Apply all overrides to the global context
 *
 *  WARNING: this must not be called until after MythContext is initialized
 */
void MythCommandLineParser::ApplySettingsOverride(void)
{
    if (m_verbose)
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
        pidfs.open(pidfile.toLatin1().constData());
        if (!pidfs)
        {
            cerr << "Could not open pid file: " << ENO_STR << endl;
            return false;
        }
    }
    return true;
}

/** \brief Drop permissions to the specified user
 */
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


/** \brief Fork application into background, and detatch from terminal
 */
int MythCommandLineParser::Daemonize(void)
{
    ofstream pidfs;
    if (!openPidfile(pidfs, toString("pidfile")))
        return GENERIC_EXIT_PERMISSIONS_ERROR;

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        LOG(VB_GENERAL, LOG_WARNING, "Unable to ignore SIGPIPE");

#if CONFIG_DARWIN
    if (toBool("daemon"))
    {
        cerr << "Daemonizing is unavailable in OSX" << endl;
        LOG(VB_GENERAL, LOG_WARNING, "Unable to daemonize");
    }
#else
    if (toBool("daemon") && (daemon(0, 1) < 0))
    {
        cerr << "Failed to daemonize: " << ENO_STR << endl;
        return GENERIC_EXIT_DAEMONIZING_ERROR;
    }
#endif

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
