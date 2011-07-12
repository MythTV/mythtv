#include <QUrl>

#include "mythdirs.h"
#include "mythdbcon.h"

#include "videoutils.h"
#include "metadataimagehelper.h"

ArtworkMap GetArtwork(QString inetref,
                             uint season)
{
    ArtworkMap map;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT host, coverart, fanart, banner "
        "FROM recordedartwork WHERE inetref = :INETREF "
        "ORDER BY season = :SEASON DESC, season DESC;");

    query.bindValue(":INETREF", inetref);
    query.bindValue(":SEASON", season);

    if (!query.exec())
    {
        MythDB::DBError("GetArtwork SELECT", query);
        return map;
    }

    if (query.next())
    {
        QString host = query.value(0).toString();
        QString coverart = query.value(1).toString();
        QString fanart = query.value(2).toString();
        QString banner = query.value(3).toString();

        if (!coverart.isEmpty())
        {
            ArtworkInfo coverartinfo;
            coverartinfo.url = generate_file_url("Coverart", host, coverart);
            map.insert(COVERART, coverartinfo);
        }

        if (!fanart.isEmpty())
        {
            ArtworkInfo fanartinfo;
            fanartinfo.url = generate_file_url("Fanart", host, fanart);
            map.insert(FANART, fanartinfo);
        }

        if (!banner.isEmpty())
        {
            ArtworkInfo bannerinfo;
            bannerinfo.url = generate_file_url("Banners", host, banner);
            map.insert(BANNER, bannerinfo);
        }
    }

    return map;
}

bool SetArtwork(const QString &inetref,
                       uint season,
                       const QString &host,
                       const QString &coverart,
                       const QString &fanart,
                       const QString &banner)
{
    bool ret = false;
    ArtworkMap map;

    if (!coverart.isEmpty())
    {
        ArtworkInfo coverartinfo;
        coverartinfo.url = generate_file_url("Coverart", host, coverart);
        map.insert(COVERART, coverartinfo);
    }

    if (!fanart.isEmpty())
    {
        ArtworkInfo fanartinfo;
        fanartinfo.url = generate_file_url("Fanart", host, fanart);
        map.insert(FANART, fanartinfo);
    }

    if (!banner.isEmpty())
    {
        ArtworkInfo bannerinfo;
        bannerinfo.url = generate_file_url("Banners", host, banner);
        map.insert(BANNER, bannerinfo);
    }

    ret = SetArtwork(inetref, season, host, map);

    return ret;
}

bool SetArtwork(const QString &inetref,
                       uint season,
                       const QString &host,
                       const ArtworkMap map)
{
    QString coverart, fanart, banner;

    QUrl coverurl(map.value(COVERART).url);
    if (!coverurl.path().isEmpty())
        coverart = coverurl.path();

    QUrl fanarturl(map.value(FANART).url);
    if (!fanarturl.path().isEmpty())
        fanart = fanarturl.path();

    QUrl bannerurl(map.value(BANNER).url);
    if (!bannerurl.path().isEmpty())
        banner = bannerurl.path();

    // Have to delete the old row for this item

    MSqlQuery prequery(MSqlQuery::InitCon());
    prequery.prepare("DELETE FROM recordedartwork WHERE "
                     "inetref = :INETREF AND season = :SEASON;");

    prequery.bindValue(":INETREF", inetref);
    prequery.bindValue(":SEASON", season);

    if (!prequery.exec())
    {
        MythDB::DBError("SetArtwork DELETE FROM", prequery);
        return false;
    }

    // Now we can insert the new
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("INSERT INTO recordedartwork(inetref,"
                  "season,host,coverart,fanart,banner) VALUES( "
                  ":INETREF, :SEASON, :HOST, :COVERART, "
                  ":FANART, :BANNER);");

    query.bindValue(":INETREF", inetref);
    query.bindValue(":SEASON", season);
    query.bindValue(":HOST", host);
    query.bindValue(":COVERART", coverart);
    query.bindValue(":FANART", fanart);
    query.bindValue(":BANNER", banner);

    if (!query.exec())
    {
        MythDB::DBError("SetArtwork INSERT INTO", query);
        return false;
    }

    return true;
}
