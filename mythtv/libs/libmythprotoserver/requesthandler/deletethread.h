#ifndef DELETETHREAD_H_
#define DELETETHREAD_H_

// POSIX headers
#include <pthread.h>

// ANSI C headers
#include <stdint.h>

// C++ headers
using namespace std;

// Qt headers
#include <QMutex>
#include <QTimer>
#include <QThread>
#include <QStringList>
#include <QWaitCondition>

typedef struct deletestruct
{
    QString path;
    int fd;
    off_t size;
} DeleteStruct;

class DeleteThread : public QThread
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
    bool                 m_run;
    QTimer               m_timer;
    int                  m_timeout;

    QStringList          m_newfiles;
    QMutex               m_newlock;

    QList<deletestruct*> m_files;
};

#endif
