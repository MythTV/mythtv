#include <map>
#include <vector>

#include <QDir>

#include <mythdirs.h>

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

const char *password_clue =
    QT_TRANSLATE_NOOP("QObject", "Setting this value to all numbers will make your life "
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

///////////////////////////////////////////////////////////
//// DVD Settings
///////////////////////////////////////////////////////////

// General Settings

HostComboBox *SetOnInsertDVD()
{
    HostComboBox *gc = new HostComboBox("DVDOnInsertDVD");
    gc->setLabel(QObject::tr("On DVD insertion"));
    gc->addSelection(QObject::tr("Display mythdvd menu"),"1");
    gc->addSelection(QObject::tr("Do nothing"),"0");
    gc->addSelection(QObject::tr("Play DVD"),"2");
    gc->addSelection(QObject::tr("Rip DVD"),"3");
    gc->setHelpText(QObject::tr("Media Monitoring should be turned on to "
                   "allow this feature (Setup -> General -> CD/DVD Monitor)."));
    return gc;
}

HostSlider *DVDBookmarkDays()
{
    HostSlider *gs = new HostSlider("DVDBookmarkDays",5, 50, 5);
    gs->setLabel(QObject::tr("Remove DVD Bookmarks Older than (days)"));
    gs->setValue(10);
    gs->setHelpText((QObject::tr("Delete DVD Bookmarks that are older than the "
                                 "number of days specified.")));
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
    gc->setHelpText(QObject::tr("Enable the setting and skipping to "
                                "of a bookmark in DVD playback."));
    return gc;
}

HostCheckBox *DVDBookmarkPrompt()
{
    HostCheckBox *gc = new HostCheckBox("DVDBookmarkPrompt");
    gc->setLabel(QObject::tr("DVD Bookmark Prompt"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("Display a prompt to choose whether "
                "to play the DVD from the beginning or from the bookmark."));
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

} // namespace

VideoGeneralSettings::VideoGeneralSettings()
{
    ConfigPage::PageList pages;

    VConfigPage page1(pages, false);
    page1->addChild(VideoStartupDirectory());
    page1->addChild(TrailerDirectory());
    page1->addChild(VideoArtworkDirectory());
    page1->addChild(VideoScreenshotDirectory());
    page1->addChild(VideoBannerDirectory());
    page1->addChild(VideoFanartDirectory());

    VConfigPage page2(pages, false);
    page2->addChild(SetOnInsertDVD());
    page2->addChild(SetDVDDriveSpeed());
    page2->addChild(new DVDBookmarkSettings());

    // page 3
    VerticalConfigurationGroup *pctrl =
            new VerticalConfigurationGroup(true, false);
    pctrl->setLabel(QObject::tr("Parental Control Settings"));
    pctrl->addChild(VideoDefaultParentalLevel());
    pctrl->addChild(VideoAdminPassword());
    pctrl->addChild(VideoAdminPasswordThree());
    pctrl->addChild(VideoAdminPasswordTwo());
    pctrl->addChild(VideoAggressivePC());
    VConfigPage page3(pages, false);
    page3->addChild(pctrl);

    VConfigPage page4(pages, false);
    page4->addChild(new RatingsToPL());

    int page_num = 1;
    for (ConfigPage::PageList::const_iterator p = pages.begin();
         p != pages.end(); ++p, ++page_num)
    {
        (*p)->setLabel(QObject::tr("General Settings (%1/%2)").arg(page_num)
                       .arg(pages.size()));
        addChild(*p);
    }
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
