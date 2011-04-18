#include <QFile>
#include <QDataStream>
#include <QDomDocument>
#include <QProcess>
#include <QDomImplementation>
#include <QHash>
#include <QLocale>
#include <QUrl>
#include <QFileInfo>
#include <QRegExp>

#include "rssparse.h"
#include "netutils.h"
#include "mythcontext.h"
#include "mythdirs.h"

using namespace std;

ResultItem::ResultItem(const QString& title, const QString& subtitle,
              const QString& desc, const QString& URL,
              const QString& thumbnail, const QString& mediaURL,
              const QString& author, const QDateTime& date,
              const QString& time, const QString& rating,
              const off_t& filesize, const QString& player,
              const QStringList& playerargs, const QString& download,
              const QStringList& downloadargs, const uint& width,
              const uint& height, const QString& language,
              const bool& downloadable, const QStringList& countries,
              const uint& season, const uint& episode,
              const bool& customhtml)
{
    m_title = title;
    m_subtitle = subtitle;
    m_desc = desc;
    m_URL = URL;
    m_thumbnail = thumbnail;
    m_mediaURL = mediaURL;
    m_author = author;
    m_date = date;
    m_time = time;
    m_rating = rating;
    m_filesize = filesize;
    m_player = player;
    m_playerargs = playerargs;
    m_download = download;
    m_downloadargs = downloadargs;
    m_width = width;
    m_height = height;
    m_language = language;
    m_downloadable = downloadable;
    m_countries = countries;
    m_season = season;
    m_episode = episode;
    m_customhtml = customhtml;
}

ResultItem::ResultItem()
{
}

ResultItem::~ResultItem()
{
}

void ResultItem::toMap(MetadataMap &metadataMap)
{
    metadataMap["title"] = m_title;
    metadataMap["subtitle"] = m_subtitle;
    metadataMap["description"] = m_desc;
    metadataMap["url"] = m_URL;
    metadataMap["thumbnail"] = m_thumbnail;
    metadataMap["mediaurl"] = m_mediaURL;
    metadataMap["author"] = m_author;

    if (m_date.isNull())
        metadataMap["date"] = QString();
    else
        metadataMap["date"] = m_date.toString(gCoreContext->
           GetSetting("DateFormat", "yyyy-MM-dd hh:mm"));

    if (m_time.toInt() == 0)
        metadataMap["length"] = QString();
    else
    {
        QTime time(0,0,0,0);
        int secs = m_time.toInt();
        QTime fin = time.addSecs(secs);
        QString format;
        if (secs >= 3600)
            format = "H:mm:ss";
        else if (secs >= 600)
            format = "mm:ss";
        else if (secs >= 60)
            format = "m:ss";
        else
            format = ":ss";
        metadataMap["length"] = fin.toString(format);
    }

    if (m_rating == 0 || m_rating.isNull())
        metadataMap["rating"] = QString();
    else
        metadataMap["rating"] = m_rating;

    if (m_filesize == -1)
        metadataMap["filesize"] = QString();
    else if (m_filesize == 0 && !m_downloadable)
        metadataMap["filesize"] = QObject::tr("Web Only");
    else if (m_filesize == 0 && m_downloadable)
        metadataMap["filesize"] = QObject::tr("Downloadable");
    else
        metadataMap["filesize"] = QString::number(m_filesize);

    QString tmpSize;
    tmpSize.sprintf("%0.2f ", m_filesize / 1024.0 / 1024.0);
    tmpSize += QObject::tr("MB", "Megabytes");
    if (m_filesize == -1)
        metadataMap["filesize_str"] = QString();
    else if (m_filesize == 0 && !m_downloadable)
        metadataMap["filesize_str"] = QObject::tr("Web Only");
    else if (m_filesize == 0 && m_downloadable)
        metadataMap["filesize_str"] = QObject::tr("Downloadable");
    else
        metadataMap["filesize"] = tmpSize;

    metadataMap["player"] = m_player;
    metadataMap["playerargs"] = m_playerargs.join(", ");
    metadataMap["downloader"] = m_download;
    metadataMap["downloadargs"] = m_downloadargs.join(", ");
    if (m_width == 0)
        metadataMap["width"] = QString();
    else
        metadataMap["width"] = QString::number(m_width);
    if (m_height == 0)
        metadataMap["height"] = QString();
    else
        metadataMap["height"] = QString::number(m_height);
    if (m_width == 0 || m_height == 0)
        metadataMap["resolution"] = QString();
    else
        metadataMap["resolution"] = QString("%1x%2").arg(m_width).arg(m_height);
    metadataMap["language"] = m_language;
    metadataMap["countries"] = m_countries.join(", ");
    if (m_season > 0 || m_episode > 0)
    {
        metadataMap["season"] = QString::number(m_season);
        metadataMap["episode"] = QString::number(m_episode);
        metadataMap["s##e##"] = QString("s%1e%2").arg(GetDisplaySeasonEpisode
                                 (m_season, 2)).arg(
                                 GetDisplaySeasonEpisode(m_episode, 2));
        metadataMap["##x##"] = QString("%1x%2").arg(GetDisplaySeasonEpisode
                                 (m_season, 1)).arg(
                                 GetDisplaySeasonEpisode(m_episode, 2));
    }
    else
    {
        metadataMap["season"] = QString();
        metadataMap["episode"] = QString();
        metadataMap["s##e##"] = QString();
        metadataMap["##x##"] = QString();
    }
}

