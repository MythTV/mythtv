/*
    archivesettings.cpp
*/

#include <unistd.h>

// myth
#include <mythcontext.h>
#include <mythdirs.h>
#include <mythdate.h>

// mytharchive
#include "archivesettings.h"


static HostLineEdit *MythArchiveTempDir()
{
    HostLineEdit *gc = new HostLineEdit("MythArchiveTempDir");

    gc->setLabel(ArchiveSettings::tr("MythArchive Temp Directory"));
    gc->setValue("");

    gc->setHelpText(ArchiveSettings::tr("Location where MythArchive should "
                                        "create its temporary work files. "
                                        "LOTS of free space required here."));
    return gc;
};

static HostLineEdit *MythArchiveShareDir()
{
    HostLineEdit *gc = new HostLineEdit("MythArchiveShareDir");

    gc->setLabel(ArchiveSettings::tr("MythArchive Share Directory"));
    gc->setValue(GetShareDir() + "mytharchive/");

    gc->setHelpText(ArchiveSettings::tr("Location where MythArchive stores its "
                                        "scripts, intro movies and theme "
                                        "files"));
    return gc;
};

static HostComboBox *PALNTSC()
{
    HostComboBox *gc = new HostComboBox("MythArchiveVideoFormat");

    gc->setLabel(ArchiveSettings::tr("Video format"));

    gc->addSelection("PAL");
    gc->addSelection("NTSC");

    gc->setHelpText(ArchiveSettings::tr("Video format for DVD recordings, PAL "
                                        "or NTSC."));
    return gc;
};

static HostLineEdit *MythArchiveFileFilter()
{
    HostLineEdit *gc = new HostLineEdit("MythArchiveFileFilter");

    gc->setLabel(ArchiveSettings::tr("File Selector Filter"));
    gc->setValue("*.mpg *.mov *.avi *.mpeg *.nuv");

    gc->setHelpText(ArchiveSettings::tr("The file name filter to use in the "
                                        "file selector."));
    return gc;
};

static HostLineEdit *MythArchiveDVDLocation()
{
    HostLineEdit *gc = new HostLineEdit("MythArchiveDVDLocation");

    gc->setLabel(ArchiveSettings::tr("Location of DVD"));
    gc->setValue("/dev/dvd");

    gc->setHelpText(ArchiveSettings::tr("Which DVD drive to use when burning "
                                        "discs."));
    return gc;
};

static HostSpinBox *MythArchiveDriveSpeed()
{
    HostSpinBox *gc = new HostSpinBox("MythArchiveDriveSpeed", 0, 48, 1);

    gc->setLabel(ArchiveSettings::tr("DVD Drive Write Speed"));
    gc->setValue(0);

    gc->setHelpText(ArchiveSettings::tr("This is the write speed to use when "
                                        "burning a DVD. Set to 0 to allow "
                                        "growisofs to choose the fastest "
                                        "available speed."));
    return gc;
};

