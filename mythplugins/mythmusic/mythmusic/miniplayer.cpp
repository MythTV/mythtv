
// C++
#include <chrono>

// Qt
#include <QTimer>

// MythTV
#include <libmyth/mythcontext.h>
#include <libmythbase/lcddevice.h>
#include <libmythui/mythmainwindow.h>

// mythmusic
#include "decoder.h"
#include "miniplayer.h"
#include "musicplayer.h"


MiniPlayer::MiniPlayer(MythScreenStack *parent)
          : MusicCommon(parent, nullptr, "music_miniplayer"),
            m_displayTimer(new QTimer(this))
{
    m_currentView = MV_MINIPLAYER;
    m_displayTimer->setSingleShot(true);
    connect(m_displayTimer, &QTimer::timeout, this, &MiniPlayer::timerTimeout);
}

MiniPlayer::~MiniPlayer(void)
{
    gPlayer->removeListener(this);

    // Timers are deleted by Qt
    m_displayTimer->disconnect();
    m_displayTimer = nullptr;

    if (LCD *lcd = LCD::Get())
        lcd->switchToTime ();
}

void MiniPlayer::timerTimeout(void)
{
    Close();
}

bool MiniPlayer::Create(void)
{
    // Load the theme for this screen
    bool err = LoadWindowFromXML("music-ui.xml", "miniplayer", this);

    if (!err)
        return false;

    // find common widgets available on any view
    err = CreateCommon();

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'miniplayer'");
        return false;
    }

    m_displayTimer->start(10s);

    BuildFocusList();

    return true;
}

bool MiniPlayer::keyPressEvent(QKeyEvent *event)
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
        const QString& action = actions[i];
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
            gPlayer->autoShowPlayer(!gPlayer->getAutoShowPlayer());
            //showAutoMode();
        }
        else
        {
            handled = false;
        }
    }

    if (!handled && MusicCommon::keyPressEvent(event))
        handled = true;

    return handled;
}
