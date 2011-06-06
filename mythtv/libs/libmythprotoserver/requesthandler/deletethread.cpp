#include <cstdlib>
#include <iostream>
#include <fcntl.h>

using namespace std;

#include <QList>
#include <QTimer>
#include <QString>
#include <QFileInfo>
#include <QStringList>
#include <QMutexLocker>

#include "mythdb.h"
#include "mythverbose.h"
#include "requesthandler/deletethread.h"
#include "mythcorecontext.h"

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

    connect(&m_timer, SIGNAL(timeout()), this, SLOT(timeout()));
    m_timer.start(m_timeout);
}

void DeleteThread::run(void)
{
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
    while (true)
    {
        QString path;
        {
            QMutexLocker lock(&m_newlock);
            if (!m_newfiles.isEmpty())
                path = m_newfiles.takeFirst();
        }

        if (path.isEmpty())
            break;

        m_timer.start(m_timeout);

        const char *cpath = path.toLocal8Bit().constData();

        QFileInfo finfo(path);
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

        if (unlink(cpath))
        {
            VERBOSE(VB_IMPORTANT, QString("Error deleting '%1': "
                        "could not unlink ").arg(path) + ENO);
            emit unlinkFailed(path);
            close(fd);
            continue;
        }

        emit fileUnlinked(path);

        deletestruct *ds = new deletestruct;
        ds->path = path;
        ds->fd = fd;
        ds->size = finfo.size();

        m_files << ds;
    }
}

void DeleteThread::ProcessOld(void)
{
    // im the only thread accessing this, so no need for a lock
    if (m_files.empty())
        return;

    m_timer.start(m_timeout);

    while (true)
    {
        deletestruct *ds = m_files.first();
        
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
