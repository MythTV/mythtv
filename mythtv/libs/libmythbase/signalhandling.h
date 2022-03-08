#ifndef SIGNALHANDLING_H_
#define SIGNALHANDLING_H_

#include <QObject>
#include <QSocketNotifier>
#include <QMutex>
#include <QList>
#include <QMap>

#include <array>
#include <cstdint>
#include <csignal>
#include <unistd.h>

#include "mythbaseexp.h"  //  MBASE_PUBLIC , etc.

#ifdef _WIN32
// Quick fix to let this compile for Windows:  we have it disabled on the
// calling side for Windows anyways, IIRC.
using siginfo_t = void;
#endif

using SigHandlerFunc = void (*)(void);

/// \brief A container object to handle UNIX signals in the Qt space correctly
class MBASE_PUBLIC SignalHandler: public QObject
{
    Q_OBJECT

  public:
    static void Init(QObject *parent = nullptr);
    static void Done(void);

    static void SetHandler(int signum, SigHandlerFunc handler);

    static bool IsExiting(void) { return s_exit_program; }

    // Unix signal handler.
    // context is of type ucontext_t * cast to void *
    static void signalHandler(int signum, siginfo_t *info, void *context);

  public slots:
    // Qt signal handler.
    void handleSignal(void);

  private:
    explicit SignalHandler(QObject *parent);
    ~SignalHandler() override;
    void SetHandlerPrivate(int signum, SigHandlerFunc handler);

    static std::array<int,2> s_sigFd;
    static volatile bool s_exit_program;
    QSocketNotifier *m_notifier {nullptr};
    char            *m_sigStack {nullptr};

    QMutex m_sigMapLock;
    QMap<int, SigHandlerFunc> m_sigMap;

    const std::array<const int, 6
#ifndef _WIN32
        + 1
#ifndef Q_OS_DARWIN
        + 1
#endif // Q_OS_DARWIN
#endif // _WIN32
    > k_defaultSignalList
    {
        SIGINT, SIGTERM, SIGSEGV, SIGABRT, SIGFPE, SIGILL,
#ifndef _WIN32
        SIGBUS,
#ifndef Q_OS_DARWIN
        SIGRTMIN, // not necessarily constexpr
#endif // Q_OS_DARWIN
#endif // _WIN32
    };

    static QMutex s_singletonLock;
    static SignalHandler *s_singleton;
};

#endif

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
