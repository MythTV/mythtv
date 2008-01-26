/*
 * main.cpp
 *
 * Copyright (C) 2003
 *
 * Author:
 * - Philippe Cattin <cattin@vision.ee.ethz.ch>
 *
 * Bugfixes from:
 *
 * Translations by:
 *
 */
#include <stdlib.h>
#include <kapplication.h>
#include <kcmdlineargs.h>
#include <klocale.h>
#include "tabview.h"

#include "mythtv/exitcodes.h"
#include "mythtv/mythcontext.h"
#include "mythtv/mythdbcon.h"
#include "mythtv/langsettings.h"


static const char *version = "v0.32";

static KCmdLineOptions options[] = {
    { "zoom ",0,0},
    { "z ","Zoom factor 20-300 (default: 200)", "200"},
    { "x ","X position of window (default: 0)", "0"},
    { "y ","Y position of window (default: 0)", "0"},
    { "w ","Screen width (default: physical screen width)", 0 },
    { "h ","Screen height (default: physical screen height)", 0 },
    { "g" ,"Run in windowed mode", 0 },
    { "+file ","URLs to display", 0 },
    KCmdLineLastOption
};

void setupKeys(void)
{
    REG_KEY("Browser", "NEXTTAB", "Move to next browser tab", "P");
    REG_KEY("Browser", "PREVTAB", "Move to previous browser tab", "");
    REG_KEY("Browser", "DELETETAB", "Delete the current browser tab", "D");
    
    REG_KEY("Browser", "ZOOMIN", "Zoom in on browser window", ".,>");
    REG_KEY("Browser", "ZOOMOUT", "Zoom out on browser window", ",,<");
    REG_KEY("Browser", "TOGGLEINPUT", "Toggle where keyboard input goes to", "F1");
    
    REG_KEY("Browser", "MOUSEUP", "Move mouse pointer up", "2");
    REG_KEY("Browser", "MOUSEDOWN", "Move mouse pointer down", "8");
    REG_KEY("Browser", "MOUSELEFT", "Move mouse pointer left", "4");
    REG_KEY("Browser", "MOUSERIGHT", "Move mouse pointer right", "6");
    REG_KEY("Browser", "MOUSELEFTBUTTON", "Mouse Left button click", "5");

    REG_KEY("Browser", "PAGEDOWN", "Scroll down half a page", "9");
    REG_KEY("Browser", "PAGEUP", "Scroll up half a page", "3");
    REG_KEY("Browser", "PAGELEFT", "Scroll left half a page", "7");
    REG_KEY("Browser", "PAGERIGHT", "Scroll right half a page", "1");
    
    REG_KEY("Browser", "NEXTLINK", "Move selection to next link", "Z");
    REG_KEY("Browser", "PREVIOUSLINK", "Move selection to previous link", "Q");
    REG_KEY("Browser", "FOLLOWLINK", "Follow selected link", "Return,Space,Enter");
    REG_KEY("Browser", "BACK", "Go back to previous page", "R,Backspace");
}

int main(int argc, char **argv)
{
    bool windowed = false;
    int x = 0, y = 0;
    float xm = 0, ym = 0;
    int zoom,width=-1,height=-1;    // defaults
    char usage[] = "Usage: mythbrowser [-z n] [-g] [-x n] [-y n] [-w n] [-h n] -u URL [URL]";
    QStringList urls;

    KCmdLineArgs::init(argc, argv, "mythbrowser", "mythbrowser", usage , version);
    KCmdLineArgs::addCmdLineOptions(options);
    KCmdLineArgs *args = KCmdLineArgs::parsedArgs();
    zoom = args->getOption("z").toInt();
    if (args->isSet("w"))
        width = args->getOption("w").toInt();
    if (args->isSet("h"))
        height = args->getOption("h").toInt();
    if (args->isSet("x"))
        x = args->getOption("x").toInt();
    if (args->isSet("y"))
        y = args->getOption("y").toInt();
    if (args->isSet("g"))
        windowed = true;

    for (int i=0;i<args->count();i++) 
    {
        urls += args->arg(i);
    }
    args->clear();

    if (urls.count() == 0)
        urls += "http://www.mythtv.org";

    KApplication a(argc,argv);

    gContext = NULL;
    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init(true))
    {
        VERBOSE(VB_IMPORTANT, "MythBrowser: Could not initialize myth context. Exiting.");
        return FRONTEND_EXIT_NO_MYTHCONTEXT;
    }

    if (width == -1 || height == -1)
    {
        // Obtain width/height and x/y offset from context
        gContext->GetScreenSettings(x, width, xm, y, height, ym);
    }

    gContext->ParseGeometryOverride(QString("%1x%2+%3+%4")
            .arg(width).arg(height).arg(x).arg(y));

    if (windowed)
        gContext->OverrideSettingForSession("RunFrontendInWindow", "1");

    if (!MSqlQuery::testDBConnection())
    {
        VERBOSE(VB_IMPORTANT, "MythBrowser: Couldn't open db. Exiting.");
        return FRONTEND_EXIT_DB_ERROR;
    }

    LanguageSettings::load("mythbrowser");

    gContext->LoadQtConfig();
    setupKeys();

    MythMainWindow *mainWindow = GetMythMainWindow();
    mainWindow->Init();
    gContext->SetMainWindow(mainWindow);

    TabView *tabView = 
            new TabView(mainWindow, "mythbrowser", urls, zoom, width, height,
                        Qt::WStyle_Customize | Qt::WStyle_NoBorder);
    tabView->exec();

    return 0;
}
