#ifndef MYTHSETTINGS_H
#define MYTHSETTINGS_H

#include "libmyth/settings.h"
#include "libmyth/mythcontext.h"

class GlobalSetting: public SimpleDBStorage, virtual public Configurable {
public:
    GlobalSetting(QString name):
        SimpleDBStorage("settings", "name") {
        setName(name);
    };

protected:
    virtual QString whereClause(void) {
        return QString("value = '%1'").arg(getName());
    };

    virtual QString setClause(void) {
        return QString("data = '%1'").arg(getValue());
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
        setValue("/var/cache/mythtv/ringbuf.nuv");
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
        setValue("/var/lib/mythtv");
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

class VertScanMode: public ComboBoxSetting, public GlobalSetting {
public:
    VertScanMode():
        GlobalSetting("VertScanMode") {
        setLabel("Vertical scan mode");
        addSelection("overscan");
        addSelection("underscan");
    };
};

class HorizScanMode: public ComboBoxSetting, public GlobalSetting {
public:
    HorizScanMode():
        GlobalSetting("HorizScanMode") {
        setLabel("Horizontal scan mode");
        addSelection("overscan");
        addSelection("underscan");
    };
};

class VertScanPercentage: public SpinBoxSetting, public GlobalSetting {
public:
    VertScanPercentage():
        SpinBoxSetting(0, 100, 1),
        GlobalSetting("VertScanPercentage") {
        setLabel("Vertical over/underscan percentage");
        setValue(0);
    };
};

class HorizScanPercentage: public SpinBoxSetting, public GlobalSetting {
public:
    HorizScanPercentage():
        SpinBoxSetting(0, 100, 1),
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

class PlaybackSettings: virtual public ConfigurationWizard {
public:
    PlaybackSettings() {
        VerticalConfigurationGroup* general = new VerticalConfigurationGroup();
        general->setLabel("General playback");
        general->addChild(new AudioOutputDevice());
        general->addChild(new Deinterlace());
        general->addChild(new FastForwardAmount());
        general->addChild(new RewindAmount());
        general->addChild(new StickyKeys());
        general->addChild(new ExactSeeking());
        general->addChild(new VertScanMode());
        general->addChild(new HorizScanMode());
        general->addChild(new VertScanPercentage());
        general->addChild(new HorizScanPercentage());
        general->addChild(new XScanDisplacement());
        general->addChild(new YScanDisplacement());
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
        // add PIP stuff here
        addChild(liveTV);

    };
};

// Temporary dumping ground for things that have not been properly categorized yet
class GeneralSettings: virtual public ConfigurationDialog,
                       virtual public VerticalConfigurationGroup {
public:
    GeneralSettings() {
        addChild(new XMLTVGrab());
        addChild(new RecordFilePrefix());
        addChild(new ChannelOrdering());
        addChild(new TunerCardInput());
        addChild(new TVFormat());
        addChild(new FreqTable());
        addChild(new RecordOverTime());
    };
};

#endif
