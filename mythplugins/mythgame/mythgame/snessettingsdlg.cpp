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
        setLabel("Use defaults");
        setValue(true);
        setHelpText("Use the global default SNES settings.  All other settings"
                    " are ignored if this is set.");
    };
};

/* video */
class SnesTransparency: public CheckBoxSetting, public SnesSetting {
public:
    SnesTransparency(QString rom):
        SnesSetting("transparency", rom) {
        setLabel("Use transparency");
        setValue(false);
    };
};

class Snes16: public CheckBoxSetting, public SnesSetting {
public:
    Snes16(QString rom):
        SnesSetting("sixteen", rom) {
        setLabel("Use 16-bit mode");
        setValue(false);
    };
};

class SnesHiRes: public CheckBoxSetting, public SnesSetting {
public:
    SnesHiRes(QString rom):
        SnesSetting("hires", rom) {
        setLabel("Use Hi-res mode");
        setValue(false);
    };
};

class SnesNoModeSwitch: public CheckBoxSetting, public SnesSetting {
public:
    SnesNoModeSwitch(QString rom):
        SnesSetting("nomodeswitch", rom) {
        setLabel("No modeswitch");
        setValue(false);
    };
};

class SnesFullScreen: public CheckBoxSetting, public SnesSetting {
public:
    SnesFullScreen(QString rom):
        SnesSetting("fullscreen", rom) {
        setLabel("Fullscreen");
        setValue(false);
    };
};

class SnesStretch: public CheckBoxSetting, public SnesSetting {
public:
    SnesStretch(QString rom):
        SnesSetting("stretch", rom) {
        setLabel("Stretch to fit");
        setValue(false);
    };
};

class SnesInterpolate: public ComboBoxSetting, public SnesSetting {
public:
    SnesInterpolate(QString rom):
        SnesSetting("interpolate", rom) {
        setLabel("Interpolation");
        addSelection("None", "0");
        addSelection("Interpolate 1", "1");
        addSelection("Interpolate 2", "2");
        addSelection("Interpolate 3", "3");
        addSelection("Interpolate 4", "4");
        addSelection("Interpolate 5", "5");
    };
};

/* misc */
class SnesNoJoy: public CheckBoxSetting, public SnesSetting {
public:
    SnesNoJoy(QString rom):
        SnesSetting("nojoy", rom) {
        setLabel("No Joystick");
        setValue(false);
    };
};

class SnesLayering: public CheckBoxSetting, public SnesSetting {
public:
    SnesLayering(QString rom):
        SnesSetting("layering", rom) {
        setLabel("Layering");
        setValue(false);
    };
};

class SnesInterleaved: public CheckBoxSetting, public SnesSetting {
public:
    SnesInterleaved(QString rom):
        SnesSetting("interleaved", rom) {
        setLabel("Interleaved");
        setValue(false);
    };
};

class SnesAltInterleaved: public CheckBoxSetting, public SnesSetting {
public:
    SnesAltInterleaved(QString rom):
        SnesSetting("altinterleaved", rom) {
        setLabel("AltInterleaved");
        setValue(false);
    };
};

class SnesHirom: public CheckBoxSetting, public SnesSetting {
public:
    SnesHirom(QString rom):
        SnesSetting("hirom", rom) {
        setLabel("Hi Rom");
        setValue(false);
    };
};

class SnesLowrom: public CheckBoxSetting, public SnesSetting {
public:
    SnesLowrom(QString rom):
        SnesSetting("lowrom", rom) {
        setLabel("Low Rom");
        setValue(false);
    };
};

class SnesHeader: public CheckBoxSetting, public SnesSetting {
public:
    SnesHeader(QString rom):
        SnesSetting("header", rom) {
        setLabel("Header");
        setValue(false);
    };
};

class SnesNoHeader: public CheckBoxSetting, public SnesSetting {
public:
    SnesNoHeader(QString rom):
        SnesSetting("noheader", rom) {
        setLabel("No Header");
        setValue(false);
    };
};

class SnesNTSC: public CheckBoxSetting, public SnesSetting {
public:
    SnesNTSC(QString rom):
        SnesSetting("ntsc", rom) {
        setLabel("NTSC");
        setValue(false);
    };
};

class SnesPAL: public CheckBoxSetting, public SnesSetting {
public:
    SnesPAL(QString rom):
        SnesSetting("pal", rom) {
        setLabel("PAL");
        setValue(false);
    };
};

class SnesNoHDMA: public CheckBoxSetting, public SnesSetting {
public:
    SnesNoHDMA(QString rom):
        SnesSetting("nohdma", rom) {
        setLabel("No HDMA");
        setValue(false);
    };
};

class SnesNoWindows: public CheckBoxSetting, public SnesSetting {
public:
    SnesNoWindows(QString rom):
        SnesSetting("nowindows", rom) {
        setLabel("No Windows");
        setValue(false);
    };
};

class SnesNoSpeedHacks: public CheckBoxSetting, public SnesSetting {
public:
    SnesNoSpeedHacks(QString rom):
        SnesSetting("nospeedhacks", rom) {
        setLabel("No Speed Hacks");
        setValue(false);
    };
};

class SnesExtraOptions: public LineEditSetting, public SnesSetting {
public:
    SnesExtraOptions(QString rom):
        SnesSetting("extraoption", rom) {
        setLabel("Extra options");
        setValue("");
    };
};

/* sound */
class SnesNoSound: public CheckBoxSetting, public SnesSetting {
public:
    SnesNoSound(QString rom):
        SnesSetting("nosound", rom) {
        setLabel("No Sound");
        setValue(false);
    };
};

