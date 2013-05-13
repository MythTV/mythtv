#include <unistd.h>
#include <cstdlib> // for llabs

#include "mythconfig.h"
#if CONFIG_DARWIN || defined(__FreeBSD__)
#include <sys/param.h>
#include <sys/mount.h>
#elif __linux__
#include <sys/vfs.h>
#endif

#include <QMutex>
#include <QFile>
#include <QMap>

#include "requesthandler/fileserverutil.h"
#include "programinfo.h"

DeleteHandler::DeleteHandler(void) :
    ReferenceCounter("DeleteHandler"), m_fd(-1), m_size(0)
{
}

DeleteHandler::DeleteHandler(QString filename) :
    ReferenceCounter(QString("DeleteHandler:%1").arg(filename)),
    m_path(filename), m_fd(-1), m_size(0)
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

QMap <QString, QString> recordingPathCache;

QString GetPlaybackURL(ProgramInfo *pginfo, bool storePath)
{
    static QMutex recordingPathLock;
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
        result = pginfo->GetPlaybackURL(false, true);
        if (storePath && result.startsWith("/"))
            recordingPathCache[cacheKey] = result;
    }

    return result;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
