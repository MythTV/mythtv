#include <QtGlobal>
#if QT_VERSION >= QT_VERSION_CHECK(6,5,0)
#include <QtSystemDetection>
#endif
#include <QObject>
#include <QSocketNotifier>
#include <QCoreApplication>
#include <QList>

#include <csignal>
#include <cstdint>
#include <cstdlib> // for free
#include <iostream>
#include <string>
#include <thread>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#ifndef Q_OS_WINDOWS
#include <sys/socket.h>
#endif

#include "compat.h"
#include "mythlogging.h"
#include "loggingserver.h"
#include "exitcodes.h"
#include "signalhandling.h"

std::array<int,2> SignalHandler::s_sigFd;
volatile bool SignalHandler::s_exit_program = false;
QMutex SignalHandler::s_singletonLock;
SignalHandler *SignalHandler::s_singleton;

volatile uint64_t SignalHandler::write_failed {0};
volatile uint64_t SignalHandler::read_failed  {0};
volatile uint64_t SignalHandler::total_bottom {0};
volatile uint64_t SignalHandler::total_top    {0};
volatile uint64_t SignalHandler::counts_bottom[my_nsig];
volatile uint64_t SignalHandler::counts_top[my_nsig];

static const std::array<const int, 6
#ifndef Q_OS_WINDOWS
    + 1
#if !defined(Q_OS_DARWIN) && !defined(Q_OS_OPENBSD)
    + 1
#endif // Q_OS_DARWIN
#endif // Q_OS_WINDOWS
    > kDefaultSignalList
{
    SIGINT, SIGTERM, SIGSEGV, SIGABRT, SIGFPE, SIGILL,
#ifndef Q_OS_WINDOWS
    SIGBUS,
#if !defined(Q_OS_DARWIN) && !defined(Q_OS_OPENBSD)
    SIGRTMIN, // not necessarily constexpr
#endif // Q_OS_DARWIN
#endif // Q_OS_WINDOWS
};

// We may need to write out signal info using just the write() function
// so we create an array of C strings + measure their lengths.
static constexpr size_t SIG_STR_COUNT { 256 };
std::array<std::string,SIG_STR_COUNT> sig_str;

static void sig_str_init(size_t sig, const char *name)
{
    if (sig >= sig_str.size())
        return;

    sig_str[sig] = qPrintable(QString("Handling %1\n").arg(name));
}

static void sig_str_init(void)
{
    for (size_t i = 0; i < sig_str.size(); i++)
        sig_str_init(i, qPrintable(QString("Signal %1").arg(i)));
}

SignalHandler::SignalHandler(QObject *parent) :
    QObject(parent)
{
    s_exit_program = false; // set here due to "C++ static initializer madness"
    sig_str_init();

#ifndef Q_OS_WINDOWS
    //NOLINTNEXTLINE(cppcoreguidelines-prefer-member-initializer)
    m_sigStack = new char[SIGSTKSZ];
    stack_t stack;
    stack.ss_sp = m_sigStack;
    stack.ss_flags = 0;
    stack.ss_size = SIGSTKSZ;

    // Carry on without the signal stack if it fails
    if (sigaltstack(&stack, nullptr) == -1)
    {
        std::cerr << "Couldn't create signal stack!" << std::endl;
        delete [] m_sigStack;
        m_sigStack = nullptr;
    }

    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, s_sigFd.data()))
    {
        std::cerr << "Couldn't create socketpair" << std::endl;
        return;
    }
    m_notifier = new QSocketNotifier(s_sigFd[1], QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, &SignalHandler::handleSignal);

    for (const int signum : kDefaultSignalList)
    {
        SetHandlerPrivate(signum, nullptr);
    }
    SetHandlerPrivate(SIGHUP, logSigHup);
#endif // Q_OS_WINDOWS
}

SignalHandler::~SignalHandler()
{
    s_singleton = nullptr;

#ifndef Q_OS_WINDOWS
    if (m_notifier)
    {
        ::close(s_sigFd[0]);
        ::close(s_sigFd[1]);
        delete m_notifier;
    }

    QMutexLocker locker(&m_sigMapLock);
    // NOLINTNEXTLINE(modernize-loop-convert)
    for (auto it = m_sigMap.begin(); it != m_sigMap.end(); ++it)
    {
        int signum = it.key();
        signal(signum, SIG_DFL);
    }

    m_sigMap.clear();

    delete [] m_sigStack;
    m_sigStack = nullptr;
#endif
}

void SignalHandler::Init(QObject *parent)
{
    QMutexLocker locker(&s_singletonLock);
    if (!s_singleton)
        s_singleton = new SignalHandler(parent);
}

