#include <qapplication.h>
#include <qsqldatabase.h>
#include <qsqlquery.h>
#include <qstring.h>
#include <qdir.h>
#include <qfile.h>
#include <qstringlist.h>

#include <unistd.h>
#include <cstdio>
#include <fcntl.h>
#include <cstdlib>
#include <sys/ioctl.h>
#include <sys/types.h>

#include <iostream>

#include "libmyth/mythcontext.h"
#include "libmythtv/videosource.h"
#include "libmythtv/channeleditor.h"
#include "libmyth/themedmenu.h"
#include "backendsettings.h"

#include "libmythtv/dbcheck.h"

using namespace std;

QSqlDatabase* db;

QString getResponse(const QString &query, const QString &def)
{
    cout << query;

    if (def != "")
    {
        cout << " [" << def << "]  ";
    }
    
    char response[80];
    cin.getline(response, 80);

    QString qresponse = response;

    if (qresponse == "")
        qresponse = def;

    return qresponse;
}

void clearCardDB(void)
{
    QSqlQuery query;

    query.exec("DELETE FROM capturecard;");
    query.exec("DELETE FROM cardinput;");
}

void clearAllDB(void)
{
    QSqlQuery query;

    query.exec("DELETE FROM channel;");
    query.exec("DELETE FROM program;");
    query.exec("DELETE FROM videosource;");
    query.exec("DELETE FROM credits;");
    query.exec("DELETE FROM programrating;");
}

void SetupMenuCallback(void* data, QString& selection) {
    (void)data;

    QString sel = selection.lower();

    if (sel == "general") {
        BackendSettings be;
        be.exec(db);
    } else if (sel == "capture cards") {
        CaptureCardEditor cce(db);
        cce.exec(db);
    } else if (sel == "video sources") {
        VideoSourceEditor vse(db);
        vse.exec(db);
    } else if (sel == "card inputs") {
        CardInputEditor cie(db);
        cie.exec(db);
    } else if (sel == "channel editor") {
        ChannelEditor ce;
        ce.exec(db);
    }
}

void SetupMenu(void) 
{
    QString theme = gContext->GetSetting("Theme", "blue");

    ThemedMenu* menu = new ThemedMenu(gContext->FindThemeDir(theme),
                                      "setup.xml", gContext->GetMainWindow(),
                                      false);

    menu->setCallback(SetupMenuCallback, gContext);
    menu->setKillable();

    if (menu->foundTheme()) {
            menu->Show();
            menu->exec();
    } else {
        cerr << "Couldn't find theme " << theme << endl;
    }
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    gContext = new MythContext(MYTH_BINARY_VERSION, true);

    db = QSqlDatabase::addDatabase("QMYSQL3");
    if (!gContext->OpenDatabase(db))
    {
        cerr << "Unable to open database:\n"
             << "Driver error was:" << endl
             << db->lastError().driverText() << endl
             << "Database error was:" << endl
             << db->lastError().databaseText() << endl;

        return -1;
    }

    UpgradeTVDatabaseSchema();

    gContext->SetSetting("Theme", "blue");
    gContext->LoadQtConfig();

    QString fileprefix = QDir::homeDirPath() + "/.mythtv";

    QDir dir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);

    QString response = getResponse("Would you like to clear all capture card\n"
                                   "settings before starting configuration?", 
                                   "no");

    if (response.left(1).lower() == "y")
        clearCardDB();

    response = getResponse("Would you like to clear all program/channel\n"
                           "settings before starting configuration?",
                           "no");

    if (response.left(1).lower() == "y")
        clearAllDB();

    MythMainWindow *mainWindow = new MythMainWindow();
    mainWindow->Show();
    gContext->SetMainWindow(mainWindow);

    SetupMenu();

    cout << "If this is the master backend server:\n";
    cout << "Now, please run 'mythfilldatabase' to populate the database\n";
    cout << "with channel information.\n";
    cout << endl;

    return 0;
}
