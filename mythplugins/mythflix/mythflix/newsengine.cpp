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
#include <mythtv/mythdirs.h>

using namespace std;

NewsArticle::NewsArticle(NewsSite *parent, const QString& title,
                         const QString& desc, const QString& articleURL)
{
    parent->insertNewsArticle(this);
    m_title  = title;
    m_desc   = desc;
    m_parent = parent;
    m_articleURL = articleURL;
}

NewsArticle::~NewsArticle()
{

}

const QString& NewsArticle::title() const
{
    return m_title;
}

const QString& NewsArticle::description() const
{
    return m_desc;
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

    m_destDir  = GetConfDir();
    m_destDir += "/MythFlix";

    m_articleList.setAutoDelete(true);
    m_articleList.clear();

    m_data.resize(0);
    q3InitNetworkProtocols();
    m_urlOp = new Q3UrlOperator(m_url);

}

NewsSite::~NewsSite()
{
    m_urlOp->stop();
    delete m_urlOp;
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

    connect(m_urlOp, SIGNAL(data(const QByteArray&, Q3NetworkOperation*)),
            this, SLOT(slotGotData(const QByteArray&, Q3NetworkOperation*)));
    connect(m_urlOp, SIGNAL(finished(Q3NetworkOperation*)),
            this, SLOT(slotFinished(Q3NetworkOperation*)));

    m_state = NewsSite::Retrieving;
    m_data.resize(0);
    m_articleList.clear();
    m_urlOp->get(m_url);
}


void NewsSite::stop()
{
    m_urlOp->stop();

    disconnect(m_urlOp, SIGNAL(data(const QByteArray&, Q3NetworkOperation*)),
               this, SLOT(slotGotData(const QByteArray&, Q3NetworkOperation*)));
    disconnect(m_urlOp, SIGNAL(finished(Q3NetworkOperation*)),
               this, SLOT(slotFinished(Q3NetworkOperation*)));
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
        op->errorCode() == Q3NetworkProtocol::NoError) {

        QFile xmlFile(m_destDir+QString("/")+m_name);
        if (xmlFile.open( QIODevice::WriteOnly )) {
            QDataStream stream( &xmlFile );
            stream.writeRawBytes(m_data.data(), m_data.size());
            xmlFile.close();
            m_updated = QDateTime::currentDateTime();
            m_state = NewsSite::Success;
        }
        else {
            m_state = NewsSite::WriteFailed;
            VERBOSE(VB_IMPORTANT, "MythNews: NewsEngine: Write failed");
        }
    }
    else {
        m_state = NewsSite::RetrieveFailed;
    }

    stop();

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
        new NewsArticle(this, tr("Failed to retrieve news"), "", "");
        m_errorString += tr("No Cached News");
        return;
    }

    if (!xmlFile.open(QIODevice::ReadOnly)) {
        new NewsArticle(this, tr("Failed to retrieve news"), "", "");
        VERBOSE(VB_IMPORTANT, "MythNews: NewsEngine: failed to open xmlfile");
        return;
    }

    if (!domDoc.setContent(&xmlFile)) {
        new NewsArticle(this, tr("Failed to retrieve news"), "", "");
        VERBOSE(VB_IMPORTANT, "MythNews: NewsEngine: failed to set content from xmlfile");
        m_errorString += tr("Failed to read downloaded file");
        return;
    }


    if (m_state == RetrieveFailed)
        m_errorString += tr("Showing Cached News");

    QDomNode channelNode = domDoc.documentElement().namedItem(QString::fromLatin1("channel"));

    m_desc = channelNode.namedItem(QString::fromLatin1("description")).toElement().text();
    StripHtml(m_desc);

    QDomNodeList items = domDoc.elementsByTagName(QString::fromLatin1("item"));

    QDomNode itemNode;
    QString title, description, url;
    for (int i = 0; i < items.count(); i++) {
        itemNode = items.item(i);
        title    = itemNode.namedItem(QString::fromLatin1("title")).toElement().text().simplified();
        QDomNode descNode = itemNode.namedItem(QString::fromLatin1("description"));
        if (!descNode.isNull())
        {
            description = descNode.toElement().text().simplified();
            ReplaceHtmlChar(description);
            StripHtml(description);
        }
        else
            description = QString::null;
        QDomNode linkNode = itemNode.namedItem(QString::fromLatin1("link"));
        if (!linkNode.isNull())
            url = linkNode.toElement().text().simplified();
        else
            url = QString::null;

        new NewsArticle(this, title, description, url);
    }

    xmlFile.close();

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
}

void NewsSite::StripHtml(QString &text)
{
    text = text.simplified();
    // Replace paragraph and break HTML with newlines
    if( text.find(QRegExp("</(p|P)>")) )
    {
        text.replace( QRegExp("<(p|P)>"), "");
        text.replace( QRegExp("</(p|P)>"), "\n\n");
    }
    else
    {
        text.replace( QRegExp("<(p|P)>"), "\n\n");
        text.replace( QRegExp("</(p|P)>"), "");
    }
    text.replace( QRegExp("<(br|BR|)/>"), "\n");
    text.replace( QRegExp("<(br|BR|)>"), "\n");
    // These are done instead of simplified
    // because that function also strips out newlines
    // Replace tab characters with nothing
//     text.replace( QRegExp("\t"), "");
//     // Replace double space with single
//     text.replace( QRegExp("  "), "");
//     // Replace whitespace at beginning of lines with newline
//     text.replace( QRegExp("\n "), "\n");
    // Remove any remaining HTML tags
    QRegExp removeHTML(QRegExp("</?.+>"));
    removeHTML.setMinimal(true);
    text.remove((const QRegExp&) removeHTML);
    text = text.trimmed();
}