void SignalHandler::Done(void)
{
    QMutexLocker locker(&s_singletonLock);
    delete s_singleton;
}


void SignalHandler::SetHandler(int signum, SigHandlerFunc handler)
{
    QMutexLocker locker(&s_singletonLock);
    if (s_singleton)
        s_singleton->SetHandlerPrivate(signum, handler);
}

void SignalHandler::SetHandlerPrivate([[maybe_unused]] int signum,
                                      [[maybe_unused]] SigHandlerFunc handler)
{
#ifndef Q_OS_WINDOWS
    const char *signame = strsignal(signum);
    QString signal_name = signame ?
        QString(signame) : QString("Unknown(%1)").arg(signum);

    bool sa_handler_already_set = false;
    {
        QMutexLocker locker(&m_sigMapLock);
        sa_handler_already_set = m_sigMap.contains(signum);
        if (m_sigMap.value(signum, nullptr) && (handler != nullptr))
        {
            LOG(VB_GENERAL, LOG_WARNING,
                QString("Warning %1 signal handler overridden")
                .arg(signal_name));
        }
        m_sigMap[signum] = handler;
    }

    if (!sa_handler_already_set)
    {
        struct sigaction sa {};
        sa.sa_sigaction = SignalHandler::signalHandler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART | SA_SIGINFO;
        if (m_sigStack)
            sa.sa_flags |= SA_ONSTACK;

        sig_str_init(signum, qPrintable(signal_name));

        sigaction(signum, &sa, nullptr);
    }

    LOG(VB_GENERAL, LOG_INFO, QString("Setup %1 handler").arg(signal_name));
#endif
}

struct SignalInfo {
    int      m_signum;
    int      m_code;
    int      m_pid;
    int      m_uid;
    uint64_t m_value;
};

