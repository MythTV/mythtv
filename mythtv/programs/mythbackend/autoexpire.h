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
    AutoExpire(bool runthread, bool master);
   ~AutoExpire();

    void CalcParams(vector<EncoderLink*>);
    void FillExpireList();
    void PrintExpireList();
    void TruncatePending(void);
    void TruncateFinished(void);

    size_t GetMaxRecordRate(void) const
        { return max_record_rate; }

    size_t GetMinTruncateRate(void) const
        { return ((max_record_rate * 6) / 5) + 1; }

    void GetAllExpiring(QStringList &strList);
    void GetAllExpiring(pginfolist_t &list);

    static void Update(QMap<int, EncoderLink*>*, bool immediately);
  protected:
    void RunExpirer(void);
    static void *ExpirerThread(void *param);

  private:
    void ExpireLiveTV(int type);
    void ExpireRecordings(void);
    void ExpireEpisodesOverMax(void);

    void FillDBOrdered(int expMethod, bool allHosts = false);
    void SendDeleteMessages(size_t availFreeKB, size_t desiredFreeKB,
                            bool deleteAll = false);
    void ClearExpireList(void);
    void Sleep(int sleepTime);
    void WaitForPendingTruncates(void);

    void UpdateDontExpireSet(void);
    bool IsInDontExpireSet(QString chanid, QDateTime starttime);
    bool IsInExpireList(QString chanid, QDateTime starttime);

    // main expire info
    set<QString>  dont_expire_set;
    pginfolist_t  expire_list;
    pthread_t     expire_thread;
    QMutex        instance_lock;
    QString       record_file_prefix;
    size_t        desired_space;
    uint          desired_freq;
    size_t        max_record_rate; // bytes/sec
    bool          expire_thread_running;
    bool          is_master_backend;

    // Pending truncates monitor
    mutable QMutex truncate_monitor_lock;
    QWaitCondition truncate_monitor_condition;
    int            truncates_pending;

    // update info
    bool          update_pending;
    pthread_t     update_thread;
    enclinklist_t encoder_links;

    friend void *SpawnUpdateThread(void *param);
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
