/** -*- Mode: c++ -*-
 *  FreeboxFeederWrapper
 *  Copyright (c) 2006 by Mickaël Remars
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#include "freeboxfeederwrapper.h"

#include "freeboxfeeder.h"
#include "freeboxfeederrtsp.h"
#include "freeboxfeederudp.h"
#include "freeboxfeederfile.h"
#include "mythcontext.h"

#define LOC QString("FBFeed: ")
#define LOC_ERR QString("FBFeed, Error: ")


FreeboxFeederWrapper::FreeboxFeederWrapper() :
    _feeder(NULL), _url(QString::null), _lock(false)
{
}

FreeboxFeederWrapper::~FreeboxFeederWrapper()
{
    if (_feeder)
    {
        _feeder->Stop();
        _feeder->Close();
        delete _feeder;
        _feeder = NULL;
    }
}

bool FreeboxFeederWrapper::InitFeeder(const QString &url)
{
    VERBOSE(VB_RECORD, LOC + "Init() -- begin");
    QMutexLocker locker(&_lock);

    if (_feeder && _feeder->CanHandle(url))
    {
        _url = QDeepCopy<QString>(url);
        VERBOSE(VB_RECORD, LOC + "Init() -- end 0");

        return true;
    }

    FreeboxFeeder *tmp_feeder = NULL;
    if (url.startsWith("rtsp://", false))
    {
        tmp_feeder = new FreeboxFeederRtsp();
    }
    else if (url.startsWith("udp://", false))
    {
        tmp_feeder = new FreeboxFeederUdp();
    }
    else if (url.startsWith("file:", false))
    {
        tmp_feeder = new FreeboxFeederFile();
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

    _url = QDeepCopy<QString>(url);

    VERBOSE(VB_RECORD, LOC + "Init() -- adding listeners");

    std::vector<IPTVListener*>::iterator it = _listeners.begin();
    for (; it != _listeners.end(); ++it)
        _feeder->AddListener(*it);

    VERBOSE(VB_RECORD, LOC + "Init() -- end 1");

    return true;
}


bool FreeboxFeederWrapper::Open(const QString& url)
{
    VERBOSE(VB_RECORD, LOC + "Open() -- begin");

    bool result = InitFeeder(url) && _feeder->Open(_url);

    VERBOSE(VB_RECORD, LOC + "Open() -- end");

    return result;
}

bool FreeboxFeederWrapper::IsOpen(void) const
{
    VERBOSE(VB_RECORD, LOC + "IsOpen() -- begin");

    bool result = _feeder && _feeder->IsOpen();

    VERBOSE(VB_RECORD, LOC + "IsOpen() -- end");

    return result;
}

void FreeboxFeederWrapper::Close(void)
{
    VERBOSE(VB_RECORD, LOC + "Close() -- begin");

    if (_feeder)
        _feeder->Close();

    VERBOSE(VB_RECORD, LOC + "Close() -- end");
}

void FreeboxFeederWrapper::Run(void)
{
    VERBOSE(VB_RECORD, LOC + "Run() -- begin");

    if (_feeder)
        _feeder->Run();

    VERBOSE(VB_RECORD, LOC + "Run() -- end");
}

void FreeboxFeederWrapper::Stop(void)
{
    VERBOSE(VB_RECORD, LOC + "Stop() -- begin");

    if (_feeder)
        _feeder->Stop();

    VERBOSE(VB_RECORD, LOC + "Stop() -- end");
}

void FreeboxFeederWrapper::AddListener(IPTVListener *item)
{
    VERBOSE(VB_RECORD, LOC + "AddListener("<<item<<") -- begin");

    if (!item)
    {
        VERBOSE(VB_RECORD, LOC + "AddListener("<<item<<") -- end 0");
        return;
    }

    QMutexLocker locker(&_lock);
    std::vector<IPTVListener*>::iterator it =
        find(_listeners.begin(), _listeners.end(), item);
    if (it == _listeners.end()) // avoid duplicates
    {
        _listeners.push_back(item);
        if (_feeder)
            _feeder->AddListener(item);
    }

    VERBOSE(VB_RECORD, LOC + "AddListener("<<item<<") -- end 1");
}

void FreeboxFeederWrapper::RemoveListener(IPTVListener *item)
{
    VERBOSE(VB_RECORD, LOC + "RemoveListener("<<item<<") -- begin");

    QMutexLocker locker(&_lock);
    std::vector<IPTVListener*>::iterator it =
        find(_listeners.begin(), _listeners.end(), item);

    if (it == _listeners.end())
    {
        VERBOSE(VB_RECORD, LOC + "RemoveListener("<<item
                <<") -- end (not found)");

        return;
    }

    // remove from local list..
    *it = *_listeners.rbegin();
    _listeners.resize(_listeners.size() - 1);
    if (_feeder)
        _feeder->RemoveListener(item);

    VERBOSE(VB_RECORD, LOC + "RemoveListener("<<item
            <<") -- end (ok, removed)");
}
