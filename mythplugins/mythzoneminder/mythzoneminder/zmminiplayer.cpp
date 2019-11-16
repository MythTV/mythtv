
#include "zmminiplayer.h"

// mythtv
#include <mythcontext.h>
#include <lcddevice.h>
#include <mythuiimage.h>
#include <mythmainwindow.h>

// mythzoneminder
#include "zmclient.h"

#include <QTimer>

// the maximum image size we are ever likely to get from ZM
#define MAX_IMAGE_SIZE  (2048*1536*3)

const int FRAME_UPDATE_TIME = 1000 / 10;  // try to update the frame 10 times a second

ZMMiniPlayer::ZMMiniPlayer(MythScreenStack *parent)
          : ZMLivePlayer(parent, true),
            m_displayTimer(new QTimer(this))
{
    m_displayTimer->setSingleShot(true);
    connect(m_displayTimer, SIGNAL(timeout()), this, SLOT(timerTimeout()));

}

ZMMiniPlayer::~ZMMiniPlayer(void)
{
    gCoreContext->removeListener(this);

    // Timers are deleted by Qt
    m_displayTimer->disconnect();
    m_displayTimer = nullptr;

    if (LCD *lcd = LCD::Get())
        lcd->switchToTime ();
}

void ZMMiniPlayer::timerTimeout(void)
{
    // if we was triggered because of an alarm wait for the monitor to become idle
    if (m_alarmMonitor != -1)
    {
        Monitor *mon = ZMClient::get()->getMonitorByID(m_alarmMonitor);
        if (mon && (mon->state == ALARM || mon->state == ALERT))
        {
            m_displayTimer->start(10000);
            return;
        }
    }

    Close();
}

bool ZMMiniPlayer::Create(void)
{
    if (!ZMLivePlayer::Create())
        return false;

    m_displayTimer->start(10000);

    gCoreContext->addListener(this);

    return true;
}

void ZMMiniPlayer::customEvent (QEvent* event)
{
    if (event->type() == MythEvent::MythEventMessage)
    {
        MythEvent *me = dynamic_cast<MythEvent*>(event);
        if (!me)
            return;

        if (me->Message().startsWith("ZONEMINDER_NOTIFICATION"))
        {
            QStringList list = me->Message().simplified().split(' ');

            if (list.size() < 2)
                return;

            int monID = list[1].toInt();
            if (monID != m_alarmMonitor)
            {
                m_alarmMonitor = monID;

                m_frameTimer->stop();

                Monitor *mon = ZMClient::get()->getMonitorByID(monID);

                if (mon)
                {
                    m_players->at(0)->setMonitor(mon);
                    m_players->at(0)->updateCamera();
                }

                m_frameTimer->start(FRAME_UPDATE_TIME);
            }

            // restart the display timer on any notification messages if it is active
            if (m_displayTimer->isActive())
                m_displayTimer->start(10000);
        }
    }

    ZMLivePlayer::customEvent(event);
}

bool ZMMiniPlayer::keyPressEvent(QKeyEvent *event)
{
    // restart the display timer on any keypress if it is active
    if (m_displayTimer && m_displayTimer->isActive())
        m_displayTimer->start();

    if (GetFocusWidget() && GetFocusWidget()->keyPressEvent(event))
        return true;

    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("Music", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "SELECT")
        {
            if (m_displayTimer)
                m_displayTimer->stop();
        }
        else if (action == "ESCAPE")
        {
            Close();
        }
        else if (action == "MENU")
        {
        }
        else
            handled = false;
    }

    if (!handled && ZMLivePlayer::keyPressEvent(event))
        handled = true;

    return handled;
}
