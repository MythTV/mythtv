#ifndef MAIN_CPP
#define MAIN_CPP

using namespace std;

#include "mythappearance.h"
#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>
#include <mythtv/lcddevice.h>
#include <mythtv/libmythui/myththemedmenu.h>
#include <mythtv/libmythui/mythscreenstack.h>
#include <mythtv/libmythui/mythmainwindow.h>

extern "C" {
    int mythplugin_init(const char *libversion);
    int mythplugin_run(void);
    int mythplugin_config(void);
}

int mythplugin_init(const char *libversion)
{
    if (!gContext->TestPopupVersion("mythappearance", libversion, MYTH_BINARY_VERSION))
        return -1;

    return 0;
}

int mythplugin_run (void)
{
    gContext->addCurrentLocation("mythappearance");

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    MythAppearance *mythappearance = new MythAppearance(mainStack, "mythappearance");

    if (mythappearance->Create())
        mainStack->AddScreen(mythappearance);
    else
        return -1;

    //GetMythMainWindow()->JumpTo("Reload Theme");

    gContext->removeCurrentLocation();

    return 1;
}

int mythplugin_config (void) { return 0; }

#endif
