#include <QMutex>
#include <QStringList>
#include <QtDebug>

#include "mythverbose.h"

#define GENERIC_EXIT_OK                             0
#define GENERIC_EXIT_INVALID_CMDLINE              252

const unsigned int verboseDefaultInt = VB_IMPORTANT | VB_GENERAL;
const char        *verboseDefaultStr = " important general";

QMutex verbose_mutex;
unsigned int print_verbose_messages = verboseDefaultInt;
QString verboseString = QString(verboseDefaultStr);

unsigned int userDefaultValueInt = verboseDefaultInt;
QString      userDefaultValueStr = QString(verboseDefaultStr);
bool         haveUserDefaultValues = false;

int parse_verbose_arg(QString arg)
{
    QString option;
    bool reverseOption;

    verbose_mutex.lock();

    print_verbose_messages = verboseDefaultInt;
    verboseString = QString(verboseDefaultStr);

    if (arg.startsWith('-'))
    {
        qDebug() << "Invalid or missing argument to -v/--verbose option\n";
        verbose_mutex.unlock();
        return GENERIC_EXIT_INVALID_CMDLINE;
    }
    else
    {
        QStringList verboseOpts = arg.split(',');
        for (QStringList::Iterator it = verboseOpts.begin();
             it != verboseOpts.end(); ++it )
        {
            option = *it;
            reverseOption = false;

            if (option != "none" && option.left(2) == "no")
            {
                reverseOption = true;
                option = option.right(option.length() - 2);
            }

            if (option == "help")
            {
                QString m_verbose = verboseString;
                m_verbose.replace(QRegExp(" "), ",");
                m_verbose.remove(QRegExp("^,"));
                qDebug() <<
                  "Verbose debug levels.\n" <<
                  "Accepts any combination (separated by comma) of:\n\n" <<

#define VERBOSE_ARG_HELP(ARG_ENUM, ARG_VALUE, ARG_STR, ARG_ADDITIVE, ARG_HELP) \
                QString("  %1").arg(ARG_STR).leftJustified(15, ' ', true) << \
                " - " << ARG_HELP << "\n" <<

                  VERBOSE_MAP(VERBOSE_ARG_HELP)

                  "\n" <<
                  "The default for this program appears to be: '-v " <<
                  m_verbose << "'\n\n" <<
                  "Most options are additive except for none, all, and important.\n" <<
                  "These three are semi-exclusive and take precedence over any\n" <<
                  "prior options given.  You can however use something like\n" <<
                  "'-v none,jobqueue' to get only JobQueue related messages\n" <<
                  "and override the default verbosity level.\n" <<
                  "\n" <<
                  "The additive options may also be subtracted from 'all' by \n" <<
                  "prefixing them with 'no', so you may use '-v all,nodatabase'\n" <<
                  "to view all but database debug messages.\n" <<
                  "\n" <<
                  "Some debug levels may not apply to this program.\n" <<
                  endl;
                verbose_mutex.unlock();
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

#define VERBOSE_ARG_CHECKS(ARG_ENUM, ARG_VALUE, ARG_STR, ARG_ADDITIVE, ARG_HELP) \
            else if (option == ARG_STR) \
            { \
                if (reverseOption) \
                { \
                    print_verbose_messages &= ~(ARG_VALUE); \
                    verboseString = verboseString + " no" + ARG_STR; \
                } \
                else \
                { \
                    if (ARG_ADDITIVE) \
                    { \
                        print_verbose_messages |= ARG_VALUE; \
                        verboseString = verboseString + ' ' + ARG_STR; \
                    } \
                    else \
                    { \
                        print_verbose_messages = ARG_VALUE; \
                        verboseString = ARG_STR; \
                    } \
                } \
            }

            VERBOSE_MAP(VERBOSE_ARG_CHECKS)

            else
            {
                qDebug() << "Unknown argument for -v/--verbose: "
                         << option << endl;;
                verbose_mutex.unlock();
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

    verbose_mutex.unlock();

    return GENERIC_EXIT_OK;
}

// Verbose helper function for ENO macro
QString safe_eno_to_string(int errnum)
{
    return QString("%1 (%2)").arg(strerror(errnum)).arg(errnum);
}

