// QT headers
#include <qapplication.h>

#include <unistd.h>

// MythTV headers
#include <mythtv/lcddevice.h>
#include <mythtv/mythcontext.h>
#include <mythtv/mythplugin.h>
#include <mythtv/mythpluginapi.h>
#include <mythtv/libmythui/myththemedmenu.h>
#include <mythtv/libmythui/mythmainwindow.h>

// MythWeather headers
#include "weather.h"
#include "weatherSetup.h"
#include "sourceManager.h"
#include "dbcheck.h"

SourceManager *srcMan = 0;

void runWeather();

void setupKeys()
{
    REG_JUMP("MythWeather", "Weather forecasts", "", runWeather);
    REG_KEY("Weather", "PAUSE", "Pause current page", "P");
    REG_KEY("Weather", "SEARCH", "Search List", "/");
    REG_KEY("Weather", "NEXTSEARCH", "Search List", "n");
    REG_KEY("Weather", "UPDATE", "Search List", "u");
    REG_KEY("Weather", "DELETE", "Delete screen from list", "D");
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

    if (gContext->GetNumSetting("weatherbackgroundfetch", 0))
    {
        srcMan = new SourceManager();
        srcMan->startTimers();
        srcMan->doUpdate();
    }

    return 0;
}

void runWeather()
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    Weather *weather = new Weather(mainStack, "mythweather", srcMan);

    if (weather->Create())
        mainStack->AddScreen(weather);
}

int mythplugin_run()
{
    runWeather();
    return 0;
}

void WeatherCallback(void *data, QString &selection)
{
    (void) data;

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    if (selection == "SETTINGS_GENERAL")
    {
        GlobalSetup *gsetup = new GlobalSetup(mainStack, "weatherglobalsetup");

        if (gsetup->Create())
            mainStack->AddScreen(gsetup);
    }
    else if (selection == "SETTINGS_SCREEN")
    {
        if (!srcMan)
        {
            srcMan = new SourceManager();
        }
        srcMan->clearSources();
        srcMan->findScripts();

        ScreenSetup *ssetup = new ScreenSetup(mainStack, "weatherscreensetup", srcMan);

        if (ssetup->Create())
            mainStack->AddScreen(ssetup);
    }
    else if (selection == "SETTINGS_SOURCE")
    {
        SourceSetup *srcsetup = new SourceSetup(mainStack, "weathersourcesetup");

        if (srcsetup->Create())
            mainStack->AddScreen(srcsetup);
//             MythPopupBox::showOkPopup(gContext->GetMainWindow(), "no sources",
//                     QObject::tr("No Sources defined, Sources are defined by"
//                                 " adding screens in Screen Setup."));
    }
}

int mythplugin_config()
{
    MythThemedMenu *menu =
            new MythThemedMenu(gContext->GetThemeDir().ascii(),
                               "weather_settings.xml",
                               gContext->GetMainWindow()->GetMainStack(),
                               "weather menu");
    menu->setCallback(WeatherCallback, 0);
    menu->setKillable();
    if (menu->foundTheme())
    {
        if (LCD *lcd = LCD::Get())
            lcd->switchToTime();

        GetMythMainWindow()->GetMainStack()->AddScreen(menu);
    }
    else
        VERBOSE(VB_IMPORTANT, "Couldn't find theme weather_settings.xml");

    return 0;
}

void  mythplugin_destroy()
{
    if (srcMan)
    {
        delete srcMan;
        srcMan = 0;
    }
}
