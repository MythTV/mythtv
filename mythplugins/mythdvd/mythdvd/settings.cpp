/*
	settings.cpp 

	(c) 2003 Thor Sigvaldason and Isaac Richards
	
	Part of the mythTV project
	
	methods to set settings for the mythDVD module
*/


#include "mythtv/mythcontext.h"

#include "settings.h"
#include <qfile.h>
#include <qdialog.h>
#include <qcursor.h>
#include <qdir.h>
#include <qimage.h>

/*
--------------- General Settings ---------------
*/

static HostLineEdit *SetVCDDevice()
{
    HostLineEdit *gc = new HostLineEdit("VCDDeviceLocation");
    gc->setLabel(QObject::tr("Location of VCD device"));
    gc->setValue("/dev/cdrom");
    gc->setHelpText(QObject::tr("This device must exist, and the user "
                    "running MythDVD needs to have read permission "
                    "on the device."));
    return gc;
}

static HostLineEdit *SetDVDDevice()
{
    HostLineEdit *gc = new HostLineEdit("DVDDeviceLocation");
    gc->setLabel(QObject::tr("Location of DVD device"));
    gc->setValue("/dev/dvd");
    gc->setHelpText(QObject::tr("This device must exist, and the user "
                    "running MythDVD needs to have read permission "
                    "on the device."));
    return gc;
}

static HostComboBox *SetOnInsertDVD()
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

DVDGeneralSettings::DVDGeneralSettings()
{
    VerticalConfigurationGroup* general = new VerticalConfigurationGroup(false);
    general->setLabel(QObject::tr("General Settings"));
    general->addChild(SetDVDDevice());
    general->addChild(SetVCDDevice());
    general->addChild(SetOnInsertDVD());
    addChild(general);
}



/*
--------------- Player Settings ---------------
*/


static HostLineEdit *PlayerCommand()
{
    HostLineEdit *gc = new HostLineEdit("mythdvd.DVDPlayerCommand");
    gc->setLabel(QObject::tr("DVD Player Command"));
//    gc->setValue("mplayer dvd:// -dvd-device %d -fs -zoom -vo xv");
    gc->setValue("Internal");
    gc->setHelpText(
        QObject::tr("This can be any command to launch a DVD player "
                    "(e.g. MPlayer, ogle, etc.). If present, %d will "
                    "be substituted for the DVD device (e.g. /dev/dvd)."));
    return gc;
}

static HostLineEdit *VCDPlayerCommand()
{
    HostLineEdit *gc = new HostLineEdit("VCDPlayerCommand");
    gc->setLabel(QObject::tr("VCD Player Command"));
    gc->setValue("mplayer vcd:// -cdrom-device %d -fs -zoom -vo xv");
    gc->setHelpText(QObject::tr("This can be any command to launch a VCD player "
                    "(e.g. MPlayer, xine, etc.). If present, %d will "
                    "be substituted for the VCD device (e.g. /dev/cdrom)."));
    return gc;
}


DVDPlayerSettings::DVDPlayerSettings()
{
    VerticalConfigurationGroup* playersettings = new VerticalConfigurationGroup(false);
    playersettings->setLabel(QObject::tr("DVD Player Settings"));
    playersettings->addChild(PlayerCommand());
    VerticalConfigurationGroup* VCDplayersettings = new VerticalConfigurationGroup(false);
    VCDplayersettings->setLabel(QObject::tr("VCD Player Settings"));
    VCDplayersettings->addChild(VCDPlayerCommand());
    addChild(playersettings);
    addChild(VCDplayersettings);
}

/*
--------------- Ripper Settings ---------------
*/

static HostLineEdit *SetRipDirectory()
{
    HostLineEdit *gc = new HostLineEdit("DVDRipLocation");
    gc->setLabel(QObject::tr("Directory to hold temporary files"));
#ifdef Q_WS_MACX
    gc->setValue(QDir::homeDirPath() + "/Library/Application Support");
#else
    gc->setValue("/var/lib/mythdvd/temp");
#endif
    gc->setHelpText(QObject::tr("This directory must exist, and the user "
                    "running MythDVD needs to have write permission "
                    "to the directory."));
    return gc;
}

