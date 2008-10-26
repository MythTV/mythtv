// C headers
#include <cstdlib>

// C++ headers
#include <iostream>
using namespace std;

// QT headers
#include <QFile>
#include <QDataStream>

// Myth headers
#include <mythcontext.h>
#include <mythdirs.h>

// MythNews headers
#include "newsengine.h"

NewsArticle::NewsArticle(const QString &title,
                         const QString &desc, const QString &articleURL,
                         const QString &thumbnail, const QString &mediaURL,
                         const QString &enclosure) :
    m_title(title),
    m_desc(desc),
    m_articleURL(articleURL),
    m_thumbnail(thumbnail),
    m_mediaURL(mediaURL),
    m_enclosure(enclosure)
{
}

NewsArticle::NewsArticle(const QString &title) :
    m_title(title),
    m_desc(QString::null),
    m_articleURL(QString::null),
    m_thumbnail(QString::null),
    m_mediaURL(QString::null),
    m_enclosure(QString::null)
{
}

NewsArticle::NewsArticle() :
    m_title(QString::null),
    m_desc(QString::null),
    m_articleURL(QString::null),
    m_thumbnail(QString::null),
    m_mediaURL(QString::null),
    m_enclosure(QString::null)
{
}

NewsSite::NewsSite(const QString   &name,
                   const QString   &url,
                   const QDateTime &updated,
                   const bool       podcast) :
    QObject(), m_name(name),  m_url(url), m_urlReq(url),
    m_desc(QString::null), m_updated(updated),
    m_destDir(GetConfDir()+"/MythNews"),
    /*m_data(),*/
    m_state(NewsSite::Success),
    m_errorString(QString::null),
    m_updateErrorString(QString::null),
    m_imageURL(""),
    m_podcast(podcast)
{
}

void NewsSite::deleteLater()
{
    MythHttpPool::GetSingleton()->RemoveListener(this);
    m_articleList.clear();
    QObject::deleteLater();
}

NewsSite::~NewsSite()
{
    MythHttpPool::GetSingleton()->RemoveListener(this);
}

void NewsSite::insertNewsArticle(const NewsArticle &item)
{
    m_articleList.push_back(item);
}

void NewsSite::clearNewsArticles(void)
{
    m_articleList.clear();
}

QString NewsSite::url(void) const
{
    return m_url;
}

QString NewsSite::name(void) const
{
    return m_name;
}

bool NewsSite::podcast(void) const
{
    return m_podcast;
}

QString NewsSite::description(void) const
{
    QString desc(m_desc+"\n"+m_errorString);
    return desc;
}

QString NewsSite::imageURL(void) const
{
    return m_imageURL;
}

QDateTime NewsSite::lastUpdated(void) const
{
    return m_updated;
}

unsigned int NewsSite::timeSinceLastUpdate(void) const
{
    QDateTime curTime(QDateTime::currentDateTime());
    unsigned int min = m_updated.secsTo(curTime)/60;
    return min;
}

NewsArticle::List &NewsSite::articleList(void)
{
    return m_articleList;
}

#define LOC      QString("NewsSite: ")
#define LOC_WARN QString("NewsSite, Warning: ")
#define LOC_ERR  QString("NewsSite, Error: ")

void NewsSite::retrieve(void)
{
    stop();
    m_state = NewsSite::Retrieving;
    m_data.resize(0);
    m_errorString = QString::null;
    m_updateErrorString = QString::null;
    m_articleList.clear();
    m_urlReq = QUrl(m_url);
    MythHttpPool::GetSingleton()->AddUrlRequest(m_urlReq, this);
}

void NewsSite::stop(void)
{
    MythHttpPool::GetSingleton()->RemoveUrlRequest(m_urlReq, this);
}

bool NewsSite::successful(void) const
{
    return (m_state == NewsSite::Success);
}

QString NewsSite::errorMsg(void) const
{
    return m_errorString;
}

