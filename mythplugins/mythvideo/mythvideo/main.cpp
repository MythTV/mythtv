
// QT headers
#include <QApplication>

// myth headers
#include <mythtv/mythcontext.h>
#include <mythtv/mythversion.h>
#include <mythtv/lcddevice.h>
#include <mythtv/mythpluginapi.h>
#include <mythtv/mythmediamonitor.h>
#include <mythtv/util.h>

// mythui headers
#include <mythtv/libmythui/myththemedmenu.h>
#include <mythtv/libmythui/mythuihelper.h>
#include <mythtv/libmythui/mythmainwindow.h>
#include <mythtv/libmythui/mythprogressdialog.h>

// Mythvideo headers
#include "videodlg.h"
#include "videotree.h"
#include "globalsettings.h"
#include "fileassoc.h"
#include "dbcheck.h"
#include "videoscan.h"
#include "cleanup.h"
#include "globals.h"
#include "videolist.h"
#include "videoutils.h"
#include "dvdripbox.h"
#include "parentalcontrols.h"

#if defined(AEW_VG)
#include <valgrind/memcheck.h>
#endif

using namespace std;

static VideoList *videoList = NULL;

void runScreen(DialogType type)
{
    QString message = QObject::tr("Loading videos ...");

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    MythUIBusyDialog *busyPopup =
            new MythUIBusyDialog(message, popupStack, "mythvideobusydialog");

    if (busyPopup->Create())
        popupStack->AddScreen(busyPopup, false);

    if (!videoList)
        videoList = new VideoList;

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    VideoDialog *mythvideo;

    if (type == DLG_TREE)
        mythvideo = new VideoTree(mainStack, "mythvideo", videoList, type);
    else
        mythvideo = new VideoDialog(mainStack, "mythvideo", videoList, type);

    if (mythvideo->Create())
    {
        busyPopup->Close();
        mainStack->AddScreen(mythvideo);
    }
    else
        busyPopup->Close();
}

void screenVideoManager() { runScreen(DLG_MANAGER); }
void screenVideoBrowser() { runScreen(DLG_BROWSER); }
void screenVideoTree()    { runScreen(DLG_TREE);    }
void screenVideoGallery() { runScreen(DLG_GALLERY); }
void screenVideoDefault() { runScreen(DLG_DEFAULT); }

///////////////////////////////////////
// MythDVD functions
///////////////////////////////////////

// This stores the last MythMediaDevice that was detected:
QString gDVDdevice;

void startDVDRipper()
{
    QString dvd_device = gDVDdevice;

    if (dvd_device.isNull())
        dvd_device = MediaMonitor::defaultDVDdevice();

#ifdef Q_OS_MAC
    // Convert the BSD 'leaf' name to a raw device in /dev
    dvd_device.prepend("/dev/r");
#endif

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    DVDRipBox *mythdvdrip = new DVDRipBox(mainStack, "ripdvd", dvd_device);

    if (mythdvdrip->Create())
        mainStack->AddScreen(mythdvdrip);
}

void playVCD()
{
    //
    //  Get the command string to play a VCD
    //
    QString command_string = gContext->GetSetting("VCDPlayerCommand");

    gContext->addCurrentLocation("playvcd");

    if(command_string.length() < 1)
    {
        //
        //  User probably never did setup
        //
        MythScreenStack *popupStack =
                                GetMythMainWindow()->GetStack("popup stack");
        QString label = QObject::tr("You have no VCD Player command defined.");
        MythDialogBox *okPopup = new MythDialogBox(label, popupStack,
                                                   "vcdmenupopup");

        if (okPopup->Create())
            popupStack->AddScreen(okPopup, false);

        okPopup->AddButton(QObject::tr("OK, I'll go run Setup"));

        gContext->removeCurrentLocation();
        return;
    }
    else
    {
        if(command_string.contains("%d"))
        {
            //
            //  Need to do device substitution
            //
            command_string
                = command_string.replace(QRegExp("%d"),
                                            MediaMonitor::defaultVCDdevice());
        }
        gContext->sendPlaybackStart();
        myth_system(command_string);
        gContext->sendPlaybackEnd();
        gContext->GetMainWindow()->raise();
        gContext->GetMainWindow()->activateWindow();
        if (gContext->GetMainWindow()->currentWidget())
            gContext->GetMainWindow()->currentWidget()->setFocus();
    }
    gContext->removeCurrentLocation();
}

