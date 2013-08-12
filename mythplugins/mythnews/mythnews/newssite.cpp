// QT headers
#include <QFile>

// MythTV headers
#include <mythdate.h>
#include <mythlogging.h>
#include <mythdirs.h>
#include <mythdownloadmanager.h>
#include <mythevent.h>

// MythNews headers
#include "newssite.h"

#define LOC      QString("NewsSite: ")
#define LOC_WARN QString("NewsSite, Warning: ")
#define LOC_ERR  QString("NewsSite, Error: ")

NewsSite::NewsSite(const QString   &name,
                   const QString   &url,
                   const QDateTime &updated,
                   const bool       podcast) :
    QObject(),
    m_lock(QMutex::Recursive),
    m_name(name),  m_url(url), m_urlReq(url),
    m_desc(QString::null), m_updated(updated),
    m_destDir(GetConfDir()+"/MythNews"),
    m_state(NewsSite::Success),
    m_errorString(QString::null),
    m_updateErrorString(QString::null),
    m_imageURL(""),
    m_podcast(podcast)
{
}

void NewsSite::deleteLater()
{
    QMutexLocker locker(&m_lock);
    GetMythDownloadManager()->removeListener(this);
    GetMythDownloadManager()->cancelDownload(m_url, false);
    m_articleList.clear();
    QObject::deleteLater();
}

NewsSite::~NewsSite()
{
    QMutexLocker locker(&m_lock);
    GetMythDownloadManager()->removeListener(this);
    GetMythDownloadManager()->cancelDownload(m_url, false);
}

void NewsSite::insertNewsArticle(const NewsArticle &item)
{
    QMutexLocker locker(&m_lock);
    m_articleList.push_back(item);
}

void NewsSite::clearNewsArticles(void)
{
    QMutexLocker locker(&m_lock);
    m_articleList.clear();
}

void NewsSite::retrieve(void)
{
    QMutexLocker locker(&m_lock);

    stop();
    m_state = NewsSite::Retrieving;
    m_errorString = QString::null;
    m_updateErrorString = QString::null;
    m_articleList.clear();
    QString destFile = QString("%1/%2").arg(m_destDir).arg(m_name);
    GetMythDownloadManager()->queueDownload(m_url, destFile, this);
}

void NewsSite::stop(void)
{
    QMutexLocker locker(&m_lock);
    GetMythDownloadManager()->removeListener(this);
    GetMythDownloadManager()->cancelDownload(m_url);
}

bool NewsSite::successful(void) const
{
    QMutexLocker locker(&m_lock);
    return (m_state == NewsSite::Success);
}

QString NewsSite::errorMsg(void) const
{
    QMutexLocker locker(&m_lock);
    return m_errorString;
}

QString NewsSite::url(void) const
{
    QMutexLocker locker(&m_lock);
    return m_url;
}

QString NewsSite::name(void) const
{
    QMutexLocker locker(&m_lock);
    return m_name;
}

bool NewsSite::podcast(void) const
{
    QMutexLocker locker(&m_lock);
    return m_podcast;
}

QString NewsSite::description(void) const
{
    QMutexLocker locker(&m_lock);
    QString result;

    if (!m_desc.isEmpty())
        result = m_desc;

    if (!m_errorString.isEmpty())
        result += m_desc.isEmpty() ? m_errorString : '\n' + m_errorString;

    return result;
}

QString NewsSite::imageURL(void) const
{
    QMutexLocker locker(&m_lock);
    return m_imageURL;
}

NewsArticle::List NewsSite::GetArticleList(void) const
{
    QMutexLocker locker(&m_lock);
    return m_articleList;
}

QDateTime NewsSite::lastUpdated(void) const
{
    QMutexLocker locker(&m_lock);
    return m_updated;
}

unsigned int NewsSite::timeSinceLastUpdate(void) const
{
    QMutexLocker locker(&m_lock);

    QDateTime curTime(MythDate::current());
    unsigned int min = m_updated.secsTo(curTime)/60;
    return min;
}

void NewsSite::customEvent(QEvent *event)
{
    if ((MythEvent::Type)(event->type()) == MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent *)event;
        QStringList tokens = me->Message().split(" ", QString::SkipEmptyParts);

        if (tokens.isEmpty())
            return;

        if (tokens[0] == "DOWNLOAD_FILE")
        {
            QStringList args = me->ExtraDataList();

            if (tokens[1] == "UPDATE")
            {
                // could update a progressbar here
            }
            else if (tokens[1] == "FINISHED")
            {
                QString filename = args[1];
                int fileSize  = args[2].toInt();
                QString errorStr = args[3];
                int errorCode = args[4].toInt();

                if ((errorCode != 0) || (fileSize == 0))
                {
                    LOG(VB_GENERAL, LOG_ERR, LOC + "HTTP Connection Error" +
                        QString("\n\t\t\tExplanation: %1: %2")
                                .arg(errorCode).arg(errorStr));

                    m_state = NewsSite::RetrieveFailed;
                    m_updateErrorString = QString("%1: %2").arg(errorCode).arg(errorStr);
                    emit finished(this);
                    return;
                }
                else
                {
                    m_updateErrorString = QString::null;
                    //m_data = data;

                    if (m_name.isEmpty())
                    {
                        m_state = NewsSite::WriteFailed;
                    }
                    else
                    {
                        if (QFile::exists(filename))
                        {
                            m_updated = MythDate::current();
                            m_state = NewsSite::Success;
                        }
                        else
                        {
                            m_state = NewsSite::WriteFailed;
                        }
                    }

                    emit finished(this);
                }
            }
        }
    }
}

