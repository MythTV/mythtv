#include "mythtv/mythcontext.h"

#include "globalsettings.h"
#include <qfile.h>
#include <qdialog.h>
#include <qcursor.h>
#include <qdir.h>
#include <qimage.h>

// General Settings

class SetMusicDirectory: public LineEditSetting, public GlobalSetting {
public:
    SetMusicDirectory():
        GlobalSetting("MusicLocation") {
        setLabel("Directory to hold music");
        setValue("/mnt/store/music/");
        setHelpText("This directory must exist, and the user "
                    "running MythMusic needs to have write permission "
                    "to the directory.");
    };
};

class AudioDevice: public ComboBoxSetting, public GlobalSetting {
public:
    AudioDevice() : ComboBoxSetting(true),
      GlobalSetting("AudioDevice") {
        setLabel("Audio device");
        QDir dev("/dev", "dsp*", QDir::Name, QDir::System);
        fillSelectionsFromDir(dev);
        dev.setNameFilter("adsp*");
        fillSelectionsFromDir(dev);

        dev.setNameFilter("dsp*");
        dev.setPath("/dev/sound");
        fillSelectionsFromDir(dev);
        dev.setNameFilter("adsp*");
        fillSelectionsFromDir(dev);
    }
};

class CDDevice: public ComboBoxSetting, public GlobalSetting {
public:
    CDDevice() : ComboBoxSetting(true),
      GlobalSetting("CDDevice") {
        setLabel("CD device");
        QDir dev("/dev", "cdrom*", QDir::Name, QDir::System);
        fillSelectionsFromDir(dev);
        dev.setNameFilter("scd*");
        fillSelectionsFromDir(dev);
        dev.setNameFilter("hd*");
        fillSelectionsFromDir(dev);

        dev.setNameFilter("cdrom*");
        dev.setPath("/dev/cdroms");
        fillSelectionsFromDir(dev);
    }
};

class TreeLevels: public LineEditSetting, public GlobalSetting {
public:
    TreeLevels():
        GlobalSetting("TreeLevels") {
        setLabel("Tree Sorting");
        setValue("artist album title");
        setHelpText("Order in which to sort the Music Selection Tree. "
                    "Possible values are space-separated list of "
                    "genre, artist, album, and title OR the "
                    "keyword \"directory\" to indicate that "
                    "the onscreen tree mirrors the actual directory "
                    "tree.");
    };
};

class NonID3FileNameFormat: public LineEditSetting, public GlobalSetting {
public:
    NonID3FileNameFormat():
        GlobalSetting("NonID3FileNameFormat") {
        setLabel("Filename Format");
        setValue("GENRE/ARTIST/ALBUM/TRACK_TITLE");
        setHelpText("Help text here.");
    };
};


class IgnoreID3Tags: public CheckBoxSetting, public GlobalSetting {
public:
    IgnoreID3Tags():
        GlobalSetting("Ignore_ID3") {
        setLabel("Ignore ID3 Tags");
        setValue(false);
        setHelpText("If set, MythMusic will skip checking ID3 tags in "
                    "files and just try to determine Genre, Artist, "
                    "Album, and Track number and title from the "
                    "filename.");
    };
};

class AutoLookupCD: public CheckBoxSetting, public GlobalSetting {
public:
    AutoLookupCD():
        GlobalSetting("AutoLookupCD") {
        setLabel("Automatically lookup CDs");
        setValue(true);
        setHelpText("Automatically lookup an audio CD if it is "
                    "present and show its information in the "
                    "Music Selection Tree.");
    };
};

class KeyboardAccelerators: public CheckBoxSetting, public GlobalSetting {
public:
    KeyboardAccelerators():
        GlobalSetting("KeyboardAccelerators") {
        setLabel("Use Keyboard/Remote Accelerated Buttons");
        setValue(true);
        setHelpText("If this is not set, you will need "
                    "to use arrow keys to select and activate "
                    "various functions.");
    };
};

class ParanoiaLevel: public ComboBoxSetting, public GlobalSetting {
public:
    ParanoiaLevel():
        GlobalSetting("ParanoiaLevel") {
        setLabel("Paranoia Level");
        addSelection("Full", "Full");
        addSelection("Faster", "");
        setHelpText("Paranoia level of the CD ripper. Set to "
                    "faster if you're not concerned about "
                    "possible errors in the audio.");
    };
};

class EjectCD: public CheckBoxSetting, public GlobalSetting {
public:
    EjectCD():
        GlobalSetting("EjectCDAfterRipping") {
        setLabel("Automatically eject CDs after ripping");
        setValue(true);
        setHelpText("If set, the CD tray will automatically open "
                    "after the CD has been ripped.");
    };
};

