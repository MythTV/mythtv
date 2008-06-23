/////////////////////////////////////////////////////////////////////
// main.cpp
//
// (c) 2003-2007 Thor Sigvaldason and Isaac Richards
// Part of the MythTV project
//
// Several portions of this module rely heavily on the
// transcode project. More information on transcode is
// available at:
//
// http://www.transcoding.org/cgi-bin/transcode
/////////////////////////////////////////////////////////////////////

#include <QApplication>

#include <mythtv/mythcontext.h>
#include <mythtv/lcddevice.h>
#include <mythtv/libmythui/myththemedmenu.h>
#include <mythtv/mythpluginapi.h>
#include <mythtv/mythmediamonitor.h>
#include <mythtv/util.h>

#include "videomanager.h"
#include "videobrowser.h"
#include "videotree.h"
#include "videogallery.h"
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

namespace
{
    template <typename T>
    class screen_inst
    {
      public:
        screen_inst(T *inst, const QString &loc_name) : m_inst(inst),
            m_location_name(loc_name)
        {
        }

        int run()
        {
            gContext->addCurrentLocation(m_location_name);
            qApp->unlock();
            m_inst->exec();
            qApp->lock();
            gContext->removeCurrentLocation();
            return m_inst->videoExitType();
        }

        ~screen_inst()
        {
            delete m_inst;
        }

      private:
        T *m_inst;
        const QString &m_location_name;
    };

    template <typename T>
    class q_screen_inst
    {
      public:
        q_screen_inst(T *inst, const QString &loc_name) : m_inst(inst),
            m_location_name(loc_name)
        {
        }

        int run()
        {
            gContext->addCurrentLocation(m_location_name);
            qApp->unlock();
            m_inst->exec();
            qApp->lock();
            gContext->removeCurrentLocation();
            return m_inst->videoExitType();
        }

        ~q_screen_inst()
        {
            m_inst->deleteLater();
            m_inst = NULL;
        }

      private:
        T *m_inst;
        const QString &m_location_name;
    };

    template <typename T>
    int exec_screen(T *inst, const QString &loc_name)
    {
        screen_inst<T> si(inst, loc_name);
        return si.run();
    }

    template <typename T>
    int q_exec_screen(T *inst, const QString &loc_name)
    {
        q_screen_inst<T> si(inst, loc_name);
        return si.run();
    }

    class screens
    {
      private:
        static int runVideoManager(VideoList *video_list)
        {
            if (checkParentPassword(ParentalLevel::plHigh))
            {
                VideoScanner scanner;
                scanner.doScan(GetVideoDirs());

                return q_exec_screen(
                    new VideoManager(gContext->GetMainWindow(), video_list),
                    "videomanager");
            }
            return 0;
        }

        static int runVideoBrowser(VideoList *video_list)
        {
            return exec_screen(new VideoBrowser(gContext->GetMainWindow(),
                                                "video browser", video_list),
                               "videobrowser");
        }

        static int runVideoTree(VideoList *video_list)
        {
            return exec_screen(new VideoTree(gContext->GetMainWindow(),
                                             "videotree", "video-",
                                             "video tree", video_list),
                               "videolistings");
        }

        static int runVideoGallery(VideoList *video_list)
        {
            return q_exec_screen(new VideoGallery(gContext->GetMainWindow(),
                                                  "video gallery", video_list),
                                 "videogallery");
        }

      public:
        // FIXME: This silliness brought to you by REG_JUMP
        // which doesn't allow for basic things like pushing
        // an int or void * through the callback.
        enum screen_type
        {
            stVideoBrowser = VideoDialog::DLG_BROWSER,
            stVideoGallery = VideoDialog::DLG_GALLERY,
            stVideoTree = VideoDialog::DLG_TREE,
            stVideoManager,
            stDefaultView
        };

        static void runScreen(screen_type st)
        {
            static VideoList *video_list = 0;

            if (st == stDefaultView)
            {
                st = static_cast<screen_type>(
                        gContext->GetNumSetting("Default MythVideo View",
                                                stVideoGallery));
                if (!VideoDialog::IsValidDialogType(st))
                    st = stVideoGallery;
            }

            if (!video_list)
            {
                video_list = new VideoList;
            }

            int sret = 0;
            switch (st)
            {
                case stVideoManager:
                {
                    sret = runVideoManager(video_list);
                    break;
                }
                case stVideoBrowser:
                {
                    sret = runVideoBrowser(video_list);
                    break;
                }
                case stVideoTree:
                {
                    sret = runVideoTree(video_list);
                    break;
                }
                case stVideoGallery:
                {
                    sret = runVideoGallery(video_list);
                    break;
                }
                case stDefaultView:
                default:
                {
                    sret = runVideoGallery(video_list);
                    break;
                }
            }

            if (sret != SCREEN_EXIT_VIA_JUMP)
            {
                // All these hoops to make view switching faster.

                // If a screen didn't exit via a jump we should clean up
                // the data we loaded.
                CleanupHooks::getInstance()->cleanup();
                delete video_list;
                video_list = 0;
            }
        }
    };

    void screenVideoManager() { screens::runScreen(screens::stVideoManager); }
    void screenVideoBrowser() { screens::runScreen(screens::stVideoBrowser); }
    void screenVideoTree() { screens::runScreen(screens::stVideoTree); }
    void screenVideoGallery() { screens::runScreen(screens::stVideoGallery); }
    void screenVideoDefault() { screens::runScreen(screens::stDefaultView); }

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
        DVDRipBox *drb = new DVDRipBox(gContext->GetMainWindow(),
                                       "dvd_rip", dvd_device, "dvd-"); 
        gContext->addCurrentLocation("ripdvd");
        qApp->unlock();
        drb->exec();
        qApp->lock();
        gContext->removeCurrentLocation();

        qApp->processEvents();

        delete drb;
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
            DialogBox *no_player_dialog =
                    new DialogBox(gContext->GetMainWindow(),
                    QObject::tr("\n\nYou have no VCD Player command defined."));
            no_player_dialog->AddButton(QObject::tr("OK, I'll go run Setup"));
            no_player_dialog->exec();

            no_player_dialog->deleteLater();
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
            gContext->GetMainWindow()->setActiveWindow();
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

        if ((command_string.find("internal", 0, false) > -1) ||
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
                gContext->GetMainWindow()->setActiveWindow();
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

        QString sel = selection.lower();

        if (sel == "manager")
            screenVideoManager();
        else if (sel == "browser")
            screenVideoBrowser();
        else if (sel == "listing")
            screenVideoTree();
        else if (sel == "gallery")
            screenVideoGallery();
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
                if (checkParentPassword(ParentalLevel::plHigh))
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
        QString themedir = gContext->GetThemeDir();

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
    general.load();
    general.save();

    VideoPlayerSettings settings;
    settings.load();
    settings.save();

    DVDRipperSettings rsettings;
    rsettings.load();
    rsettings.save();

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
