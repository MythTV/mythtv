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
class SetVCDDevice: public LineEditSetting, public GlobalSetting {
public:
    SetVCDDevice():
        GlobalSetting("VCDDeviceLocation") {
        setLabel("Location of VCD device");
        setValue("/dev/cdrom");
        setHelpText("This device must exist, and the user "
                    "running MythDVD needs to have read permission "
                    "on the device.");
    };
};
#endif

class SetDVDDevice: public LineEditSetting, public GlobalSetting {
public:
    SetDVDDevice():
        GlobalSetting("DVDDeviceLocation") {
        setLabel("Location of DVD device");
        setValue("/dev/dvd");
        setHelpText("This device must exist, and the user "
                    "running MythDVD needs to have read permission "
                    "on the device.");
    };
};

GeneralSettings::GeneralSettings()
{
    VerticalConfigurationGroup* general = new VerticalConfigurationGroup(false);
    general->setLabel("General Settings");
    general->addChild(new SetDVDDevice());
#ifdef VCD_SUPPORT
    general->addChild(new SetVCDDevice());
#endif
    addChild(general);
}



/*
--------------- Player Settings ---------------
*/


class PlayerCommand: public ComboBoxSetting, public GlobalSetting {
public:
    PlayerCommand():
        ComboBoxSetting(true), GlobalSetting("DVDPlayerCommand") {
        setLabel("DVD Player Command");
        addSelection("mplayer dvd:// -dvd-device %d -fs -zoom -vo xv");
        addSelection("xine -pfhq --auto-scan dvd");
        addSelection("ogle");
        setHelpText("This can be any command to launch a DVD player "
                    "(e.g. MPlayer, ogle, etc.). If present, %d will "
                    "be substituted for the DVD device (e.g. /dev/dvd).");
    };
};

#ifdef VCD_SUPPORT
class VCDPlayerCommand: public ComboBoxSetting, public GlobalSetting {
public:
    VCDPlayerCommand():
        ComboBoxSetting(true), GlobalSetting("VCDPlayerCommand") {
        setLabel("VCD Player Command");
        addSelection("xine -f -g vcdx://");
        addSelection("mplayer -vcd 1 -cdrom-device %d -fs -zoom -vo xv");
        setHelpText("This can be any command to launch a VCD player "
                    "(e.g. MPlayer, xine, etc.). If present, %d will "
                    "be substituted for the VCD device (e.g. /dev/cdrom).");
    };
};
#endif


