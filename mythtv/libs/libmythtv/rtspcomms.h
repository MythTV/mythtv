/** -*- Mode: c++ -*-
 *  RTSPComms
 *  Copyright (c) 2006 by Laurent Arnal, Benjamin Lerman & Mickaël Remars
 *  Distributed as part of MythTV under GPL v2 and later.
 */

// C++ headers
#include <vector>
using namespace std;

// Qt headers
#include <qwaitcondition.h>
#include <qmutex.h>

// MythTV headers
#include "freeboxchannel.h"

class UsageEnvironment;
class RTSPClient;
class MediaSession;
class RTSPListener;

class RTSPComms 
{
  public:
    RTSPComms();
    virtual ~RTSPComms();

    bool Init(void);
    void Deinit(void);

    bool Open(const QString &url);
    bool IsOpen(void) const { return _session; }
    void Close(void);

    void Run(void);
    void Stop(void);

    void AddListener(RTSPListener*);
    void RemoveListener(RTSPListener*);

  private:

    char                _abort;
    bool                _running;
    UsageEnvironment   *_live_env;
    RTSPClient         *_rtsp_client;
    MediaSession       *_session;
    vector<RTSPListener*> _listeners;
    QWaitCondition      _cond;       ///< Condition  used to coordinate threads
    mutable QMutex      _lock;       ///< Lock  used to coordinate threads
};
