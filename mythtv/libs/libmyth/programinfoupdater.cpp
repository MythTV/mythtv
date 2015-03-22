// MythTV headers
#include "programinfoupdater.h"
#include "mthreadpool.h"
#include "mythlogging.h"
#include "mythcorecontext.h"
#include "remoteutil.h"

#include <unistd.h> // for usleep()

using std::vector;

void ProgramInfoUpdater::insert(
    uint     recordedid, PIAction action, uint64_t filesize)
{
    QMutexLocker locker(&lock);
    if (kPIUpdate == action || kPIUpdateFileSize == action)
    {
        QHash<uint,PIKeyData>::iterator it = needsUpdate.find(recordedid);
        // If there is no action in the set we can insert
        // If it is the same type of action we can overwrite,
        // If it the new action is a full update we can overwrite
        if (it == needsUpdate.end())
            needsUpdate.insert(recordedid, PIKeyData(action, filesize));
        else if (((*it).action == action) || (kPIUpdate == action))
            (*it) = PIKeyData(action, filesize);
    }
    else
    {
        needsAddDelete.push_back(PIKeyAction(recordedid, action));
    }

    // Start a new run() if one isn't already running..
    // The lock prevents anything from getting stuck on a queue.
    if (!isRunning)
    {
        isRunning = true;
        MThreadPool::globalInstance()->start(this, "ProgramInfoUpdater");
    }
    else
        moreWork.wakeAll();
}

void ProgramInfoUpdater::run(void)
{
    bool workDone;

    do {
        workDone = false;

        // we don't need instant updates allow a few to queue up
        // if they come in quick succession, this allows multiple
        // updates to be consolidated into one update...
        usleep(200 * 1000); // 200ms

        lock.lock();

        // send adds and deletes in the order they were queued
        vector<PIKeyAction>::iterator ita = needsAddDelete.begin();
        for (; ita != needsAddDelete.end(); ++ita)
        {
            if (kPIAdd != (*ita).action && kPIDelete != (*ita).action)
                continue;

            QString type = (kPIAdd == (*ita).action) ? "ADD" : "DELETE";
            QString msg = QString("RECORDING_LIST_CHANGE %1 %2")
                .arg(type).arg((*ita).recordedid);

            workDone = true;
            gCoreContext->SendMessage(msg);
        }
        needsAddDelete.clear();

        // Send updates in any old order, we just need
        // one per updated ProgramInfo.
        QHash<uint,PIKeyData>::iterator itu = needsUpdate.begin();
        for (; itu != needsUpdate.end(); ++itu)
        {
            QString msg;

            if (kPIUpdateFileSize == (*itu).action)
            {
                msg = QString("UPDATE_FILE_SIZE %1 %2")
                    .arg(itu.key())
                    .arg((*itu).filesize);
            }
            else
            {
                msg = QString("MASTER_UPDATE_REC_INFO %1")
                    .arg(itu.key());
            }

            workDone = true;
            gCoreContext->SendMessage(msg);
        }
        needsUpdate.clear();

        if ( workDone )
            moreWork.wait(&lock, 1000);

        lock.unlock();
    } while( workDone );

    isRunning = false;
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