namespace
{
        QList<QDomNode> GetDirectChildrenNS(const QDomElement& elem,
                        const QString& ns, const QString& name)
        {
                QList<QDomNode> result;
                QDomNodeList unf = elem.elementsByTagNameNS(ns, name);
                for (int i = 0, size = unf.size(); i < size; ++i)
                        if (unf.at(i).parentNode() == elem)
                                result << unf.at(i);
                return result;
        }
}

class MRSSParser
{
    struct ArbitraryLocatedData
    {
        QString URL;
        QString Rating;
        QString RatingScheme;
        QString Title;
        QString Description;
        QString Keywords;
        QString CopyrightURL;
        QString CopyrightText;
        int RatingAverage;
        int RatingCount;
        int RatingMin;
        int RatingMax;
        int Views;
        int Favs;
        QString Tags;
        QList<MRSSThumbnail> Thumbnails;
        QList<MRSSCredit> Credits;
        QList<MRSSComment> Comments;
        QList<MRSSPeerLink> PeerLinks;
        QList<MRSSScene> Scenes;

        /**  Updates *this's fields according to the
         * child. Some kind of merge.
         */
        ArbitraryLocatedData& operator+= (const ArbitraryLocatedData& child)
        {
            if (!child.URL.isEmpty())
                URL = child.URL;
            if (!child.Rating.isEmpty())
                Rating = child.Rating;
            if (!child.RatingScheme.isEmpty())
                RatingScheme = child.RatingScheme;
            if (!child.Title.isEmpty())
                Title = child.Title;
            if (!child.Description.isEmpty())
                Description = child.Description;
            if (!child.Keywords.isEmpty())
                Keywords = child.Keywords;
            if (!child.CopyrightURL.isEmpty())
                CopyrightURL = child.CopyrightURL;
            if (!child.CopyrightText.isEmpty())
                CopyrightText = child.CopyrightText;
            if (child.RatingAverage != 0)
                RatingAverage = child.RatingAverage;
            if (child.RatingCount != 0)
                RatingCount = child.RatingCount;
            if (child.RatingMin != 0)
                RatingMin = child.RatingMin;
            if (child.RatingMax != 0)
                RatingMax = child.RatingMax;
            if (child.Views != 0)
                Views = child.Views;
            if (child.Favs != 0)
                Favs = child.Favs;
            if (!child.Tags.isEmpty())
                Tags = child.Tags;

            Thumbnails += child.Thumbnails;
            Credits += child.Credits;
            Comments += child.Comments;
            PeerLinks += child.PeerLinks;
            Scenes += child.Scenes;
            return *this;
        }
    };


public:
    MRSSParser() {}

    QList<MRSSEntry> operator() (const QDomElement& item)
    {
        QList<MRSSEntry> result;

        QDomNodeList groups = item.elementsByTagNameNS(Parse::MediaRSS,
            "group");

        for (int i = 0; i < groups.size(); ++i)
            result += CollectChildren(groups.at(i).toElement());

        result += CollectChildren(item);

        return result;
    }

private:

