// -*- Mode: c++ -*-
// vim:set sw=4 ts=4 expandtab:
// Copyright (c) 2009, Daniel Thor Kristjansson
// Distributed as part of MythTV under GPL version 2
// (or at your option a later version)

#include "programinfocache.h"
#include "mthreadpool.h"
#include "mythlogging.h"
#include "programinfo.h"
#include "remoteutil.h"
#include "mythevent.h"

#include <QCoreApplication>
#include <QRunnable>

#include <algorithm>

using VPI_ptr = vector<ProgramInfo *> *;
static void free_vec(VPI_ptr &v)
{
    if (v)
    {
        auto it = v->begin();
        for (; it != v->end(); ++it)
            delete *it;
        delete v;
        v = nullptr;
    }
}

class ProgramInfoLoader : public QRunnable
{
  public:
    ProgramInfoLoader(ProgramInfoCache &c, const bool updateUI)
      : m_cache(c), m_updateUI(updateUI) {}

    void run(void) override // QRunnable
    {
        m_cache.Load(m_updateUI);
    }

    ProgramInfoCache &m_cache;
    bool              m_updateUI;
};

ProgramInfoCache::~ProgramInfoCache()
{
    QMutexLocker locker(&m_lock);

    while (m_loads_in_progress)
        m_load_wait.wait(&m_lock);

    Clear();
    free_vec(m_next_cache);
}

void ProgramInfoCache::ScheduleLoad(const bool updateUI)
{
    QMutexLocker locker(&m_lock);
    if (!m_load_is_queued)
    {
        m_load_is_queued = true;
        m_loads_in_progress++;
        MThreadPool::globalInstance()->start(
            new ProgramInfoLoader(*this, updateUI), "ProgramInfoLoader");
    }
}

void ProgramInfoCache::Load(const bool updateUI)
{
    QMutexLocker locker(&m_lock);
    m_load_is_queued = false;

    locker.unlock();
    /**/
    // Get an unsorted list (sort = 0) from RemoteGetRecordedList
    // we sort the list later anyway.
    vector<ProgramInfo*> *tmp = RemoteGetRecordedList(0);
    /**/
    locker.relock();

    free_vec(m_next_cache);
    m_next_cache = tmp;

    if (updateUI)
        QCoreApplication::postEvent(
            m_listener, new MythEvent("UPDATE_UI_LIST"));

    m_loads_in_progress--;
    m_load_wait.wakeAll();
}

bool ProgramInfoCache::IsLoadInProgress(void) const
{
    QMutexLocker locker(&m_lock);
    return m_loads_in_progress != 0U;
}

void ProgramInfoCache::WaitForLoadToComplete(void) const
{
    QMutexLocker locker(&m_lock);
    while (m_loads_in_progress)
        m_load_wait.wait(&m_lock);
}

/** \brief Refreshed the cache.
 *
 *  If a new list has been loaded this fills the cache with that list
 *  if not, this simply removes list items marked for deletion from the
 *  the list.
 *
 *  \note This must only be called from the UI thread.
 *  \note All references to the ProgramInfo pointers should be cleared
 *        before this is called.
 */
void ProgramInfoCache::Refresh(void)
{
    QMutexLocker locker(&m_lock);
    if (m_next_cache)
    {
        Clear();
        auto it = m_next_cache->begin();
        for (; it != m_next_cache->end(); ++it)
        {
            if (!(*it)->GetChanID())
                continue;

            m_cache[(*it)->GetRecordingID()] = *it;
        }
        delete m_next_cache;
        m_next_cache = nullptr;
        return;
    }
    locker.unlock();

    Cache::iterator it = m_cache.begin();
    Cache::iterator nit = it;
    for (; it != m_cache.end(); it = nit)
    {
        nit = it;
        ++nit;

        if ((*it)->GetAvailableStatus() == asDeleted)
        {
            delete (*it);
            m_cache.erase(it);
        }
    }
}

