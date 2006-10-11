#include <mythtv/mythcontext.h>

#include "globalsettings.h"
#include "globals.h"

// General Settings

static HostComboBox *VideoDefaultParentalLevel()
{
    HostComboBox *gc = new HostComboBox("VideoDefaultParentalLevel");
    gc->setLabel(QObject::tr("Starting Parental Level"));
    gc->addSelection(QObject::tr("4 - Highest"), "4");
    gc->addSelection(QObject::tr("1 - Lowest"), "1");
    gc->addSelection("2");
    gc->addSelection("3");
    gc->setHelpText(QObject::tr("This is the 'level' that MythVideo starts at. "
                    "Any videos with a level at or below this will be shown in "
                    "the list or while browsing by default. The Parental PIN "
                    "should be set to limit changing of the default level."));
    return gc;
}


static HostComboBox *VideoDefaultView()
{
    HostComboBox *gc = new HostComboBox("Default MythVideo View");
    gc->setLabel(QObject::tr("Default View"));
    gc->addSelection(QObject::tr("Gallery"), "1");
    gc->addSelection(QObject::tr("Browser"), "0");
    gc->addSelection(QObject::tr("Listings"), "2");
    gc->setHelpText(QObject::tr("The default view for MythVideo. "
                    "Other views can be reached via the popup menu available "
                    "via the MENU key."));
    return gc;
}


static HostLineEdit *VideoAdminPassword()
{
    HostLineEdit *gc = new HostLineEdit("VideoAdminPassword");
    gc->setLabel(QObject::tr("Parental Control PIN"));
    gc->setHelpText(QObject::tr("This PIN is used to control the current "
                    "Parental Level. If you want to use this feature, then "
                    "setting the value to all numbers will make your life much "
                    "easier."));
    return gc;
}

static HostCheckBox *VideoAggressivePC()
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

static HostCheckBox *VideoListUnknownFiletypes()
{
    HostCheckBox *gc = new HostCheckBox("VideoListUnknownFiletypes");
    gc->setLabel(QObject::tr("Show Unknown File Types"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If set, all files below the Myth Video "
                    "directory will be displayed unless their "
                    "extension is explicitly set to be ignored. "));
    return gc;
}

static HostCheckBox *VideoBrowserNoDB()
{
    HostCheckBox *gc = new HostCheckBox("VideoBrowserNoDB");
    gc->setLabel(QObject::tr("Video Browser browses files"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If set, this will cause the Video Browser "
                    "screen to show all relevant files below "
                    "the MythVideo starting directory whether "
                    "they have been scanned or not."));
    return gc;
}

static HostCheckBox *VideoGalleryNoDB()
{
    HostCheckBox *gc = new HostCheckBox("VideoGalleryNoDB");
    gc->setLabel(QObject::tr("Video Gallery browses files"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If set, this will cause the Video Gallery "
                    "screen to show all relevant files below "
                    "the MythVideo starting directory whether "
                    "they have been scanned or not."));
    return gc;
}

static HostCheckBox *VideoTreeNoDB()
{
    HostCheckBox *gc = new HostCheckBox("VideoTreeNoDB");
    gc->setLabel(QObject::tr("Video List browses files"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If set, this will cause the Video List "
                    "screen to show all relevant files below "
                    "the MythVideo starting directory whether "
                    "they have been scanned or not."));
    return gc;
}

static HostCheckBox *VideoTreeNoMetaData()
{
    HostCheckBox *gc = new HostCheckBox("VideoTreeLoadMetaData");
    gc->setLabel(QObject::tr("Video List Loads Video Meta Data"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If set along with Browse Files, this "
                    "will cause the Video List to load any known video meta"
                    "data from the database. Turning this off can greatly "
                    " speed up how long it takes to load the Video List tree"));
    return gc;
}


