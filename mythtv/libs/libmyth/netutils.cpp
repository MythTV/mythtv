#include <QDir>
#include <QFileInfo>

#include "mythdirs.h"
#include "mythdb.h"
#include "mythcontext.h"
#include "mythdate.h"

#include "netutils.h"

bool findTreeGrabberInDB(const QString& commandline,
                         ArticleType type)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT * FROM internetcontent WHERE "
                  "commandline = :COMMAND AND host = :HOST "
                  "AND type = :TYPE AND tree = 1;");
    QFileInfo fi(commandline);
    query.bindValue(":COMMAND", fi.fileName());
    query.bindValue(":HOST", gCoreContext->GetHostName());
    query.bindValue(":TYPE", type);
    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("Tree find in db", query);
        return false;
    }

    return query.size() > 0;
}

bool findSearchGrabberInDB(const QString& commandline,
                           ArticleType type)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT * FROM internetcontent WHERE "
                  "commandline = :COMMAND AND host = :HOST "
                  "AND type = :TYPE AND search = 1;");
    QFileInfo fi(commandline);
    query.bindValue(":COMMAND", fi.fileName());
    query.bindValue(":HOST", gCoreContext->GetHostName());
    query.bindValue(":TYPE", type);
    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("Search find in db", query);
        return false;
    }

    return query.size() > 0;
}

GrabberScript* findTreeGrabberByCommand(const QString& commandline,
                                        ArticleType type)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT name,thumbnail,author,description,commandline,"
                  "version,search,tree FROM internetcontent "
                  "WHERE commandline = :COMMAND AND "
                  "host = :HOST AND type = :TYPE "
                  "AND tree = 1;");
    QFileInfo fi(commandline);
    query.bindValue(":COMMAND", fi.fileName());
    query.bindValue(":HOST", gCoreContext->GetHostName());
    query.bindValue(":TYPE", type);
    if (!query.exec() || !query.isActive()) {
        MythDB::DBError("Tree find in db", query);
    }

    QString title = query.value(0).toString();
    QString image = query.value(1).toString();
    QString author = query.value(2).toString();
    QString desc = query.value(3).toString();
    QString command = QString("%1/internetcontent/%2").arg(GetShareDir())
                        .arg(query.value(4).toString());
    double ver = query.value(5).toDouble();
    bool search = query.value(6).toBool();
    bool tree = query.value(7).toBool();

    GrabberScript *tmp = new GrabberScript(title, image, type, author, search,
                                     tree, desc, command, ver);
    return tmp;
}

GrabberScript* findSearchGrabberByCommand(const QString& commandline,
                                          ArticleType type)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT name,thumbnail,author,description,commandline,"
                  "version,search,tree FROM internetcontent "
                  "WHERE commandline = :COMMAND AND "
                  "type = :TYPE AND host = :HOST AND "
                  "search = 1;");
    QFileInfo fi(commandline);
    query.bindValue(":COMMAND", fi.fileName());
    query.bindValue(":HOST", gCoreContext->GetHostName());
    query.bindValue(":TYPE", type);
    if (!query.exec() || !query.isActive()) {
        MythDB::DBError("Search find in db", query);
    }

    QString title = query.value(0).toString();
    QString image = query.value(1).toString();
    QString author = query.value(2).toString();
    QString desc = query.value(3).toString();
    QString command = QString("%1/internetcontent/%2").arg(GetShareDir())
                        .arg(query.value(4).toString());
    double ver = query.value(5).toDouble();
    bool search = query.value(6).toBool();
    bool tree = query.value(7).toBool();

    GrabberScript *tmp = new GrabberScript(title, image, type, author, search,
                                     tree, desc, command, ver);
    return tmp;
}

GrabberScript::scriptList findAllDBTreeGrabbers()
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT DISTINCT name,thumbnail,type,"
                  "author,description,commandline,"
                  "version,search,tree FROM internetcontent "
                  "where tree = 1 ORDER BY name;");
    if (!query.exec() || !query.isActive()) {
        MythDB::DBError("Tree find in db", query);
    }

    GrabberScript::scriptList tmp;

    while (query.next())
    {
        QString title = query.value(0).toString();
        QString image = query.value(1).toString();
        ArticleType type = (ArticleType)query.value(2).toInt();
        QString author = query.value(3).toString();
        QString desc = query.value(4).toString();
        QString commandline = QString("%1/internetcontent/%2").arg(GetShareDir())
                            .arg(query.value(5).toString());
        double ver = query.value(6).toDouble();
        bool search = query.value(7).toBool();
        bool tree = query.value(8).toBool();

        GrabberScript *script = new GrabberScript(title, image, type, author, search,
                                     tree, desc, commandline, ver);
        tmp.append(script);
    }

    return tmp;
}

