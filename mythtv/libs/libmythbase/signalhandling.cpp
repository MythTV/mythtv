#include <QObject>
#include <QSocketNotifier>
#include <QCoreApplication>
#include <QList>

#include <stdint.h>
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

SignalHandler::SignalHandler(QList<int> &signallist, QObject *parent) :
    QObject(parent), m_notifier(NULL), m_usr1Handler(NULL), m_usr2Handler(NULL)
{
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
            sigaction(*it, &sa, NULL);
            LOG(VB_GENERAL, LOG_INFO, "Setup SIGINT handler");
            break;
        case SIGTERM:
            sigaction(*it, &sa, NULL);
            LOG(VB_GENERAL, LOG_INFO, "Setup SIGTERM handler");
            break;
        case SIGSEGV:
            sigaction(*it, &sa, NULL);
            LOG(VB_GENERAL, LOG_INFO, "Setup SIGSEGV handler");
            break;
        case SIGABRT:
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
    int ret = ::write(sigFd[0], &a, sizeof(a));
    (void)ret;
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
        break;
    case SIGTERM:
        LOG(VB_GENERAL, LOG_CRIT, "Received SIGTERM");
        signal(SIGTERM, SIG_DFL);
        qApp->quit();
        break;
    case SIGSEGV:
        LOG(VB_GENERAL, LOG_CRIT, "Received SIGSEGV");
        signal(SIGSEGV, SIG_DFL);
        usleep(100000);
        raise(signum);
        break;
    case SIGABRT:
        LOG(VB_GENERAL, LOG_CRIT, "Received SIGABRT");
        signal(SIGABRT, SIG_DFL);
        usleep(100000);
        raise(signum);
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
