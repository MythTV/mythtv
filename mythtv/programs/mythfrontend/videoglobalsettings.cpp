#include <map>
#include <vector>

#include <QDir>

#include "mythdirs.h"
#include "globals.h"

#include "videodlg.h"
#include "videoglobalsettings.h"

namespace
{
// General Settings
HostComboBoxSetting *VideoDefaultParentalLevel()
{
    HostComboBoxSetting *gc = new HostComboBoxSetting("VideoDefaultParentalLevel");

    gc->setLabel(VideoGeneralSettings::tr("Starting Parental Level"));

    gc->addSelection(VideoGeneralSettings::tr("4 - Highest"),
                     QString::number(ParentalLevel::plHigh));
    gc->addSelection(VideoGeneralSettings::tr("1 - Lowest"),
                     QString::number(ParentalLevel::plLowest));
    gc->addSelection(QString::number(ParentalLevel::plLow));
    gc->addSelection(QString::number(ParentalLevel::plMedium));

    gc->setHelpText(VideoGeneralSettings::tr("This is the 'level' that Video "
                                             "starts at. Any videos with a "
                                             "level at or below this will be "
                                             "shown in the list or while "
                                             "browsing by default. The "
                                             "Parental PIN should be set to "
                                             "limit changing of the default "
                                             "level."));
    return gc;
}

const char *password_clue =
    QT_TRANSLATE_NOOP("VideoGeneralSettings", "Setting this value to all "
                                              "numbers will make your life "
                                              "much easier.");

HostTextEditSetting *VideoAdminPassword()
{
    HostTextEditSetting *gc = new HostTextEditSetting("VideoAdminPassword");

    gc->setLabel(VideoGeneralSettings::tr("Parental Level 4 PIN"));

    gc->setHelpText(QString("%1 %2")
        .arg(VideoGeneralSettings::tr("This PIN is used to enter Parental "
                                      "Control Level 4 as well as the Video "
                                      "Manager."))
        .arg(VideoGeneralSettings::tr(password_clue)));

    return gc;
}

HostTextEditSetting *VideoAdminPasswordThree()
{
    HostTextEditSetting *gc = new HostTextEditSetting("VideoAdminPasswordThree");

    gc->setLabel(VideoGeneralSettings::tr("Parental Level 3 PIN"));

    gc->setHelpText(QString("%1 %2")
        .arg(VideoGeneralSettings::tr("This PIN is used to enter Parental "
                                      "Control Level 3."))
        .arg(VideoGeneralSettings::tr(password_clue)));

    return gc;
}

HostTextEditSetting *VideoAdminPasswordTwo()
{
    HostTextEditSetting *gc = new HostTextEditSetting("VideoAdminPasswordTwo");

    gc->setLabel(VideoGeneralSettings::tr("Parental Level 2 PIN"));

    gc->setHelpText(QString("%1 %2")
        .arg(VideoGeneralSettings::tr("This PIN is used to enter Parental "
                                      "Control Level 2."))
        .arg(VideoGeneralSettings::tr(password_clue)));

    return gc;
}

HostCheckBoxSetting *VideoAggressivePC()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("VideoAggressivePC");

    gc->setLabel(VideoGeneralSettings::tr("Aggressive Parental Control"));
    gc->setValue(false);

    gc->setHelpText(VideoGeneralSettings::tr("If set, you will not be able to "
                                             "return to this screen and reset "
                                             "the Parental PIN without first "
                                             "entering the current PIN. You "
                                             "have been warned."));
    return gc;
}

HostTextEditSetting *VideoStartupDirectory()
{
    HostTextEditSetting *gc = new HostTextEditSetting("VideoStartupDir");

    gc->setLabel(VideoGeneralSettings::tr("Directories that hold videos"));

    gc->setValue(DEFAULT_VIDEOSTARTUP_DIR);

    gc->setHelpText(VideoGeneralSettings::tr("Multiple directories can be "
                                             "separated by ':'. Each "
                                             "directory must exist and be "
                                             "readable by the user running "
                                             "the frontend."));
    return gc;
}

HostTextEditSetting *VideoArtworkDirectory()
{
    HostTextEditSetting *gc = new HostTextEditSetting("VideoArtworkDir");

    gc->setLabel(VideoGeneralSettings::tr("Directory that holds movie "
                                          "posters"));

    gc->setValue(GetConfDir() + "/Video/Artwork");

    gc->setHelpText(VideoGeneralSettings::tr("This directory must exist, and "
                                             "the user running the frontend "
                                             "needs to have read/write "
                                             "permission to the directory."));
    return gc;
}

HostTextEditSetting *VideoScreenshotDirectory()
{
    HostTextEditSetting *gc = new HostTextEditSetting("mythvideo.screenshotDir");

    gc->setLabel(VideoGeneralSettings::tr("Directory that holds movie "
                                          "screenshots"));

    gc->setValue(GetConfDir() + "/Video/Screenshots");

    gc->setHelpText(VideoGeneralSettings::tr("This directory must exist, and "
                                             "the user running the frontend "
                                             "needs to have read/write "
                                             "permission to the directory."));
    return gc;
}

HostTextEditSetting *VideoBannerDirectory()
{
    HostTextEditSetting *gc = new HostTextEditSetting("mythvideo.bannerDir");

    gc->setLabel(VideoGeneralSettings::tr("Directory that holds movie/TV "
                                          "Banners"));

    gc->setValue(GetConfDir() + "/Video/Banners");

    gc->setHelpText(VideoGeneralSettings::tr("This directory must exist, and "
                                             "the user running the frontend "
                                             "needs to have read/write "
                                             "permission to the directory."));
    return gc;
}

