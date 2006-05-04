/*
	archivesettings.cpp
*/

#include "mythtv/mythcontext.h"

#include <unistd.h>

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
    vcg2->setLabel(QObject::tr("MythArchive External Commands (1)"));
    vcg2->addChild(MythArchiveFfmpegCmd());
    vcg2->addChild(MythArchiveMplexCmd());
    vcg2->addChild(MythArchiveDvdauthorCmd());
    vcg2->addChild(MythArchiveSpumuxCmd());
    vcg2->addChild(MythArchiveMpeg2encCmd());
    addChild(vcg2);

    VerticalConfigurationGroup* vcg3 = new VerticalConfigurationGroup(false);
    vcg3->setLabel(QObject::tr("MythArchive External Commands (2)"));
    vcg3->addChild(MythArchiveMkisofsCmd());
    vcg3->addChild(MythArchiveGrowisofsCmd());
    vcg3->addChild(MythArchiveTcrequantCmd());
    vcg3->addChild(MythArchivePng2yuvCmd());
    addChild(vcg3);
}
