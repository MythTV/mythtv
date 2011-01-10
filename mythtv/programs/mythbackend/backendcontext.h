#ifndef _BACKEND_CONTEXT_H_
#define _BACKEND_CONTEXT_H_

#include <QString>
#include <QMap>

class EncoderLink;
class AutoExpire;
class Scheduler;
class JobQueue;
class HouseKeeper;
class MediaServer;

extern QMap<int, EncoderLink *> tvList;
extern AutoExpire  *expirer;
extern Scheduler   *sched;
extern JobQueue    *jobqueue;
extern HouseKeeper *housekeeping;
extern MediaServer *g_pUPnp;
extern QString      pidfile;
extern QString      logfile;

class BackendContext
{
    
};

#endif // _BACKEND_CONTEXT_H_
