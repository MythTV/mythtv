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

#include "config.h"

/*
--------------- General Settings ---------------
*/

#ifdef VCD_SUPPORT
class SetVCDDevice: public GlobalLineEdit {
public:
    SetVCDDevice():
        GlobalLineEdit("VCDDeviceLocation") {
        setLabel(QObject::tr("Location of VCD device"));
        setValue("/dev/cdrom");
        setHelpText(QObject::tr("This device must exist, and the user "
                    "running MythDVD needs to have read permission "
                    "on the device."));
    };
};
#endif

class SetDVDDevice: public GlobalLineEdit {
public:
    SetDVDDevice():
        GlobalLineEdit("DVDDeviceLocation") {
        setLabel(QObject::tr("Location of DVD device"));
        setValue("/dev/dvd");
        setHelpText(QObject::tr("This device must exist, and the user "
                    "running MythDVD needs to have read permission "
                    "on the device."));
    };
};

class SetOnInsertDVD: public GlobalComboBox {
public :
    SetOnInsertDVD():
       GlobalComboBox("DVDOnInsertDVD") {
       setLabel(QObject::tr("On DVD insertion"));
       addSelection(QObject::tr("Display mythdvd menu"),"1");
       addSelection(QObject::tr("Do nothing"),"0");
       addSelection(QObject::tr("Play DVD"),"2");
#ifdef TRANSCODE_SUPPORT       
       addSelection(QObject::tr("Rip DVD"),"3");
#endif
       setHelpText(QObject::tr("Media Monitoring should be turned on to "
                   "allow this feature (Setup -> General -> CD/DVD Monitor"));
       };
};

DVDGeneralSettings::DVDGeneralSettings()
{
    VerticalConfigurationGroup* general = new VerticalConfigurationGroup(false);
    general->setLabel(QObject::tr("General Settings"));
    general->addChild(new SetDVDDevice());
#ifdef VCD_SUPPORT
    general->addChild(new SetVCDDevice());
#endif
    general->addChild(new SetOnInsertDVD());
    addChild(general);
}



/*
--------------- Player Settings ---------------
*/


class PlayerCommand: public GlobalLineEdit {
public:
    PlayerCommand():
        GlobalLineEdit("DVDPlayerCommand") {
        setLabel(QObject::tr("DVD Player Command"));
        setValue("mplayer dvd:// -dvd-device %d -fs -zoom -vo xv");
        setHelpText(QObject::tr("This can be any command to launch a DVD player "
                    "(e.g. MPlayer, ogle, etc.). If present, %d will "
                    "be substituted for the DVD device (e.g. /dev/dvd)."));
    };
};

#ifdef VCD_SUPPORT
class VCDPlayerCommand: public GlobalLineEdit {
public:
    VCDPlayerCommand():
        GlobalLineEdit("VCDPlayerCommand") {
        setLabel(QObject::tr("VCD Player Command"));
        setValue("mplayer vcd:// -cdrom-device %d -fs -zoom -vo xv");
        setHelpText(QObject::tr("This can be any command to launch a VCD player "
                    "(e.g. MPlayer, xine, etc.). If present, %d will "
                    "be substituted for the VCD device (e.g. /dev/cdrom)."));
    };
};
#endif


DVDPlayerSettings::DVDPlayerSettings()
{
    VerticalConfigurationGroup* playersettings = new VerticalConfigurationGroup(false);
    playersettings->setLabel(QObject::tr("DVD Player Settings"));
    playersettings->addChild(new PlayerCommand());
#ifdef VCD_SUPPORT
    VerticalConfigurationGroup* VCDplayersettings = new VerticalConfigurationGroup(false);
    VCDplayersettings->setLabel(QObject::tr("VCD Player Settings"));
    VCDplayersettings->addChild(new VCDPlayerCommand());
#endif
    addChild(playersettings);
#ifdef VCD_SUPPORT
    addChild(VCDplayersettings);
#endif
}

/*
--------------- Ripper Settings ---------------
*/

class SetRipDirectory: public GlobalLineEdit {
public:
    SetRipDirectory():
        GlobalLineEdit("DVDRipLocation") {
        setLabel(QObject::tr("Directory to hold temporary files"));
        setValue("/var/lib/mythdvd/temp");
        setHelpText(QObject::tr("This directory must exist, and the user "
                    "running MythDVD needs to have write permission "
                    "to the directory."));
    };
};

class TitlePlayCommand: public GlobalLineEdit {
public:
    TitlePlayCommand():
        GlobalLineEdit("TitlePlayCommand"){
        setLabel(QObject::tr("Title Playing Command"));
        setValue("mplayer dvd://%t -dvd-device %d -fs -zoom -vo xv -aid %a -channels %c");
        setHelpText(QObject::tr("This is a command used to preview a given "
                    "title on a DVD. If present %t will be set "
                    "to the title, %d for device, %a for audio "
                    "track, %c for audio channels."));
    };
};

