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
#include "weathercomms.h"

#include <mythtv/themedmenu.h>
#include <mythtv/mythcontext.h>

MythContext *gContext;

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QTranslator translator(0);

    gContext = new MythContext(MYTH_BINARY_VERSION);

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

    translator.load(PREFIX + QString("/share/mythtv/i18n/mythweather_") +
                    QString(gContext->GetSetting("Language").lower()) +
                    QString(".qm"), ".");
    a.installTranslator(&translator);

    gContext->LoadQtConfig();

    int appCode = 0;

    if (argc > 1)
    {
       QString cmdline = QString(argv[1]);
       if (cmdline == "--debug")
           appCode = 1;
       if (cmdline == "--configure")
           appCode = 2;
    }

    Weather weatherDat(db, appCode);
    weatherDat.Show();
    weatherDat.exec();

    delete gContext;

    return 0;
}
