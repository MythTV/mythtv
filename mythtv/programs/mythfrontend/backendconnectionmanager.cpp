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
        // Note: GetMasterHostPrefix() implicitly reconnects the sockets
        if (gCoreContext->GetMasterHostPrefix().isEmpty())
            gCoreContext->dispatch(MythEvent(QString("RECONNECT_FAILURE")));
        else
            gCoreContext->dispatch(MythEvent(QString("RECONNECT_SUCCESS")));
    }
};

BackendConnectionManager::BackendConnectionManager() :
    QObject(), m_reconnecting(NULL), m_reconnect_timer(NULL),
    m_reconnect_again(false)
{
    setObjectName("BackendConnectionManager");
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
            LOG(VB_SOCKET, LOG_INFO, "Got BACKEND_SOCKETS_CLOSED message");

            if (!m_reconnecting)
            {
                LOG(VB_SOCKET, LOG_INFO, "Will reconnect");
                reconnect = true;
                reconnect_timeout = 500;
            }
            else
            {
                LOG(VB_SOCKET, LOG_INFO, "Already reconnecting");
                m_reconnect_again = true;
            }
        }
        else if ((message == "RECONNECT_SUCCESS") ||
                 (message == "RECONNECT_FAILURE"))
        {
            LOG(VB_SOCKET, LOG_INFO, QString("Got %1 message")
                .arg(message));

            delete m_reconnecting;
            m_reconnecting = NULL;

            reconnect = m_reconnect_again;
            m_reconnect_again = false;
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
    LOG(VB_SOCKET, LOG_INFO, "Reconnecting");
    m_reconnecting = new Reconnect();
    MThreadPool::globalInstance()->start(m_reconnecting, "Reconnect");
}
