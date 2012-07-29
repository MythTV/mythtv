#ifndef SIGNALHANDLING_H_
#define SIGNALHANDLING_H_

#include <QObject>
#include <QSocketNotifier>
#include <QMutex>
#include <QList>
#include <QMap>

#include <stdint.h>
#include <signal.h>
#include <unistd.h>

#include "mythbaseexp.h"  //  MBASE_PUBLIC , etc.
#include "mthread.h"

/// \brief A container object to handle UNIX signals in the Qt space correctly
class MBASE_PUBLIC SignalHandler: public QObject
{
    Q_OBJECT

  public:
    static void Init(QList<int> &signallist, QObject *parent = NULL);
    static void Done(void);

    static void SetHandler(int signal, void (*handler)(void));

    static bool IsExiting(void) { return s_exit_program; }

    // Unix signal handler.
    static void signalHandler(int signum);

  public slots:
    // Qt signal handler.
    void handleSignal(void);

  private:
    SignalHandler(QList<int> &signallist, QObject *parent);
    ~SignalHandler();
    void SetHandlerPrivate(int signal, void (*handler)(void));

    static int sigFd[2];
    static volatile bool s_exit_program;
    QSocketNotifier *m_notifier;

    QMutex m_sigMapLock;
    QMap<int, void (*)(void)> m_sigMap;
    static QList<int> s_defaultHandlerList;

    static QMutex s_singletonLock;
    static SignalHandler *s_singleton;
};

#endif

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
