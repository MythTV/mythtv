#include <QObject>
#include <QSocketNotifier>
#include <QCoreApplication>
#include <QList>

#include <stdint.h>
#include <stdlib.h> // for free
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#ifndef _WIN32
#include <sys/socket.h>
#endif
#include <iostream>

using namespace std;

#include "compat.h"
#include "mythlogging.h"
#include "exitcodes.h"
#include "signalhandling.h"

int SignalHandler::sigFd[2];
volatile bool SignalHandler::s_exit_program = false;

// We may need to write out signal info using just the write() function
// so we create an array of C strings + measure their lengths.
#define SIG_STR_COUNT 256
char *sig_str[SIG_STR_COUNT];
uint sig_str_len[SIG_STR_COUNT];

static void sig_str_init(int sig, const char *name)
{
    char line[128];

    if (sig < SIG_STR_COUNT)
    {
        if (sig_str[sig])
            free(sig_str[sig]);
        snprintf(line, 128, "Handling %s\n", name);
        line[127] = '\0';
        sig_str[sig]     = strdup(line);
        sig_str_len[sig] = strlen(line);
    }
}

static void sig_str_init(void)
{
    for (int i = 0; i < SIG_STR_COUNT; i++)
    {
        sig_str[i] = NULL;
        sig_str_init(i, qPrintable(QString("Signal %1").arg(i)));
    }
}

QList<int> SignalHandler::s_defaultHandlerList;

SignalHandler::SignalHandler(QList<int> &signallist, QObject *parent) :
    QObject(parent), m_notifier(NULL)
{
    s_exit_program = false; // set here due to "C++ static initializer madness"
    sig_str_init();

    if (s_defaultHandlerList.isEmpty())
        s_defaultHandlerList << SIGINT << SIGTERM << SIGSEGV << SIGABRT
#ifndef _WIN32
                             << SIGBUS 
#endif
                             << SIGFPE << SIGILL;

#ifndef _WIN32
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sigFd))
    {
        cerr << "Couldn't create socketpair" << endl;
        return;
    }
    m_notifier = new QSocketNotifier(sigFd[1], QSocketNotifier::Read, this);
    connect(m_notifier, SIGNAL(activated(int)), this, SLOT(handleSignal()));

    QList<int>::iterator it = signallist.begin();
    for( ; it != signallist.end(); ++it )
    {
        int signum = *it;
        if (!s_defaultHandlerList.contains(signum))
        {
            cerr << "No default handler for signal " << signum << endl;
            continue;
        }

        AddHandler(signum, NULL);
    }
#endif
}

SignalHandler::~SignalHandler()
{
#ifndef _WIN32
    if (m_notifier)
    {
        ::close(sigFd[0]);
        ::close(sigFd[1]);
        delete m_notifier;
    }

    QMap<int, void (*)(void)>::iterator it = m_sigMap.begin();
    for ( ; it != m_sigMap.end(); ++it)
    {
        int signum = it.key();
        signal(signum, SIG_DFL);
    }

    m_sigMap.clear();
#endif
}

void SignalHandler::AddHandler(int signum, void (*handler)(void))
{
#ifndef _WIN32
    struct sigaction sa;
    sa.sa_handler = SignalHandler::signalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    const char *signame = strsignal(signum);
    signame = strdup(signame ? signame : "Unknown");

    sig_str_init(signum, signame);
    sigaction(signum, &sa, NULL);
    m_sigMap.insert(signum, handler);
    LOG(VB_GENERAL, LOG_INFO, QString("Setup %1 handler").arg(signame));

    free((void *)signame);
#endif
}

void SignalHandler::signalHandler(int signum)
{
    char a = (char)signum;

    // Keep trying if there's no room to write, but stop on error (-1)
    while (::write(sigFd[0], &a, 1) == 0);

    // One must not return from SEGV, ILL, BUS or FPE. When these
    // are raised by the program itself they will immediately get
    // re-raised on return.
    //
    // We also handle SIGABRT the same way. While it is safe to
    // return from the signal handler for SIGABRT doing so means
    // SIGABRT will fail when the UI thread is deadlocked; but
    // SIGABRT is the signal one uses to get a core of a
    // deadlocked program.
    switch (signum)
    {
    case SIGSEGV:
    case SIGILL:
#ifndef _WIN32
    case SIGBUS:
#endif
    case SIGFPE:
    case SIGABRT:
        // clear the signal handler so if it reoccurs we get instant death.
        signal(signum, SIG_DFL);

        // Wait for UI event loop to handle this, however we may be
        // blocking it if this signal occured in the UI thread.
        // Note, can not use usleep() as it is not a signal safe function.
        sleep(1);

        if (!s_exit_program)
        {
            // log something to console.. regular logging should be kaput
            // NOTE:  This needs to use write rather than cout or printf as
            //        we need to stick to system calls that are known to be
            //        signal-safe.  write is, the other two aren't.
            int d = 0;
            if (signum < SIG_STR_COUNT)
                d+=::write(STDERR_FILENO, sig_str[signum], sig_str_len[signum]);
            (void) d; // quiet ignoring return value warning.
        }

        // call the default signal handler.  This will kill the application.
        raise(signum);
        break;
    case SIGINT:
    case SIGTERM:
        // clear the signal handler so if it reoccurs we get instant death.
        signal(signum, SIG_DFL);
        break;
    }
}

void SignalHandler::handleSignal(void)
{
#ifndef _WIN32
    m_notifier->setEnabled(false);

    char a;
    int ret = ::read(sigFd[1], &a, sizeof(a));
    int signum = (int)a;
    (void)ret;

    const char *signame = strsignal(signum);
    signame = strdup(signame ? signame : "Unknown");
    LOG(VB_GENERAL, LOG_CRIT, QString("Received %1").arg(signame));
    free((void *)signame);

    switch (signum)
    {
    case SIGINT:
    case SIGTERM:
        qApp->quit();
        s_exit_program = true;
        break;
    case SIGSEGV:
    case SIGABRT:
    case SIGBUS:
    case SIGFPE:
    case SIGILL:
        usleep(100000);
        s_exit_program = true;
        break;
    default:
        void (*handler)(void) = m_sigMap.value(signum, NULL);
        if (handler)
        {
            handler();
        }
        else
        {
            LOG(VB_GENERAL, LOG_CRIT, QString("Recieved unexpected signal %1")
                .arg(signum));
        }
        break;
    }

    m_notifier->setEnabled(true);
#endif
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
