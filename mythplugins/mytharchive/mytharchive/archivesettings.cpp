/*
    archivesettings.cpp
*/

#include <unistd.h>

// myth
#include <libmyth/mythcontext.h>
#include <libmythbase/mythdirs.h>
#include <libmythbase/mythdate.h>

// mytharchive
#include "archivesettings.h"


static HostFileBrowserSetting *MythArchiveTempDir()
{
    auto *gc = new HostFileBrowserSetting("MythArchiveTempDir");

    gc->setLabel(ArchiveSettings::tr("MythArchive Temp Directory"));
    gc->setValue("");

    gc->setHelpText(ArchiveSettings::tr("Location where MythArchive should "
                                        "create its temporary work files. "
                                        "LOTS of free space required here."));
    gc->SetTypeFilter(QDir::AllDirs | QDir::Hidden);

    return gc;
};

static HostFileBrowserSetting *MythArchiveShareDir()
{
    auto *gc = new HostFileBrowserSetting("MythArchiveShareDir");

    gc->setLabel(ArchiveSettings::tr("MythArchive Share Directory"));
    gc->setValue(GetShareDir() + "mytharchive/");

    gc->setHelpText(ArchiveSettings::tr("Location where MythArchive stores its "
                                        "scripts, intro movies and theme "
                                        "files"));
    gc->SetTypeFilter(QDir::AllDirs | QDir::Hidden);

    return gc;
};

static HostComboBoxSetting *PALNTSC()
{
    auto *gc = new HostComboBoxSetting("MythArchiveVideoFormat");

    gc->setLabel(ArchiveSettings::tr("Video format"));

    gc->addSelection("PAL");
    gc->addSelection("NTSC");

    gc->setHelpText(ArchiveSettings::tr("Video format for DVD recordings, PAL "
                                        "or NTSC."));
    return gc;
};

static HostTextEditSetting *MythArchiveFileFilter()
{
    auto *gc = new HostTextEditSetting("MythArchiveFileFilter");

    gc->setLabel(ArchiveSettings::tr("File Selector Filter"));
    gc->setValue("*.mpg *.mov *.avi *.mpeg *.nuv");

    gc->setHelpText(ArchiveSettings::tr("The file name filter to use in the "
                                        "file selector."));
    return gc;
};

static HostFileBrowserSetting *MythArchiveDVDLocation()
{
    auto *gc = new HostFileBrowserSetting("MythArchiveDVDLocation");

    gc->setLabel(ArchiveSettings::tr("Location of DVD"));
    gc->setValue("/dev/dvd");

    gc->setHelpText(ArchiveSettings::tr("Which DVD drive to use when burning "
                                        "discs."));
    gc->SetTypeFilter(QDir::AllDirs | QDir::Files | QDir::System | QDir::Hidden);
    return gc;
};

static HostSpinBoxSetting *MythArchiveDriveSpeed()
{
    auto *gc = new HostSpinBoxSetting("MythArchiveDriveSpeed", 0, 48, 1);

    gc->setLabel(ArchiveSettings::tr("DVD Drive Write Speed"));
    gc->setValue(0);

    gc->setHelpText(ArchiveSettings::tr("This is the write speed to use when "
                                        "burning a DVD. Set to 0 to allow "
                                        "growisofs to choose the fastest "
                                        "available speed."));
    return gc;
};

static HostTextEditSetting *MythArchiveDVDPlayerCmd()
{
    auto *gc = new HostTextEditSetting("MythArchiveDVDPlayerCmd");

    gc->setLabel(ArchiveSettings::tr("Command to play DVD"));
    gc->setValue("Internal");

    gc->setHelpText(ArchiveSettings::tr("Command to run when test playing a "
                                        "created DVD. 'Internal' will use the "
                                        "internal MythTV player. %f will be "
                                        "replaced with the path to the created "
                                        "DVD structure eg. 'xine -pfhq "
                                        "--no-splash dvd:/%f'."));
    return gc;
};

static HostCheckBoxSetting *MythArchiveCopyRemoteFiles()
{
    auto *gc = new HostCheckBoxSetting("MythArchiveCopyRemoteFiles");

    gc->setLabel(ArchiveSettings::tr("Copy remote files"));
    gc->setValue(false);

    gc->setHelpText(ArchiveSettings::tr("If set files on remote filesystems "
                                        "will be copied over to the local "
                                        "filesystem before processing. Speeds "
                                        "processing and reduces bandwidth on "
                                        "the network"));
    return gc;
};

