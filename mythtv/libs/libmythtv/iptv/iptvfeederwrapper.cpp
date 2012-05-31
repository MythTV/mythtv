/** -*- Mode: c++ -*-
 *  IPTVFeederWrapper
 *  Copyright (c) 2006 by Mickaël Remars
 *  Distributed as part of MythTV under GPL v2 and later.
 */

// C++ headers
#include <algorithm>
using namespace std;

// MythTV headers
#include "iptvfeederwrapper.h"

#include "iptvfeeder.h"
#include "iptvfeederrtsp.h"
#include "iptvfeederudp.h"
#include "iptvfeederrtp.h"
#include "iptvfeederfile.h"
#include "iptvfeederhls.h"
#include "mythcontext.h"
#include "mythlogging.h"

#define LOC QString("IPTVFeed: ")


IPTVFeederWrapper::IPTVFeederWrapper() :
    _feeder(NULL), _url(QString::null), _lock()
{
}

IPTVFeederWrapper::~IPTVFeederWrapper()
{
    if (_feeder)
    {
        _feeder->Stop();
        _feeder->Close();
        delete _feeder;
        _feeder = NULL;
    }
}

bool IPTVFeederWrapper::InitFeeder(const QString &url)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "Init() -- begin");
    QMutexLocker locker(&_lock);

    if (_feeder && _feeder->CanHandle(url))
    {
        _url = url;
        _url.detach();
        LOG(VB_RECORD, LOG_INFO, LOC + "Init() -- end 0");

        return true;
    }

    IPTVFeeder *tmp_feeder = NULL;
    if (IPTVFeederRTSP::IsRTSP(url))
    {
        tmp_feeder = new IPTVFeederRTSP();
    }
    else if (IPTVFeederUDP::IsUDP(url))
    {
        tmp_feeder = new IPTVFeederUDP();
    }
    else if (IPTVFeederRTP::IsRTP(url))
    {
        tmp_feeder = new IPTVFeederRTP();
    }
    else if (IPTVFeederFile::IsFile(url))
    {
        tmp_feeder = new IPTVFeederFile();
    }
    else if (IPTVFeederHLS::IsHLS(url))
    {
        tmp_feeder = new IPTVFeederHLS();
    }
    else
    {
        LOG(VB_RECORD, LOG_ERR, LOC +
            QString("Init() -- unhandled url (%1)").arg(url));

        return false;
    }

    if (_feeder)
        delete _feeder;

    _feeder = tmp_feeder;

    _url = url;
    _url.detach();

    LOG(VB_RECORD, LOG_INFO, LOC + "Init() -- adding listeners");

    vector<TSDataListener*>::iterator it = _listeners.begin();
    for (; it != _listeners.end(); ++it)
        _feeder->AddListener(*it);

    LOG(VB_RECORD, LOG_INFO, LOC + "Init() -- end 1");

    return true;
}


bool IPTVFeederWrapper::Open(const QString& url)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "Open() -- begin");

    bool result = InitFeeder(url) && _feeder->Open(_url);

    LOG(VB_RECORD, LOG_INFO, LOC + "Open() -- end");

    return result;
}

bool IPTVFeederWrapper::IsOpen(void) const
{
    LOG(VB_RECORD, LOG_INFO, LOC + "IsOpen() -- begin");

    bool result = _feeder && _feeder->IsOpen();

    LOG(VB_RECORD, LOG_INFO, LOC + "IsOpen() -- end");

    return result;
}

void IPTVFeederWrapper::Close(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "Close() -- begin");

    if (_feeder)
        _feeder->Close();

    LOG(VB_RECORD, LOG_INFO, LOC + "Close() -- end");
}

void IPTVFeederWrapper::Run(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "Run() -- begin");

    if (_feeder)
        _feeder->Run();

    LOG(VB_RECORD, LOG_INFO, LOC + "Run() -- end");
}

void IPTVFeederWrapper::Stop(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "Stop() -- begin");

    if (_feeder)
        _feeder->Stop();

    LOG(VB_RECORD, LOG_INFO, LOC + "Stop() -- end");
}

void IPTVFeederWrapper::AddListener(TSDataListener *item)
{
    LOG(VB_RECORD, LOG_INFO, LOC + QString("AddListener(0x%1) -- begin")
                       .arg((uint64_t)item,0,16));

    if (!item)
    {
        LOG(VB_RECORD, LOG_INFO, LOC + QString("AddListener(0x%1) -- end 0")
                           .arg((uint64_t)item,0,16));
        return;
    }

    QMutexLocker locker(&_lock);
    vector<TSDataListener*>::iterator it =
        find(_listeners.begin(), _listeners.end(), item);
    if (it == _listeners.end()) // avoid duplicates
    {
        _listeners.push_back(item);
        if (_feeder)
            _feeder->AddListener(item);
    }

    LOG(VB_RECORD, LOG_INFO, LOC + QString("AddListener(0x%1) -- end 1")
                       .arg((uint64_t)item,0,16));
}

void IPTVFeederWrapper::RemoveListener(TSDataListener *item)
{
    LOG(VB_RECORD, LOG_INFO, LOC + QString("RemoveListener(0x%1) -- begin")
                       .arg((uint64_t)item,0,16));

    QMutexLocker locker(&_lock);
    vector<TSDataListener*>::iterator it =
        find(_listeners.begin(), _listeners.end(), item);

    if (it == _listeners.end())
    {
        LOG(VB_RECORD, LOG_ERR, LOC + QString("RemoveListener(0x%1) -- end "
                                              "(not found)")
                           .arg((uint64_t)item,0,16));

        return;
    }

    // remove from local list..
    *it = *_listeners.rbegin();
    _listeners.resize(_listeners.size() - 1);
    if (_feeder)
        _feeder->RemoveListener(item);

    LOG(VB_RECORD, LOG_INFO, LOC + QString("RemoveListener(0x%1) -- end (ok, "
                                           "removed)")
                       .arg((uint64_t)item,0,16));
}
