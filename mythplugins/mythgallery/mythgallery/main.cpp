#include <iostream>

using namespace std;

#include <qapplication.h>
#include <qsqldatabase.h>

#include "iconview.h"

#include <mythtv/themedmenu.h>
#include <mythtv/mythcontext.h>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    MythContext *context = new MythContext();

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

    context->LoadSettingsFiles("mythgallery-settings.txt");

    context->LoadQtConfig();

    QString startdir = context->GetSetting("GalleryDir");

    IconView icv(context, db, startdir);

    icv.exec();

    delete context;

    return 0;
}

