/* ============================================================
 * File  : newsengine.h
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

#ifndef NEWSENGINE_H
#define NEWSENGINE_H

#include <qstring.h>
#include <qptrlist.h>
#include <qobject.h>
#include <qdatetime.h>
#include <qcstring.h>

class QUrlOperator;
class QNetworkOperation;
class NewsSite;

// -------------------------------------------------------

class NewsArticle
{
public:

    typedef QPtrList<NewsArticle> List;

    NewsArticle(NewsSite *parent, const QString& title,
                const QString& desc, const QString& artURL);
    ~NewsArticle();

    const QString& title() const;
    const QString& description() const;
    const QString& articleURL() const { return m_articleURL; }

private:

    QString   m_title;
    QString   m_desc;
    NewsSite *m_parent;
    QString m_articleURL;
};

// -------------------------------------------------------

class NewsSite : public QObject
{
    Q_OBJECT

public:

    enum State {
        Retrieving = 0,
        RetrieveFailed,
        WriteFailed,
        Success
    };

    typedef QPtrList<NewsSite> List;

    NewsSite(const QString& name, const QString& url,
             const QDateTime& updated);
    ~NewsSite();

    const QString&   url()  const;
    const QString&   name() const;
    QString          description() const;
    const QDateTime& lastUpdated() const;
    unsigned int timeSinceLastUpdate() const; // in minutes

    void insertNewsArticle(NewsArticle* item);
    void clearNewsArticles();
    NewsArticle::List& articleList();

    void retrieve();
    void stop();
    void process();

    bool     successful() const;
    QString  errorMsg() const;

private:

    QString    m_name;
    QString    m_url;
    QString    m_desc;
    QDateTime  m_updated;
    QString    m_destDir;
    QByteArray m_data;
    State      m_state;
    QString    m_errorString;

    NewsArticle::List m_articleList;
    QUrlOperator*     m_urlOp;

signals:

    void finished(NewsSite* item);

private slots:

    void slotFinished(QNetworkOperation*);
    void slotGotData(const QByteArray& data,
                     QNetworkOperation* op);
};

#endif /* NEWSENGINE_H */
