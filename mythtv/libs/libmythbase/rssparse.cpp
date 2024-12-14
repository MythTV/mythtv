#include "rssparse.h"

#include <QFile>
#include <QDataStream>
#include <QDomDocument>
#include <QDomImplementation>
#include <QHash>
#include <QLocale>
#include <QUrl>
#include <QFileInfo>

#include "libmythbase/mythdate.h"
#include "libmythbase/mythdirs.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythsorthelper.h"
#include "libmythbase/stringutil.h"

ResultItem::ResultItem(QString title, QString sortTitle,
              QString subtitle, QString sortSubtitle,
              QString desc, QString URL,
              QString thumbnail, QString mediaURL,
              QString author, const QDateTime& date,
              const QString& time, const QString& rating,
              const off_t filesize, const QString& player,
              const QStringList& playerargs, const QString& download,
              const QStringList& downloadargs, const uint width,
              const uint height, const QString& language,
              const bool downloadable, const QStringList& countries,
              const uint season, const uint episode,
              const bool customhtml)
    : m_title(std::move(title)), m_sorttitle(std::move(sortTitle)),
      m_subtitle(std::move(subtitle)), m_sortsubtitle(std::move(sortSubtitle)),
      m_desc(std::move(desc)), m_url(std::move(URL)),
      m_thumbnail(std::move(thumbnail)), m_mediaURL(std::move(mediaURL)),
      m_author(std::move(author))
{
    if (!date.isNull())
        m_date = date;
    else
        m_date = QDateTime();
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

    ensureSortFields();
}

void ResultItem::ensureSortFields(void)
{
    std::shared_ptr<MythSortHelper>sh = getMythSortHelper();

    if (m_sorttitle.isEmpty() and not m_title.isEmpty())
        m_sorttitle = sh->doTitle(m_title);
    if (m_sortsubtitle.isEmpty() and not m_subtitle.isEmpty())
        m_sortsubtitle = sh->doTitle(m_subtitle);
}

