#ifndef SIGNALHANDLING_H_
#define SIGNALHANDLING_H_

#include <QObject>
#include <QSocketNotifier>
#include <QList>

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

    // Unix signal handler.
    static void signalHandler(int signum);

  public slots:
    // Qt signal handler.
    void handleSignal(void);

  private:
    static int sigFd[2];
    QSocketNotifier *m_notifier;

    void (*m_usr1Handler)(void);
    void (*m_usr2Handler)(void);
};

#endif

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