    QList<MRSSEntry> CollectChildren(const QDomElement& holder)
    {
         QList<MRSSEntry> result;
         QDomNodeList entries = holder.elementsByTagNameNS(Parse::MediaRSS,
             "content");

         for (int i = 0; i < entries.size(); ++i)
         {
             MRSSEntry entry;

             QDomElement en = entries.at(i).toElement();
             ArbitraryLocatedData d = GetArbitraryLocatedDataFor(en);

             if (en.hasAttribute("url"))
                 entry.URL = en.attribute("url");
             else
                 entry.URL = d.URL;

             entry.Size = en.attribute("fileSize").toInt();
             entry.Type = en.attribute("type");
             entry.Medium = en.attribute("medium");
             entry.IsDefault = (en.attribute("isDefault") == "true");
             entry.Expression = en.attribute("expression");
             if (entry.Expression.isEmpty())
                 entry.Expression = "full";
             entry.Bitrate = en.attribute("bitrate").toInt();
             entry.Framerate = en.attribute("framerate").toDouble();
             entry.SamplingRate = en.attribute("samplingrate").toDouble();
             entry.Channels = en.attribute("channels").toInt();
             if (!en.attribute("duration").isNull())
                 entry.Duration = en.attribute("duration").toInt();
             else
                 entry.Duration = 0;
             if (!en.attribute("width").isNull())
                 entry.Width = en.attribute("width").toInt();
             else
                 entry.Width = 0;
             if (!en.attribute("height").isNull())
                 entry.Height = en.attribute("height").toInt();
             else
                 entry.Height = 0;
             if (!en.attribute("lang").isNull())
                 entry.Lang = en.attribute("lang");
             else
                 entry.Lang = QString();

             if (!en.attribute("rating").isNull())
                 entry.Rating = d.Rating;
             else
                 entry.Rating = QString();
             entry.RatingScheme = d.RatingScheme;
             entry.Title = d.Title;
             entry.Description = d.Description;
             entry.Keywords = d.Keywords;
             entry.CopyrightURL = d.CopyrightURL;
             entry.CopyrightText = d.CopyrightText;
             if (d.RatingAverage != 0)
                 entry.RatingAverage = d.RatingAverage;
             else
                 entry.RatingAverage = 0;
             entry.RatingCount = d.RatingCount;
             entry.RatingMin = d.RatingMin;
             entry.RatingMax = d.RatingMax;
             entry.Views = d.Views;
             entry.Favs = d.Favs;
             entry.Tags = d.Tags;
             entry.Thumbnails = d.Thumbnails;
             entry.Credits = d.Credits;
             entry.Comments = d.Comments;
             entry.PeerLinks = d.PeerLinks;
             entry.Scenes = d.Scenes;

             result << entry;
        }
        return result;
    }

    ArbitraryLocatedData GetArbitraryLocatedDataFor(const QDomElement& holder)
    {
        ArbitraryLocatedData result;

        QList<QDomElement> parents;
        QDomElement parent = holder;
        while (!parent.isNull())
        {
            parents.prepend(parent);
            parent = parent.parentNode().toElement();
        }

        Q_FOREACH(QDomElement p, parents)
            result += CollectArbitraryLocatedData(p);

        return result;
    }

    QString GetURL(const QDomElement& element)
    {
        QList<QDomNode> elems = GetDirectChildrenNS(element, Parse::MediaRSS,
            "player");
        if (!elems.size())
            return QString();

        return QString(elems.at(0).toElement().attribute("url"));
    }

    QString GetTitle(const QDomElement& element)
    {
        QList<QDomNode> elems = GetDirectChildrenNS(element, Parse::MediaRSS,
            "title");

        if (!elems.size())
            return QString();

        QDomElement telem = elems.at(0).toElement();
        return QString(Parse::UnescapeHTML(telem.text()));
    }

    QString GetDescription(const QDomElement& element)
    {
        QList<QDomNode> elems = GetDirectChildrenNS(element, Parse::MediaRSS,
            "description");

        if (!elems.size())
            return QString();

        QDomElement telem = elems.at(0).toElement();
        return QString(Parse::UnescapeHTML(telem.text()));
    }

    QString GetKeywords(const QDomElement& element)
    {
        QList<QDomNode> elems = GetDirectChildrenNS(element, Parse::MediaRSS,
            "keywords");

        if (!elems.size())
            return QString();

        QDomElement telem = elems.at(0).toElement();
        return QString(telem.text());
    }

    int GetInt(const QDomElement& elem, const QString& attrname)
    {
        if (elem.hasAttribute(attrname))
        {
            bool ok = false;
            int result = elem.attribute(attrname).toInt(&ok);
            if (ok)
                return int(result);
        }
        return int();
    }

