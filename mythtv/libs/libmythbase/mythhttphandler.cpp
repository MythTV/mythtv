// -*- Mode: c++ -*-
// Copyright (c) 2008, Daniel Thor Kristjansson
// Licensed under the GPL v2 or later, see COPYING for details

// Qt headers
#include <QHttp>

// Myth headers
#include "mythhttphandler.h"
#include "mythhttppool.h"
#include "mythlogging.h"

#define LOC      QString("MythHttpHandler: ")

const uint MythHttpHandler::kMaxRedirectCount = 32;

MythHttpHandler::MythHttpHandler(MythHttpPool *pool) :
    m_cur_status_id(0), m_cur_get_id(0), m_cur_redirect_cnt(0),
    m_pool(pool), m_qhttp(new QHttp())
{
    connect(m_qhttp, SIGNAL(done(bool)),
            this,    SLOT(Done(bool)));
    connect(m_qhttp, SIGNAL(requestFinished(int,bool)),
            this,    SLOT(RequestFinished(int,bool)));
    connect(m_qhttp, SIGNAL(requestStarted(int)),
            this,    SLOT(RequestStarted(int)));
    connect(m_qhttp, SIGNAL(stateChanged(int)),
            this,    SLOT(StateChanged(int)));
    connect(m_qhttp, SIGNAL(responseHeaderReceived(const QHttpResponseHeader&)),
            this,    SLOT(ResponseHeaderReceived(const QHttpResponseHeader&)));
}

void MythHttpHandler::TeardownAll(void)
{
    QMutexLocker locker(&m_lock);

    if (m_qhttp)
    {
        m_qhttp->abort();
        m_qhttp->disconnect();
        m_qhttp->deleteLater();
    }
    m_pool = NULL;
    m_qhttp = NULL;
}

bool MythHttpHandler::HasPendingRequests(void) const
{
    QMutexLocker locker(&m_lock);
    return (m_qhttp->hasPendingRequests() ||
            m_qhttp->currentRequest().isValid() ||
            !m_urls.empty());
}

void MythHttpHandler::AddUrlRequest(const QUrl &url)
{
    QMutexLocker locker(&m_lock);

    LOG(VB_NETWORK, LOG_DEBUG, LOC +
        QString("AddUrlRequest(%1)").arg(url.toString()));

    if (!m_qhttp->hasPendingRequests() && !m_qhttp->currentRequest().isValid())
        Get(url);
    else
        m_urls.push_back(url);
}

void MythHttpHandler::Get(const QUrl &url)
{
    LOG(VB_NETWORK, LOG_DEBUG, LOC + QString("Get(%1)").arg(url.toString()));

    m_cur_url          = url;
    m_cur_status_id    = 0;
    m_cur_status_str   = QString::null;
    m_cur_redirect_cnt = 0;

    QHttp::ConnectionMode mode = 
        m_cur_url.scheme().toLower() == "https" ? QHttp::ConnectionModeHttps : 
                                                  QHttp::ConnectionModeHttp;
    m_qhttp->setHost(m_cur_url.host(), mode, 
                     m_cur_url.port() == -1 ? 0 : m_cur_url.port());
    
    if (!m_cur_url.userName().isEmpty())
        m_qhttp->setUser(m_cur_url.userName(), m_cur_url.password());

    QByteArray path = QUrl::toPercentEncoding(m_cur_url.path(), "!$&'()*+,;=:@/");
    if (path.isEmpty())
        path = "/";
    
    if (m_cur_url.hasQuery()) 
        path += "?" + m_cur_url.encodedQuery();  

    m_cur_get_id = m_qhttp->get(path);
}

void MythHttpHandler::RemoveUrlRequest(const QUrl &url)
{
    QMutexLocker locker(&m_lock);

    UrlQueue urls = m_urls;
    m_urls.clear();
    while (!urls.empty())
    {
        QUrl item = urls.front();
        urls.pop_front();
        if (item != url)
            m_urls.push_back(item);
    }

    if (url == m_cur_url)
    {
        m_cur_url          = QUrl();
        m_cur_status_id    = 0;
        m_cur_status_str   = QString::null;
        m_cur_redirect_cnt = 0;
        m_cur_get_id       = 0;
        m_qhttp->abort();
    }
}

