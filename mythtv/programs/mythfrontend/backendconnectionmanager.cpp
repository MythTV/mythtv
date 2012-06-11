#include <QCoreApplication>
#include <QRunnable>
#include <QString>
#include <QEvent>
#include <QTimer>

#include "backendconnectionmanager.h"
#include "mythcorecontext.h"
#include "mythdialogbox.h"
#include "mythscreenstack.h"
#include "mythmainwindow.h"
#include "mthreadpool.h"
#include "mythlogging.h"
#include "exitcodes.h"
#include "mythtimezone.h"
using namespace MythTZ;

class Reconnect : public QRunnable
{
  public:
    Reconnect()
    {
        setAutoDelete(false);
    }

    virtual void run(void)
    {
        if (gCoreContext->GetMasterHostPrefix().isEmpty())
            gCoreContext->dispatch(MythEvent(QString("RECONNECT_FAILURE")));
        else
            gCoreContext->dispatch(MythEvent(QString("RECONNECT_SUCCESS")));
    }
};

BackendConnectionManager::BackendConnectionManager() :
    QObject(), m_reconnecting(NULL), m_reconnect_timer(NULL)
{
    gCoreContext->addListener(this);

    uint reconnect_timeout = 1;
    m_reconnect_timer = new QTimer(this);
    m_reconnect_timer->setSingleShot(true);
    connect(m_reconnect_timer, SIGNAL(timeout()),
            this,              SLOT(ReconnectToBackend()));
    m_reconnect_timer->start(reconnect_timeout);
}

BackendConnectionManager::~BackendConnectionManager()
{
    while (m_reconnecting)
        usleep(250*1000);
    gCoreContext->removeListener(this);
}

void BackendConnectionManager::customEvent(QEvent *event)
{
    bool reconnect = false;
    uint reconnect_timeout = 5000;

    if ((MythEvent::Type)(event->type()) == MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent *)event;
        QString message = me->Message();

        if (message == "BACKEND_SOCKETS_CLOSED")
        {
            if (!m_reconnecting)
            {
                reconnect = true;
                reconnect_timeout = 500;
            }
        }
        else if (message == "RECONNECT_SUCCESS")
        {
            delete m_reconnecting;
            m_reconnecting = NULL;
        }
        else if (message == "RECONNECT_FAILURE")
        {
            delete m_reconnecting;
            m_reconnecting = NULL;
            reconnect = true;
        }
    }

    if (reconnect)
    {
        if (!m_reconnect_timer)
        {
            m_reconnect_timer = new QTimer(this);
            m_reconnect_timer->setSingleShot(true);
            connect(m_reconnect_timer, SIGNAL(timeout()),
                    this,              SLOT(ReconnectToBackend()));
        }
        m_reconnect_timer->start(reconnect_timeout);
    }
}

void BackendConnectionManager::ReconnectToBackend(void)
{
    m_reconnecting = new Reconnect();
    MThreadPool::globalInstance()->start(m_reconnecting, "Reconnect");
}