static HostLineEdit *TitlePlayCommand()
{
    HostLineEdit *gc = new HostLineEdit("TitlePlayCommand");
    gc->setLabel(QObject::tr("Title Playing Command"));
    gc->setValue("mplayer dvd://%t -dvd-device %d -fs -zoom -vo xv -aid %a -channels %c");
    gc->setHelpText(QObject::tr("This is a command used to preview a given "
                    "title on a DVD. If present %t will be set "
                    "to the title, %d for device, %a for audio "
                    "track, %c for audio channels."));
    return gc;
}

static HostLineEdit *SubTitleCommand()
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

static HostLineEdit *TranscodeCommand()
{
    HostLineEdit *gc = new HostLineEdit("TranscodeCommand");
    gc->setLabel(QObject::tr("Base transcode command"));
    gc->setValue("transcode");
    gc->setHelpText(QObject::tr("This is the base (without arguments) command "
                    "to run transcode on your system."));
    return gc;
}

static HostSpinBox *MTDPortNumber()
{
    HostSpinBox *gc = new HostSpinBox("MTDPort", 1024, 65535, 1);
    gc->setLabel(QObject::tr("MTD port number"));
    gc->setValue(2442);
    gc->setHelpText(QObject::tr("The port number that should be used for "
                    "communicating with the MTD (Myth Transcoding "
                    "Daemon)"));
    return gc;
}

static HostCheckBox *MTDLogFlag()
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


static HostCheckBox *MTDac3Flag()
{
    HostCheckBox *gc = new HostCheckBox("MTDac3Flag");
    gc->setLabel(QObject::tr("Transcode AC3 Audio"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If set, the MTD (Myth Transcoding Daemon) "
                    "will, by default, preserve AC3 (Dolby "
                    "Digital) audio in transcoded files. "));
    return gc;
}


static HostCheckBox *MTDxvidFlag()
{
    HostCheckBox *gc = new HostCheckBox("MTDxvidFlag");
    gc->setLabel(QObject::tr("Use xvid rather than divx"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If set, mythdvd will use the (open, free) "
                    "xvid codec rather than divx whenever "
                    "possible."));
    return gc;
}


static HostSpinBox *MTDNiceLevel()
{
    HostSpinBox *gc = new HostSpinBox("MTDNiceLevel", 0, 20, 1);
    gc->setLabel(QObject::tr("Nice level for MTD"));
    gc->setValue(20);
    gc->setHelpText(QObject::tr("This determines the priority of the Myth "
                    "Transcoding Daemon. Higher numbers mean "
                    "lower priority (more CPU to other tasks)."));
    return gc;
}

static HostSpinBox *MTDConcurrentTranscodes()
{
    HostSpinBox *gc = new HostSpinBox("MTDConcurrentTranscodes", 1, 99, 1);
    gc->setLabel(QObject::tr("Simultaneous Transcode Jobs"));
    gc->setValue(1);
    gc->setHelpText(QObject::tr("This determines the number of simultaneous "
                    "transcode jobs. If set at 1 (the default), "
                    "there will only be one active job at a time."));
    return gc;
}

static HostSpinBox *MTDRipSize()
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

DVDRipperSettings::DVDRipperSettings()
{
    VerticalConfigurationGroup* rippersettings = new VerticalConfigurationGroup(false);
    rippersettings->setLabel(QObject::tr("DVD Ripper Settings"));
    rippersettings->addChild(SetRipDirectory());
    rippersettings->addChild(TitlePlayCommand());
    rippersettings->addChild(SubTitleCommand());
    rippersettings->addChild(TranscodeCommand());
    addChild(rippersettings);

    VerticalConfigurationGroup* mtdsettings = new VerticalConfigurationGroup(false);
    mtdsettings->setLabel(QObject::tr("MTD Settings"));
    mtdsettings->addChild(MTDPortNumber());
    mtdsettings->addChild(MTDNiceLevel());
    mtdsettings->addChild(MTDConcurrentTranscodes());
    mtdsettings->addChild(MTDRipSize());
    mtdsettings->addChild(MTDLogFlag());
    mtdsettings->addChild(MTDac3Flag());
    mtdsettings->addChild(MTDxvidFlag());
    addChild(mtdsettings);
}