void playDVD()
{
    //
    //  Get the command string to play a DVD
    //

    QString command_string =
            gContext->GetSetting("mythdvd.DVDPlayerCommand");
    //    , "Internal");
    QString dvd_device = gDVDdevice;

    if (dvd_device.isNull())
        dvd_device = MediaMonitor::defaultDVDdevice();

    gContext->addCurrentLocation("playdvd");

    if ((command_string.indexOf("internal", 0, Qt::CaseInsensitive) > -1) ||
        (command_string.length() < 1))
    {
#ifdef Q_OS_MAC
        // Convert a BSD 'leaf' name into a raw device path
        QString filename = "dvd://dev/r";   // e.g. 'dvd://dev/rdisk2'
#elif USING_MINGW
        QString filename = "dvd:";          // e.g. 'dvd:E\\'
#else
        QString filename = "dvd:/";         // e.g. 'dvd://dev/sda'
#endif
        filename += dvd_device;

        command_string = "Internal";
        gContext->GetMainWindow()->HandleMedia(command_string, filename);
        gContext->removeCurrentLocation();

        return;
    }
    else
    {
        if (command_string.contains("%d"))
        {
            //
            //  Need to do device substitution
            //
            command_string =
                    command_string.replace(QRegExp("%d"), dvd_device);
        }
        gContext->sendPlaybackStart();
        myth_system(command_string);
        gContext->sendPlaybackEnd();
        if (gContext->GetMainWindow())
        {
            gContext->GetMainWindow()->raise();
            gContext->GetMainWindow()->activateWindow();
            if (gContext->GetMainWindow()->currentWidget())
                gContext->GetMainWindow()->currentWidget()->setFocus();
        }
    }
    gContext->removeCurrentLocation();
}

/////////////////////////////////////////////////
//// Media handlers
/////////////////////////////////////////////////
void handleDVDMedia(MythMediaDevice *dvd)
{
    if (!dvd)
        return;

    QString newDevice = dvd->getDevicePath();

    // Device insertion. Store it for later use
    if (dvd->isUsable())
        if (gDVDdevice.length() && gDVDdevice != newDevice)
        {
            // Multiple DVD devices. Clear the old one so the user has to
            // select a disk to play (in MediaMonitor::defaultDVDdevice())

            VERBOSE(VB_MEDIA, "MythVideo: Multiple DVD drives? Forgetting "
                                + gDVDdevice);
            gDVDdevice = QString::null;
        }
        else
        {
            gDVDdevice = newDevice;
            VERBOSE(VB_MEDIA,
                    "MythVideo: Storing DVD device " + gDVDdevice);
        }
    else
    {
        // Ejected/unmounted/error.

        if (gDVDdevice.length() && gDVDdevice == newDevice)
        {
            VERBOSE(VB_MEDIA,
                    "MythVideo: Forgetting existing DVD " + gDVDdevice);
            gDVDdevice = QString::null;

            // How do I tell the MTD to ignore this device?
        }

        return;
    }

    switch (gContext->GetNumSetting("DVDOnInsertDVD", 1))
    {
        case 0 : // Do nothing
            break;
        case 1 : // Display menu (mythdvd)*/
            mythplugin_run();
            break;
        case 2 : // play DVD
            playDVD();
            break;
        case 3 : //Rip DVD
            startDVDRipper();
            break;
        default:
            VERBOSE(VB_IMPORTANT, "mythdvd main.o: handleMedia() does not know what to do");
    }
}

void handleVCDMedia(MythMediaDevice *vcd)
{
    if (!vcd || !vcd->isUsable())
        return;

    switch (gContext->GetNumSetting("DVDOnInsertDVD", 1))
    {
        case 0 : // Do nothing
            break;
        case 1 : // Display menu (mythdvd)*/
            mythplugin_run();
            break;
        case 2 : // play VCD
            playVCD();
            break;
        case 3 : // Do nothing, cannot rip VCD?
            break;
    }
}

///////////////////////////////////////
// MythVideo functions
///////////////////////////////////////