HostTextEditSetting *VideoFanartDirectory()
{
    HostTextEditSetting *gc = new HostTextEditSetting("mythvideo.fanartDir");

    gc->setLabel(VideoGeneralSettings::tr("Directory that holds movie fanart"));

    gc->setValue(GetConfDir() + "/Video/Fanart");

    gc->setHelpText(VideoGeneralSettings::tr("This directory must exist, and "
                                             "the user running the frontend "
                                             "needs to have read/write "
                                             "permission to the directory."));
    return gc;
}

HostTextEditSetting *TrailerDirectory()
{
    HostTextEditSetting *gc = new HostTextEditSetting("mythvideo.TrailersDir");

    gc->setLabel(VideoGeneralSettings::tr("Directory that holds movie "
                                          "trailers"));

    gc->setValue(GetConfDir() + "/Video/Trailers");

    gc->setHelpText(VideoGeneralSettings::tr("This directory must exist, and "
                                             "the user running the frontend "
                                             "needs to have read/write "
                                             "permission to the directory."));
    return gc;
}

///////////////////////////////////////////////////////////
//// DVD Settings
///////////////////////////////////////////////////////////

// General Settings

HostComboBoxSetting *SetOnInsertDVD()
{
    HostComboBoxSetting *gc = new HostComboBoxSetting("DVDOnInsertDVD");

    gc->setLabel(VideoGeneralSettings::tr("On DVD insertion"));

    gc->addSelection(VideoGeneralSettings::tr("Display mythdvd menu"),"1");
    gc->addSelection(VideoGeneralSettings::tr("Do nothing"),"0");
    gc->addSelection(VideoGeneralSettings::tr("Play DVD"),"2");

    gc->setHelpText(VideoGeneralSettings::tr("Media Monitoring should be "
                                             "turned on to allow this feature "
                                             "(Setup -> General -> CD/DVD "
                                             "Monitor)."));
    return gc;
}

HostCheckBoxSetting *VideoTreeRemember()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("mythvideo.VideoTreeRemember");

    gc->setLabel(VideoGeneralSettings::tr("Video Tree remembers last selected "
                                          "position"));

    gc->setValue(false);

    gc->setHelpText(VideoGeneralSettings::tr("If set, the current position in "
                                             "the Video Tree is persistent."));
    return gc;
}

struct ConfigPage
{
    typedef std::vector<ConfigurationGroup *> PageList;

  protected:
    explicit ConfigPage(PageList &pl) : m_pl(pl)
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

HostCheckBoxSetting *RatingsToPL()
{
    HostCheckBoxSetting *r2pl =
        new HostCheckBoxSetting("mythvideo.ParentalLevelFromRating");

    r2pl->setLabel(VideoGeneralSettings::tr("Enable automatic Parental "
                                            "Level from rating"));
    r2pl->setValue(false);

    r2pl->setHelpText(VideoGeneralSettings::tr("If enabled, searches will "
                                               "automatically set the "
                                               "Parental Level to the one "
                                               "matching the rating "
                                               "below."));

    typedef std::map<ParentalLevel::Level, QString> r2pl_map;

    r2pl_map r2pl_defaults;

    r2pl_defaults.insert(r2pl_map::value_type(ParentalLevel::plLowest,
                                              VideoGeneralSettings::tr("G", "PL 1 default search string.")));
    r2pl_defaults.insert(r2pl_map::value_type(ParentalLevel::plLow,
                                              VideoGeneralSettings::tr("PG", "PL 2 default search string.")));
    r2pl_defaults.insert(r2pl_map::value_type(ParentalLevel::plMedium,
                                              VideoGeneralSettings::tr("PG-13", "PL3 default search "
                                                                       "string.")));
    r2pl_defaults.insert(r2pl_map::value_type(ParentalLevel::plHigh,
                                              VideoGeneralSettings::tr("R:NC-17", "PL4 default search "
                                                                       "string.")));

    for (ParentalLevel pl(ParentalLevel::plLowest);
         pl.GetLevel() <= ParentalLevel::plHigh && pl.good(); ++pl)
    {
        HostTextEditSetting *hle = new HostTextEditSetting(QString("mythvideo.AutoR2PL%1")
                                                           .arg(pl.GetLevel()));

        hle->setLabel(VideoGeneralSettings::tr("Level %1")
                      .arg(pl.GetLevel()));

        hle->setHelpText(VideoGeneralSettings::tr("Ratings containing "
                                                  "these strings "
                                                  "(separated by :) will "
                                                  "be assigned to Parental "
                                                  "Level %1.")
                         .arg(pl.GetLevel()));

        r2pl_map::const_iterator def_setting =
            r2pl_defaults.find(pl.GetLevel());

        if (def_setting != r2pl_defaults.end())
        {
            hle->setValue(def_setting->second);
        }

        r2pl->addTargetedChild("1", hle);
    }

    return r2pl;
}

} // namespace

VideoGeneralSettings::VideoGeneralSettings()
    : GroupSetting()
{
    setLabel(tr("General Settings"));

    addChild(VideoStartupDirectory());
    addChild(TrailerDirectory());
    addChild(VideoArtworkDirectory());
    addChild(VideoScreenshotDirectory());
    addChild(VideoBannerDirectory());
    addChild(VideoFanartDirectory());

    addChild(SetOnInsertDVD());
    addChild(VideoTreeRemember());

    GroupSetting *pctrl = new GroupSetting();
    pctrl->setLabel(tr("Parental Control Settings"));
    pctrl->addChild(VideoDefaultParentalLevel());
    pctrl->addChild(VideoAdminPassword());
    pctrl->addChild(VideoAdminPasswordThree());
    pctrl->addChild(VideoAdminPasswordTwo());
    pctrl->addChild(VideoAggressivePC());
    pctrl->addChild(RatingsToPL());

    addChild(pctrl);
}
