#ifndef _PROGRAM_INFO_UPDATER_H_
#define _PROGRAM_INFO_UPDATER_H_

// ANSI C headers
#include <stdint.h> // for [u]int[32,64]_t

// C++ headers
#include <vector>
using namespace std;

// Qt headers
#include <QThreadPool>
#include <QDateTime>
#include <QMutex>
#include <QHash>
#include <QWaitCondition>

typedef enum PIAction {
    kPIAdd,
    kPIDelete,
    kPIUpdate,
    kPIUpdateFileSize,
} PIAction;

class PIKey
{
  public:
    PIKey(uint c, const QDateTime &r) : chanid(c), recstartts(r) {}

    uint      chanid;
    QDateTime recstartts;

    bool operator==(const PIKey &other) const
    {
        return (chanid     == other.chanid &&
                recstartts == other.recstartts);
    }
};
uint qHash(const PIKey &k);

class PIKeyAction : public PIKey
{
  public:
    PIKeyAction(uint c, const QDateTime &r, PIAction a) :
        PIKey(c, r), action(a) { }

    PIAction action;
};

class PIKeyData
{
  public:
    PIKeyData(PIAction a, uint64_t f) : action(a), filesize(f) { }
    PIAction action;
    uint64_t filesize;
};

class ProgramInfoUpdater : public QRunnable
{
  public:
    ProgramInfoUpdater() : isRunning(false) { setAutoDelete(false); }

    void insert(uint     chanid, const QDateTime &recstartts,
                PIAction action, uint64_t         filesize = 0ULL);
    void run(void);

  private:
    QMutex        lock;
    QWaitCondition moreWork; 
    bool          isRunning;
    vector<PIKeyAction>    needsAddDelete;
    QHash<PIKey,PIKeyData> needsUpdate;
};

#endif // _PROGRAM_INFO_UPDATER_H_
