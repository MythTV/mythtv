// mythtv
#include <mythcontext.h>
#include <lcddevice.h>

// mythmusic
#include "miniplayer.h"
#include "musicplayer.h"
#include "decoder.h"

MiniPlayer::MiniPlayer(MythScreenStack *parent)
          : MusicCommon(parent, NULL, "music_miniplayer")
{
    m_currentView = MV_MINIPLAYER;
    m_displayTimer = new QTimer(this);
    m_displayTimer->setSingleShot(true);
    connect(m_displayTimer, SIGNAL(timeout()), this, SLOT(timerTimeout()));
}

MiniPlayer::~MiniPlayer(void)
{
    gPlayer->removeListener(this);

    // Timers are deleted by Qt
    m_displayTimer->disconnect();
    m_displayTimer = NULL;

    if (LCD *lcd = LCD::Get())
        lcd->switchToTime ();
}

void MiniPlayer::timerTimeout(void)
{
    Close();
}

bool MiniPlayer::Create(void)
{
    bool err = false;

    // Load the theme for this screen
    err = LoadWindowFromXML("music-ui.xml", "miniplayer", this);

    if (!err)
        return false;

    // find common widgets available on any view
    err = CreateCommon();

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'miniplayer'");
        return false;
    }

    m_displayTimer->start(10000);

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
            gPlayer->autoShowPlayer(!gPlayer->getAutoShowPlayer());
            //showAutoMode();
        }
        else
            handled = false;
    }

    if (!handled && MusicCommon::keyPressEvent(event))
        handled = true;

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}
