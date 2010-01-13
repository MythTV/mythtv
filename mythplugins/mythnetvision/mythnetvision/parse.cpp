#include <QFile>
#include <QDataStream>
#include <QDomDocument>
#include <QProcess>
#include <QDomImplementation>
#include <QHash>
#include <QUrl>
#include <QFileInfo>
#include <QRegExp>

#include "parse.h"

#include <mythtv/mythcontext.h>
#include <mythtv/mythdirs.h>

using namespace std;

ResultVideo::ResultVideo(const QString& title, const QString& desc,
              const QString& URL, const QString& thumbnail,
              const QString& mediaURL, const QString& enclosure,
              const QDateTime& date, const QString& time, 
              const QString& rating, const off_t& filesize,
              const QString& player, const QStringList& playerargs,
              const QString& download, const QStringList& downloadargs,
              const uint& width, const uint& height,
              const QString& language, const bool& downloadable)
{
    m_title = title;
    m_desc = desc;
    m_URL = URL;
    m_thumbnail = thumbnail;
    m_mediaURL = mediaURL;
    m_enclosure = enclosure;
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
}

ResultVideo::ResultVideo()
{
}

ResultVideo::~ResultVideo()
{
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
        QString URL_;
        QString Rating_;
        QString RatingScheme_;
        QString Title_;
        QString Description_;
        QString Keywords_;
        QString CopyrightURL_;
        QString CopyrightText_;
        int RatingAverage_;
        int RatingCount_;
        int RatingMin_;
        int RatingMax_;
        int Views_;
        int Favs_;
        QString Tags_;
        QList<MRSSThumbnail> Thumbnails_;
        QList<MRSSCredit> Credits_;
        QList<MRSSComment> Comments_;
        QList<MRSSPeerLink> PeerLinks_;
        QList<MRSSScene> Scenes_;

        /**  Updates *this's fields according to the
         * child. Some kind of merge.
         */
        ArbitraryLocatedData& operator+= (const ArbitraryLocatedData& child)
        {
            if (!child.URL_.isEmpty())
                URL_ = child.URL_;
            if (!child.Rating_.isEmpty())
                Rating_ = child.Rating_;
            if (!child.RatingScheme_.isEmpty())
                RatingScheme_ = child.RatingScheme_;
            if (!child.Title_.isEmpty())
                Title_ = child.Title_;
            if (!child.Description_.isEmpty())
                Description_ = child.Description_;
            if (!child.Keywords_.isEmpty())
                Keywords_ = child.Keywords_;
            if (!child.CopyrightURL_.isEmpty())
                CopyrightURL_ = child.CopyrightURL_;
            if (!child.CopyrightText_.isEmpty())
                CopyrightText_ = child.CopyrightText_;
            if (child.RatingAverage_ != 0)
                RatingAverage_ = child.RatingAverage_;
            if (child.RatingCount_ != 0)
                RatingCount_ = child.RatingCount_;
            if (child.RatingMin_ != 0)
                RatingMin_ = child.RatingMin_;
            if (child.RatingMax_ != 0)
                RatingMax_ = child.RatingMax_;
            if (child.Views_ != 0)
                Views_ = child.Views_;
            if (child.Favs_ != 0)
                Favs_ = child.Favs_;
            if (!child.Tags_.isEmpty())
                Tags_ = child.Tags_;

            Thumbnails_ += child.Thumbnails_;
            Credits_ += child.Credits_;
            Comments_ += child.Comments_;
            PeerLinks_ += child.PeerLinks_;
            Scenes_ += child.Scenes_;
            return *this;
        }
    };


