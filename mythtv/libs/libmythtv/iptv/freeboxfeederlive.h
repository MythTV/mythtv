/** -*- Mode: c++ -*-
 *  FreeboxFeederLive -- base class for livemedia based FreeboxFeeders
 *  Copyright (c) 2006 by Laurent Arnal, Benjamin Lerman & Mickaël Remars
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef _FREEBOX_FEEDER_LIVE_H_
#define _FREEBOX_FEEDER_LIVE_H_

// C++ headers
#include <vector>
using namespace std;

// Qt headers
#include <qwaitcondition.h>
#include <qmutex.h>

// Mythtv headers
#include "freeboxfeeder.h"

class QString;
class IPTVListener;
class UsageEnvironment;


class FreeboxFeederLive : public FreeboxFeeder
{
  public:
    FreeboxFeederLive();
    virtual ~FreeboxFeederLive();

    void Run(void);
    void Stop(void);
    
  protected:
    bool InitEnv(void);
    void FreeEnv(void);

  protected: // TODO make them private
    char                _abort;
    bool                _running;
    UsageEnvironment   *_live_env;
    QWaitCondition      _cond;       ///< Condition  used to coordinate threads
    mutable QMutex      _lock;       ///< Lock  used to coordinate threads
    vector<IPTVListener*> _listeners;

  private:
    /// avoid default copy operator
    FreeboxFeederLive &operator=(const FreeboxFeederLive&);
    /// avoid default copy operator
    FreeboxFeederLive(const FreeboxFeederLive&);
};

#endif // _FREEBOX_FEEDER_LIVE_H_
