#include <QMutex>
#include <QStringList>
#include <QMap>
#include <QMutexLocker>
#include <iostream>

using namespace std;

#include "mythverbose.h"
#include "exitcodes.h"

typedef struct {
    uint64_t    mask;
    QString     name;
    bool        additive;
    QString     helpText;
} VerboseDef;

typedef QMap<QString, VerboseDef *> VerboseMap;

bool verboseInitialized = false;
VerboseMap verboseMap;
QMutex verboseMapMutex;

const uint64_t verboseDefaultInt = VB_IMPORTANT | VB_GENERAL;
const char    *verboseDefaultStr = " important general";

uint64_t print_verbose_messages = verboseDefaultInt;
QString verboseString = QString(verboseDefaultStr);

unsigned int userDefaultValueInt = verboseDefaultInt;
QString      userDefaultValueStr = QString(verboseDefaultStr);
bool         haveUserDefaultValues = false;

void verboseAdd(uint64_t mask, QString name, bool additive, QString helptext);
void verboseInit(void);
void verboseHelp();


void verboseAdd(uint64_t mask, QString name, bool additive, QString helptext)
{
    VerboseDef *item = new VerboseDef;

    item->mask = mask;
    name.detach();
    // VB_GENERAL -> general
    name.remove(0, 3);
    name = name.toLower();
    item->name = name;
    item->additive = additive;
    helptext.detach();
    item->helpText = helptext;

    verboseMap.insert(name, item);
}

void verboseInit(void)
{
    QMutexLocker locker(&verboseMapMutex);
    verboseMap.clear();

    // This looks funky, so I'll put some explanation here.  The verbosedefs.h
    // file gets included as part of the mythverbose.h include, and at that
    // time, the normal (without _IMPLEMENT_VERBOSE defined) code case will
    // define the VerboseMask enum.  At this point, we force it to allow us
    // to include the file again, but with _IMPLEMENT_VERBOSE set so that the
    // single definition of the VB_* values can be shared to define also the
    // contents of verboseMap, via repeated calls to verboseAdd()

#undef VERBOSEDEFS_H_
#define _IMPLEMENT_VERBOSE
#include "verbosedefs.h"
    
    verboseInitialized = true;
}

void verboseHelp()
{
    QString m_verbose = verboseString.trimmed();
    m_verbose.replace(QRegExp(" "), ",");
    m_verbose.remove(QRegExp("^,"));

    cerr << "Verbose debug levels.\n"
            "Accepts any combination (separated by comma) of:\n\n";

    for (VerboseMap::Iterator vit = verboseMap.begin();
         vit != verboseMap.end(); ++vit )
    {
        VerboseDef *item = vit.value();
        QString name = QString("  %1").arg(item->name, -15, ' ');
        cerr << name.toLocal8Bit().constData() << " - " << 
                item->helpText.toLocal8Bit().constData() << endl;
    }

    cerr << endl <<
      "The default for this program appears to be: '-v " <<
      m_verbose.toLocal8Bit().constData() << "'\n\n"
      "Most options are additive except for none, all, and important.\n"
      "These three are semi-exclusive and take precedence over any\n"
      "prior options given.  You can however use something like\n"
      "'-v none,jobqueue' to get only JobQueue related messages\n"
      "and override the default verbosity level.\n\n"
      "The additive options may also be subtracted from 'all' by \n"
      "prefixing them with 'no', so you may use '-v all,nodatabase'\n"
      "to view all but database debug messages.\n\n"
      "Some debug levels may not apply to this program.\n\n";
}

int parse_verbose_arg(QString arg)
{
    QString option;

    if (!verboseInitialized)
        verboseInit();

    QMutexLocker locker(&verboseMapMutex);

    print_verbose_messages = verboseDefaultInt;
    verboseString = QString(verboseDefaultStr);

    if (arg.startsWith('-'))
    {
        cerr << "Invalid or missing argument to -v/--verbose option\n";
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    QStringList verboseOpts = arg.split(',');
    for (QStringList::Iterator it = verboseOpts.begin();
         it != verboseOpts.end(); ++it )
    {
        option = (*it).toLower();
        bool reverseOption = false;

        if (option != "none" && option.left(2) == "no")
        {
            reverseOption = true;
            option = option.right(option.length() - 2);
        }

        if (option == "help")
        {
            verboseHelp();
            return GENERIC_EXIT_INVALID_CMDLINE;
        }
        else if (option == "default")
        {
            if (haveUserDefaultValues)
            {
                print_verbose_messages = userDefaultValueInt;
                verboseString = userDefaultValueStr;
            }
            else
            {
                print_verbose_messages = verboseDefaultInt;
                verboseString = QString(verboseDefaultStr);
            }
        }
        else 
        {
            VerboseDef *item = verboseMap.value(option);

            if (item)
            {
                if (reverseOption)
                {
                    print_verbose_messages &= ~(item->mask);
                    verboseString += " no" + item->name;
                }
                else
                {
                    if (item->additive)
                    {
                        print_verbose_messages |= item->mask;
                        verboseString += ' ' + item->name;
                    }
                    else
                    {
                        print_verbose_messages = item->mask;
                        verboseString = item->name;
                    }
                }
            }
            else
            {
                cerr << "Unknown argument for -v/--verbose: " << 
                        option.toLocal8Bit().constData() << endl;;
                return GENERIC_EXIT_INVALID_CMDLINE;
            }
        }
    }

    if (!haveUserDefaultValues)
    {
        haveUserDefaultValues = true;
        userDefaultValueInt = print_verbose_messages;
        userDefaultValueStr = verboseString;
    }

    return GENERIC_EXIT_OK;
}

// Verbose helper function for ENO macro
QString safe_eno_to_string(int errnum)
{
    return QString("%1 (%2)").arg(strerror(errnum)).arg(errnum);
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