static HostLineEdit *MythArchiveDVDPlayerCmd()
{
    HostLineEdit *gc = new HostLineEdit("MythArchiveDVDPlayerCmd");

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

static HostCheckBox *MythArchiveCopyRemoteFiles()
{
    HostCheckBox *gc = new HostCheckBox("MythArchiveCopyRemoteFiles");

    gc->setLabel(ArchiveSettings::tr("Copy remote files"));
    gc->setValue(false);

    gc->setHelpText(ArchiveSettings::tr("If set files on remote filesystems "
                                        "will be copied over to the local "
                                        "filesystem before processing. Speeds "
                                        "processing and reduces bandwidth on "
                                        "the network"));
    return gc;
};

static HostCheckBox *MythArchiveAlwaysUseMythTranscode()
{
    HostCheckBox *gc = new HostCheckBox("MythArchiveAlwaysUseMythTranscode");

    gc->setLabel(ArchiveSettings::tr("Always Use Mythtranscode"));
    gc->setValue(true);

    gc->setHelpText(ArchiveSettings::tr("If set mpeg2 files will always be "
                                        "passed though mythtranscode to clean "
                                        "up any errors. May help to fix some "
                                        "audio problems. Ignored if 'Use "
                                        "ProjectX' is set."));
    return gc;
};

static HostCheckBox *MythArchiveUseProjectX()
{
    HostCheckBox *gc = new HostCheckBox("MythArchiveUseProjectX");

    gc->setLabel(ArchiveSettings::tr("Use ProjectX"));
    gc->setValue(false);

    gc->setHelpText(ArchiveSettings::tr("If set ProjectX will be used to cut "
                                        "commercials and split mpeg2 files "
                                        "instead of mythtranscode and "
                                        "mythreplex."));
    return gc;
};

static HostCheckBox *MythArchiveUseFIFO()
{
    HostCheckBox *gc = new HostCheckBox("MythArchiveUseFIFO");

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

static HostCheckBox *MythArchiveAddSubtitles()
{
    HostCheckBox *gc = new HostCheckBox("MythArchiveAddSubtitles");

    gc->setLabel(ArchiveSettings::tr("Add Subtitles"));
    gc->setValue(false);

    gc->setHelpText(ArchiveSettings::tr("If available this option will add "
                                        "subtitles to the final DVD. Requires "
                                        "'Use ProjectX' to be on."));
    return gc;
};

static HostComboBox *MainMenuAspectRatio()
{
    HostComboBox *gc = new HostComboBox("MythArchiveMainMenuAR");

    gc->setLabel(ArchiveSettings::tr("Main Menu Aspect Ratio"));

    gc->addSelection(ArchiveSettings::tr("4:3", "Aspect ratio"), "4:3");
    gc->addSelection(ArchiveSettings::tr("16:9", "Aspect ratio"), "16:9");

    gc->setValue(1);

    gc->setHelpText(ArchiveSettings::tr("Aspect ratio to use when creating the "
                                        "main menu."));
    return gc;
};

static HostComboBox *ChapterMenuAspectRatio()
{
    HostComboBox *gc = new HostComboBox("MythArchiveChapterMenuAR");

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

static HostComboBox *MythArchiveDateFormat()
{
    HostComboBox *gc = new HostComboBox("MythArchiveDateFormat");
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

static HostComboBox *MythArchiveTimeFormat()
{
    HostComboBox *gc = new HostComboBox("MythArchiveTimeFormat");
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

static HostComboBox *MythArchiveDefaultEncProfile()
{
    HostComboBox *gc = new HostComboBox("MythArchiveDefaultEncProfile");
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

static HostLineEdit *MythArchiveMplexCmd()
{
    HostLineEdit *gc = new HostLineEdit("MythArchiveMplexCmd");

    gc->setLabel(ArchiveSettings::tr("mplex Command"));

    gc->setValue("mplex");

    gc->setHelpText(ArchiveSettings::tr("Command to run mplex"));

    return gc;
};

static HostLineEdit *MythArchiveDvdauthorCmd()
{
    HostLineEdit *gc = new HostLineEdit("MythArchiveDvdauthorCmd");

    gc->setLabel(ArchiveSettings::tr("dvdauthor command"));

    gc->setValue("dvdauthor");

    gc->setHelpText(ArchiveSettings::tr("Command to run dvdauthor."));

    return gc;
};

static HostLineEdit *MythArchiveMkisofsCmd()
{
    HostLineEdit *gc = new HostLineEdit("MythArchiveMkisofsCmd");

    gc->setLabel(ArchiveSettings::tr("mkisofs command"));

    gc->setValue("mkisofs");

    gc->setHelpText(ArchiveSettings::tr("Command to run mkisofs. (Used to "
                                        "create ISO images)"));
    return gc;
};

static HostLineEdit *MythArchiveGrowisofsCmd()
{
    HostLineEdit *gc = new HostLineEdit("MythArchiveGrowisofsCmd");

    gc->setLabel(ArchiveSettings::tr("growisofs command"));

    gc->setValue("growisofs");

    gc->setHelpText(ArchiveSettings::tr("Command to run growisofs. (Used to "
                                        "burn DVDs)"));
    return gc;
};

static HostLineEdit *MythArchiveM2VRequantiserCmd()
{
    HostLineEdit *gc = new HostLineEdit("MythArchiveM2VRequantiserCmd");

    gc->setLabel(ArchiveSettings::tr("M2VRequantiser command"));

    gc->setValue("M2VRequantiser");

    gc->setHelpText(ArchiveSettings::tr("Command to run M2VRequantiser. "
                                        "Optional - leave blank if you don't "
                                        "have M2VRequantiser installed."));
    return gc;
};

static HostLineEdit *MythArchiveJpeg2yuvCmd()
{
    HostLineEdit *gc = new HostLineEdit("MythArchiveJpeg2yuvCmd");

    gc->setLabel(ArchiveSettings::tr("jpeg2yuv command"));

    gc->setValue("jpeg2yuv");

    gc->setHelpText(ArchiveSettings::tr("Command to run jpeg2yuv. Part of "
                                        "mjpegtools package"));
    return gc;
};

static HostLineEdit *MythArchiveSpumuxCmd()
{
    HostLineEdit *gc = new HostLineEdit("MythArchiveSpumuxCmd");

    gc->setLabel(ArchiveSettings::tr("spumux command"));

    gc->setValue("spumux");

    gc->setHelpText(ArchiveSettings::tr("Command to run spumux. Part of "
                                        "dvdauthor package"));
    return gc;
};

static HostLineEdit *MythArchiveMpeg2encCmd()
{
    HostLineEdit *gc = new HostLineEdit("MythArchiveMpeg2encCmd");

    gc->setLabel(ArchiveSettings::tr("mpeg2enc command"));

    gc->setValue("mpeg2enc");

    gc->setHelpText(ArchiveSettings::tr("Command to run mpeg2enc. Part of "
                                        "mjpegtools package"));
    return gc;
};

static HostLineEdit *MythArchiveProjectXCmd()
{
    HostLineEdit *gc = new HostLineEdit("MythArchiveProjectXCmd");

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
    VerticalConfigurationGroup* vcg1 = new VerticalConfigurationGroup(false);
    vcg1->setLabel(ArchiveSettings::tr("MythArchive Settings"));
    vcg1->addChild(MythArchiveTempDir());
    vcg1->addChild(MythArchiveShareDir());
    vcg1->addChild(PALNTSC());
    vcg1->addChild(MythArchiveFileFilter());
    vcg1->addChild(MythArchiveDVDLocation());
    vcg1->addChild(MythArchiveDriveSpeed());
    vcg1->addChild(MythArchiveDVDPlayerCmd());
    addChild(vcg1);

    VerticalConfigurationGroup* vcg2 = new VerticalConfigurationGroup(false);
    vcg2->setLabel(ArchiveSettings::tr("MythArchive Settings (2)"));
    vcg2->addChild(MythArchiveCopyRemoteFiles());
    vcg2->addChild(MythArchiveAlwaysUseMythTranscode());
    vcg2->addChild(MythArchiveUseProjectX());
    vcg2->addChild(MythArchiveAddSubtitles());
    vcg2->addChild(MythArchiveUseFIFO());
    vcg2->addChild(MythArchiveDefaultEncProfile());
    addChild(vcg2);

    VerticalConfigurationGroup* vcg3 = new VerticalConfigurationGroup(false);
    vcg3->setLabel(ArchiveSettings::tr("DVD Menu Settings"));
    vcg3->addChild(MainMenuAspectRatio());
    vcg3->addChild(ChapterMenuAspectRatio());
    vcg3->addChild(MythArchiveDateFormat());
    vcg3->addChild(MythArchiveTimeFormat());
    addChild(vcg3);

    VerticalConfigurationGroup* vcg4 = new VerticalConfigurationGroup(false);
    vcg4->setLabel(ArchiveSettings::tr("MythArchive External Commands (1)"));
    vcg4->addChild(MythArchiveMplexCmd());
    vcg4->addChild(MythArchiveDvdauthorCmd());
    vcg4->addChild(MythArchiveSpumuxCmd());
    vcg4->addChild(MythArchiveMpeg2encCmd());
    addChild(vcg4);

    VerticalConfigurationGroup* vcg5 = new VerticalConfigurationGroup(false);
    vcg5->setLabel(ArchiveSettings::tr("MythArchive External Commands (2)"));
    vcg5->addChild(MythArchiveMkisofsCmd());
    vcg5->addChild(MythArchiveGrowisofsCmd());
    vcg5->addChild(MythArchiveM2VRequantiserCmd());
    vcg5->addChild(MythArchiveJpeg2yuvCmd());
    vcg5->addChild(MythArchiveProjectXCmd());
    addChild(vcg5);
}
