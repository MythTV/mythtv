#include <qstring.h>

#include <mythtv/mythcontext.h>

#include "mamesettingsdlg.h"
#include "mametypes.h"

class MameSetting: public SimpleDBStorage {
public:
    MameSetting(QString name, QString _romname):
        SimpleDBStorage("mamesettings", name),
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

class MameDefaultOptions: public CheckBoxSetting, public MameSetting {
public:
    MameDefaultOptions(QString rom):
        MameSetting("usedefault", rom) {
        setLabel(QObject::tr("Use defaults"));
        setValue(true);
        setHelpText(QObject::tr("Use the global default MAME settings. "
                    "All other settings are ignored if this is set."));
    };
};

/* video */
class MameFullscreen: public ComboBoxSetting, public MameSetting {
public:
    MameFullscreen(QString rom, Prefs *prefs):
        MameSetting("fullscreen", rom) {
        setLabel(QObject::tr("Fullscreen mode"));
        addSelection(QObject::tr("Windowed"), "0");
        if ((!strcmp(prefs->xmame_display_target, "x11")) &&
            (atoi(prefs->xmame_minor) >= 61))
        {
            addSelection(QObject::tr("Fullscreen/DGA"), "1");
            addSelection(QObject::tr("Fullscreen/Xv"), "2");
        }
        else
            addSelection(QObject::tr("Fullscreen"), "1");
    };
};

class MameSkip: public CheckBoxSetting, public MameSetting {
public:
    MameSkip(QString rom):
        MameSetting("autoframeskip", rom) {
        setLabel(QObject::tr("Auto frame skip"));
        setValue(false);
        setHelpText(QObject::tr("Enable autoframeskip"));
    };
};

class MameLeft: public CheckBoxSetting, public MameSetting {
public:
    MameLeft(QString rom):
        MameSetting("rotleft", rom) {
        setLabel(QObject::tr("Rotate left"));
        setValue(false);
        setHelpText(QObject::tr("Rotate screen anti-clockwise"));
    };
};

class MameRight: public CheckBoxSetting, public MameSetting {
public:
    MameRight(QString rom):
        MameSetting("rotright", rom) {
        setLabel(QObject::tr("Rotate right"));
        setValue(false);
        setHelpText(QObject::tr("Rotate screen clockwise"));
    };
};

class MameFlipx: public CheckBoxSetting, public MameSetting {
public:
    MameFlipx(QString rom):
        MameSetting("flipx", rom) {
        setLabel(QObject::tr("Flip X Axis"));
        setValue(false);
        setHelpText(QObject::tr("Flip screen left-right"));
    };
};

class MameFlipy: public CheckBoxSetting, public MameSetting {
public:
    MameFlipy(QString rom):
        MameSetting("flipy", rom) {
        setLabel(QObject::tr("Flip Y Axis"));
        setValue(false);
        setHelpText(QObject::tr("Flip screen upside-down"));
    };
};

class MameExtraArt: public CheckBoxSetting, public MameSetting {
public:
    MameExtraArt(QString rom):
        MameSetting("extra_artwork", rom) {
        setLabel(QObject::tr("Extra Artwork"));
        setValue(false);
        setHelpText(QObject::tr("Use additional game artwork"));
    };
};

class MameScan: public CheckBoxSetting, public MameSetting {
public:
    MameScan(QString rom):
        MameSetting("scanlines", rom) {
        setLabel(QObject::tr("Scanlines"));
        setValue(false);
        setHelpText(QObject::tr("Enable displaying simulated scanlines"));
    };
};

class MameColor: public CheckBoxSetting, public MameSetting {
public:
    MameColor(QString rom):
        MameSetting("autocolordepth", rom) {
        setLabel(QObject::tr("Automatic color depth"));
        setValue(false);
        setHelpText(QObject::tr("No Help text"));
    };
};

class MameScale: public SpinBoxSetting, public MameSetting {
public:
    MameScale(QString rom):
        SpinBoxSetting(1, 5, 1),
        MameSetting("scale", rom) {
        setLabel(QObject::tr("Scaling"));
        setValue(1);
        setHelpText(QObject::tr("Set X-Y Scale to the same aspect ratio. For "
                                "vector games scale may have value's like 1.5 and even "
                                "0.5. For scaling of regular games this will be "
                                "rounded to an int"));
    };
};

/* vector */
class MameAlias: public CheckBoxSetting, public MameSetting {
public:
    MameAlias(QString rom):
        MameSetting("antialias", rom) {
        setLabel(QObject::tr("Antialiasing"));
        setValue(false);
        setHelpText(QObject::tr("Enable antialiasing"));
    };
};

class MameTrans: public CheckBoxSetting, public MameSetting {
public:
    MameTrans(QString rom):
        MameSetting("translucency", rom) {
        setLabel(QObject::tr("Translucency"));
        setValue(false);
        setHelpText(QObject::tr("Enable tranlucency"));
    };
};

class MameRes: public ComboBoxSetting, public MameSetting {
public:
    MameRes(QString rom):
        MameSetting("vectorres", rom) {
        setLabel(QObject::tr("Resolution"));
        addSelection(QObject::tr("Use Scale"), "0");
        addSelection(QObject::tr("640 x 480"), "1");
        addSelection(QObject::tr("800 x 600"), "2");
        addSelection(QObject::tr("1024 x 768"), "3");
        addSelection(QObject::tr("1280 x 1024"), "4");
        addSelection(QObject::tr("1600 x 1200"), "5");
        setHelpText(QObject::tr("Always scale vectorgames to X x Y, keeping "
                                "their aspect ratio. This overrides the scale "
                                "options"));
    };
};

class MameBeam: public LineEditSetting, public MameSetting {
public:
    MameBeam(QString rom):
        MameSetting("beam", rom) {
        setLabel(QObject::tr("Beam"));
        setValue("1.0");
        setHelpText(QObject::tr("Set the beam size for vector games (float)"));
    };
};

class MameFlicker: public LineEditSetting, public MameSetting {
public:
    MameFlicker(QString rom):
        MameSetting("flicker", rom) {
        setLabel(QObject::tr("Flicker"));
        setValue("0.0");
        setHelpText(QObject::tr("Set the flicker for vector games (float)"));
    };
};

/* sound */
class MameSound: public CheckBoxSetting, public MameSetting {
public:
    MameSound(QString rom):
        MameSetting("sound", rom) {
        setLabel(QObject::tr("Use sound"));
        setValue(true);
        setHelpText(QObject::tr("Enable sound (if available)"));
    };
};

class MameSamples: public CheckBoxSetting, public MameSetting {
public:
    MameSamples(QString rom):
        MameSetting("samples", rom) {
        setLabel(QObject::tr("Use samples"));
        setValue(true);
        setHelpText(QObject::tr("Enable samples (if available)"));
    };
};

class MameFake: public CheckBoxSetting, public MameSetting {
public:
    MameFake(QString rom):
        MameSetting("fakesound", rom) {
        setLabel(QObject::tr("Fake sound"));
        setValue(false);
        setHelpText(QObject::tr("Generate sound even when sound is disabled, "
                                "this is needed for some games which won't run "
                                "without sound"));
    };
};

class MameVolume: public SpinBoxSetting, public MameSetting {
public:
    MameVolume(QString rom):
        SpinBoxSetting(-32, 0, 1),
        MameSetting("volume", rom) {
        setLabel(QObject::tr("Volume"));
        setValue(-3);
        setHelpText(QObject::tr("Set volume to x db, (-32 (soft) - 0(loud) )"));
    };
};

/* misc */
class MameCheat: public CheckBoxSetting, public MameSetting {
public:
    MameCheat(QString rom):
        MameSetting("cheat", rom) {
        setLabel(QObject::tr("Enable cheats"));
        setValue(true);
        setHelpText(QObject::tr("Enable cheat subsystem"));
    };
};

class MameExtraOptions: public LineEditSetting, public MameSetting {
public:
    MameExtraOptions(QString rom):
        MameSetting("extraoption", rom) {
        setLabel(QObject::tr("Extra options"));
        setValue("");
        setHelpText(QObject::tr("No Help text"));
    };
};

/* input */
class MameWindows: public CheckBoxSetting, public MameSetting {
public:
    MameWindows(QString rom):
        MameSetting("winkeys", rom) {
        setLabel(QObject::tr("Use Windows Keys"));
        setValue(false);
        setHelpText(QObject::tr("No Help text"));
    };
};

class MameMouse: public CheckBoxSetting, public MameSetting {
public:
    MameMouse(QString rom):
        MameSetting("mouse", rom) {
        setLabel(QObject::tr("Use Mouse"));
        setValue(false);
        setHelpText(QObject::tr("Enable mouse (if supported)"));
    };
};

class MameGrabMouse: public CheckBoxSetting, public MameSetting {
public:
    MameGrabMouse(QString rom):
        MameSetting("grabmouse", rom) {
        setLabel(QObject::tr("Grab Mouse"));
        setValue(false);
        setHelpText(QObject::tr("No Help text"));
    };
};

class MameJoystickType: public ComboBoxSetting, public MameSetting {
public:
    MameJoystickType(QString rom):
        MameSetting("joytype", rom) {
        setLabel(QObject::tr("Joystick Type"));
        addSelection(QObject::tr("No Joystick"), "0");
        addSelection(QObject::tr("i386 Joystick"), "1");
        addSelection(QObject::tr("Fm Town Pad"), "2");
        addSelection(QObject::tr("X11 Input Extension Joystick"), "3");
        addSelection(QObject::tr("New i386 linux 1.x.x Joystick"), "4");
        addSelection(QObject::tr("NetBSD USB Joystick"), "5");
        addSelection(QObject::tr("PS2-Linux native pad"), "6");
        addSelection(QObject::tr("SDL Joystick"), "7");
        setHelpText(QObject::tr("Select type of joystick support to use"));
    };
};

class MameAnalogJoy: public CheckBoxSetting, public MameSetting {
public:
    MameAnalogJoy(QString rom):
        MameSetting("analogjoy", rom) {
        setLabel(QObject::tr("Use joystick as analog"));
        setValue(false);
        setHelpText(QObject::tr("Use Joystick as analog for analog controls"));
    };
};

MameSettingsDlg::MameSettingsDlg(QString romname, Prefs *prefs)
{
    QString title = QObject::tr("Mame Game Settings - ") + romname + 
	    QObject::tr(" - ");
    if (romname != "default")
    {
        VerticalConfigurationGroup *toplevel =
                                        new VerticalConfigurationGroup(false);
        toplevel->setLabel(title + QObject::tr(" - top level"));

        toplevel->addChild(new MameDefaultOptions(romname));
        addChild(toplevel);
    }

    VerticalConfigurationGroup *video1 = new VerticalConfigurationGroup(false);
    video1->setLabel(title + QObject::tr("video page 1"));
    video1->addChild(new MameFullscreen(romname, prefs));
    video1->addChild(new MameSkip(romname));
    video1->addChild(new MameLeft(romname));
    video1->addChild(new MameRight(romname));
    video1->addChild(new MameFlipx(romname));
    video1->addChild(new MameFlipy(romname));
    addChild(video1);

    VerticalConfigurationGroup *video2 = new VerticalConfigurationGroup(false);
    video2->setLabel(title + QObject::tr("video page 2"));
    video2->addChild(new MameExtraArt(romname));
    video2->addChild(new MameScan(romname));
    video2->addChild(new MameColor(romname));
    video2->addChild(new MameScale(romname));
    addChild(video2);

    VerticalConfigurationGroup *vector1 = new VerticalConfigurationGroup(false);
    vector1->setLabel(title + QObject::tr("vector"));
    vector1->addChild(new MameAlias(romname));
    vector1->addChild(new MameTrans(romname));
    vector1->addChild(new MameRes(romname));
    vector1->addChild(new MameBeam(romname));
    vector1->addChild(new MameFlicker(romname));
    addChild(vector1);

    VerticalConfigurationGroup *sound1 = new VerticalConfigurationGroup(false);
    sound1->setLabel(title + QObject::tr("sound"));
    sound1->addChild(new MameSound(romname));
    sound1->addChild(new MameSamples(romname));
    sound1->addChild(new MameFake(romname));
    sound1->addChild(new MameVolume(romname));
    addChild(sound1);

    VerticalConfigurationGroup *input1 = new VerticalConfigurationGroup(false);
    input1->setLabel(title + QObject::tr("input"));
    input1->addChild(new MameWindows(romname));
    input1->addChild(new MameMouse(romname));
    input1->addChild(new MameGrabMouse(romname));
    input1->addChild(new MameJoystickType(romname));
    input1->addChild(new MameAnalogJoy(romname));
    addChild(input1);

    VerticalConfigurationGroup *misc1 = new VerticalConfigurationGroup(false);
    misc1->setLabel(title + QObject::tr("misc"));
    misc1->addChild(new MameCheat(romname));
    misc1->addChild(new MameExtraOptions(romname));
    addChild(misc1);
}