static HostCheckBoxSetting *MythArchiveAlwaysUseMythTranscode()
{
    auto *gc = new HostCheckBoxSetting("MythArchiveAlwaysUseMythTranscode");

    gc->setLabel(ArchiveSettings::tr("Always Use Mythtranscode"));
    gc->setValue(true);

    gc->setHelpText(ArchiveSettings::tr("If set mpeg2 files will always be "
                                        "passed though mythtranscode to clean "
                                        "up any errors. May help to fix some "
                                        "audio problems. Ignored if 'Use "
                                        "ProjectX' is set."));
    return gc;
};

static HostCheckBoxSetting *MythArchiveUseProjectX()
{
    auto *gc = new HostCheckBoxSetting("MythArchiveUseProjectX");

    gc->setLabel(ArchiveSettings::tr("Use ProjectX"));
    gc->setValue(false);

    gc->setHelpText(ArchiveSettings::tr("If set ProjectX will be used to cut "
                                        "commercials and split mpeg2 files "
                                        "instead of mythtranscode and "
                                        "mythreplex."));
    return gc;
};

static HostCheckBoxSetting *MythArchiveUseFIFO()
{
    auto *gc = new HostCheckBoxSetting("MythArchiveUseFIFO");

    gc->setLabel(ArchiveSettings::tr("Use FIFOs"));
    gc->setValue(true);
    
    gc->setHelpText(ArchiveSettings::tr("The script will use FIFOs to pass the "
                                        "output of mplex into dvdauthor rather "
                                        "than creating intermediate files. "
                                        "Saves time and disk space during "
                                        "multiplex operations but not "
                                        "supported on Windows platform"));
    return gc;
};

static HostCheckBoxSetting *MythArchiveAddSubtitles()
{
    auto *gc = new HostCheckBoxSetting("MythArchiveAddSubtitles");

    gc->setLabel(ArchiveSettings::tr("Add Subtitles"));
    gc->setValue(false);

    gc->setHelpText(ArchiveSettings::tr("If available this option will add "
                                        "subtitles to the final DVD. Requires "
                                        "'Use ProjectX' to be on."));
    return gc;
};

static HostComboBoxSetting *MainMenuAspectRatio()
{
    auto *gc = new HostComboBoxSetting("MythArchiveMainMenuAR");

    gc->setLabel(ArchiveSettings::tr("Main Menu Aspect Ratio"));

    gc->addSelection(ArchiveSettings::tr("4:3", "Aspect ratio"), "4:3");
    gc->addSelection(ArchiveSettings::tr("16:9", "Aspect ratio"), "16:9");

    gc->setValue(1);

    gc->setHelpText(ArchiveSettings::tr("Aspect ratio to use when creating the "
                                        "main menu."));
    return gc;
};

static HostComboBoxSetting *ChapterMenuAspectRatio()
{
    auto *gc = new HostComboBoxSetting("MythArchiveChapterMenuAR");

    gc->setLabel(ArchiveSettings::tr("Chapter Menu Aspect Ratio"));

    gc->addSelection(ArchiveSettings::tr("4:3", "Aspect ratio"), "4:3");
    gc->addSelection(ArchiveSettings::tr("16:9", "Aspect ratio"), "16:9");
    gc->addSelection(ArchiveSettings::tr("Video"), "Video");

    gc->setValue(2);

    //: %1 is the translation of the "Video" combo box choice
    gc->setHelpText(ArchiveSettings::tr("Aspect ratio to use when creating the "
                                        "chapter menu. '%1' means use the "
                                        "same aspect ratio as the associated "
                                        "video.")
                                        .arg(ArchiveSettings::tr("Video")));
    return gc;
};