void SignalHandler::signalHandler(int signum,
                                  [[maybe_unused]] siginfo_t *info,
                                  [[maybe_unused]] void *context)
{
    total_bottom += 1;
    counts_bottom[signum] = counts_bottom[signum] + 1;

    SignalInfo signalInfo {};

    signalInfo.m_signum = signum;
#ifdef Q_OS_WINDOWS
    signalInfo.m_code   = 0;
    signalInfo.m_pid    = 0;
    signalInfo.m_uid    = 0;
    signalInfo.m_value  = 0;
#else
    signalInfo.m_code   = (info ? info->si_code : 0);
    signalInfo.m_pid    = (info ? (int)info->si_pid : 0);
    signalInfo.m_uid    = (info ? (int)info->si_uid : 0);
    signalInfo.m_value  = (info ? info->si_value.sival_int : 0);
#endif

    // Keep trying if there's no room to write, but stop on error (-1)
    int index = 0;
    int size  = sizeof(SignalInfo);
    char *buffer = (char *)&signalInfo;
    while (size > 0)
    {
        int written = ::write(s_sigFd[0], &buffer[index], size);
        // If there's an error, the signal will not be seen be the application,
        // but we can't keep trying.
        if (written < 0)
        {
            write_failed += 1;
            break;
        }
        index += written;
        size  -= written;
    }

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
#ifndef Q_OS_WINDOWS
    case SIGBUS:
#endif
    case SIGFPE:
    case SIGABRT:
        // clear the signal handler so if it reoccurs we get instant death.
        signal(signum, SIG_DFL);

        // Wait for UI event loop to handle this, however we may be
        // blocking it if this signal occured in the UI thread.
        // Note, we cannot use std::this_thread::sleep_for()
        // since it is not a signal safe function.
        sleep(1);

        if (!s_exit_program)
        {
            // log something to console.. regular logging should be kaput
            // NOTE:  This needs to use write rather than cout or printf as
            //        we need to stick to system calls that are known to be
            //        signal-safe.  write is, the other two aren't.
            int d = 0;
            if (signum < static_cast<int>(sig_str.size()))
                d+=::write(STDERR_FILENO, sig_str[signum].c_str(), sig_str[signum].size());
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
#ifndef Q_OS_WINDOWS
    m_notifier->setEnabled(false);

    total_top += 1;
    int available;
    ioctl(s_sigFd[1], FIONREAD, &available);

    errno = 0;
    SignalInfo signalInfo {};
    int ret = ::read(s_sigFd[1], &signalInfo, sizeof(SignalInfo));
    if (ret < 0)
    {
        read_failed += 1;
        LOG(VB_GENERAL, LOG_CRIT,
            QString("Read failed on socket. Avail: %1. Error %2: %3. B/T %4/%5, WF/RF %6/%7")
            .arg(available)
            .arg(errno).arg(strerror(errno))
            .arg(total_bottom).arg(total_top)
            .arg(write_failed).arg(read_failed));
        std::this_thread::sleep_for(100ms);
        QCoreApplication::exit(0);
        s_exit_program = true;
        return;
    }

    bool infoComplete = (ret == sizeof(SignalInfo));
    int signum = (infoComplete ? signalInfo.m_signum : 0);

    counts_top[signum] = counts_top[signum] + 1;
    if ( (total_top <= 10) ||
        ((total_top <= 100) && (total_top % 10 == 0)))
    {
        LOG(VB_GENERAL, LOG_CRIT,
            QString("Read returned %1 of %2 bytes: %3 %4 %5 %6 %7, RUN %8/%9, FAIL %10/%11")
            .arg(ret).arg(available)
            .arg(signum)
            .arg(signalInfo.m_code)
            .arg(signalInfo.m_pid)
            .arg(signalInfo.m_uid)
            .arg(signalInfo.m_value,8,16,QChar('0'))
            .arg(total_bottom).arg(total_top).arg(write_failed).arg(read_failed));
    }
    if ((total_top == 10) || (total_top % 100 == 0))
    {
        LOG(VB_GENERAL, LOG_CRIT, QString("There are %1 signals. RUN %2/%3, FAIL %4/%5")
            .arg(_NSIG)
            .arg(total_bottom).arg(total_top).arg(write_failed).arg(read_failed));
        for (int i = 0; i < _NSIG; i += 8)
            LOG(VB_GENERAL, LOG_CRIT,
                QString("Signal %1: %2/%3 %4/%5 %6/%7 %8/%9 %10/%11 %12/%13 %14/%15 %16/%17")
                .arg(i,2)
                .arg(counts_bottom[i+0]).arg(counts_top[i+0])
                .arg(counts_bottom[i+1]).arg(counts_top[i+1])
                .arg(counts_bottom[i+2]).arg(counts_top[i+2])
                .arg(counts_bottom[i+3]).arg(counts_top[i+3])
                .arg(counts_bottom[i+4]).arg(counts_top[i+4])
                .arg(counts_bottom[i+5]).arg(counts_top[i+5])
                .arg(counts_bottom[i+6]).arg(counts_top[i+6])
                .arg(counts_bottom[i+7]).arg(counts_top[i+7])
                );
    }

    if (infoComplete)
    {
        QString signame = strsignal(signum);
        if (signame.isEmpty())
            signame = "Unknown Signal";
        LOG(VB_GENERAL, LOG_CRIT,
            QString("Received %1(%2): Code %3, PID %4, UID %5, Value 0x%6, RUN %7/%8, FAIL %10/%11")
            .arg(signame) .arg(signum) .arg(signalInfo.m_code) .arg(signalInfo.m_pid)
            .arg(signalInfo.m_uid) .arg(signalInfo.m_value,8,16,QChar('0'))
            .arg(total_bottom).arg(total_top).arg(write_failed).arg(read_failed));
    }

    SigHandlerFunc handler = nullptr;
    bool allowNullHandler = false;

#if !defined(Q_OS_DARWIN) && !defined(Q_OS_OPENBSD)
    if (signum == SIGRTMIN)
    {
        // glibc idiots seem to have made SIGRTMIN a macro that expands to a
        // function, so we can't do this in the switch below.
        // This uses the default handler to just get us here and to ignore it.
        allowNullHandler = true;
    }
#endif // !defined(Q_OS_DARWIN) && !defined(Q_OS_OPENBSD)

    switch (signum)
    {
    case SIGINT:
    case SIGTERM:
        m_sigMapLock.lock();
        handler = m_sigMap.value(signum, nullptr);
        m_sigMapLock.unlock();

        if (handler)
            handler();
        else
            QCoreApplication::exit(0);
        s_exit_program = true;
        break;
    case SIGSEGV:
    case SIGABRT:
    case SIGBUS:
    case SIGFPE:
    case SIGILL:
        std::this_thread::sleep_for(100ms);
        s_exit_program = true;
        break;
    default:
        m_sigMapLock.lock();
        handler = m_sigMap.value(signum, nullptr);
        m_sigMapLock.unlock();
        if (handler)
        {
            handler();
        }
        else if (!allowNullHandler)
        {
            LOG(VB_GENERAL, LOG_CRIT, QString("Received unexpected signal %1")
                .arg(signum));
        }
        break;
    }

    m_notifier->setEnabled(true);
#endif // Q_OS_WINDOWS
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