    QList<MRSSThumbnail> GetThumbnails(const QDomElement& element)
    {
        QList<MRSSThumbnail> result;
        QList<QDomNode> thumbs = GetDirectChildrenNS(element, Parse::MediaRSS,
            "thumbnail");
        for (int i = 0; i < thumbs.size(); ++i)
        {
            QDomElement thumbNode = thumbs.at(i).toElement();
            int widthOpt = GetInt(thumbNode, "width");
            int width = widthOpt ? widthOpt : 0;
            int heightOpt = GetInt(thumbNode, "height");
            int height = heightOpt ? heightOpt : 0;
            MRSSThumbnail thumb =
            {
                thumbNode.attribute("url"),
                width,
                height,
                thumbNode.attribute("time")
             };
             result << thumb;
        }
        return result;
    }

    QList<MRSSCredit> GetCredits(const QDomElement& element)
    {
        QList<MRSSCredit> result;
        QList<QDomNode> credits = GetDirectChildrenNS(element, Parse::MediaRSS,
           "credit");

        for (int i = 0; i < credits.size(); ++i)
        {
            QDomElement creditNode = credits.at(i).toElement();
            if (!creditNode.hasAttribute("role"))
                 continue;
            MRSSCredit credit =
            {
                creditNode.attribute("role"),
                creditNode.text()
            };
            result << credit;
        }
        return result;
    }

    QList<MRSSComment> GetComments(const QDomElement& element)
    {
        QList<MRSSComment> result;
        QList<QDomNode> commParents = GetDirectChildrenNS(element, Parse::MediaRSS,
            "comments");

        if (commParents.size())
        {
            QDomNodeList comments = commParents.at(0).toElement()
                .elementsByTagNameNS(Parse::MediaRSS,
                "comment");
            for (int i = 0; i < comments.size(); ++i)
            {
                MRSSComment comment =
                {
                    QObject::tr("Comments"),
                    comments.at(i).toElement().text()
                };
                result << comment;
            }
        }

        QList<QDomNode> respParents = GetDirectChildrenNS(element, Parse::MediaRSS,
            "responses");

        if (respParents.size())
        {
            QDomNodeList responses = respParents.at(0).toElement()
                .elementsByTagNameNS(Parse::MediaRSS,
                "response");
            for (int i = 0; i < responses.size(); ++i)
            {
                MRSSComment comment =
                {
                    QObject::tr("Responses"),
                    responses.at(i).toElement().text()
                };
                result << comment;
            }
        }

        QList<QDomNode> backParents = GetDirectChildrenNS(element, Parse::MediaRSS,
            "backLinks");

        if (backParents.size())
        {
            QDomNodeList backlinks = backParents.at(0).toElement()
                .elementsByTagNameNS(Parse::MediaRSS,
                "backLink");
            for (int i = 0; i < backlinks.size(); ++i)
            {
                MRSSComment comment =
                {
                    QObject::tr("Backlinks"),
                    backlinks.at(i).toElement().text()
                };
                result << comment;
            }
        }
        return result;
    }

    QList<MRSSPeerLink> GetPeerLinks(const QDomElement& element)
    {
        QList<MRSSPeerLink> result;
        QList<QDomNode> links = GetDirectChildrenNS(element, Parse::MediaRSS,
            "peerLink");

        for (int i = 0; i < links.size(); ++i)
        {
            QDomElement linkNode = links.at(i).toElement();
            MRSSPeerLink pl =
            {
                linkNode.attribute("type"),
                linkNode.attribute("href")
            };
            result << pl;
        }
        return result;
    }

    QList<MRSSScene> GetScenes(const QDomElement& element)
    {
        QList<MRSSScene> result;
        QList<QDomNode> scenesNode = GetDirectChildrenNS(element, Parse::MediaRSS,
            "scenes");

        if (scenesNode.size())
        {
            QDomNodeList scenesNodes = scenesNode.at(0).toElement()
                .elementsByTagNameNS(Parse::MediaRSS, "scene");

            for (int i = 0; i < scenesNodes.size(); ++i)
            {
                QDomElement sceneNode = scenesNodes.at(i).toElement();
                MRSSScene scene =
                {
                    sceneNode.firstChildElement("sceneTitle").text(),
                    sceneNode.firstChildElement("sceneDescription").text(),
                    sceneNode.firstChildElement("sceneStartTime").text(),
                    sceneNode.firstChildElement("sceneEndTime").text()
                };
                result << scene;
            }
        }
        return result;
    }