GrabberScript::scriptList findAllDBTreeGrabbersByHost(ArticleType type)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT name,thumbnail,author,description,commandline,"
                  "version,search,tree FROM internetcontent "
                  "WHERE host = :HOST AND type = :TYPE "
                  "AND tree = 1 ORDER BY name;");
    query.bindValue(":HOST", gCoreContext->GetHostName());
    query.bindValue(":TYPE", type);
    if (!query.exec() || !query.isActive()) {
        MythDB::DBError("Tree find in db", query);
    }

    GrabberScript::scriptList tmp;

    while (query.next())
    {
        QString title = query.value(0).toString();
        QString image = query.value(1).toString();
        QString author = query.value(2).toString();
        QString desc = query.value(3).toString();
        QString commandline = QString("%1/internetcontent/%2").arg(GetShareDir())
                            .arg(query.value(4).toString());
        double ver = query.value(5).toDouble();
        bool search = query.value(6).toBool();
        bool tree = query.value(7).toBool();

        GrabberScript *script = new GrabberScript(title, image, type, author, search,
                                     tree, desc, commandline, ver);
        tmp.append(script);
    }

    return tmp;
}

GrabberScript::scriptList findAllDBSearchGrabbers(ArticleType type)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT name,thumbnail,author,description,commandline,"
                  "version,search,tree FROM internetcontent "
                  "WHERE host = :HOST AND type = :TYPE "
                  "AND search = 1 ORDER BY name;");
    query.bindValue(":HOST", gCoreContext->GetHostName());
    query.bindValue(":TYPE", type);
    if (!query.exec() || !query.isActive()) {
        MythDB::DBError("Search find in db", query);
    }

    GrabberScript::scriptList tmp;

    while (query.next())
    {
        QString title = query.value(0).toString();
        QString image = query.value(1).toString();
        QString author = query.value(2).toString();
        QString desc = query.value(3).toString();
        QString commandline = QString("%1/internetcontent/%2").arg(GetShareDir())
                            .arg(query.value(4).toString());
        double ver = query.value(5).toDouble();
        bool search = query.value(6).toBool();
        bool tree = query.value(7).toBool();

        GrabberScript *script = new GrabberScript(title, image, type, author, search,
                                     tree, desc, commandline, ver);
        tmp.append(script);
    }

    return tmp;
}

bool insertSearchInDB(GrabberScript* script, ArticleType type)
{
    if (!script)
        return false;

    return insertGrabberInDB(script->GetTitle(), script->GetImage(),
                      type, script->GetAuthor(), script->GetDescription(),
                      script->GetCommandline(), script->GetVersion(),
                      true, false, false);
}

bool insertTreeInDB(GrabberScript* script, ArticleType type)
{
    if (!script)
        return false;

    return insertGrabberInDB(script->GetTitle(), script->GetImage(),
                      type, script->GetAuthor(), script->GetDescription(),
                      script->GetCommandline(), script->GetVersion(),
                      false, true, false);
}

bool insertGrabberInDB(const QString &name, const QString &thumbnail,
                ArticleType type, const QString &author,
                const QString &description, const QString &commandline,
                const double &version, bool search, bool tree,
                bool podcast)
{
    QFileInfo fi(thumbnail);
    QString thumbbase = fi.fileName();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("INSERT INTO internetcontent (name,thumbnail,type,author,"
                  "description,commandline,version,search,tree,podcast,"
                  "host) "
            "VALUES( :NAME, :THUMBNAIL, :TYPE, :AUTHOR, :DESCRIPTION, :COMMAND, "
            ":VERSION, :SEARCH, :TREE, :PODCAST, :HOST);");
    query.bindValue(":NAME", name);
    query.bindValue(":THUMBNAIL", thumbbase);
    query.bindValue(":TYPE", type);
    query.bindValue(":AUTHOR", author);
    query.bindValue(":DESCRIPTION", description);
    QFileInfo cmd(commandline);
    query.bindValue(":COMMAND", cmd.fileName());
    query.bindValue(":VERSION", version);
    query.bindValue(":SEARCH", search);
    query.bindValue(":TREE", tree);
    query.bindValue(":PODCAST", podcast);
    query.bindValue(":HOST", gCoreContext->GetHostName());
    if (!query.exec() || !query.isActive()) {
        MythDB::DBError("netcontent: inserting in DB", query);
        return false;
    }

    return (query.numRowsAffected() > 0);
}

