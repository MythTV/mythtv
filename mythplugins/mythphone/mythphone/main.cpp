/*
	main.cpp

	(c) 2003 Paul Volkaerts
	
	Starting point for the mythPhone module

*/

#include <iostream>
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
#include <mythtv/themedmenu.h>
#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>
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
void mythplugin_destroy(void);
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
    REG_KEY("Phone", "LOOPBACK", "Loopback Video", "L");
}

QString GetMySipIp()
{
    QSocketDevice *tempSocket = new QSocketDevice (QSocketDevice::Datagram);
    QString ifName = gContext->GetSetting("SipBindInterface");
    struct ifreq ifreq;
    strcpy(ifreq.ifr_name, ifName);
    if (ioctl(tempSocket->socket(), SIOCGIFADDR, &ifreq) != 0)
    {
        cerr << "Failed to find network interface " << ifName << endl;
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
QString thequery;
char myHostname[64];

    // Create an automatic new entry in the MythPhone Directory for this Frontend to assist in
    // calling between Frontends.

    if (gethostname(myHostname, sizeof(myHostname)) == -1)
        myHostname[0] = 0;
    QString Dir       = "My MythTVs";
    QString Surname   = myHostname;
    QString FirstName = QString("Local Myth Host");
    QString NickName  = gContext->GetSetting("MySipName") + "(" + myHostname + ")";
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


    // First check if an entry already exists; and if it is up-to-date (IP address etc)
    MSqlQuery query(MSqlQuery::InitCon());
    thequery = QString("SELECT intid,nickname,url "
                       "FROM phonedirectory "
                       "WHERE directory = \"%1\" and "
                       "firstname = \"%2\" and "
                       "surname = \"%3\";")
                       .arg(Dir.latin1()).arg(FirstName.latin1()).arg(myHostname);
    query.exec(thequery);

    if(query.isActive() && query.size() > 0)
    {
        while(query.next())
        {
            // See if the user has changed their display name or the IP address has changed
            if ((query.value(1).toString() != NickName) || 
                (query.value(2).toString() != Uri))
            {
                cout << "SIP: Updating out-of-date autogen directory entry; " << query.value(1).toString() << ", " << query.value(2).toString() << endl;

                MSqlQuery query2(MSqlQuery::InitCon());
                thequery = QString("UPDATE phonedirectory "
                                   "SET nickname=\"%1\", "
                                   "url=\"%2\" "
                                   "WHERE intid=%3 ;")
                           .arg(NickName.latin1()).arg(Uri.latin1()).arg(query.value(0).toInt());
                query2.exec(thequery);
            }
        }
    }
    else
    {
        cout << "SIP: Creating autogen directory entry for this host\n";
        thequery = QString("INSERT INTO phonedirectory (nickname,firstname,surname,"
                               "url,directory,photofile,speeddial,onhomelan) VALUES "
                               "(\"%1\",\"%2\",\"%3\",\"%4\",\"%5\",\"\",1,1);")
                              .arg(NickName.latin1()).arg(FirstName.latin1())
                              .arg(Surname.latin1()).arg(Uri.latin1())
                              .arg(Dir.latin1())
                              ;
        query.exec(thequery);
    }
}

int mythplugin_init(const char *libversion)
{
    if (!gContext->TestPopupVersion("mythphone", libversion,
                                    MYTH_BINARY_VERSION))
    {
        cerr << "Test Popup Version Failed " << endl;
        return -1;
    }

    gContext->ActivateSettingsCache(false);
    UpgradePhoneDatabaseSchema();
    gContext->ActivateSettingsCache(true);

    MythPhoneSettings mpSettings;
    mpSettings.load();
    mpSettings.save();

    // Make sure all the required directories exist
    QString dirName = MythContext::GetConfDir();
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
 
    sipStack = new SipContainer();

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