void NewsSite::Update(QHttp::Error      error,
                      const QString    &error_str,
                      const QUrl       &url,
                      uint              http_status_id,
                      const QString    &http_status_str,
                      const QByteArray &data)
{
    if (url != m_urlReq)
    {
        return;
    }

    if (QHttp::NoError != error)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "HTTP Connection Error" +
                QString("\n\t\t\tExplanation: %1: %2")
                .arg(error).arg(error_str));

        m_state = NewsSite::RetrieveFailed;
        m_updateErrorString = QString("%1: %2").arg(error).arg(error_str);
        emit finished(this);
        return;
    }

    if (200 != http_status_id)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "HTTP Protocol Error" +
                QString("\n\t\t\tExplanation: %1: %2")
                .arg(http_status_id).arg(http_status_str));

        m_state = NewsSite::RetrieveFailed;
        m_updateErrorString =
            QString("%1: %2").arg(http_status_id).arg(http_status_str);
        emit finished(this);
        return;
    }

    m_updateErrorString = QString::null;
    m_data = data;

    if (m_name.isEmpty())
    {
        m_state = NewsSite::WriteFailed;
    }
    else
    {
        QFile xmlFile(QString("%1/%2").arg(m_destDir).arg(m_name));
        if (xmlFile.open(QIODevice::WriteOnly))
        {
            xmlFile.write(m_data);
            xmlFile.close();
            m_updated = QDateTime::currentDateTime();
            m_state = NewsSite::Success;
        }
        else
        {
            m_state = NewsSite::WriteFailed;
        }
    }

    if (NewsSite::WriteFailed == m_state)
        VERBOSE(VB_IMPORTANT, "MythNews: NewsEngine: Write failed");

    emit finished(this);
}

void NewsSite::process(void)
{
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
        VERBOSE(VB_IMPORTANT, "MythNews: NewsEngine: failed to open xmlfile");
        if (!m_updateErrorString.isEmpty())
            m_errorString += "\n" + m_updateErrorString;
        return;
    }

    if (!domDoc.setContent(&xmlFile))
    {
        insertNewsArticle(NewsArticle(tr("Failed to retrieve news")));
        VERBOSE(VB_IMPORTANT, "MythNews: NewsEngine: "
                "failed to set content from xmlfile");
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
    if(rootName == QString::fromLatin1("rss") ||
       rootName == QString::fromLatin1("rdf:RDF")) {
        parseRSS(domDoc);
        xmlFile.close();
        return;
    }
    else if (rootName == QString::fromLatin1("feed")) {
        parseAtom(domDoc);
        xmlFile.close();
        return;
    }
    else {
        VERBOSE(VB_IMPORTANT, "MythNews: NewsEngine: XML-file is not valid RSS-feed");
        m_errorString += tr("XML-file is not valid RSS-feed");
        return;
    }

}

