#include <qapplication.h>
#include <qsqldatabase.h>
#include <stdlib.h>

#include "libmyth/mythcontext.h"
#include "libmyth/settings.h"

#include "libmythtv/guidegrid.h"
#include "libmythtv/tv.h"

MythContext *gContext;

int main(int argc, char **argv)
{
    QApplication a(argc, argv);

    gContext = new MythContext(MYTH_BINARY_VERSION);

    QSqlDatabase *db = QSqlDatabase::addDatabase("QMYSQL3");
    if (!gContext->OpenDatabase(db))
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
    mainWindow->Show();
    gContext->SetMainWindow(mainWindow);

    TV::InitKeys();
    
    QString chanstr = RunProgramGuide(startchannel);

    int chan = 0;
    
    if (chanstr != QString::null)
        chan = atoi(chanstr.ascii());

    delete gContext;

    return chan;
}
