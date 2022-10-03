#include <unistd.h>
#include <cstdlib> // for llabs

#include <QtGlobal>
#if defined(Q_OS_DARWIN) || defined(__FreeBSD__)
#include <sys/param.h>
#include <sys/mount.h>
#elif __linux__
#include <sys/vfs.h>
#endif

#include <QMutex>
#include <QFile>
#include <QMap>

#include "libmythbase/programinfo.h"

#include "requesthandler/fileserverutil.h"

DeleteHandler::DeleteHandler(void) :
    ReferenceCounter("DeleteHandler")
{
}

DeleteHandler::DeleteHandler(const QString& filename) :
    ReferenceCounter(QString("DeleteHandler:%1").arg(filename)),
    m_path(filename)
{
}

DeleteHandler::~DeleteHandler()
{
    Close();
}

void DeleteHandler::Close(void)
{
    if (m_fd >= 0)
        close(m_fd);
    m_fd = -1;
}

static QMap <QString, QString> recordingPathCache;
static QMutex recordingPathLock;

QString GetPlaybackURL(ProgramInfo *pginfo, bool storePath)
{
    QString result = "";
    QMutexLocker locker(&recordingPathLock);
    QString cacheKey = QString("%1:%2").arg(pginfo->GetChanID())
        .arg(pginfo->GetRecordingStartTime(MythDate::ISODate));
    if ((recordingPathCache.contains(cacheKey)) &&
        (QFile::exists(recordingPathCache[cacheKey])))
    {
        result = recordingPathCache[cacheKey];
        if (!storePath)
            recordingPathCache.remove(cacheKey);
    }
    else
    {
        locker.unlock();
        result = pginfo->GetPlaybackURL(false, true);
        locker.relock();
        if (storePath && result.startsWith("/"))
            recordingPathCache[cacheKey] = result;
    }

    return result;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
