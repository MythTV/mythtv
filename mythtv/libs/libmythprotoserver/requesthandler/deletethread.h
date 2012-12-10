#ifndef DELETETHREAD_H_
#define DELETETHREAD_H_

// ANSI C headers
#include <stdint.h>
#include <unistd.h>

// Qt headers
#include <QMutex>
#include <QTimer>
#include <QDateTime>
#include <QStringList>
#include <QWaitCondition>

// MythTV headers
#include "mthread.h"
#include "requesthandler/fileserverutil.h"

class DeleteThread : public QObject, public MThread
{
    Q_OBJECT
  public:
    DeleteThread(void);
    void run(void);
    bool AddFile(QString path);
    bool AddFile(DeleteHandler *handler);
    void Stop(void)         { m_run = false; }

  private:
    void ProcessNew(void);
    void ProcessOld(void);

    size_t               m_increment;
    bool                 m_slow;
    bool                 m_link;
    bool                 m_run;

    QList<DeleteHandler*> m_newfiles;
    QMutex                m_newlock;

    QList<DeleteHandler*> m_files;
};

#endif
