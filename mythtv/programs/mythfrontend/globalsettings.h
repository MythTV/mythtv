#ifndef MYTHSETTINGS_H
#define MYTHSETTINGS_H

#include "libmyth/settings.h"
#include "libmyth/mythcontext.h"

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

class XMLTVGrab: public ComboBoxSetting, public GlobalSetting {
public:
    XMLTVGrab(): GlobalSetting("XMLTVGrab") {
        setLabel("XMLTV listings grabber");
        addSelection("tv_grab_na");
        addSelection("tv_grab_de");
        addSelection("tv_grab_sn");
        addSelection("tv_grab_uk");
        addSelection("tv_grab_uk_rt");
    };
};

class BufferName: public LineEditSetting, public GlobalSetting {
public:
    BufferName():
        GlobalSetting("BufferName") {
        setLabel("Live TV buffer");
        setValue("/mnt/store/ringbuf.nuv");
    }
};
        
class BufferSize: public SliderSetting, public GlobalSetting {
public:
    BufferSize():
        SliderSetting(1, 100, 1),
        GlobalSetting("BufferSize") {

        setLabel("Live TV buffer (GB)");
        setValue(5);
    };
};

class MaxBufferFill: public SliderSetting, public GlobalSetting {
public:
    MaxBufferFill():
        SliderSetting(1, 100, 1),
        GlobalSetting("MaxBufferFill") {
        setValue(50);
    };
};


class RecordFilePrefix: public LineEditSetting, public GlobalSetting {
public:
    RecordFilePrefix():
        GlobalSetting("RecordFilePrefix") {
        setLabel("Directory to hold recordings");
        setValue("/mnt/store/");
    };
};

class AudioOutputDevice: public ComboBoxSetting, public GlobalSetting {
public:
    AudioOutputDevice();
protected:
    static const char* paths[];
};

class Deinterlace: public CheckBoxSetting, public GlobalSetting {
public:
    Deinterlace():
        GlobalSetting("Deinterlace") {
        setLabel("Deinterlace playback?");
        setValue(true);
    };
};

class FastForwardAmount: public SpinBoxSetting, public GlobalSetting {
public:
    FastForwardAmount():
        SpinBoxSetting(1, 600, 1),
        GlobalSetting("FastForwardAmount") {
        setLabel("Fast forward amount (seconds)");
        setValue(30);
    };
};

class RewindAmount: public SpinBoxSetting, public GlobalSetting {
public:
    RewindAmount():
        SpinBoxSetting(1, 600, 1),
        GlobalSetting("RewindAmount") {
        setLabel("Rewind amount (seconds)");
        setValue(5);
    };
};

class ExactSeeking: public CheckBoxSetting, public GlobalSetting {
public:
    ExactSeeking():
        GlobalSetting("ExactSeeking") {
        setLabel("Seek to exact frame?");
        setValue(false);
    };
};

class RecordOverTime: public SpinBoxSetting, public GlobalSetting {
public:
    RecordOverTime():
        SpinBoxSetting(0, 600, 1),
        GlobalSetting("RecordOverTime") {
        setLabel("Time to record past end of show (seconds)");
        setValue(0);
    };
};

class StickyKeys: public CheckBoxSetting, public GlobalSetting {
public:
    StickyKeys():
        GlobalSetting("StickyKeys") {
        setLabel("Sticky keys?");
        setValue(false);
    };
};

