/*
	main.cpp

	(c) 2003 Paul Volkaerts
	
	Starting point for the mythPhone module

*/

#include <iostream>
using namespace std;

#include <qapplication.h>
#include <qsqldatabase.h>
#include <qdir.h>
#include <qmutex.h>
#include <qregexp.h>
#include <linux/videodev.h>
#include <mythtv/themedmenu.h>
#include <mythtv/mythcontext.h>
#include <mythtv/mythplugin.h>
#include <mythtv/dialogbox.h>
#include <mythtv/util.h>

#include "config.h"
#include "dbcheck.h"
#include "PhoneSettings.h"
#include "webcam.h"
//#include "webcam_set.h"
#include "sipfsm.h"
#include "phoneui.h"

SipContainer *sipStack;


void PhoneUI(void)
{
    PhoneUIBox *puib;

    puib = new PhoneUIBox(QSqlDatabase::database(),
                                gContext->GetMainWindow(),
                                "phone_ui", "phone-");
    qApp->unlock();
    puib->exec();
    qApp->lock();
    qApp->processEvents();

    delete puib;
}


#if 0  // Removed web-cam settings screen and pushed all config into one set of screens. Leave here in case I change my mind
void startWebcamSettings(void)
{
    WebcamSettingsBox *wsb;

    wsb = new WebcamSettingsBox(QSqlDatabase::database(),
                                gContext->GetMainWindow(),
                                "webcam_settings", "webcam-");
    qApp->unlock();
    wsb->exec();
    qApp->lock();
    qApp->processEvents();

    delete wsb;
}

void PhoneCallback(void *data, QString &selection)
{
    (void)data;
    QString sel = selection.lower();

    if (sel == "phone_settings_general")
    {
        MythPhoneSettings settings;
        settings.exec(QSqlDatabase::database());
    }
    else if (sel == "webcam_settings")
    {
        startWebcamSettings();
    }

}

void runMenu(QString which_menu)
{
    QString themedir = gContext->GetThemeDir();

    ThemedMenu *diag = new ThemedMenu(themedir.ascii(), which_menu, 
                                      gContext->GetMainWindow(), "phone menu");

    diag->setCallback(PhoneCallback, NULL);
    diag->setKillable();

    if (diag->foundTheme())
    {
        gContext->GetLCDDevice()->switchToTime();
        diag->exec();
    }
    else
    {
        cerr << "Couldn't find theme " << themedir << endl;
    }

    delete diag;
}
#endif


extern "C" {
int mythplugin_init(const char *libversion);
int mythplugin_run(void);
int mythplugin_config(void);
}


void initKeys(void)
{
    REG_KEY("Phone", "0", "0", "0");
    REG_KEY("Phone", "1", "1", "1");
    REG_KEY("Phone", "2", "2", "2");
    REG_KEY("Phone", "3", "3", "3");
    REG_KEY("Phone", "4", "4", "4");
    REG_KEY("Phone", "5", "5", "5");
    REG_KEY("Phone", "6", "6", "6");
    REG_KEY("Phone", "7", "7", "7");
    REG_KEY("Phone", "8", "8", "8");
    REG_KEY("Phone", "9", "9", "9");
    REG_KEY("Phone", "HASH", "HASH", "#");
    REG_KEY("Phone", "STAR", "STAR", "*");
    REG_KEY("Phone", "Up", "Up", "Up");
    REG_KEY("Phone", "Down", "Down", "Down");
    REG_KEY("Phone", "Left", "Left", "Left");
    REG_KEY("Phone", "Right", "Right", "Right");
    REG_KEY("Phone", "VOLUMEDOWN", "Volume down", "[,{,F10");
    REG_KEY("Phone", "VOLUMEUP", "Volume up", "],},F11");
    REG_KEY("Phone", "ZOOMIN", "Zoom the video window in", ">,.,Z,End");
    REG_KEY("Phone", "ZOOMOUT", "Zoom the video window out", ",,<,Q,Home");
    REG_KEY("Phone", "FULLSCRN", "Show received video full-screen", "P");
    REG_KEY("Phone", "HANGUP", "Hangup an active call", "O");
    REG_KEY("Phone", "MUTE", "Mute", "|,\\,F9");
}

int mythplugin_init(const char *libversion)
{
    if (!gContext->TestPopupVersion("mythphone", libversion,
                                    MYTH_BINARY_VERSION))
    {
        cerr << "Test Popup Version Failed " << endl;
        return -1;
    }

    UpgradePhoneDatabaseSchema();

    MythPhoneSettings mpSettings;
    mpSettings.load(QSqlDatabase::database());
    mpSettings.save(QSqlDatabase::database());

    // Make sure all the required directories exist
    char *homeDir = getenv("HOME");
    QString dirName = QString(homeDir) + "/.mythtv";
    QDir dir(dirName);
    if (!dir.exists())
        dir.mkdir(dirName);
    dirName += "/MythPhone";
    dir = QDir(dirName);
    if (!dir.exists())
        dir.mkdir(dirName);
    QString vmName = dirName + "/Voicemail";
    dir = QDir(vmName);
    if (!dir.exists())
        dir.mkdir(vmName);
    QString phName = dirName + "/Photos";
    dir = QDir(phName);
    if (!dir.exists())
        dir.mkdir(phName);

    initKeys();

    sipStack = new SipContainer();

    return 0;
}

int mythplugin_run(void)
{
    QTranslator translator( 0 );
    translator.load(PREFIX + QString("/share/mythtv/i18n/mythphone_") +
                    QString(gContext->GetSetting("Language").lower()) +
                    QString(".qm"), ".");
    qApp->installTranslator(&translator);

    PhoneUI();

    qApp->removeTranslator(&translator);

    return 0;
}

int mythplugin_config(void)
{
    QTranslator translator( 0 );
    translator.load(PREFIX + QString("/share/mythtv/i18n/mythphone_") +
                    QString(gContext->GetSetting("Language").lower()) +
                    QString(".qm"), ".");
    qApp->installTranslator(&translator);


    // Was several options under here; and probably will be again so just comment this out for now
    //runMenu("phone_settings.xml");
    MythPhoneSettings settings;
    settings.exec(QSqlDatabase::database());

    qApp->removeTranslator(&translator);

    return 0;
}

