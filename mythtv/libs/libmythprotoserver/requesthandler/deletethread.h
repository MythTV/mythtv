#ifndef DELETETHREAD_H_
#define DELETETHREAD_H_

// ANSI C headers
#include <stdint.h>
#include <unistd.h>

// C++ headers
using namespace std;

// Qt headers
#include <QMutex>
#include <QTimer>
#include <QDateTime>
#include <QStringList>
#include <QWaitCondition>

// MythTV headers
#include "mthread.h"

typedef struct deletestruct
{
    QString path;
    int fd;
    off_t size;
    QDateTime wait;
} DeleteStruct;

class DeleteThread : public QObject, public MThread
{
    Q_OBJECT
  public:
    DeleteThread(void);
    void run(void);
    bool AddFile(QString path);

  signals:
    void fileUnlinked(QString path);
    void unlinkFailed(QString path);

  private slots:
    void timeout(void) { m_run = false; }

  private:
    void ProcessNew(void);
    void ProcessOld(void);

    size_t               m_increment;
    bool                 m_slow;
    bool                 m_link;
    bool                 m_run;
    QTimer               m_timer;
    int                  m_timeout;

    QStringList          m_newfiles;
    QMutex               m_newlock;

    QList<deletestruct*> m_files;
};

#endif