void MythHttpHandler::Done(bool error)
{
    QMutexLocker locker(&m_lock);

    LOG(VB_NETWORK, LOG_DEBUG, LOC + QString("Done(%1) url: %2")
            .arg(error).arg(m_cur_url.toString()));

    if (m_pool)
        m_pool->Done(m_cur_url.host(), this);
}

void MythHttpHandler::ResponseHeaderReceived(const QHttpResponseHeader &resp)
{
    QMutexLocker locker(&m_lock);

    LOG(VB_NETWORK, LOG_DEBUG, LOC +
        QString("ResponseHeaderReceived(%1,%2) url: %3")
            .arg(resp.statusCode()).arg(resp.reasonPhrase())
            .arg(m_cur_url.toString()));
    m_cur_status_id  = resp.statusCode();
    m_cur_status_str = resp.reasonPhrase();
}

static QString extract_url(const QString &text)
{
    QString url = text;
    static QMutex lock;
    static QRegExp aTagExp("<a.*href.*=.*>", Qt::CaseInsensitive);
    static QRegExp httpEndExp("\"|\\s|\'");

    QMutexLocker locker(&lock);

    int a_tag = url.indexOf(aTagExp);
    if (a_tag >= 0)
    {
        url = url.mid(a_tag+1);
        url = url.left(url.indexOf(">"));
        url = url.mid(url.indexOf("href", Qt::CaseInsensitive) + 4);
        url = url.mid(url.indexOf("=") + 1);
        url = url.trimmed();
        if ((url.size()>=2) && url[0] == QChar('"'))
        {
            url = url.mid(1);
            if (url[url.length()-1] == QChar('"'))
                url = url.left(url.length()-1);
        }
        if ((url.size()>=2) && url[0] == QChar('\''))
        {
            url = url.mid(1);
            if (url[url.length()-1] == QChar('\''))
                url = url.left(url.length()-1);
        }
        return url;
    }

    int http = url.indexOf("http:");
    if (http >= 0)
    {
        url = url.mid(http);
        int end_http = url.indexOf(httpEndExp);
        url = url.left(end_http);
        return url;
    }

    return QString::null;
}

void MythHttpHandler::RequestFinished(int id, bool error)
{
    QMutexLocker locker(&m_lock);

    LOG(VB_NETWORK, LOG_DEBUG, LOC + QString("RequestFinished(%1,%2) url: %3")
            .arg(id).arg(error).arg(m_cur_url.toString()));
    if (error && m_pool)
    {
        m_pool->Update(m_qhttp->error(), m_qhttp->errorString(),
                       m_cur_url,
                       m_cur_status_id, m_cur_status_str, QByteArray());
    }
    else if ((id == m_cur_get_id) && m_pool)
    {
        if ((307 == m_cur_status_id) || // temporary move..
            (303 == m_cur_status_id) || // move.. MUST use get
            (302 == m_cur_status_id) || // temporary move..
            (301 == m_cur_status_id))   // permanent move..
        {
            m_cur_status_id = 0;
            QString urlStr = extract_url(QString(m_qhttp->readAll()));
            if (!urlStr.isEmpty() && m_cur_redirect_cnt < kMaxRedirectCount)
            {
                m_cur_redirect_cnt++;
                QUrl url = QUrl(urlStr);
                m_qhttp->setHost(url.host());
                QString path = url.path().isEmpty() ? "/" : url.path();
                m_cur_get_id = m_qhttp->get(path);
                return;
            }
        }

        m_pool->Update(QHttp::NoError, QString::null,
                       m_cur_url,
                       m_cur_status_id, m_cur_status_str,
                       m_qhttp->readAll());
    }
    else
        return;

    if (!m_urls.empty())
    {
        Get(m_urls.front());
        m_urls.pop_front();
    }
}

void MythHttpHandler::RequestStarted(int id)
{
    QMutexLocker locker(&m_lock);

    LOG(VB_NETWORK, LOG_DEBUG, LOC + QString("RequestStarted(%1) url: %2")
            .arg(id).arg(m_cur_url.toString()));
}

void MythHttpHandler::StateChanged(int state)
{
    QMutexLocker locker(&m_lock);

    LOG(VB_NETWORK, LOG_DEBUG, LOC + QString("StateChanged(%1) url: %2")
            .arg(state).arg(m_cur_url.toString()));
}

