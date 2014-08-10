#include <QUrl>

#include "mythdirs.h"
#include "mythdbcon.h"

#include "metadataimagehelper.h"

ArtworkMap GetArtwork(QString inetref,
                      uint season,
                      bool strict)
{
    ArtworkMap map;

    if (inetref.isEmpty())
        return map;

    MSqlQuery query(MSqlQuery::InitCon());

    QString querystring = "SELECT host, coverart, fanart, banner "
        "FROM recordedartwork WHERE inetref = :INETREF ";

    if (strict)
        querystring += "AND season = :SEASON;";
    else
    {
        if (season > 0)
        {
            querystring += "ORDER BY season = :SEASON DESC, season DESC;";
        }
        else
            querystring += "ORDER BY season DESC;";
    }

    query.prepare(querystring);

    query.bindValue(":INETREF", inetref);
    if (strict || season > 0)
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
            coverartinfo.url = generate_myth_url("Coverart", host, coverart);
            map.insert(kArtworkCoverart, coverartinfo);
        }

        if (!fanart.isEmpty())
        {
            ArtworkInfo fanartinfo;
            fanartinfo.url = generate_myth_url("Fanart", host, fanart);
            map.insert(kArtworkFanart, fanartinfo);
        }

        if (!banner.isEmpty())
        {
            ArtworkInfo bannerinfo;
            bannerinfo.url = generate_myth_url("Banners", host, banner);
            map.insert(kArtworkBanner, bannerinfo);
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
        coverartinfo.url = generate_myth_url("Coverart", host, coverart);
        map.insert(kArtworkCoverart, coverartinfo);
    }

    if (!fanart.isEmpty())
    {
        ArtworkInfo fanartinfo;
        fanartinfo.url = generate_myth_url("Fanart", host, fanart);
        map.insert(kArtworkFanart, fanartinfo);
    }

    if (!banner.isEmpty())
    {
        ArtworkInfo bannerinfo;
        bannerinfo.url = generate_myth_url("Banners", host, banner);
        map.insert(kArtworkBanner, bannerinfo);
    }

    ret = SetArtwork(inetref, season, host, map);

    return ret;
}

bool SetArtwork(const QString &inetref,
                       uint season,
                       const QString &host,
                       const ArtworkMap map)
{
    if (inetref.isEmpty())
        return false;

    QString coverart, fanart, banner;

    QUrl coverurl(map.value(kArtworkCoverart).url);
    if (!coverurl.path().isEmpty())
    {
        coverart = coverurl.path();
        coverart = coverart.remove(0,1);
    }

    QUrl fanarturl(map.value(kArtworkFanart).url);
    if (!fanarturl.path().isEmpty())
    {
        fanart = fanarturl.path();
        fanart = fanart.remove(0,1);
    }

    QUrl bannerurl(map.value(kArtworkBanner).url);
    if (!bannerurl.path().isEmpty())
    {
        banner = bannerurl.path();
        banner = banner.remove(0,1);
    }

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
    query.bindValue(":COVERART", coverart.isNull() ? "" : coverart);
    query.bindValue(":FANART", fanart.isNull() ? "" : fanart);
    query.bindValue(":BANNER", banner.isNull() ? "" : banner);

    if (!query.exec())
    {
        MythDB::DBError("SetArtwork INSERT INTO", query);
        return false;
    }

    return true;
}