class SubTitleCommand: public GlobalLineEdit {
public:
    SubTitleCommand():
        GlobalLineEdit("SubTitleCommand"){
        setLabel(QObject::tr("Subtitle arguments:"));
        setValue("-sid %s");
        setHelpText(QObject::tr("If you choose any subtitles for ripping, this "
                    "command is added to the end of the Title Play "
                    "Command to allow previewing of subtitles. If  "
                    "present %s will be set to the subtitle track. "));
    };
};

class TranscodeCommand: public GlobalLineEdit {
public:
    TranscodeCommand():
        GlobalLineEdit("TranscodeCommand"){
        setLabel(QObject::tr("Base transcode command"));
        setValue("transcode");
        setHelpText(QObject::tr("This is the base (without arguments) command "
                    "to run transcode on your system."));
    };
};

class MTDPortNumber: public GlobalSpinBox {
public:
    MTDPortNumber():
        GlobalSpinBox("MTDPort", 1024, 65535, 1) {
        setLabel(QObject::tr("MTD port number"));
        setValue(2442);
        setHelpText(QObject::tr("The port number that should be used for "
                    "communicating with the MTD (Myth Transcoding "
                    "Daemon)"));
    };
};

class MTDLogFlag: public GlobalCheckBox {
public:
    MTDLogFlag():
        GlobalCheckBox("MTDLogFlag") {
        setLabel(QObject::tr("MTD logs to terminal window"));
        setValue(false);
        setHelpText(QObject::tr("If set, the MTD (Myth Transcoding Daemon) "
                    "will log to the window it is started from. "
                    "Otherwise, it will write to a file called  "
                    "mtd.log in the top level ripping directory."));
     };
};


class MTDac3Flag: public GlobalCheckBox {
public:
    MTDac3Flag():
        GlobalCheckBox("MTDac3Flag") {
        setLabel(QObject::tr("Transcode AC3 Audio"));
        setValue(false);
        setHelpText(QObject::tr("If set, the MTD (Myth Transcoding Daemon) "
                    "will, by default, preserve AC3 (Dolby "
                    "Digital) audio in transcoded files. "));
     };
};


class MTDxvidFlag: public GlobalCheckBox {
public:
    MTDxvidFlag():
        GlobalCheckBox("MTDxvidFlag") {
        setLabel(QObject::tr("Use xvid rather than divx"));
        setValue(true);
        setHelpText(QObject::tr("If set, mythdvd will use the (open, free) "
                    "xvid codec rather than divx whenever "
                    "possible."));
     };
};


class MTDNiceLevel: public GlobalSpinBox {
public:
    MTDNiceLevel():
        GlobalSpinBox("MTDNiceLevel", 0, 20, 1) {
        setLabel(QObject::tr("Nice level for MTD"));
        setValue(20);
        setHelpText(QObject::tr("This determines the priority of the Myth "
                    "Transcoding Daemon. Higher numbers mean "
                    "lower priority (more CPU to other tasks)."));
    };
};

class MTDConcurrentTranscodes: public GlobalSpinBox {
public:
    MTDConcurrentTranscodes():
        GlobalSpinBox("MTDConcurrentTranscodes", 1, 99, 1) {
        setLabel(QObject::tr("Simultaneous Transcode Jobs"));
        setValue(1);
        setHelpText(QObject::tr("This determines the number of simultaneous "
                    "transcode jobs. If set at 1 (the default), "
                    "there will only be one active job at a time."));
    };
};

class MTDRipSize: public GlobalSpinBox {
public:
    MTDRipSize():
        GlobalSpinBox("MTDRipSize", 0, 4096, 1) {
        setLabel(QObject::tr("Ripped video segments"));
        setValue(0);
        setHelpText(QObject::tr("If set to something other than 0, ripped "
                    "video titles will be broken up into files "
                    "of this size (in MB). Applies to both perfect "
                    "quality recordings and intermediate files "
                    "used for transcoding."));
    };
};

DVDRipperSettings::DVDRipperSettings()
{
    VerticalConfigurationGroup* rippersettings = new VerticalConfigurationGroup(false);
    rippersettings->setLabel(QObject::tr("DVD Ripper Settings"));
    rippersettings->addChild(new SetRipDirectory());
    rippersettings->addChild(new TitlePlayCommand());
    rippersettings->addChild(new SubTitleCommand());
    rippersettings->addChild(new TranscodeCommand());
    addChild(rippersettings);

    VerticalConfigurationGroup* mtdsettings = new VerticalConfigurationGroup(false);
    mtdsettings->setLabel(QObject::tr("MTD Settings"));
    mtdsettings->addChild(new MTDPortNumber());
    mtdsettings->addChild(new MTDNiceLevel());
    mtdsettings->addChild(new MTDConcurrentTranscodes());
    mtdsettings->addChild(new MTDRipSize());
    mtdsettings->addChild(new MTDLogFlag());
    mtdsettings->addChild(new MTDac3Flag());
    mtdsettings->addChild(new MTDxvidFlag());
    addChild(mtdsettings);
}