void ResultItem::toMap(InfoMap &metadataMap)
{
    metadataMap["title"] = m_title;
    metadataMap["sorttitle"] = m_sorttitle;
    metadataMap["subtitle"] = m_subtitle;
    metadataMap["sortsubtitle"] = m_sortsubtitle;
    metadataMap["description"] = m_desc;
    metadataMap["url"] = m_url;
    metadataMap["thumbnail"] = m_thumbnail;
    metadataMap["mediaurl"] = m_mediaURL;
    metadataMap["author"] = m_author;

    if (m_date.isNull())
        metadataMap["date"] = QString();
    else
        metadataMap["date"] = MythDate::toString(m_date, MythDate::kDateFull);

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

    if (m_rating == nullptr || m_rating.isNull())
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

    QString tmpSize = QString("%1 ")
        .arg(m_filesize / 1024.0 / 1024.0, 0,'f',2);
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
        metadataMap["season"] = StringUtil::intToPaddedString(m_season, 1);
        metadataMap["episode"] = StringUtil::intToPaddedString(m_episode, 1);
        metadataMap["s##e##"] = metadataMap["s00e00"] = QString("s%1e%2")
            .arg(StringUtil::intToPaddedString(m_season, 2),
                 StringUtil::intToPaddedString(m_episode, 2));
        metadataMap["##x##"] = metadataMap["00x00"] = QString("%1x%2")
            .arg(StringUtil::intToPaddedString(m_season, 1),
                 StringUtil::intToPaddedString(m_episode, 2));
    }
    else
    {
        metadataMap["season"] = QString();
        metadataMap["episode"] = QString();
        metadataMap["s##e##"] = metadataMap["s00e00"] = QString();
        metadataMap["##x##"] = metadataMap["00x00"] = QString();
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
        QString              m_url;
        QString              m_rating;
        QString              m_ratingScheme;
        QString              m_title;
        QString              m_description;
        QString              m_keywords;
        QString              m_copyrightUrl;
        QString              m_copyrightText;
        int                  m_ratingAverage {0};
        int                  m_ratingCount   {0};
        int                  m_ratingMin     {0};
        int                  m_ratingMax     {0};
        int                  m_views         {0};
        int                  m_favs          {0};
        QString              m_tags;
        QList<MRSSThumbnail> m_thumbnails;
        QList<MRSSCredit>    m_credits;
        QList<MRSSComment>   m_comments;
        QList<MRSSPeerLink>  m_peerLinks;
        QList<MRSSScene>     m_scenes;

        ArbitraryLocatedData() = default;

        /**  Updates *this's fields according to the
         * child. Some kind of merge.
         */
        ArbitraryLocatedData& operator+= (const ArbitraryLocatedData& child)
        {
            if (!child.m_url.isEmpty())
                m_url = child.m_url;
            if (!child.m_rating.isEmpty())
                m_rating = child.m_rating;
            if (!child.m_ratingScheme.isEmpty())
                m_ratingScheme = child.m_ratingScheme;
            if (!child.m_title.isEmpty())
                m_title = child.m_title;
            if (!child.m_description.isEmpty())
                m_description = child.m_description;
            if (!child.m_keywords.isEmpty())
                m_keywords = child.m_keywords;
            if (!child.m_copyrightUrl.isEmpty())
                m_copyrightUrl = child.m_copyrightUrl;
            if (!child.m_copyrightText.isEmpty())
                m_copyrightText = child.m_copyrightText;
            if (child.m_ratingAverage != 0)
                m_ratingAverage = child.m_ratingAverage;
            if (child.m_ratingCount != 0)
                m_ratingCount = child.m_ratingCount;
            if (child.m_ratingMin != 0)
                m_ratingMin = child.m_ratingMin;
            if (child.m_ratingMax != 0)
                m_ratingMax = child.m_ratingMax;
            if (child.m_views != 0)
                m_views = child.m_views;
            if (child.m_favs != 0)
                m_favs = child.m_favs;
            if (!child.m_tags.isEmpty())
                m_tags = child.m_tags;

            m_thumbnails += child.m_thumbnails;
            m_credits += child.m_credits;
            m_comments += child.m_comments;
            m_peerLinks += child.m_peerLinks;
            m_scenes += child.m_scenes;
            return *this;
        }
    };


public:
    MRSSParser() = default;

    QList<MRSSEntry> operator() (const QDomElement& item)
    {
        QList<MRSSEntry> result;

        QDomNodeList groups = item.elementsByTagNameNS(Parse::kMediaRSS,
            "group");

        for (int i = 0; i < groups.size(); ++i)
            result += CollectChildren(groups.at(i).toElement());

        result += CollectChildren(item);

        return result;
    }

