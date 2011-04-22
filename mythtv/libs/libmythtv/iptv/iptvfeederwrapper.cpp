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
#include "mythcontext.h"
#include "mythverbose.h"

#define LOC QString("IPTVFeed: ")
#define LOC_ERR QString("IPTVFeed, Error: ")


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
    VERBOSE(VB_RECORD, LOC + "Init() -- begin");
    QMutexLocker locker(&_lock);

    if (_feeder && _feeder->CanHandle(url))
    {
        _url = url;
        _url.detach();
        VERBOSE(VB_RECORD, LOC + "Init() -- end 0");

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
    else
    {
        VERBOSE(VB_RECORD, LOC_ERR +
                QString("Init() -- unhandled url (%1)").arg(url));

        return false;
    }

    if (_feeder)
        delete _feeder;

    _feeder = tmp_feeder;

    _url = url;
    _url.detach();

    VERBOSE(VB_RECORD, LOC + "Init() -- adding listeners");

    vector<TSDataListener*>::iterator it = _listeners.begin();
    for (; it != _listeners.end(); ++it)
        _feeder->AddListener(*it);

    VERBOSE(VB_RECORD, LOC + "Init() -- end 1");

    return true;
}


bool IPTVFeederWrapper::Open(const QString& url)
{
    VERBOSE(VB_RECORD, LOC + "Open() -- begin");

    bool result = InitFeeder(url) && _feeder->Open(_url);

    VERBOSE(VB_RECORD, LOC + "Open() -- end");

    return result;
}

bool IPTVFeederWrapper::IsOpen(void) const
{
    VERBOSE(VB_RECORD, LOC + "IsOpen() -- begin");

    bool result = _feeder && _feeder->IsOpen();

    VERBOSE(VB_RECORD, LOC + "IsOpen() -- end");

    return result;
}

void IPTVFeederWrapper::Close(void)
{
    VERBOSE(VB_RECORD, LOC + "Close() -- begin");

    if (_feeder)
        _feeder->Close();

    VERBOSE(VB_RECORD, LOC + "Close() -- end");
}

void IPTVFeederWrapper::Run(void)
{
    VERBOSE(VB_RECORD, LOC + "Run() -- begin");

    if (_feeder)
        _feeder->Run();

    VERBOSE(VB_RECORD, LOC + "Run() -- end");
}

void IPTVFeederWrapper::Stop(void)
{
    VERBOSE(VB_RECORD, LOC + "Stop() -- begin");

    if (_feeder)
        _feeder->Stop();

    VERBOSE(VB_RECORD, LOC + "Stop() -- end");
}

void IPTVFeederWrapper::AddListener(TSDataListener *item)
{
    VERBOSE(VB_RECORD, LOC + QString("AddListener(0x%1) -- begin")
                       .arg((uint64_t)item,0,16));

    if (!item)
    {
        VERBOSE(VB_RECORD, LOC + QString("AddListener(0x%1) -- end 0")
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

    VERBOSE(VB_RECORD, LOC + QString("AddListener(0x%1) -- end 1")
                       .arg((uint64_t)item,0,16));
}

void IPTVFeederWrapper::RemoveListener(TSDataListener *item)
{
    VERBOSE(VB_RECORD, LOC + QString("RemoveListener(0x%1) -- begin")
                       .arg((uint64_t)item,0,16));

    QMutexLocker locker(&_lock);
    vector<TSDataListener*>::iterator it =
        find(_listeners.begin(), _listeners.end(), item);

    if (it == _listeners.end())
    {
        VERBOSE(VB_RECORD, LOC + QString("RemoveListener(0x%1) -- end "
                           "(not found)")
                           .arg((uint64_t)item,0,16));

        return;
    }

    // remove from local list..
    *it = *_listeners.rbegin();
    _listeners.resize(_listeners.size() - 1);
    if (_feeder)
        _feeder->RemoveListener(item);

    VERBOSE(VB_RECORD, LOC + QString("RemoveListener(0x%1) -- end (ok, "
                       "removed)")
                       .arg((uint64_t)item,0,16));
}
