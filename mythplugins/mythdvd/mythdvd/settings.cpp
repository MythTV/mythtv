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
    addChild(general);
}



/*
--------------- Player Settings ---------------
*/


class PlayerCommand: public LineEditSetting, public GlobalSetting {
public:
    PlayerCommand():
        GlobalSetting("DVDPlayerCommand") {
        setLabel("DVD Player Command");
        setValue("mplayer dvd:// -dvd-device %d -fs -zoom -vo xv");
        setHelpText("This can be any command to launch a DVD player "
                    "(e.g. MPlayer, ogle, etc.). If present %d will "
                    "be substituted for the DVD device (e.g. /dev/dvd).");
    };
};


PlayerSettings::PlayerSettings()
{
    VerticalConfigurationGroup* playersettings = new VerticalConfigurationGroup(false);
    playersettings->setLabel("DVD Player Settings");
    playersettings->addChild(new PlayerCommand());
    addChild(playersettings);

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

class MTDRipSize: public SpinBoxSetting, public GlobalSetting {
public:
    MTDRipSize():
        SpinBoxSetting(0, 4096, 1),
        GlobalSetting("MTDRipSize") {
        setLabel("Ripped video segments");
        setValue(0);
        setHelpText("If set to something other than 0, ripped "
                    "video titles will be broken up into files "
                    "of this size (in MB). Only applies to Perfect "
                    "quality recordings, not intermediate files.");
    };
};

RipperSettings::RipperSettings()
{
    VerticalConfigurationGroup* rippersettings = new VerticalConfigurationGroup(false);
    rippersettings->setLabel("DVD Ripper Settings");
    rippersettings->addChild(new SetRipDirectory());
    rippersettings->addChild(new TitlePlayCommand());
    rippersettings->addChild(new TranscodeCommand());
    rippersettings->addChild(new MTDPortNumber());
    rippersettings->addChild(new MTDNiceLevel());
    rippersettings->addChild(new MTDRipSize());
    rippersettings->addChild(new MTDLogFlag());
    rippersettings->addChild(new MTDac3Flag());
    rippersettings->addChild(new MTDxvidFlag());
    addChild(rippersettings);
}