static HostCheckBox *VideoNewBrowsable()
{
    HostCheckBox *gc = new HostCheckBox("VideoNewBrowsable");
    gc->setLabel(QObject::tr("Newly scanned files are browsable by default"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If set, newly scanned files in the Video "
                    "Manager will be marked as browsable and will appear in "
                    "the 'Browse' menu."));
    return gc;
}

namespace
{
    HostCheckBox *VideoSortIgnoresCase()
    {
        HostCheckBox *hcb = new HostCheckBox("mythvideo.sort_ignores_case");
        hcb->setLabel(QObject::tr("Sorting ignores case"));
        hcb->setValue(true);
        hcb->setHelpText(QObject::tr("If set, case is ignored when sorting "
                                     "entries in a view."));
        return hcb;
    }

    HostCheckBox *VideoDBFolderView()
    {
        HostCheckBox *hcb = new HostCheckBox("mythvideo.db_folder_view");
        hcb->setLabel(QObject::tr("Show folders for database views"));
        hcb->setValue(true);
        hcb->setHelpText(QObject::tr("If set, sub folders of your video "
                                     "directory will be shown in supported "
                                     "views."));
        return hcb;
    }

    HostSpinBox *VideoImageCacheSize()
    {
        HostSpinBox *hsb = new HostSpinBox("mythvideo.ImageCacheSize", 10,
                                           1000, 10);
        hsb->setValue(50);
        hsb->setLabel(QObject::tr("Image cache size"));
        hsb->setHelpText(QObject::tr("This setting determines the number "
                                     "of images MythVideo will cache for "
                                     "views."));
        return hsb;
    }

}

static HostLineEdit *SearchListingsCommand()
{
    HostLineEdit *gc = new HostLineEdit("MovieListCommandLine");
    gc->setLabel(QObject::tr("Command to search for movie listings"));
    gc->setValue(gContext->GetShareDir() + "mythvideo/scripts/imdb.pl " +
                 "-M tv=no;video=no");
    gc->setHelpText(QObject::tr("This command must be "
                    "executable by the user running MythVideo."));
    return gc;
}


static HostLineEdit *GetPostersCommand()
{
    HostLineEdit *gc = new HostLineEdit("MoviePosterCommandLine");
    gc->setLabel(QObject::tr("Command to search for movie posters"));
    gc->setValue(gContext->GetShareDir() + "mythvideo/scripts/imdb.pl -P");
    gc->setHelpText(QObject::tr("This command must be "
                    "executable by the user running MythVideo."));
    return gc;
}


static HostLineEdit *GetDataCommand()
{
    HostLineEdit *gc = new HostLineEdit("MovieDataCommandLine");
    gc->setLabel(QObject::tr("Command to extract data for movies"));
    gc->setValue(gContext->GetShareDir() + "mythvideo/scripts/imdb.pl -D");
    gc->setHelpText(QObject::tr("This command must be "
                    "executable by the user running MythVideo."));
    return gc;
}


static HostLineEdit *VideoStartupDirectory()
{
    HostLineEdit *gc = new HostLineEdit("VideoStartupDir");
    gc->setLabel(QObject::tr("Directory that holds videos"));
    gc->setValue(DEFAULT_VIDEOSTARTUP_DIR);
    gc->setHelpText(QObject::tr("This directory must exist, and the user "
                    "running MythVideo only needs to have read permission "
                    "to the directory."));
    return gc;
}


static HostLineEdit *VideoArtworkDirectory()
{
    HostLineEdit *gc = new HostLineEdit("VideoArtworkDir");
    gc->setLabel(QObject::tr("Directory that holds movie posters"));
    gc->setValue(MythContext::GetConfDir() + "/MythVideo");
    gc->setHelpText(QObject::tr("This directory must exist, and the user "
                    "running MythVideo needs to have read/write permission "
                    "to the directory."));
    return gc;
}

//Player Settings

