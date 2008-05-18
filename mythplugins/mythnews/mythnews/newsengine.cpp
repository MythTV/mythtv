/* ============================================================
 * File  : newsengine.cpp
 * Author: Renchi Raju <renchi@pooh.tam.uiuc.edu>
 * Date  : 2003-09-03
 * Description :
 *
 * Copyright 2003 by Renchi Raju

 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published bythe Free Software Foundation;
 * either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * ============================================================ */

#include <iostream>

#include <qfile.h>
#include <qdatastream.h>
#include <qdom.h>
#include <q3urloperator.h>
#include <q3network.h>

extern "C" {
#include <stdlib.h>
}

#include "newsengine.h"
#include <mythtv/mythcontext.h>

using namespace std;

NewsArticle::NewsArticle(NewsSite *parent, const QString& title,
                         const QString& desc, const QString& articleURL,
                         const QString& thumbnail, const QString& mediaURL,
                         const QString& enclosure)
{
    parent->insertNewsArticle(this);
    m_title  = title;
    m_desc   = desc;
    m_parent = parent;
    m_articleURL = articleURL;
    m_thumbnail = thumbnail;
    m_mediaURL = mediaURL;
    m_enclosure = enclosure;
}

NewsArticle::~NewsArticle()
{

}

NewsSite::NewsSite(const QString& name,
                   const QString& url,
                   const QDateTime& updated)
    : QObject()
{
    m_url     = url;
    m_name    = name;
    m_updated = updated;
    m_state   = NewsSite::Success;

    m_destDir  = MythContext::GetConfDir();
    m_destDir += "/MythNews";

    m_articleList.setAutoDelete(true);
    m_articleList.clear();

    m_data.resize(0);
    q3InitNetworkProtocols();
    m_urlOp = new Q3UrlOperator(m_url);

    connect(m_urlOp, SIGNAL(data(const QByteArray&, Q3NetworkOperation*)),
            this, SLOT(slotGotData(const QByteArray&, Q3NetworkOperation*)));
    connect(m_urlOp, SIGNAL(finished(Q3NetworkOperation*)),
            this, SLOT(slotFinished(Q3NetworkOperation*)));
}

NewsSite::~NewsSite()
{
    m_urlOp->disconnect(this, 0, 0, 0);
    m_urlOp->stop();
    m_urlOp->deleteLater();
    m_articleList.clear();
}

void NewsSite::insertNewsArticle(NewsArticle* item)
{
    m_articleList.append(item);
}

void NewsSite::clearNewsArticles()
{
    m_articleList.clear();
}

const QString& NewsSite::url() const
{
    return m_url;
}

const QString& NewsSite::name() const
{
    return m_name;
}

QString NewsSite::description() const
{
    QString desc(m_desc+"\n"+m_errorString);
    return desc;
}

const QString& NewsSite::imageURL() const
{
    return m_imageURL;
}

const QDateTime& NewsSite::lastUpdated() const
{
    return m_updated;
}

unsigned int NewsSite::timeSinceLastUpdate() const
{
    QDateTime curTime(QDateTime::currentDateTime());
    unsigned int min = m_updated.secsTo(curTime)/60;
    return min;
}

NewsArticle::List& NewsSite::articleList()
{
    return m_articleList;
}

void NewsSite::retrieve()
{
    stop();

    m_state = NewsSite::Retrieving;
    m_data.resize(0);
    m_articleList.clear();
    m_urlOp->get(m_url);
}

void NewsSite::stop()
{
    m_urlOp->stop();
}

bool NewsSite::successful() const
{
    return (m_state == NewsSite::Success);
}

QString NewsSite::errorMsg() const
{
    return m_errorString;
}

void NewsSite::slotFinished(Q3NetworkOperation* op)
{
    if (op->state() == Q3NetworkProtocol::StDone &&
        op->errorCode() == Q3NetworkProtocol::NoError) 
    {

        QFile xmlFile(m_destDir+QString("/")+m_name);
        if (xmlFile.open( QIODevice::WriteOnly )) {
            QDataStream stream( &xmlFile );
            stream.writeRawBytes(m_data.data(), m_data.size());
            xmlFile.close();
            m_updated = QDateTime::currentDateTime();
            m_state = NewsSite::Success;
        }
        else 
        {
            m_state = NewsSite::WriteFailed;
            VERBOSE(VB_IMPORTANT, "MythNews: NewsEngine: Write failed");
        }
    }
    else 
    {
        m_state = NewsSite::RetrieveFailed;
    }

    emit finished(this);
}

