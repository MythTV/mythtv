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
#include "mythdb.h"
#include "mconcurrent.h"

#include <QCoreApplication>
#include <QRunnable>

#include <algorithm>

typedef vector<ProgramInfo*> *VPI_ptr;
static void free_vec(VPI_ptr &v)
{
    if (v)
    {
        vector<ProgramInfo*>::iterator it = v->begin();
        for (; it != v->end(); ++it)
            delete *it;
        delete v;
        v = NULL;
    }
}

class ProgramInfoLoader : public QRunnable
{
  public:
    ProgramInfoLoader(ProgramInfoCache &c, const bool updateUI)
      : m_cache(c), m_updateUI(updateUI) {}

    void run(void)
    {
        m_cache.Load(m_updateUI);
    }

    ProgramInfoCache &m_cache;
    bool              m_updateUI;
};

ProgramInfoCache::ProgramInfoCache(QObject *o) :
    m_next_cache(NULL), m_listener(o),
    m_load_is_queued(false), m_loads_in_progress(0)
{
}

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

void ProgramInfoCache::CalculateProgress(ProgramInfo &pg, int pos)
{
    uint lastPlayPercent = 0;
    if (pos > 0)
    {
        int total = 0;

        switch (pg.GetRecordingStatus())
        {
        case RecStatus::Recorded:
            total = pg.QueryTotalFrames();
            break;
        case RecStatus::Recording:
            // Active recordings won't have total frames set yet.
            total = pg.QueryLastFrameInPosMap();
            break;
        default:
            break;
        }

        lastPlayPercent = (total > pos) ? (100 * pos) / total : 0;

        LOG(VB_GUI, LOG_DEBUG, QString("%1 %2  %3/%4 = %5%")
            .arg(pg.GetRecordingID()).arg(pg.GetTitle())
            .arg(pos).arg(total).arg(lastPlayPercent));
    }
    pg.SetProgressPercent(lastPlayPercent);
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

    // Calculate play positions for UI
    if (tmp)
    {
        // Played progress
        typedef QPair<uint, QDateTime> ProgId;
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
            foreach (ProgramInfo* pg, *tmp)
            {
                // Enable last play pos for all recordings
                pg->SetAllowLastPlayPos(true);

                ProgId id = qMakePair(pg->GetChanID(),
                                      pg->GetRecordingStartTime());
                CalculateProgress(*pg, lastPlayFrames.value(id));
            }
        }
        else
            MythDB::DBError("Watched progress", query);
    }

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
    return m_loads_in_progress;
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
        vector<ProgramInfo*>::iterator it = m_next_cache->begin();
        for (; it != m_next_cache->end(); ++it)
        {
            if (!(*it)->GetChanID())
                continue;

            m_cache[(*it)->GetRecordingID()] = *it;
        }
        delete m_next_cache;
        m_next_cache = NULL;
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
 *  \return Flags indicating the result of the update
 */
uint32_t ProgramInfoCache::Update(const ProgramInfo& pginfo)
{
    QMutexLocker locker(&m_lock);

    uint recordingId = pginfo.GetRecordingID();
    Cache::iterator it = m_cache.find(recordingId);

    if (it == m_cache.end())
        return PIC_NO_ACTION;

    ProgramInfo& pg = **it;
    uint32_t flags = PIC_NONE;

    if (pginfo.GetBookmarkUpdate() != pg.GetBookmarkUpdate())
        flags |= PIC_MARK_CHANGED;

    if (pginfo.GetRecordingGroup() != pg.GetRecordingGroup())
        flags |= PIC_RECGROUP_CHANGED;

    if (pginfo.GetSeason() != pg.GetSeason()
            || pginfo.GetEpisode() != pg.GetEpisode()
            || pginfo.GetTitle() != pg.GetTitle())
        flags |= PIC_WATCHLIST_CHANGED;

    if (pg.GetProgressPercent() > 0)
        flags |= PIC_PART_WATCHED;

    pg.clone(pginfo, true);
    pg.SetAllowLastPlayPos(true);

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

/** \brief Updates file size calculations of a ProgramInfo in the cache.
 *  \note This should only be run by a non-UI thread as it contains multiple
 *   Db queries
 *  \return True iff the ProgramInfo was in the cache and was updated.
 */
void ProgramInfoCache::UpdateFileSize(uint recordingId, uint64_t filesize,
                                      uint32_t flags)
{
    QMutexLocker locker(&m_lock);

    Cache::iterator it = m_cache.find(recordingId);
    if (it == m_cache.end())
        return;

    ProgramInfo& pg = **it;

    CalculateProgress(pg, pg.QueryLastPlayPos());

    if (filesize > 0)
    {
        // Filesize update
        pg.SetFilesize(filesize);
        pg.SetAvailableStatus(asAvailable, "PIC::UpdateFileSize");
    }
    else // Info update
    {
        // Don't keep regenerating previews of files being played
        QString byWhom;
        if (pg.QueryIsInUse(byWhom) && byWhom.contains(QObject::tr("Playing")))
            flags &= ~PIC_MARK_CHANGED;

        // Changing to or from part-watched may affect watchlist
        if ((pg.GetProgressPercent() == 0) != !(flags & PIC_PART_WATCHED))
            flags |= PIC_WATCHLIST_CHANGED;
    }

    QString mesg = QString("UPDATE_UI_ITEM %1 %2").arg(recordingId).arg(flags);
    QCoreApplication::postEvent(m_listener, new MythEvent(mesg));

    LOG(VB_GUI, LOG_DEBUG, mesg);
}

/** \brief Adds a ProgramInfo to the cache.
 *  \note This must only be called from the UI thread.
 */
void ProgramInfoCache::Add(const ProgramInfo &pginfo)
{
    if (!pginfo.GetRecordingID() || Update(pginfo) != PIC_NO_ACTION)
        return;

    QMutexLocker locker(&m_lock);

    ProgramInfo* pg = new ProgramInfo(pginfo);
    pg->SetAllowLastPlayPos(true);
    m_cache[pginfo.GetRecordingID()] = pg;
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

    return NULL;
}

/// Clears the cache, m_lock must be held when this is called.
void ProgramInfoCache::Clear(void)
{
    for (Cache::iterator it = m_cache.begin(); it != m_cache.end(); ++it)
        delete (*it);
    m_cache.clear();
}
