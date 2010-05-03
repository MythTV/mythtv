// Myth headers
#include <mythdb.h>

// MythNews headers
#include "rssdbutil.h"

bool findInDB(const QString& url)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT url FROM netvisionsites WHERE url = :URL ;");
    query.bindValue(":URL", url);
    if (!query.exec() || !query.isActive()) {
        MythDB::DBError("RSS find in db", query);
        return false;
    }

    return query.size() > 0;
}

RSSSite* findByURL(const QString& url)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT name,thumbnail,description,url,"
                  "author,download,updated FROM netvisionsites "
                  "WHERE url = :URL ;");
    query.bindValue(":URL", url);
    if (!query.exec() || !query.isActive()) {
        MythDB::DBError("RSS find in db", query);
        RSSSite *tmp = new RSSSite(QString(), QString(), QString(),
                       QString(), QString(), false,
                       QDateTime());
        return tmp;
    }
    else
        query.next();

    QString title = query.value(0).toString();
    QString image  = query.value(1).toString();
    QString description = query.value(2).toString();
    QString outurl = query.value(3).toString();
    QString author = query.value(4).toString();
    bool download = query.value(5).toInt();
    QDateTime updated; query.value(6).toDate();

    RSSSite *tmp = new RSSSite(title, image, description, outurl,
                       author, download, updated);
    return tmp;
}

RSSSite::rssList findAllDBRSS()
{
    RSSSite::rssList tmp;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT name, thumbnail, description, url, author, download, updated "
        "FROM netvisionsites "
        "ORDER BY name");

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
        tmp.append(new RSSSite(title, image, description, url,
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
                      site->GetUpdated());
}

bool insertInDB(const QString &name, const QString &thumbnail,
                const QString &description, const QString &url,
                const QString &author, const bool &download,
                const QDateTime &updated)
{
    if (findInDB(name))
        return false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("INSERT INTO netvisionsites (name,thumbnail,description,"
                  "url,author,download,updated) "
            "VALUES( :NAME, :THUMBNAIL, :DESCRIPTION, :URL, :AUTHOR, :DOWNLOAD, "
            ":UPDATED);");
    query.bindValue(":NAME", name);
    query.bindValue(":THUMBNAIL", thumbnail);
    query.bindValue(":DESCRIPTION", description);
    query.bindValue(":URL", url);
    query.bindValue(":AUTHOR", author);
    query.bindValue(":DOWNLOAD", download);
    query.bindValue(":UPDATED", updated);
    if (!query.exec() || !query.isActive()) {
        MythDB::DBError("netvision: inserting in DB", query);
        return false;
    }

    return (query.numRowsAffected() > 0);
}

bool removeFromDB(RSSSite* site)
{
    if (!site) return false;

    return removeFromDB(site->GetURL());
}

bool removeFromDB(const QString &url)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM netvisionsites WHERE url = :URL ;");
    query.bindValue(":URL", url);
    if (!query.exec() || !query.isActive()) {
        MythDB::DBError("netvision: delete from db", query);
        return false;
    }

    return (query.numRowsAffected() > 0);
}

void markUpdated(RSSSite *site)
{
    QDateTime now = QDateTime::currentDateTime();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("UPDATE netvisionsites SET updated = :UPDATED "
                  "WHERE name = :NAME ;");
    query.bindValue(":UPDATED", now);
    query.bindValue(":NAME", site->GetTitle());
    if (!query.exec() || !query.isActive())
        MythDB::DBError("netvision update time", query);
}

bool clearRSSArticles(const QString &feedtitle)
{
    if (feedtitle.isEmpty())
        return false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM netvisionrssitems "
        "WHERE feedtitle = :FEEDTITLE;");
    query.bindValue(":FEEDTITLE", feedtitle);

    if (!query.exec() || !query.isActive()) {
        MythDB::DBError("netvision: clearing DB", query);
        return false;
    }

    return (query.numRowsAffected() > 0);
}

bool insertArticleInDB(const QString &feedtitle, ResultVideo *item)
{
    if (!item || feedtitle.isEmpty())
        return false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("INSERT INTO netvisionrssitems (feedtitle, title, "
                  "description, url, thumbnail, mediaURL, author, date, time, "
                  "rating, filesize, player, playerargs, download, "
                  "downloadargs, width, height, language, downloadable, countries) "
            "VALUES( :FEEDTITLE, :TITLE, :DESCRIPTION, :URL, :THUMBNAIL, "
            ":MEDIAURL, :AUTHOR, :DATE, :TIME, :RATING, :FILESIZE, :PLAYER, "
            ":PLAYERARGS, :DOWNLOAD, :DOWNLOADARGS, :WIDTH, :HEIGHT, "
            ":LANGUAGE, :DOWNLOADABLE, :COUNTRIES);");
    query.bindValue(":FEEDTITLE", feedtitle);
    query.bindValue(":TITLE", item->GetTitle());
    query.bindValue(":DESCRIPTION", item->GetDescription());
    query.bindValue(":URL", item->GetURL());
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
    query.bindValue(":PLAYER", item->GetPlayer());
    query.bindValue(":PLAYERARGS", item->GetPlayerArguments().join(" "));
    query.bindValue(":DOWNLOAD", item->GetDownloader());
    query.bindValue(":DOWNLOADARGS", item->GetDownloaderArguments().join(" "));
    query.bindValue(":WIDTH", item->GetWidth());
    query.bindValue(":HEIGHT", item->GetHeight());
    query.bindValue(":LANGUAGE", item->GetLanguage());
    query.bindValue(":DOWNLOADABLE", item->GetDownloadable());
    query.bindValue(":COUNTRIES", item->GetCountries());

    if (!query.exec() || !query.isActive()) {
        MythDB::DBError("netvision: inserting article in DB", query);
        return false;
    }

    return (query.numRowsAffected() > 0);
}

ResultVideo::resultList getRSSArticles(const QString &feedtitle)
{
    ResultVideo::resultList ret;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT title, description, url, "
                  "thumbnail, mediaURL, author, date, time, "
                  "rating, filesize, player, playerargs, download, "
                  "downloadargs, width, height, language, "
                  "downloadable, countries FROM netvisionrssitems "
                  "WHERE feedtitle = :FEEDTITLE ORDER BY "
                  "date DESC;");
    query.bindValue(":FEEDTITLE", feedtitle);
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
        QDateTime   date = query.value(6).toDateTime();
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

        ret.append(new ResultVideo(title, desc, URL, thumbnail,
                   mediaURL, author, date, time, rating, filesize,
                   player, playerargs, download, downloadargs,
                   width, height, language, downloadable, countries));
    }

    return ret;
}
