/** -*- Mode: c++ -*-
 *  FreeboxFeederWrapper
 *  Copyright (c) 2006 by Mickaël Remars
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef _FREEBOX_FEEDER_WRAPPER_H_
#define _FREEBOX_FEEDER_WRAPPER_H_

#include <vector>
using namespace std;

#include <qmutex.h>
#include <qstring.h>

class FreeboxFeeder;
class IPTVListener;

/* \brief Relays data from FreeboxFeeder to IPTVListener for FreeboxRecorder
 */
class FreeboxFeederWrapper
{
  public:
    FreeboxFeederWrapper();
    ~FreeboxFeederWrapper();

  public:
    bool Open(const QString &url);
    bool IsOpen(void) const;
    void Close(void);

    void Run(void);
    void Stop(void);

    void AddListener(IPTVListener*);
    void RemoveListener(IPTVListener*);
    
  private:
    bool InitFeeder(const QString &url);

  private:
    FreeboxFeeder         *_feeder;
    QString                _url;
    mutable QMutex         _lock; ///< Lock  used to coordinate threads
    vector<IPTVListener*>  _listeners;

  private:
    /// avoid default copy operator
    FreeboxFeederWrapper &operator=(const FreeboxFeederWrapper&);
    /// avoid default copy constructor
    FreeboxFeederWrapper(const FreeboxFeederWrapper&);
};

#endif // _FREEBOX_FEEDER_WRAPPER_H_