bool removeTreeFromDB(GrabberScript* script)
{
    if (!script) return false;

    return removeGrabberFromDB(script->GetCommandline(), false);
}

bool removeSearchFromDB(GrabberScript* script)
{
    if (!script) return false;

    return removeGrabberFromDB(script->GetCommandline(), true);
}

bool removeGrabberFromDB(const QString &commandline, const bool &search)
{
    MSqlQuery query(MSqlQuery::InitCon());
    if (search)
        query.prepare("DELETE FROM internetcontent WHERE commandline = :COMMAND "
                  "AND host = :HOST AND search = 1;");
    else
        query.prepare("DELETE FROM internetcontent WHERE commandline = :COMMAND "
                  "AND host = :HOST AND search = 0;");
    QFileInfo fi(commandline);
    query.bindValue(":COMMAND", fi.fileName());
    query.bindValue(":HOST", gCoreContext->GetHostName());
    if (!query.exec() || !query.isActive()) {
        MythDB::DBError("netcontent: delete from db", query);
        return false;
    }

    return (query.numRowsAffected() > 0);
}

bool markTreeUpdated(GrabberScript* script, QDateTime curTime)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("UPDATE internetcontent SET updated = :UPDATED "
                  "WHERE commandline = :COMMAND AND tree = 1;");
    query.bindValue(":UPDATED", curTime);
    QFileInfo fi(script->GetCommandline());
    query.bindValue(":COMMAND", fi.fileName());
    if (!query.exec() || !query.isActive()) {
        MythDB::DBError("netcontent: update db time", query);
        return false;
    }

    return (query.numRowsAffected() > 0);
}

bool needsUpdate(GrabberScript* script, uint updateFreq)
{
    QDateTime now = MythDate::current();
    QDateTime then = lastUpdate(script);

    return then.addSecs(updateFreq * 60 * 60) < now;
}

QDateTime lastUpdate(GrabberScript* script)
{
    QDateTime updated;
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT updated "
                  "FROM internetcontent "
                  "WHERE commandline = :COMMAND ORDER "
                  "BY updated DESC LIMIT 1;");
    QFileInfo fi(script->GetCommandline());
    query.bindValue(":COMMAND", fi.fileName());
    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("Tree last update in db", query);
    }
    else if (query.next())
    {
        updated = MythDate::as_utc(query.value(0).toDateTime());
    }

    return updated;
}

bool clearTreeItems(const QString &feedcommand)
{
    if (feedcommand.isEmpty())
        return false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM internetcontentarticles "
        "WHERE feedtitle = :FEEDTITLE AND podcast = 0;");
    query.bindValue(":FEEDTITLE", feedcommand);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("netcontent: clearing DB", query);
        return false;
    }

    return (query.numRowsAffected() > 0);
}

bool isTreeInUse(const QString &feedcommand)
{
    if (feedcommand.isEmpty())
        return false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT * FROM internetcontent "
        "WHERE commandline = :COMMAND;");
    QFileInfo fi(feedcommand);
    query.bindValue(":COMMAND", fi.fileName());

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("netcontent:  isTreeInUse", query);
        return false;
    }

    return query.next();
}

