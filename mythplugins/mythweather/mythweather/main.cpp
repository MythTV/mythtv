
#include <unistd.h>

// QT headers
#include <QApplication>
#include <QDir>

// MythTV headers
#include <lcddevice.h>
#include <mythcontext.h>
#include <mythplugin.h>
#include <mythpluginapi.h>
#include <mythversion.h>
#include <myththemedmenu.h>
#include <mythmainwindow.h>
#include <mythuihelper.h>

// MythWeather headers
#include "weather.h"
#include "weatherSetup.h"
#include "sourceManager.h"
#include "dbcheck.h"

SourceManager *srcMan = 0;

static int RunWeather()
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    Weather *weather = new Weather(mainStack, "mythweather", srcMan);

    if (weather->Create())
    {
        if( weather->SetupScreens() )
            mainStack->AddScreen(weather);

        return 0;
    }

    delete weather;
    return -1;
}

static void runWeather()
{
    RunWeather();
}

static void setupKeys()
{
    REG_JUMP("MythWeather", QT_TRANSLATE_NOOP("MythControls",
        "Weather forecasts"), "", runWeather);
    REG_KEY("Weather", "PAUSE", QT_TRANSLATE_NOOP("MythControls",
        "Pause current page"), "P");
    REG_KEY("Weather", "SEARCH", QT_TRANSLATE_NOOP("MythControls",
        "Search List"), "/");
    REG_KEY("Weather", "NEXTSEARCH", QT_TRANSLATE_NOOP("MythControls",
        "Search List"), "n");
    REG_KEY("Weather", "UPDATE", QT_TRANSLATE_NOOP("MythControls",
        "Search List"), "u");
}

int mythplugin_init(const char *libversion)
{
    if (!gCoreContext->TestPluginVersion("mythweather", libversion,
                                    MYTH_BINARY_VERSION))
        return -1;

    gCoreContext->ActivateSettingsCache(false);
    InitializeDatabase();
    gCoreContext->ActivateSettingsCache(true);

    setupKeys();

    if (gCoreContext->GetNumSetting("weatherbackgroundfetch", 0))
    {
        srcMan = new SourceManager();
        srcMan->startTimers();
        srcMan->doUpdate();
    }

    return 0;
}

int mythplugin_run()
{
    return RunWeather();
}

static void WeatherCallback(void *data, QString &selection)
{
    (void) data;

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    if (selection == "SETTINGS_GENERAL")
    {
        GlobalSetup *gsetup = new GlobalSetup(mainStack, "weatherglobalsetup");

        if (gsetup->Create())
            mainStack->AddScreen(gsetup);
        else
            delete gsetup;
    }
    else if (selection == "SETTINGS_SCREEN")
    {
        ScreenSetup *ssetup = new ScreenSetup(mainStack, "weatherscreensetup", srcMan);

        if (ssetup->Create())
            mainStack->AddScreen(ssetup);
        else
            delete ssetup;
    }
    else if (selection == "SETTINGS_SOURCE")
    {
        SourceSetup *srcsetup = new SourceSetup(mainStack, "weathersourcesetup");

        if (srcsetup->Create())
            mainStack->AddScreen(srcsetup);
        else
            delete srcsetup;
    }
}

int mythplugin_config()
{
    QString menuname = "weather_settings.xml";
    QString themedir = GetMythUI()->GetThemeDir();

    MythThemedMenu *menu = new MythThemedMenu(
        themedir, menuname,
        GetMythMainWindow()->GetMainStack(), "weather menu");

    menu->setCallback(WeatherCallback, 0);
    menu->setKillable();
    if (menu->foundTheme())
    {
        if (LCD *lcd = LCD::Get()) {
            lcd->setFunctionLEDs(FUNC_NEWS, false);
            lcd->switchToTime();
        }

        GetMythMainWindow()->GetMainStack()->AddScreen(menu);
        return 0;
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Couldn't find menu %1 or theme %2")
                .arg(menuname).arg(themedir));
        delete menu;
        return -1;
    }
}

void  mythplugin_destroy()
{
    if (srcMan)
    {
        delete srcMan;
        srcMan = 0;
    }
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
