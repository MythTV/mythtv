#include <qapplication.h>

#include <mythtv/mythcontext.h>
#include <mythtv/lcddevice.h>
#include <mythtv/libmythui/myththemedmenu.h>

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
    int exec_screen(T *inst, const QString &loc_name)
    {
        screen_inst<T> si(inst, loc_name);
        return si.run();
    }

    class screens
    {
      private:
        static int runVideoManager(VideoList *video_list)
        {
            if (checkParentPassword())
            {
                VideoScanner scanner;
                scanner.doScan(GetVideoDirs());

                return exec_screen(new VideoManager(gContext->GetMainWindow(),
                                                    "video manager",
                                                    video_list),
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
            return exec_screen(new VideoGallery(gContext->GetMainWindow(),
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
}

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

void runMenu(QString themedir, const QString &menuname)
{
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
        VERBOSE(VB_IMPORTANT, QString("Couldn't find theme %1").arg(themedir));
        delete diag;
    }
}

extern "C"
{
    int mythplugin_init(const char *libversion);
    int mythplugin_run();
    int mythplugin_config();
    void mythplugin_destroy();
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

    setupKeys();

    return 0;
}

int mythplugin_run()
{
    QString themedir = gContext->GetThemeDir();
    runMenu(themedir, "videomenu.xml");
    return 0;
}

int mythplugin_config()
{
    QString themedir = gContext->GetThemeDir();
    runMenu(themedir, "video_settings.xml");

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
