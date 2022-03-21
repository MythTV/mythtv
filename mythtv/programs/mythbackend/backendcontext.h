#ifndef BACKEND_CONTEXT_H
#define BACKEND_CONTEXT_H

#include <QString>
#include <QMap>
#include <QList>
#include <QHostAddress>

#include "libmythtv/mythsystemevent.h"

class EncoderLink;
class AutoExpire;
class Scheduler;
class JobQueue;
class HouseKeeper;
class MediaServer;
class BackendContext;

extern QMap<int, EncoderLink *> gTVList;
extern AutoExpire  *gExpirer;
extern JobQueue    *gJobQueue;
extern HouseKeeper *gHousekeeping;
extern MediaServer *g_pUPnp;
extern BackendContext *gBackendContext;
extern QString      gPidFile;
extern MythSystemEventHandler *gSysEventHandler;

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
