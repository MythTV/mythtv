#ifndef AUTOEXPIRE_H_
#define AUTOEXPIRE_H_

#include <pthread.h>

#include <list>
#include <vector>
#include <set>

#include <qmap.h> 
#include <qmutex.h>
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
    bool          expire_thread_running;
    bool          is_master_backend;

    // update info
    bool          update_pending;
    pthread_t     update_thread;
    enclinklist_t encoder_links;

    friend void *SpawnUpdateThread(void *param);
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
