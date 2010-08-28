#include <map>
#include <vector>

#include <QDir>

#include <mythdirs.h>
#include <video/globals.h>

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
    page2->addChild(DVDBookmarkDays());

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
