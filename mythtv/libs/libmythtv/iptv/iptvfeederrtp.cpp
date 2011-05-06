/** -*- Mode: c++ -*-
 *  IPTVFeederRtp
 *  Copyright (c) 2006 by Mike Mironov & Mickaël Remars
 *  Distributed as part of MythTV under GPL v2 and later.
 */
#include <algorithm>

#include "iptvfeederrtp.h"

// Qt headers
#include <QUrl>

// Live555 headers
#include <BasicUsageEnvironment.hh>
#include <Groupsock.hh>
#include <GroupsockHelper.hh>
#include <SimpleRTPSource.hh>
#include <TunnelEncaps.hh>

// MythTV headers
#include "iptvmediasink.h"
#include "mythcontext.h"
#include "mythverbose.h"
#include "tspacket.h"

#define LOC QString("IPTVFeedRTP: ")
#define LOC_ERR QString("IPTVFeedRTP, Error: ")

IPTVFeederRTP::IPTVFeederRTP() :
    _source(NULL),
    _sink(NULL)
{
    VERBOSE(VB_RECORD, LOC + "ctor -- success");
}

IPTVFeederRTP::~IPTVFeederRTP()
{
    VERBOSE(VB_RECORD, LOC + "dtor -- begin");
    Close();
    VERBOSE(VB_RECORD, LOC + "dtor -- end");
}

bool IPTVFeederRTP::IsRTP(const QString &url)
{
    return url.startsWith("rtp://", Qt::CaseInsensitive);
}

bool IPTVFeederRTP::Open(const QString &url)
{
    VERBOSE(VB_RECORD, LOC + QString("Open(%1) -- begin").arg(url));

    QMutexLocker locker(&_lock);

    if (_source)
    {
        VERBOSE(VB_RECORD, LOC + "Open() -- end 1");
        return true;
    }

    QUrl parse(url);
    if (!parse.isValid() || parse.host().isEmpty() || (-1 == parse.port()))
    {
        VERBOSE(VB_RECORD, LOC + "Open() -- end 2");
        return false;
    }

    struct in_addr addr;
    QByteArray host = parse.host().toLatin1();
    addr.s_addr = our_inet_addr(host.constData());

    // Begin by setting up our usage environment:
    if (!InitEnv())
        return false;

    Groupsock *socket = new Groupsock(*_live_env, addr, parse.port(), 0);
    if (!socket)
    {
        VERBOSE(VB_IMPORTANT, LOC + "Failed to create Live RTP Socket.");
        FreeEnv();
        return false;
    }

    _source = SimpleRTPSource::createNew(*_live_env, socket, 33, 90000,
                                         "video/MP2T", 0, False);
    if (!_source)
    {
        VERBOSE(VB_IMPORTANT, LOC + "Failed to create Live RTP Source.");

        if (socket)
            delete socket;

        FreeEnv();
        return false;
    }

    _sink = IPTVMediaSink::CreateNew(*_live_env, TSPacket::kSize * 128*1024);
    if (!_sink)
    {
        VERBOSE(VB_IMPORTANT,
                QString("IPTV # Failed to create sink: %1")
                .arg(_live_env->getResultMsg()));

        Medium::close(_source);
        _source = NULL;
        if (socket)
            delete socket;
        FreeEnv();

        return false;
    }

    _sink->startPlaying(*_source, NULL, NULL);
    vector<TSDataListener*>::iterator it = _listeners.begin();
    for (; it != _listeners.end(); ++it)
        _sink->AddListener(*it);

    VERBOSE(VB_RECORD, LOC + "Open() -- end");

    return true;
}

void IPTVFeederRTP::Close(void)
{
    VERBOSE(VB_RECORD, LOC + "Close() -- begin");
    Stop();

    QMutexLocker locker(&_lock);

    if (_sink)
    {
        Medium::close(_sink);
        _sink = NULL;
    }

    if (_source)
    {
        Groupsock *socket = _source->RTPgs();
        Medium::close(_source);
        _source = NULL;
        if (socket)
            delete socket;
    }

    FreeEnv();

    VERBOSE(VB_RECORD, LOC + "Close() -- end");
}

void IPTVFeederRTP::AddListener(TSDataListener *item)
{
    VERBOSE(VB_RECORD, LOC + "AddListener("<<item<<") -- begin");
    if (!item)
    {
        VERBOSE(VB_RECORD, LOC + "AddListener("<<item<<") -- end");
        return;
    }

    // avoid duplicates
    RemoveListener(item);

    // add to local list
    QMutexLocker locker(&_lock);
    _listeners.push_back(item);

    if (_sink)
        _sink->AddListener(item);

    VERBOSE(VB_RECORD, LOC + "AddListener("<<item<<") -- end");
}

void IPTVFeederRTP::RemoveListener(TSDataListener *item)
{
    VERBOSE(VB_RECORD, LOC + "RemoveListener("<<item<<") -- begin");
    QMutexLocker locker(&_lock);
    vector<TSDataListener*>::iterator it =
        find(_listeners.begin(), _listeners.end(), item);

    if (it == _listeners.end())
    {
        VERBOSE(VB_RECORD, LOC + "RemoveListener("<<item<<") -- end 1");
        return;
    }

    // remove from local list..
    *it = *_listeners.rbegin();
    _listeners.resize(_listeners.size() - 1);

    if (_sink)
        _sink->RemoveListener(item);

    VERBOSE(VB_RECORD, LOC + "RemoveListener("<<item<<") -- end 2");
}
