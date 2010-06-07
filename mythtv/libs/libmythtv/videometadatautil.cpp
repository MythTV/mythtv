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
typedef QPair< QString, QString > ArtPair;
static QMultiHash<QString, ArtPair> art_path_map;
typedef QList< ArtPair > ArtList;

QString VideoMetaDataUtil::GetArtPath(const QString &pathname, const QString &type)
{
    QString basename = pathname.section('/', -1);

    if (basename == pathname)
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                "Programmer Error: Can not determine art path\n\t\t\t"
                "until the ProgramInfo pathname has been fully resolved.");
        return QString();
    }

    art_path_map_lock.lockForRead();
    ArtList ret(art_path_map.values(basename));
    art_path_map_lock.unlock();
    for (ArtList::const_iterator i = ret.begin();
            i != ret.end(); ++i)
    {
        if ((*i).first == type)
            return (*i).second;
    }

    QString fn = basename;
    fn.prepend("%");

    QString dbcolumn;
    if (type == "Coverart")
        dbcolumn = "coverfile";
    else if (type == "Fanart")
        dbcolumn = "fanart";
    else if (type == "Banners")
        dbcolumn = "banner";
    else if (type == "Screenshots")
        dbcolumn = "screenshot";

    QString querystr = QString("SELECT %1 "
        "FROM videometadata WHERE filename "
        "LIKE :FILENAME").arg(dbcolumn);

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(querystr);
    query.bindValue(":FILENAME", fn);

    QString artpath;
    if (query.exec() && query.next())
        artpath = query.value(0).toString();

    if (!artpath.startsWith('/') && pathname.startsWith("myth://"))
    {
        QString workURL = pathname;
        QUrl baseURL(workURL);
        baseURL.setUserName(type);
        QString finalURL =
            baseURL.toString(QUrl::RemovePath) + '/' + artpath;
        artpath = finalURL;
    }

    ArtPair ins(type, artpath);
    art_path_map_lock.lockForWrite();
    art_path_map.insert(basename, ins);
    art_path_map_lock.unlock();

    return artpath;
}
