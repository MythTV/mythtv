// Qt
#include <QTimer>

// MythTV
#include "mythmainwindow.h"
#include "myththemebase.h"
#include "mythverbose.h"

#include "tv_play_win.h"


TvPlayWindow::TvPlayWindow(MythScreenStack *parent, const char *name)
            : MythScreenType(parent, name)
{
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

