// -*- Mode: c++ -*-
// vim:set sw=4 ts=4 expandtab:
#ifndef _PROGRAM_INFO_CACHE_H_
#define _PROGRAM_INFO_CACHE_H_

// ANSI C headers
#include <stdint.h>

// C++ headers
#include <vector>
using namespace std;

// Qt headers
#include <QWaitCondition>
#include <QDateTime>
#include <QMutex>
#include <QHash>

class ProgramInfoLoader;
class ProgramInfo;
class QObject;

typedef enum {
    PIC_NONE              = 0x00,
    PIC_MARK_CHANGED      = 0x01,
    PIC_RECGROUP_CHANGED  = 0x02,
    PIC_PART_WATCHED      = 0X04,
    PIC_WATCHLIST_CHANGED = 0x08,
    PIC_NO_ACTION         = 0x80,
} UpdateState;

class ProgramInfoCache
{
    friend class ProgramInfoLoader;
  public:
    explicit ProgramInfoCache(QObject *o);
    ~ProgramInfoCache();

    void ScheduleLoad(const bool updateUI = true);
    bool IsLoadInProgress(void) const;
    void WaitForLoadToComplete(void) const;

    // All the following public methods must only be called from the UI Thread.
    void Refresh(void);
    void Add(const ProgramInfo&);
    bool Remove(uint recordingID);
    uint32_t Update(const ProgramInfo& pginfo);
    void UpdateFileSize(uint recordingID, uint64_t filesize, uint32_t flags);
    void GetOrdered(vector<ProgramInfo*> &list, bool newest_first = false);
    /// \note This must only be called from the UI thread.
    bool empty(void) const { return m_cache.empty(); }
    ProgramInfo *GetRecordingInfo(uint recordingID) const;

  private:
    void Load(const bool updateUI = true);
    void Clear(void);
    void CalculateProgress(ProgramInfo &pg, int playPos);
    void LoadProgressMarks();

  private:
    // NOTE: Hash would be faster for lookups and updates, but we need a sorted
    // list for to rebuild the full list. Question is, which is done more?
    // We could store a hash, but sort the vector in GetOrdered which might
    // be a suitable compromise, fractionally slower initial load but faster
    // scrolling and updates
    typedef QHash<uint,ProgramInfo*> Cache;

    mutable QMutex          m_lock;
    Cache                   m_cache;
    vector<ProgramInfo*>   *m_next_cache;
    QObject                *m_listener;
    bool                    m_load_is_queued;
    uint                    m_loads_in_progress;
    mutable QWaitCondition  m_load_wait;
};

#endif // _PROGRAM_INFO_CACHE_H_