    ArbitraryLocatedData CollectArbitraryLocatedData(const QDomElement& element)
    {

        QString rating;
        QString rscheme;
        {
            QList<QDomNode> elems = GetDirectChildrenNS(element, Parse::MediaRSS,
                "rating");

            if (elems.size())
            {
                QDomElement relem = elems.at(0).toElement();
                rating = relem.text();
                if (relem.hasAttribute("scheme"))
                    rscheme = relem.attribute("scheme");
                else
                    rscheme = "urn:simple";
            }
        }

        QString curl;
        QString ctext;
        {
            QList<QDomNode> elems = GetDirectChildrenNS(element, Parse::MediaRSS,
                "copyright");

            if (elems.size())
            {
                QDomElement celem = elems.at(0).toElement();
                ctext = celem.text();
                if (celem.hasAttribute("url"))
                    curl = celem.attribute("url");
            }
        }

        int raverage = 0;
        int rcount = 0;
        int rmin = 0;
        int rmax = 0;
        int views = 0;
        int favs = 0;
        QString tags;
        {
            QList<QDomNode> comms = GetDirectChildrenNS(element, Parse::MediaRSS,
                "community");
            if (comms.size())
            {
                QDomElement comm = comms.at(0).toElement();
                QDomNodeList stars = comm.elementsByTagNameNS(Parse::MediaRSS,
                    "starRating");
                if (stars.size())
                {
                    QDomElement rating = stars.at(0).toElement();
                    raverage = GetInt(rating, "average");
                    rcount = GetInt(rating, "count");
                    rmin = GetInt(rating, "min");
                    rmax = GetInt(rating, "max");
                }

                QDomNodeList stats = comm.elementsByTagNameNS(Parse::MediaRSS,
                    "statistics");
                if (stats.size())
                {
                    QDomElement stat = stats.at(0).toElement();
                    views = GetInt(stat, "views");
                    favs = GetInt(stat, "favorites");
                 }

                QDomNodeList tagsNode = comm.elementsByTagNameNS(Parse::MediaRSS,
                    "tags");
                if (tagsNode.size())
                {
                    QDomElement tag = tagsNode.at(0).toElement();
                    tags = tag.text();
                }
            }
        }

        ArbitraryLocatedData result =
        {
            GetURL(element),
            rating,
            rscheme,
            GetTitle(element),
            GetDescription(element),
            GetKeywords(element),
            curl,
            ctext,
            raverage,
            rcount,
            rmin,
            rmax,
            views,
            favs,
            tags,
            GetThumbnails(element),
            GetCredits(element),
            GetComments(element),
            GetPeerLinks(element),
            GetScenes(element)
        };

        return result;
    }
};


//========================================================================================
//          Search Construction, Destruction
//========================================================================================

const QString Parse::DC = "http://purl.org/dc/elements/1.1/";
const QString Parse::WFW = "http://wellformedweb.org/CommentAPI/";
const QString Parse::Atom = "http://www.w3.org/2005/Atom";
const QString Parse::RDF = "http://www.w3.org/1999/02/22-rdf-syntax-ns#";
const QString Parse::Slash = "http://purl.org/rss/1.0/modules/slash/";
const QString Parse::Enc = "http://purl.oclc.org/net/rss_2.0/enc#";
const QString Parse::ITunes = "http://www.itunes.com/dtds/podcast-1.0.dtd";
const QString Parse::GeoRSSSimple = "http://www.georss.org/georss";
const QString Parse::GeoRSSW3 = "http://www.w3.org/2003/01/geo/wgs84_pos#";
const QString Parse::MediaRSS = "http://search.yahoo.com/mrss/";
const QString Parse::MythRSS = "http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format";

Parse::Parse()
{
}

Parse::~Parse()
{
}

ResultItem::resultList Parse::parseRSS(QDomDocument domDoc)
{
    ResultItem::resultList vList;

    QString document = domDoc.toString();
    VERBOSE(VB_GENERAL|VB_EXTRA, QString("Will Be Parsing: %1").arg(document));

    QDomElement root = domDoc.documentElement();
    QDomElement channel = root.firstChildElement("channel");
    while (!channel.isNull())
    {
        QDomElement item = channel.firstChildElement("item");
        while (!item.isNull())
        {
            vList.append(ParseItem(item));
            item = item.nextSiblingElement("item");
        }
        channel = channel.nextSiblingElement("channel");
    }

    return vList;
}

