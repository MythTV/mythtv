#include "mythcontext.h"

#include "globalsettings.h"
#include <qfile.h>
#include <qdialog.h>
#include <qcursor.h>
#include <qdir.h>
#include <qimage.h>

class AudioOutputDevice: public ComboBoxSetting, public GlobalSetting {
public:
    AudioOutputDevice();
};

AudioOutputDevice::AudioOutputDevice():
    ComboBoxSetting(true),
    GlobalSetting("AudioOutputDevice") {

    setLabel("Audio output device");
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

class MythControlsVolume: public CheckBoxSetting, public GlobalSetting {
public:
    MythControlsVolume():
        GlobalSetting("MythControlsVolume") {
        setLabel("Use internal volume controls");
        setValue(true);
        setHelpText("MythTV can control the PCM and master "
                    "mixer volume.  If you prefer to use an external mixer "
                    "program, uncheck this box.");
    };
};

class MixerDevice: public ComboBoxSetting, public GlobalSetting {
public:
    MixerDevice();
protected:
    static const char* paths[];
};

const char* MixerDevice::paths[] = { "/dev/mixer",
                                     "/dev/mixer1",
                                     "/dev/mixer2",
                                     "/dev/mixer3",
                                     "/dev/mixer4",
                                     "/dev/sound/mixer",
                                     "/dev/sound/mixer1",
                                     "/dev/sound/mixer2",
                                     "/dev/sound/mixer3",
                                     "/dev/sound/mixer4" };

MixerDevice::MixerDevice():
    ComboBoxSetting(true),
    GlobalSetting("MixerDevice") {

    setLabel("Mixer Device");
    for(unsigned int i = 0; i < sizeof(paths) / sizeof(char*); ++i) {
        if (QFile(paths[i]).exists())
            addSelection(paths[i]);
    }
}

class MixerControl: public ComboBoxSetting, public GlobalSetting {
public:
    MixerControl();
protected:
    static const char* controls[];
};

const char* MixerControl::controls[] = { "PCM",
                                         "Master" };

MixerControl::MixerControl():
    ComboBoxSetting(true),
    GlobalSetting("MixerControl") {

    setLabel("Mixer Controls");
    for(unsigned int i = 0; i < sizeof(controls) / sizeof(char*); ++i) {
        addSelection(controls[i]);
    }
    setHelpText("Changing the volume adjusts the selected mixer.");
}

class MixerVolume: public SliderSetting, public GlobalSetting {
public:
    MixerVolume():
	SliderSetting(0, 100, 1),
        GlobalSetting("MasterMixerVolume") {
        setLabel("Master Mixer Volume");
        setValue(70);
        setHelpText("Initial volume for the Master Mixer.  This affects "
                    "all sound created by the soundcard.  Note: Do not "
                    "set this too low." );
        };
};

class PCMVolume: public SliderSetting, public GlobalSetting {
public:
    PCMVolume():
        SliderSetting(0, 100, 1),
        GlobalSetting("PCMMixerVolume") {
        setLabel("PCM Mixer Volume");
        setValue(70);
        setHelpText("Initial volume for PCM output.  Use of the volume "
                    "keys in MythTV will adjust this parameter.");
        };
};

class Deinterlace: public CheckBoxSetting, public GlobalSetting {
public:
    Deinterlace():
        GlobalSetting("Deinterlace") {
        setLabel("Deinterlace playback");
        setValue(true);
        setHelpText("Make the video look normal on a progressive display "
                    "(i.e. monitor).  Deinterlace requires that your CPU "
                    "supports SSE instructions.  Enabling this without "
                    "proper CPU support will cause the program to segfault. "
                    "See the HOWTO document for more information.");
    };
};

class JumpAmount: public SpinBoxSetting, public GlobalSetting {
public:
    JumpAmount():
        SpinBoxSetting(1, 30, 1),
        GlobalSetting("JumpAmount") {
        setLabel("Jump amount (in minutes)");
        setValue(10);
        setHelpText("How many minutes to jump forward or backward "
                   "when the jump keys are pressed.");
    };
};

class FastForwardAmount: public SpinBoxSetting, public GlobalSetting {
public:
    FastForwardAmount():
        SpinBoxSetting(1, 600, 1),
        GlobalSetting("FastForwardAmount") {
        setLabel("Fast forward amount (in seconds)");
        setValue(30);
        setHelpText("How many seconds to skip forward on a fast forward.");
    };
};

class RewindAmount: public SpinBoxSetting, public GlobalSetting {
public:
    RewindAmount():
        SpinBoxSetting(1, 600, 1),
        GlobalSetting("RewindAmount") {
        setLabel("Rewind amount (in seconds)");
        setValue(5);
        setHelpText("How many seconds to skip backward on a rewind.");
    };
};

class ExactSeeking: public CheckBoxSetting, public GlobalSetting {
public:
    ExactSeeking():
        GlobalSetting("ExactSeeking") {
        setLabel("Seek to exact frame");
        setValue(false);
        setHelpText("If enabled, seeking is frame exact, but slower.");
    };
};

class CommercialSkipMethod: public ComboBoxSetting, public GlobalSetting {
public:
    CommercialSkipMethod():
        GlobalSetting("CommercialSkipMethod") {

        setLabel("Commercial Skip Method");
        addSelection("Blank Screen Detection (default)", "1");
        setHelpText("This determines the method used by MythTV to detect when "
                    "commercials start and end.  The manual "
                    "and automatic commercial skip modes of MythTV will "
                    "both use this function." );
    };
};

class CommercialSkipEverywhere: public CheckBoxSetting, public GlobalSetting {
public:
    CommercialSkipEverywhere():
        GlobalSetting("CommercialSkipEverywhere") {
        setLabel("Commercial Skip Everywhere");
        setValue(true);
        setHelpText("Allow the commercial skip buttons to jump to the "
                    "beginning of a commercial break as well as the end.");
    };
};

class AutoCommercialSkip: public CheckBoxSetting, public GlobalSetting {
public:
    AutoCommercialSkip():
        GlobalSetting("AutoCommercialSkip") {
        setLabel("Automatically Skip Commercials");
        setValue(false);
        setHelpText("Automatically skip commercial breaks using the selected "
                    "Commercial Skip Method. If a commercial break cutlist "
                    "exists, it will be used instead.");
    };
};

class AutoCommercialFlag: public CheckBoxSetting, public GlobalSetting {
public:
    AutoCommercialFlag():
        GlobalSetting("AutoCommercialFlag") {
        setLabel("Automatically Flag Commercials");
        setValue(false);
        setHelpText("Automatically flag commercials after a recording "
                    "completes.");
    };
};

class RecordOverTime: public SpinBoxSetting, public GlobalSetting {
public:
    RecordOverTime():
        SpinBoxSetting(0, 600, 1),
        GlobalSetting("RecordOverTime") {
        setLabel("Time to record past end of show (in seconds)");
        setValue(0);
    };
};

class PlayBoxOrdering: public CheckBoxSetting, public GlobalSetting {
public:
    PlayBoxOrdering():
        GlobalSetting("PlayBoxOrdering") {
        setLabel("List Newest Recording First");
        setValue(true);
        setHelpText("If checked (default) the most recent recording will be listed "
		    "first in the 'Watch Recordings' screen. If unchecked the oldest recording "
		    "will be listed first.");
    };
};

class StickyKeys: public CheckBoxSetting, public GlobalSetting {
public:
    StickyKeys():
        GlobalSetting("StickyKeys") {
        setLabel("Sticky keys");
        setValue(false);
        setHelpText("If this is set, fast forward and rewind continue after the key is released, until "
                    "the key is pressed again.  If enabled, set FastForwardAmount "
                    "and RewindAmount to one second.");
    };
};

class OSDDisplayTime: public SpinBoxSetting, public GlobalSetting {
public:
    OSDDisplayTime():
        SpinBoxSetting(0, 30, 1),
        GlobalSetting("OSDDisplayTime") {
        setLabel("Number of seconds for OSD information");
        setValue(3);
        setHelpText("How long the program information remains on the On "
                    "Screen Display after a channel change.");
    };
};

class OSDTheme: public ComboBoxSetting, public GlobalSetting {
public:
    OSDTheme():
        GlobalSetting("OSDTheme") {
        setLabel("OSD theme");

        QDir themes(PREFIX"/share/mythtv/themes");
        themes.setFilter(QDir::Dirs);
        themes.setSorting(QDir::Name | QDir::IgnoreCase);

        const QFileInfoList *fil = themes.entryInfoList(QDir::Dirs);
        if (!fil)
            return;

        QFileInfoListIterator it( *fil );
        QFileInfo *theme;

        for( ; it.current() != 0 ; ++it ) {
            theme = it.current();
            QFileInfo xml(theme->absFilePath() + "/osd.xml");

            if (theme->fileName()[0] != '.' && xml.exists())
                addSelection(theme->fileName());
        }
    }
};

class OSDFont: public ComboBoxSetting, public GlobalSetting {
public:
    OSDFont():
        GlobalSetting("OSDFont") {

        setLabel("OSD font");
        addSelection("FreeSans.ttf");
        addSelection("FreeMono.ttf");
    };
};

class OSDCCFont: public ComboBoxSetting, public GlobalSetting {
public:
    OSDCCFont():
        GlobalSetting("OSDCCFont") {

        setLabel("Closed Caption font");
        addSelection("FreeMono.ttf");
        addSelection("FreeSans.ttf");
        addSelection("ClosedCaption.ttf");
    };
};

class ChannelOrdering: public ComboBoxSetting, public GlobalSetting {
public:
    ChannelOrdering():
        GlobalSetting("ChannelOrdering") {
        setLabel("Channel ordering");
        addSelection("channel number (numeric)", "channum + 0");
        addSelection("channel number (alpha)", "channum");
        addSelection("database order", "chanid");
        addSelection("channel name", "callsign");
    };
};

class VertScanPercentage: public SpinBoxSetting, public GlobalSetting {
public:
    VertScanPercentage():
        SpinBoxSetting(-100, 100, 1),
        GlobalSetting("VertScanPercentage") {
        setLabel("Vertical over/underscan percentage");
        setValue(0);
        setHelpText("Adjust this if the image does not fill your screen vertically.");
    };
};

class HorizScanPercentage: public SpinBoxSetting, public GlobalSetting {
public:
    HorizScanPercentage():
        SpinBoxSetting(-100, 100, 1),
        GlobalSetting("HorizScanPercentage") {
        setLabel("Horizontal over/underscan percentage");
        setValue(0);
        setHelpText("Adjust this if the image does not fill your screen horizontally.");
    };
};

class XScanDisplacement: public SpinBoxSetting, public GlobalSetting {
public:
    XScanDisplacement():
        SpinBoxSetting(0, 800, 1),
        GlobalSetting("XScanDisplacement") {
        setLabel("Scan displacement (X)");
        setValue(0);
        setHelpText("Adjust this to move the image horizontally.");
    };
};

class YScanDisplacement: public SpinBoxSetting, public GlobalSetting {
public:
    YScanDisplacement():
        SpinBoxSetting(0, 800, 1),
        GlobalSetting("YScanDisplacement") {
        setLabel("Scan displacement (Y)");
        setValue(0);
        setHelpText("Adjust this to move the image vertically.");

    };
};

class ReduceJitter: public CheckBoxSetting, public GlobalSetting {
public:
    ReduceJitter():
       GlobalSetting("ReduceJitter") {
       setLabel("Jitter reduction");
       setValue(false);
       setHelpText("If this is set, frame timing will be adjusted for "
                   "smoother motion.");
    };
};

class AggressiveBuffer: public CheckBoxSetting, public GlobalSetting {
public:
    AggressiveBuffer():
       GlobalSetting("AggressiveSoundcardBuffer") {
       setLabel("Aggressive Soundcard Buffering");
       setValue(false);
       setHelpText("If this is set, MythTV will pretend to have a smaller "
                   "soundcard buffer than is really present.  This may speed up "
                   "seeking, but can also cause playback problems.");
    };
};

class FixedAspectRatio: public CheckBoxSetting, public GlobalSetting {
public:
    FixedAspectRatio():
       GlobalSetting("FixedAspectRatio") {
       setLabel("Fixed aspect ratio");
       setValue(false);
       setHelpText("If this is set, the TV picture will be shaped to fit a "
                   "4x3 aspect ratio when not in fullscreen mode.");
    };
};

class PlaybackExitPrompt: public CheckBoxSetting, public GlobalSetting {
public:
    PlaybackExitPrompt():
        GlobalSetting("PlaybackExitPrompt") {
        setLabel("Prompt on playback exit");
        setValue(false);
        setHelpText("If set, a menu will be displayed when you exit "
                    "playback mode.  The options available will "
                    "allow you to save your position, delete the "
                    "recording, or continue watching.");
    };
};


class EndOfRecordingExitPrompt: public CheckBoxSetting, public GlobalSetting {
public:
    EndOfRecordingExitPrompt():
        GlobalSetting("EndOfRecordingExitPrompt") {
        setLabel("Prompt at end of recording");
        setValue(false);
        setHelpText("If set, a "
                    "menu will be displayed allowing you to delete the "
                    "recording when it has finished playing.");
    };
};

class GeneratePreviewPixmaps: public CheckBoxSetting, public GlobalSetting {
public:
    GeneratePreviewPixmaps():
        GlobalSetting("GeneratePreviewPixmaps") {
        setLabel("Generate thumbnail preview images for recordings");
        setValue(false);
	setHelpText("If set, a static image of the recording will be "
                    "displayed on the \"Watch a Recording\" menu.");
    };
};

class PlaybackPreview: public CheckBoxSetting, public GlobalSetting {
public:
    PlaybackPreview():
        GlobalSetting("PlaybackPreview") {
        setLabel("Display live preview of recordings");
        setValue(true);
        setHelpText("If set, a preview of the recording will play in a "
                    "small window on the \"Watch a Recording\" menu.");

    };
};

class PlayBoxTransparency: public CheckBoxSetting, public GlobalSetting {
public:
    PlayBoxTransparency():
        GlobalSetting("PlayBoxTransparency") {
        setLabel("Use Transparent Boxes");
        setValue(true);
        setHelpText("If set, the Watch Recording and Delete Recording "
                    "screens will use transparency. Unset this option "
		    "if selecting the recordings is slow.");

    };
};

class PlayBoxShading: public ComboBoxSetting, public GlobalSetting {
public:
    PlayBoxShading():
        GlobalSetting("PlayBoxShading") {
        setLabel("Popup Background Shading Method");
        addSelection("Fill", "0");
        addSelection("Image", "1");
        addSelection("None", "2");
        setHelpText("Fill is the quickest method, but it doesn't look good up close. "
                    "Image looks good from up close, but is somewhat slow. "
                    "And of course no shading will be the fastest.");
    };
};

class AllowQuitShutdown: public ComboBoxSetting, public GlobalSetting {
public:
    AllowQuitShutdown():
        GlobalSetting("AllowQuitShutdown") {
        setLabel("System shutdown");
        addSelection("No exit key", "0");
	addSelection("ESC", "4");
        addSelection("Control-ESC", "1");
        addSelection("Meta-ESC", "2");
        addSelection("Alt-ESC", "3");
        setHelpText("MythTV is designed to run continuously.  If you wish, "
                    "you may use the ESC key or the ESC key + a modifier to exit "
                    "MythTV.  Do not choose a key combination that will be "
                    "intercepted by your window manager.");
    };
};

class HaltCommand: public LineEditSetting, public GlobalSetting {
public:
    HaltCommand():
        GlobalSetting("HaltCommand") {
        setLabel("Halt command");
        setValue("halt");
	setHelpText("If you have configured an exit key on the System "
                    "Shutdown menu, you will be given the opportunity "
                    "to exit MythTV or halt the system completely. "
                    "Another possibility for this field is "
                    "poweroff");
    };
};

class XineramaScreen: public SpinBoxSetting, public GlobalSetting {
public:
    XineramaScreen():
        SpinBoxSetting(0, 8, 1),
        GlobalSetting("XineramaScreen") {
        setLabel("Xinerama screen");
        setValue(0);
        setHelpText("If using Xinerama, run only on the specified screen.");
    };
};

// Theme settings

class GuiWidth: public SpinBoxSetting, public GlobalSetting {
public:
    GuiWidth():
        SpinBoxSetting(0, 1600, 8), GlobalSetting("GuiWidth") {
        setLabel("GUI width");
        setValue(0);
	setHelpText("The width of the GUI.  Do not make the GUI "
                    "wider than your actual screen resolution.  Set to 0 to "
                    "automatically scale to fullscreen.");
    };
};

class GuiHeight: public SpinBoxSetting, public GlobalSetting {
public:
    GuiHeight():
        SpinBoxSetting(0, 1600, 8), GlobalSetting("GuiHeight") {
        setLabel("GUI height");
        setValue(0);
	setHelpText("The height of the GUI.  Do not make the GUI "
                    "taller than your actual screen resolution.  Set to 0 to "
                    "automatically scale to fullscreen.");

    };
};

class ThemeQt: public CheckBoxSetting, public GlobalSetting {
public:
    ThemeQt():
        GlobalSetting("ThemeQt") {
        setLabel("Decorate Qt widgets according to theme");
        setValue(true);
    };
};

class RandomTheme: public CheckBoxSetting, public GlobalSetting {
public:
    RandomTheme():
        GlobalSetting("RandomTheme") {
        setLabel("Use a random theme");
        setValue(false);
        setHelpText("Use a random theme each time MythTV is started.");
    };
};

class MythDateFormat: public ComboBoxSetting, public GlobalSetting {
public:
    MythDateFormat():
        ComboBoxSetting(true), GlobalSetting("DateFormat") {
        setLabel("Date format");
        addSelection("ddd MMM d");
        addSelection("ddd MMMM d");
        addSelection("MMM d");
        addSelection("MM/dd");
        addSelection("MM.dd");
        addSelection("ddd d MMM");
	setHelpText("Your preferred date format.");
    };
};

class MythShortDateFormat: public ComboBoxSetting, public GlobalSetting {
public:
    MythShortDateFormat():
        ComboBoxSetting(true), GlobalSetting("ShortDateFormat") {
        setLabel("Short Date format");
        addSelection("M/d");
        addSelection("d/M");
        addSelection("MM/dd");
        addSelection("dd/MM");
        addSelection("MM.dd");
        setHelpText("Your preferred short date format.");
    };
};

class MythTimeFormat: public ComboBoxSetting, public GlobalSetting {
public:
    MythTimeFormat():
        ComboBoxSetting(true), GlobalSetting("TimeFormat") {
        setLabel("Time format");
        addSelection("h:mm AP");
        addSelection("h:mm ap");
        addSelection("hh:mm AP");
        addSelection("hh:mm ap");
        addSelection("hh:mm");
        addSelection("h:mm");
	setHelpText("Your preferred time format.  Choose a format "
                    "with \"AP\" in it for an AM/PM display, otherwise "
                    "your time display will be 24-hour or \"military\" "
                    "time.");
    };
};

ThemeSelector::ThemeSelector():
    GlobalSetting("Theme") {

    setLabel("Theme");

    QDir themes(PREFIX"/share/mythtv/themes");
    themes.setFilter(QDir::Dirs);
    themes.setSorting(QDir::Name | QDir::IgnoreCase);

    const QFileInfoList *fil = themes.entryInfoList(QDir::Dirs);
    if (!fil)
        return;

    QFileInfoListIterator it( *fil );
    QFileInfo *theme;

    for( ; it.current() != 0 ; ++it ) {
        theme = it.current();
        QFileInfo preview(theme->absFilePath() + "/preview.jpg");
        QFileInfo xml(theme->absFilePath() + "/theme.xml");

        if (theme->fileName()[0] == '.' || !preview.exists() || !xml.exists()) {
            //cout << theme->absFilePath() << " doesn't look like a theme\n";
            continue;
        }

        QImage* previewImage = new QImage(preview.absFilePath());
        if (previewImage->width() == 0 || previewImage->height() == 0) {
            cout << "Problem reading theme preview image " << preview.dirPath() << endl;
            continue;
        }

        addImageSelection(theme->fileName(), previewImage);
    }
}

class DisplayChanNum: public CheckBoxSetting, public GlobalSetting {
public:
    DisplayChanNum():
        GlobalSetting("DisplayChanNum") {
        setLabel("Display channel names instead of numbers");
        setValue(false);
    };
};

class QtFontBig: public SpinBoxSetting, public GlobalSetting {
public:
    QtFontBig():
        SpinBoxSetting(1, 48, 1), GlobalSetting("QtFontBig") {
        setLabel("\"Big\" font");
        setValue(25);
    };
};

class QtFontMedium: public SpinBoxSetting, public GlobalSetting {
public:
    QtFontMedium():
        SpinBoxSetting(1, 48, 1), GlobalSetting("QtFontMedium") {
        setLabel("\"Medium\" font");
        setValue(16);
    };
};

class QtFontSmall: public SpinBoxSetting, public GlobalSetting {
public:
    QtFontSmall():
        SpinBoxSetting(1, 48, 1), GlobalSetting("QtFontSmall") {
        setLabel("\"Small\" font");
        setValue(12);
    };
};

// EPG settings

class EPGScrollType: public CheckBoxSetting, public GlobalSetting {
public:
    EPGScrollType():
        GlobalSetting("EPGScrollType") {
        setLabel("Program Guide Selection Placement");
        setValue(true);
        setHelpText("If unchecked, the program guide's selector will "
                    "stay in the middle of the guide at all times.");
    };
};

class EPGFillType: public ComboBoxSetting, public GlobalSetting {
public:
    EPGFillType():
        GlobalSetting("EPGFillType") {
        setLabel("Guide Shading Method");
        addSelection("Colorized (alpha)", "6");
        addSelection("Colorized (shaded)", "5");
        addSelection("Embossed (shaded)", "3");
        addSelection("Embossed (solid)", "4");
        addSelection("Rounded (shaded)", "1");
        addSelection("Rounded (solid)", "2");
    };
};

class EPGShowChannelIcon: public CheckBoxSetting, public GlobalSetting {
public:
    EPGShowChannelIcon():
        GlobalSetting("EPGShowChannelIcon") {
        setLabel("Display the channel icon");
        setValue(true);
    };
};

class EPGChanDisplay: public SpinBoxSetting, public GlobalSetting {
public:
    EPGChanDisplay():
        SpinBoxSetting(3, 8, 1), GlobalSetting("chanPerPage") {
        setLabel("Channels to Display");
	setValue(5);
	
    };
};

class EPGTimeDisplay: public SpinBoxSetting, public GlobalSetting {
public:
    EPGTimeDisplay():
        SpinBoxSetting(1, 5, 1), GlobalSetting("timePerPage") {
        setLabel("Time Blocks (30 mins) to Display");
        setValue(4);

    };
};

class ChannelSorting: public ComboBoxSetting, public GlobalSetting {
public:
    ChannelSorting():
        GlobalSetting("ChannelSorting") {
        setLabel("Display order of channels");
        addSelection("channum + 0");
        addSelection("chanid");
        addSelection("callsign");
    };
};

class DefaultTVChannel: public LineEditSetting, public GlobalSetting {
public:
    DefaultTVChannel():
        GlobalSetting("DefaultTVChannel") {
        setLabel("Guide starts at channel");
        setValue("3");
        setHelpText("The program guide starts on this channel if it is run "
                    "from outside of LiveTV mode.");
    };
};

class UnknownTitle: public LineEditSetting, public GlobalSetting {
public:
    UnknownTitle():
        GlobalSetting("UnknownTitle") {
        setLabel("What to call 'unknown' programs");
        setValue("Unknown");
    };
};

class UnknownCategory: public LineEditSetting, public GlobalSetting {
public:
    UnknownCategory():
        GlobalSetting("UnknownCategory") {
        setLabel("What category to give 'unknown' programs");
        setValue("Unknown");
    };
};

class AudioSettings: public VerticalConfigurationGroup,
                      public TriggeredConfigurationGroup {
public:
     AudioSettings():
         VerticalConfigurationGroup(false),
         TriggeredConfigurationGroup(false) {
         setLabel("Audio");
         setUseLabel(false);

         addChild(new AudioOutputDevice());
         addChild(new AggressiveBuffer());

         Setting* volumeControl = new MythControlsVolume();
         addChild(volumeControl);
         setTrigger(volumeControl);

         ConfigurationGroup* settings = new VerticalConfigurationGroup(false);
         settings->addChild(new MixerDevice());
         settings->addChild(new MixerControl());
         settings->addChild(new MixerVolume());
         settings->addChild(new PCMVolume());
         addTarget("1", settings);
         
         // show nothing if volumeControl is off
         addTarget("0", new VerticalConfigurationGroup(true));
     };
};

class MythLanguage: public ComboBoxSetting, public GlobalSetting {
public:
    MythLanguage():
        ComboBoxSetting(true), GlobalSetting("Language") {
        setLabel("Language");
        addSelection("English", "EN");
        addSelection("Italian", "IT");
        setHelpText("Your preferred language.");
    };
};

PlaybackSettings::PlaybackSettings()
{
    VerticalConfigurationGroup* general = new VerticalConfigurationGroup(false);
    general->setLabel("General playback");
    general->addChild(new Deinterlace());
    general->addChild(new ReduceJitter());
    general->addChild(new PlaybackExitPrompt());
    general->addChild(new EndOfRecordingExitPrompt());
    general->addChild(new FixedAspectRatio());
    addChild(general);

    AudioSettings *audio = new AudioSettings();
    addChild(audio);

    VerticalConfigurationGroup* seek = new VerticalConfigurationGroup(false);
    seek->setLabel("Seeking");
    seek->addChild(new FastForwardAmount());
    seek->addChild(new RewindAmount());
    seek->addChild(new StickyKeys());
    seek->addChild(new ExactSeeking());
    seek->addChild(new JumpAmount());
    seek->addChild(new CommercialSkipMethod());
    seek->addChild(new CommercialSkipEverywhere());
    seek->addChild(new AutoCommercialSkip());
    seek->addChild(new AutoCommercialFlag());
    addChild(seek);

    VerticalConfigurationGroup* oscan = new VerticalConfigurationGroup(false);
    oscan->setLabel("Overscan");
    oscan->addChild(new VertScanPercentage());
    oscan->addChild(new HorizScanPercentage());
    oscan->addChild(new XScanDisplacement());
    oscan->addChild(new YScanDisplacement());
    addChild(oscan);
}

GeneralSettings::GeneralSettings()
{
    VerticalConfigurationGroup* general = new VerticalConfigurationGroup(false);
    general->setLabel("General");
    general->addChild(new RecordOverTime());
    general->addChild(new PlayBoxOrdering());
    addChild(general);

    VerticalConfigurationGroup* general2 = new VerticalConfigurationGroup(false);
    general2->setLabel("General");
    general2->addChild(new ChannelOrdering());
    general2->addChild(new ChannelSorting());
    general2->addChild(new DisplayChanNum());
    addChild(general2);

    VerticalConfigurationGroup* general3 = new VerticalConfigurationGroup(false);
    general3->setLabel("General");
    general3->addChild(new GeneratePreviewPixmaps());
    general3->addChild(new PlaybackPreview());
    general3->addChild(new XineramaScreen());
    general3->addChild(new AllowQuitShutdown());
    general3->addChild(new HaltCommand());
    addChild(general3);
}

EPGSettings::EPGSettings()
{
    VerticalConfigurationGroup* epg = new VerticalConfigurationGroup(false);
    epg->setLabel("Program Guide");

    epg->addChild(new EPGFillType());
    epg->addChild(new EPGScrollType());
    epg->addChild(new EPGShowChannelIcon());
    epg->addChild(new EPGChanDisplay());
    epg->addChild(new EPGTimeDisplay());
    addChild(epg);

    VerticalConfigurationGroup* gen = new VerticalConfigurationGroup(false);
    gen->setLabel("Program Guide");
    gen->addChild(new UnknownTitle());
    gen->addChild(new UnknownCategory());
    gen->addChild(new DefaultTVChannel());
    addChild(gen);
}

AppearanceSettings::AppearanceSettings()
{
    VerticalConfigurationGroup* theme = new VerticalConfigurationGroup(false);
    theme->setLabel("Theme");

    theme->addChild(new ThemeSelector());
    theme->addChild(new RandomTheme());
    theme->addChild(new GuiWidth());
    theme->addChild(new GuiHeight());
    addChild(theme);

    VerticalConfigurationGroup* dates = new VerticalConfigurationGroup(false);
    dates->setLabel("Localization");    
    dates->addChild(new MythLanguage());
    dates->addChild(new MythDateFormat());
    dates->addChild(new MythShortDateFormat());
    dates->addChild(new MythTimeFormat());
    addChild(dates);

    VerticalConfigurationGroup* qttheme = new VerticalConfigurationGroup(false);
    qttheme->setLabel("QT");
    qttheme->addChild(new QtFontSmall());
    qttheme->addChild(new QtFontMedium());
    qttheme->addChild(new QtFontBig());
    qttheme->addChild(new ThemeQt());
    qttheme->addChild(new PlayBoxTransparency());
    qttheme->addChild(new PlayBoxShading());
    addChild(qttheme);

    VerticalConfigurationGroup* osd = new VerticalConfigurationGroup(false);
    osd->setLabel("On-screen display");
    osd->addChild(new OSDTheme());
    osd->addChild(new OSDDisplayTime());
    osd->addChild(new OSDFont());
    osd->addChild(new OSDCCFont());
    addChild(osd);
}

