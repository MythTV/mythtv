#include "backendsettings.h"
#include "libmyth/mythcontext.h"
#include "libmyth/settings.h"

class BackendSetting: public SimpleDBStorage, virtual public Configurable {
public:
    BackendSetting(QString name):
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

class RecordFilePrefix: public LineEditSetting, public BackendSetting {
public:
    RecordFilePrefix():
        BackendSetting("RecordFilePrefix") {
        setLabel("Directory to hold recordings");
        setValue("/mnt/store/");
        setHelpText("All recordings get stored in this directory");
    };
};

class TVFormat: public ComboBoxSetting, public BackendSetting {
public:
    TVFormat():
        BackendSetting("TVFormat") {
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

class FreqTable: public ComboBoxSetting, public BackendSetting {
public:
    FreqTable():
        BackendSetting("FreqTable") {
        setLabel("Channel frequency table");
        addSelection("us-cable");
        addSelection("us-bcast");
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

class BufferName: public LineEditSetting, public BackendSetting {
public:
    BufferName():
        BackendSetting("BufferName") {
        setLabel("Live TV buffer");
        setValue("/mnt/store/ringbuf.nuv");
        setHelpText("Create the live tv buffer at this pathname");
    }
};

class BufferSize: public SliderSetting, public BackendSetting {
public:
    BufferSize():
        SliderSetting(1, 100, 1),
        BackendSetting("BufferSize") {

        setLabel("Live TV buffer (GB)");
        setValue(5);
        setHelpText("How large the live tv buffer is allowed to grow");
    };
};

class MaxBufferFill: public SliderSetting, public BackendSetting {
public:
    MaxBufferFill():
        SliderSetting(1, 100, 1),
        BackendSetting("MaxBufferFill") {
        setValue(50);
        setHelpText("How full the live tv buffer is allowed to grow before "
                    "forcing an unpause");
    };
};

class PIPBufferSize: public SpinBoxSetting, public BackendSetting {
public:
    PIPBufferSize():
        SpinBoxSetting(1, 1024, 1), BackendSetting("PIPBufferSize") {
        setLabel("Buffer size");
        setValue(1);
    };
};

class PIPMaxBufferFill: public SpinBoxSetting, public BackendSetting {
public:
    PIPMaxBufferFill():
        SpinBoxSetting(1, 100, 1), BackendSetting("PIPMaxBufferFill") {
        setLabel("Max buffer fill (%)");
        setValue(50);
    };
};

class PIPBufferName: public LineEditSetting, public BackendSetting {
public:
    PIPBufferName():
        BackendSetting("PIPBufferName") {
        setLabel("Buffer name");
        setValue("/mnt/store/ringbuf2.nuv");
    };
};

BackendSettings::BackendSettings(MythContext* context):
    ConfigurationWizard(context) {

    VerticalConfigurationGroup* group1 = new VerticalConfigurationGroup();
    group1->addChild(new RecordFilePrefix());
    group1->addChild(new BufferName());
    group1->addChild(new BufferSize());
    group1->addChild(new MaxBufferFill());
    group1->addChild(new TVFormat());
    group1->addChild(new FreqTable());
    addChild(group1);

    VerticalConfigurationGroup* group2 = new VerticalConfigurationGroup();
    group2->setLabel("Picture-in-picture");
    group2->addChild(new PIPBufferSize());
    group2->addChild(new PIPMaxBufferFill());
    group2->addChild(new PIPBufferName());
    addChild(group2);
}