bool insertTreeArticleInDB(const QString &feedtitle, const QString &path,
                       const QString &paththumb, ResultItem *item,
                       ArticleType type)
{
    if (!item || feedtitle.isEmpty() || path.isEmpty())
        return false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("INSERT INTO internetcontentarticles (feedtitle, path, paththumb, "
                  " title, subtitle, description, url, type, thumbnail, mediaURL, author, "
                  "date, time, rating, filesize, player, playerargs, download, "
                  "downloadargs, width, height, language, podcast, downloadable, "
                  "customhtml, countries, season, episode) "
            "VALUES( :FEEDTITLE, :PATH, :PATHTHUMB, :TITLE, :SUBTITLE, :DESCRIPTION, "
            ":URL, :TYPE, :THUMBNAIL, :MEDIAURL, :AUTHOR, :DATE, :TIME, :RATING, "
            ":FILESIZE, :PLAYER, :PLAYERARGS, :DOWNLOAD, :DOWNLOADARGS, :WIDTH, :HEIGHT, "
            ":LANGUAGE, :PODCAST, :DOWNLOADABLE, :CUSTOMHTML, :COUNTRIES, :SEASON, "
            ":EPISODE);");
    query.bindValue(":FEEDTITLE", feedtitle);
    query.bindValue(":PATH", path);
    query.bindValue(":PATHTHUMB", paththumb);
    query.bindValue(":TITLE", item->GetTitle());
    query.bindValue(":SUBTITLE", item->GetSubtitle().isNull() ? "" : item->GetSubtitle());
    query.bindValue(":DESCRIPTION", item->GetDescription());
    query.bindValue(":URL", item->GetURL());
    query.bindValue(":TYPE", type);
    query.bindValue(":THUMBNAIL", item->GetThumbnail());
    query.bindValue(":MEDIAURL", item->GetMediaURL());
    query.bindValue(":AUTHOR", item->GetAuthor());
    query.bindValue(":DATE", item->GetDate());
    QString time;
    if (item->GetTime().isEmpty())
        time = QString::number(0);
    else
        time = item->GetTime();
    query.bindValue(":TIME", time);
    query.bindValue(":RATING", item->GetRating());
    query.bindValue(":FILESIZE", (qulonglong)item->GetFilesize());
    query.bindValue(":PLAYER", item->GetPlayer().isNull() ? "" : item->GetPlayer());
    query.bindValue(":PLAYERARGS", !item->GetPlayerArguments().count() ? "" :
                                   item->GetPlayerArguments().join(" "));
    query.bindValue(":DOWNLOAD", item->GetDownloader().isNull() ? "" :
                                 item->GetDownloader());
    query.bindValue(":DOWNLOADARGS", !item->GetDownloaderArguments().count() ? "" :
                                     item->GetDownloaderArguments().join(" "));
    query.bindValue(":WIDTH", item->GetWidth());
    query.bindValue(":HEIGHT", item->GetHeight());
    query.bindValue(":LANGUAGE", item->GetLanguage().isNull() ? "" : item->GetLanguage());
    query.bindValue(":PODCAST", false);
    query.bindValue(":DOWNLOADABLE", item->GetDownloadable());
    query.bindValue(":CUSTOMHTML", item->GetCustomHTML());
    query.bindValue(":COUNTRIES", !item->GetCountries().count() ? "" :
                                  item->GetCountries().join(" "));
    query.bindValue(":SEASON", item->GetSeason());
    query.bindValue(":EPISODE", item->GetEpisode());

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("netcontent: inserting article in DB", query);
        return false;
    }

    return (query.numRowsAffected() > 0);
}

QMultiMap<QPair<QString,QString>, ResultItem*> getTreeArticles(const QString &feedtitle,
                                                               ArticleType type)
{
    QMultiMap<QPair<QString,QString>, ResultItem*> ret;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT title, subtitle, description, url, "
                  "type, thumbnail, mediaURL, author, date, time, "
                  "rating, filesize, player, playerargs, download, "
                  "downloadargs, width, height, language, "
                  "downloadable, customhtml, countries, season, episode, "
                  "path, paththumb FROM internetcontentarticles "
                  "WHERE feedtitle = :FEEDTITLE AND podcast = 0 "
                  "AND type = :TYPE ORDER BY title DESC;");
    query.bindValue(":FEEDTITLE", feedtitle);
    query.bindValue(":TYPE", type);
    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("Tree find in db", query);
        return ret;
    }

    while (query.next())
    {
        QString     title = query.value(0).toString();
        QString     subtitle = query.value(1).toString();
        QString     desc = query.value(2).toString();
        QString     URL = query.value(3).toString();
        QString     type = query.value(4).toString();
        QString     thumbnail = query.value(5).toString();
        QString     mediaURL = query.value(6).toString();
        QString     author = query.value(7).toString();
        QDateTime   date = MythDate::as_utc(query.value(8).toDateTime());
        QString     time = query.value(9).toString();
        QString     rating = query.value(10).toString();
        off_t       filesize = query.value(11).toULongLong();
        QString     player = query.value(12).toString();
        QStringList playerargs = query.value(13).toString().split(" ");
        QString     download = query.value(14).toString();
        QStringList downloadargs = query.value(15).toString().split(" ");
        uint        width = query.value(16).toUInt();
        uint        height = query.value(17).toUInt();
        QString     language = query.value(18).toString();
        bool        downloadable = query.value(19).toBool();
        bool        customhtml = query.value(20).toBool();
        QStringList countries = query.value(21).toString().split(" ");
        uint        season = query.value(22).toUInt();
        uint        episode = query.value(23).toUInt();

        QString     path = query.value(24).toString();
        QString     paththumb = query.value(25).toString();

        QPair<QString,QString> pair(path,paththumb);
        ret.insert(pair, new ResultItem(title, subtitle, desc, URL,
                   thumbnail, mediaURL, author, date, time, rating, filesize,
                   player, playerargs, download, downloadargs,
                   width, height, language, downloadable, countries,
                   season, episode, customhtml));
    }

    return ret;
}

