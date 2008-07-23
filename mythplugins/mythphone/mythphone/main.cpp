/*
	main.cpp

	(c) 2003 Paul Volkaerts
	
	Starting point for the mythPhone module

*/

#include <iostream>
#include <cstdlib>
using namespace std;

#include <qapplication.h>
#include <qdir.h>
#include <qmutex.h>
#include <qregexp.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <net/if.h>
#include <linux/videodev.h>
#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>
#include <mythtv/mythdb.h>
#include <mythtv/mythplugin.h>
#include <mythtv/dialogbox.h>
#include <mythtv/util.h>
#include <mythtv/mythpluginapi.h>
#include <mythtv/mythdirs.h>

#include "config.h"
#include "dbcheck.h"
#include "PhoneSettings.h"
#include "webcam.h"
//#include "webcam_set.h"
#include "sipfsm.h"
#include "phoneui.h"
#include "sipfsm.h"

SipContainer *sipStack;


void PhoneUI(void)
{
    PhoneUIBox *puib;

    puib = new PhoneUIBox(gContext->GetMainWindow(),
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

    wsb = new WebcamSettingsBox(gContext->GetMainWindow(),
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
        settings.exec();
    }
    else if (sel == "webcam_settings")
    {
        startWebcamSettings();
    }

}

void runMenu(QString which_menu)
{
    QString themedir = gContext->GetThemeDir();

    ThemedMenu *diag = new ThemedMenu(themedir, which_menu, 
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
        cerr << "Couldn't find theme " << themedir.toLocal8Bit().constData()
             << endl;
    }

    delete diag;
}
#endif

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
    REG_KEY("Phone", "VOLUMEDOWN", "Volume down", "[,{,F10,Volume Down");
    REG_KEY("Phone", "VOLUMEUP",   "Volume up",   "],},F11,Volume Up");
    REG_KEY("Phone", "MUTE",       "Mute",        "|,\\,F9,Volume Mute");
    REG_KEY("Phone", "ZOOMIN", "Zoom the video window in", ">,.,Z,End");
    REG_KEY("Phone", "ZOOMOUT", "Zoom the video window out", ",,<,Q,Home");
    REG_KEY("Phone", "FULLSCRN", "Show received video full-screen", "P");
    REG_KEY("Phone", "HANGUP", "Hangup an active call", "O");
    REG_KEY("Phone", "LOOPBACK", "Loopback Video", "L");
}

QString GetMySipIp()
{
    Q3SocketDevice *tempSocket = new Q3SocketDevice (Q3SocketDevice::Datagram);
    QString ifName = gContext->GetSetting("SipBindInterface");
    struct ifreq ifreq;
    strcpy(ifreq.ifr_name, ifName);
    if (ioctl(tempSocket->socket(), SIOCGIFADDR, &ifreq) != 0)
    {
        VERBOSE(VB_IMPORTANT, QString("Failed to find network interface %1")
             .arg(ifName.toLocal8Bit().constData()));
        delete tempSocket;
        tempSocket = 0;
        return "";
    }
    delete tempSocket;
    tempSocket = 0;
    struct sockaddr_in * sptr = (struct sockaddr_in *)&ifreq.ifr_addr;
    QHostAddress myIP;
    myIP.setAddress(htonl(sptr->sin_addr.s_addr));
    return myIP.toString();
}