class SnesStereo: public CheckBoxSetting, public SnesSetting {
public:
    SnesStereo(QString rom):
        SnesSetting("stereo", rom) {
        setLabel("Use stereo audio");
        setValue(true);
    };
};

class SnesEnvx: public CheckBoxSetting, public SnesSetting {
public:
    SnesEnvx(QString rom):
        SnesSetting("envx", rom) {
        setLabel("Envx");
        setValue(false);
    };
};

class SnesNoEcho: public CheckBoxSetting, public SnesSetting {
public:
    SnesNoEcho(QString rom):
        SnesSetting("noecho", rom) {
        setLabel("No Echo");
        setValue(true);
    };
};

class SnesThreadSound: public CheckBoxSetting, public SnesSetting {
public:
    SnesThreadSound(QString rom):
        SnesSetting("threadsound", rom) {
        setLabel("Threaded Sound");
        setValue(false);
    };
};

class SnesSyncSound: public CheckBoxSetting, public SnesSetting {
public:
    SnesSyncSound(QString rom):
        SnesSetting("syncsound", rom) {
        setLabel("Synced Sound");
        setValue(false);
    };
};

class SnesInterpSound: public CheckBoxSetting, public SnesSetting {
public:
    SnesInterpSound(QString rom):
        SnesSetting("interpolatedsound", rom) {
        setLabel("Interpolated Sound");
        setValue(false);
    };
};

class SnesNoSampleCache: public CheckBoxSetting, public SnesSetting {
public:
    SnesNoSampleCache(QString rom):
        SnesSetting("nosamplecaching", rom) {
        setLabel("No sample caching");
        setValue(false);
    };
};

class SnesAltDecode: public CheckBoxSetting, public SnesSetting {
public:
    SnesAltDecode(QString rom):
        SnesSetting("altsampledecode", rom) {
        setLabel("Alt sample decoding");
        setValue(true);
    };
};

class SnesNoMaster: public CheckBoxSetting, public SnesSetting {
public:
    SnesNoMaster(QString rom):
        SnesSetting("nomastervolume", rom) {
        setLabel("No master volume");
        setValue(false);
    };
};

class SnesBufferSize: public SpinBoxSetting, public SnesSetting {
public:
    SnesBufferSize(QString rom):
        SpinBoxSetting(0, 32, 1),
        SnesSetting("buffersize", rom) {
        setLabel("Audio buffer size");
        setValue(0);
        setHelpText("Select 0 to use the default buffer size");
    };
};

class SnesSoundSkip: public SpinBoxSetting, public SnesSetting {
public:
    SnesSoundSkip(QString rom):
        SpinBoxSetting(0, 3, 1),
        SnesSetting("soundskip", rom) {
        setLabel("Sound skip");
        setValue(0);
    };
};

class SnesQuality: public SpinBoxSetting, public SnesSetting {
public:
    SnesQuality(QString rom):
        SpinBoxSetting(0, 7, 1),
        SnesSetting("soundquality", rom) {
        setLabel("Sound quality");
        setValue(4);
    };
};

SnesSettingsDlg::SnesSettingsDlg(QString romname)
{
    QString title = "SNES Game Settings - " + romname + " - ";
    if (romname != "default")
    {
        VerticalConfigurationGroup *toplevel = 
                                        new VerticalConfigurationGroup(false);
        toplevel->setLabel(title + " - top level");

        toplevel->addChild(new SnesDefaultOptions(romname));
        addChild(toplevel);
    }
        
    VerticalConfigurationGroup *video1 = new VerticalConfigurationGroup(false);
    video1->setLabel(title + "video");
    video1->addChild(new SnesTransparency(romname));
    video1->addChild(new Snes16(romname));
    video1->addChild(new SnesHiRes(romname));
    video1->addChild(new SnesNoModeSwitch(romname));
    video1->addChild(new SnesFullScreen(romname));
    video1->addChild(new SnesStretch(romname));
    video1->addChild(new SnesInterpolate(romname));
    addChild(video1);

    VerticalConfigurationGroup *misc1 = new VerticalConfigurationGroup(false);
    misc1->setLabel(title + "misc page 1");
    misc1->addChild(new SnesNoJoy(romname));
    misc1->addChild(new SnesLayering(romname));
    misc1->addChild(new SnesInterleaved(romname));
    misc1->addChild(new SnesAltInterleaved(romname));
    misc1->addChild(new SnesHirom(romname));
    misc1->addChild(new SnesLowrom(romname));
    addChild(misc1);

    VerticalConfigurationGroup *misc2 = new VerticalConfigurationGroup(false);
    misc2->setLabel(title + "misc page 2");
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
    sound1->setLabel(title + "sound page 1");
    sound1->addChild(new SnesNoSound(romname));
    sound1->addChild(new SnesStereo(romname));
    sound1->addChild(new SnesEnvx(romname));
    sound1->addChild(new SnesNoEcho(romname));
    sound1->addChild(new SnesThreadSound(romname));
    sound1->addChild(new SnesSyncSound(romname));
    sound1->addChild(new SnesInterpSound(romname));
    addChild(sound1);
    
    VerticalConfigurationGroup *sound2 = new VerticalConfigurationGroup(false);
    sound2->setLabel(title + "sound page 2");
    sound2->addChild(new SnesNoSampleCache(romname));
    sound2->addChild(new SnesAltDecode(romname));
    sound2->addChild(new SnesNoMaster(romname));
    sound2->addChild(new SnesBufferSize(romname));
    sound2->addChild(new SnesSoundSkip(romname));
    sound2->addChild(new SnesQuality(romname));
    addChild(sound2);
}

