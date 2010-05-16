#include <QReadWriteLock>
#include <QHash>
#include <QUrl>

#include "videometadatautil.h"
#include "mythverbose.h"
#include "mythdb.h"

#define LOC      QString("VideoMetaDataUtil: ")
#define LOC_WARN QString("VideoMetaDataUtil, Warning: ")
#define LOC_ERR  QString("VideoMetaDataUtil, Error: ")

static QReadWriteLock art_path_map_lock;
static QHash<QString,QString> art_path_map;

QString VideoMetaDataUtil::GetCoverArtPath(const QString &pathname)
{
    QString basename = pathname.section('/', -1);

    if (basename == pathname)
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                "Programmer Error: Can not determine cover art path\n\t\t\t"
                "until the ProgramInfo pathname has been fully resolved.");
        return QString();
    }

    art_path_map_lock.lockForRead();
    QString ret(art_path_map.value(basename, "_cold_"));
    art_path_map_lock.unlock();
    if (ret != "_cold_")
        return ret;

    QString fn = basename;
    fn.prepend("%"); 

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT coverfile "
        "FROM videometadata "
        "WHERE filename LIKE :FILENAME");
    query.bindValue(":FILENAME", fn); 

    QString coverartpath;
    if (query.exec() && query.next()) 
        coverartpath = query.value(0).toString();

    if (!coverartpath.startsWith('/') && pathname.startsWith("myth://"))
    {
        QString workURL = pathname;
        QUrl baseURL(workURL);
        baseURL.setUserName("Coverart");
        QString finalURL =
            baseURL.toString(QUrl::RemovePath) + '/' + coverartpath;
        coverartpath = finalURL;
    }

    art_path_map_lock.lockForWrite();
    art_path_map[basename] = coverartpath;
    art_path_map_lock.unlock();

    return coverartpath;
}
