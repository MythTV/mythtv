// -*- Mode: c++ -*-
// vim:set sw=4 ts=4 expandtab:
#ifndef _PROGRAM_INFO_CACHE_H_
#define _PROGRAM_INFO_CACHE_H_

// ANSI C headers
#include <stdint.h>

// C++ headers
#include <vector>
#include <map>
using namespace std;

// Qt headers
#include <QWaitCondition>
#include <QDateTime>
#include <QMutex>

class ProgramInfoLoader;
class ProgramInfo;
class QObject;

class ProgramInfoCache
{
    friend class ProgramInfoLoader;
  public:
    ProgramInfoCache(QObject *o);
    ~ProgramInfoCache();

    void ScheduleLoad(const bool updateUI = true);
    bool IsLoadInProgress(void) const;
    void WaitForLoadToComplete(void) const;

    // All the following public methods must only be called from the UI Thread.
    void Refresh(void);
    void Add(const ProgramInfo&);
    bool Remove(uint chanid, const QDateTime &recstartts);
    bool Update(const ProgramInfo&);
    bool UpdateFileSize(uint chanid, const QDateTime &recstartts,
                        uint64_t filesize);
    QString GetRecGroup(uint chanid, const QDateTime &recstartts) const;
    void GetOrdered(vector<ProgramInfo*> &list, bool newest_first = false);
    /// \note This must only be called from the UI thread.
    bool empty(void) const { return m_cache.empty(); }
    ProgramInfo *GetProgramInfo(uint chanid, const QDateTime &recstartts) const;
    ProgramInfo *GetProgramInfo(const QString &piKey) const;

  private:
    void Load(const bool updateUI = true);
    void Clear(void);

  private:
    class PICKey
    {
      public:
        PICKey(uint c, const QDateTime &r) : chanid(c), recstartts(r) { }
        uint      chanid;
        QDateTime recstartts;
    };

    struct ltkey
    {
        bool operator()(const PICKey &a, const PICKey &b) const
        {
            if (a.recstartts == b.recstartts)
                return a.chanid < b.chanid;
            return (a.recstartts < b.recstartts);
        }
    };

    typedef map<PICKey,ProgramInfo*,ltkey> Cache;

    mutable QMutex          m_lock;
    Cache                   m_cache;
    vector<ProgramInfo*>   *m_next_cache;
    QObject                *m_listener;
    bool                    m_load_is_queued;
    uint                    m_loads_in_progress;
    mutable QWaitCondition  m_load_wait;
};

#endif // _PROGRAM_INFO_CACHE_H_
