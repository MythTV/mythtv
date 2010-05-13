#include <QCoreApplication>
#include <QThreadPool>
#include <QRunnable>
#include <QString>
#include <QEvent>
#include <QTimer>

#include "mythcorecontext.h"
#include "mythdialogbox.h"
#include "mythscreenstack.h"
#include "mythmainwindow.h"
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
        if (gCoreContext->GetMasterHostPrefix().isEmpty())
            gCoreContext->dispatch(MythEvent(QString("RECONNECT_FAILURE")));
        else
            gCoreContext->dispatch(MythEvent(QString("RECONNECT_SUCCESS")));
    }
};

BackendConnectionManager::BackendConnectionManager() :
    QObject(), m_reconnecting(NULL), m_reconnect_timer(NULL), m_first_time(true)
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
                MythScreenStack *popupStack = GetMythMainWindow()->
                                                 GetStack("popup stack");
                QString message = tr("Your frontend and backend are configured "
                                     "in different timezones.  You must correct "
                                     "this mismatch to continue.");
                MythConfirmationDialog *error =  new MythConfirmationDialog(
                                                     popupStack, message, false);
                if (error->Create())
                {
                    QObject::connect(error, SIGNAL(haveResult(bool)), 
                                     qApp, SLOT(quit()));
                    popupStack->AddScreen(error);
                }
                else
                {
                    delete error;
                    delete popupStack;
                    qApp->exit(FRONTEND_EXIT_INVALID_TIMEZONE);
                }
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