bool findInDB(const QString& url, ArticleType type)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT commandline FROM internetcontent WHERE commandline = :URL AND "
                  "type = :TYPE AND podcast = 1;");
    query.bindValue(":URL", url);
    query.bindValue(":TYPE", type);
    if (!query.exec() || !query.isActive()) {
        MythDB::DBError("RSS find in db", query);
        return false;
    }

    return query.size() > 0;
}

RSSSite* findByURL(const QString& url, ArticleType type)
{
    RSSSite *tmp = NULL;
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT name,thumbnail,author,description,"
                  "commandline,download,updated FROM internetcontent "
                  "WHERE commandline = :URL AND type = :TYPE "
                  "AND podcast = 1;");
    query.bindValue(":URL", url);
    query.bindValue(":TYPE", type);
    if (!query.exec() || !query.next())
    {
        MythDB::DBError("RSS find in db", query);
        tmp = new RSSSite(QString(), QString(), (ArticleType)0, QString(),
                          QString(), QString(), false,
                          QDateTime());
    }
    else
    {
        QString title = query.value(0).toString();
        QString image  = query.value(1).toString();
        QString author = query.value(2).toString();
        QString description = query.value(3).toString();
        QString outurl = query.value(4).toString();
        bool download = query.value(5).toInt();
        QDateTime updated; query.value(6).toDate();

        tmp = new RSSSite(title, image, type, description, outurl,
                        author, download, updated);
    }

    return tmp;
}

RSSSite::rssList findAllDBRSSByType(ArticleType type)
{
    RSSSite::rssList tmp;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT name, thumbnail, description, commandline, author, "
        "download, updated FROM internetcontent WHERE podcast = 1 "
        "AND type = :TYPE ORDER BY name");

    query.bindValue(":TYPE", type);

    if (!query.exec())
    {
        return tmp;
    }

    while (query.next())
    {
        QString title = query.value(0).toString();
        QString image  = query.value(1).toString();
        QString description = query.value(2).toString();
        QString url = query.value(3).toString();
        QString author = query.value(4).toString();
        bool download = query.value(5).toInt();
        QDateTime updated; query.value(6).toDate();
        tmp.append(new RSSSite(title, image, type, description, url,
                       author, download, updated));
    }

    return tmp;
}

RSSSite::rssList findAllDBRSS()
{
    RSSSite::rssList tmp;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT name, thumbnail, type, description, commandline, author, "
        "download, updated FROM internetcontent WHERE podcast = 1 "
        "ORDER BY name");

    if (!query.exec())
    {
        return tmp;
    }

    while (query.next())
    {
        QString title = query.value(0).toString();
        QString image  = query.value(1).toString();
        ArticleType type  = (ArticleType)query.value(2).toInt();
        QString description = query.value(3).toString();
        QString url = query.value(4).toString();
        QString author = query.value(5).toString();
        bool download = query.value(6).toInt();
        QDateTime updated; query.value(7).toDate();
        tmp.append(new RSSSite(title, image, type, description, url,
                       author, download, updated));
    }

    return tmp;
}

