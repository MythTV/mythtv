#include <map>
#include <vector>

#include <QDir>

#include <mythtv/mythdirs.h>

#include "globals.h"
#include "videodlg.h"
#include "globalsettings.h"

namespace
{
// General Settings
HostComboBox *VideoDefaultParentalLevel()
{
    HostComboBox *gc = new HostComboBox("VideoDefaultParentalLevel");
    gc->setLabel(QObject::tr("Starting Parental Level"));
    gc->addSelection(QObject::tr("4 - Highest"),
                     QString::number(ParentalLevel::plHigh));
    gc->addSelection(QObject::tr("1 - Lowest"),
                     QString::number(ParentalLevel::plLowest));
    gc->addSelection(QString::number(ParentalLevel::plLow));
    gc->addSelection(QString::number(ParentalLevel::plMedium));
    gc->setHelpText(QObject::tr("This is the 'level' that MythVideo starts at. "
                    "Any videos with a level at or below this will be shown in "
                    "the list or while browsing by default. The Parental PIN "
                    "should be set to limit changing of the default level."));
    return gc;
}

HostComboBox *VideoDefaultView()
{
    HostComboBox *gc = new HostComboBox("Default MythVideo View");
    gc->setLabel(QObject::tr("Default View"));
    gc->addSelection(QObject::tr("Gallery"),
                     QString::number(VideoDialog::DLG_GALLERY));
    gc->addSelection(QObject::tr("Browser"),
                     QString::number(VideoDialog::DLG_BROWSER));
    gc->addSelection(QObject::tr("Listings"),
                     QString::number(VideoDialog::DLG_TREE));
    gc->addSelection(QObject::tr("Manager"),
                     QString::number(VideoDialog::DLG_MANAGER));
    gc->setHelpText(QObject::tr("The default view for MythVideo. "
                    "Other views can be reached via the popup menu available "
                    "via the MENU key."));
    return gc;
}

const char *password_clue =
    QT_TR_NOOP("Setting this value to all numbers will make your life "
                "much easier.");

HostLineEdit *VideoAdminPassword()
{
    HostLineEdit *gc = new HostLineEdit("VideoAdminPassword");
    gc->setLabel(QObject::tr("Parental Level 4 PIN"));
    gc->setHelpText(QString("%1 %2")
        .arg(QObject::tr("This PIN is used to enter Parental Control "
                         "Level 4 as well as the Video Manager."))
        .arg(QObject::tr(password_clue)));
    return gc;
}

HostLineEdit *VideoAdminPasswordThree()
{
    HostLineEdit *gc = new HostLineEdit("VideoAdminPasswordThree");
    gc->setLabel(QObject::tr("Parental Level 3 PIN"));
    gc->setHelpText(QString("%1 %2")
        .arg(QObject::tr("This PIN is used to enter Parental Control Level 3."))
        .arg(QObject::tr(password_clue)));
    return gc;
}

HostLineEdit *VideoAdminPasswordTwo()
{
    HostLineEdit *gc = new HostLineEdit("VideoAdminPasswordTwo");
    gc->setLabel(QObject::tr("Parental Level 2 PIN"));
    gc->setHelpText(QString("%1 %2")
        .arg(QObject::tr("This PIN is used to enter Parental Control Level 2."))
        .arg(QObject::tr(password_clue)));
    return gc;
}

HostCheckBox *VideoAggressivePC()
{
    HostCheckBox *gc = new HostCheckBox("VideoAggressivePC");
    gc->setLabel(QObject::tr("Aggressive Parental Control"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If set, you will not be able to return "
                    "to this screen and reset the Parental "
                    "PIN without first entering the current PIN. You have "
                    "been warned."));
    return gc;
}

HostCheckBox *VideoListUnknownFiletypes()
{
    HostCheckBox *gc = new HostCheckBox("VideoListUnknownFiletypes");
    gc->setLabel(QObject::tr("Show Unknown File Types"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If set, all files below the Myth Video "
                    "directory will be displayed unless their "
                    "extension is explicitly set to be ignored. "));
    return gc;
}

HostCheckBox *VideoTreeNoMetaData() 
{ 
    HostCheckBox *gc = new HostCheckBox("VideoTreeLoadMetaData"); 
    gc->setLabel(QObject::tr("Video List Loads Video Meta Data")); 
    gc->setValue(true); 
    gc->setHelpText(QObject::tr("If set along with Browse Files, this " 
                    "will cause the Video List to load any known video meta" 
                    "data from the database. Turning this off can greatly " 
                    "speed up how long it takes to load the Video List tree")); 
    return gc;
}

HostCheckBox *VideoNewBrowsable()
{
    HostCheckBox *gc = new HostCheckBox("VideoNewBrowsable");
    gc->setLabel(QObject::tr("Newly scanned files are browsable by default"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If set, newly scanned files in the Video "
                    "Manager will be marked as browsable and will appear in "
                    "the 'Browse' menu."));
    return gc;
}

HostCheckBox *VideoSortIgnoresCase()
{
    HostCheckBox *hcb = new HostCheckBox("mythvideo.sort_ignores_case");
    hcb->setLabel(QObject::tr("Sorting ignores case"));
    hcb->setValue(true);
    hcb->setHelpText(QObject::tr("If set, case is ignored when sorting "
                                 "entries in a view."));
    return hcb;
}

HostCheckBox *VideoDBGroupView()
{
    HostCheckBox *hcb = new HostCheckBox("mythvideo.db_group_view");
    hcb->setLabel(QObject::tr("Enable Metadata Browse Modes"));
    hcb->setValue(true);
    hcb->setHelpText(QObject::tr("If set, metadata groupings of your video "
                                 "directory will be shown in supported "
                                 "views.  Default group is set below."));
    return hcb;
}

HostComboBox *VideoTreeGroup() 
{ 
    HostComboBox *gc = new HostComboBox("mythvideo.db_group_type"); 
    gc->setLabel(QObject::tr("Default Metadata View"));
    gc->addSelection(QObject::tr("Folder"),"0");
    gc->addSelection(QObject::tr("Genres"),"1");
    gc->addSelection(QObject::tr("Category"),"2");
    gc->addSelection(QObject::tr("Year"),"3");
    gc->addSelection(QObject::tr("Director"),"4");
    gc->addSelection(QObject::tr("Cast"),"5");
    gc->addSelection(QObject::tr("User Rating"),"6");
    gc->addSelection(QObject::tr("Date Added"),"7");
    gc->addSelection(QObject::tr("TV/Movies"),"8");
    gc->setHelpText(QObject::tr("Default metadata view contols "
                                "the method used to build the tree. Folder "
                                "mode (the default) displays the videos as "
                                "they are found in the filesystem.")); 
    return gc; 
} 

HostCheckBox *VideoTreeRemember()
{
    HostCheckBox *gc = new HostCheckBox("mythvideo.VideoTreeRemember");
    gc->setLabel(QObject::tr("Video Tree remembers last selected position"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If set, the current position in the Video "
                                "Tree is persistent."));
    return gc;
}

HostLineEdit *SearchListingsCommand()
{
    HostLineEdit *gc = new HostLineEdit("MovieListCommandLine");
    gc->setLabel(QObject::tr("Command to search for movie listings"));
    gc->setValue(GetShareDir() + "mythvideo/scripts/tmdb.pl -M");
    gc->setHelpText(QObject::tr("This command must be "
                    "executable by the user running MythVideo."));
    return gc;
}

HostLineEdit *GetPostersCommand()
{
    HostLineEdit *gc = new HostLineEdit("MoviePosterCommandLine");
    gc->setLabel(QObject::tr("Command to search for movie posters"));
    gc->setValue(GetShareDir() + "mythvideo/scripts/tmdb.pl -P");
    gc->setHelpText(QObject::tr("This command must be "
                    "executable by the user running MythVideo."));
    return gc;
}

HostLineEdit *GetFanartCommand()
{
    HostLineEdit *gc = new HostLineEdit("MovieFanartCommandLine");
    gc->setLabel(QObject::tr("Command to search for movie fanart"));
    gc->setValue(GetShareDir() + "mythvideo/scripts/tmdb.pl -B");
    gc->setHelpText(QObject::tr("This command must be "
                    "executable by the user running MythVideo."));
    return gc;
}

HostLineEdit *GetDataCommand()
{
    HostLineEdit *gc = new HostLineEdit("MovieDataCommandLine");
    gc->setLabel(QObject::tr("Command to extract data for movies"));
    gc->setValue(GetShareDir() + "mythvideo/scripts/tmdb.pl -D");
    gc->setHelpText(QObject::tr("This command must be "
                    "executable by the user running MythVideo."));
    return gc;
}

HostLineEdit *SearchTVListingsCommand()
{
    HostLineEdit *gc = new HostLineEdit("mythvideo.TVListCommandLine");
    gc->setLabel(QObject::tr("Command to search for TV shows in MythVideo"));
    gc->setValue(GetShareDir() + "mythvideo/scripts/ttvdb.py -M");
    gc->setHelpText(QObject::tr("This command must be "
                    "executable by the user running MythVideo."));
    return gc;
}

HostLineEdit *GetTVPostersCommand()
{
    HostLineEdit *gc = new HostLineEdit("mythvideo.TVPosterCommandLine");
    gc->setLabel(QObject::tr("Command to search for TV Season posters"));
    gc->setValue(GetShareDir() + "mythvideo/scripts/ttvdb.py -P");
    gc->setHelpText(QObject::tr("This command must be "
                    "executable by the user running MythVideo."));
    return gc;
}

HostLineEdit *GetTVFanartCommand()
{
    HostLineEdit *gc = new HostLineEdit("mythvideo.TVFanartCommandLine");
    gc->setLabel(QObject::tr("Command to search for TV fanart"));
    gc->setValue(GetShareDir() + "mythvideo/scripts/ttvdb.py -F");
    gc->setHelpText(QObject::tr("This command must be "
                    "executable by the user running MythVideo."));
    return gc;
}

HostLineEdit *GetTVBannerCommand()
{
    HostLineEdit *gc = new HostLineEdit("mythvideo.TVBannerCommandLine");
    gc->setLabel(QObject::tr("Command to search for TV banners"));
    gc->setValue(GetShareDir() + "mythvideo/scripts/ttvdb.py -B");
    gc->setHelpText(QObject::tr("This command must be "
                    "executable by the user running MythVideo."));
    return gc;
}

HostLineEdit *GetTVScreenshotCommand()
{
    HostLineEdit *gc = new HostLineEdit("mythvideo.TVScreenshotCommandLine");
    gc->setLabel(QObject::tr("Command to search for TV Screenshots"));
    gc->setValue(GetShareDir() + "mythvideo/scripts/ttvdb.py -S");
    gc->setHelpText(QObject::tr("This command must be "
                    "executable by the user running MythVideo."));
    return gc;
}

HostLineEdit *GetTVDataCommand()
{
    HostLineEdit *gc = new HostLineEdit("mythvideo.TVDataCommandLine");
    gc->setLabel(QObject::tr("Command to extract data for TV Episodes"));
    gc->setValue(GetShareDir() + "mythvideo/scripts/ttvdb.py -D");
    gc->setHelpText(QObject::tr("This command must be "
                    "executable by the user running MythVideo."));
    return gc;
}

HostLineEdit *GetTVTitleSubCommand()
{
    HostLineEdit *gc = new HostLineEdit("mythvideo.TVTitleSubCommandLine");
    gc->setLabel(QObject::tr("Command to search for TV by Title/Subtitle"));
    gc->setValue(GetShareDir() + "mythvideo/scripts/ttvdb.py -N");
    gc->setHelpText(QObject::tr("This command must be "
                    "executable by the user running MythVideo."));
    return gc;
}

HostLineEdit *VideoStartupDirectory()
{
    HostLineEdit *gc = new HostLineEdit("VideoStartupDir");
    gc->setLabel(QObject::tr("Directories that hold videos"));
    gc->setValue(DEFAULT_VIDEOSTARTUP_DIR);
    gc->setHelpText(QObject::tr("Multiple directories can be separated by ':'. "
                    "Each directory must exist and be readable by the user "
                    "running MythVideo."));
    return gc;
}

HostLineEdit *VideoArtworkDirectory()
{
    HostLineEdit *gc = new HostLineEdit("VideoArtworkDir");
    gc->setLabel(QObject::tr("Directory that holds movie posters"));
    gc->setValue(GetConfDir() + "/MythVideo/Artwork");
    gc->setHelpText(QObject::tr("This directory must exist, and the user "
                    "running MythVideo needs to have read/write permission "
                    "to the directory."));
    return gc;
}

HostLineEdit *VideoScreenshotDirectory()
{
    HostLineEdit *gc = new HostLineEdit("mythvideo.screenshotDir");
    gc->setLabel(QObject::tr("Directory that holds movie screenshots"));
    gc->setValue(GetConfDir() + "/MythVideo/Screenshots");
    gc->setHelpText(QObject::tr("This directory must exist, and the user "
                    "running MythVideo needs to have read/write permission "
                    "to the directory."));
    return gc;
}

HostLineEdit *VideoBannerDirectory()
{
    HostLineEdit *gc = new HostLineEdit("mythvideo.bannerDir");
    gc->setLabel(QObject::tr("Directory that holds movie/TV Banners"));
    gc->setValue(GetConfDir() + "/MythVideo/Banners");
    gc->setHelpText(QObject::tr("This directory must exist, and the user "
                    "running MythVideo needs to have read/write permission "
                    "to the directory."));
    return gc;
}

HostLineEdit *VideoFanartDirectory()
{
    HostLineEdit *gc = new HostLineEdit("mythvideo.fanartDir");
    gc->setLabel(QObject::tr("Directory that holds movie fanart"));
    gc->setValue(GetConfDir() + "/MythVideo/Fanart");
    gc->setHelpText(QObject::tr("This directory must exist, and the user "
                    "running MythVideo needs to have read/write permission "
                    "to the directory."));
    return gc;
}

HostLineEdit *TrailerDirectory()
{
    HostLineEdit *gc = new HostLineEdit("mythvideo.TrailersDir");
    gc->setLabel(QObject::tr("Directory that holds movie trailers"));
    gc->setValue(GetConfDir() + "/MythVideo/Trailers");
    gc->setHelpText(QObject::tr("This directory must exist, and the user "
                    "running MythVideo needs to have read/write permission "
                    "to the directory."));
    return gc;
}

//Player Settings

HostLineEdit *VideoDefaultPlayer()
{
    HostLineEdit *gc = new HostLineEdit("VideoDefaultPlayer");
    gc->setLabel(QObject::tr("Default Video Player"));
    gc->setValue("Internal");
    gc->setHelpText(QObject::tr("This is the command used for any file "
                    "that the extension is not specifically defined. "
                    "You may also enter the name of one of the playback "
                    "plugins such as 'Internal'."));
    return gc;
}

HostCheckBox *EnableAlternatePlayer()
{
    HostCheckBox *gc = new HostCheckBox("mythvideo.EnableAlternatePlayer");
    gc->setLabel(QObject::tr("Enable Alternate Video Player"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If checked, you can select an alternate "
                                "player command for videos when the default "
                                "choice fails."));
    return gc;
}

HostLineEdit *VideoAlternatePlayer()
{
    HostLineEdit *gc = new HostLineEdit("mythvideo.VideoAlternatePlayer");
    gc->setLabel(QObject::tr("Alternate Player"));
    gc->setValue("Internal");
    gc->setHelpText(QObject::tr("If for some reason the default player "
                    "doesn't play a video, you can play it in an alternate "
                    "player by selecting 'Play in Alternate Player.'"));
    return gc;
};

class AlternatePlayerSettings : public TriggeredConfigurationGroup
{
    public:
        AlternatePlayerSettings():
            TriggeredConfigurationGroup(false, false, true, true)
        {
            Setting *altplayerSettings = EnableAlternatePlayer();
            addChild(altplayerSettings);
            setTrigger(altplayerSettings);

            ConfigurationGroup *settings =
                    new VerticalConfigurationGroup(false);
            settings->addChild(VideoAlternatePlayer());
            addTarget("1", settings);
            addTarget("0", new VerticalConfigurationGroup(true));
        }
};

///////////////////////////////////////////////////////////
//// DVD Settings
///////////////////////////////////////////////////////////

// General Settings

HostLineEdit *SetVCDDevice()
{
    HostLineEdit *gc = new HostLineEdit("VCDDeviceLocation");
    gc->setLabel(QObject::tr("Location of VCD device"));
    gc->setValue("default");
    gc->setHelpText(QObject::tr("This device must exist, and the user "
                    "running MythDVD needs to have read permission "
                    "on the device.")
                    + QObject::tr(" 'default' will let the "
                                  "MediaMonitor choose a device."));
    return gc;
}

HostLineEdit *SetDVDDevice()
{
    HostLineEdit *gc = new HostLineEdit("DVDDeviceLocation");
    gc->setLabel(QObject::tr("Location of DVD device"));
    gc->setValue("default");
    gc->setHelpText(QObject::tr("This device must exist, and the user "
                    "running MythDVD needs to have read permission "
                    "on the device.")
                    + QObject::tr(" 'default' will let the "
                                  "MediaMonitor choose a device."));
    return gc;
}

HostComboBox *SetOnInsertDVD()
{
    HostComboBox *gc = new HostComboBox("DVDOnInsertDVD");
    gc->setLabel(QObject::tr("On DVD insertion"));
    gc->addSelection(QObject::tr("Display mythdvd menu"),"1");
    gc->addSelection(QObject::tr("Do nothing"),"0");
    gc->addSelection(QObject::tr("Play DVD"),"2");
    gc->addSelection(QObject::tr("Rip DVD"),"3");
    gc->setHelpText(QObject::tr("Media Monitoring should be turned on to "
                   "allow this feature (Setup -> General -> CD/DVD Monitor"));
    return gc;
}

HostSlider *DVDBookmarkDays()
{
    HostSlider *gs = new HostSlider("DVDBookmarkDays",5, 50, 5);
    gs->setLabel(QObject::tr("Remove DVD Bookmarks Older than (days)"));
    gs->setValue(10);
    gs->setHelpText((QObject::tr("Delete DVD Bookmarks that are older than the "
                                 "Number of days specified")));
    return gs;
}

HostSlider *SetDVDDriveSpeed()
{
    HostSlider *gs = new HostSlider("DVDDriveSpeed", 2, 12, 2);
    gs->setLabel(QObject::tr("DVD Drive Speed"));
    gs->setValue(12);
    gs->setHelpText(QObject::tr("Set DVD Drive Speed during DVD Playback. "
                                "Speed is in multiples of 177KB/s. "
                                "Slower speeds may reduce drive noise but in "
                                "some cases it causes playback to stutter."));
    return gs;
}

HostCheckBox *EnableDVDBookmark()
{
    HostCheckBox *gc = new HostCheckBox("EnableDVDBookmark");
    gc->setLabel(QObject::tr("Enable DVD Bookmark Support"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("Enable DVD Bookmark Support"));
    return gc;
}

HostCheckBox *DVDBookmarkPrompt()
{
    HostCheckBox *gc = new HostCheckBox("DVDBookmarkPrompt");
    gc->setLabel(QObject::tr("DVD Bookmark Prompt"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("Display a prompt to choose whether "
                "to play the DVD from the beginning or from the bookmark"));
    return gc;
}

class DVDBookmarkSettings : public TriggeredConfigurationGroup
{
    public:
        DVDBookmarkSettings():
            TriggeredConfigurationGroup(false, false, true, true)
        {
            Setting *dvdbookmarkSettings = EnableDVDBookmark();
            addChild(dvdbookmarkSettings);
            setTrigger(dvdbookmarkSettings);

            ConfigurationGroup *settings =
                    new VerticalConfigurationGroup(false);
            settings->addChild(DVDBookmarkPrompt());
            settings->addChild(DVDBookmarkDays());
            addTarget("1", settings);
            addTarget("0", new VerticalConfigurationGroup(true));
        }
};

// Player Settings

HostLineEdit *PlayerCommand()
{
    HostLineEdit *gc = new HostLineEdit("mythdvd.DVDPlayerCommand");
    gc->setLabel(QObject::tr("DVD Player Command"));
    gc->setValue("Internal");
    gc->setHelpText(QObject::tr("This can be any command to launch a DVD "
                    " player (e.g. MPlayer, ogle, etc.). If present, %d will "
                    "be substituted for the DVD device (e.g. /dev/dvd)."));
    return gc;
}

HostLineEdit *VCDPlayerCommand()
{
    HostLineEdit *gc = new HostLineEdit("VCDPlayerCommand");
    gc->setLabel(QObject::tr("VCD Player Command"));
    gc->setValue("mplayer vcd:// -cdrom-device %d -fs -zoom -vo xv");
    gc->setHelpText(QObject::tr("This can be any command to launch a VCD "
                    "player (e.g. MPlayer, xine, etc.). If present, %d will "
                    "be substituted for the VCD device (e.g. /dev/cdrom)."));
    return gc;
}

// Ripper Settings

HostLineEdit *SetRipDirectory()
{
    HostLineEdit *gc = new HostLineEdit("DVDRipLocation");
    gc->setLabel(QObject::tr("Directory to hold temporary files"));
#ifdef Q_WS_MACX
    gc->setValue(QDir::homePath() + "/Library/Application Support");
#else
    gc->setValue("/var/lib/mythdvd/temp");
#endif
    gc->setHelpText(QObject::tr("This directory must exist, and the user "
                    "running MythDVD needs to have write permission "
                    "to the directory."));
    return gc;
}

HostLineEdit *TitlePlayCommand()
{
    HostLineEdit *gc = new HostLineEdit("TitlePlayCommand");
    gc->setLabel(QObject::tr("Title Playing Command"));
    gc->setValue("mplayer dvd://%t -dvd-device %d -fs -zoom -vo xv -aid %a "
                 "-channels %c");
    gc->setHelpText(QObject::tr("This is a command used to preview a given "
                    "title on a DVD. If present %t will be set "
                    "to the title, %d for device, %a for audio "
                    "track, %c for audio channels."));
    return gc;
}

HostLineEdit *SubTitleCommand()
{
    HostLineEdit *gc = new HostLineEdit("SubTitleCommand");
    gc->setLabel(QObject::tr("Subtitle arguments:"));
    gc->setValue("-sid %s");
    gc->setHelpText(QObject::tr("If you choose any subtitles for ripping, this "
                    "command is added to the end of the Title Play "
                    "Command to allow previewing of subtitles. If  "
                    "present %s will be set to the subtitle track. "));
    return gc;
}

HostLineEdit *TranscodeCommand()
{
    HostLineEdit *gc = new HostLineEdit("TranscodeCommand");
    gc->setLabel(QObject::tr("Base transcode command"));
    gc->setValue("transcode");
    gc->setHelpText(QObject::tr("This is the base (without arguments) command "
                    "to run transcode on your system."));
    return gc;
}

HostSpinBox *MTDPortNumber()
{
    HostSpinBox *gc = new HostSpinBox("MTDPort", 1024, 65535, 1);
    gc->setLabel(QObject::tr("MTD port number"));
    gc->setValue(2442);
    gc->setHelpText(QObject::tr("The port number that should be used for "
                    "communicating with the MTD (Myth Transcoding "
                    "Daemon)"));
    return gc;
}

HostCheckBox *MTDLogFlag()
{
    HostCheckBox *gc = new HostCheckBox("MTDLogFlag");
    gc->setLabel(QObject::tr("MTD logs to terminal window"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If set, the MTD (Myth Transcoding Daemon) "
                    "will log to the window it is started from. "
                    "Otherwise, it will write to a file called  "
                    "mtd.log in the top level ripping directory."));
    return gc;
}

HostCheckBox *MTDac3Flag()
{
    HostCheckBox *gc = new HostCheckBox("MTDac3Flag");
    gc->setLabel(QObject::tr("Transcode AC3 Audio"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If set, the MTD (Myth Transcoding Daemon) "
                    "will, by default, preserve AC3 (Dolby "
                    "Digital) audio in transcoded files. "));
    return gc;
}

HostCheckBox *MTDxvidFlag()
{
    HostCheckBox *gc = new HostCheckBox("MTDxvidFlag");
    gc->setLabel(QObject::tr("Use xvid rather than divx"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If set, mythdvd will use the (open, free) "
                    "xvid codec rather than divx whenever "
                    "possible."));
    return gc;
}

HostCheckBox *MTDTrustTranscodeFRDetect()
{
    HostCheckBox *gc = new HostCheckBox("mythvideo.TrustTranscodeFRDetect");
    gc->setLabel(QObject::tr("Let transcode determine frame rate"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If set, transcode will determine the frame "
                                "rate automatically. If not set, 23.976 is "
                                "assumed."));
    return gc;
}

HostSpinBox *MTDNiceLevel()
{
    HostSpinBox *gc = new HostSpinBox("MTDNiceLevel", 0, 20, 1);
    gc->setLabel(QObject::tr("Nice level for MTD"));
    gc->setValue(20);
    gc->setHelpText(QObject::tr("This determines the priority of the Myth "
                    "Transcoding Daemon. Higher numbers mean "
                    "lower priority (more CPU to other tasks)."));
    return gc;
}

HostSpinBox *MTDConcurrentTranscodes()
{
    HostSpinBox *gc = new HostSpinBox("MTDConcurrentTranscodes", 1, 99, 1);
    gc->setLabel(QObject::tr("Simultaneous Transcode Jobs"));
    gc->setValue(1);
    gc->setHelpText(QObject::tr("This determines the number of simultaneous "
                    "transcode jobs. If set at 1 (the default), "
                    "there will only be one active job at a time."));
    return gc;
}

HostSpinBox *MTDRipSize()
{
    HostSpinBox *gc = new HostSpinBox("MTDRipSize", 0, 4096, 1);
    gc->setLabel(QObject::tr("Ripped video segments"));
    gc->setValue(0);
    gc->setHelpText(QObject::tr("If set to something other than 0, ripped "
                    "video titles will be broken up into files "
                    "of this size (in MB). Applies to both perfect "
                    "quality recordings and intermediate files "
                    "used for transcoding."));
    return gc;
}

struct ConfigPage
{
    typedef std::vector<ConfigurationGroup *> PageList;

  protected:
    ConfigPage(PageList &pl) : m_pl(pl)
    {
    }

    void Add(ConfigurationGroup *page)
    {
        m_pl.push_back(page);
    }

  private:
    ConfigPage(const ConfigPage &);
    ConfigPage &operator=(const ConfigPage &);

  private:
    PageList &m_pl;
};

struct VConfigPage : public ConfigPage
{
    VConfigPage(PageList &pl, bool luselabel = true, bool luseframe  = true,
                bool lzeroMargin = false, bool lzeroSpace = false) :
        ConfigPage(pl)
    {
        m_vc_page = new VerticalConfigurationGroup(luselabel, luseframe,
                                                   lzeroMargin, lzeroSpace);
        Add(m_vc_page);
    }

    VerticalConfigurationGroup *operator->()
    {
        return m_vc_page;
    }

  private:
    VerticalConfigurationGroup *m_vc_page;
};

class RatingsToPL : public TriggeredConfigurationGroup
{
  public:
    RatingsToPL() : TriggeredConfigurationGroup(false)
    {
        HostCheckBox *r2pl =
                new HostCheckBox("mythvideo.ParentalLevelFromRating");
        r2pl->setLabel(QObject::tr("Enable automatic Parental Level from "
                                   "rating"));
        r2pl->setValue(false);
        r2pl->setHelpText(QObject::tr("If enabled, searches will automatically "
                                      "set the Parental Level to the one "
                                      "matching the rating below."));
        addChild(r2pl);
        setTrigger(r2pl);

        typedef std::map<ParentalLevel::Level, QString> r2pl_map;
        r2pl_map r2pl_defaults;
        r2pl_defaults.insert(r2pl_map::value_type(ParentalLevel::plLowest,
                tr("G", "PL 1 default search string.")));
        r2pl_defaults.insert(r2pl_map::value_type(ParentalLevel::plLow,
                tr("PG", "PL 2 default search string.")));
        r2pl_defaults.insert(r2pl_map::value_type(ParentalLevel::plMedium,
                tr("PG-13", "PL3 default search string.")));
        r2pl_defaults.insert(r2pl_map::value_type(ParentalLevel::plHigh,
                tr("R:NC-17", "PL4 default search string.")));

        VerticalConfigurationGroup *vcg = new VerticalConfigurationGroup(true);

        for (ParentalLevel pl(ParentalLevel::plLowest);
             pl.GetLevel() <= ParentalLevel::plHigh && pl.good(); ++pl)
        {
            HostLineEdit *hle = new HostLineEdit(QString("mythvideo.AutoR2PL%1")
                                                 .arg(pl.GetLevel()));
            hle->setLabel(QObject::tr("Level %1").arg(pl.GetLevel()));
            hle->setHelpText(QObject::tr("Ratings containing these strings "
                                         "(separated by :) will be assigned "
                                         "to Parental Level %1.")
                             .arg(pl.GetLevel()));

            r2pl_map::const_iterator def_setting =
                    r2pl_defaults.find(pl.GetLevel());
            if (def_setting != r2pl_defaults.end())
            {
                hle->setValue(def_setting->second);
            }

            vcg->addChild(hle);
        }

        addTarget("0", new VerticalConfigurationGroup(true));
        addTarget("1", vcg);
    }
};

class RandomTrailers : public TriggeredConfigurationGroup
{
  public:
    RandomTrailers() : TriggeredConfigurationGroup(false)
    {
        HostCheckBox *rt = new HostCheckBox("mythvideo.TrailersRandomEnabled");
        rt->setLabel(QObject::tr("Enable random trailers before videos"));
        rt->setValue(false);
        rt->setHelpText(QObject::tr("If set, this will enable a button "
                        "called \"Watch With Trailers\" which will "
                        "play a user-specified number of trailers "
                        "before the movie."));

        addChild(rt);
        setTrigger(rt);

        VerticalConfigurationGroup *vcg = new VerticalConfigurationGroup(true);
        HostSpinBox *rc = new HostSpinBox("mythvideo.TrailersRandomCount", 0,
                10, 1);
        rc->setLabel(QObject::tr("Number of trailers to play"));
        rc->setValue(3);
        rc->setHelpText(QObject::tr("The number of trailers to play "
                        "before playing the film itself "));
        vcg->addChild(rc);

        addTarget("0", new VerticalConfigurationGroup(true));
        addTarget("1", vcg);
    }
};

} // namespace

VideoGeneralSettings::VideoGeneralSettings()
{
    ConfigPage::PageList pages;

    VConfigPage page1(pages, false);
    page1->addChild(VideoStartupDirectory());
    page1->addChild(VideoArtworkDirectory());
    page1->addChild(VideoScreenshotDirectory()); 
    page1->addChild(VideoBannerDirectory()); 
    page1->addChild(VideoFanartDirectory()); 
    page1->addChild(VideoDefaultView());

    VConfigPage page2(pages, false);
    page2->addChild(VideoListUnknownFiletypes());
    page2->addChild(VideoTreeNoMetaData()); 
    page2->addChild(VideoNewBrowsable());
    page2->addChild(VideoSortIgnoresCase());
    page2->addChild(VideoDBGroupView());
    page2->addChild(VideoTreeRemember());
    page2->addChild(VideoTreeGroup());

    VConfigPage page3(pages, false);
    page3->addChild(SetDVDDevice());
    page3->addChild(SetVCDDevice());
    page3->addChild(SetOnInsertDVD());
    page3->addChild(SetDVDDriveSpeed());
    page3->addChild(new DVDBookmarkSettings());

    // page 4
    VerticalConfigurationGroup *vman =
            new VerticalConfigurationGroup(true, false);
    vman->setLabel(QObject::tr("Video Manager"));
    vman->addChild(SearchListingsCommand());
    vman->addChild(GetPostersCommand());
    vman->addChild(GetFanartCommand());
    vman->addChild(GetDataCommand());

    VConfigPage page4(pages, false);
    page4->addChild(vman);

    // page 5
    VerticalConfigurationGroup *pctrl =
            new VerticalConfigurationGroup(true, false);
    pctrl->addChild(VideoDefaultParentalLevel());
    pctrl->addChild(VideoAdminPassword());
    pctrl->addChild(VideoAdminPasswordThree());
    pctrl->addChild(VideoAdminPasswordTwo());
    pctrl->addChild(VideoAggressivePC());
    VConfigPage page5(pages, false);
    page5->addChild(pctrl);

    VConfigPage page6(pages, false);
    page6->addChild(new RatingsToPL());

    // page 7
    VerticalConfigurationGroup *trlr =
            new VerticalConfigurationGroup(true, false);
    trlr->addChild(TrailerDirectory());
    trlr->addChild(new RandomTrailers());
    VConfigPage page7(pages, false);
    page7->addChild(trlr);

    // page 8
    VerticalConfigurationGroup *tvman =
            new VerticalConfigurationGroup(true, false);
    tvman->setLabel(QObject::tr("Television in MythVideo"));
    tvman->addChild(SearchTVListingsCommand());
    tvman->addChild(GetTVPostersCommand());
    tvman->addChild(GetTVFanartCommand());
    tvman->addChild(GetTVBannerCommand());
    tvman->addChild(GetTVDataCommand());
    tvman->addChild(GetTVTitleSubCommand());
    tvman->addChild(GetTVScreenshotCommand());

    VConfigPage page8(pages, false);
    page8->addChild(tvman);

    int page_num = 1;
    for (ConfigPage::PageList::const_iterator p = pages.begin();
         p != pages.end(); ++p, ++page_num)
    {
        (*p)->setLabel(QObject::tr("General Settings (%1/%2)").arg(page_num)
                       .arg(pages.size()));
        addChild(*p);
    }
}

VideoPlayerSettings::VideoPlayerSettings()
{
    VerticalConfigurationGroup *videoplayersettings =
            new VerticalConfigurationGroup(false);
    videoplayersettings->setLabel(QObject::tr("Player Settings"));
    videoplayersettings->addChild(VideoDefaultPlayer());
    videoplayersettings->addChild(PlayerCommand());
    videoplayersettings->addChild(VCDPlayerCommand());
    videoplayersettings->addChild(new AlternatePlayerSettings());
    addChild(videoplayersettings);
}

DVDRipperSettings::DVDRipperSettings()
{
    VerticalConfigurationGroup *rippersettings =
            new VerticalConfigurationGroup(false);
    rippersettings->setLabel(QObject::tr("DVD Ripper Settings"));
    rippersettings->addChild(SetRipDirectory());
    rippersettings->addChild(TitlePlayCommand());
    rippersettings->addChild(SubTitleCommand());
    rippersettings->addChild(TranscodeCommand());
    addChild(rippersettings);

    VerticalConfigurationGroup *mtdsettings =
            new VerticalConfigurationGroup(false);
    mtdsettings->setLabel(QObject::tr("MTD Settings"));
    mtdsettings->addChild(MTDPortNumber());
    mtdsettings->addChild(MTDNiceLevel());
    mtdsettings->addChild(MTDConcurrentTranscodes());
    mtdsettings->addChild(MTDRipSize());
    mtdsettings->addChild(MTDLogFlag());
    mtdsettings->addChild(MTDac3Flag());
    mtdsettings->addChild(MTDxvidFlag());
    mtdsettings->addChild(MTDTrustTranscodeFRDetect());
    addChild(mtdsettings);
}
