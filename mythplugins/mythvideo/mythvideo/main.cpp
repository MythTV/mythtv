#include <qdir.h>
#include <iostream>
using namespace std;

#include <qapplication.h>
#include <unistd.h>
#include <qsocketnotifier.h>
#include <qtextcodec.h>
#include <qregexp.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>

#include "metadata.h"
#include "videomanager.h"
#include "videobrowser.h"
#include "videotree.h"
#include "videogallery.h"
#include "videofilter.h"
#include "globalsettings.h"
#include "fileassoc.h"
#include "dbcheck.h"
#include "videoscan.h"

#include <mythtv/themedmenu.h>
#include <mythtv/mythcontext.h>
#include <mythtv/mythplugin.h>
#include <mythtv/lcddevice.h>
#include <mythtv/mythdbcon.h>



void runMenu(QString, const QString &);
void VideoCallback(void *, QString &);
void SearchDir(QString &);


extern "C" {
int mythplugin_init(const char *libversion);
int mythplugin_run(void);
int mythplugin_config(void);
}

void runVideoManager(void);
void runVideoBrowser(void);
void runVideoTree(void);
void runVideoGallery(void);
void runDefaultView(void);


void setupKeys(void)
{
    REG_JUMP("MythVideo", "The MythVideo default view", "", 
             runDefaultView);
             
    REG_JUMP("Video Manager", "The MythVideo video manager", "", 
             runVideoManager);
    REG_JUMP("Video Browser", "The MythVideo video browser", "", 
             runVideoBrowser);
    REG_JUMP("Video Listings", "The MythVideo video listings", "", 
             runVideoTree);
    REG_JUMP("Video Gallery", "The MythVideo video gallery", "",
             runVideoGallery);

    
    REG_KEY("Video","FILTER","Open video filter dialog","F");
    
    REG_KEY("Video","DELETE","Delete video","D");
    REG_KEY("Video","BROWSE","Change browsable in video manager","B");
    REG_KEY("Video","INCPARENT","Increase Parental Level","],},F11");
    REG_KEY("Video","DECPARENT","Decrease Parental Level","[,{,F10");

    REG_KEY("Video","HOME","Go to the first video","Home");
    REG_KEY("Video","END","Go to the last video","End");
}


int mythplugin_init(const char *libversion)
{
    if (!gContext->TestPopupVersion("mythvideo", libversion,
                                    MYTH_BINARY_VERSION))
        return -1;

    UpgradeVideoDatabaseSchema();

    VideoGeneralSettings general;
    general.load();
    general.save();
    VideoPlayerSettings settings;
    settings.load();
    settings.save();

    setupKeys();

    return 0;
}

int mythplugin_run(void)
{
    QString themedir = gContext->GetThemeDir();
    runMenu(themedir, "videomenu.xml");

    return 0;
}

int mythplugin_config(void)
{
    QString themedir = gContext->GetThemeDir();
    runMenu(themedir, "video_settings.xml");

    return 0;
}

void runMenu(QString themedir, const QString &menuname)
{
    ThemedMenu *diag = new ThemedMenu(themedir.ascii(), menuname,
                                      gContext->GetMainWindow(), "videomenu");

    diag->setCallback(VideoCallback, NULL);
    diag->setKillable();

    if (diag->foundTheme())
    {
        if (class LCD * lcd = LCD::Get())
            lcd->switchToTime();

        qApp->unlock();
        diag->exec();
        qApp->lock();
    }
    else
    {
        cerr << "Couldn't find theme " << themedir << endl;
    }

    delete diag;
}

