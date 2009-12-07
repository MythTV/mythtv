#include <QCoreApplication>
#include <QThreadPool>
#include <QRunnable>
#include <QString>
#include <QEvent>
#include <QTimer>

#include "mythcontext.h"
#include "exitcodes.h"
#include "util.h" // for checkTimeZone()
#include "backendconnectionmanager.h"

class Reconnect : public QRunnable
{
  public:
    Reconnect()
    {
        setAutoDelete(false);
    }

    virtual void run(void)
    {
        if (gContext->GetMasterHostPrefix().isEmpty())
            gContext->dispatch(MythEvent(QString("RECONNECT_FAILURE")));
        else
            gContext->dispatch(MythEvent(QString("RECONNECT_SUCCESS")));
    }
};

BackendConnectionManager::BackendConnectionManager() :
    QObject(), m_reconnecting(NULL), m_reconnect_timer(NULL), m_first_time(true)
{
    gContext->addListener(this);

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
    gContext->removeListener(this);
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
            if (m_first_time && !checkTimeZone())
            {
                // Check for different time zones, 
                // different offsets, different times
                VERBOSE(VB_IMPORTANT,
                        "The time and/or time zone settings on this "
                        "system do not match those in use on the master "
                        "backend. Please ensure all frontend and backend "
                        "systems are configured to use the same time "
                        "zone and have the current time properly set.");
                VERBOSE(VB_IMPORTANT,
                        "Unable to run with invalid time settings. "
                        "Exiting.");
                qApp->exit(FRONTEND_EXIT_INVALID_TIMEZONE);
            }
            m_first_time = false;
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
    QThreadPool::globalInstance()->start(m_reconnecting);
}
