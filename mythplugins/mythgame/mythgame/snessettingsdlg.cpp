#include <qstring.h>

#include <mythtv/mythcontext.h>

#include "snessettingsdlg.h"

class SnesSetting: public SimpleDBStorage {
public:
    SnesSetting(QString name, QString _romname):
        SimpleDBStorage("snessettings", name),
        romname(_romname) {
        setName(name);
    }

    virtual QString setClause(void)
    {
        return QString("romname = \"%1\", %2 = '%3'")
                      .arg(romname).arg(getColumn()).arg(getValue());
    }

    virtual QString whereClause(void)
    {
        return QString("romname = \"%1\"").arg(romname);
    }

    QString romname;
};

class SnesDefaultOptions: public CheckBoxSetting, public SnesSetting {
public: 
    SnesDefaultOptions(QString rom):
        SnesSetting("usedefault", rom) {
        setLabel(QObject::tr("Use defaults"));
        setValue(true);
        setHelpText(QObject::tr("Use the global default SNES settings. "
                    "All other settings are ignored if this is set."));
    };
};

/* video */
class SnesTransparency: public CheckBoxSetting, public SnesSetting {
public:
    SnesTransparency(QString rom):
        SnesSetting("transparency", rom) {
        setLabel(QObject::tr("Use transparency"));
        setValue(false);
        setHelpText(QObject::tr("No Help Text"));
    };
};

class Snes16: public CheckBoxSetting, public SnesSetting {
public:
    Snes16(QString rom):
        SnesSetting("sixteen", rom) {
        setLabel(QObject::tr("Use 16-bit mode"));
        setValue(false);
        setHelpText(QObject::tr("No Help Text"));
    };
};

class SnesHiRes: public CheckBoxSetting, public SnesSetting {
public:
    SnesHiRes(QString rom):
        SnesSetting("hires", rom) {
        setLabel(QObject::tr("Use Hi-res mode"));
        setValue(false);
        setHelpText(QObject::tr("No Help Text"));
    };
};

class SnesNoModeSwitch: public CheckBoxSetting, public SnesSetting {
public:
    SnesNoModeSwitch(QString rom):
        SnesSetting("nomodeswitch", rom) {
        setLabel(QObject::tr("No modeswitch"));
        setValue(false);
        setHelpText(QObject::tr("No Help Text"));
    };
};

class SnesFullScreen: public CheckBoxSetting, public SnesSetting {
public:
    SnesFullScreen(QString rom):
        SnesSetting("fullscreen", rom) {
        setLabel(QObject::tr("Fullscreen"));
        setValue(false);
        setHelpText(QObject::tr("No Help Text"));
    };
};

class SnesStretch: public CheckBoxSetting, public SnesSetting {
public:
    SnesStretch(QString rom):
        SnesSetting("stretch", rom) {
        setLabel(QObject::tr("Stretch to fit"));
        setValue(false);
        setHelpText(QObject::tr("No Help Text"));
    };
};

class SnesInterpolate: public ComboBoxSetting, public SnesSetting {
public:
    SnesInterpolate(QString rom):
        SnesSetting("interpolate", rom) {
        setLabel(QObject::tr("Interpolation"));
        addSelection(QObject::tr("None"), "0");
        addSelection(QObject::tr("Interpolate 1"), "1");
        addSelection(QObject::tr("Interpolate 2"), "2");
        addSelection(QObject::tr("Interpolate 3"), "3");
        addSelection(QObject::tr("Interpolate 4"), "4");
        addSelection(QObject::tr("Interpolate 5"), "5");
        setHelpText(QObject::tr("No Help Text"));
    };
};

/* misc */
class SnesNoJoy: public CheckBoxSetting, public SnesSetting {
public:
    SnesNoJoy(QString rom):
        SnesSetting("nojoy", rom) {
        setLabel(QObject::tr("No Joystick"));
        setValue(false);
        setHelpText(QObject::tr("No Help Text"));
    };
};

class SnesLayering: public CheckBoxSetting, public SnesSetting {
public:
    SnesLayering(QString rom):
        SnesSetting("layering", rom) {
        setLabel(QObject::tr("Layering"));
        setValue(false);
        setHelpText(QObject::tr("No Help Text"));
    };
};

class SnesInterleaved: public CheckBoxSetting, public SnesSetting {
public:
    SnesInterleaved(QString rom):
        SnesSetting("interleaved", rom) {
        setLabel(QObject::tr("Interleaved"));
        setValue(false);
        setHelpText(QObject::tr("No Help Text"));
    };
};

class SnesAltInterleaved: public CheckBoxSetting, public SnesSetting {
public:
    SnesAltInterleaved(QString rom):
        SnesSetting("altinterleaved", rom) {
        setLabel(QObject::tr("AltInterleaved"));
        setValue(false);
        setHelpText(QObject::tr("No Help Text"));
    };
};

class SnesHirom: public CheckBoxSetting, public SnesSetting {
public:
    SnesHirom(QString rom):
        SnesSetting("hirom", rom) {
        setLabel(QObject::tr("Hi Rom"));
        setValue(false);
        setHelpText(QObject::tr("No Help Text"));
    };
};

