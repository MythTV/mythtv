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
#include <qsqldatabase.h>
#include <qsqlquery.h>
#include "tabview.h"

#include "mythtv/mythcontext.h"
#include "mythtv/langsettings.h"


static const char *version = "v0.31";

static KCmdLineOptions options[] = {
    { "zoom ",0,0},
    { "z ","Zoom factor 20-300 (default: 200)", "200"},
    { "w ","Screen width (default: physical screen width)", 0 },
    { "h ","Screen height (default: physical screen height)", 0 },
    { "+file ","URLs to display", 0 },
    KCmdLineLastOption
};


QSqlDatabase* db;

int main(int argc, char **argv)
{
    int x = 0, y = 0;
    float xm = 0, ym = 0;
    int zoom,width=-1,height=-1;    // defaults
    char usage[] = "Usage: mythbrowser [-z n] [-w n] [-h n] -u URL [URL]";
    QStringList urls;

    KCmdLineArgs::init(argc, argv, "mythbrowser", usage , version);
    KCmdLineArgs::addCmdLineOptions(options);
    KCmdLineArgs *args = KCmdLineArgs::parsedArgs();
    zoom = args->getOption("z").toInt();
    if(args->isSet("w"))
        width = args->getOption("w").toInt();
    if(args->isSet("h"))
        height = args->getOption("h").toInt();
    for(int i=0;i<args->count();i++) {
        urls += args->arg(i);
     }
    args->clear();

    KApplication a(argc,argv);
    if(width == -1)
        width = a.desktop()->width();
    if(height == -1)
        height = a.desktop()->height();

    gContext = new MythContext(MYTH_BINARY_VERSION, true);
    db = QSqlDatabase::addDatabase("QMYSQL3");
    if (!gContext->OpenDatabase(db)) {
        cerr << "Unable to open database:\n"
             << "Driver error was:" << endl
             << db->lastError().driverText() << endl
             << "Database error was:" << endl
             << db->lastError().databaseText() << endl;
        return -1;
    }

    LanguageSettings::load("mythbrowser");

    gContext->SetSetting("Theme", "blue");
    gContext->LoadQtConfig();

//    MythMainWindow *mainWindow = new MythMainWindow();
//    mainWindow->Show();
//    gContext->SetMainWindow(mainWindow);
//    a.setMainWidget(mainWindow);

    // Obtain width/height and x/y offset from context
    gContext->GetScreenSettings(x, width, xm, y, height, ym);

    TabView *mainWindow = 
        new TabView(db, urls, zoom, width, height, Qt::WStyle_Customize | Qt::WStyle_NoBorder);
    gContext->SetMainWindow(mainWindow);
    a.setMainWidget(mainWindow);
    mainWindow->setGeometry(x, y, width, height);
    mainWindow->setFixedSize(QSize(width, height));
    mainWindow->Show();

    return a.exec();
}
