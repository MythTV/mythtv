#ifndef _PROGRAM_INFO_UPDATER_H_
#define _PROGRAM_INFO_UPDATER_H_

// ANSI C headers
#include <stdint.h> // for [u]int[32,64]_t

// C++ headers
#include <vector>

// Qt headers
#include <QWaitCondition>
#include <QDateTime>
#include <QRunnable>
#include <QMutex>
#include <QHash>

// Myth
#include "mythexp.h"

typedef enum PIAction {
    kPIAdd,
    kPIDelete,
    kPIUpdate,
    kPIUpdateFileSize,
} PIAction;

class MPUBLIC PIKeyAction
{
  public:
    PIKeyAction(uint recordedid, PIAction a) :
        recordedid(recordedid), action(a) { }

    uint recordedid;
    PIAction action;

    bool operator==(const PIKeyAction &other) const
    {
        return (recordedid == other.recordedid);
    }
};

class MPUBLIC PIKeyData
{
  public:
    PIKeyData(PIAction a, uint64_t f) : action(a), filesize(f) { }
    PIAction action;
    uint64_t filesize;
};

class MPUBLIC ProgramInfoUpdater : public QRunnable
{
  public:
    ProgramInfoUpdater() : isRunning(false) { setAutoDelete(false); }

    void insert(uint     recordedid,
                PIAction action, uint64_t         filesize = 0ULL);
    void run(void);

  private:
    QMutex        lock;
    QWaitCondition moreWork; 
    bool          isRunning;
    std::vector<PIKeyAction>    needsAddDelete;
    QHash<uint,PIKeyData> needsUpdate;
};

#endif // _PROGRAM_INFO_UPDATER_H_
