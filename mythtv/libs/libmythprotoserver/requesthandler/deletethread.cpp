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

#include "util.h"
#include "mythdb.h"
#include "mythverbose.h"
#include "requesthandler/deletethread.h"
#include "mythcorecontext.h"
#include "mythlogging.h"

/*
 Rather than attempt to calculate a delete speed from tuner card information
 that may be completely irrelevent to a machine that does not record, just 
 choose a reasonable value.

 38 Mbps (full QAM-256 multiplex) * 8 tuners = 9961472B/0.5s
*/

DeleteThread::DeleteThread(void) :
        m_increment(9961472), m_run(true), m_timeout(20000)
{
    m_slow = (bool) gCoreContext->GetNumSetting("TruncateDeletesSlowly", 0);
    m_link = (bool) gCoreContext->GetNumSetting("DeletesFollowLinks", 0);

    connect(&m_timer, SIGNAL(timeout()), this, SLOT(timeout()));
    m_timer.start(m_timeout);
}

void DeleteThread::run(void)
{
    threadRegister("Delete");

    VERBOSE(VB_FILE, "Spawning new delete thread.");

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
        QList<deletestruct *>::iterator i;
        for (i = m_files.begin(); i != m_files.end(); ++i)
        {
            close((*i)->fd);
            delete (*i);
        }
    }
    else
        VERBOSE(VB_FILE, "Delete thread self-terminating due to idle.");

    threadDeregister();
}

bool DeleteThread::AddFile(QString path)
{
    // check if a file exists, and add to the list of new files to be deleted
    QFileInfo finfo(path);
    if (!finfo.exists())
        return false;

    QMutexLocker lock(&m_newlock);
    m_newfiles << path;
    return true;
}

void DeleteThread::ProcessNew(void)
{
    // loop through new files, unlinking and adding for deletion
    // until none are left
    // TODO: add support for symlinks

    QDateTime ctime = QDateTime::currentDateTime();

    while (true)
    {
        // pull a new path from the stack
        QString path;
        {
            QMutexLocker lock(&m_newlock);
            if (!m_newfiles.isEmpty())
                path = m_newfiles.takeFirst();
        }

        // empty path given to delete thread, this should not happen
        if (path.isEmpty())
            continue;

        m_timer.start(m_timeout);

        const char *cpath = path.toLocal8Bit().constData();

        QFileInfo finfo(path);
        if (finfo.isSymLink())
        {
            if (m_link)
            {
                // if file is a symlink and symlinks are processed,
                // grab the target of the link, and attempt to unlink
                // the link itself
                QString tmppath = getSymlinkTarget(path);

                if (unlink(cpath))
                {
                    VERBOSE(VB_IMPORTANT, QString("Error deleting '%1' "
                            "-> '%2': ").arg(path).arg(tmppath) + ENO);
                    emit unlinkFailed(path);
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
                emit fileUnlinked(path);
                path = tmppath;
                cpath = path.toLocal8Bit().constData();
            }
            else
            {
                // symlinks are not followed, so unlink the link
                // itself and continue
                if (unlink(cpath))
                {
                    VERBOSE(VB_IMPORTANT, QString("Error deleting '%1': "
                                    "count not unlink ").arg(path) + ENO);
                    emit unlinkFailed(path);
                }
                else
                    emit fileUnlinked(path);

                continue;
            }
        }

        // open the file so it can be unlinked without immediate deletion
        VERBOSE(VB_FILE, QString("About to unlink/delete file: '%1'")
                                .arg(path));
        int fd = open(cpath, O_WRONLY);
        if (fd == -1)
        {
            VERBOSE(VB_IMPORTANT, QString("Error deleting '%1': "
                        "could not open ").arg(path) + ENO);
            emit unlinkFailed(path);
            continue;
        }

        // unlink the file so as soon as it is closed, the system will
        // delete it from the filesystem
        if (unlink(cpath))
        {
            VERBOSE(VB_IMPORTANT, QString("Error deleting '%1': "
                        "could not unlink ").arg(path) + ENO);
            emit unlinkFailed(path);
            close(fd);
            continue;
        }

        emit fileUnlinked(path);

        // insert the file into a queue of opened references to be deleted
        deletestruct *ds = new deletestruct;
        ds->path = path;
        ds->fd = fd;
        ds->size = finfo.size();
        ds->wait = ctime.addSecs(3); // delay deletion a bit to allow
                                     // UI to get any needed IO time

        m_files << ds;
    }
}

void DeleteThread::ProcessOld(void)
{
    // im the only thread accessing this, so no need for a lock
    if (m_files.empty())
        return;

    // files exist for deletion, reset the timer
    m_timer.start(m_timeout);

    QDateTime ctime = QDateTime::currentDateTime();

    // only operate on one file at a time
    // delete that file completely before moving onto the next
    while (true)
    {
        deletestruct *ds = m_files.first();

        // first file in the list has been delayed for deletion
        if (ds->wait > ctime)
            break;
        
        if (m_slow)
        {
            ds->size -= m_increment;
            int err = ftruncate(ds->fd, ds->size);

            if (err)
            {
                VERBOSE(VB_IMPORTANT, QString("Error truncating '%1'")
                            .arg(ds->path) + ENO);
                ds->size = 0;
            }
        }
        else
            ds->size = 0;

        if (ds->size == 0)
        {
            close(ds->fd);
            m_files.removeFirst();
            delete ds;
        }

        // fast delete can close out all, but slow delete needs
        // to return to sleep
        if (m_slow || m_files.empty())
            break;
    }
}