bool insertInDB(RSSSite* site)
{
    if (!site) return false;

    return insertInDB(site->GetTitle(), site->GetImage(),
                      site->GetDescription(), site->GetURL(),
                      site->GetAuthor(), site->GetDownload(),
                      site->GetUpdated(), site->GetType());
}

bool insertInDB(const QString &name, const QString &thumbnail,
                const QString &description, const QString &url,
                const QString &author, const bool &download,
                const QDateTime &updated, ArticleType type)
{
    if (findInDB(name, type))
        return false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("INSERT INTO internetcontent (name,thumbnail,description,"
                  "commandline,author,download,updated,podcast, type) "
            "VALUES( :NAME, :THUMBNAIL, :DESCRIPTION, :URL, :AUTHOR, :DOWNLOAD, "
            ":UPDATED, :PODCAST, :TYPE);");
    query.bindValue(":NAME", name);
    query.bindValue(":THUMBNAIL", thumbnail);
    query.bindValue(":DESCRIPTION", description);
    query.bindValue(":URL", url);
    query.bindValue(":AUTHOR", author);
    query.bindValue(":DOWNLOAD", download);
    query.bindValue(":UPDATED", updated);
    query.bindValue(":PODCAST", true);
    query.bindValue(":TYPE", type);
    if (!query.exec() || !query.isActive()) {
        MythDB::DBError("netcontent: inserting in DB", query);
        return false;
    }

    return (query.numRowsAffected() > 0);
}

bool removeFromDB(RSSSite* site)
{
    if (!site) return false;

    return removeFromDB(site->GetURL(), site->GetType());
}

bool removeFromDB(const QString &url, ArticleType type)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM internetcontent WHERE commandline = :URL "
                  "AND type = :TYPE;");
    query.bindValue(":URL", url);
    query.bindValue(":TYPE", type);
    if (!query.exec() || !query.isActive()) {
        MythDB::DBError("netcontent: delete from db", query);
        return false;
    }

    return (query.numRowsAffected() > 0);
}

void markUpdated(RSSSite *site)
{
    QDateTime now = MythDate::current();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("UPDATE internetcontent SET updated = :UPDATED "
                  "WHERE commandline = :URL AND type = :TYPE;");
    query.bindValue(":UPDATED", now);
    query.bindValue(":URL", site->GetURL());
    query.bindValue(":TYPE", site->GetType());
    if (!query.exec() || !query.isActive())
        MythDB::DBError("netcontent update time", query);
}

bool clearRSSArticles(const QString &feedtitle, ArticleType type)
{
    if (feedtitle.isEmpty())
        return false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM internetcontentarticles "
        "WHERE feedtitle = :FEEDTITLE AND podcast = 1 "
        "AND type = :TYPE ;");
    query.bindValue(":FEEDTITLE", feedtitle);
    query.bindValue(":TYPE", type);

    if (!query.exec() || !query.isActive()) {
        MythDB::DBError("netcontent: clearing DB", query);
        return false;
    }

    return (query.numRowsAffected() > 0);
}

