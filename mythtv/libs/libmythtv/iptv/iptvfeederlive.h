/** -*- Mode: c++ -*-
 *  IPTVFeederLive -- base class for livemedia based IPTVFeeders
 *  Copyright (c) 2006 by Laurent Arnal, Benjamin Lerman & Mickaël Remars
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef _IPTV_FEEDER_LIVE_H_
#define _IPTV_FEEDER_LIVE_H_

// C++ headers
#include <vector>
using namespace std;

// Qt headers
#include <qwaitcondition.h>
#include <qmutex.h>

// Mythtv headers
#include "iptvfeeder.h"

class QString;
class TSDataListener;
class UsageEnvironment;


class IPTVFeederLive : public IPTVFeeder
{
  public:
    IPTVFeederLive();
    virtual ~IPTVFeederLive();

    void Run(void);
    void Stop(void);
    
  protected:
    bool InitEnv(void);
    void FreeEnv(void);

  private:
    IPTVFeederLive &operator=(const IPTVFeederLive&);
    IPTVFeederLive(const IPTVFeederLive&);

  protected:
    UsageEnvironment       *_live_env;
    mutable QMutex          _lock;
    vector<TSDataListener*> _listeners;

  private:
    char                    _abort;
    bool                    _running;
    QWaitCondition          _cond;
};

#endif // _IPTV_FEEDER_LIVE_H_