void NewsSite::process()
{
    m_articleList.clear();

    if (m_state == RetrieveFailed)
        m_errorString = tr("Retrieve Failed. ");
    else
        m_errorString = "";

    QDomDocument domDoc;

    QFile xmlFile(m_destDir+QString("/")+m_name);
    if (!xmlFile.exists()) {
        new NewsArticle(this, tr("Failed to retrieve news"), "", "", "", "", "");
        m_errorString += tr("No Cached News");
        return;
    }

    if (!xmlFile.open(QIODevice::ReadOnly)) {
        new NewsArticle(this, tr("Failed to retrieve news"), "", "", "", "", "");
        VERBOSE(VB_IMPORTANT, "MythNews: NewsEngine: failed to open xmlfile");
        return;
    }

    if (!domDoc.setContent(&xmlFile)) {
        new NewsArticle(this, tr("Failed to retrieve news"), "", "", "", "", "");
        VERBOSE(VB_IMPORTANT, "MythNews: NewsEngine: failed to set content from xmlfile");
        m_errorString += tr("Failed to read downloaded file");
        return;
    }


    if (m_state == RetrieveFailed)
        m_errorString += tr("Showing Cached News");

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

    m_desc = channelNode.namedItem(QString::fromLatin1("description")).toElement().text().simplifyWhiteSpace();

    QDomNode imageNode = channelNode.namedItem(QString::fromLatin1("image"));
    if (!imageNode.isNull())
        m_imageURL = imageNode.namedItem(QString::fromLatin1("url")).toElement().text().simplifyWhiteSpace();

    QDomNodeList items = domDoc.elementsByTagName(QString::fromLatin1("item"));

    QDomNode itemNode;
    QString title, description, url, thumbnail, mediaurl, enclosure, imageURL, enclosure_type;
    for (unsigned int i = 0; i < items.count(); i++) {
        itemNode = items.item(i);
        title    = itemNode.namedItem(QString::fromLatin1("title")).toElement().text().simplifyWhiteSpace();
        if (!title.isNull())
            ReplaceHtmlChar(title);

        QDomNode descNode = itemNode.namedItem(QString::fromLatin1("description"));
        if (!descNode.isNull())
        {
            description = descNode.toElement().text().simplifyWhiteSpace();
            ReplaceHtmlChar(description);
        }            
        else
            description = QString::null;

        QDomNode linkNode = itemNode.namedItem(QString::fromLatin1("link"));
        if (!linkNode.isNull())
            url = linkNode.toElement().text().simplifyWhiteSpace();
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
            description = descNode.toElement().text().simplifyWhiteSpace();
 
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
        new NewsArticle(this, title, description, url, thumbnail, mediaurl, enclosure);
    }
}

void NewsSite::parseAtom(QDomDocument domDoc)
{
    QDomNodeList entries = domDoc.elementsByTagName(QString::fromLatin1("entry"));

    QDomNode itemNode;
    QString title, description, url, thumbnail, mediaurl, enclosure, imageURL, enclosure_type;
    for (unsigned int i = 0; i < entries.count(); i++) {
        itemNode = entries.item(i);
        title    = itemNode.namedItem(QString::fromLatin1("title")).toElement().text().simplifyWhiteSpace();
        if (!title.isNull())
            ReplaceHtmlChar(title);

        QDomNode summNode = itemNode.namedItem(QString::fromLatin1("summary"));
        if (!summNode.isNull())
        {
            description = summNode.toElement().text().simplifyWhiteSpace();
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

        new NewsArticle(this, title, description, url, QString::null, QString::null, QString::null);
    }
}

void NewsSite::slotGotData(const QByteArray& data,
                           Q3NetworkOperation* op)
{
    if (op)
    {
        const char *charData = data.data();
        int len = data.count();

        int size = m_data.size();
        m_data.resize(size + len);
        memcpy(m_data.data()+size, charData, len);
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

