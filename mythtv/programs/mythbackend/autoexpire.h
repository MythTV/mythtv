#ifndef AUTOEXPIRE_H_
#define AUTOEXPIRE_H_

#include <cstdint>
#include <vector>
using namespace std;

#include <QWaitCondition>
#include <QDateTime>
#include <QPointer>
#include <QObject>
#include <QString>
#include <QMutex>
#include <QQueue>
#include <QSet>
#include <QMap>

#include "mthread.h"

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
    emNormalDeletedPrograms = 10003,
    emQuickDeletedPrograms  = 10004
};

class AutoExpire;

class ExpireThread : public MThread
{
  public:
    explicit ExpireThread(AutoExpire *p) : MThread("Expire"), m_parent(p) {}
    virtual ~ExpireThread() { wait(); }
    void run(void) override; // MThread
  private:
    QPointer<AutoExpire> m_parent;
};

class UpdateEntry
{
  public:
    UpdateEntry(int _encoder, int _fsID)
        : m_encoder(_encoder), m_fsID(_fsID) {};

    int m_encoder;
    int m_fsID;
};

class AutoExpire : public QObject
{
    Q_OBJECT

    friend class ExpireThread;
  public:
    explicit AutoExpire(QMap<int, EncoderLink *> *tvList);
    AutoExpire() = default;
   ~AutoExpire();

    void CalcParams(void);
    void PrintExpireList(const QString& expHost = "ALL");

    uint64_t GetDesiredSpace(int fsID) const;

    void GetAllExpiring(QStringList &strList);
    void GetAllExpiring(pginfolist_t &list);
    static void ClearExpireList(pginfolist_t &expireList, bool deleteProg = true);

    static void Update(int encoder, int fsID, bool immediately);
    static void Update(bool immediately) { Update(0, -1, immediately); }

    void SetMainServer(MainServer *ms)
    {
        QMutexLocker locker(&m_instance_lock);
        m_main_server = ms;
    }

    QMap<int, EncoderLink *> *m_encoderList {nullptr};

  protected:
    void RunExpirer(void);

  private:
    void ExpireLiveTV(int type);
    void ExpireOldDeleted(void);
    void ExpireQuickDeleted(void);
    void ExpireRecordings(void);
    void ExpireEpisodesOverMax(void);

    void FillExpireList(pginfolist_t &expireList);
    void FillDBOrdered(pginfolist_t &expireList, int expMethod);
    static void SendDeleteMessages(pginfolist_t &deleteList);
    void Sleep(int sleepTime /*ms*/);

    void UpdateDontExpireSet(void);
    bool IsInDontExpireSet(uint chanid, const QDateTime &recstartts) const;
    static bool IsInExpireList(const pginfolist_t &expireList,
                               uint chanid, const QDateTime &recstartts);

    // main expire info
    QSet<QString> m_dont_expire_set;
    ExpireThread *m_expire_thread      {nullptr}; // protected by instance_lock
    uint          m_desired_freq       {15};      // protected by instance_lock
    bool          m_expire_thread_run  {false};   // protected by instance_lock

    QMap<int, int64_t>  m_desired_space;          // protected by instance_lock
    QMap<int, int>      m_used_encoders;          // protected by instance_lock

    mutable QMutex m_instance_lock;
    QWaitCondition m_instance_cond;               // protected by instance_lock

    MainServer    *m_main_server      {nullptr};  // protected by instance_lock

    // update info
    QMutex              m_update_lock;
    QQueue<UpdateEntry> m_update_queue;           // protected by update_lock
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
