// -*- Mode: c++ -*-
// vim:set sw=4 ts=4 expandtab:
#ifndef _PROGRAM_INFO_CACHE_H_
#define _PROGRAM_INFO_CACHE_H_

// C++ headers
#include <cstdint>
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

class ProgramInfoCache
{
    friend class ProgramInfoLoader;
  public:
    explicit ProgramInfoCache(QObject *o)
        : m_listener(o) {}
    ~ProgramInfoCache();

    void ScheduleLoad(const bool updateUI = true);
    bool IsLoadInProgress(void) const;
    void WaitForLoadToComplete(void) const;

    // All the following public methods must only be called from the UI Thread.
    void Refresh(void);
    void Add(const ProgramInfo&);
    bool Remove(uint recordingID);
    bool Update(const ProgramInfo&);
    bool UpdateFileSize(uint recordingID, uint64_t filesize);
    QString GetRecGroup(uint recordingID) const;
    void GetOrdered(vector<ProgramInfo*> &list, bool newest_first = false);
    /// \note This must only be called from the UI thread.
    bool empty(void) const { return m_cache.empty(); }
    ProgramInfo *GetRecordingInfo(uint recordingID) const;

  private:
    void Load(const bool updateUI = true);
    void Clear(void);

  private:
    // NOTE: Hash would be faster for lookups and updates, but we need a sorted
    // list for to rebuild the full list. Question is, which is done more?
    // We could store a hash, but sort the vector in GetOrdered which might
    // be a suitable compromise, fractionally slower initial load but faster
    // scrolling and updates
    using Cache = QHash<uint,ProgramInfo*>;

    mutable QMutex          m_lock;
    Cache                   m_cache;
    vector<ProgramInfo*>   *m_next_cache        {nullptr};
    QObject                *m_listener          {nullptr};
    bool                    m_load_is_queued    {false};
    uint                    m_loads_in_progress {0};
    mutable QWaitCondition  m_load_wait;
};

#endif // _PROGRAM_INFO_CACHE_H_