static HostComboBoxSetting *MythArchiveDateFormat()
{
    auto *gc = new HostComboBoxSetting("MythArchiveDateFormat");
    gc->setLabel(ArchiveSettings::tr("Date format"));

    QDate sampdate = MythDate::current().toLocalTime().date();
    QString sampleStr = ArchiveSettings::tr("Samples are shown using today's "
                                            "date.");

    if (sampdate.month() == sampdate.day())
    {
        sampdate = sampdate.addDays(1);
        sampleStr = ArchiveSettings::tr("Samples are shown using tomorrow's "
                                        "date.");
    }

    gc->addSelection(sampdate.toString("ddd MMM d"), "%a  %b  %d");
    gc->addSelection(sampdate.toString("ddd MMMM d"), "%a %B %d");
    gc->addSelection(sampdate.toString("MMM d"), "%b %d");
    gc->addSelection(sampdate.toString("MM/dd"), "%m/%d");
    gc->addSelection(sampdate.toString("MM.dd"), "%m.%d");
    gc->addSelection(sampdate.toString("ddd d MMM"), "%a %d %b");
    gc->addSelection(sampdate.toString("M/d/yyyy"), "%m/%d/%Y");
    gc->addSelection(sampdate.toString("dd.MM.yyyy"), "%d.%m.%Y");
    gc->addSelection(sampdate.toString("yyyy-MM-dd"), "%Y-%m-%d");
    gc->addSelection(sampdate.toString("ddd MMM d yyyy"), "%a %b %d %Y");
    gc->addSelection(sampdate.toString("ddd yyyy-MM-dd"), "%a %Y-%m-%d");
    gc->addSelection(sampdate.toString("ddd dd MMM yyyy"), "%a %d %b %Y");

    //: %1 gives additional info on the date used
    gc->setHelpText(ArchiveSettings::tr("Your preferred date format to use on "
                                        "DVD menus. %1")
                                        .arg(sampleStr));
    return gc;
}

static HostComboBoxSetting *MythArchiveTimeFormat()
{
    auto *gc = new HostComboBoxSetting("MythArchiveTimeFormat");
    gc->setLabel(ArchiveSettings::tr("Time format"));

    QTime samptime = QTime::currentTime();

    gc->addSelection(samptime.toString("hh:mm AP"), "%I:%M %p");
    gc->addSelection(samptime.toString("hh:mm"), "%H:%M");

    gc->setHelpText(ArchiveSettings::tr("Your preferred time format to display "
                                        "on DVD menus. You must choose a "
                                        "format with \"AM\" or \"PM\" in it, "
                                        "otherwise your time display will be "
                                        "24-hour or \"military\" time."));
    return gc;
}

static HostComboBoxSetting *MythArchiveDefaultEncProfile()
{
    auto *gc = new HostComboBoxSetting("MythArchiveDefaultEncProfile");
    gc->setLabel(ArchiveSettings::tr("Default Encoder Profile"));

    gc->addSelection(ArchiveSettings::tr("HQ", "Encoder profile"), "HQ");
    gc->addSelection(ArchiveSettings::tr("SP", "Encoder profile"), "SP");
    gc->addSelection(ArchiveSettings::tr("LP", "Encoder profile"), "LP");
    gc->addSelection(ArchiveSettings::tr("EP", "Encoder profile"), "EP");

    gc->setValue(1);

    gc->setHelpText(ArchiveSettings::tr("Default encoding profile to use if a "
                                        "file needs re-encoding."));
    return gc;
}

static HostTextEditSetting *MythArchiveMplexCmd()
{
    auto *gc = new HostTextEditSetting("MythArchiveMplexCmd");

    gc->setLabel(ArchiveSettings::tr("mplex Command"));

    gc->setValue("mplex");

    gc->setHelpText(ArchiveSettings::tr("Command to run mplex"));

    return gc;
};

static HostTextEditSetting *MythArchiveDvdauthorCmd()
{
    auto *gc = new HostTextEditSetting("MythArchiveDvdauthorCmd");

    gc->setLabel(ArchiveSettings::tr("dvdauthor command"));

    gc->setValue("dvdauthor");

    gc->setHelpText(ArchiveSettings::tr("Command to run dvdauthor."));

    return gc;
};

static HostTextEditSetting *MythArchiveMkisofsCmd()
{
    auto *gc = new HostTextEditSetting("MythArchiveMkisofsCmd");

    gc->setLabel(ArchiveSettings::tr("mkisofs command"));

    gc->setValue("mkisofs");

    gc->setHelpText(ArchiveSettings::tr("Command to run mkisofs. (Used to "
                                        "create ISO images)"));
    return gc;
};

static HostTextEditSetting *MythArchiveGrowisofsCmd()
{
    auto *gc = new HostTextEditSetting("MythArchiveGrowisofsCmd");

    gc->setLabel(ArchiveSettings::tr("growisofs command"));

    gc->setValue("growisofs");

    gc->setHelpText(ArchiveSettings::tr("Command to run growisofs. (Used to "
                                        "burn DVDs)"));
    return gc;
};

