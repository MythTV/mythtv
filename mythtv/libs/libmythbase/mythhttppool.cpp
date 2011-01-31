// -*- Mode: c++ -*-
// Copyright (c) 2008, Daniel Thor Kristjansson
// Licensed under the GPL v2 or later, see COPYING for details

// Myth headers
#include "mythhttppool.h"
#include "mythhttphandler.h"
#include "mythverbose.h"

#define LOC      QString("MythHttpPool: ")
#define LOC_WARN QString("MythHttpPool, Warning: ")
#define LOC_ERR  QString("MythHttpPool, Error: ")

MythHttpPool *MythHttpPool::singleton;

MythHttpPool::MythHttpPool(uint max_connections) :
    m_maxConnections(max_connections)
{
}

MythHttpPool::~MythHttpPool()
{
    while (!m_hostToHandler.empty())
    {
        HostToHandler::iterator it = m_hostToHandler.begin();
        MythHttpHandler *h = *it;
        m_hostToHandler.erase(it);
        h->deleteLater();
    }
}

void MythHttpPool::AddUrlRequest(const QUrl &url, MythHttpListener *listener)
{
    QMutexLocker locker(&m_lock);

    VERBOSE(VB_NETWORK, LOC + QString("AddUrlRequest(%1, 0x%2)")
            .arg(url.toString()).arg((quint64)listener,0,16));

    bool in_queue = m_urlToListener.find(url) != m_urlToListener.end();

    m_urlToListener.insert(url, listener);
    set<MythHttpListener*>::iterator lit = m_listeners.find(listener);
    if (lit == m_listeners.end())
        m_listeners.insert(listener);

    if (in_queue)
        return;

    HostToHandler::iterator it = m_hostToHandler.find(url.host());

    if (it != m_hostToHandler.end())
    {
        (*it)->AddUrlRequest(url);
    }
    else if ((uint)m_hostToHandler.size() < m_maxConnections)
    {
        MythHttpHandler *hh = new MythHttpHandler(this);
        m_hostToHandler[url.host()] = hh;
        hh->AddUrlRequest(url);
    }
    else
    {
        HostToHandler::iterator it = m_hostToHandler.begin();
        for (; it != m_hostToHandler.end(); ++it)
        {
            if (!(*it)->HasPendingRequests())
                break;
        }
        if (it != m_hostToHandler.end())
        {
            MythHttpHandler *hh = *it;
            m_hostToHandler.erase(it);
            m_hostToHandler[url.host()] = hh;
            hh->AddUrlRequest(url);
        }
        else
        {
            m_httpQueue.push_back(HttpRequest(url,listener));
        }
    }
}

void MythHttpPool::RemoveUrlRequest(const QUrl &url, MythHttpListener *listener)
{
    QMutexLocker locker(&m_lock);

    VERBOSE(VB_NETWORK, LOC + QString("RemoveUrlRequest(%1, 0x%2)")
            .arg(url.toString()).arg((quint64)listener,0,16));
}

void MythHttpPool::RemoveListener(MythHttpListener *listener)
{
    QMutexLocker locker(&m_lock);

    VERBOSE(VB_NETWORK, LOC + QString("RemoveListener(0x%1)")
            .arg((quint64)listener,0,16));
    set<MythHttpListener*>::iterator it = m_listeners.find(listener);
    if (it != m_listeners.end())
        m_listeners.erase(it);
}

void MythHttpPool::Update(QHttp::Error      error,
                          const QString    &error_str,
                          const QUrl        &url,
                          uint              http_status_id,
                          const QString    &http_status_str,
                          const QByteArray &data)
{
    QMutexLocker locker(&m_lock);

    UrlToListener::iterator it = m_urlToListener.find(url);
    for (; (it != m_urlToListener.end()) && (it.key() == url); ++it)
    {
        if (m_listeners.find(*it) != m_listeners.end())
        {
            (*it)->Update(error, error_str, url,
                          http_status_id, http_status_str,
                          data);
        }
    }

    m_urlToListener.remove(url);
}

void MythHttpPool::Done(const QString &host, MythHttpHandler *handler)
{
    QMutexLocker locker(&m_lock);

    VERBOSE(VB_NETWORK, LOC + QString("Done(%1, 0x%2)")
            .arg(host).arg((quint64)handler,0,16));

    HostToHandler::iterator it = m_hostToHandler.find(host);
    if (handler != *it)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + QString("Done(%1, 0x%2)")
                .arg(host).arg((quint64)handler,0,16));
        return;
    }

    while (!m_httpQueue.empty())
    {
        QUrl              url      = m_httpQueue.front().m_url;
        MythHttpListener *listener = m_httpQueue.front().m_listener;
        m_httpQueue.pop_front();

        if (m_listeners.find(listener) == m_listeners.end())
            continue;

        if (url.host() != host)
        {
            m_hostToHandler.erase(it);
            m_hostToHandler[url.host()] = handler;
        }
        handler->AddUrlRequest(url);
        break;
    }
}

MythHttpPool *MythHttpPool::GetSingleton()
{
    static QMutex lock;
    QMutexLocker locker(&lock);
    if (!singleton)
        singleton = new MythHttpPool();
    return singleton;
}
