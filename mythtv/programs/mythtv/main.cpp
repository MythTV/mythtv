#include <qapplication.h>
#include <qsqldatabase.h>
#include <qstring.h>
#include <unistd.h>
#include "tv.h"

#include "libmyth/mythcontext.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    MythContext *context = new MythContext();
    QString themename = context->GetSetting("Theme");
    QString themedir = context->FindThemeDir(themename);
    if (themedir == "")
    {   
        cerr << "Couldn't find theme " << themename << endl;
        exit(0);
    }
    
    context->LoadQtConfig();

    QSqlDatabase *db = QSqlDatabase::addDatabase("QMYSQL3");
    if (!db)
    {
        printf("Couldn't connect to database\n");
        return -1; 
    }       
    if (!context->OpenDatabase(db))
    {
        printf("couldn't open db\n");
        return -1;
    }

    QString startChannel = context->GetSetting("DefaultTVChannel");
    if (startChannel == "")
        startChannel = "3";

    TV *tv = new TV(context, startChannel, 1, 2);
    tv->LiveTV();

    while (tv->GetState() == kState_None)
        usleep(1000);

    qApp->unlock();
    while (tv->GetState() != kState_None)
    {
        usleep(1000);
        qApp->processEvents();
    }

    sleep(1);
    delete tv;

    exit(0);
}