class SnesLowrom: public CheckBoxSetting, public SnesSetting {
public:
    SnesLowrom(QString rom):
        SnesSetting("lowrom", rom) {
        setLabel(QObject::tr("Low Rom"));
        setValue(false);
        setHelpText(QObject::tr("No Help Text"));
    };
};

class SnesHeader: public CheckBoxSetting, public SnesSetting {
public:
    SnesHeader(QString rom):
        SnesSetting("header", rom) {
        setLabel(QObject::tr("Header"));
        setValue(false);
        setHelpText(QObject::tr("No Help Text"));
    };
};

class SnesNoHeader: public CheckBoxSetting, public SnesSetting {
public:
    SnesNoHeader(QString rom):
        SnesSetting("noheader", rom) {
        setLabel(QObject::tr("No Header"));
        setValue(false);
        setHelpText(QObject::tr("No Help Text"));
    };
};

class SnesNTSC: public CheckBoxSetting, public SnesSetting {
public:
    SnesNTSC(QString rom):
        SnesSetting("ntsc", rom) {
        setLabel(QObject::tr("NTSC"));
        setValue(false);
        setHelpText(QObject::tr("No Help Text"));
    };
};

class SnesPAL: public CheckBoxSetting, public SnesSetting {
public:
    SnesPAL(QString rom):
        SnesSetting("pal", rom) {
        setLabel(QObject::tr("PAL"));
        setValue(false);
        setHelpText(QObject::tr("No Help Text"));
    };
};

class SnesNoHDMA: public CheckBoxSetting, public SnesSetting {
public:
    SnesNoHDMA(QString rom):
        SnesSetting("nohdma", rom) {
        setLabel(QObject::tr("No HDMA"));
        setValue(false);
        setHelpText(QObject::tr("No Help Text"));
    };
};

class SnesNoWindows: public CheckBoxSetting, public SnesSetting {
public:
    SnesNoWindows(QString rom):
        SnesSetting("nowindows", rom) {
        setLabel(QObject::tr("No Windows"));
        setValue(false);
        setHelpText(QObject::tr("No Help Text"));
    };
};

class SnesNoSpeedHacks: public CheckBoxSetting, public SnesSetting {
public:
    SnesNoSpeedHacks(QString rom):
        SnesSetting("nospeedhacks", rom) {
        setLabel(QObject::tr("No Speed Hacks"));
        setValue(false);
        setHelpText(QObject::tr("No Help Text"));
    };
};

class SnesExtraOptions: public LineEditSetting, public SnesSetting {
public:
    SnesExtraOptions(QString rom):
        SnesSetting("extraoption", rom) {
        setLabel(QObject::tr("Extra options"));
        setValue("");
        setHelpText(QObject::tr("No Help Text"));
    };
};

/* sound */
class SnesNoSound: public CheckBoxSetting, public SnesSetting {
public:
    SnesNoSound(QString rom):
        SnesSetting("nosound", rom) {
        setLabel(QObject::tr("No Sound"));
        setValue(false);
        setHelpText(QObject::tr("No Help Text"));
    };
};

class SnesStereo: public CheckBoxSetting, public SnesSetting {
public:
    SnesStereo(QString rom):
        SnesSetting("stereo", rom) {
        setLabel(QObject::tr("Use stereo audio"));
        setValue(true);
        setHelpText(QObject::tr("No Help Text"));
    };
};

class SnesEnvx: public CheckBoxSetting, public SnesSetting {
public:
    SnesEnvx(QString rom):
        SnesSetting("envx", rom) {
        setLabel(QObject::tr("Envx"));
        setValue(false);
        setHelpText(QObject::tr("No Help Text"));
    };
};

class SnesNoEcho: public CheckBoxSetting, public SnesSetting {
public:
    SnesNoEcho(QString rom):
        SnesSetting("noecho", rom) {
        setLabel(QObject::tr("No Echo"));
        setValue(true);
        setHelpText(QObject::tr("No Help Text"));
    };
};

class SnesThreadSound: public CheckBoxSetting, public SnesSetting {
public:
    SnesThreadSound(QString rom):
        SnesSetting("threadsound", rom) {
        setLabel(QObject::tr("Threaded Sound"));
        setValue(false);
        setHelpText(QObject::tr("No Help Text"));
    };
};

class SnesSyncSound: public CheckBoxSetting, public SnesSetting {
public:
    SnesSyncSound(QString rom):
        SnesSetting("syncsound", rom) {
        setLabel(QObject::tr("Synced Sound"));
        setValue(false);
        setHelpText(QObject::tr("No Help Text"));
    };
};

class SnesInterpSound: public CheckBoxSetting, public SnesSetting {
public:
    SnesInterpSound(QString rom):
        SnesSetting("interpolatedsound", rom) {
        setLabel(QObject::tr("Interpolated Sound"));
        setValue(false);
        setHelpText(QObject::tr("No Help Text"));
    };
};

class SnesNoSampleCache: public CheckBoxSetting, public SnesSetting {
public:
    SnesNoSampleCache(QString rom):
        SnesSetting("nosamplecaching", rom) {
        setLabel(QObject::tr("No sample caching"));
        setValue(false);
        setHelpText(QObject::tr("No Help Text"));
    };
};