ResultItem* Parse::ParseItem(const QDomElement& item) const
{
    QString title, subtitle, description, url, author, duration, rating,
            thumbnail, mediaURL, player, language, download;
    off_t filesize = 0;
    uint width = 0, height = 0, season = 0, episode = 0;
    QDateTime date;
    QStringList playerargs, downloadargs, countries;
    bool downloadable = true;
    bool customhtml = false;

    // Get the title of the article/video
    title = item.firstChildElement("title").text();
    title = UnescapeHTML(title);
    if (title.isEmpty())
        title = "";

    // Get the subtitle of this item.
    QDomNodeList subt = item.elementsByTagNameNS(MythRSS, "subtitle");
    if (subt.size())
    {
        subtitle = subt.at(0).toElement().text();
    }

    // Parse the description of the article/video
    QDomElement descriptiontemp = item.firstChildElement("description");
    if (!descriptiontemp.isNull())
        description = descriptiontemp.text();
    if (description.isEmpty())
    {
        QDomNodeList nodes = item.elementsByTagNameNS(ITunes, "summary");
        if (nodes.size())
            description = nodes.at(0).toElement().text();
    }
    // Unescape and remove HTML tags from the description.
    if (description.isEmpty())
        description = "";
    else
        description = UnescapeHTML(description);

    // Get the link (web playable)
    url = item.firstChildElement("link").text();

    // Parse the item author
    QDomElement authortemp = item.firstChildElement("author");
    if (!authortemp.isNull())
        author = authortemp.text();
    if (author.isEmpty())
        author = GetAuthor(item);

    // Turn the RFC-822 pubdate into a QDateTime
    date = RFC822TimeToQDateTime(item.firstChildElement("pubDate").text());
    if (!date.isValid() || date.isNull())
        date = GetDCDateTime(item);
    if (!date.isValid() || date.isNull())
        date = QDateTime::currentDateTime();

    // Parse the insane iTunes duration (HH:MM:SS or H:MM:SS or MM:SS or M:SS or SS)
    QDomNodeList dur = item.elementsByTagNameNS(ITunes, "duration");
    if (dur.size())
    {
        QString itunestime = dur.at(0).toElement().text();
        QString dateformat;

        if (itunestime.count() == 8)
            dateformat = "hh:mm:ss";
        else if (itunestime.count() == 7)
            dateformat = "h:mm:ss";
        else if (itunestime.count() == 5)
            dateformat = "mm:ss";
        else if (itunestime.count() == 4)
            dateformat = "m:ss";
        else if (itunestime.count() == 2)
            dateformat = "ss";
        else
            duration = "0";

        if (!dateformat.isNull())
        {
            QTime itime = QTime::fromString(itunestime, dateformat);
            if (itime.isValid())
            {
                int seconds = itime.second() + (itime.minute() * 60) + (itime.hour() * 3600);
                duration = QString::number(seconds);
            }
        }
    }

    // Get the rating
    QDomElement ratingtemp = item.firstChildElement("rating");
    if (!ratingtemp.isNull())
        rating = ratingtemp.text();

    // Get the external player binary
    QDomElement playertemp = item.firstChildElement("player");
    if (!playertemp.isNull() && !playertemp.hasChildNodes())
        player = playertemp.text();

    // Get the arguments to pass to the external player
    QDomElement playerargstemp = item.firstChildElement("playerargs");
    if (!playerargstemp.isNull())
        playerargs = playerargstemp.text().split(" ");

    // Get the external downloader binary/script
    QDomElement downloadtemp = item.firstChildElement("download");
    if (!downloadtemp.isNull())
        download = downloadtemp.text();

    // Get the arguments to pass to the external downloader
    QDomElement downloadargstemp = item.firstChildElement("downloadargs");
    if (!downloadargstemp.isNull())
        downloadargs = downloadargstemp.text().split(" ");

    // Get the countries in which this item is playable
    QDomNodeList cties = item.elementsByTagNameNS(MythRSS, "country");
    if (cties.size())
    {
        int i = 0;
        while (i < cties.size())
        {
            countries.append(cties.at(i).toElement().text());
            i++;
        }
    }

    // Get the season number of this item.
    QDomNodeList seas = item.elementsByTagNameNS(MythRSS, "season");
    if (seas.size())
    {
        season = seas.at(0).toElement().text().toUInt();
    }

    // Get the Episode number of this item.
    QDomNodeList ep = item.elementsByTagNameNS(MythRSS, "episode");
    if (ep.size())
    {
        episode = ep.at(0).toElement().text().toUInt();
    }

    // Does this grabber return custom HTML?
    QDomNodeList html = item.elementsByTagNameNS(MythRSS, "customhtml");
    if (html.size())
    {
        QString htmlstring = html.at(0).toElement().text();
        if (htmlstring.toLower().contains("true") || htmlstring == "1" ||
            htmlstring.toLower().contains("yes"))
            customhtml = true;
    }

    QList<MRSSEntry> enclosures = GetMediaRSS(item);

    if (enclosures.size())
    {
        MRSSEntry media = enclosures.takeAt(0);

        QList<MRSSThumbnail> thumbs = media.Thumbnails;
        if (thumbs.size())
        {
            MRSSThumbnail thumb = thumbs.takeAt(0);
            thumbnail = thumb.URL;
        }

        mediaURL = media.URL;

        width = media.Width;
        height = media.Height;
        language = media.Lang;

        if (duration.isEmpty())
            duration = QString::number(media.Duration);

        if (filesize == 0)
            filesize = media.Size;

        if (rating.isEmpty())
            rating = QString::number(media.RatingAverage);
    }
    if (mediaURL.isEmpty())
    {
        QList<Enclosure> stdEnc = GetEnclosures(item);

        if (stdEnc.size())
        {
            Enclosure e = stdEnc.takeAt(0);

            mediaURL = e.URL;

            if (filesize == 0)
                filesize = e.Length;
        }
    }

    if (mediaURL.isNull() || mediaURL == url)
        downloadable = false;

    return(new ResultItem(title, subtitle, description,
              url, thumbnail, mediaURL, author, date, duration,
              rating, filesize, player, playerargs,
              download, downloadargs, width, height,
              language, downloadable, countries, season,
              episode, customhtml));
}