static HostLineEdit *VideoDefaultPlayer()
{
    HostLineEdit *gc = new HostLineEdit("VideoDefaultPlayer");
    gc->setLabel(QObject::tr("Default Player"));
    gc->setValue("mplayer -fs -zoom -quiet -vo xv %s");
    gc->setHelpText(QObject::tr("This is the command used for any file "
                    "that the extension is not specifically defined. "
                    "You may also enter the name of one of the playback "
                    "plugins such as 'Internal'."));
    return gc;
}

static HostSpinBox *VideoGalleryRows()
{
    HostSpinBox *gc = new HostSpinBox("VideoGalleryRowsPerPage", 2, 5, 1);
    gc->setLabel(QObject::tr("Rows to display"));
    gc->setValue(3);
    return gc;
}

static HostSpinBox *VideoGalleryColumns()
{
    HostSpinBox *gc = new HostSpinBox("VideoGalleryColsPerPage", 2, 6, 1);
    gc->setLabel(QObject::tr("Columns to display"));
    gc->setValue(4);
    return gc;
}

static HostCheckBox *VideoGallerySubtitle()
{
    HostCheckBox *gc = new HostCheckBox("VideoGallerySubtitle");
    gc->setLabel(QObject::tr("Show title below thumbnails"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If set, the additional text will make the "
                    "thumbnails smaller."));
    return gc;
}

static HostCheckBox *VideoGalleryAspectRatio()
{
    HostCheckBox *gc = new HostCheckBox("VideoGalleryAspectRatio");
    gc->setLabel(QObject::tr("Maintain aspect ratio of thumbnails"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If set, the scaled thumbnails will maintain "
                    "their original aspect ratio. If not set, they are scaled "
                    "to match the size of the background icon."));
    return gc;
}

VideoGeneralSettings::VideoGeneralSettings()
{
    VerticalConfigurationGroup* general = new VerticalConfigurationGroup(false);
    general->setLabel(QObject::tr("General Settings (1/2)"));
    general->addChild(VideoStartupDirectory());
    general->addChild(VideoArtworkDirectory());
    general->addChild(VideoDefaultParentalLevel());
    general->addChild(VideoAdminPassword());
    general->addChild(VideoAggressivePC());
    general->addChild(VideoDefaultView());
    addChild(general);

    VerticalConfigurationGroup* general2 =
            new VerticalConfigurationGroup(false);
    general2->setLabel(QObject::tr("General Settings (2/2)"));
    general2->addChild(VideoListUnknownFiletypes());
    general2->addChild(VideoBrowserNoDB());
    general2->addChild(VideoGalleryNoDB());
    general2->addChild(VideoTreeNoDB());
    general2->addChild(VideoTreeNoMetaData());
    general2->addChild(VideoNewBrowsable());
    general2->addChild(VideoSortIgnoresCase());
    general2->addChild(VideoDBFolderView());
    general2->addChild(VideoImageCacheSize());
    addChild(general2);

    VerticalConfigurationGroup* vman = new VerticalConfigurationGroup(false);
    vman->setLabel(QObject::tr("Video Manager"));
    vman->addChild(SearchListingsCommand());
    vman->addChild(GetPostersCommand());
    vman->addChild(GetDataCommand());
    addChild(vman);

    VerticalConfigurationGroup * vgal = new VerticalConfigurationGroup(false);
    vgal->setLabel(QObject::tr("Video Gallery"));
    vgal->addChild(VideoGalleryColumns());
    vgal->addChild(VideoGalleryRows());
    vgal->addChild(VideoGallerySubtitle());
    vgal->addChild(VideoGalleryAspectRatio());
    addChild(vgal);
}

VideoPlayerSettings::VideoPlayerSettings()
{
    VerticalConfigurationGroup *playersettings =
            new VerticalConfigurationGroup(false);
    playersettings->setLabel(QObject::tr("Player Settings"));
    playersettings->addChild(VideoDefaultPlayer());
    addChild(playersettings);
}
