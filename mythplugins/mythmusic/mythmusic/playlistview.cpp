#include <iostream>

// qt
#include <QKeyEvent>

// myth
#include <mythdialogbox.h>

// mythmusic
#include "musiccommon.h"
#include "playlistview.h"

PlaylistView::PlaylistView(MythScreenStack *parent)
         :MusicCommon(parent, "playlistview")
{
    m_currentView = MV_PLAYLIST;
}

PlaylistView::~PlaylistView()
{
}

bool PlaylistView::Create(void)
{
    bool err = false;

    // Load the theme for this screen
    err = LoadWindowFromXML("music-ui.xml", "playlistview", this);

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
    bool handled = false;

#if 0
    if (event->type() == DialogCompletionEvent::kEventType)
    {
        DialogCompletionEvent *dce = (DialogCompletionEvent*)(event);

        QString resultid   = dce->GetId();
        QString resulttext = dce->GetResultText();
        //TODO::
        if (resultid == "menu")
        {
        }
    }
#endif

    if (!handled)
        MusicCommon::customEvent(event);
}


bool PlaylistView::keyPressEvent(QKeyEvent *event)
{
    if (!m_moveTrackMode && GetFocusWidget() && GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("Music", event, actions);

#if 0
    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "MENU")
        {
            showMenu();
        }
        else
            handled = false;
    }
#endif

    if (!handled && MusicCommon::keyPressEvent(event))
        handled = true;

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}