void addMyselfToDirectory()
{
char myHostname[64];

    // Create an automatic new entry in the MythPhone Directory for this Frontend to assist in
    // calling between Frontends.

    if (gethostname(myHostname, sizeof(myHostname)) == -1)
        myHostname[0] = 0;
    QString Dir       = "My MythTVs";
    QString Surname   = myHostname;
    QString FirstName = QString("Local Myth Host");
    QString NickName  = QString("%1(%2)")
        .arg(gContext->GetSetting("MySipName")).arg(myHostname);
    QString Uri;
    if (gContext->GetNumSetting("SipRegisterWithProxy", 1))
        Uri = gContext->GetSetting("SipProxyAuthName");
    else
    {
        Uri = "MythPhone@" + GetMySipIp();
        int     myPort    = atoi((const char *)gContext->GetSetting("SipLocalPort"));
        if (myPort != 5060)
            Uri += ":" + QString::number(myPort);
    }


    // First check if an entry already exists;
    // and if it is up-to-date (IP address etc)
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT intid, nickname, url "
        "FROM phonedirectory "
        "WHERE directory = :DIRECTORY AND "
        "      firstname = :FIRSTNAME AND "
        "      surname   = :SURNAME");
    query.bindValue(":DIRECTORY", Dir);
    query.bindValue(":FIRSTNAME", FirstName);
    query.bindValue(":SURNAME",   Surname);

    if (!query.exec() || !query.next())
    {
        VERBOSE(VB_IMPORTANT,
                "SIP: Creating autogen directory entry for this host");

        query.prepare(
            "INSERT INTO phonedirectory "
            " ( nickname,   firstname,  surname,    url, "
            "   directory,  photofile,  speeddial,  onhomelan ) "
            "VALUES "
            " (:NICKNAME,  :FIRSTNAME, :SURNAME,  :URL, "
            "  :DIRECTORY,  '',         '1',       '1')");
        query.bindValue(":NICKNAME",  NickName);
        query.bindValue(":FIRSTNAME", FirstName);
        query.bindValue(":SURNAME",   Surname);
        query.bindValue(":URL",       Uri);
        query.bindValue(":DIRECTORY", Dir);

        if (!query.exec())
        {
            MythDB::DBError("addMyselfToDirectory() 1", query);
        }
    }
    else
    {
        do
        {
            // See if the user has changed their display name
            // or the IP address has changed

            if ((query.value(1).toString() != NickName) || 
                (query.value(2).toString() != Uri))
            {
                VERBOSE(VB_IMPORTANT, QString(
                            "SIP: Updating out-of-date "
                            "autogen directory entry: %1, %2")
                        .arg(query.value(1).toString())
                        .arg(query.value(2).toString()));

                MSqlQuery query2(MSqlQuery::InitCon());
                query2.prepare(
                    "UPDATE phonedirectory "
                    "SET nickname = :NICKNAME, "
                    "    url      = :URL "
                    "WHERE intid = :INTID");
                query2.bindValue(":NICKNAME", NickName);
                query2.bindValue(":URL",      Uri);
                query2.bindValue(":INTID",    query.value(0).toUInt());
                if (!query2.exec())
                {
                    MythDB::DBError("addMyselfToDirectory() 2", query2);
                }
            }
        }
        while (query.next());
    }
}

int mythplugin_init(const char *libversion)
{
    if (!gContext->TestPopupVersion("mythphone", libversion,
                                    MYTH_BINARY_VERSION))
    {
        VERBOSE(VB_IMPORTANT, "Test Popup Version Failed");
        return -1;
    }

    gContext->ActivateSettingsCache(false);
    if (!UpgradePhoneDatabaseSchema())
    {
        VERBOSE(VB_IMPORTANT,
                "Couldn't upgrade database to new schema, exiting.");
        return -1;
    }
    gContext->ActivateSettingsCache(true);

    MythPhoneSettings mpSettings;
    mpSettings.Load();
    mpSettings.Save();

    // Make sure all the required directories exist
    QString dirName = GetConfDir();
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

    addMyselfToDirectory();

    SipFsm *sipFsm = new SipFsm();
    sipStack = new SipContainer(sipFsm);

    return 0;
}

int mythplugin_run(void)
{
    PhoneUI();

    return 0;
}

int mythplugin_config(void)
{
    // Was several options under here; and probably will be again so just comment this out for now
    //runMenu("phone_settings.xml");
    MythPhoneSettings settings;
    settings.exec();

    return 0;
}

void mythplugin_destroy(void)
{
    delete sipStack;
    sipStack = NULL;
}