class SetRatingWeight: public SpinBoxSetting, public GlobalSetting {
public:
    SetRatingWeight():
        SpinBoxSetting(0, 100, 1),
        GlobalSetting("IntelliRatingWeight") {
        setLabel("Rating Weight");
        setValue(35);
        setHelpText("Used in \"Smart\" Shuffle mode. "
                    "This weighting affects how much strength is "
                    "given to your rating of a given track when "
                    "ordering a group of songs.");
    };
};

class SetPlayCountWeight: public SpinBoxSetting, public GlobalSetting {
public:
    SetPlayCountWeight():
        SpinBoxSetting(0, 100, 1),
        GlobalSetting("IntelliPlayCountWeight") {
        setLabel("Play Count Weight");
        setValue(25);
        setHelpText("Used in \"Smart\" Shuffle mode. "
                    "This weighting affects how much strength is "
                    "given to how many times a given track has been "
                    "played when ordering a group of songs.");
    };
};

class SetLastPlayWeight: public SpinBoxSetting, public GlobalSetting {
public:
    SetLastPlayWeight():
        SpinBoxSetting(0, 100, 1),
        GlobalSetting("IntelliLastPlayWeight") {
        setLabel("Last Play Weight");
        setValue(25);
        setHelpText("Used in \"Smart\" Shuffle mode. "
                    "This weighting affects how much strength is "
                    "given to how long it has been since a given "
                    "track was played when ordering a group of songs.");
    };
};

class SetRandomWeight: public SpinBoxSetting, public GlobalSetting {
public:
    SetRandomWeight():
        SpinBoxSetting(0, 100, 1),
        GlobalSetting("IntelliRandomWeight") {
        setLabel("Random Weight");
        setValue(15);
        setHelpText("Used in \"Smart\" Shuffle mode. "
                    "This weighting affects how much strength is "
                    "given to good old (peudo-)randomness "
                    "when ordering a group of songs.");
    };
};

class UseShowRatings: public CheckBoxSetting, public GlobalSetting {
public:
    UseShowRatings():
        GlobalSetting("MusicShowRatings") {
        setLabel("Show Song Ratings");
        setValue(false);
        setHelpText("Show song ratings on the playback screen.");
    };
};

class UseListShuffled: public CheckBoxSetting, public GlobalSetting {
public:
    UseListShuffled():
        GlobalSetting("ListAsShuffled") {
        setLabel("List as Shuffled");
        setValue(false);
        setHelpText("List songs on the playback screen "
                    "in the order they will be played.");
    };
};

class UseShowWholeTree: public CheckBoxSetting, public GlobalSetting {
public:
    UseShowWholeTree():
        GlobalSetting("ShowWholeTree") {
        setLabel("Show entire music tree");
        setValue(false);
        setHelpText("If selected, you can navigate your entire music "
                    "tree from the playing screen.");
    };
};

//Player Settings

class PlayMode: public ComboBoxSetting, public GlobalSetting {
public:
    PlayMode():
        GlobalSetting("PlayMode") {
        setLabel("Play mode");
        addSelection("Normal");
        addSelection("Random");
        addSelection("Intelligent");
        setHelpText("Starting shuffle mode for the player.  Can be either "
                    "normal, random, or intelligent (random).");
    };
};

class VisualModeDelay: public SliderSetting, public GlobalSetting {
public:
    VisualModeDelay():
	SliderSetting(0, 100, 1),
        GlobalSetting("VisualModeDelay") {
        setLabel("Delay before Visualizations start (seconds)");
        setValue(0);
        setHelpText("If set to 0, visualizations will never "
                    "automatically start.");
        };
};

class VisualCycleOnSongChange: public CheckBoxSetting, public GlobalSetting {
public:
    VisualCycleOnSongChange():
        GlobalSetting("VisualCycleOnSongChange") {
        setLabel("Change Visualizer on each song");
        setValue(false);
        setHelpText("Change the visualizer when the song "
                    "change.");
    };
};

class VisualScaleWidth: public SpinBoxSetting, public GlobalSetting {
public:
    VisualScaleWidth():
        SpinBoxSetting(1, 2, 1),
        GlobalSetting("VisualScaleWidth") {
        setLabel("Width for Visual Scaling");
        setValue(1);
        setHelpText("If set to \"2\", visualizations will be "
                    "scaled in half.  Currently only used by "
                    "the goom visualization.  Reduces CPU load "
                    "on slower machines.");
    };
};

class VisualScaleHeight: public SpinBoxSetting, public GlobalSetting {
public:
    VisualScaleHeight():
        SpinBoxSetting(1, 2, 1),
        GlobalSetting("VisualScaleHeight") {
        setLabel("Height for Visual Scaling");
        setValue(1);
        setHelpText("If set to \"2\", visualizations will be "
                    "scaled in half.  Currently only used by "
                    "the goom visualization.  Reduces CPU load "
                    "on slower machines.");
    };
};

