#include <qapplication.h>
#include <qsqldatabase.h>
#include <cstdlib>

#include "libmyth/mythcontext.h"
#include "libmyth/mythdbcon.h"
#include "libmyth/settings.h"

#include "libmythtv/guidegrid.h"
#include "libmythtv/tv.h"

int main(int argc, char **argv)
{
    QApplication a(argc, argv);

    gContext = NULL;
    gContext = new MythContext(MYTH_BINARY_VERSION);
    if(!gContext->Init())
    {
        VERBOSE(VB_IMPORTANT, "Failed to init MythContext, exiting.");
        return -1;
    }

    QString startchannel = gContext->GetSetting("DefaultTVChannel");
    if (startchannel == "")
        startchannel = "3";

    if (a.argc() > 1)
    {
        startchannel = a.argv()[1];
    }

    gContext->LoadQtConfig();

    MythMainWindow *mainWindow = new MythMainWindow();
    mainWindow->Init();
    gContext->SetMainWindow(mainWindow);

    TV::InitKeys();

    RunProgramGuide(startchannel);

    int chan = 0;
    
    if (startchannel != "")
        chan = atoi(startchannel.ascii());

    delete gContext;

    return chan;
}