bool insertRSSArticleInDB(const QString &feedtitle, ResultItem *item,
                          ArticleType type)
{
    if (!item || feedtitle.isEmpty())
        return false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("INSERT INTO internetcontentarticles (feedtitle, title, "
                  "description, url, type, thumbnail, mediaURL, author, date, time, "
                  "rating, filesize, player, playerargs, download, "
                  "downloadargs, width, height, language, downloadable, countries, "
                  "podcast) "
            "VALUES( :FEEDTITLE, :TITLE, :DESCRIPTION, :URL, :TYPE, :THUMBNAIL, "
            ":MEDIAURL, :AUTHOR, :DATE, :TIME, :RATING, :FILESIZE, :PLAYER, "
            ":PLAYERARGS, :DOWNLOAD, :DOWNLOADARGS, :WIDTH, :HEIGHT, "
            ":LANGUAGE, :DOWNLOADABLE, :COUNTRIES, :PODCAST);");
    query.bindValue(":FEEDTITLE", feedtitle);
    query.bindValue(":TITLE", item->GetTitle());
    query.bindValue(":DESCRIPTION", item->GetDescription());
    query.bindValue(":URL", item->GetURL());
    query.bindValue(":TYPE", type);
    query.bindValue(":THUMBNAIL", item->GetThumbnail());
    query.bindValue(":MEDIAURL", item->GetMediaURL());
    query.bindValue(":AUTHOR", item->GetAuthor());
    query.bindValue(":DATE", item->GetDate());
    QString time;
    if (item->GetTime().isEmpty())
        time = QString::number(0);
    else
        time = item->GetTime();
    query.bindValue(":TIME", time);
    query.bindValue(":RATING", item->GetRating());
    query.bindValue(":FILESIZE", (qulonglong)item->GetFilesize());
    query.bindValue(":PLAYER", item->GetPlayer().isNull() ? "" : item->GetPlayer());
    query.bindValue(":PLAYERARGS", !item->GetPlayerArguments().count() ? "" :
                                   item->GetPlayerArguments().join(" "));
    query.bindValue(":DOWNLOAD", item->GetDownloader().isNull() ? "" :
                                 item->GetDownloader());
    query.bindValue(":DOWNLOADARGS", !item->GetDownloaderArguments().count() ? "" :
                                     item->GetDownloaderArguments().join(" "));
    query.bindValue(":WIDTH", item->GetWidth());
    query.bindValue(":HEIGHT", item->GetHeight());
    query.bindValue(":LANGUAGE", item->GetLanguage().isNull() ? "" : item->GetLanguage());
    query.bindValue(":DOWNLOADABLE", item->GetDownloadable());
    query.bindValue(":COUNTRIES", item->GetCountries());
    query.bindValue(":PODCAST", true);

    if (!query.exec() || !query.isActive()) {
        MythDB::DBError("netcontent: inserting article in DB", query);
        return false;
    }

    return (query.numRowsAffected() > 0);
}

ResultItem::resultList getRSSArticles(const QString &feedtitle,
                                      ArticleType type)
{
    ResultItem::resultList ret;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT title, description, url, "
                  "thumbnail, mediaURL, author, date, time, "
                  "rating, filesize, player, playerargs, download, "
                  "downloadargs, width, height, language, "
                  "downloadable, countries, season, episode "
                  "FROM internetcontentarticles "
                  "WHERE feedtitle = :FEEDTITLE AND podcast = 1 "
                  "AND type = :TYPE ORDER BY date DESC;");
    query.bindValue(":FEEDTITLE", feedtitle);
    query.bindValue(":TYPE", type);
    if (!query.exec() || !query.isActive()) {
        MythDB::DBError("RSS find in db", query);
        return ret;
    }

    while (query.next())
    {
        QString     title = query.value(0).toString();
        QString     desc = query.value(1).toString();
        QString     URL = query.value(2).toString();
        QString     thumbnail = query.value(3).toString();
        QString     mediaURL = query.value(4).toString();
        QString     author = query.value(5).toString();
        QDateTime   date = MythDate::as_utc(query.value(6).toDateTime());
        QString     time = query.value(7).toString();
        QString     rating = query.value(8).toString();
        off_t       filesize = query.value(9).toULongLong();
        QString     player = query.value(10).toString();
        QStringList playerargs = query.value(11).toString().split(" ");
        QString     download = query.value(12).toString();
        QStringList downloadargs = query.value(13).toString().split(" ");
        uint        width = query.value(14).toUInt();
        uint        height = query.value(15).toUInt();
        QString     language = query.value(16).toString();
        bool        downloadable = query.value(17).toBool();
        QStringList countries = query.value(18).toString().split(" ");
        uint        season = query.value(19).toUInt();
        uint        episode = query.value(20).toUInt();

        ret.append(new ResultItem(title, QString(), desc, URL, thumbnail,
                   mediaURL, author, date, time, rating, filesize,
                   player, playerargs, download, downloadargs,
                   width, height, language, downloadable, countries,
                   season, episode, false));
    }

    return ret;
}

QString GetDownloadFilename(QString title, QString url)
{
    QByteArray urlarr(url.toLatin1());
    quint16 urlChecksum = qChecksum(urlarr.data(), urlarr.length());
    QByteArray titlearr(title.toLatin1());
    quint16 titleChecksum = qChecksum(titlearr.data(), titlearr.length());
    QUrl qurl(url);
    QString ext = QFileInfo(qurl.path()).suffix();
    QString basefilename = QString("download_%1_%2.%3")
                           .arg(QString::number(urlChecksum))
                           .arg(QString::number(titleChecksum)).arg(ext);

    return basefilename;
}