static HostTextEditSetting *MythArchiveM2VRequantiserCmd()
{
    auto *gc = new HostTextEditSetting("MythArchiveM2VRequantiserCmd");

    gc->setLabel(ArchiveSettings::tr("M2VRequantiser command"));

    gc->setValue("M2VRequantiser");

    gc->setHelpText(ArchiveSettings::tr("Command to run M2VRequantiser. "
                                        "Optional - leave blank if you don't "
                                        "have M2VRequantiser installed."));
    return gc;
};

static HostTextEditSetting *MythArchiveJpeg2yuvCmd()
{
    auto *gc = new HostTextEditSetting("MythArchiveJpeg2yuvCmd");

    gc->setLabel(ArchiveSettings::tr("jpeg2yuv command"));

    gc->setValue("jpeg2yuv");

    gc->setHelpText(ArchiveSettings::tr("Command to run jpeg2yuv. Part of "
                                        "mjpegtools package"));
    return gc;
};

static HostTextEditSetting *MythArchiveSpumuxCmd()
{
    auto *gc = new HostTextEditSetting("MythArchiveSpumuxCmd");

    gc->setLabel(ArchiveSettings::tr("spumux command"));

    gc->setValue("spumux");

    gc->setHelpText(ArchiveSettings::tr("Command to run spumux. Part of "
                                        "dvdauthor package"));
    return gc;
};

static HostTextEditSetting *MythArchiveMpeg2encCmd()
{
    auto *gc = new HostTextEditSetting("MythArchiveMpeg2encCmd");

    gc->setLabel(ArchiveSettings::tr("mpeg2enc command"));

    gc->setValue("mpeg2enc");

    gc->setHelpText(ArchiveSettings::tr("Command to run mpeg2enc. Part of "
                                        "mjpegtools package"));
    return gc;
};

static HostTextEditSetting *MythArchiveProjectXCmd()
{
    auto *gc = new HostTextEditSetting("MythArchiveProjectXCmd");

    gc->setLabel(ArchiveSettings::tr("projectx command"));

    gc->setValue("projectx");

    gc->setHelpText(ArchiveSettings::tr("Command to run ProjectX. Will be used "
                                        "to cut commercials and split mpegs "
                                        "files instead of mythtranscode and "
                                        "mythreplex."));
    return gc;
};

ArchiveSettings::ArchiveSettings()
{
    setLabel(ArchiveSettings::tr("MythArchive Settings"));
    addChild(MythArchiveTempDir());
    addChild(MythArchiveShareDir());
    addChild(PALNTSC());
    addChild(MythArchiveFileFilter());
    addChild(MythArchiveDVDLocation());
    addChild(MythArchiveDriveSpeed());
    addChild(MythArchiveDVDPlayerCmd());
    addChild(MythArchiveCopyRemoteFiles());
    addChild(MythArchiveAlwaysUseMythTranscode());
    addChild(MythArchiveUseProjectX());
    addChild(MythArchiveAddSubtitles());
    addChild(MythArchiveUseFIFO());
    addChild(MythArchiveDefaultEncProfile());

    auto* DVDSettings = new GroupSetting();
    DVDSettings->setLabel(ArchiveSettings::tr("DVD Menu Settings"));
    DVDSettings->addChild(MainMenuAspectRatio());
    DVDSettings->addChild(ChapterMenuAspectRatio());
    DVDSettings->addChild(MythArchiveDateFormat());
    DVDSettings->addChild(MythArchiveTimeFormat());
    addChild(DVDSettings);

    auto* externalCmdSettings = new GroupSetting();
    externalCmdSettings->setLabel(ArchiveSettings::tr("MythArchive External Commands"));
    externalCmdSettings->addChild(MythArchiveMplexCmd());
    externalCmdSettings->addChild(MythArchiveDvdauthorCmd());
    externalCmdSettings->addChild(MythArchiveSpumuxCmd());
    externalCmdSettings->addChild(MythArchiveMpeg2encCmd());
    externalCmdSettings->addChild(MythArchiveMkisofsCmd());
    externalCmdSettings->addChild(MythArchiveGrowisofsCmd());
    externalCmdSettings->addChild(MythArchiveM2VRequantiserCmd());
    externalCmdSettings->addChild(MythArchiveJpeg2yuvCmd());
    externalCmdSettings->addChild(MythArchiveProjectXCmd());
    addChild(externalCmdSettings);
}