class SnesAltDecode: public CheckBoxSetting, public SnesSetting {
public:
    SnesAltDecode(QString rom):
        SnesSetting("altsampledecode", rom) {
        setLabel(QObject::tr("Alt sample decoding"));
        setValue(true);
        setHelpText(QObject::tr("No Help Text"));
    };
};

class SnesNoMaster: public CheckBoxSetting, public SnesSetting {
public:
    SnesNoMaster(QString rom):
        SnesSetting("nomastervolume", rom) {
        setLabel(QObject::tr("No master volume"));
        setValue(false);
        setHelpText(QObject::tr("No Help Text"));
    };
};

class SnesBufferSize: public SpinBoxSetting, public SnesSetting {
public:
    SnesBufferSize(QString rom):
        SpinBoxSetting(0, 32, 1),
        SnesSetting("buffersize", rom) {
        setLabel(QObject::tr("Audio buffer size"));
        setValue(0);
        setHelpText("Select 0 to use the default buffer size");
    };
};

class SnesSoundSkip: public SpinBoxSetting, public SnesSetting {
public:
    SnesSoundSkip(QString rom):
        SpinBoxSetting(0, 3, 1),
        SnesSetting("soundskip", rom) {
        setLabel(QObject::tr("Sound skip"));
        setValue(0);
        setHelpText(QObject::tr("No Help Text"));
    };
};

class SnesQuality: public SpinBoxSetting, public SnesSetting {
public:
    SnesQuality(QString rom):
        SpinBoxSetting(0, 7, 1),
        SnesSetting("soundquality", rom) {
        setLabel(QObject::tr("Sound quality"));
        setValue(4);
        setHelpText(QObject::tr("No Help Text"));
    };
};

SnesSettingsDlg::SnesSettingsDlg(QString romname)
{
    QString title = QObject::tr("SNES Game Settings - ") + romname + 
	    QObject::tr(" - ");
    if (romname != "default")
    {
        VerticalConfigurationGroup *toplevel = 
                                        new VerticalConfigurationGroup(false);
        toplevel->setLabel(title + QObject::tr(" - top level"));

        toplevel->addChild(new SnesDefaultOptions(romname));
        addChild(toplevel);
    }
        
    VerticalConfigurationGroup *video1 = new VerticalConfigurationGroup(false);
    video1->setLabel(title + QObject::tr("video"));
    video1->addChild(new SnesTransparency(romname));
    video1->addChild(new Snes16(romname));
    video1->addChild(new SnesHiRes(romname));
    video1->addChild(new SnesNoModeSwitch(romname));
    video1->addChild(new SnesFullScreen(romname));
    video1->addChild(new SnesStretch(romname));
    video1->addChild(new SnesInterpolate(romname));
    addChild(video1);

    VerticalConfigurationGroup *misc1 = new VerticalConfigurationGroup(false);
    misc1->setLabel(title + QObject::tr("misc page 1"));
    misc1->addChild(new SnesNoJoy(romname));
    misc1->addChild(new SnesLayering(romname));
    misc1->addChild(new SnesInterleaved(romname));
    misc1->addChild(new SnesAltInterleaved(romname));
    misc1->addChild(new SnesHirom(romname));
    misc1->addChild(new SnesLowrom(romname));
    addChild(misc1);

    VerticalConfigurationGroup *misc2 = new VerticalConfigurationGroup(false);
    misc2->setLabel(title + QObject::tr("misc page 2"));
    misc2->addChild(new SnesHeader(romname));
    misc2->addChild(new SnesNoHeader(romname));
    misc2->addChild(new SnesNTSC(romname));
    misc2->addChild(new SnesPAL(romname));
    misc2->addChild(new SnesNoHDMA(romname));
    misc2->addChild(new SnesNoWindows(romname));
    misc2->addChild(new SnesNoSpeedHacks(romname));
    misc2->addChild(new SnesExtraOptions(romname));
    addChild(misc2);

    VerticalConfigurationGroup *sound1 = new VerticalConfigurationGroup(false);
    sound1->setLabel(title + QObject::tr("sound page 1"));
    sound1->addChild(new SnesNoSound(romname));
    sound1->addChild(new SnesStereo(romname));
    sound1->addChild(new SnesEnvx(romname));
    sound1->addChild(new SnesNoEcho(romname));
    sound1->addChild(new SnesThreadSound(romname));
    sound1->addChild(new SnesSyncSound(romname));
    sound1->addChild(new SnesInterpSound(romname));
    addChild(sound1);
    
    VerticalConfigurationGroup *sound2 = new VerticalConfigurationGroup(false);
    sound2->setLabel(title + QObject::tr("sound page 2"));
    sound2->addChild(new SnesNoSampleCache(romname));
    sound2->addChild(new SnesAltDecode(romname));
    sound2->addChild(new SnesNoMaster(romname));
    sound2->addChild(new SnesBufferSize(romname));
    sound2->addChild(new SnesSoundSkip(romname));
    sound2->addChild(new SnesQuality(romname));
    addChild(sound2);
}

