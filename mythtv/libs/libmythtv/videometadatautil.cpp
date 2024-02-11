#include <QReadWriteLock>
#include <QHash>
#include <QUrl>

#include "libmythbase/mythdb.h"
#include "libmythbase/mythlogging.h"

#include "videometadatautil.h"

#define LOC      QString("VideoMetaDataUtil: ")

static QReadWriteLock art_path_map_lock;
using ArtPair = QPair< QString, QString >;
static QMultiHash<QString, ArtPair> art_path_map;
using ArtList = QList<ArtPair>;

QString VideoMetaDataUtil::GetArtPath(const QString &pathname,
                                      const QString &type)
{
    QString basename = pathname.section('/', -1);

    if (basename == pathname)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
                "Programmer Error: Cannot determine art path\n\t\t\t"
                "until the ProgramInfo pathname has been fully resolved.");
        return {};
    }

    art_path_map_lock.lockForRead();
    ArtList ret(art_path_map.values(basename));
    art_path_map_lock.unlock();
    // cppcheck-suppress unassignedVariable
    for (const auto & [arttype, artpath] : std::as_const(ret))
    {
        if (arttype == type)
            return artpath;
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
        const QString& workURL = pathname;
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
