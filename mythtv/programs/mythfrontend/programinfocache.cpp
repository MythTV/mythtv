// -*- Mode: c++ -*-
// vim:set sw=4 ts=4 expandtab:
// Copyright (c) 2009, Daniel Thor Kristjansson
// Distributed as part of MythTV under GPL version 2
// (or at your option a later version)

// C++
#include <algorithm>

// Qt
#include <QCoreApplication>
#include <QRunnable>

// MythTV
#include "libmythbase/mconcurrent.h"
#include "libmythbase/mthreadpool.h"
#include "libmythbase/mythdb.h"
#include "libmythbase/mythevent.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/programinfo.h"
#include "libmythbase/remoteutil.h"

// MythFrontend
#include "programinfocache.h"

using VPI_ptr = std::vector<ProgramInfo *> *;
static void free_vec(VPI_ptr &v)
{
    if (v)
    {
        for (auto & it : *v)
            delete it;
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

    while (m_loadsInProgress)
        m_loadWait.wait(&m_lock);

    Clear();
    free_vec(m_nextCache);
}

void ProgramInfoCache::ScheduleLoad(const bool updateUI)
{
    QMutexLocker locker(&m_lock);
    if (!m_loadIsQueued)
    {
        m_loadIsQueued = true;
        m_loadsInProgress++;
        MThreadPool::globalInstance()->start(
            new ProgramInfoLoader(*this, updateUI), "ProgramInfoLoader");
    }
}

void ProgramInfoCache::Load(const bool updateUI)
{
    QMutexLocker locker(&m_lock);
    m_loadIsQueued = false;

    locker.unlock();

    // Get an unsorted list (sort = 0) from RemoteGetRecordedList
    // we sort the list later anyway.
    std::vector<ProgramInfo*> *tmp = RemoteGetRecordedList(0);

    // Calculate play positions for UI
    if (tmp)
    {
        // Played progress
        using ProgId = QPair<uint, QDateTime>;
        QHash<ProgId, uint> lastPlayFrames;

        // Get all lastplaypos marks in a single lookup
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT chanid, starttime, mark "
                      "FROM recordedmarkup "
                      "WHERE type = :TYPE ");
        query.bindValue(":TYPE", MARK_UTIL_LASTPLAYPOS);

        if (query.exec())
        {
            while (query.next())
            {
                ProgId id = qMakePair(query.value(0).toUInt(),
                                      MythDate::as_utc(query.value(1).toDateTime()));
                lastPlayFrames[id] = query.value(2).toUInt();
            }

            // Determine progress of each prog
            for (ProgramInfo* pg : *tmp)
            {
                ProgId id = qMakePair(pg->GetChanID(),
                                      pg->GetRecordingStartTime());
                pg->CalculateProgress(lastPlayFrames.value(id));
            }
        }
        else
        {
            MythDB::DBError("Watched progress", query);
        }
     }

   locker.relock();

    free_vec(m_nextCache);
    m_nextCache = tmp;

    if (updateUI)
        QCoreApplication::postEvent(
            m_listener, new MythEvent("UPDATE_UI_LIST"));

    m_loadsInProgress--;
    m_loadWait.wakeAll();
}

bool ProgramInfoCache::IsLoadInProgress(void) const
{
    QMutexLocker locker(&m_lock);
    return m_loadsInProgress != 0U;
}

void ProgramInfoCache::WaitForLoadToComplete(void) const
{
    QMutexLocker locker(&m_lock);
    while (m_loadsInProgress)
        m_loadWait.wait(&m_lock);
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
    if (m_nextCache)
    {
        Clear();
        for (auto & it : *m_nextCache)
        {
            if (!it->GetChanID())
                continue;

            if (m_cache.contains(it->GetRecordingID()))
            {
                // An entry using that key already exists in hash.
                // Free allocated memory for the entry to be replaced.
                delete m_cache[it->GetRecordingID()];
            }

            m_cache[it->GetRecordingID()] = it;
        }
        delete m_nextCache;
        m_nextCache = nullptr;
        return;
    }

    for (auto it = m_cache.begin(); it != m_cache.end(); )
    {
        if ((*it)->GetAvailableStatus() == asDeleted)
        {
            delete (*it);
            it = m_cache.erase(it);
        }
        else
        {
            it++;
        }
    }
}

/** \brief Updates a ProgramInfo in the cache.
 *  \note This must only be called from the UI thread.
 *  \return True iff the ProgramInfo was in the cache and was updated.
 */
ProgramInfoCache::UpdateStates ProgramInfoCache::Update(const ProgramInfo &pginfo)
{
    QMutexLocker locker(&m_lock);

    uint recordingId = pginfo.GetRecordingID();
    Cache::iterator it = m_cache.find(recordingId);

    if (it == m_cache.end())
        return PIC_NO_ACTION;

    ProgramInfo& pg = **it;
    UpdateStates flags { PIC_NONE };

    if (pginfo.GetBookmarkUpdate() != pg.m_previewUpdate)
        flags |= PIC_MARK_CHANGED;

    if (pginfo.GetRecordingGroup() != pg.GetRecordingGroup())
        flags |= PIC_RECGROUP_CHANGED;

    pg.clone(pginfo, true);

    if (flags & PIC_MARK_CHANGED)
    {
        // Delegate this update to a background task
        MConcurrent::run("UpdateProg", this, &ProgramInfoCache::UpdateFileSize,
                         recordingId, 0, flags);

        // Ignore this update
        flags = PIC_NO_ACTION;
    }

    LOG(VB_GUI, LOG_DEBUG, QString("Pg %1 %2 update state %3")
        .arg(recordingId).arg(pg.GetTitle()).arg(flags));
    return flags;
}

/** \brief Updates a ProgramInfo in the cache.
 *  \note This runs in a background thread as it contains multiple Db
 *   queries.
 */
void ProgramInfoCache::UpdateFileSize(uint recordingID, uint64_t filesize,
                                      UpdateStates flags)
{
    Cache::iterator it = m_cache.find(recordingID);
    if (it == m_cache.end())
        return;

    ProgramInfo *pg = *it;

    pg->CalculateProgress(pg->QueryLastPlayPos());

    if (filesize > 0)
    {
        // Filesize update
        pg->SetFilesize(filesize);
        pg->SetAvailableStatus(asAvailable, "PIC::UpdateFileSize");
    }
    else // Info update
    {
        // Don't keep regenerating previews of files being played
        QString byWhom;
        if (pg->QueryIsInUse(byWhom) && byWhom.contains(QObject::tr("Playing")))
            flags &= ~PIC_MARK_CHANGED;
    }

    // Time of preview picture generation request for next comparison
    if (flags & PIC_MARK_CHANGED)
        pg->m_previewUpdate = pg->GetBookmarkUpdate();

    QString mesg = QString("UPDATE_UI_ITEM %1 %2").arg(recordingID).arg(flags);
    QCoreApplication::postEvent(m_listener, new MythEvent(mesg));

    LOG(VB_GUI, LOG_DEBUG, mesg);
}

/** \brief Adds a ProgramInfo to the cache.
 *  \note This must only be called from the UI thread.
 */
void ProgramInfoCache::Add(const ProgramInfo &pginfo)
{
    if (!pginfo.GetRecordingID() || (Update(pginfo) != PIC_NO_ACTION))
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

void ProgramInfoCache::GetOrdered(std::vector<ProgramInfo*> &list, bool newest_first)
{
    std::copy(m_cache.cbegin(), m_cache.cend(), std::back_inserter(list));

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
    for (const auto & pi : std::as_const(m_cache))
        delete pi;
    m_cache.clear();
}
