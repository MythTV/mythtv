#include <iostream>

using namespace std;

#include <qapplication.h>
#include <qsqldatabase.h>

#include "iconview.h"

#include <mythtv/themedmenu.h>
#include <mythtv/mythcontext.h>

MythContext *gContext;

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    gContext = new MythContext();

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

    gContext->LoadSettingsFiles("mythgallery-settings.txt");

    gContext->LoadQtConfig();

    QString startdir = gContext->GetSetting("GalleryDir");

    IconView icv(db, startdir);

    icv.exec();

    delete gContext;

    return 0;
}

