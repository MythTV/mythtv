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
const int sig_str_count = 256;
char *sig_str[sig_str_count];
uint sig_str_len[sig_str_count];
static void sig_str_init(int sig, const char *name)
{
    if (sig < sig_str_count)
    {
        if (sig_str[sig])
            free(sig_str[sig]);
        sig_str[sig] = strdup(name);
        sig_str_len[sig] = strlen(sig_str[sig]);
    }
}
static void sig_str_init(void)
{
    for (int i = 0; i < sig_str_count; i++)
    {
        sig_str[i] = NULL;
        sig_str_init(i, qPrintable(QString("%1").arg(i)));
    }
}

SignalHandler::SignalHandler(QList<int> &signallist, QObject *parent) :
    QObject(parent), m_notifier(NULL), m_usr1Handler(NULL), m_usr2Handler(NULL)
{
    s_exit_program = false; // set here due to "C++ static initializer madness"
    sig_str_init();
#ifndef _WIN32
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sigFd))
    {
        cerr << "Couldn't create socketpair" << endl;
        return;
    }
    m_notifier = new QSocketNotifier(sigFd[1], QSocketNotifier::Read, this);
    connect(m_notifier, SIGNAL(activated(int)), this, SLOT(handleSignal()));

    struct sigaction sa;
    sa.sa_handler = SignalHandler::signalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    QList<int>::iterator it = signallist.begin();
    for( ; it != signallist.end(); ++it )
    {
        switch (*it)
        {
        case SIGINT:
            sig_str_init(*it, "SIGINT");
            sigaction(*it, &sa, NULL);
            LOG(VB_GENERAL, LOG_INFO, "Setup SIGINT handler");
            break;
        case SIGTERM:
            sig_str_init(*it, "SIGTERM");
            sigaction(*it, &sa, NULL);
            LOG(VB_GENERAL, LOG_INFO, "Setup SIGTERM handler");
            break;
        case SIGSEGV:
            sig_str_init(*it, "SIGSEGV");
            sigaction(*it, &sa, NULL);
            LOG(VB_GENERAL, LOG_INFO, "Setup SIGSEGV handler");
            break;
        case SIGABRT:
            sig_str_init(*it, "SIGABRT");
            sigaction(*it, &sa, NULL);
            LOG(VB_GENERAL, LOG_INFO, "Setup SIGABRT handler");
            break;
        default:
            cerr << "No handler for signal " << *it << endl;
            break;
        }
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

    signal(SIGINT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    signal(SIGSEGV, SIG_DFL);
    signal(SIGABRT, SIG_DFL);

    if (m_usr1Handler)
    {
        signal(SIGUSR1, SIG_DFL);
        m_usr1Handler = NULL;
    }

    if (m_usr2Handler)
    {
        signal(SIGUSR2, SIG_DFL);
        m_usr2Handler = NULL;
    }
#endif
}

void SignalHandler::AddHandler(int signum, void (*handler)(void))
{
    switch (signum)
    {
    case SIGUSR1:
        LOG(VB_GENERAL, LOG_INFO, "Setup SIGUSR1 handler");
        m_usr1Handler = handler;
        break;
    case SIGUSR2:
        LOG(VB_GENERAL, LOG_INFO, "Setup SIGUSR2 handler");
        m_usr2Handler = handler;
        break;
    default:
        LOG(VB_GENERAL, LOG_CRIT, QString("Unhandled signal %1").arg(signum));
        break;
    }
}

void SignalHandler::signalHandler(int signum)
{
    char a = (char)signum;
    while (::write(sigFd[0], &a, 1) != 1);

    // One must not return from SEGV, ILL, BUS or FPE. When these
    // are raised by the program itself they will immediately get
    // re-raised on return.
    //
    // We also handle SIGABRT the same way. While it is safe to
    // return from the signal handler for SIGABRT doing so means
    // SIGABRT will fail when the UI thread is deadlocked; but
    // SIGABRT is the signal one uses to get a core of a
    // deadlocked program.
    if ((signum == SIGSEGV) || (signum == SIGILL) ||
        (signum == SIGBUS)  || (signum == SIGFPE) ||
        (signum == SIGABRT))
    {
        // Wait for UI event loop to handle this, however we may be
        // blocking it if this signal occured in the UI thread.
        // Note, can not use usleep() as it is not a signal safe function.
        sleep(1);

        if (!s_exit_program)
        {
            // log something to console.. regular logging may be kaput
            ssize_t d = ::write(STDERR_FILENO, "Handling ", 9);
            if (signum < sig_str_count)
                d+=::write(STDERR_FILENO, sig_str[signum], sig_str_len[signum]);
            d+=::write(STDERR_FILENO, "\n", 1);
            (void) d; // quiet ignoring return value warning.
        }

        // call the default signal handler.
        signal(signum, SIG_DFL);
        raise(signum);
    }
}

void SignalHandler::handleSignal(void)
{
    m_notifier->setEnabled(false);

    char a;
    int ret = ::read(sigFd[1], &a, sizeof(a));
    int signum = (int)a;
    (void)ret;

    switch (signum)
    {
    case SIGINT:
        LOG(VB_GENERAL, LOG_CRIT, "Received SIGINT");
        signal(SIGINT, SIG_DFL);
        qApp->quit();
        s_exit_program = true;
        break;
    case SIGTERM:
        LOG(VB_GENERAL, LOG_CRIT, "Received SIGTERM");
        signal(SIGTERM, SIG_DFL);
        qApp->quit();
        s_exit_program = true;
        break;
    case SIGSEGV:
        LOG(VB_GENERAL, LOG_CRIT, "Received SIGSEGV");
        signal(SIGSEGV, SIG_DFL);
        usleep(100000);
        s_exit_program = true;
        break;
    case SIGABRT:
        LOG(VB_GENERAL, LOG_CRIT, "Received SIGABRT");
        signal(SIGABRT, SIG_DFL);
        usleep(100000);
        s_exit_program = true;
        break;
    case SIGUSR1:
        LOG(VB_GENERAL, LOG_CRIT, "Received SIGUSR1");
        if (m_usr1Handler)
            m_usr1Handler();
        break;
    case SIGUSR2:
        LOG(VB_GENERAL, LOG_CRIT, "Received SIGUSR2");
        if (m_usr2Handler)
            m_usr2Handler();
        break;
    default:
        LOG(VB_GENERAL, LOG_CRIT, QString("Recieved unexpected signal %1")
            .arg(signum));
        break;
    }
    m_notifier->setEnabled(true);
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
