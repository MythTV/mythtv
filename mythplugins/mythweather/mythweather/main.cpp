/*
	MythWeather
	Version 0.8
	January 8th, 2003

	By John Danner & Dustin Doris

	Note: Portions of this code taken from MythMusic

*/

#include <iostream>
using namespace std;

#include <qapplication.h>
#include <qsqldatabase.h>
#include <unistd.h>

#include "weather.h"

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

    QString themename = context->GetSetting("Theme");
    QString themedir = context->FindThemeDir(themename);

    QString paths = context->GetSetting("TreeLevels");

    if (themedir == "")
    {
        cerr << "Couldn't find theme " << themename << endl;
        exit(0);
    }

    context->LoadQtConfig();

    Weather weatherDat(context);
    weatherDat.Show();
    weatherDat.exec();
 
    delete context;

    return 0;
}