class visualization_Synaestesia: public CheckBoxSetting, public GlobalSetting {
public:
    visualization_Synaestesia():
        GlobalSetting("visualization_Synaestesia") {
        setLabel("Synaestesia");
        setValue(false);
    };
};

class visualization_MonoScope: public CheckBoxSetting, public GlobalSetting {
public:
    visualization_MonoScope():
        GlobalSetting("visualization_MonoScope") {
        setLabel("MonoScope");
        setValue(false);
    };
};

class visualization_StereoScope: public CheckBoxSetting, public GlobalSetting {
public:
    visualization_StereoScope():
        GlobalSetting("visualization_StereoScope") {
        setLabel("StereoScope");
        setValue(false);
    };
};

class visualization_Blank: public CheckBoxSetting, public GlobalSetting {
public:
    visualization_Blank():
        GlobalSetting("visualization_Blank") {
        setLabel("Blank");
        setValue(false);
    };
};

class visualization_Random: public CheckBoxSetting, public GlobalSetting {
public:
    visualization_Random():
        GlobalSetting("visualization_Random") {
        setLabel("Random");
        setValue(false);
    };
};

class visualization_Spectrum: public CheckBoxSetting, public GlobalSetting {
public:
    visualization_Spectrum():
        GlobalSetting("visualization_Spectrum") {
        setLabel("Spectrum");
        setValue(false);
        setHelpText("Requires FFTW.");
    };
};

class visualization_BumpScope: public CheckBoxSetting, public GlobalSetting {
public:
    visualization_BumpScope():
        GlobalSetting("visualization_BumpScope") {
        setLabel("BumpScope");
        setValue(false);
        setHelpText("Requires SDL.");
    };
};

class visualization_Goom: public CheckBoxSetting, public GlobalSetting {
public:
    visualization_Goom():
        GlobalSetting("visualization_Goom") {
        setLabel("Goom");
        setValue(false);
        setHelpText("Requires SDL.");
    };
};

class visualization_Gears: public CheckBoxSetting, public GlobalSetting {
public:
    visualization_Gears():
        GlobalSetting("visualization_Gears") {
        setLabel("Gears");
        setValue(false);
        setHelpText("Requires OpenGL and FFTW.");
    };
};

class VisualizationMode: public LineEditSetting, public GlobalSetting {
public:
    VisualizationMode():
        GlobalSetting("VisualMode") {
        setLabel("Visualizations");
        setValue("Random");
        setHelpText("List of visualizations to use during playback. "
                    "Possible values are space-separated list of "
                    "Random, MonoScope, StereoScope, Spectrum, "
                    "BumpScope, Goom, Synaesthesia, Gears, and Blank");
    };
};

GeneralSettings::GeneralSettings()
{
    VerticalConfigurationGroup* general = new VerticalConfigurationGroup(false);
    general->setLabel("General Settings");
    general->addChild(new SetMusicDirectory());
    general->addChild(new AudioDevice());
    general->addChild(new TreeLevels());
    general->addChild(new NonID3FileNameFormat());
    general->addChild(new IgnoreID3Tags());
    general->addChild(new AutoLookupCD());
    general->addChild(new KeyboardAccelerators());
    addChild(general);

    VerticalConfigurationGroup *general2 = new VerticalConfigurationGroup(false);
    general2->setLabel("General Settings");
    general2->addChild(new CDDevice());
    general2->addChild(new ParanoiaLevel());
    general2->addChild(new EjectCD());
    addChild(general2);
}

PlayerSettings::PlayerSettings()
{
    VerticalConfigurationGroup* playersettings = new VerticalConfigurationGroup(false);
    playersettings->setLabel("Playback Settings");
    playersettings->addChild(new PlayMode());
    playersettings->addChild(new SetRatingWeight());
    playersettings->addChild(new SetPlayCountWeight());
    playersettings->addChild(new SetLastPlayWeight());
    playersettings->addChild(new SetRandomWeight());
    playersettings->addChild(new UseShowRatings());
    playersettings->addChild(new UseShowWholeTree());
    playersettings->addChild(new UseListShuffled());
    addChild(playersettings);

    VerticalConfigurationGroup* playersettings2 = new VerticalConfigurationGroup(false);
    playersettings2->setLabel("Visualization Settings");
    playersettings2->addChild(new VisualizationMode());
    playersettings2->addChild(new VisualCycleOnSongChange());
    playersettings2->addChild(new VisualModeDelay());
    playersettings2->addChild(new VisualScaleWidth());
    playersettings2->addChild(new VisualScaleHeight());
    addChild(playersettings2);
}