bool checkParentPassword()
{
    QDateTime curr_time = QDateTime::currentDateTime();
    QString last_time_stamp = gContext->GetSetting("VideoPasswordTime");
    QString password = gContext->GetSetting("VideoAdminPassword");

    if (password.length() < 1)
        return true;

    // See if we recently (and succesfully) asked for a password
    
    if (last_time_stamp.length() < 1)
    {
        // Probably first time used

        cerr << "main.o: Could not read password/pin time stamp. "
             << "This is only an issue if it happens repeatedly. " << endl;
    }
    else
    {
        QDateTime last_time = QDateTime::fromString(last_time_stamp, 
                                                    Qt::TextDate);
        if (last_time.secsTo(curr_time) < 120)
        {
            // Two minute window
            last_time_stamp = curr_time.toString(Qt::TextDate);
            gContext->SetSetting("VideoPasswordTime", last_time_stamp);
            gContext->SaveSetting("VideoPasswordTime", last_time_stamp);
            return true;
        }
    }
    
    // See if there is a password set
    
    if (password.length() > 0)
    {
        bool ok = false;
        MythPasswordDialog *pwd = new MythPasswordDialog(QObject::tr("Parental Pin:"),
                                                         &ok,
                                                         password,
                                                         gContext->GetMainWindow());
        pwd->exec();
        delete pwd;

        if (ok)
        {
            // All is good
            last_time_stamp = curr_time.toString(Qt::TextDate);
            gContext->SetSetting("VideoPasswordTime", last_time_stamp);
            gContext->SaveSetting("VideoPasswordTime", last_time_stamp);
            return true;
        }
    }
    else
        return true;
    return false;
}

void runVideoManager(void)
{
    if (checkParentPassword())
    {
        QString startdir = gContext->GetSetting("VideoStartupDir",
                                                "/share/Movies/dvd");
        VideoScanner scanner;
        scanner.doScan(startdir);

        VideoManager *manage = new VideoManager(gContext->GetMainWindow(),
                                                "video manager");
        qApp->unlock();
        manage->exec();
        qApp->lock();
        delete manage;
    }
}

void runVideoBrowser(void)
{
    VideoBrowser *browse = new VideoBrowser(gContext->GetMainWindow(),
                                            "video browser");
    qApp->unlock();
    browse->exec();
    qApp->lock();
    delete browse;
}

void runVideoTree(void)
{
    VideoTree *tree = new VideoTree(gContext->GetMainWindow(),
                                    "videotree",
                                    "video-",
                                    "video tree"); 

    qApp->unlock();
    tree->exec();
    qApp->lock();
    delete tree;
}

void runDefaultView(void)
{
    int viewType = gContext->GetNumSetting("Default MythVideo View", VideoDialog::DLG_GALLERY);

    switch (viewType)
    {
        case VideoDialog::DLG_BROWSER:
            runVideoBrowser();
            break;
        case VideoDialog::DLG_GALLERY:
            runVideoGallery();
            break;
        case VideoDialog::DLG_TREE:
            runVideoTree();
            break;
        default:            
            runVideoGallery();
            break;
    };
    
}

void runVideoGallery(void)
{
    VideoGallery *gallery = new VideoGallery(gContext->GetMainWindow(),
                                             "video gallery");
    qApp->unlock();
    gallery->exec();
    qApp->lock();
    delete gallery;
}

void VideoCallback(void *data, QString &selection)
{
    (void)data;

    QString sel = selection.lower();

    if (sel == "manager")
        runVideoManager();
    else if (sel == "browser")
        runVideoBrowser();
    else if (sel == "listing")
        runVideoTree();
    else if (sel == "gallery")
        runVideoGallery();
    else if (sel == "settings_general")
    {
        //
        //  If we are doing aggressive 
        //  Parental Control, then junior
        //  is going to have to try harder
        //  than that!
        //
        
        if (gContext->GetNumSetting("VideoAggressivePC", 0))
        {
            if (checkParentPassword())
            {
                VideoGeneralSettings settings;
                settings.exec();
            }
        }
        else
        {
            VideoGeneralSettings settings;
            settings.exec();
        }
    }
    else if (sel == "settings_player")
    {
        VideoPlayerSettings settings;
        settings.exec();
    }
    else if (sel == "settings_associations")
    {
        FileAssocDialog fa(gContext->GetMainWindow(),
                           "file_associations",
                           "video-",
                           "fa dialog");
        
        fa.exec();
    }
}






