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

void TvPlayWindow::Close(int delay)
{
    // delay closing to give the player a little more time to start up
    QTimer::singleShot(delay, this, SLOT(DoClose(void)));
}

void TvPlayWindow::DoClose(void)
{
    GetMythMainWindow()->GetPaintWindow()->hide();
    MythScreenType::Close();
}
