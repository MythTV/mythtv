#include "globalsettings.h"
#include <qfile.h>
#include <qdialog.h>
#include <qcursor.h>
#include <qdir.h>
#include <qimage.h>

class GlobalSetting: public SimpleDBStorage, virtual public Configurable {
public:
    GlobalSetting(QString name):
        SimpleDBStorage("settings", "data") {
        setName(name);
    };

protected:
    virtual QString whereClause(void) {
        return QString("value = '%1'").arg(getName());
    };

    virtual QString setClause(void) {
        return QString("value = '%1', data = '%1'").arg(getName()).arg(getValue());
    };
};

class AudioOutputDevice: public ComboBoxSetting, public GlobalSetting {
public:
    AudioOutputDevice();
protected:
    static const char* paths[];
};

const char* AudioOutputDevice::paths[] = { "/dev/dsp",
                                           "/dev/dsp1",
                                           "/dev/dsp2",
                                           "/dev/sound/dsp" };

AudioOutputDevice::AudioOutputDevice():
    ComboBoxSetting(true),
    GlobalSetting("AudioOutputDevice") {

    setLabel("Audio output device");
    for(unsigned int i = 0; i < sizeof(paths) / sizeof(char*); ++i) {
        if (QFile(paths[i]).exists())
            addSelection(paths[i]);
    }
}

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
        setLabel("Jump amount (minutes)");
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

class RecordOverTime: public SpinBoxSetting, public GlobalSetting {
public:
    RecordOverTime():
        SpinBoxSetting(0, 600, 1),
        GlobalSetting("RecordOverTime") {
        setLabel("Time to record past end of show (in seconds)");
        setValue(0);
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
        setLabel("Number of seconds for which OSD should be displayed");
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

        addSelection("defaultosd");
        addSelection("blueosd");
        addSelection("oldosd");
        addSelection("none");
    };
};

class OSDFont: public ComboBoxSetting, public GlobalSetting {
public:
    OSDFont():
        GlobalSetting("OSDFont") {

        setLabel("OSD font");
        addSelection("FreeSans.ttf");
    };
};