public:
    MRSSParser() {}

    QList<MRSSEntry> operator() (const QDomElement& item)
    {
        QList<MRSSEntry> result;

        QDomNodeList groups = item.elementsByTagNameNS(QString("http://search.yahoo.com/mrss/"),
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
         QDomNodeList entries = holder.elementsByTagNameNS(Parse::MediaRSS_,
             "content");

         for (int i = 0; i < entries.size(); ++i)
         {
             MRSSEntry entry;

             QDomElement en = entries.at(i).toElement();
             ArbitraryLocatedData d = GetArbitraryLocatedDataFor(en);

             if (en.hasAttribute("url"))
                 entry.URL_ = en.attribute("url");
             else
                 entry.URL_ = d.URL_;

             entry.Size_ = en.attribute("fileSize").toInt();
             entry.Type_ = en.attribute("type");
             entry.Medium_ = en.attribute("medium");
             entry.IsDefault_ = (en.attribute("isDefault") == "true");
             entry.Expression_ = en.attribute("expression");
             if (entry.Expression_.isEmpty())
                 entry.Expression_ = "full";
             entry.Bitrate_ = en.attribute("bitrate").toInt();
             entry.Framerate_ = en.attribute("framerate").toDouble();
             entry.SamplingRate_ = en.attribute("samplingrate").toDouble();
             entry.Channels_ = en.attribute("channels").toInt();
             entry.Duration_ = en.attribute("duration").toInt();
             if (!en.attribute("width").isNull())
                 entry.Width_ = en.attribute("width").toInt();
             else
                 entry.Width_ = 0;
             if (!en.attribute("height").isNull())
                 entry.Height_ = en.attribute("height").toInt();
             else
                 entry.Height_ = 0;
             if (!en.attribute("lang").isNull())
                 entry.Lang_ = en.attribute("lang");
             else
                 entry.Lang_ = QString();

             entry.Rating_ = d.Rating_;
             entry.RatingScheme_ = d.RatingScheme_;
             entry.Title_ = d.Title_;
             entry.Description_ = d.Description_;
             entry.Keywords_ = d.Keywords_;
             entry.CopyrightURL_ = d.CopyrightURL_;
             entry.CopyrightText_ = d.CopyrightText_;
             if (d.RatingAverage_ != 0)
                 entry.RatingAverage_ = d.RatingAverage_;
             else
                 entry.RatingAverage_ = 0;
             entry.RatingCount_ = d.RatingCount_;
             entry.RatingMin_ = d.RatingMin_;
             entry.RatingMax_ = d.RatingMax_;
             entry.Views_ = d.Views_;
             entry.Favs_ = d.Favs_;
             entry.Tags_ = d.Tags_;
             entry.Thumbnails_ = d.Thumbnails_;
             entry.Credits_ = d.Credits_;
             entry.Comments_ = d.Comments_;
             entry.PeerLinks_ = d.PeerLinks_;
             entry.Scenes_ = d.Scenes_;

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
        QList<QDomNode> elems = GetDirectChildrenNS(element, Parse::MediaRSS_,
            "player");
        if (!elems.size())
            return QString();

        return QString(elems.at(0).toElement().attribute("url"));
    }

    QString GetTitle(const QDomElement& element)
    {
        QList<QDomNode> elems = GetDirectChildrenNS(element, Parse::MediaRSS_,
            "title");

        if (!elems.size())
            return QString();

        QDomElement telem = elems.at(0).toElement();
        return QString(Parse::UnescapeHTML(telem.text()));
    }

    QString GetDescription(const QDomElement& element)
    {
        QList<QDomNode> elems = GetDirectChildrenNS(element, Parse::MediaRSS_,
            "description");

        if (!elems.size())
            return QString();

        QDomElement telem = elems.at(0).toElement();
        return QString(Parse::UnescapeHTML(telem.text()));
    }

    QString GetKeywords(const QDomElement& element)
    {
        QList<QDomNode> elems = GetDirectChildrenNS(element, Parse::MediaRSS_,
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
        QList<QDomNode> thumbs = GetDirectChildrenNS(element, Parse::MediaRSS_,
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
        QList<QDomNode> credits = GetDirectChildrenNS(element, Parse::MediaRSS_,
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
        QList<QDomNode> commParents = GetDirectChildrenNS(element, Parse::MediaRSS_,
            "comments");

        if (commParents.size())
        {
            QDomNodeList comments = commParents.at(0).toElement()
                .elementsByTagNameNS(Parse::MediaRSS_,
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

        QList<QDomNode> respParents = GetDirectChildrenNS(element, Parse::MediaRSS_,
            "responses");

        if (respParents.size())
        {
            QDomNodeList responses = respParents.at(0).toElement()
                .elementsByTagNameNS(Parse::MediaRSS_,
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

        QList<QDomNode> backParents = GetDirectChildrenNS(element, Parse::MediaRSS_,
            "backLinks");

        if (backParents.size())
        {
            QDomNodeList backlinks = backParents.at(0).toElement()
                .elementsByTagNameNS(Parse::MediaRSS_,
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
        QList<QDomNode> links = GetDirectChildrenNS(element, Parse::MediaRSS_,
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
        QList<QDomNode> scenesNode = GetDirectChildrenNS(element, Parse::MediaRSS_,
            "scenes");

        if (scenesNode.size())
        {
            QDomNodeList scenesNodes = scenesNode.at(0).toElement()
                .elementsByTagNameNS(Parse::MediaRSS_, "scene");

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
            QList<QDomNode> elems = GetDirectChildrenNS(element, Parse::MediaRSS_,
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
            QList<QDomNode> elems = GetDirectChildrenNS(element, Parse::MediaRSS_,
                "copyright");

            if (elems.size())
            {
                QDomElement celem = elems.at(0).toElement();
                ctext = celem.text();
                if (celem.hasAttribute("url"))
                    curl = celem.attribute("url");
            }
        }

        int raverage;
        int rcount;
        int rmin;
        int rmax;
        int views;
        int favs;
        QString tags;
        {
            QList<QDomNode> comms = GetDirectChildrenNS(element, Parse::MediaRSS_,
                "community");
            if (comms.size())
            {
                QDomElement comm = comms.at(0).toElement();
                QDomNodeList stars = comm.elementsByTagNameNS(Parse::MediaRSS_,
                    "starRating");
                if (stars.size())
                {
                    QDomElement rating = stars.at(0).toElement();
                    raverage = GetInt(rating, "average");
                    rcount = GetInt(rating, "count");
                    rmin = GetInt(rating, "min");
                    rmax = GetInt(rating, "max");
                }

                QDomNodeList stats = comm.elementsByTagNameNS(Parse::MediaRSS_,
                    "statistics");
                if (stats.size())
                {
                    QDomElement stat = stats.at(0).toElement();
                    views = GetInt(stat, "views");
                    favs = GetInt(stat, "favorites");
                 }

                QDomNodeList tagsNode = comm.elementsByTagNameNS(Parse::MediaRSS_,
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

const QString Parse::DC_ = "http://purl.org/dc/elements/1.1/";
const QString Parse::WFW_ = "http://wellformedweb.org/CommentAPI/";
const QString Parse::Atom_ = "http://www.w3.org/2005/Atom";
const QString Parse::RDF_ = "http://www.w3.org/1999/02/22-rdf-syntax-ns#";
const QString Parse::Slash_ = "http://purl.org/rss/1.0/modules/slash/";
const QString Parse::Enc_ = "http://purl.oclc.org/net/rss_2.0/enc#";
const QString Parse::ITunes_ = "http://www.itunes.com/dtds/podcast-1.0.dtd";
const QString Parse::GeoRSSSimple_ = "http://www.georss.org/georss";
const QString Parse::GeoRSSW3_ = "http://www.w3.org/2003/01/geo/wgs84_pos#";
const QString Parse::MediaRSS_ = "http://search.yahoo.com/mrss/";

Parse::Parse()
{
}

Parse::~Parse()
{
}

ResultVideo::resultList Parse::parseRSS(QDomDocument domDoc)
{
    ResultVideo::resultList vList;

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

ResultVideo* Parse::ParseItem(const QDomElement& item) const
{
    QString title, description, url, author, duration, rating,
            thumbnail, mediaURL, player, language, download = NULL;
    off_t filesize = 0;
    uint width, height = 0;
    QDateTime date;
    QStringList playerargs, downloadargs;
    bool downloadable = true;

    title = item.firstChildElement("title").text();
    title = UnescapeHTML(title);
    if (title.isEmpty())
        title = "";

    description = item.firstChildElement("description").text();
    if (description.isEmpty())
    {
        QDomNodeList nodes = item.elementsByTagNameNS(ITunes_, "summary");
        if (nodes.size())
            description = nodes.at(0).toElement().text();
    }
    if (description.isEmpty())
        description = "";
    else
        description = UnescapeHTML(description);

    url = item.firstChildElement("link").text();

    author = item.firstChildElement("author").text();
    if (author.isEmpty())
        author = GetAuthor(item);

    date = RFC822TimeToQDateTime(item.firstChildElement("pubDate").text());
    if (!date.isValid() || date.isNull())
        date = GetDCDateTime(item);
    if (!date.isValid() || date.isNull())
        date = QDateTime::currentDateTime();

    QDomNodeList dur = item.elementsByTagNameNS(ITunes_, "duration");
    if (dur.size())
    {
        duration = dur.at(0).toElement().text();
    }

    rating = item.firstChildElement("rating").text();

    player = item.firstChildElement("player").text();
    playerargs = item.firstChildElement("playerargs").text().split(" ");
    download = item.firstChildElement("download").text();
    downloadargs = item.firstChildElement("downloadargs").text().split(" ");

    QList<MRSSEntry> enclosures = GetMediaRSS(item);

    if (enclosures.size())
    {
        MRSSEntry media = enclosures.takeAt(0);

        QList<MRSSThumbnail> thumbs = media.Thumbnails_;
        if (thumbs.size())
        {
            MRSSThumbnail thumb = thumbs.takeAt(0);
            thumbnail = thumb.URL_;
        }

        mediaURL = media.URL_;

        width = media.Width_;
        height = media.Height_;
        language = media.Lang_;

        if (duration.isEmpty())
            duration = media.Duration_;

        if (filesize == 0)
            filesize = media.Size_;

        if (rating.isEmpty())
            rating = QString::number(media.RatingAverage_);
    }
    if (mediaURL.isEmpty())
    {
        QList<Enclosure> stdEnc = GetEnclosures(item);

        if (stdEnc.size())
        {
            Enclosure e = stdEnc.takeAt(0);

            mediaURL = e.URL_;

            if (filesize == 0)
                filesize = e.Length_;
        }
    }

    if (mediaURL.isNull() || mediaURL == url)
        downloadable = false;

    return(new ResultVideo(title, description, url, thumbnail,
              mediaURL, author, date, duration,
              rating, filesize, player, playerargs,
              download, downloadargs, width, height,
              language, downloadable));
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
    QDomNodeList nodes = parent.elementsByTagNameNS(ITunes_,
        "author");
    if (nodes.size())
    {
        result = nodes.at(0).toElement().text();
        return result;
    }

    nodes = parent.elementsByTagNameNS(DC_,
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
    QDomNodeList nodes = parent.elementsByTagNameNS(WFW_,
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
    QDomNodeList dates = parent.elementsByTagNameNS(DC_, "date");
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
        hoursShift = TimezoneOffsets_.value(timezone, 0);
   
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
