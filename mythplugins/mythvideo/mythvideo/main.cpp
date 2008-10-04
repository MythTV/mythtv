#include <mythtv/mythcontext.h>
#include <mythtv/mythversion.h>
#include <mythtv/lcddevice.h>
#include <mythtv/mythpluginapi.h>
#include <mythtv/mythmediamonitor.h>
#include <mythtv/util.h>

#include <mythtv/libmythui/myththemedmenu.h>
#include <mythtv/libmythui/mythuihelper.h>
#include <mythtv/libmythui/mythmainwindow.h>
#include <mythtv/libmythui/mythprogressdialog.h>
#include <mythtv/libmythui/mythdialogbox.h>

#include "videodlg.h"
#include "globalsettings.h"
#include "fileassoc.h"
#include "dbcheck.h"
#include "cleanup.h"
#include "globals.h"
#include "videolist.h"
#include "dvdripbox.h"

#if defined(AEW_VG)
#include <valgrind/memcheck.h>
#endif

namespace
{
    void runScreen(VideoDialog::DialogType type, bool fromJump = false)
    {
        QString message = QObject::tr("Loading videos ...");

        MythScreenStack *popupStack =
                GetMythMainWindow()->GetStack("popup stack");

        MythUIBusyDialog *busyPopup = new MythUIBusyDialog(message,
                popupStack, "mythvideobusydialog");

        if (busyPopup->Create())
            popupStack->AddScreen(busyPopup, false);

        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

        VideoDialog::VideoListPtr video_list;
        if (fromJump)
        {
            VideoDialog::VideoListDeathDelayPtr &saved =
                    VideoDialog::GetSavedVideoList();
            if (!saved.isNull())
            {
                video_list = saved->GetSaved();
            }
        }

        if (!video_list)
            video_list = new VideoList;

        VideoDialog *mythvideo =
                new VideoDialog(mainStack, "mythvideo", video_list, type);

        if (mythvideo->Create())
        {
            busyPopup->Close();
            mainStack->AddScreen(mythvideo);
        }
        else
            busyPopup->Close();
    }

    void jumpScreenVideoManager() { runScreen(VideoDialog::DLG_MANAGER, true); }
    void jumpScreenVideoBrowser() { runScreen(VideoDialog::DLG_BROWSER, true); }
    void jumpScreenVideoTree()    { runScreen(VideoDialog::DLG_TREE, true);    }
    void jumpScreenVideoGallery() { runScreen(VideoDialog::DLG_GALLERY, true); }
    void jumpScreenVideoDefault() { runScreen(VideoDialog::DLG_DEFAULT, true); }

    ///////////////////////////////////////
    // MythDVD functions
    ///////////////////////////////////////

    // This stores the last MythMediaDevice that was detected:
    QString gDVDdevice;

    void startDVDRipper()
    {
        QString dvd_device = gDVDdevice;

        if (dvd_device.isEmpty())
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
            QString label =
                    QObject::tr("You have no VCD Player command defined.");
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
                command_string = command_string.replace(QRegExp("%d"),
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

        if (dvd_device.isEmpty())
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
                VERBOSE(VB_IMPORTANT, "mythdvd main.o: handleMedia() does not "
                        "know what to do");
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
                    jumpScreenVideoDefault);
        REG_JUMP(JUMP_VIDEO_MANAGER, "The MythVideo video manager", "",
                    jumpScreenVideoManager);
        REG_JUMP(JUMP_VIDEO_BROWSER, "The MythVideo video browser", "",
                    jumpScreenVideoBrowser);
        REG_JUMP(JUMP_VIDEO_TREE, "The MythVideo video listings", "",
                    jumpScreenVideoTree);
        REG_JUMP(JUMP_VIDEO_GALLERY, "The MythVideo video gallery", "",
                    jumpScreenVideoGallery);

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

    class RunSettingsCompletion : public QObject
    {
        Q_OBJECT

      public:
        static void Create(bool check)
        {
            new RunSettingsCompletion(check);
        }

      private:
        RunSettingsCompletion(bool check)
        {
            if (check)
            {
                connect(&m_plcc,
                        SIGNAL(SigResultReady(bool, ParentalLevel::Level)),
                        SLOT(OnPasswordResultReady(bool,
                                        ParentalLevel::Level)));
                m_plcc.Check(ParentalLevel::plMedium, ParentalLevel::plHigh);
            }
            else
            {
                OnPasswordResultReady(true, ParentalLevel::plHigh);
            }
        }

        ~RunSettingsCompletion() {}

      private slots:
        void OnPasswordResultReady(bool passwordValid,
                ParentalLevel::Level newLevel)
        {
            if (passwordValid)
            {
                VideoGeneralSettings settings;
                settings.exec();
            }
            else
            {
                VERBOSE(VB_IMPORTANT,
                        QObject::tr("Aggressive Parental Controls Warning: "
                                "invalid password. An attempt to enter a "
                                "MythVideo settings screen was prevented."));
            }

            deleteLater();
        }

      public:
        ParentalLevelChangeChecker m_plcc;
    };

    void VideoCallback(void *data, QString &selection)
    {
        (void) data;

        QString sel = selection.toLower();

        if (sel == "manager")
            runScreen(VideoDialog::DLG_MANAGER);
        else if (sel == "browser")
            runScreen(VideoDialog::DLG_BROWSER);
        else if (sel == "listing")
            runScreen(VideoDialog::DLG_TREE);
        else if (sel == "gallery")
            runScreen(VideoDialog::DLG_GALLERY);
        else if (sel == "settings_general")
        {
            RunSettingsCompletion::Create(gContext->
                    GetNumSetting("VideoAggressivePC", 0));
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
    CleanupHooks::getInstance()->cleanup();
#if defined(AEW_VG)
    // valgrind forgets symbols of unloaded modules
    // I'd rather sort out known non-leaks than piece
    // together a call stack.
    VALGRIND_DO_LEAK_CHECK;
#endif
}

#include "main.moc"
