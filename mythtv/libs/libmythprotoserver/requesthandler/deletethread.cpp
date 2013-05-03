#include <cstdlib>
#include <iostream>
#include <fcntl.h>

using namespace std;

#include <QList>
#include <QTimer>
#include <QString>
#include <QFileInfo>
#include <QDateTime>
#include <QStringList>
#include <QMutexLocker>

#include "requesthandler/deletethread.h"
#include "mythmiscutil.h"
#include "mythdb.h"
#include "mythcorecontext.h"
#include "mythlogging.h"

/*
 Rather than attempt to calculate a delete speed from tuner card information
 that may be completely irrelevent to a machine that does not record, just 
 choose a reasonable value.

 38 Mbps (full QAM-256 multiplex) * 4 tuners = 9961472B/0.5s
*/

DeleteThread::DeleteThread(void) :
    MThread("Delete"), m_increment(9961472), m_run(true)
{
    m_slow = (bool) gCoreContext->GetNumSetting("TruncateDeletesSlowly", 0);
    m_link = (bool) gCoreContext->GetNumSetting("DeletesFollowLinks", 0);
}

void DeleteThread::run(void)
{
    RunProlog();

    LOG(VB_FILE, LOG_DEBUG, "Spawning new delete thread.");

    while (gCoreContext && m_run)
    {
        // loop through any stored files every half second 
        ProcessNew();
        ProcessOld();
        usleep(500000);
    }

    if (!m_files.empty())
    {
        // this will only happen if the program is closing, so fast
        // deletion is not a problem
        QList<DeleteHandler*>::iterator i;
        for (i = m_files.begin(); i != m_files.end(); ++i)
        {
            (*i)->Close();
            (*i)->DecrRef();
        }
        m_files.clear();
    }
    else
        LOG(VB_FILE, LOG_DEBUG, "Delete thread self-terminating due to idle.");

    RunEpilog();
}

bool DeleteThread::AddFile(QString path)
{
    // check if a file exists, and add to the list of new files to be deleted
    QFileInfo finfo(path);
    if (!finfo.exists())
        return false;

    QMutexLocker lock(&m_newlock);
    DeleteHandler *handler = new DeleteHandler(path);
    m_newfiles << handler;
    return true;
}

bool DeleteThread::AddFile(DeleteHandler *handler)
{
    handler->IncrRef();
    QMutexLocker lock(&m_newlock);
    m_newfiles << handler;
    return true;
}

void DeleteThread::ProcessNew(void)
{
    // loop through new files, unlinking and adding for deletion
    // until none are left

    QDateTime ctime = MythDate::current();

    while (true)
    {
        // pull a new path from the stack
        DeleteHandler *handler;
        {
            QMutexLocker lock(&m_newlock);
            if (m_newfiles.isEmpty())
                break;
            else
                handler = m_newfiles.takeFirst();
        }

        // empty path given to delete thread, this should not happen
        //if (path.isEmpty())
        //    continue;

        QString path = handler->m_path;
        QByteArray cpath_ba = handler->m_path.toLocal8Bit();
        const char *cpath = cpath_ba.constData();

        QFileInfo finfo(handler->m_path);
        if (finfo.isSymLink())
        {
            if (m_link)
            {
                // if file is a symlink and symlinks are processed,
                // grab the target of the link, and attempt to unlink
                // the link itself
                QString tmppath = getSymlinkTarget(handler->m_path);

                if (unlink(cpath))
                {
                    LOG(VB_GENERAL, LOG_ERR, 
                        QString("Error deleting '%1' -> '%2': ")
                            .arg(handler->m_path).arg(tmppath) + ENO);
                    handler->DeleteFailed();
                    handler->DecrRef();
                    continue;
                }

                // if successful, emit that the link has been removed,
                // and continue processing the target of the link as
                // normal
                //
                // this may cause problems in which the link is unlinked
                // signalling the matching metadata for removal, but the
                // target itself fails, resulting in a spurious file in
                // an external directory with no link into mythtv
                handler->DeleteSucceeded();
                handler->m_path = tmppath;
                cpath_ba = handler->m_path.toLocal8Bit();
                cpath = cpath_ba.constData();
            }
            else
            {
                // symlinks are not followed, so unlink the link
                // itself and continue
                if (unlink(cpath))
                {
                    LOG(VB_GENERAL, LOG_ERR,
                        QString("Error deleting '%1': count not unlink ")
                            .arg(path) + ENO);
                    handler->DeleteFailed();
                }
                else
                    handler->DeleteFailed();

                handler->DecrRef();
                continue;
            }
        }

        // open the file so it can be unlinked without immediate deletion
        LOG(VB_FILE, LOG_INFO, QString("About to unlink/delete file: '%1'")
                                .arg(handler->m_path));
        int fd = open(cpath, O_WRONLY);
        if (fd == -1)
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("Error deleting '%1': could not open ")
                    .arg(handler->m_path) + ENO);
            handler->DeleteFailed();
            handler->DecrRef();
            continue;
        }

        // unlink the file so as soon as it is closed, the system will
        // delete it from the filesystem
        if (unlink(cpath))
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("Error deleting '%1': could not unlink ")
                    .arg(path) + ENO);
            handler->DeleteFailed();
            close(fd);
            handler->DecrRef();
            continue;
        }

        handler->DeleteSucceeded();

        // insert the file into a queue of opened references to be deleted
        handler->m_fd = fd;
        handler->m_size = finfo.size();
        handler->m_wait = ctime.addSecs(3); // delay deletion a bit to allow
                                          // UI to get any needed IO time

        m_files << handler;
    }
}

void DeleteThread::ProcessOld(void)
{
    // im the only thread accessing this, so no need for a lock
    if (m_files.empty())
        return;

    QDateTime ctime = MythDate::current();

    // only operate on one file at a time
    // delete that file completely before moving onto the next
    while (true)
    {
        DeleteHandler *handler = m_files.first();

        // first file in the list has been delayed for deletion
        if (handler->m_wait > ctime)
            break;

        if (m_slow)
        {
            handler->m_size -= m_increment;
            int err = ftruncate(handler->m_fd, handler->m_size);

            if (err)
            {
                LOG(VB_GENERAL, LOG_ERR, QString("Error truncating '%1'")
                            .arg(handler->m_path) + ENO);
                handler->m_size = 0;
            }
        }
        else
            handler->m_size = 0;

        if (handler->m_size == 0)
        {
            handler->Close();
            m_files.removeFirst();
            handler->DecrRef();
        }

        // fast delete can close out all, but slow delete needs
        // to return to sleep
        if (m_slow || m_files.empty())
            break;
    }
}