class OSDCCFont: public ComboBoxSetting, public GlobalSetting {
public:
    OSDCCFont():
        GlobalSetting("OSDCCFont") {

        setLabel("Closed Caption font");
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

// XXX, this should find available inputs in a reusable way
class TunerCardInput: public ComboBoxSetting, public GlobalSetting {
public:
    TunerCardInput():
        ComboBoxSetting(true), GlobalSetting("TunerCardInput") {
        setLabel("Default input");
        addSelection("Television");
        addSelection("Composite1");
        addSelection("Composite3");
        addSelection("S-Video");
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
        setHelpText("If this is set, a "
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
	setHelpText("CURRENTLY DISABLED: If set, a static image of the saved file will be "
                    "displayed on the \"Watch a Recording\" menu.");
    };
};

class PlaybackPreview: public CheckBoxSetting, public GlobalSetting {
public:
    PlaybackPreview():
        GlobalSetting("PlaybackPreview") {
        setLabel("Display live preview of recordings");
        setValue(true);
        setHelpText("If set, a preview of the saved file will play in a "
                    "small window on the \"Watch a Recording\" menu.");

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

class TimeOffset: public ComboBoxSetting, public GlobalSetting {
public:
    TimeOffset():
        GlobalSetting("TimeOffset") {
        setLabel("Time offset for XMLTV listings");
        addSelection("(None)", "");
        addSelection("-0100");
        addSelection("-0200");
        addSelection("-0300");
        addSelection("-0400");
        addSelection("-0500");
        addSelection("-0600");
        addSelection("-0700");
        addSelection("-0800");
        addSelection("-0900");
        addSelection("-1000");
        addSelection("-1100");
        addSelection("-1200");
        addSelection("+0100");
        addSelection("+0200");
        addSelection("+0300");
        addSelection("+0400");
        addSelection("+0500");
        addSelection("+0600");
        addSelection("+0700");
        addSelection("+0800");
        addSelection("+0900");
        addSelection("+1000");
        addSelection("+1100");
        addSelection("+1200");
        setHelpText("If your local timezone does not match the timezone "
                    "returned by XMLTV, use this setting to have "
                    "mythfilldatabase adjust the program start and end times.");
    };
};

// Theme settings

class GuiWidth: public SpinBoxSetting, public GlobalSetting {
public:
    GuiWidth():
        SpinBoxSetting(160, 1600, 8), GlobalSetting("GuiWidth") {
        setLabel("GUI width");
        setValue(800);
	setHelpText("The width of the GUI.  Do not make the GUI "
                    "wider than your actual screen resolution.");
    };
};

class GuiHeight: public SpinBoxSetting, public GlobalSetting {
public:
    GuiHeight():
        SpinBoxSetting(160, 1600, 8), GlobalSetting("GuiHeight") {
        setLabel("GUI height");
        setValue(600);
	setHelpText("The height of the GUI.  Do not make the GUI "
                    "taller than your actual screen resolution.");

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

class MythDateFormat: public ComboBoxSetting, public GlobalSetting {
public:
    MythDateFormat():
        ComboBoxSetting(true), GlobalSetting("DateFormat") {
        setLabel("Date format");
        addSelection("ddd MMMM d");
        addSelection("ddd MMM d");
        addSelection("MMM d");
        addSelection("MM/dd");
        addSelection("MM.dd");
    };
};

class MythTimeFormat: public ComboBoxSetting, public GlobalSetting {
public:
    MythTimeFormat():
        ComboBoxSetting(true), GlobalSetting("TimeFormat") {
        setLabel("Time format");
        addSelection("h:mm AP");
        addSelection("hh:mm AP");
        addSelection("hh:mm");
        addSelection("h:mm");
    };
};

class ThemeSelector: public ImageSelectSetting, public GlobalSetting {
public:
    ThemeSelector();
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

        if ( theme->fileName() == "." || theme->fileName() == ".." )
            continue;

        QFileInfo preview(theme->absFilePath() + "/preview.jpg");
        QFileInfo xml(theme->absFilePath() + "/theme.xml");

        if (!preview.exists() || !xml.exists()) {
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

class EPGShowTitle: public CheckBoxSetting, public GlobalSetting {
public:
    EPGShowTitle():
        GlobalSetting("EPGShowTitle") {
        setLabel("Display the program title at the bottom");
        setValue(true);
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

class EPGTimeFontSize: public SpinBoxSetting, public GlobalSetting {
public:
    EPGTimeFontSize():
        SpinBoxSetting(1, 48, 1), GlobalSetting("EPGTimeFontSize") {
        setLabel("Font size for time");
        setValue(13);
    };
};

class EPGChanFontSize: public SpinBoxSetting, public GlobalSetting {
public:
    EPGChanFontSize():
        SpinBoxSetting(1, 48, 1), GlobalSetting("EPGChanFontSize") {
        setLabel("Font size for channel");
        setValue(13);
    };
};

class EPGChanCallsignFontSize: public SpinBoxSetting, public GlobalSetting {
public:
    EPGChanCallsignFontSize():
        SpinBoxSetting(1, 48, 1), GlobalSetting("EPGChanCallsignFontSize") {
        setLabel("Font size for callsign");
        setValue(11);
    };
};

class EPGProgFontSize: public SpinBoxSetting, public GlobalSetting {
public:
    EPGProgFontSize():
        SpinBoxSetting(1, 48, 1), GlobalSetting("EPGProgFontSize") {
        setLabel("Font size for program");
        setValue(13);
    };
};

class EPGTitleFontSize: public SpinBoxSetting, public GlobalSetting {
public:
    EPGTitleFontSize():
        SpinBoxSetting(1, 48, 1), GlobalSetting("EPGTitleFontSize") {
        setLabel("Font size for title");
        setValue(19);
    };
};

class EPGShowCurrentTime: public CheckBoxSetting, public GlobalSetting {
public:
    EPGShowCurrentTime():
        GlobalSetting("EPGShowCurrentTime") {
        setLabel("Display a line indicating the current time");
        setValue(false);
    };
};

class EPGType: public CheckBoxSetting, public GlobalSetting {
public:
    EPGType():
        GlobalSetting("EPGType") {
        setLabel("Use alternative EPG layout");
        setValue(false);
    };
};

class altEPGChanDisplay: public SpinBoxSetting, public GlobalSetting {
public:
    altEPGChanDisplay():
        SpinBoxSetting(3, 8, 1), GlobalSetting("chanPerPage") {
        setLabel("Channels to Display (in Alternate EPG)");
	setValue(8);
	
    };
};

class altEPGTimeDisplay: public SpinBoxSetting, public GlobalSetting {
public:
    altEPGTimeDisplay():
        SpinBoxSetting(1, 5, 1), GlobalSetting("timePerPage") {
        setLabel("Time Blocks (30 mins) to Display (in Alternate EPG)");
        setValue(5);

    };
};

class EPGCurrentTimeColor: public LineEditSetting, public GlobalSetting {
public:
    EPGCurrentTimeColor():
        GlobalSetting("EPGCurrentTimeColor") {
        setLabel("Color of line indicating the current time");
        setValue("red");
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

class DefaultTVChannel: public SpinBoxSetting, public GlobalSetting {
public:
    DefaultTVChannel():
        SpinBoxSetting(1, 1024, 1), GlobalSetting("DefaultTVChannel") {
        setLabel("Starting channel");
        setValue(3);
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

PlaybackSettings::PlaybackSettings()
{
    VerticalConfigurationGroup* general = new VerticalConfigurationGroup(false);
    general->setLabel("General playback");
    general->addChild(new AudioOutputDevice());
    general->addChild(new Deinterlace());
    general->addChild(new PlaybackExitPrompt());
    general->addChild(new EndOfRecordingExitPrompt());
    general->addChild(new FixedAspectRatio());
    addChild(general);

    VerticalConfigurationGroup* seek = new VerticalConfigurationGroup(false);
    seek->setLabel("Seeking");
    seek->addChild(new FastForwardAmount());
    seek->addChild(new RewindAmount());
    seek->addChild(new StickyKeys());
    seek->addChild(new ExactSeeking());
    seek->addChild(new JumpAmount());
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
    general->addChild(new TimeOffset());
    addChild(general);

    VerticalConfigurationGroup* general2 = new VerticalConfigurationGroup(false);
    general2->setLabel("General");
    general2->addChild(new ChannelOrdering());
    general2->addChild(new ChannelSorting());
    general2->addChild(new DefaultTVChannel());
    general2->addChild(new TunerCardInput());
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

    epg->addChild(new EPGShowTitle());
    epg->addChild(new EPGShowChannelIcon());
    epg->addChild(new EPGShowCurrentTime());
    epg->addChild(new EPGCurrentTimeColor());
    epg->addChild(new EPGType());
    epg->addChild(new altEPGChanDisplay());
    epg->addChild(new altEPGTimeDisplay());
    addChild(epg);

    VerticalConfigurationGroup* fonts = new VerticalConfigurationGroup(false);
    fonts->setLabel("Program Guide Font Sizes");
    fonts->addChild(new EPGTimeFontSize());
    fonts->addChild(new EPGChanFontSize());
    fonts->addChild(new EPGChanCallsignFontSize());
    fonts->addChild(new EPGProgFontSize());
    fonts->addChild(new EPGTitleFontSize());
    addChild(fonts);

    VerticalConfigurationGroup* gen = new VerticalConfigurationGroup(false);
    gen->setLabel("Program Guide");
    gen->addChild(new UnknownTitle());
    gen->addChild(new UnknownCategory());
    addChild(gen);
}

AppearanceSettings::AppearanceSettings()
{
    VerticalConfigurationGroup* theme = new VerticalConfigurationGroup(false);
    theme->setLabel("Theme");

    theme->addChild(new ThemeSelector());
    theme->addChild(new GuiWidth());
    theme->addChild(new GuiHeight());
    theme->addChild(new MythDateFormat());
    theme->addChild(new MythTimeFormat());
    addChild(theme);

    VerticalConfigurationGroup* qttheme = new VerticalConfigurationGroup(false);
    qttheme->setLabel("QT");
    qttheme->addChild(new QtFontSmall());
    qttheme->addChild(new QtFontMedium());
    qttheme->addChild(new QtFontBig());
    qttheme->addChild(new ThemeQt());
    addChild(qttheme);

    VerticalConfigurationGroup* osd = new VerticalConfigurationGroup(false);
    osd->setLabel("On-screen display");
    osd->addChild(new OSDTheme());
    osd->addChild(new OSDDisplayTime());
    osd->addChild(new OSDFont());
    osd->addChild(new OSDCCFont());
    addChild(osd);
}
