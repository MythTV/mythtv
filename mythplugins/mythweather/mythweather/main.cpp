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
    REG_JUMP("MythWeather", "Weather forecasts", "", runWeather);

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

    Weather weatherDat(appCode, gContext->GetMainWindow(), "weather");
    weatherDat.exec();
}

int mythplugin_run(void)
{
    runWeather();
    return 0;
}

int mythplugin_config(void)
{
    int appCode = 2;

    Weather weatherDat(appCode, gContext->GetMainWindow(), "weather");
    weatherDat.exec();

    return 0;
}

