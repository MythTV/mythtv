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
        setLabel(QObject::tr("Directory that holds videos"));
        setValue("/share/Movies/dvd");
        setHelpText(QObject::tr("This directory must exist, and the user "
                    "running MythVideo only needs to have read permission "
                    "to the directory."));
    };
};

class VideoDefaultParentalLevel: public ComboBoxSetting, public GlobalSetting {
public:
    VideoDefaultParentalLevel() :
      GlobalSetting("VideoDefaultParentalLevel") {
        setLabel(QObject::tr("Starting Parental Level"));
        addSelection(QObject::tr("4 - Highest"), "4");
        addSelection(QObject::tr("1 - Lowest"), "1");
        addSelection(QObject::tr("2"), "2"); 
        addSelection(QObject::tr("3"), "3");
        setHelpText(QObject::tr("This is the 'level' that MythVideo starts at. "
                    "Any videos with a level at or below this will be shown in "
                    "the list or while browsing by default. The Parental PIN "
                    "should be set to limit changing of the default level."));
    }
};

class VideoAdminPassword: public LineEditSetting, public GlobalSetting {
public:
    VideoAdminPassword():
        GlobalSetting("VideoAdminPassword") {
        setLabel(QObject::tr("Parental Control PIN"));
        setHelpText(QObject::tr("This PIN is used to control the current "
                    "Parental Level. If you want to use this feature, then "
                    "setting the value to all numbers will make your life much "
                    "easier. If you don't want to be bothered by PC dialogs, "
                    "please set it to be blank.")); 
    };
};

class VideoAggressivePC: public CheckBoxSetting, public GlobalSetting {
public:
    VideoAggressivePC():
        GlobalSetting("VideoAggressivePC") {
        setLabel(QObject::tr("Aggresive Parental Control"));
        setValue(false);
        setHelpText(QObject::tr("If set, you will not be able to return "
                    "to this screen and reset the Parental "
                    "PIN without first entering the current PIN. You have "
                    "been warned."));
    };
};

class VideoListUnknownFiletypes: public CheckBoxSetting, public GlobalSetting {
public:
    VideoListUnknownFiletypes():
        GlobalSetting("VideoListUnknownFiletypes") {
        setLabel(QObject::tr("Show Unknown File Types"));
        setValue(true);
        setHelpText(QObject::tr("If set, all files below the Myth Video "
                    "directory will be displayed unless their "
                    "extension is explicitly set to be ignored. "));
    };
};

class VideoTreeNoDB: public CheckBoxSetting, public GlobalSetting {
public:
    VideoTreeNoDB():
        GlobalSetting("VideoTreeNoDB") {
        setLabel(QObject::tr("Video List browses files"));
        setValue(false);
        setHelpText(QObject::tr("If set, this will cause the Video List "
                    "screen to show all relevant files below "
                    "the MythVideo starting directory whether "
                    "they have been scanned or not."));
    };
};

class VideoNewBrowsable: public CheckBoxSetting, public GlobalSetting {
public:
    VideoNewBrowsable():
        GlobalSetting("VideoNewBrowsable") {
        setLabel(QObject::tr("Newly scanned files are browsable by default"));
        setValue(true);
        setHelpText(QObject::tr("If set, newly scanned files in the Video Manager "
                                "will be marked as browsable and will appear in "
                                "the 'Browse' menu."));
    };
};

//Player Settings

class VideoDefaultPlayer: public LineEditSetting, public GlobalSetting {
public:
    VideoDefaultPlayer():
        GlobalSetting("VideoDefaultPlayer") {
        setLabel(QObject::tr("Default Player"));
        setValue("mplayer -fs -zoom -quiet -vo xv %s");
        setHelpText(QObject::tr("This is the command used for any file "
                    "that the extension is not specifically defined."));
    };
};

GeneralSettings::GeneralSettings()
{
    VerticalConfigurationGroup* general = new VerticalConfigurationGroup(false);
    general->setLabel(QObject::tr("General Settings"));
    general->addChild(new VideoStartupDirectory());
    general->addChild(new VideoDefaultParentalLevel());
    general->addChild(new VideoAdminPassword());
    general->addChild(new VideoAggressivePC());
    general->addChild(new VideoListUnknownFiletypes());
    general->addChild(new VideoTreeNoDB());
    general->addChild(new VideoNewBrowsable());
    addChild(general);

}

PlayerSettings::PlayerSettings()
{
    VerticalConfigurationGroup* playersettings = new VerticalConfigurationGroup(false);
    playersettings->setLabel(QObject::tr("Player Settings"));
    playersettings->addChild(new VideoDefaultPlayer());
    addChild(playersettings);
}
