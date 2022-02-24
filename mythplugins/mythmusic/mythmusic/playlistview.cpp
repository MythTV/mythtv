#include <iostream>

// qt
#include <QKeyEvent>

// MythTV
#include <libmythui/mythdialogbox.h>

// mythmusic
#include "musiccommon.h"
#include "playlistview.h"

PlaylistView::PlaylistView(MythScreenStack *parent, MythScreenType *parentScreen)
         :MusicCommon(parent, parentScreen, "playlistview")
{
    m_currentView = MV_PLAYLIST;
}

bool PlaylistView::Create(void)
{
    // Load the theme for this screen
    bool err = LoadWindowFromXML("music-ui.xml", "playlistview", this);

    if (!err)
        return false;

    // find common widgets available on any view
    err = CreateCommon();

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'playlisteditorview'");
        return false;
    }

    BuildFocusList();

    return true;
}

void PlaylistView::customEvent(QEvent *event)
{
    MusicCommon::customEvent(event);
}

bool PlaylistView::keyPressEvent(QKeyEvent *event)
{
    if (!m_moveTrackMode && GetFocusWidget() && GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;

    if (MusicCommon::keyPressEvent(event))
        handled = true;

    return handled;
}
