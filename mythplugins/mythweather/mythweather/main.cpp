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
#include "weatherSetup.h"
#include "sourceManager.h"
#include "dbcheck.h"

#include <mythtv/lcddevice.h>
#include <mythtv/mythcontext.h>
#include <mythtv/mythdialogs.h>
#include <mythtv/mythplugin.h>
#include <mythtv/libmythui/myththemedmenu.h>

extern "C" {
int mythplugin_init(const char *libversion);
void mythplugin_destroy(void);
int mythplugin_run(void);
int mythplugin_config(void);
}

SourceManager *srcMan = 0;
XMLParse *theme = 0;

void runWeather(void);
void loadTheme();

void setupKeys(void)
{
    REG_JUMP("MythWeather", "Weather forecasts", "", runWeather);
    REG_KEY("Weather", "PAUSE", "Pause current page", "P");
    REG_KEY("Weather", "SEARCH", "Search List", "/");
    REG_KEY("Weather", "NEXTSEARCH", "Search List", "n");
    REG_KEY("Weather", "UPDATE", "Search List", "u");
}

int mythplugin_init(const char *libversion)
{
    if (!gContext->TestPopupVersion("mythweather", libversion,
                                    MYTH_BINARY_VERSION))
        return -1;
    
    gContext->ActivateSettingsCache(false);
    InitializeDatabase();
    gContext->ActivateSettingsCache(true);

    setupKeys();

    if(gContext->GetNumSetting("weatherbackgroundfetch",0)) {
        srcMan = new SourceManager();
        srcMan->startTimers();
        srcMan->doUpdate();
    }
    return 0;
}

void runWeather(void)
{
    gContext->addCurrentLocation("mythweather");
    if (!srcMan) {
        srcMan = new SourceManager();
        srcMan->startTimers();
        srcMan->doUpdate();
    }
    Weather *weatherDat = new Weather(gContext->GetMainWindow(), srcMan, "weather");
    weatherDat->exec();
    delete weatherDat;
    gContext->removeCurrentLocation();
    if (!gContext->GetNumSetting("weatherbackgroundfetch",0)) {
        delete srcMan;
        srcMan = 0;
    } 
}

int mythplugin_run(void)
{
    runWeather();
    return 0;
}

void WeatherCallback(void* data, QString &selection) {
    (void) data;
    if (selection == "SETTINGS_GENERAL") {
        GlobalSetup gsetup(gContext->GetMainWindow());
        gsetup.exec();
    } else if (selection == "SETTINGS_SCREEN") {
        if (!srcMan) {
            srcMan = new SourceManager();
        }
        srcMan->clearSources();
        srcMan->findScripts();
        ScreenSetup ssetup(gContext->GetMainWindow(), srcMan);
        ssetup.exec();
        if (gContext->GetNumSetting("weatherbackgroundfetch",0)) {
            if (!srcMan) 
                srcMan = new SourceManager();
            else {
                srcMan->clearSources();
                srcMan->findScriptsDB();
                srcMan->setupSources();
            }

            srcMan->startTimers();
            srcMan->doUpdate();
        } else {
            if (srcMan) {
                delete srcMan;
                srcMan = 0;
            }
        } 
    } else if (selection == "SETTINGS_SOURCE") {
        SourceSetup srcsetup(gContext->GetMainWindow());
		if (srcsetup.loadData()) {
			srcsetup.exec();
		} else {
			MythPopupBox::showOkPopup(gContext->GetMainWindow(), "no sources", 
					QObject::tr("No Sources defined, Sources are defined by"
						" adding screens in Screen Setup."));

		}
    }

}

int mythplugin_config(void)
{
    MythThemedMenu *menu = new MythThemedMenu(gContext->GetThemeDir().ascii(), "weather_settings.xml",
            gContext->GetMainWindow()->GetMainStack(), "weather menu");
    menu->setCallback(WeatherCallback, 0);
    menu->setKillable();
    if (menu->foundTheme()) {
        if (class LCD * lcd = LCD::Get())
            lcd->switchToTime();
        
        GetMythMainWindow()->GetMainStack()->AddScreen(menu);
    }
    else
        cerr << "Couldn't find theme weather_settings.xml" << endl;

    return 0;
}

void  mythplugin_destroy(void) {
    if (srcMan) {
        delete srcMan;
        srcMan = 0;
    } 
}
