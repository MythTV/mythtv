#ifndef COMMERCIALFLAG_H_
#define COMMERCIALFLAG_H_

class ProgramInfo;
class QSqlDatabase;

#include <qmap.h>
#include <qmutex.h>
#include <qobject.h>

using namespace std;

enum FlagStatus {
    FLAG_STARTING,
    FLAG_RUNNING,
    FLAG_STOP,
    FLAG_ABORT
};

class CommercialFlagger : public QObject
{
  public:
     CommercialFlagger(bool master, QSqlDatabase *ldb);
     ~CommercialFlagger(void);
     void customEvent(QCustomEvent *e);

  protected:
    static void *RestartUnfinishedJobs(void *param);
    void DoRestartUnfinishedJobs(void);

  private:
    void ProcessUnflaggedRecordings(QString flagHost = "");
    void FlagCommercials(ProgramInfo *tmpInfo);
    static void *FlagCommercialsThread(void *param);
    void DoFlagCommercialsThread(void);

    QMutex dblock;
    QMutex eventlock;
    QSqlDatabase *db;

    ProgramInfo *flagInfo;

    QMap<QString, int> flaggingList;
    QMap<QString, bool *> flaggingSems;

    pthread_t commercials;

    bool commercialFlagThreadStarted;
    bool flagThreadStarted;
    bool isMaster;
};

#endif

