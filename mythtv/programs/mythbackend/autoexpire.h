#ifndef AUTOEXPIRE_H_
#define AUTOEXPIRE_H_

#include <stdint.h>

#include <vector>
using namespace std;

#include <QWaitCondition>
#include <QObject>
#include <QString>
#include <QMutex>
#include <QSet>
#include <QMap>
#include <QDateTime>
#include <QThread>
#include <QPointer>

class ProgramInfo;
class EncoderLink;
class FileSystemInfo;
class MainServer;

typedef vector<ProgramInfo*> pginfolist_t;
typedef vector<EncoderLink*> enclinklist_t;

enum ExpireMethodType {
    emOldestFirst           = 1,
    emLowestPriorityFirst   = 2,
    emWeightedTimePriority  = 3,
    emShortLiveTVPrograms   = 10000,
    emNormalLiveTVPrograms  = 10001,
    emOldDeletedPrograms    = 10002,
    emNormalDeletedPrograms = 10003
};

class AutoExpire;

class ExpireThread : public QThread
{
    Q_OBJECT
  public:
    ExpireThread() : m_parent(NULL) {}
    void SetParent(AutoExpire *parent) { m_parent = parent; }
    void run(void);
  private:
    AutoExpire *m_parent;
};

class UpdateThread : public QThread
{
    Q_OBJECT
  public:
    UpdateThread() : m_parent(NULL) {}
    void SetParent(AutoExpire *parent) { m_parent = parent; }
    void run(void);
  private:
    AutoExpire *m_parent;
};

class AutoExpire : public QObject
{
    Q_OBJECT

    friend class ExpireThread;
    friend class UpdateThread;
  public:
    AutoExpire(QMap<int, EncoderLink *> *encoderList);
    AutoExpire(void);
   ~AutoExpire();

    void CalcParams(void);
    void PrintExpireList(QString expHost = "ALL");

    size_t GetDesiredSpace(int fsID) const;

    void GetAllExpiring(QStringList &strList);
    void GetAllExpiring(pginfolist_t &list);
    void ClearExpireList(pginfolist_t &expireList, bool deleteProg = true);

    static void Update(int encoder, int fsID, bool immediately);
    static void Update(bool immediately) { Update(0, -1, immediately); }

    void SetMainServer(MainServer *ms) { mainServer = ms; }

    QMap<int, EncoderLink *> *encoderList;

  protected:
    void RunExpirer(void);

  private:
    void Init(void);

    void ExpireLiveTV(int type);
    void ExpireOldDeleted(void);
    void ExpireRecordings(void);
    void ExpireEpisodesOverMax(void);

    void FillExpireList(pginfolist_t &expireList);
    void FillDBOrdered(pginfolist_t &expireList, int expMethod);
    void SendDeleteMessages(pginfolist_t &deleteList);
    void Sleep(int sleepTime);

    void UpdateDontExpireSet(void);
    bool IsInDontExpireSet(uint chanid, const QDateTime &recstartts) const;
    static bool IsInExpireList(const pginfolist_t &expireList,
                               uint chanid, const QDateTime &recstartts);

    // main expire info
    QSet<QString> dont_expire_set;
    QSet<QString> deleted_set;
    ExpireThread  expireThread;
    uint          desired_freq;
    bool          expire_thread_run;

    QMap<int, uint64_t> desired_space;
    QMap<int, int>      used_encoders;

    QMutex         instance_lock;
    QWaitCondition instance_cond;

    MainServer *mainServer;

    // update info
    bool                    update_pending;
    QPointer<UpdateThread>  updateThread;
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
