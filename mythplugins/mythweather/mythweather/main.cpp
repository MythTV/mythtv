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

void runWeather(void);

void setupKeys(void)
{
    REG_JUMP("MythWeather", "Weather forcasts", "", runWeather);

    REG_KEY("Weather", "PAUSE", "Pause current page", "P");
}

int mythplugin_init(const char *libversion)
{
    if (!gContext->TestPopupVersion("mythweather", libversion,
                                    MYTH_BINARY_VERSION))
        return -1;

    setupKeys();

    return 0;
}

void runWeather(void)
{
    int appCode = 0;

    QTranslator translator(0);
    translator.load(gContext->GetTranslationsDir() + QString("mythweather_") +
                    QString(gContext->GetSetting("Language").lower()) +
                    QString(".qm"), ".");

    qApp->installTranslator(&translator);

    Weather weatherDat(QSqlDatabase::database(), appCode, 
                       gContext->GetMainWindow(), "weather");
    weatherDat.exec();

    qApp->removeTranslator(&translator);
}

int mythplugin_run(void)
{
    runWeather();
    return 0;
}

int mythplugin_config(void)
{
    int appCode = 2;

    QTranslator translator(0);
    translator.load(gContext->GetTranslationsDir() + QString("mythweather_") +
                    QString(gContext->GetSetting("Language").lower()) +
                    QString(".qm"), ".");

    qApp->installTranslator(&translator);

    Weather weatherDat(QSqlDatabase::database(), appCode,
                       gContext->GetMainWindow(), "weather");
    weatherDat.exec();

    qApp->removeTranslator(&translator);

    return 0;
}

