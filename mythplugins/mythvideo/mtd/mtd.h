#ifndef MTD_H_
#define MTD_H_
/*
    mtd.h

    (c) 2003 Thor Sigvaldason and Isaac Richards
    Part of the mythTV project

    Headers for the core mtd object

*/

#include <QObject>
#include <QList>

#include "logging.h"
#include "jobthread.h"
#include "dvdprobe.h"
#include "threadevents.h"

class QStringList;
class QTimer;
class QMutex;
class QTcpServer;
class QTcpSocket;
class QWaitCondition;

class MythTranscodeDaemon;

class DiscCheckingThread : public QThread
{
  public:
    DiscCheckingThread(DVDProbe *probe,
                       QMutex   *drive_access_mutex,
                       QMutex   *mutex_for_titles);
    virtual void run(void);
    void    Cancel(void);

    bool    IsDiscPresent(void) const { return have_disc; }

  private:
    ~DiscCheckingThread();
    bool    IsCancelled(void) const;

  private:
    MythTranscodeDaemon *parent;
    DVDProbe            *dvd_probe;
    volatile bool        have_disc;
    QMutex              *dvd_drive_access;
    QMutex              *titles_mutex;
    QMutex              *cancelLock;
    QWaitCondition      *cancelWaitCond;
    bool                 cancel_me;
};

class MythTranscodeDaemon : public QObject
{

    Q_OBJECT

    //
    //  Core logic (wait for connections, launch transcoding
    //  threads)
    //

  public:

    MythTranscodeDaemon(int port, bool log_stdout);
    ~MythTranscodeDaemon();
    bool Init(void);

    bool threadsShouldContinue(void) const { return keep_running; }
    bool IncrConcurrentTranscodeCounter(void);

  signals:

    void writeToLog(const QString &entry);

  private slots:

    void newConnection(void);
    void endConnection(void);
    void readSocket(void);
    void parseTokens(const QStringList &tokens, QTcpSocket *socket);
    void sendMessage(QTcpSocket *where, const QString &what);
    void sayHi(QTcpSocket *socket);
    void sendStatusReport(QTcpSocket *socket);
    void sendMediaReport(QTcpSocket *socket);
    void startJob  (const QStringList &tokens);
    void startAbort(const QStringList &tokens);
    void startDVD  (const QStringList &tokens);
    void useDrive  (const QStringList &tokens);
    void useDVD    (const QStringList &tokens);
    void noDrive   (const QStringList &tokens);
    QString noDVD  (const QStringList &tokens);
    void forgetDVD();
    void cleanThreads();
    void checkDisc();
    bool checkFinalFile(QFile *final_file, const QString &extension);

  private:
    void customEvent(QEvent *ce);
    void ShutDownThreads(void);

    int                 listening_port;
    bool                log_to_stdout;
    MTDLogger          *mtd_log;
    QTcpServer         *server_socket;
    JobThreadList       job_threads;
    mutable QMutex     *dvd_drive_access;
    mutable QMutex     *titles_mutex;
    volatile bool       keep_running;
    bool                have_disc;
    QTimer             *thread_cleaning_timer;
    QTimer             *disc_checking_timer;
    QString             dvd_device;
    DVDProbe           *dvd_probe;
    DiscCheckingThread *disc_checking_thread;
    int                 nice_level;
    mutable QMutex     *concurrent_transcodings_mutex;
    int                 concurrent_transcodings;
    int                 max_concurrent_transcodings;
};

#endif  // mtd_h_

