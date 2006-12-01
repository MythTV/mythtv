#ifndef AUTOEXPIRE_H_
#define AUTOEXPIRE_H_

#include <pthread.h>

#include <list>
#include <vector>
#include <set>

#include <qmap.h> 
#include <qmutex.h>
#include <qwaitcondition.h>
#include <qobject.h>

using namespace std;
class ProgramInfo;
class EncoderLink;
class FileSystemInfo;

typedef vector<ProgramInfo*> pginfolist_t;
typedef vector<EncoderLink*> enclinklist_t;

enum ExpireMethodType {
    emOldestFirst          = 1,
    emLowestPriorityFirst  = 2,
    emWeightedTimePriority = 3,
    emShortLiveTVPrograms  = 10000,
    emNormalLiveTVPrograms = 10001
};

class AutoExpire : public QObject
{
  public:
    AutoExpire(QMap<int, EncoderLink *> *encoderList);
    AutoExpire(void);
   ~AutoExpire();

    void CalcParams(void);
    void PrintExpireList(void);

    size_t GetDesiredSpace(void) const
        { return desired_space; }

    size_t GetMaxRecordRate(void) const
        { return max_record_rate; }

    void GetAllExpiring(QStringList &strList);
    void GetAllExpiring(pginfolist_t &list);

    static void Update(bool immediately);

    QMap<int, EncoderLink *> *encoderList;

  protected:
    void RunExpirer(void);
    static void *ExpirerThread(void *param);

  private:
    void Init(void);

    void ExpireLiveTV(int type);
    void ExpireRecordings(void);
    void ExpireEpisodesOverMax(void);

    void FillExpireList(pginfolist_t &expireList);
    void FillDBOrdered(pginfolist_t &expireList, int expMethod);
    void SendDeleteMessages(pginfolist_t &deleteList);
    void ClearExpireList(pginfolist_t &expireList, bool deleteProg = true);
    void Sleep(int sleepTime);

    void UpdateDontExpireSet(void);
    bool IsInDontExpireSet(QString chanid, QDateTime starttime);
    bool IsInExpireList(pginfolist_t &expireList, QString chanid,
                        QDateTime starttime);

    // main expire info
    set<QString>  dont_expire_set;
    pthread_t     expire_thread;
    QMutex        instance_lock;
    size_t        desired_space;
    uint          desired_freq;
    size_t        max_record_rate; // bytes/sec
    bool          expire_thread_running;

    // update info
    bool          update_pending;
    pthread_t     update_thread;

    friend void *SpawnUpdateThread(void *param);
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
