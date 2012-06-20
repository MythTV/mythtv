#ifndef SIGNALHANDLING_H_
#define SIGNALHANDLING_H_

#include <QObject>
#include <QSocketNotifier>
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
    SignalHandler(QList<int> &signallist, QObject *parent = NULL);
    ~SignalHandler();

    void AddHandler(int signal, void (*handler)(void));

    static bool IsExiting(void) { return s_exit_program; }

    // Unix signal handler.
    static void signalHandler(int signum);

  public slots:
    // Qt signal handler.
    void handleSignal(void);

  private:
    static int sigFd[2];
    static volatile bool s_exit_program;
    QSocketNotifier *m_notifier;

    QMap<int, void (*)(void)> m_sigMap;
    static QList<int> s_defaultHandlerList;
};

#endif

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
