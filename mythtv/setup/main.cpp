#include <qapplication.h>
#include <qsqldatabase.h>
#include <qsqlquery.h>
#include <qstring.h>
#include <qdir.h>
#include <qfile.h>
#include <qstringlist.h>

#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <linux/videodev.h>

#include <iostream>

#include "libmyth/mythcontext.h"
#include "libmythtv/videosource.h"
#include "libmyth/themedmenu.h"
#include "backendsettings.h"

using namespace std;

MythContext* context;
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

void clearDB(void)
{
    QSqlQuery query;

    query.exec("DELETE FROM channel;");
    query.exec("DELETE FROM program;");
    query.exec("DELETE FROM singlerecord;");
    query.exec("DELETE FROM timeslotrecord;");
    query.exec("DELETE FROM capturecard;");
    query.exec("DELETE FROM videosource;");
    query.exec("DELETE FROM cardinput;");
}

void SetupMenuCallback(void* data, QString& selection) {
    MythContext* context = (MythContext*)data;
    QString sel = selection.lower();

    if (sel == "general") {
        BackendSettings be;
        be.exec(context, db);
    } else if (sel == "capture cards") {
        CaptureCardEditor cce(db);
        cce.exec(context, db);
    } else if (sel == "video sources") {
        VideoSourceEditor vse(db);
        vse.exec(context, db);
    } else if (sel == "card inputs") {
        CardInputEditor cie(db);
        cie.exec(context, db);
    }
}

void SetupMenu(MythContext* context) {
    QString theme = context->GetSetting("Theme");

    ThemedMenu* menu = new ThemedMenu(context,
                                      context->FindThemeDir(theme),
                                      "setup.xml");
    menu->setCallback(SetupMenuCallback, context);
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

    context = new MythContext(true);

    db = QSqlDatabase::addDatabase("QMYSQL3");
    if (!context->OpenDatabase(db))
    {
        cerr << "Unable to open database:\n"
             << "Driver error was:" << endl
             << db->lastError().driverText() << endl
             << "Database error was:" << endl
             << db->lastError().databaseText() << endl;

        return -1;
    }

    context->SetSetting("ThemeQt", "1");
    context->SetSetting("Theme", "blue");
    context->LoadQtConfig();

    char *home = getenv("HOME");
    QString fileprefix = QString(home) + "/.mythtv";

    QDir dir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);

    QString response = getResponse("Would you like to clear all program/channel/recording/card\n"
                                   "settings before starting configuration?", "n");
    if (response == "y")
        clearDB();

    SetupMenu(context);

    cout << "Now, please run 'mythfilldatabase' to populate the database\n";
    cout << "with channel information.\n";
    cout << endl;

    return 0;
}
