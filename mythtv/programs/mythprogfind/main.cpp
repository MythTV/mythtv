#include <qapplication.h>
#include <qsqldatabase.h>
#include <stdlib.h>

#include "libmyth/mythcontext.h"
#include "libmyth/settings.h"

#include "libmythtv/progfind.h"
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

    gContext->LoadQtConfig();

    MythMainWindow *mainWindow = new MythMainWindow();
    mainWindow->Show();
    gContext->SetMainWindow(mainWindow);
    
    TV::InitKeys();

    RunProgramFind();

    delete gContext;

    return 0;
}
