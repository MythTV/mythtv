/** -*- Mode: c++ -*-
 *  IPTVFeederWrapper
 *  Copyright (c) 2006 by Mickaël Remars
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef _IPTV_FEEDER_WRAPPER_H_
#define _IPTV_FEEDER_WRAPPER_H_

#include <vector>
using namespace std;

#include <qmutex.h>
#include <qstring.h>

class IPTVFeeder;
class TSDataListener;

/** \class IPTVFeederWrapper
 *  \brief Helper class for dealing with IPTVFeeder instances.
 */
class IPTVFeederWrapper
{
  public:
    IPTVFeederWrapper();
    ~IPTVFeederWrapper();

  public:
    bool IsOpen(void) const;

    bool Open(const QString &url);
    void Close(void);

    void Run(void);
    void Stop(void);

    void AddListener(TSDataListener*);
    void RemoveListener(TSDataListener*);
    
  private:
    bool InitFeeder(const QString &url);

  private:
    IPTVFeeder              *_feeder;
    QString                  _url;
    mutable QMutex           _lock; ///< Lock  used to coordinate threads
    vector<TSDataListener*>  _listeners;

  private:
    IPTVFeederWrapper &operator=(const IPTVFeederWrapper&);
    IPTVFeederWrapper(const IPTVFeederWrapper&);
};

#endif // _IPTV_FEEDER_WRAPPER_H_
