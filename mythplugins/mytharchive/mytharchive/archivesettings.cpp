/*
	archivesettings.cpp
*/

#include <unistd.h>

// myth
#include <mythcontext.h>
#include <mythdirs.h>

// mytharchive
#include "archivesettings.h"


static HostLineEdit *MythArchiveTempDir()
{
    HostLineEdit *gc = new HostLineEdit("MythArchiveTempDir");
    gc->setLabel(QObject::tr("MythArchive Temp Directory"));
    gc->setValue("");
    gc->setHelpText(QObject::tr("Location where MythArchive should create its "
            "temporory work files. LOTS of free space required here."));
    return gc;
};

static HostLineEdit *MythArchiveShareDir()
{
    HostLineEdit *gc = new HostLineEdit("MythArchiveShareDir");
    gc->setLabel(QObject::tr("MythArchive Share Directory"));
    gc->setValue(GetShareDir() + "mytharchive/");
    gc->setHelpText(QObject::tr("Location where MythArchive stores its scripts, "
            "intro movies and theme files"));
    return gc;
};

static HostComboBox *PALNTSC()
{
    HostComboBox *gc = new HostComboBox("MythArchiveVideoFormat");
    gc->setLabel(QObject::tr("Video format"));
    gc->addSelection("PAL");
    gc->addSelection("NTSC");
    gc->setHelpText(QObject::tr("Video format for DVD recordings, PAL or NTSC."));
    return gc;
};

static HostLineEdit *MythArchiveFileFilter()
{
    HostLineEdit *gc = new HostLineEdit("MythArchiveFileFilter");
    gc->setLabel(QObject::tr("File Selector Filter"));
    gc->setValue("*.mpg *.mov *.avi *.mpeg *.nuv");
    gc->setHelpText(QObject::tr("The file name filter to use in the file selector."));
    return gc;
};

static HostLineEdit *MythArchiveDVDLocation()
{
    HostLineEdit *gc = new HostLineEdit("MythArchiveDVDLocation");
    gc->setLabel(QObject::tr("Location of DVD"));
    gc->setValue("/dev/dvd");
    gc->setHelpText(QObject::tr("Which DVD drive to use when burning discs."));
    return gc;
};

static HostSpinBox *MythArchiveDriveSpeed()
{
    HostSpinBox *gc = new HostSpinBox("MythArchiveDriveSpeed", 0, 48, 1);
    gc->setLabel(QObject::tr("DVD Drive Write Speed"));
    gc->setValue(0);
    gc->setHelpText(QObject::tr("This is the write speed to use when burning a DVD. "
                "Set to 0 to allow growisofs to choose the fastest available speed."));
    return gc;
};

static HostLineEdit *MythArchiveDVDPlayerCmd()
{
    HostLineEdit *gc = new HostLineEdit("MythArchiveDVDPlayerCmd");
    gc->setLabel(QObject::tr("Command to play DVD"));
    gc->setValue("Internal");
    gc->setHelpText(QObject::tr("Command to run when test playing a created DVD. "
             "'Internal' will use the internal MythTV player. %f will be replaced with "
            "the path to the created DVD structure eg. 'xine -pfhq --no-splash dvd:/%f'."));
    return gc;
};

static HostCheckBox *MythArchiveEncodeToAc3()
{
    HostCheckBox *gc = new HostCheckBox("MythArchiveEncodeToAc3");
    gc->setLabel(QObject::tr("Always Encode to AC3"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If set audio tracks will always be "
            "re-encoded to AC3 for better compatibility with DVD players in "
            "NTSC countries."));
    return gc;
};

static HostCheckBox *MythArchiveCopyRemoteFiles()
{
    HostCheckBox *gc = new HostCheckBox("MythArchiveCopyRemoteFiles");
    gc->setLabel(QObject::tr("Copy remote files"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If set files on remote filesystems "
            "will be copied over to the local filesystem before processing. "
            "Speeds processing and reduces bandwidth on the network"));
    return gc;
};

static HostCheckBox *MythArchiveAlwaysUseMythTranscode()
{
    HostCheckBox *gc = new HostCheckBox("MythArchiveAlwaysUseMythTranscode");
    gc->setLabel(QObject::tr("Always Use Mythtranscode"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If set mpeg2 files will always be passed"
            " though mythtranscode to clean up any errors. May help to fix"
            " some audio problems. Ignored if 'Use ProjectX' is set."));
    return gc;
};

static HostCheckBox *MythArchiveUseProjectX()
{
    HostCheckBox *gc = new HostCheckBox("MythArchiveUseProjectX");
    gc->setLabel(QObject::tr("Use ProjectX"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If set ProjectX will be used to cut"
            " commercials and split mpeg2 files instead of mythtranscode"
            " and mythreplex."));
    return gc;
};

