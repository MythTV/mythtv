#include "mythtv/mythcontext.h"

#include "globalsettings.h"
#include <qfile.h>
#include <qdialog.h>
#include <qcursor.h>
#include <qdir.h>
#include <qimage.h>

// General Settings

class VideoStartupDirectory: public LineEditSetting, public GlobalSetting {
public:
    VideoStartupDirectory():
        GlobalSetting("VideoStartupDir") {
        setLabel("Directory that holds videos");
        setValue("/share/Movies/dvd");
        setHelpText("This directory must exist, and the user "
                    "running MythVideo only needs to have read permission "
                    "to the directory.");
    };
};

class VideoDefaultParentalLevel: public ComboBoxSetting, public GlobalSetting {
public:
    VideoDefaultParentalLevel() : ComboBoxSetting(true),
      GlobalSetting("VideoDefaultParentalLevel") {
        setLabel("Starting Parental Level");
        addSelection("4 - Highest", "4");
        addSelection("1 - Lowest", "1");
        addSelection("2", "2"); 
        addSelection("3", "3");
        setHelpText("This is the 'level' that MythVideo starts at. Any videos "
                    "with a level at or below this will be shown in the list "
                    "or while browsing by default. The Parental PIN should be "
                    "set to limit changing of the default level.");
    }
};

class VideoAdminPassword: public LineEditSetting, public GlobalSetting {
public:
    VideoAdminPassword():
        GlobalSetting("VideoAdminPassword") {
        setLabel("Parental Control PIN");
        setHelpText("This PIN is used to control the current Parental Level. "
                    "If you want to use this feature, then setting the value "
                    "to all numbers will make your life much easier. If you "
                    "don't want to be bothered by PC dialogs, please "
                    "set it to be blank."); 
    };
};

class VideoAggressivePC: public CheckBoxSetting, public GlobalSetting {
public:
    VideoAggressivePC():
        GlobalSetting("VideoAggressivePC") {
        setLabel("Aggresive Parental Control");
        setValue(false);
        setHelpText("If set, you will not be able to return "
                    "to this screen and reset the Parental "
                    "PIN without first entering the current PIN. You have "
                    "been warned.");
    };
};

class VideoListUnknownFiletypes: public CheckBoxSetting, public GlobalSetting {
public:
    VideoListUnknownFiletypes():
        GlobalSetting("VideoListUnknownFiletypes") {
        setLabel("Show Unknown File Types");
        setValue(true);
        setHelpText("If set, all files below the Myth Video "
                    "directory will be displayed unless their "
                    "extension is explicitly set to be ignored. ");
    };
};

class VideoTreeNoDB: public CheckBoxSetting, public GlobalSetting {
public:
    VideoTreeNoDB():
        GlobalSetting("VideoTreeNoDB") {
        setLabel("Video List browses files");
        setValue(false);
        setHelpText("If set, this will cause the Video List "
                    "screen to show all relevant files below "
                    "the MythVideo starting directory whether "
                    "they have been scanned or not.");
    };
};

//Player Settings

class VideoDefaultPlayer: public LineEditSetting, public GlobalSetting {
public:
    VideoDefaultPlayer():
        GlobalSetting("VideoDefaultPlayer") {
        setLabel("Default Player");
        setValue("/usr/bin/mplayer -fs -zoom -quiet -vo xv %s");
        setHelpText("This is the command used for any file that the "
                    "extension is not specifically defined.");
    };
};

GeneralSettings::GeneralSettings()
{
    VerticalConfigurationGroup* general = new VerticalConfigurationGroup(false);
    general->setLabel("General Settings");
    general->addChild(new VideoStartupDirectory());
    general->addChild(new VideoDefaultParentalLevel());
    general->addChild(new VideoAdminPassword());
    general->addChild(new VideoAggressivePC());
    general->addChild(new VideoListUnknownFiletypes());
    general->addChild(new VideoTreeNoDB());
    addChild(general);

}

PlayerSettings::PlayerSettings()
{
    VerticalConfigurationGroup* playersettings = new VerticalConfigurationGroup(false);
    playersettings->setLabel("Player Settings");
    playersettings->addChild(new VideoDefaultPlayer());
    addChild(playersettings);
}
