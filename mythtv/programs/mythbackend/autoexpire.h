#ifndef AUTOEXPIRE_H_
#define AUTOEXPIRE_H_

#include <cstdint>
#include <vector>

#include <QWaitCondition>
#include <QDateTime>
#include <QPointer>
#include <QObject>
#include <QString>
#include <QMutex>
#include <QQueue>
#include <QSet>
#include <QMap>

#include "libmythbase/mthread.h"

class ProgramInfo;
class EncoderLink;
class MainServer;

using pginfolist_t  = std::vector<ProgramInfo*>;
using enclinklist_t = std::vector<EncoderLink*>;

enum ExpireMethodType : std::uint16_t {
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
    ~ExpireThread() override { wait(); }
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
   ~AutoExpire() override;

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
        QMutexLocker locker(&m_instanceLock);
        m_mainServer = ms;
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
    void Sleep(std::chrono::milliseconds sleepTime);

    void UpdateDontExpireSet(void);
    bool IsInDontExpireSet(uint chanid, const QDateTime &recstartts) const;
    static bool IsInExpireList(const pginfolist_t &expireList,
                               uint chanid, const QDateTime &recstartts);

    // main expire info
    QSet<QString> m_dontExpireSet;
    ExpireThread *m_expireThread      {nullptr}; // protected by m_instanceLock
    uint          m_desiredFreq       {15};      // protected by m_instanceLock
    bool          m_expireThreadRun   {false};   // protected by m_instanceLock

    QMap<int, int64_t>  m_desiredSpace;          // protected by m_instanceLock
    QMap<int, int>      m_usedEncoders;          // protected by m_instanceLock

    mutable QMutex m_instanceLock;
    QWaitCondition m_instanceCond;               // protected by m_instanceLock

    MainServer    *m_mainServer       {nullptr};  // protected by m_instanceLock

    // update info
    QMutex              m_updateLock;
    QQueue<UpdateEntry> m_updateQueue;           // protected by m_updateLock
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
