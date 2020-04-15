#ifndef BACKEND_CONTEXT_H
#define BACKEND_CONTEXT_H

#include <QString>
#include <QMap>
#include <QList>
#include <QHostAddress>
#include "mythsystemevent.h"

class EncoderLink;
class AutoExpire;
class Scheduler;
class JobQueue;
class HouseKeeper;
class MediaServer;
class BackendContext;

extern QMap<int, EncoderLink *> tvList;
extern AutoExpire  *expirer;
extern JobQueue    *jobqueue;
extern HouseKeeper *housekeeping;
extern MediaServer *g_pUPnp;
extern BackendContext *gBackendContext;
extern QString      pidfile;
extern QString      logfile;
extern MythSystemEventHandler *sysEventHandler;

class Frontend
{
  public:
    Frontend() = default;
   ~Frontend() = default;

    QString      m_name; /// The user friendly name of the frontend
    QHostAddress m_ip; /// The frontend IP address

    int m_connectionCount {0};

    //int m_capabilities; // Future expansion, a bitmap of 'capabilities'
};

class BackendContext
{
  public:
    BackendContext() = default;
   ~BackendContext();

    void SetFrontendConnected(Frontend *frontend);
    void SetFrontendDisconnected(const QString &name);

    QMap<QString, Frontend*> GetConnectedFrontends() const { return m_connectedFrontends; }
    QMap<QString, Frontend*> GetFrontends() const { return m_knownFrontends; }

  private:
    QMap<QString, Frontend*> m_connectedFrontends;
    QMap<QString, Frontend*> m_knownFrontends;
};

#endif // BACKEND_CONTEXT_H
