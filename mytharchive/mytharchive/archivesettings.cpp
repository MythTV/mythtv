/*
	archivesettings.cpp
*/

#include <unistd.h>

// myth
#include <mythtv/mythcontext.h>

// mytharchive
#include "archivesettings.h"


static HostLineEdit *MythArchiveTempDir()
{
    HostLineEdit *gc = new HostLineEdit("MythArchiveTempDir");
    gc->setLabel(QObject::tr("Myth Archive Temp Directory"));
    gc->setValue("/tmp");
    gc->setHelpText(QObject::tr("Location where MythArchive should create its "
                                "temporory work files"));
    return gc;
};

static HostLineEdit *MythArchiveShareDir()
{
    HostLineEdit *gc = new HostLineEdit("MythArchiveShareDir");
    gc->setLabel(QObject::tr("Myth Archive Share Directory"));
    gc->setValue(gContext->GetShareDir() + "mytharchive/");
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
            " some audio problems."));
    return gc;
};

static HostCheckBox *MythArchiveUseFIFO()
{
    HostCheckBox *gc = new HostCheckBox("MythArchiveUseFIFO");
    gc->setLabel(QObject::tr("Use FIFOs"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("The script will use FIFOs to pass the output"
            " of mplex into dvdauthor rather than creating intermediate files."
            " Saves time and disk space during multiplex operations but not supported"
            " on Windows platform"));
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

static HostLineEdit *MythArchiveFfmpegCmd()
{
    HostLineEdit *gc = new HostLineEdit("MythArchiveFfmpegCmd");
    gc->setLabel(QObject::tr("ffmpeg Command"));
    gc->setValue("ffmpeg");
    gc->setHelpText(QObject::tr("Command to run ffmpeg."));
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

static HostLineEdit *MythArchiveTcrequantCmd()
{
    HostLineEdit *gc = new HostLineEdit("MythArchiveTcrequantCmd");
    gc->setLabel(QObject::tr("tcrequant command"));
    gc->setValue("tcrequant");
    gc->setHelpText(QObject::tr("Command to run tcrequant (Part of transcode package). Optional - leave blank if you don't have the transcode package installed.")); 
    return gc;
};

static HostLineEdit *MythArchivePng2yuvCmd()
{
    HostLineEdit *gc = new HostLineEdit("MythArchivePng2yuvCmd");
    gc->setLabel(QObject::tr("png2yuv command"));
    gc->setValue("png2yuv");
    gc->setHelpText(QObject::tr("Command to run png2yuv. Part of mjpegtools package")); 
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

ArchiveSettings::ArchiveSettings()
{
    VerticalConfigurationGroup* vcg1 = new VerticalConfigurationGroup(false);
    vcg1->setLabel(QObject::tr("MythArchive Settings"));
    vcg1->addChild(MythArchiveTempDir());
    vcg1->addChild(MythArchiveShareDir());
    vcg1->addChild(PALNTSC());
    vcg1->addChild(MythArchiveFileFilter());
    vcg1->addChild(MythArchiveDVDLocation());
    addChild(vcg1);

    VerticalConfigurationGroup* vcg2 = new VerticalConfigurationGroup(false);
    vcg2->setLabel(QObject::tr("MythArchive Settings (2)"));
    vcg2->addChild(MythArchiveEncodeToAc3());
    vcg2->addChild(MythArchiveCopyRemoteFiles());
    vcg2->addChild(MythArchiveAlwaysUseMythTranscode());
    vcg2->addChild(MythArchiveUseFIFO());
    vcg2->addChild(MainMenuAspectRatio());
    vcg2->addChild(ChapterMenuAspectRatio());
    addChild(vcg2);

    VerticalConfigurationGroup* vcg3 = new VerticalConfigurationGroup(false);
    vcg3->setLabel(QObject::tr("MythArchive External Commands (1)"));
    vcg3->addChild(MythArchiveFfmpegCmd());
    vcg3->addChild(MythArchiveMplexCmd());
    vcg3->addChild(MythArchiveDvdauthorCmd());
    vcg3->addChild(MythArchiveSpumuxCmd());
    vcg3->addChild(MythArchiveMpeg2encCmd());
    addChild(vcg3);

    VerticalConfigurationGroup* vcg4 = new VerticalConfigurationGroup(false);
    vcg4->setLabel(QObject::tr("MythArchive External Commands (2)"));
    vcg4->addChild(MythArchiveMkisofsCmd());
    vcg4->addChild(MythArchiveGrowisofsCmd());
    vcg4->addChild(MythArchiveTcrequantCmd());
    vcg4->addChild(MythArchivePng2yuvCmd());
    addChild(vcg4);
}