void NewsSite::process(void)
{
    QMutexLocker locker(&m_lock);

    m_articleList.clear();

    m_errorString = "";
    if (RetrieveFailed == m_state)
        m_errorString = tr("Retrieve Failed. ")+"\n";
 
    QDomDocument domDoc;

    QFile xmlFile(m_destDir+QString("/")+m_name);
    if (!xmlFile.exists())
    {
        insertNewsArticle(NewsArticle(tr("Failed to retrieve news")));
        m_errorString += tr("No Cached News.");
        if (!m_updateErrorString.isEmpty())
            m_errorString += "\n" + m_updateErrorString;
        return;
    }

    if (!xmlFile.open(QIODevice::ReadOnly))
    {
        insertNewsArticle(NewsArticle(tr("Failed to retrieve news")));
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to open xmlfile");
        if (!m_updateErrorString.isEmpty())
            m_errorString += "\n" + m_updateErrorString;
        return;
    }

    if (!domDoc.setContent(&xmlFile))
    {
        insertNewsArticle(NewsArticle(tr("Failed to retrieve news")));
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to set content from xmlfile");
        m_errorString += tr("Failed to read downloaded file.");
        if (!m_updateErrorString.isEmpty())
            m_errorString += "\n" + m_updateErrorString;
        return;
    }

    if (RetrieveFailed == m_state)
    {
        m_errorString += tr("Showing Cached News.");
        if (!m_updateErrorString.isEmpty())
            m_errorString += "\n" + m_updateErrorString;
    }

    //Check the type of the feed
    QString rootName = domDoc.documentElement().nodeName();
    if (rootName == "rss" || rootName == "rdf:RDF")
    {
        parseRSS(domDoc);
        xmlFile.close();
        return;
    }
    else if (rootName == "feed")
    {
        parseAtom(domDoc);
        xmlFile.close();
        return;
    }
    else {
        LOG(VB_GENERAL, LOG_ERR, LOC + "XML-file is not valid RSS-feed");
        m_errorString += tr("XML-file is not valid RSS-feed");
        return;
    }

}

static bool isImage(const QString &mimeType)
{
    if (mimeType == "image/png" || mimeType == "image/jpeg" ||
        mimeType == "image/jpg" || mimeType == "image/gif" ||
        mimeType == "image/bmp")
        return true;

    return false;
}

static bool isVideo(const QString &mimeType)
{
    if (mimeType == "video/mpeg" || mimeType == "video/x-ms-wmv" ||
        mimeType == "application/x-troff-msvideo" || mimeType == "video/avi" ||
        mimeType == "video/msvideo" || mimeType == "video/x-msvideo")
        return true;

    return false;
}

