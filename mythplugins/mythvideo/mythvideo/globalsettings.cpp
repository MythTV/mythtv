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
        setHelpText("This is the 'level' that MythVideo starts at. Any videos"
                    " with a level at or below this will be shown in the list"
                    " or while browsing. Passwords should be set to ensure this" 
                    " is useful. Any video/file set to level 0 will not be shown");
    }
};

class VideoAdminPassword: public LineEditSetting, public GlobalSetting {
public:
    VideoAdminPassword():
        GlobalSetting("VideoAdminPassword") {
        setLabel("Video Manager Password");
        setHelpText("This password is used to enter the 'Video Manager' "
                    "section. The 'Video Manager' is where the video's"
                    " parental level is set."); 
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
    addChild(general);

}

PlayerSettings::PlayerSettings()
{
    VerticalConfigurationGroup* playersettings = new VerticalConfigurationGroup(false);
    playersettings->setLabel("Player Settings");
    playersettings->addChild(new VideoDefaultPlayer());
    addChild(playersettings);
}