/** \brief Updates a ProgramInfo in the cache.
 *  \note This must only be called from the UI thread.
 *  \return True iff the ProgramInfo was in the cache and was updated.
 */
bool ProgramInfoCache::Update(const ProgramInfo &pginfo)
{
    QMutexLocker locker(&m_lock);

    Cache::iterator it = m_cache.find(pginfo.GetRecordingID());

    if (it != m_cache.end())
        (*it)->clone(pginfo, true);

    return it != m_cache.end();
}

/** \brief Updates a ProgramInfo in the cache.
 *  \note This must only be called from the UI thread.
 *  \return True iff the ProgramInfo was in the cache and was updated.
 */
bool ProgramInfoCache::UpdateFileSize(uint recordingID, uint64_t filesize)
{
    QMutexLocker locker(&m_lock);

    Cache::iterator it = m_cache.find(recordingID);

    if (it != m_cache.end())
    {
        (*it)->SetFilesize(filesize);
        if (filesize)
            (*it)->SetAvailableStatus(asAvailable, "PIC::UpdateFileSize");
    }

    return it != m_cache.end();
}

/** \brief Returns the ProgramInfo::recgroup or an empty string if not found.
 *  \note This must only be called from the UI thread.
 */
QString ProgramInfoCache::GetRecGroup(uint recordingID) const
{
    QMutexLocker locker(&m_lock);

    Cache::const_iterator it = m_cache.find(recordingID);

    QString recgroup;
    if (it != m_cache.end())
        recgroup = (*it)->GetRecordingGroup();

    return recgroup;
}

/** \brief Adds a ProgramInfo to the cache.
 *  \note This must only be called from the UI thread.
 */
void ProgramInfoCache::Add(const ProgramInfo &pginfo)
{
    if (!pginfo.GetRecordingID() || Update(pginfo))
        return;

    m_cache[pginfo.GetRecordingID()] = new ProgramInfo(pginfo);
}

/** \brief Marks a ProgramInfo in the cache for deletion on the next
 *         call to Refresh().
 *  \note This must only be called from the UI thread.
 *  \return True iff the ProgramInfo was in the cache.
 */
bool ProgramInfoCache::Remove(uint recordingID)
{
    Cache::iterator it = m_cache.find(recordingID);

    if (it != m_cache.end())
        (*it)->SetAvailableStatus(asDeleted, "PIC::Remove");

    return it != m_cache.end();
}

// two helper functions that are used only in this file
namespace {
    // Sorting functions for ProgramInfoCache::GetOrdered()
    bool PISort(const ProgramInfo *a, const ProgramInfo *b)
    {
        if (a->GetRecordingStartTime() == b->GetRecordingStartTime())
            return a->GetChanID() < b->GetChanID();
        return (a->GetRecordingStartTime() < b->GetRecordingStartTime());
    }

    bool reversePISort(const ProgramInfo *a, const ProgramInfo *b)
    {
        if (a->GetRecordingStartTime() == b->GetRecordingStartTime())
            return a->GetChanID() > b->GetChanID();
        return (a->GetRecordingStartTime() > b->GetRecordingStartTime());
    }
}

void ProgramInfoCache::GetOrdered(vector<ProgramInfo*> &list, bool newest_first)
{
    for (Cache::iterator it = m_cache.begin(); it != m_cache.end(); ++it)
            list.push_back(*it);

    if (newest_first)
        std::sort(list.begin(), list.end(), reversePISort);
    else
        std::sort(list.begin(), list.end(), PISort);

}

ProgramInfo *ProgramInfoCache::GetRecordingInfo(uint recordingID) const
{
    Cache::const_iterator it = m_cache.find(recordingID);

    if (it != m_cache.end())
        return *it;

    return nullptr;
}

/// Clears the cache, m_lock must be held when this is called.
void ProgramInfoCache::Clear(void)
{
    for (Cache::iterator it = m_cache.begin(); it != m_cache.end(); ++it)
        delete (*it);
    m_cache.clear();
}