class OSDDisplayTime: public SpinBoxSetting, public GlobalSetting {
public:
    OSDDisplayTime():
        SpinBoxSetting(0, 30, 1),
        GlobalSetting("OSDDisplayTime") {
        setLabel("Number of seconds for which OSD should be displayed");
        setValue(3);
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

class ChannelOrdering: public ComboBoxSetting, public GlobalSetting {
public:
    ChannelOrdering():
        GlobalSetting("ChannelOrdering") {
        setLabel("Channel ordering");
        addSelection("channum + 0");
        addSelection("channum");
        addSelection("chanid");
        addSelection("callsign");
    };
};

// XXX, this should find available inputs in a reusable way
class TunerCardInput: public ComboBoxSetting, public GlobalSetting {
public:
    TunerCardInput():
        GlobalSetting("TunerCardInput") {
        setLabel("Default input");
        addSelection("Television");
        addSelection("Composite1");
        addSelection("Composite3");
        addSelection("S-Video");
    };
};

class TVFormat: public ComboBoxSetting, public GlobalSetting {
public:
    TVFormat():
        GlobalSetting("TVFormat") {
        setLabel("TV format");
        addSelection("NTSC");
        addSelection("PAL");
        addSelection("SECAM");
        addSelection("PAL-NC");
        addSelection("PAL-M");
        addSelection("PAL-N");
        addSelection("NTSC-JP");
    };
};

class FreqTable: public ComboBoxSetting, public GlobalSetting {
public:
    FreqTable():
        GlobalSetting("FreqTable") {
        setLabel("Channel frequency table");
        addSelection("us-bcast");
        addSelection("us-cable");
        addSelection("us-cable-hrc");
        addSelection("japan-bcast");
        addSelection("japan-cable");
        addSelection("europe-west");
        addSelection("europe-east");
        addSelection("italy");
        addSelection("newzealand");
        addSelection("australia");
        addSelection("ireland");
        addSelection("france");
        addSelection("china-bcast");
        addSelection("southafrica");
        addSelection("argentina");
        addSelection("canada-cable");
        addSelection("australia-optus");
    };
};

class VertScanPercentage: public SpinBoxSetting, public GlobalSetting {
public:
    VertScanPercentage():
        SpinBoxSetting(-100, 100, 1),
        GlobalSetting("VertScanPercentage") {
        setLabel("Vertical over/underscan percentage");
        setValue(0);
    };
};

class HorizScanPercentage: public SpinBoxSetting, public GlobalSetting {
public:
    HorizScanPercentage():
        SpinBoxSetting(-100, 100, 1),
        GlobalSetting("HorizScanPercentage") {
        setLabel("Horizontal over/underscan percentage");
        setValue(0);
    };
};

class XScanDisplacement: public SpinBoxSetting, public GlobalSetting {
public:
    XScanDisplacement():
        SpinBoxSetting(0, 800, 1),
        GlobalSetting("XScanDisplacement") {
        setLabel("Scan displacement (X)");
        setValue(0);
    };
};

class YScanDisplacement: public SpinBoxSetting, public GlobalSetting {
public:
    YScanDisplacement():
        SpinBoxSetting(0, 800, 1),
        GlobalSetting("YScanDisplacement") {
        setLabel("Scan displacement (Y)");
        setValue(0);
    };
};

class PlaybackExitPrompt: public CheckBoxSetting, public GlobalSetting {
public:
    PlaybackExitPrompt():
        GlobalSetting("PlaybackExitPrompt") {
        setLabel("Prompt on playback exit");
        setValue(false);
    };
};

class ExternalChannelCommand: public LineEditSetting, public GlobalSetting {
public:
    ExternalChannelCommand():
        GlobalSetting("ExternalChannelCommand") {
        setLabel("External channel change command");
        setValue("");
    };
};

// Theme settings

class ThemeSelector: public PathSetting, public GlobalSetting {
public:
    ThemeSelector();
};

class GuiWidth: public SpinBoxSetting, public GlobalSetting {
public:
    GuiWidth():
        SpinBoxSetting(160, 1600, 16), GlobalSetting("GuiWidth") {
        setLabel("GUI width");
    };
};

class GuiHeight: public SpinBoxSetting, public GlobalSetting {
public:
    GuiHeight():
        SpinBoxSetting(160, 1600, 16), GlobalSetting("GuiHeight") {
        setLabel("GUI height");
    };
};

class ThemeQt: public CheckBoxSetting, public GlobalSetting {
public:
    ThemeQt():
        GlobalSetting("ThemeQt") {
        setLabel("Decorate Qt widgets according to theme?");
        setValue(true);
    };
};

class GeneratePreviewPixmaps: public CheckBoxSetting, public GlobalSetting {
public:
    GeneratePreviewPixmaps():
        GlobalSetting("GeneratePreviewPixmaps") {
        setLabel("Generate static thumbnail pixmaps for recordings?");
        setValue(false);
    };
};

class PlaybackPreview: public CheckBoxSetting, public GlobalSetting {
public:
    PlaybackPreview():
        GlobalSetting("PlaybackPreview") {
        setLabel("Play the video back in a little preview window?");
        setValue(true);
    };
};

class DateFormat: public LineEditSetting, public GlobalSetting {
public:
    DateFormat():
        GlobalSetting("DateFormat") {
        setLabel("Date format");
        setValue("ddd MMMM d");
    };
};

class TimeFormat: public LineEditSetting, public GlobalSetting {
public:
    TimeFormat():
        GlobalSetting("TimeFormat") {
        setLabel("Time format");
        setValue("h:mm AP");
    };
};

class DisplayChanNum: public CheckBoxSetting, public GlobalSetting {
public:
    DisplayChanNum():
        GlobalSetting("DisplayChanNum") {
        setLabel("Display channel numbers instead of names?");
        setValue(true);
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
        setLabel("Display the program title at the bottom?");
        setValue(true);
    };
};

class EPGShowChannelIcon: public CheckBoxSetting, public GlobalSetting {
public:
    EPGShowChannelIcon():
        GlobalSetting("EPGShowChannelIcon") {
        setLabel("Display the channel icon?");
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
        setLabel("Display a line indicating the current time?");
        setValue(false);
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

class PIPBufferSize: public SpinBoxSetting, public GlobalSetting {
public:
    PIPBufferSize():
        SpinBoxSetting(1, 1024, 1), GlobalSetting("PIPBufferSize") {
        setLabel("Buffer size");
        setValue(1);
    };
};

class PIPMaxBufferFill: public SpinBoxSetting, public GlobalSetting {
public:
    PIPMaxBufferFill():
        SpinBoxSetting(1, 100, 1), GlobalSetting("PIPMaxBufferFill") {
        setLabel("Max buffer fill (%)");
        setValue(50);
    };
};

class PIPBufferName: public LineEditSetting, public GlobalSetting {
public:
    PIPBufferName():
        GlobalSetting("PIPBufferName") {
        setLabel("Buffer name");
        setValue("/mnt/store/ringbuf2.nuv");
    };
};

class PlaybackSettings: virtual public ConfigurationWizard {
public:
    PlaybackSettings(MythContext *context) : ConfigurationWizard(context) {
        VerticalConfigurationGroup* general = new VerticalConfigurationGroup();
        general->setLabel("General playback");
        general->addChild(new AudioOutputDevice());
        general->addChild(new Deinterlace());
        general->addChild(new FastForwardAmount());
        general->addChild(new RewindAmount());
        general->addChild(new StickyKeys());
        general->addChild(new ExactSeeking());
        general->addChild(new VertScanPercentage());
        general->addChild(new HorizScanPercentage());
        general->addChild(new XScanDisplacement());
        general->addChild(new YScanDisplacement());
        general->addChild(new PlaybackExitPrompt());
        addChild(general);

        VerticalConfigurationGroup* osd = new VerticalConfigurationGroup();
        osd->setLabel("On-screen display");
        osd->addChild(new OSDDisplayTime());
        osd->addChild(new OSDTheme());
        osd->addChild(new OSDFont());
        addChild(osd);

        VerticalConfigurationGroup* liveTV = new VerticalConfigurationGroup();
        liveTV->setLabel("Live TV");
        liveTV->addChild(new BufferName());
        liveTV->addChild(new BufferSize());
        liveTV->addChild(new MaxBufferFill());
        addChild(liveTV);

        VerticalConfigurationGroup* PIP = new VerticalConfigurationGroup();
        PIP->setLabel("Picture-in-picture");
        PIP->addChild(new PIPBufferSize());
        PIP->addChild(new PIPMaxBufferFill());
        PIP->addChild(new PIPBufferName());
        addChild(PIP);
    };
};

// Temporary dumping ground for things that have not been properly categorized yet
class GeneralSettings: virtual public ConfigurationDialog,
                       virtual public VerticalConfigurationGroup {
public:
    GeneralSettings(MythContext *context) : ConfigurationDialog(context) {
        addChild(new XMLTVGrab());
        addChild(new RecordFilePrefix());
        addChild(new ChannelOrdering());
        addChild(new TunerCardInput());
        addChild(new TVFormat());
        addChild(new FreqTable());
        addChild(new RecordOverTime());
        addChild(new ExternalChannelCommand());
        addChild(new ChannelSorting());
        addChild(new DefaultTVChannel());
        addChild(new UnknownTitle());
        addChild(new UnknownCategory());
    };
};

class EPGSettings: virtual public ConfigurationDialog,
                   virtual public VerticalConfigurationGroup {
public:
    EPGSettings(MythContext* context): ConfigurationDialog(context) {
        addChild(new EPGShowTitle());
        addChild(new EPGShowChannelIcon());
        addChild(new EPGTimeFontSize());
        addChild(new EPGChanFontSize());
        addChild(new EPGChanCallsignFontSize());
        addChild(new EPGProgFontSize());
        addChild(new EPGTitleFontSize());
        addChild(new EPGShowCurrentTime());
        addChild(new EPGCurrentTimeColor());
        
    };
};

class ThemeSettings: virtual public ConfigurationDialog,
                     virtual public VerticalConfigurationGroup {
public:
    ThemeSettings(MythContext* context): ConfigurationDialog(context) {
        addChild(new ThemeSelector());
        addChild(new GuiWidth());
        addChild(new GuiHeight());
        addChild(new ThemeQt());
    };
};

#endif
