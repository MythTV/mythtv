#include <qapplication.h>
#include <qsqldatabase.h>
#include <qstring.h>
#include <unistd.h>
#include "tv.h"
#include "programinfo.h"

#include "libmyth/mythcontext.h"

#include <iostream>
using namespace std;

MythContext *gContext;

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    gContext = new MythContext(MYTH_BINARY_VERSION);

    QString themename = gContext->GetSetting("Theme");
    QString themedir = gContext->FindThemeDir(themename);
    if (themedir == "")
    {   
        cerr << "Couldn't find theme " << themename << endl;
        exit(0);
    }
    
    gContext->LoadQtConfig();

    QSqlDatabase *db = QSqlDatabase::addDatabase("QMYSQL3");
    if (!db)
    {
        printf("Couldn't connect to database\n");
        return -1; 
    }       
    if (!gContext->OpenDatabase(db))
    {
        printf("couldn't open db\n");
        return -1;
    }

    QString server = gContext->GetSetting("MasterServerIP", "localhost");
    int port = gContext->GetNumSetting("MasterServerPort", 6543);
    gContext->ConnectServer(server, port);

    gContext->LoadQtConfig();

    MythMainWindow *mainWindow = new MythMainWindow();
    mainWindow->Show();
    gContext->SetMainWindow(mainWindow);

    TV *tv = new TV(db);
    tv->Init();

    if (a.argc() > 1)
    {
        QString filename = a.argv()[1];

        ProgramInfo *pginfo = new ProgramInfo();
        pginfo->pathname = filename;
    
        tv->Playback(pginfo);
    }
    else
        tv->LiveTV();

    qApp->unlock();
    while (tv->GetState() == kState_None)
    {
        usleep(1000);
        qApp->processEvents();
    }

    while (tv->GetState() != kState_None)
    {
        usleep(1000);
        qApp->processEvents();
    }

    sleep(1);
    delete tv;
    delete gContext;

    exit(0);
}
