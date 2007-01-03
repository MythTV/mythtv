#ifndef MTD_H_
#define MTD_H_
/*
	mtd.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for the core mtd object

*/

#include <qobject.h>
#include <qptrlist.h>

#include "logging.h"
#include "serversocket.h"
#include "jobthread.h"
#include "dvdprobe.h"
#include "threadevents.h"

class QStringList;
class QTimer;

class DiscCheckingThread : public QThread
{

  public:
  
    DiscCheckingThread( MTD *owner,
                        DVDProbe *probe, 
                        QMutex *drive_access_mutex,
                        QMutex *mutex_for_titles);
    virtual void run();
    bool    haveDisc(){return have_disc;}
    bool    keepGoing();
    
  private:
  
    MTD      *parent;
    DVDProbe *dvd_probe;
    bool     have_disc;
    QMutex   *dvd_drive_access;
    QMutex   *titles_mutex;
};



class MTD : public QObject
{

    Q_OBJECT

    //
    //  Core logic (wait for connections, launch transcoding
    //  threads)
    //

  public:
  
    MTD(int port, bool log_stdout);
    bool threadsShouldContinue(){return keep_running;}
    bool isItOkToStartTranscoding();
    
  signals:
  
    void writeToLog(const QString &entry);

  private slots:
  
    void newConnection(QSocket *);
    void endConnection(QSocket *);
    void readSocket();
    void parseTokens(const QStringList &tokens, QSocket *socket);
    void shutDown();
    void sendMessage(QSocket *where, const QString &what);
    void sayHi(QSocket *socket);
    void sendStatusReport(QSocket *socket);
    void sendMediaReport(QSocket *socket);
    void startJob(const QStringList &tokens);
    void startAbort(const QStringList &tokens);
    void startDVD(const QStringList &tokens);
    void cleanThreads();
    void checkDisc();
    bool checkFinalFile(QFile *final_file, const QString &extension);
    
  private:

    void customEvent(QCustomEvent *ce);
  
    MTDLogger           *mtd_log;    
    MTDServerSocket     *server_socket;
    QPtrList<JobThread> job_threads;
    QMutex              *dvd_drive_access;
    QMutex              *titles_mutex;
    bool                keep_running;
    bool                have_disc;
    QTimer              *thread_cleaning_timer;
    QTimer              *disc_checking_timer;
    DVDProbe            *dvd_probe;
    DiscCheckingThread  *disc_checking_thread;
    int                 nice_level;
    QMutex              *concurrent_transcodings_mutex;
    int                 concurrent_transcodings;
    int                 max_concurrent_transcodings;
};

#endif  // mtd_h_