static HostCheckBox *MythArchiveUseFIFO()
{
    HostCheckBox *gc = new HostCheckBox("MythArchiveUseFIFO");
    gc->setLabel(QObject::tr("Use FIFOs"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("The script will use FIFOs to pass the output"
            " of mplex into dvdauthor rather than creating intermediate files."
            " Saves time and disk space during multiplex operations but not "
            " supported on Windows platform"));
    return gc;
};

static HostCheckBox *MythArchiveAddSubtitles()
{
    HostCheckBox *gc = new HostCheckBox("MythArchiveAddSubtitles");
    gc->setLabel(QObject::tr("Add Subtitles"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If available this option will add subtitles "
            "to the final DVD. Requires 'Use ProjectX' to be on."));
    return gc;
};

static HostComboBox *MainMenuAspectRatio()
{
    HostComboBox *gc = new HostComboBox("MythArchiveMainMenuAR");
    gc->setLabel(QObject::tr("Main Menu Aspect Ratio"));
    gc->addSelection("4:3");
    gc->addSelection("16:9");
    gc->setValue(1);
    gc->setHelpText(QObject::tr("Aspect ratio to use when creating the main menu."));
    return gc;
};

static HostComboBox *ChapterMenuAspectRatio()
{
    HostComboBox *gc = new HostComboBox("MythArchiveChapterMenuAR");
    gc->setLabel(QObject::tr("Chapter Menu Aspect Ratio"));
    gc->addSelection("4:3");
    gc->addSelection("16:9");
    gc->addSelection("Video");
    gc->setValue(2);
    gc->setHelpText(QObject::tr("Aspect ratio to use when creating the chapter menu. "
            "Video means use the same aspect ratio as the associated video."));
    return gc;
};

static HostComboBox *MythArchiveDateFormat()
{
    HostComboBox *gc = new HostComboBox("MythArchiveDateFormat");
    gc->setLabel(QObject::tr("Date format"));

    QDate sampdate = QDate::currentDate();
    QString sampleStr =
            QObject::tr("Samples are shown using today's date.");

    if (sampdate.month() == sampdate.day())
    {
        sampdate = sampdate.addDays(1);
        sampleStr =
                QObject::tr("Samples are shown using tomorrow's date.");
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
    gc->setHelpText(QObject::tr("Your preferred date format to use on DVD menus.") + " " +
            sampleStr);
    return gc;
}

static HostComboBox *MythArchiveTimeFormat()
{
    HostComboBox *gc = new HostComboBox("MythArchiveTimeFormat");
    gc->setLabel(QObject::tr("Time format"));

    QTime samptime = QTime::currentTime();

    gc->addSelection(samptime.toString("hh:mm AP"), "%I:%M %p");
    gc->addSelection(samptime.toString("hh:mm"), "%H:%M");
    gc->setHelpText(QObject::tr("Your preferred time format to display on DVD menus. "
            "You must choose a format with \"AM\" or \"PM\" in it, otherwise your "
            "time display will be 24-hour or \"military\" time."));
    return gc;
}

static HostComboBox *MythArchiveDefaultEncProfile()
{
    HostComboBox *gc = new HostComboBox("MythArchiveDefaultEncProfile");
    gc->setLabel(QObject::tr("Default Encoder Profile"));

    gc->addSelection("HQ", "HQ");
    gc->addSelection("SP", "SP");
    gc->addSelection("LP", "LP");
    gc->addSelection("EP", "EP");
    gc->setValue(1);
    gc->setHelpText(QObject::tr("Default encoding profile to use if a file "
                                "needs re-encoding."));
    return gc;
}

static HostLineEdit *MythArchiveFfmpegCmd()
{
    HostLineEdit *gc = new HostLineEdit("MythArchiveFfmpegCmd");
    gc->setLabel(QObject::tr("FFmpeg Command"));
    gc->setValue("ffmpeg");
    gc->setHelpText(QObject::tr("Command to run FFmpeg."));
    return gc;
};

static HostLineEdit *MythArchiveMplexCmd()
{
    HostLineEdit *gc = new HostLineEdit("MythArchiveMplexCmd");
    gc->setLabel(QObject::tr("mplex Command"));
    gc->setValue("mplex");
    gc->setHelpText(QObject::tr("Command to run mplex"));
    return gc;
};

static HostLineEdit *MythArchiveDvdauthorCmd()
{
    HostLineEdit *gc = new HostLineEdit("MythArchiveDvdauthorCmd");
    gc->setLabel(QObject::tr("dvdauthor command"));
    gc->setValue("dvdauthor");
    gc->setHelpText(QObject::tr("Command to run dvdauthor."));
    return gc;
};

static HostLineEdit *MythArchiveMkisofsCmd()
{
    HostLineEdit *gc = new HostLineEdit("MythArchiveMkisofsCmd");
    gc->setLabel(QObject::tr("mkisofs command"));
    gc->setValue("mkisofs");
    gc->setHelpText(QObject::tr("Command to run mkisofs. (Used to create ISO images)"));
    return gc;
};

static HostLineEdit *MythArchiveGrowisofsCmd()
{
    HostLineEdit *gc = new HostLineEdit("MythArchiveGrowisofsCmd");
    gc->setLabel(QObject::tr("growisofs command"));
    gc->setValue("growisofs");
    gc->setHelpText(QObject::tr("Command to run growisofs. (Used to burn DVD's)"));
    return gc;
};

static HostLineEdit *MythArchiveM2VRequantiserCmd()
{
    HostLineEdit *gc = new HostLineEdit("MythArchiveM2VRequantiserCmd");
    gc->setLabel(QObject::tr("M2VRequantiser command"));
    gc->setValue("M2VRequantiser");
    gc->setHelpText(QObject::tr("Command to run M2VRequantiser. Optional - leave blank if you don't have M2VRequantiser installed."));
    return gc;
};

static HostLineEdit *MythArchiveJpeg2yuvCmd()
{
    HostLineEdit *gc = new HostLineEdit("MythArchiveJpeg2yuvCmd");
    gc->setLabel(QObject::tr("jpeg2yuv command"));
    gc->setValue("jpeg2yuv");
    gc->setHelpText(QObject::tr("Command to run jpeg2yuv. Part of mjpegtools package"));
    return gc;
};

static HostLineEdit *MythArchiveSpumuxCmd()
{
    HostLineEdit *gc = new HostLineEdit("MythArchiveSpumuxCmd");
    gc->setLabel(QObject::tr("spumux command"));
    gc->setValue("spumux");
    gc->setHelpText(QObject::tr("Command to run spumux. Part of dvdauthor package"));
    return gc;
};

static HostLineEdit *MythArchiveMpeg2encCmd()
{
    HostLineEdit *gc = new HostLineEdit("MythArchiveMpeg2encCmd");
    gc->setLabel(QObject::tr("mpeg2enc command"));
    gc->setValue("mpeg2enc");
    gc->setHelpText(QObject::tr("Command to run mpeg2enc. Part of mjpegtools package"));
    return gc;
};

static HostLineEdit *MythArchiveProjectXCmd()
{
    HostLineEdit *gc = new HostLineEdit("MythArchiveProjectXCmd");
    gc->setLabel(QObject::tr("projectx command"));
    gc->setValue("projectx");
    gc->setHelpText(QObject::tr("Command to run ProjectX. Will be used to cut "
            "commercials and split mpegs files instead of mythtranscode and mythreplex."));
    return gc;
};

ArchiveSettings::ArchiveSettings()
{
    VerticalConfigurationGroup* vcg1 = new VerticalConfigurationGroup(false);
    vcg1->setLabel(QObject::tr("MythArchive Settings"));
    vcg1->addChild(MythArchiveTempDir());
    vcg1->addChild(MythArchiveShareDir());
    vcg1->addChild(PALNTSC());
    vcg1->addChild(MythArchiveFileFilter());
    vcg1->addChild(MythArchiveDVDLocation());
    vcg1->addChild(MythArchiveDriveSpeed());
    vcg1->addChild(MythArchiveDVDPlayerCmd());
    addChild(vcg1);

    VerticalConfigurationGroup* vcg2 = new VerticalConfigurationGroup(false);
    vcg2->setLabel(QObject::tr("MythArchive Settings (2)"));
    vcg2->addChild(MythArchiveEncodeToAc3());
    vcg2->addChild(MythArchiveCopyRemoteFiles());
    vcg2->addChild(MythArchiveAlwaysUseMythTranscode());
    vcg2->addChild(MythArchiveUseProjectX());
    vcg2->addChild(MythArchiveAddSubtitles());
    vcg2->addChild(MythArchiveUseFIFO());
    vcg2->addChild(MythArchiveDefaultEncProfile());
    addChild(vcg2);

    VerticalConfigurationGroup* vcg3 = new VerticalConfigurationGroup(false);
    vcg3->setLabel(QObject::tr("DVD Menu Settings"));
    vcg3->addChild(MainMenuAspectRatio());
    vcg3->addChild(ChapterMenuAspectRatio());
    vcg3->addChild(MythArchiveDateFormat());
    vcg3->addChild(MythArchiveTimeFormat());
    addChild(vcg3);

    VerticalConfigurationGroup* vcg4 = new VerticalConfigurationGroup(false);
    vcg4->setLabel(QObject::tr("MythArchive External Commands (1)"));
    vcg4->addChild(MythArchiveFfmpegCmd());
    vcg4->addChild(MythArchiveMplexCmd());
    vcg4->addChild(MythArchiveDvdauthorCmd());
    vcg4->addChild(MythArchiveSpumuxCmd());
    vcg4->addChild(MythArchiveMpeg2encCmd());
    addChild(vcg4);

    VerticalConfigurationGroup* vcg5 = new VerticalConfigurationGroup(false);
    vcg5->setLabel(QObject::tr("MythArchive External Commands (2)"));
    vcg5->addChild(MythArchiveMkisofsCmd());
    vcg5->addChild(MythArchiveGrowisofsCmd());
    vcg5->addChild(MythArchiveM2VRequantiserCmd());
    vcg5->addChild(MythArchiveJpeg2yuvCmd());
    vcg5->addChild(MythArchiveProjectXCmd());
    addChild(vcg5);
}