void setupKeys()
{
    REG_JUMP(JUMP_VIDEO_DEFAULT, "The MythVideo default view", "",
                screenVideoDefault);
    REG_JUMP(JUMP_VIDEO_MANAGER, "The MythVideo video manager", "",
                screenVideoManager);
    REG_JUMP(JUMP_VIDEO_BROWSER, "The MythVideo video browser", "",
                screenVideoBrowser);
    REG_JUMP(JUMP_VIDEO_TREE, "The MythVideo video listings", "",
                screenVideoTree);
    REG_JUMP(JUMP_VIDEO_GALLERY, "The MythVideo video gallery", "",
                screenVideoGallery);

    REG_KEY("Video","FILTER","Open video filter dialog","F");

    REG_KEY("Video","DELETE","Delete video","D");
    REG_KEY("Video","BROWSE","Change browsable in video manager","B");
    REG_KEY("Video","INCPARENT","Increase Parental Level","],},F11");
    REG_KEY("Video","DECPARENT","Decrease Parental Level","[,{,F10");

    REG_KEY("Video","HOME","Go to the first video","Home");
    REG_KEY("Video","END","Go to the last video","End");

    // MythDVD
    REG_JUMP("Play DVD", "Play a DVD", "", playDVD);
    REG_MEDIA_HANDLER("MythDVD DVD Media Handler", "", "", handleDVDMedia,
                        MEDIATYPE_DVD, QString::null);

    REG_JUMP("Play VCD", "Play a VCD", "", playVCD);
    REG_MEDIA_HANDLER("MythDVD VCD Media Handler", "", "", handleVCDMedia,
                        MEDIATYPE_VCD, QString::null);

    REG_JUMP("Rip DVD", "Import a DVD into your MythVideo database", "",
                startDVDRipper);
}

void VideoCallback(void *data, QString &selection)
{
    (void)data;

    QString sel = selection.toLower();

    if (sel == "manager")
        runScreen(DLG_MANAGER);
    else if (sel == "browser")
        runScreen(DLG_BROWSER);
    else if (sel == "listing")
        runScreen(DLG_TREE);
    else if (sel == "gallery")
        runScreen(DLG_GALLERY);
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
//             ParentalLevel pl(ParentalLevel::plHigh);
//             pl.checkPassword();
//
//             if ()
//             {
                VideoGeneralSettings settings;
                settings.exec();
//             }
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
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

        FileAssocDialog *fa = new FileAssocDialog(mainStack, "fa dialog");

        if (fa->Create())
            mainStack->AddScreen(fa);
    }
    else if (sel == "dvd_play")
    {
        playDVD();
    }
    else if (sel == "vcd_play")
    {
        playVCD();
    }
    else if (sel == "dvd_rip")
    {
        startDVDRipper();
    }
    else if (sel == "dvd_settings_rip")
    {
        DVDRipperSettings settings;
        settings.exec();
    }
}

void runMenu(const QString &menuname)
{
    QString themedir = GetMythUI()->GetThemeDir();

    MythThemedMenu *diag =
            new MythThemedMenu(themedir.ascii(), menuname,
                                GetMythMainWindow()->GetMainStack(),
                                "video menu");

    diag->setCallback(VideoCallback, NULL);
    diag->setKillable();

    if (diag->foundTheme())
    {
        if (LCD *lcd = LCD::Get())
        {
            lcd->switchToTime();
        }
        GetMythMainWindow()->GetMainStack()->AddScreen(diag);
    }
    else
    {
        VERBOSE(VB_IMPORTANT,
                QString("Couldn't find theme %1").arg(themedir));
        delete diag;
    }
}

int mythplugin_init(const char *libversion)
{
    if (!gContext->TestPopupVersion("mythvideo", libversion,
                                    MYTH_BINARY_VERSION))
        return -1;

    gContext->ActivateSettingsCache(false);
    UpgradeVideoDatabaseSchema();
    gContext->ActivateSettingsCache(true);

    VideoGeneralSettings general;
    general.Load();
    general.Save();

    VideoPlayerSettings settings;
    settings.Load();
    settings.Save();

    DVDRipperSettings rsettings;
    rsettings.Load();
    rsettings.Save();

    setupKeys();

    return 0;
}

int mythplugin_run()
{
    runMenu("videomenu.xml");
    return 0;
}

int mythplugin_config()
{
    runMenu("video_settings.xml");

    return 0;
}

void mythplugin_destroy()
{
    if (videoList)
        delete videoList;

    CleanupHooks::getInstance()->cleanup();
#if defined(AEW_VG)
    // valgrind forgets symbols of unloaded modules
    // I'd rather sort out known non-leaks than piece
    // together a call stack.
    VALGRIND_DO_LEAK_CHECK;
#endif
}
