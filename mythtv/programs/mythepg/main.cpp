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
    gContext->Init();

    if (!MSqlQuery::testDBConnection())
    {
        printf("couldn't open db\n");
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

    QString chanstr = RunProgramGuide(startchannel);

    int chan = 0;
    
    if (chanstr != "")
        chan = atoi(chanstr.ascii());

    delete gContext;

    return chan;
}
