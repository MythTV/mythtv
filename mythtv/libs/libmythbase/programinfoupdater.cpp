// Posix headers
#include <unistd.h> // for usleep()

// MythTV headers
#include "libmythbase/mthreadpool.h"
#include "libmythbase/mythcorecontext.h"

#include "programinfoupdater.h"
#include "remoteutil.h"

void ProgramInfoUpdater::insert(
    uint     recordedid, PIAction action, uint64_t filesize)
{
    if (recordedid == 0)
        return;
    QMutexLocker locker(&m_lock);
    if (kPIUpdate == action || kPIUpdateFileSize == action)
    {
        QHash<uint,PIKeyData>::iterator it = m_needsUpdate.find(recordedid);
        // If there is no action in the set we can insert
        // If it is the same type of action we can overwrite,
        // If it the new action is a full update we can overwrite
        if (it == m_needsUpdate.end())
            m_needsUpdate.insert(recordedid, PIKeyData(action, filesize));
        else if (((*it).m_action == action) || (kPIUpdate == action))
            (*it) = PIKeyData(action, filesize);
    }
    else
    {
        m_needsAddDelete.emplace_back(recordedid, action);
    }

    // Start a new run() if one isn't already running..
    // The lock prevents anything from getting stuck on a queue.
    if (!m_isRunning)
    {
        m_isRunning = true;
        MThreadPool::globalInstance()->start(this, "ProgramInfoUpdater");
    }
    else
    {
        m_moreWork.wakeAll();
    }
}

void ProgramInfoUpdater::run(void)
{
    bool workDone = true;

    while( workDone )
    {
        workDone = false;

        // we don't need instant updates allow a few to queue up
        // if they come in quick succession, this allows multiple
        // updates to be consolidated into one update...
        usleep(200 * 1000); // 200ms

        m_lock.lock();

        // send adds and deletes in the order they were queued
        for (auto & pi : m_needsAddDelete)
        {
            if (kPIAdd != pi.m_action && kPIDelete != pi.m_action)
                continue;

            QString type = (kPIAdd == pi.m_action) ? "ADD" : "DELETE";
            QString msg = QString("RECORDING_LIST_CHANGE %1 %2")
                .arg(type).arg(pi.m_recordedid);

            workDone = true;
            gCoreContext->SendMessage(msg);
        }
        m_needsAddDelete.clear();

        // Send updates in any old order, we just need
        // one per updated ProgramInfo.
        // NOLINTNEXTLINE(modernize-loop-convert)
        for (auto itu = m_needsUpdate.begin(); itu != m_needsUpdate.end(); ++itu)
        {
            QString msg;

            if (kPIUpdateFileSize == (*itu).m_action)
            {
                msg = QString("UPDATE_FILE_SIZE %1 %2")
                    .arg(itu.key())
                    .arg((*itu).m_filesize);
            }
            else
            {
                msg = QString("MASTER_UPDATE_REC_INFO %1")
                    .arg(itu.key());
            }

            workDone = true;
            gCoreContext->SendMessage(msg);
        }
        m_needsUpdate.clear();

        if ( workDone )
            m_moreWork.wait(&m_lock, 1000);

        m_lock.unlock();
    }

    m_isRunning = false;
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
