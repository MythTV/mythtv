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

#include <mythtv/mythcontext.h>
#include <mythtv/mythdialogs.h>
#include <mythtv/mythplugin.h>

extern "C" {
int mythplugin_init(const char *libversion);
int mythplugin_run(void);
int mythplugin_config(void);
}

int mythplugin_init(const char *libversion)
{
    if (!gContext->TestPopupVersion("mythweather", libversion,
                                    MYTH_BINARY_VERSION))
        return -1;

    return 0;
}

int mythplugin_run(void)
{
    int appCode = 0;

/*
    if (argc > 1)
    {
       QString cmdline = QString(argv[1]);
       if (cmdline == "--debug")
           appCode = 1;
       if (cmdline == "--configure")
           appCode = 2;
    }
*/

    QTranslator translator(0);
    translator.load(PREFIX + QString("/share/mythtv/i18n/mythweather_") +
                    QString(gContext->GetSetting("Language").lower()) +
                    QString(".qm"), ".");

    qApp->installTranslator(&translator);

    Weather weatherDat(QSqlDatabase::database(), appCode, 
                       gContext->GetMainWindow(), "weather");
    weatherDat.exec();

    qApp->removeTranslator(&translator);

    return 0;
}

int mythplugin_config(void)
{
    int appCode = 2;

    QTranslator translator(0);
    translator.load(PREFIX + QString("/share/mythtv/i18n/mythweather_") +
                    QString(gContext->GetSetting("Language").lower()) +
                    QString(".qm"), ".");

    qApp->installTranslator(&translator);

    Weather weatherDat(QSqlDatabase::database(), appCode,
                       gContext->GetMainWindow(), "weather");
    weatherDat.exec();

    qApp->removeTranslator(&translator);

    return 0;
}