QString Parse::GetLink(const QDomElement& parent) const
{
    QString result;
    QDomElement link = parent.firstChildElement("link");
    while(!link.isNull())
    {
        if (!link.hasAttribute("rel") || link.attribute("rel") == "alternate")
        {
            if (!link.hasAttribute("href"))
                result = link.text();
            else
                result = link.attribute("href");
            break;
        }
        link = link.nextSiblingElement("link");
    }
    return result;
}

QString Parse::GetAuthor(const QDomElement& parent) const
{
    QString result;
    QDomNodeList nodes = parent.elementsByTagNameNS(ITunes,
        "author");
    if (nodes.size())
    {
        result = nodes.at(0).toElement().text();
        return result;
    }

    nodes = parent.elementsByTagNameNS(DC,
       "creator");
    if (nodes.size())
    {
        result = nodes.at(0).toElement().text();
        return result;
    }

    return result;
}

QString Parse::GetCommentsRSS(const QDomElement& parent) const
{
    QString result;
    QDomNodeList nodes = parent.elementsByTagNameNS(WFW,
        "commentRss");
    if (nodes.size())
        result = nodes.at(0).toElement().text();
    return result;
}

QString Parse::GetCommentsLink(const QDomElement& parent) const
{
    QString result;
    QDomNodeList nodes = parent.elementsByTagNameNS("", "comments");
    if (nodes.size())
        result = nodes.at(0).toElement().text();
    return result;
}

QDateTime Parse::GetDCDateTime(const QDomElement& parent) const
{
    QDomNodeList dates = parent.elementsByTagNameNS(DC, "date");
    if (!dates.size())
        return QDateTime();
    return FromRFC3339(dates.at(0).toElement().text());
}

