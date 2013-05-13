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

#include "backendutil.h"
#include "programinfo.h"

QMutex recordingPathLock;
QMap <QString, QString> recordingPathCache;

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