void NewsSite::parseRSS(QDomDocument domDoc)
{
    QMutexLocker locker(&m_lock);

    QDomNode channelNode = domDoc.documentElement().namedItem("channel");

    m_desc = channelNode.namedItem("description")
        .toElement().text().simplified();

    QDomNode imageNode = channelNode.namedItem("image");
    if (!imageNode.isNull())
        m_imageURL = imageNode.namedItem("url").toElement().text().simplified();

    QDomNodeList items = domDoc.elementsByTagName("item");

    for (unsigned int i = 0; i < (unsigned) items.count(); i++)
    {
        QDomNode itemNode = items.item(i);
        QString title = ReplaceHtmlChar(itemNode.namedItem("title")
                                        .toElement().text().simplified());

        QDomNode descNode = itemNode.namedItem("description");
        QString description = QString::null;
        if (!descNode.isNull())
        {
            description = descNode.toElement().text().simplified();
            description = ReplaceHtmlChar(description);
        }

        QDomNode linkNode = itemNode.namedItem("link");
        QString url = QString::null;
        if (!linkNode.isNull())
            url = linkNode.toElement().text().simplified();

        QDomNode enclosureNode = itemNode.namedItem("enclosure");
        QString enclosure = QString::null;
        QString enclosure_type = QString::null;
        QString thumbnail = QString::null;
        if (!enclosureNode.isNull())
        {
            QDomAttr enclosureURL = enclosureNode.toElement()
                .attributeNode("url");

            if (!enclosureURL.isNull())
                enclosure  = enclosureURL.value();

            QDomAttr enclosureType = enclosureNode.toElement()
                .attributeNode("type");
            if (!enclosureType.isNull()) 
            {
                enclosure_type  = enclosureType.value();

                if (enclosure_type == "image/jpeg")
                {
                    thumbnail = enclosure;
                    enclosure = QString::null;
                }
            }
        }

        //////////////////////////////////////////////////////////////
        // From this point forward, we process RSS 2.0 media tags.
        // Please put all other tag processing before this comment.
        // See http://www.rssboard.org/media-rss for details
        //////////////////////////////////////////////////////////////

        // Some media: tags can be enclosed in a media:group item.
        // If this item is present, use it to find the media tags,
        // otherwise, proceed.
        QDomNode mediaGroup = itemNode.namedItem("media:group");
        if (!mediaGroup.isNull())
            itemNode = mediaGroup;

        QDomNode thumbNode = itemNode.namedItem("media:thumbnail");
        if (!thumbNode.isNull())
        {
            QDomAttr thumburl = thumbNode.toElement().attributeNode("url");
            if (!thumburl.isNull())
                thumbnail = thumburl.value();
        }

        QDomNode playerNode = itemNode.namedItem("media:player");
        QString mediaurl = QString::null;
        if (!playerNode.isNull())
        {
            QDomAttr mediaURL = playerNode.toElement().attributeNode("url");
            if (!mediaURL.isNull())
                mediaurl = mediaURL.value();
        }

        // If present, the media:description superscedes the RSS description
        descNode = itemNode.namedItem("media:description");
        if (!descNode.isNull())
            description = descNode.toElement().text().simplified();

        // parse any media:content items looking for any images or videos
        QDomElement e = itemNode.toElement();
        QDomNodeList mediaNodes = e.elementsByTagName("media:content");
        for (int x = 0; x < mediaNodes.count(); x++)
        {
            QString medium;
            QString type;
            QString url;

            QDomElement mediaElement = mediaNodes.at(x).toElement();

            if (mediaElement.isNull())
                continue;

            if (mediaElement.hasAttribute("medium"))
                medium = mediaElement.attributeNode("medium").value();

            if (mediaElement.hasAttribute("type"))
                type = mediaElement.attributeNode("type").value();

            if (mediaElement.hasAttribute("url"))
                url = mediaElement.attributeNode("url").value();

            LOG(VB_GENERAL, LOG_DEBUG,
                QString("parseRSS found media:content: medium: %1, type: %2, url: %3")
                        .arg(medium).arg(type).arg(url));

            // if this is an image use it as the thumbnail if we haven't found one yet
            if (thumbnail.isEmpty() && (medium == "image" || isImage(type)))
                thumbnail = url;

            // if this is a video use it as the enclosure if we haven't found one yet
            if (enclosure.isEmpty() && (medium == "video" || isVideo(type)))
                enclosure = url;
        }

        insertNewsArticle(NewsArticle(title, description, url,
                                      thumbnail, mediaurl, enclosure));
    }
}

void NewsSite::parseAtom(QDomDocument domDoc)
{
    QDomNodeList entries = domDoc.elementsByTagName("entry");

    for (unsigned int i = 0; i < (unsigned) entries.count(); i++)
    {
        QDomNode itemNode = entries.item(i);
        QString title =  ReplaceHtmlChar(itemNode.namedItem("title").toElement()
                                         .text().simplified());

        QDomNode summNode = itemNode.namedItem("summary");
        QString description = QString::null;
        if (!summNode.isNull())
        {
            description = ReplaceHtmlChar(
                summNode.toElement().text().simplified());
        }

        QDomNode linkNode = itemNode.namedItem("link");
        QString url = QString::null;
        if (!linkNode.isNull())
        {
            QDomAttr linkHref = linkNode.toElement().attributeNode("href");
            if (!linkHref.isNull())
                url = linkHref.value();
        }

        insertNewsArticle(NewsArticle(title, description, url));
    }
}

QString NewsSite::ReplaceHtmlChar(const QString &orig)
{
    if (orig.isEmpty())
        return orig;

    QString s = orig;
    s.replace("&amp;", "&");
    s.replace("&lt;", "<");
    s.replace("&gt;", ">");
    s.replace("&quot;", "\"");
    s.replace("&apos;", "\'");
    s.replace("&#8230;",QChar(8230));
    s.replace("&#233;",QChar(233));
    s.replace("&mdash;", QChar(8212));
    s.replace("&nbsp;", " ");
    s.replace("&#160;", QChar(160));
    s.replace("&#225;", QChar(225));
    s.replace("&#8216;", QChar(8216));
    s.replace("&#8217;", QChar(8217));
    s.replace("&#039;", "\'");
    s.replace("&ndash;", QChar(8211));
    // german umlauts
    s.replace("&auml;", QChar(0x00e4));
    s.replace("&ouml;", QChar(0x00f6));
    s.replace("&uuml;", QChar(0x00fc));
    s.replace("&Auml;", QChar(0x00c4));
    s.replace("&Ouml;", QChar(0x00d6));
    s.replace("&Uuml;", QChar(0x00dc));
    s.replace("&szlig;", QChar(0x00df));

    return s;
}