QDateTime Parse::RFC822TimeToQDateTime(const QString& t) const
{
    if (t.size() < 20)
        return QDateTime();

    QString time = t.simplified();
    short int hoursShift = 0, minutesShift = 0;

    QStringList tmp = time.split(' ');
    if (tmp.isEmpty())
        return QDateTime();
    if (tmp. at(0).contains(QRegExp("\\D")))
        tmp.removeFirst();
    if (tmp.size() != 5)
        return QDateTime();
    QString timezone = tmp.takeAt(tmp.size() -1);
    if (timezone.size() == 5)
    {
        bool ok;
        int tz = timezone.toInt(&ok);
        if(ok)
        {
            hoursShift = tz / 100;
            minutesShift = tz % 100;
        }
    }
    else
        hoursShift = TimezoneOffsets.value(timezone, 0);

    if (tmp.at(0).size() == 1)
        tmp[0].prepend("0");
    tmp [1].truncate(3);

    time = tmp.join(" ");

    QDateTime result;
    if (tmp.at(2).size() == 4)
        result = QLocale::c().toDateTime(time, "dd MMM yyyy hh:mm:ss");
    else
        result = QLocale::c().toDateTime(time, "dd MMM yy hh:mm:ss");
    if (result.isNull() || !result.isValid())
        return QDateTime();
    result = result.addSecs(hoursShift * 3600 * (-1) + minutesShift *60 * (-1));
    result.setTimeSpec(Qt::UTC);
    return result.toLocalTime();
}

QDateTime Parse::FromRFC3339(const QString& t) const
{
    int hoursShift = 0, minutesShift = 0;
    if (t.size() < 19)
        return QDateTime();
    QDateTime result = QDateTime::fromString(t.left(19).toUpper(), "yyyy-MM-ddTHH:mm:ss");
    QRegExp fractionalSeconds("(\\.)(\\d+)");
    if (fractionalSeconds.indexIn(t) > -1)
    {
        bool ok;
        int fractional = fractionalSeconds.cap(2).toInt(&ok);
        if (ok)
        {
            if (fractional < 100)
                fractional *= 10;
            if (fractional <10)
                fractional *= 100;
            result.addMSecs(fractional);
        }
    }
    QRegExp timeZone("(\\+|\\-)(\\d\\d)(:)(\\d\\d)$");
    if (timeZone.indexIn(t) > -1)
    {
        short int multiplier = -1;
        if (timeZone.cap(1) == "-")
            multiplier = 1;
        hoursShift = timeZone.cap(2).toInt();
        minutesShift = timeZone.cap(4).toInt();
        result = result.addSecs(hoursShift * 3600 * multiplier + minutesShift * 60 * multiplier);
    }
    result.setTimeSpec(Qt::UTC);
    return result.toLocalTime();
}

QList<Enclosure> Parse::GetEnclosures(const QDomElement& entry) const
{
    QList<Enclosure> result;
    QDomNodeList links = entry.elementsByTagName("enclosure");
    for (int i = 0; i < links.size(); ++i)
    {
        QDomElement link = links.at(i).toElement();

        Enclosure e =
        {
            link.attribute("url"),
            link.attribute("type"),
            link.attribute("length", "-1").toLongLong(),
            link.attribute("hreflang")
        };

        result << e;
    }
    return result;
}

QList<MRSSEntry> Parse::GetMediaRSS(const QDomElement& item) const
{
    return MRSSParser() (item);
}

QString Parse::UnescapeHTML(const QString& escaped)
{
    QString result = escaped;
    result.replace("&amp;", "&");
    result.replace("&lt;", "<");
    result.replace("&gt;", ">");
    result.replace("&apos;", "\'");
    result.replace("&rsquo;", "\'");
    result.replace("&#x2019;", "\'");
    result.replace("&quot;", "\"");
    result.replace("&#8230;",QChar(8230));
    result.replace("&#233;",QChar(233));
    result.replace("&mdash;", QChar(8212));
    result.replace("&nbsp;", " ");
    result.replace("&#160;", QChar(160));
    result.replace("&#225;", QChar(225));
    result.replace("&#8216;", QChar(8216));
    result.replace("&#8217;", QChar(8217));
    result.replace("&#039;", "\'");
    result.replace("&ndash;", QChar(8211));
    result.replace("&auml;", QChar(0x00e4));
    result.replace("&ouml;", QChar(0x00f6));
    result.replace("&uuml;", QChar(0x00fc));
    result.replace("&Auml;", QChar(0x00c4));
    result.replace("&Ouml;", QChar(0x00d6));
    result.replace("&Uuml;", QChar(0x00dc));
    result.replace("&szlig;", QChar(0x00df));
    result.replace("&euro;", "€");
    result.replace("&#8230;", "...");
    result.replace("&#x00AE;", QChar(0x00ae));
    result.replace("&#x201C;", QChar(0x201c));
    result.replace("&#x201D;", QChar(0x201d));
    result.replace("<p>", "\n");

    QRegExp stripHTML(QLatin1String("<.*>"));
    stripHTML.setMinimal(true);
    result.remove(stripHTML);

    return result;
}