private:

    static QList<MRSSEntry> CollectChildren(const QDomElement& holder)
    {
         QList<MRSSEntry> result;
         QDomNodeList entries = holder.elementsByTagNameNS(Parse::kMediaRSS,
             "content");

         for (int i = 0; i < entries.size(); ++i)
         {
             MRSSEntry entry;

             QDomElement en = entries.at(i).toElement();
             ArbitraryLocatedData d = GetArbitraryLocatedDataFor(en);

             if (en.hasAttribute("url"))
                 entry.URL = en.attribute("url");
             else
                 entry.URL = d.m_url;

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
                 entry.Rating = d.m_rating;
             else
                 entry.Rating = QString();
             entry.RatingScheme = d.m_ratingScheme;
             entry.Title = d.m_title;
             entry.Description = d.m_description;
             entry.Keywords = d.m_keywords;
             entry.CopyrightURL = d.m_copyrightUrl;
             entry.CopyrightText = d.m_copyrightText;
             if (d.m_ratingAverage != 0)
                 entry.RatingAverage = d.m_ratingAverage;
             else
                 entry.RatingAverage = 0;
             entry.RatingCount = d.m_ratingCount;
             entry.RatingMin = d.m_ratingMin;
             entry.RatingMax = d.m_ratingMax;
             entry.Views = d.m_views;
             entry.Favs = d.m_favs;
             entry.Tags = d.m_tags;
             entry.Thumbnails = d.m_thumbnails;
             entry.Credits = d.m_credits;
             entry.Comments = d.m_comments;
             entry.PeerLinks = d.m_peerLinks;
             entry.Scenes = d.m_scenes;

             result << entry;
        }
        return result;
    }

    static ArbitraryLocatedData GetArbitraryLocatedDataFor(const QDomElement& holder)
    {
        ArbitraryLocatedData result;

        QList<QDomElement> parents;
        QDomElement parent = holder;
        while (!parent.isNull())
        {
            parents.prepend(parent);
            parent = parent.parentNode().toElement();
        }

        for (const auto& p : std::as_const(parents))
            result += CollectArbitraryLocatedData(p);

        return result;
    }

    static QString GetURL(const QDomElement& element)
    {
        QList<QDomNode> elems = GetDirectChildrenNS(element, Parse::kMediaRSS,
            "player");
        if (elems.empty())
            return {};

        return elems.at(0).toElement().attribute("url");
    }

    static QString GetTitle(const QDomElement& element)
    {
        QList<QDomNode> elems = GetDirectChildrenNS(element, Parse::kMediaRSS,
            "title");

        if (elems.empty())
            return {};

        QDomElement telem = elems.at(0).toElement();
        return Parse::UnescapeHTML(telem.text());
    }

    static QString GetDescription(const QDomElement& element)
    {
        QList<QDomNode> elems = GetDirectChildrenNS(element, Parse::kMediaRSS,
            "description");

        if (elems.empty())
            return {};

        QDomElement telem = elems.at(0).toElement();
        return Parse::UnescapeHTML(telem.text());
    }

    static QString GetKeywords(const QDomElement& element)
    {
        QList<QDomNode> elems = GetDirectChildrenNS(element, Parse::kMediaRSS,
            "keywords");

        if (elems.empty())
            return {};

        QDomElement telem = elems.at(0).toElement();
        return telem.text();
    }

    static int GetInt(const QDomElement& elem, const QString& attrname)
    {
        if (elem.hasAttribute(attrname))
        {
            bool ok = false;
            int result = elem.attribute(attrname).toInt(&ok);
            if (ok)
                return result;
        }
        return int();
    }

    static QList<MRSSThumbnail> GetThumbnails(const QDomElement& element)
    {
        QList<MRSSThumbnail> result;
        QList<QDomNode> thumbs = GetDirectChildrenNS(element, Parse::kMediaRSS,
            "thumbnail");
        for (const auto& dom : std::as_const(thumbs))
        {
            QDomElement thumbNode = dom.toElement();
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

    static QList<MRSSCredit> GetCredits(const QDomElement& element)
    {
        QList<MRSSCredit> result;
        QList<QDomNode> credits = GetDirectChildrenNS(element, Parse::kMediaRSS,
           "credit");

        for (const auto& dom : std::as_const(credits))
        {
            QDomElement creditNode = dom.toElement();
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

    static QList<MRSSComment> GetComments(const QDomElement& element)
    {
        QList<MRSSComment> result;
        QList<QDomNode> commParents = GetDirectChildrenNS(element, Parse::kMediaRSS,
            "comments");

        if (!commParents.empty())
        {
            QDomNodeList comments = commParents.at(0).toElement()
                .elementsByTagNameNS(Parse::kMediaRSS,
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

        QList<QDomNode> respParents = GetDirectChildrenNS(element, Parse::kMediaRSS,
            "responses");

        if (!respParents.empty())
        {
            QDomNodeList responses = respParents.at(0).toElement()
                .elementsByTagNameNS(Parse::kMediaRSS,
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

        QList<QDomNode> backParents = GetDirectChildrenNS(element, Parse::kMediaRSS,
            "backLinks");

        if (!backParents.empty())
        {
            QDomNodeList backlinks = backParents.at(0).toElement()
                .elementsByTagNameNS(Parse::kMediaRSS,
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

    static QList<MRSSPeerLink> GetPeerLinks(const QDomElement& element)
    {
        QList<MRSSPeerLink> result;
        QList<QDomNode> links = GetDirectChildrenNS(element, Parse::kMediaRSS,
            "peerLink");

        for (const auto& dom : std::as_const(links))
        {
            QDomElement linkNode = dom.toElement();
            MRSSPeerLink pl =
            {
                linkNode.attribute("type"),
                linkNode.attribute("href")
            };
            result << pl;
        }
        return result;
    }

    static QList<MRSSScene> GetScenes(const QDomElement& element)
    {
        QList<MRSSScene> result;
        QList<QDomNode> scenesNode = GetDirectChildrenNS(element, Parse::kMediaRSS,
            "scenes");

        if (!scenesNode.empty())
        {
            QDomNodeList scenesNodes = scenesNode.at(0).toElement()
                .elementsByTagNameNS(Parse::kMediaRSS, "scene");

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

    static ArbitraryLocatedData CollectArbitraryLocatedData(const QDomElement& element)
    {

        QString rating;
        QString rscheme;
        {
            QList<QDomNode> elems = GetDirectChildrenNS(element, Parse::kMediaRSS,
                "rating");

            if (!elems.empty())
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
            QList<QDomNode> elems = GetDirectChildrenNS(element, Parse::kMediaRSS,
                "copyright");

            if (!elems.empty())
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
            QList<QDomNode> comms = GetDirectChildrenNS(element, Parse::kMediaRSS,
                "community");
            if (!comms.empty())
            {
                QDomElement comm = comms.at(0).toElement();
                QDomNodeList stars = comm.elementsByTagNameNS(Parse::kMediaRSS,
                    "starRating");
                if (stars.size())
                {
                    QDomElement ratingDom = stars.at(0).toElement();
                    raverage = GetInt(ratingDom, "average");
                    rcount = GetInt(ratingDom, "count");
                    rmin = GetInt(ratingDom, "min");
                    rmax = GetInt(ratingDom, "max");
                }

                QDomNodeList stats = comm.elementsByTagNameNS(Parse::kMediaRSS,
                    "statistics");
                if (stats.size())
                {
                    QDomElement stat = stats.at(0).toElement();
                    views = GetInt(stat, "views");
                    favs = GetInt(stat, "favorites");
                 }

                QDomNodeList tagsNode = comm.elementsByTagNameNS(Parse::kMediaRSS,
                    "tags");
                if (tagsNode.size())
                {
                    QDomElement tag = tagsNode.at(0).toElement();
                    tags = tag.text();
                }
            }
        }

        ArbitraryLocatedData result;
        result.m_url = GetURL(element);
        result.m_rating = rating;
        result.m_ratingScheme = rscheme;
        result.m_title = GetTitle(element);
        result.m_description = GetDescription(element);
        result.m_keywords = GetKeywords(element);
        result.m_copyrightUrl = curl;
        result.m_copyrightText = ctext;
        result.m_ratingAverage = raverage;
        result.m_ratingCount = rcount;
        result.m_ratingMin = rmin;
        result.m_ratingMax = rmax;
        result.m_views = views;
        result.m_favs = favs;
        result.m_tags = tags;
        result.m_thumbnails = GetThumbnails(element);
        result.m_credits = GetCredits(element);
        result.m_comments = GetComments(element);
        result.m_peerLinks = GetPeerLinks(element);
        result.m_scenes = GetScenes(element);

        return result;
    }
};


//========================================================================================
//          Search Construction, Destruction
//========================================================================================

const QString Parse::kDC = "http://purl.org/dc/elements/1.1/";
const QString Parse::kWFW = "http://wellformedweb.org/CommentAPI/";
const QString Parse::kAtom = "http://www.w3.org/2005/Atom";
const QString Parse::kRDF = "http://www.w3.org/1999/02/22-rdf-syntax-ns#";
const QString Parse::kSlash = "http://purl.org/rss/1.0/modules/slash/";
const QString Parse::kEnc = "http://purl.oclc.org/net/rss_2.0/enc#";
const QString Parse::kITunes = "http://www.itunes.com/dtds/podcast-1.0.dtd";
const QString Parse::kGeoRSSSimple = "http://www.georss.org/georss";
const QString Parse::kGeoRSSW3 = "http://www.w3.org/2003/01/geo/wgs84_pos#";
const QString Parse::kMediaRSS = "http://search.yahoo.com/mrss/";
const QString Parse::kMythRSS = "http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format";

QMap<QString, int> Parse::m_timezoneOffsets {
    { "EDT" , -4 },
    { "EST" , -5 }
};

ResultItem::resultList Parse::parseRSS(const QDomDocument& domDoc)
{
    ResultItem::resultList vList;

    QString document = domDoc.toString();
    LOG(VB_GENERAL, LOG_DEBUG, "Will Be Parsing: " + document);

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

ResultItem* Parse::ParseItem(const QDomElement& item) 
{
    QString title("");
    QString subtitle("");
    QString description("");
    QString url("");
    QString author("");
    QString duration("");
    QString rating("");
    QString thumbnail("");
    QString mediaURL("");
    QString player("");
    QString language("");
    QString download("");
    off_t filesize = 0;
    uint width = 0;
    uint height = 0;
    uint season = 0;
    uint episode = 0;
    QDateTime date;
    QStringList playerargs;
    QStringList downloadargs;
    QStringList countries;
    bool downloadable = true;
    bool customhtml = false;

    // Get the title of the article/video
    title = item.firstChildElement("title").text();
    title = UnescapeHTML(title);
    if (title.isEmpty())
        title = "";

    // Get the subtitle of this item.
    QDomNodeList subt = item.elementsByTagNameNS(kMythRSS, "subtitle");
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
        QDomNodeList nodes = item.elementsByTagNameNS(kITunes, "summary");
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
        date = MythDate::current();

    // Parse the insane iTunes duration (HH:MM:SS or H:MM:SS or MM:SS or M:SS or SS)
    QDomNodeList dur = item.elementsByTagNameNS(kITunes, "duration");
    if (dur.size())
    {
        QString itunestime = dur.at(0).toElement().text();
        QString dateformat;

        if (itunestime.size() == 8)
            dateformat = "hh:mm:ss";
        else if (itunestime.size() == 7)
            dateformat = "h:mm:ss";
        else if (itunestime.size() == 5)
            dateformat = "mm:ss";
        else if (itunestime.size() == 4)
            dateformat = "m:ss";
        else if (itunestime.size() == 2)
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
    if (!playertemp.isNull())
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
    QDomNodeList cties = item.elementsByTagNameNS(kMythRSS, "country");
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
    QDomNodeList seas = item.elementsByTagNameNS(kMythRSS, "season");
    if (seas.size())
    {
        season = seas.at(0).toElement().text().toUInt();
    }

    // Get the Episode number of this item.
    QDomNodeList ep = item.elementsByTagNameNS(kMythRSS, "episode");
    if (ep.size())
    {
        episode = ep.at(0).toElement().text().toUInt();
    }

    // Does this grabber return custom HTML?
    QDomNodeList html = item.elementsByTagNameNS(kMythRSS, "customhtml");
    if (html.size())
    {
        QString htmlstring = html.at(0).toElement().text();
        if (htmlstring.contains("true", Qt::CaseInsensitive) || htmlstring == "1" ||
            htmlstring.contains("yes", Qt::CaseInsensitive))
            customhtml = true;
    }

    QList<MRSSEntry> enclosures = GetMediaRSS(item);

    if (!enclosures.empty())
    {
        MRSSEntry media = enclosures.takeAt(0);

        QList<MRSSThumbnail> thumbs = media.Thumbnails;
        if (!thumbs.empty())
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

        if (!stdEnc.empty())
        {
            Enclosure e = stdEnc.takeAt(0);

            mediaURL = e.URL;

            if (filesize == 0)
                filesize = e.Length;
        }
    }

    if (mediaURL.isNull() || mediaURL == url)
        downloadable = false;

    std::shared_ptr<MythSortHelper>sh = getMythSortHelper();
    return(new ResultItem(title, sh->doTitle(title),
              subtitle, sh->doTitle(subtitle), description,
              url, thumbnail, mediaURL, author, date, duration,
              rating, filesize, player, playerargs,
              download, downloadargs, width, height,
              language, downloadable, countries, season,
              episode, customhtml));
}

QString Parse::GetLink(const QDomElement& parent)
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

QString Parse::GetAuthor(const QDomElement& parent)
{
    QString result("");
    QDomNodeList nodes = parent.elementsByTagNameNS(kITunes,
        "author");
    if (nodes.size())
    {
        result = nodes.at(0).toElement().text();
        return result;
    }

    nodes = parent.elementsByTagNameNS(kDC,
       "creator");
    if (nodes.size())
    {
        result = nodes.at(0).toElement().text();
        return result;
    }

    return result;
}

QString Parse::GetCommentsRSS(const QDomElement& parent)
{
    QString result;
    QDomNodeList nodes = parent.elementsByTagNameNS(kWFW,
        "commentRss");
    if (nodes.size())
        result = nodes.at(0).toElement().text();
    return result;
}

QString Parse::GetCommentsLink(const QDomElement& parent)
{
    QString result;
    QDomNodeList nodes = parent.elementsByTagNameNS("", "comments");
    if (nodes.size())
        result = nodes.at(0).toElement().text();
    return result;
}

QDateTime Parse::GetDCDateTime(const QDomElement& parent)
{
    QDomNodeList dates = parent.elementsByTagNameNS(kDC, "date");
    if (!dates.size())
        return {};
    return FromRFC3339(dates.at(0).toElement().text());
}

QDateTime Parse::RFC822TimeToQDateTime(const QString& t)
{
    static const QRegularExpression kNonDigitRE { R"(\D)" };

    if (t.size() < 20)
        return {};

    QString time = t.simplified();
    short int hoursShift = 0;
    short int minutesShift = 0;

    QStringList tmp = time.split(' ');
    if (tmp.isEmpty())
        return {};
    if (tmp.at(0).contains(kNonDigitRE))
        tmp.removeFirst();
    if (tmp.size() != 5)
        return {};
    QString tmpTimezone = tmp.takeAt(tmp.size() -1);
    if (tmpTimezone.size() == 5)
    {
        bool ok = false;
        int tz = tmpTimezone.toInt(&ok);
        if(ok)
        {
            hoursShift = tz / 100;
            minutesShift = tz % 100;
        }
    }
    else
    {
        hoursShift = m_timezoneOffsets.value(tmpTimezone, 0);
    }

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
        return {};
    result = result.addSecs((hoursShift * 3600 * (-1)) + (minutesShift * 60 * (-1)));
#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
    result.setTimeSpec(Qt::UTC);
#else
    result.setTimeZone(QTimeZone(QTimeZone::UTC));
#endif
    return result;
}

QDateTime Parse::FromRFC3339(const QString& t)
{
    if (t.size() < 19)
        return {};
    QDateTime result = MythDate::fromString(t.left(19).toUpper());
    static const QRegularExpression fractionalSeconds { R"(\.(\d+))" };
    auto match = fractionalSeconds.match(t);
    if (match.hasMatch())
    {
        bool ok = false;
        int fractional = match.capturedView(1).toInt(&ok);
        if (ok)
        {
            if (fractional < 100)
                fractional *= 10;
            if (fractional <10)
                fractional *= 100;
            result = result.addMSecs(fractional);
        }
    }
    static const QRegularExpression timeZone { R"((\+|\-)(\d\d):(\d\d)$)" };
    match = timeZone.match(t);
    if (match.hasMatch())
    {
        short int multiplier = -1;
        if (match.captured(1) == "-")
            multiplier = 1;
        int hoursShift =   match.capturedView(2).toInt();
        int minutesShift = match.capturedView(3).toInt();
        result = result.addSecs((hoursShift * 3600 * multiplier) + (minutesShift * 60 * multiplier));
    }
#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
    result.setTimeSpec(Qt::UTC);
#else
    result.setTimeZone(QTimeZone(QTimeZone::UTC));
#endif
    return result;
}

QList<Enclosure> Parse::GetEnclosures(const QDomElement& entry)
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

QList<MRSSEntry> Parse::GetMediaRSS(const QDomElement& item)
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

    static const QRegularExpression kStripHtmlRE {"<.*?>"};
    result.remove(kStripHtmlRE);

    return result;
}
