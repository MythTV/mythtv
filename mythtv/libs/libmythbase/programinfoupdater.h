#ifndef PROGRAM_INFO_UPDATER_H
#define PROGRAM_INFO_UPDATER_H

// C++ headers
#include <cstdint> // for [u]int[32,64]_t
#include <vector>

// Qt headers
#include <QWaitCondition>
#include <QDateTime>
#include <QRunnable>
#include <QMutex>
#include <QHash>

// Myth
#include "mythbaseexp.h"

enum PIAction : std::uint8_t {
    kPIAdd,
    kPIDelete,
    kPIUpdate,
    kPIUpdateFileSize,
};

class MBASE_PUBLIC PIKeyAction
{
  public:
    PIKeyAction(uint recordedid, PIAction a) :
        m_recordedid(recordedid), m_action(a) { }

    uint     m_recordedid;
    PIAction m_action;

    bool operator==(PIKeyAction other) const
    {
        return (m_recordedid == other.m_recordedid);
    }
};

class MBASE_PUBLIC PIKeyData
{
  public:
    PIKeyData(PIAction a, uint64_t f) : m_action(a), m_filesize(f) { }
    PIAction m_action;
    uint64_t m_filesize;
};

class MBASE_PUBLIC ProgramInfoUpdater : public QRunnable
{
  public:
    ProgramInfoUpdater() { setAutoDelete(false); }

    void insert(uint     recordedid,
                PIAction action, uint64_t         filesize = 0ULL);
    void run(void) override; // QRunnable

  private:
    QMutex                   m_lock;
    QWaitCondition           m_moreWork; 
    bool                     m_isRunning {false};
    std::vector<PIKeyAction> m_needsAddDelete;
    QHash<uint,PIKeyData>    m_needsUpdate;
};

#endif // PROGRAM_INFO_UPDATER_H
