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
#include "libmyth/langsettings.h"
#include "libmyth/dialogbox.h"
#include "libmythtv/videosource.h"
#include "libmythtv/channeleditor.h"
#include "libmyth/themedmenu.h"
#include "backendsettings.h"

#include "libmythtv/dbcheck.h"

using namespace std;

QSqlDatabase* db;

void clearCardDB(void)
{
    QSqlQuery query;

    query.exec("TRUNCATE TABLE capturecard;");
    query.exec("TRUNCATE TABLE cardinput;");
    query.exec("TRUNCATE TABLE dvb_sat;");
    query.exec("TRUNCATE TABLE dvb_signal_quality;");
}

void clearAllDB(void)
{
    QSqlQuery query;

    query.exec("TRUNCATE TABLE channel;");
    query.exec("TRUNCATE TABLE program;");
    query.exec("TRUNCATE TABLE videosource;");
    query.exec("TRUNCATE TABLE credits;");
    query.exec("TRUNCATE TABLE programrating;");
    query.exec("TRUNCATE TABLE programgenres;");
    query.exec("TRUNCATE TABLE dvb_channel;");
    query.exec("TRUNCATE TABLE dvb_pids;");
    query.exec("TRUNCATE TABLE cardinput;");
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
#ifdef Q_WS_MACX
    // Without this, we can't set focus to any of the CheckBoxSetting, and most
    // of the MythPushButton widgets, and they don't use the themed background.
    QApplication::setDesktopSettingsAware(FALSE);
#endif
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

    LanguageSettings::load("mythfrontend");

    gContext->SetSetting("Theme", "blue");
    gContext->LoadQtConfig();

    QString fileprefix = MythContext::GetConfDir();

    QDir dir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);

    MythMainWindow *mainWindow = new MythMainWindow();
    mainWindow->Show();
    gContext->SetMainWindow(mainWindow);
    LanguageSettings::prompt();

    DialogBox dboxCard(mainWindow, QObject::tr("Would you like to clear all "
                                   "capture card settings before starting "
                                   "configuration?"));
    dboxCard.AddButton(QObject::tr("No, leave my card settings alone"));
    dboxCard.AddButton(QObject::tr("Yes, delete my card settings"));
    if (dboxCard.exec() == 2)
        clearCardDB();
    
    // Give the user time to realize the first dialog is gone
    // before we bring up a similar-looking one
    usleep(750000);
    
    DialogBox dboxProg(mainWindow, QObject::tr("Would you like to clear all "
                                   "program data and channel settings before "
                                   "starting configuration? This will not "
                                   "affect any existing recordings."));
    dboxProg.AddButton(QObject::tr("No, leave my channel settings alone"));
    dboxProg.AddButton(QObject::tr("Yes, delete my channel settings"));
    if (dboxProg.exec() == 2)
        clearAllDB();

    REG_KEY("qt", "DELETE", "Delete", "D");
    REG_KEY("qt", "EDIT", "Edit", "E");

    SetupMenu();

    cout << "If this is the master backend server:\n";
    cout << "Now, please run 'mythfilldatabase' to populate the database\n";
    cout << "with channel information.\n";
    cout << endl;

    return 0;
}
