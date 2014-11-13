// mythtv
#include <mythcontext.h>
#include <lcddevice.h>
#include <mythuiimage.h>

// mythzoneminder
#include "zmminiplayer.h"

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
    // Timers are deleted by Qt
    m_displayTimer->disconnect();
    m_displayTimer = NULL;

    if (LCD *lcd = LCD::Get())
        lcd->switchToTime ();
}

void ZMMiniPlayer::timerTimeout(void)
{
    Close();
}

bool ZMMiniPlayer::Create(void)
{
    if (!ZMLivePlayer::Create())
        return false;

    m_displayTimer->start(10000);

    return true;
}

bool ZMMiniPlayer::keyPressEvent(QKeyEvent *event)
{
    // restart the display timer on any keypress if it is active
    if (m_displayTimer && m_displayTimer->isActive())
        m_displayTimer->start();

    if (GetFocusWidget() && GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("Music", event, actions);

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

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}
