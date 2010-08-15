// Qt
#include <QTimer>

// MythTV
#include "mythmainwindow.h"
#include "mythverbose.h"

#include "tv_play_win.h"


TvPlayWindow::TvPlayWindow(MythScreenStack *parent, const char *name)
            : MythScreenType(parent, name)
{
    SetCanTakeFocus(true);
}

TvPlayWindow::~TvPlayWindow()
{
}

bool TvPlayWindow::Create()
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = CopyWindowFromBase("videowindow", this);

    if (!foundtheme)
    {
        VERBOSE(VB_IMPORTANT, "Cannot load screen videowindow from base.xml");
        return false;
    }

    return true;
}

/** \brief Mouse click/movement handler, recieves mouse gesture events
 *         from event loop. Should not be used directly.
 *
 *  \param uitype The mythuitype receiving the event
 *  \param event Mouse event
 */
void TvPlayWindow::gestureEvent(MythUIType *uitype, MythGestureEvent *event)
{
    if (event->gesture() == MythGestureEvent::Click)
    {

    }
}