void NewsSite::parseRSS(QDomDocument domDoc)
{
    QDomNode channelNode = domDoc.documentElement().namedItem(QString::fromLatin1("channel"));

    m_desc = channelNode.namedItem(QString::fromLatin1("description")).toElement().text().simplified();

    QDomNode imageNode = channelNode.namedItem(QString::fromLatin1("image"));
    if (!imageNode.isNull())
        m_imageURL = imageNode.namedItem(QString::fromLatin1("url")).toElement().text().simplified();

    QDomNodeList items = domDoc.elementsByTagName(QString::fromLatin1("item"));

    QDomNode itemNode;
    QString title, description, url, thumbnail, mediaurl, enclosure, imageURL, enclosure_type;
    for (unsigned int i = 0; i < (unsigned) items.count(); i++) {
        itemNode = items.item(i);
        title    = itemNode.namedItem(QString::fromLatin1("title")).toElement().text().simplified();
        if (!title.isNull())
            ReplaceHtmlChar(title);

        QDomNode descNode = itemNode.namedItem(QString::fromLatin1("description"));
        if (!descNode.isNull())
        {
            description = descNode.toElement().text().simplified();
            ReplaceHtmlChar(description);
        }
        else
            description = QString::null;

        QDomNode linkNode = itemNode.namedItem(QString::fromLatin1("link"));
        if (!linkNode.isNull())
            url = linkNode.toElement().text().simplified();
        else
            url = QString::null;

        QDomNode enclosureNode = itemNode.namedItem(QString::fromLatin1("enclosure"));
        if (!enclosureNode.isNull())
        {
            QDomAttr enclosureURL = enclosureNode.toElement().attributeNode("url");
            if (!enclosureURL.isNull())
               enclosure  = enclosureURL.value();

            QDomAttr enclosureType = enclosureNode.toElement().attributeNode("type");
            if (!enclosureType.isNull())
               enclosure_type  = enclosureType.value();

            // VERBOSE(VB_GENERAL, QString("MythNews: Enclosure URL is %1").arg(enclosure));
        } else
            enclosure = QString::null;

        // From this point forward, we process RSS 2.0 media tags.  Please put all
        // other tag processing before this
        // Some media: tags can be enclosed in a media:group item.  If this item is
        // present, use it to find the media tags, otherwise, proceed.
        QDomNode mediaGroup = itemNode.namedItem(QString::fromLatin1("media:group"));
        if (!mediaGroup.isNull())
            itemNode = mediaGroup;

        QDomNode thumbNode = itemNode.namedItem(QString::fromLatin1("media:thumbnail"));
        if (!thumbNode.isNull())
        {
            QDomAttr thumburl = thumbNode.toElement().attributeNode("url");
            if (!thumburl.isNull())
                thumbnail = thumburl.value();
            // VERBOSE(VB_GENERAL, QString("MythNews: Thumbnail is %1").arg(thumbnail));
        } else
            thumbnail = QString::null;

        QDomNode playerNode = itemNode.namedItem(QString::fromLatin1("media:player"));
        if (!playerNode.isNull())
        {
            QDomAttr mediaURL = playerNode.toElement().attributeNode("url");
            if (!mediaURL.isNull())
               mediaurl  = mediaURL.value();
            // VERBOSE(VB_GENERAL, QString("MythNews: Media URL is %1").arg(mediaurl));
        } else
            mediaurl = QString::null;

        // If present, the media:description superscedes the RSS description
        descNode = itemNode.namedItem(QString::fromLatin1("media:description"));
        if (!descNode.isNull())
            description = descNode.toElement().text().simplified();

        if (enclosure.isEmpty())
        {
            QDomNode contentNode = itemNode.namedItem(QString::fromLatin1("media:content"));
            if (!contentNode.isNull())
            {
                QDomAttr enclosureURL = contentNode.toElement().attributeNode("url");
                if (!enclosureURL.isNull())
                   enclosure  = enclosureURL.value();

                QDomAttr enclosureType = contentNode.toElement().attributeNode("type");
                if (!enclosureType.isNull())
                   enclosure_type  = enclosureType.value();
                // VERBOSE(VB_GENERAL, QString("MythNews: media:content URL is %1").arg(enclosure));

                // This hack causes the engine to prefer other content over MP4 encoded content
                // because this can often include AAC audio which requires additional compile options
                // to enable in MythTV Internal player
                /*
                while (enclosure_type == "video/mp4")
                {
                    QDomNode altcontentNode = contentNode.nextSibling();
                    if (altcontentNode.nodeName() != "media:content")
                        break;

                    QDomAttr enclosureType = altcontentNode.toElement().attributeNode("type");
                    if (!enclosureType.isNull())
                    {
                        enclosure_type  = enclosureType.value();

                        QDomAttr altenclosureURL = altcontentNode.toElement().attributeNode("url");
                        if (!altenclosureURL.isNull())
                            enclosure  = altenclosureURL.value();
                    }
                }
                */
            }
        }
        insertNewsArticle(NewsArticle(title, description, url,
                                      thumbnail, mediaurl, enclosure));
    }
}

void NewsSite::parseAtom(QDomDocument domDoc)
{
    QDomNodeList entries = domDoc.elementsByTagName(QString::fromLatin1("entry"));

    QDomNode itemNode;
    QString title, description, url, thumbnail, mediaurl, enclosure, imageURL, enclosure_type;
    for (unsigned int i = 0; i < (unsigned) entries.count(); i++) {
        itemNode = entries.item(i);
        title    = itemNode.namedItem(QString::fromLatin1("title")).toElement().text().simplified();
        if (!title.isNull())
            ReplaceHtmlChar(title);

        QDomNode summNode = itemNode.namedItem(QString::fromLatin1("summary"));
        if (!summNode.isNull())
        {
            description = summNode.toElement().text().simplified();
            ReplaceHtmlChar(description);
        }
        else
            description = QString::null;

        QDomNode linkNode = itemNode.namedItem(QString::fromLatin1("link"));
        if (!linkNode.isNull()){
            QDomAttr linkHref = linkNode.toElement().attributeNode("href");
            if(!linkHref.isNull())
                url = linkHref.value();
        }
        else
            url = QString::null;

        insertNewsArticle(NewsArticle(title, description, url,
                                      QString::null, QString::null,
                                      QString::null));
    }
}

void NewsSite::ReplaceHtmlChar(QString &s)
{
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
}

