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

    gContext->LoadQtConfig();

    QString auddevice = gContext->GetSetting("AudioOutputDevice");
    if (auddevice == "" || auddevice == QString::null)
    {
        cerr << "You need to run 'mythfrontend', not 'mythtv'.\n";
        exit(-1);
    }

    MythMainWindow *mainWindow = new MythMainWindow();
    mainWindow->Init();
    gContext->SetMainWindow(mainWindow);

    TV::InitKeys();

    TV *tv = new TV();
    tv->Init();

    if (a.argc() > 1)
    {
        QString filename = a.argv()[1];

        ProgramInfo *pginfo = new ProgramInfo();
        pginfo->endts = QDateTime::currentDateTime().addSecs(-180);
        pginfo->pathname = filename;
    
        tv->Playback(pginfo);
    }
    else
        tv->LiveTV(true);

    qApp->unlock();
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