PlayerSettings::PlayerSettings()
{
    VerticalConfigurationGroup* playersettings = new VerticalConfigurationGroup(false);
    playersettings->setLabel("DVD Player Settings");
    playersettings->addChild(new PlayerCommand());
#ifdef VCD_SUPPORT
    VerticalConfigurationGroup* VCDplayersettings = new VerticalConfigurationGroup(false);
    VCDplayersettings->setLabel("VCD Player Settings");
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

class SetRipDirectory: public LineEditSetting, public GlobalSetting {
public:
    SetRipDirectory():
        GlobalSetting("DVDRipLocation") {
        setLabel("Directory to hold temporary files");
        setValue("/mnt/store/dvdrip/");
        setHelpText("This directory must exist, and the user "
                    "running MythDVD needs to have write permission "
                    "to the directory.");
    };
};

class TitlePlayCommand: public LineEditSetting, public GlobalSetting {
public:
    TitlePlayCommand():
        GlobalSetting("TitlePlayCommand"){
        setLabel("Title Playing Command");
        setValue("mplayer -dvd-device %d -fs -zoom -vo xv -dvd %t -aid %a -channels %c");
        setHelpText("This is a command used to preview a given "
                    "title on a DVD. If present %t will be set "
                    "to the title, %d for device, %a for audio "
                    "track, %c for audio channels.");
    };
};

class SubTitleCommand: public LineEditSetting, public GlobalSetting {
public:
    SubTitleCommand():
        GlobalSetting("SubTitleCommand"){
        setLabel("Subtitle arguments:");
        setValue("-sid %s");
        setHelpText("If you choose any subtitles for ripping, this "
                    "command is added to the end of the Title Play "
                    "Command to allow previewing of subtitles. If  "
                    "present %s will be set to the subtitle track. ");
    };
};

class TranscodeCommand: public LineEditSetting, public GlobalSetting {
public:
    TranscodeCommand():
        GlobalSetting("TranscodeCommand"){
        setLabel("Base transcode command");
        setValue("/usr/local/bin/transcode");
        setHelpText("This is the base (without arguments) command "
                    "to run transcode on your system.");
    };
};

class MTDPortNumber: public SpinBoxSetting, public GlobalSetting {
public:
    MTDPortNumber():
        SpinBoxSetting(1024, 65535, 1),
        GlobalSetting("MTDPort") {
        setLabel("mtd port number");
        setValue(2342);
        setHelpText("The port number that should be used for "
                    "communicating with the mtd (Myth Transcoding "
                    "Daemon)");
    };
};

class MTDLogFlag: public CheckBoxSetting, public GlobalSetting {
public:
    MTDLogFlag():
        GlobalSetting("MTDLogFlag") {
        setLabel("mtd logs to terminal window");
        setValue(false);
        setHelpText("If set, the mtd (Myth Transcoding Daemon) "
                    "will log to the window it is started from. "
                    "Otherwise, it will write to a file called  "
                    "mtd.log in the top level ripping directory.");
     };
};


class MTDac3Flag: public CheckBoxSetting, public GlobalSetting {
public:
    MTDac3Flag():
        GlobalSetting("MTDac3Flag") {
        setLabel("Transcode AC3 Audio");
        setValue(false);
        setHelpText("If set, the mtd (Myth Transcoding Daemon) "
                    "will, by default, preserve AC3 (Dolby "
                    "Digital) audio in transcoded files. ");
     };
};


class MTDxvidFlag: public CheckBoxSetting, public GlobalSetting {
public:
    MTDxvidFlag():
        GlobalSetting("MTDxvidFlag") {
        setLabel("Use xvid rather than divx");
        setValue(true);
        setHelpText("If set, mythdvd will use the (open, free) "
                    "xvid codec rather than divx whenever "
                    "possible.");
     };
};


class MTDNiceLevel: public SpinBoxSetting, public GlobalSetting {
public:
    MTDNiceLevel():
        SpinBoxSetting(0, 20, 1),
        GlobalSetting("MTDNiceLevel") {
        setLabel("nice level for mtd");
        setValue(20);
        setHelpText("This determines the priority of the Myth "
                    "Transcoding Daemon. Higher numbers mean "
                    "lower priority (more CPU to other tasks). ");
    };
};

class MTDConcurrentTranscodes: public SpinBoxSetting, public GlobalSetting {
public:
    MTDConcurrentTranscodes():
        SpinBoxSetting(1, 99, 1),
        GlobalSetting("MTDConcurrentTranscodes") {
        setLabel("Simultaneous Transcode Jobs");
        setValue(1);
        setHelpText("This determines the number of simultaneous "
                    "transcode jobs. If set at 1 (the default), "
                    "there will only be one active job at a time.");
    };
};

class MTDRipSize: public SpinBoxSetting, public GlobalSetting {
public:
    MTDRipSize():
        SpinBoxSetting(0, 4096, 1),
        GlobalSetting("MTDRipSize") {
        setLabel("Ripped video segments");
        setValue(0);
        setHelpText("If set to something other than 0, ripped "
                    "video titles will be broken up into files "
                    "of this size (in MB). Applies to both perfect "
                    "quality recordings and intermediate files "
                    "used for transcoding.");
    };
};

RipperSettings::RipperSettings()
{
    VerticalConfigurationGroup* rippersettings = new VerticalConfigurationGroup(false);
    rippersettings->setLabel("DVD Ripper Settings");
    rippersettings->addChild(new SetRipDirectory());
    rippersettings->addChild(new TitlePlayCommand());
    rippersettings->addChild(new SubTitleCommand());
    rippersettings->addChild(new TranscodeCommand());
    addChild(rippersettings);

    VerticalConfigurationGroup* mtdsettings = new VerticalConfigurationGroup(false);
    mtdsettings->setLabel("MTD Settings");
    mtdsettings->addChild(new MTDPortNumber());
    mtdsettings->addChild(new MTDNiceLevel());
    mtdsettings->addChild(new MTDConcurrentTranscodes());
    mtdsettings->addChild(new MTDRipSize());
    mtdsettings->addChild(new MTDLogFlag());
    mtdsettings->addChild(new MTDac3Flag());
    mtdsettings->addChild(new MTDxvidFlag());
    addChild(mtdsettings);
}

