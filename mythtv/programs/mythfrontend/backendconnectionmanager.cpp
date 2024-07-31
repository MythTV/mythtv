// C++
#include <chrono> // for milliseconds
#include <thread> // for sleep_for

// Qt
#include <QCoreApplication>
#include <QRunnable>
#include <QString>
#include <QEvent>
#include <QTimer>

// MythTV
#include "libmythbase/exitcodes.h"
#include "libmythbase/mthreadpool.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythtimezone.h"
#include "libmythui/mythdialogbox.h"
#include "libmythui/mythmainwindow.h"
#include "libmythui/mythscreenstack.h"

// MythFrontend
#include "backendconnectionmanager.h"


using namespace MythTZ;

class Reconnect : public QRunnable
{
  public:
    Reconnect()
    {
        setAutoDelete(false);
    }

    void run(void) override // QRunnable
    {
        if (!gCoreContext->SafeConnectToMasterServer(gCoreContext->IsBlockingClient()))
            gCoreContext->dispatch(MythEvent(QString("RECONNECT_FAILURE")));
        else
            gCoreContext->dispatch(MythEvent(QString("RECONNECT_SUCCESS")));
    }
};

BackendConnectionManager::BackendConnectionManager()
  : m_reconnectTimer(new QTimer(this))
{
    setObjectName("BackendConnectionManager");
    gCoreContext->addListener(this);

    uint reconnect_timeout = 1;
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout,
            this,              &BackendConnectionManager::ReconnectToBackend);
    m_reconnectTimer->start(reconnect_timeout);
}

BackendConnectionManager::~BackendConnectionManager()
{
    while (m_reconnecting)
        std::this_thread::sleep_for(250ms);
    gCoreContext->removeListener(this);
}

void BackendConnectionManager::customEvent(QEvent *event)
{
    bool reconnect = false;
    uint reconnect_timeout = 5000;

    if (event->type() == MythEvent::kMythEventMessage)
    {
        auto *me = dynamic_cast<MythEvent *>(event);
        if (me == nullptr)
            return;

        const QString& message = me->Message();

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
                m_reconnectAgain = true;
            }
        }
        else if ((message == "RECONNECT_SUCCESS") ||
                 (message == "RECONNECT_FAILURE"))
        {
            LOG(VB_SOCKET, LOG_INFO, QString("Got %1 message")
                .arg(message));

            delete m_reconnecting;
            m_reconnecting = nullptr;

            if (!m_reconnectAgain)
            {
                m_reconnectAgain = message == "RECONNECT_FAILURE";
            }
            reconnect = m_reconnectAgain;
            m_reconnectAgain = message == "RECONNECT_FAILURE";
        }
    }

    if (reconnect)
    {
        if (!m_reconnectTimer)
        {
            m_reconnectTimer = new QTimer(this);
            m_reconnectTimer->setSingleShot(true);
            connect(m_reconnectTimer, &QTimer::timeout,
                    this,              &BackendConnectionManager::ReconnectToBackend);
        }
        m_reconnectTimer->start(reconnect_timeout);
    }
}

void BackendConnectionManager::ReconnectToBackend(void)
{
    LOG(VB_SOCKET, LOG_INFO, "Reconnecting");
    m_reconnecting = new Reconnect();
    MThreadPool::globalInstance()->start(m_reconnecting, "Reconnect");
}
