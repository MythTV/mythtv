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
        setLabel("Use defaults");
        setValue(true);
        setHelpText("Use the global default MAME settings.  All other settings"
                    " are ignored if this is set.");
    };
};

/* video */
class MameFullscreen: public ComboBoxSetting, public MameSetting {
public:
    MameFullscreen(QString rom, Prefs *prefs):
        MameSetting("fullscreen", rom) {
        setLabel("Fullscreen mode");
        addSelection("Windowed", "0");
        if ((!strcmp(prefs->xmame_display_target, "x11")) &&
            (atoi(prefs->xmame_minor) >= 61))
        {
            addSelection("Fullscreen/DGA", "1");
            addSelection("Fullscreen/Xv", "2");
        }
        else
            addSelection("Fullscreen", "1");
    };
};

class MameSkip: public CheckBoxSetting, public MameSetting {
public:
    MameSkip(QString rom):
        MameSetting("autoframeskip", rom) {
        setLabel("Auto frame skip");
        setValue(false);
    };
};

class MameLeft: public CheckBoxSetting, public MameSetting {
public:
    MameLeft(QString rom):
        MameSetting("rotleft", rom) {
        setLabel("Rotate left");
        setValue(false);
    };
};

class MameRight: public CheckBoxSetting, public MameSetting {
public:
    MameRight(QString rom):
        MameSetting("rotright", rom) {
        setLabel("Rotate right");
        setValue(false);
    };
};

class MameFlipx: public CheckBoxSetting, public MameSetting {
public:
    MameFlipx(QString rom):
        MameSetting("flipx", rom) {
        setLabel("Flip X Axis");
        setValue(false);
    };
};

class MameFlipy: public CheckBoxSetting, public MameSetting {
public:
    MameFlipy(QString rom):
        MameSetting("flipy", rom) {
        setLabel("Flip Y Axis");
        setValue(false);
    };
};

class MameExtraArt: public CheckBoxSetting, public MameSetting {
public:
    MameExtraArt(QString rom):
        MameSetting("extra_artwork", rom) {
        setLabel("Extra Artwork");
        setValue(false);
    };
};

class MameScan: public CheckBoxSetting, public MameSetting {
public:
    MameScan(QString rom):
        MameSetting("scanlines", rom) {
        setLabel("Scanlines");
        setValue(false);
    };
};

class MameColor: public CheckBoxSetting, public MameSetting {
public:
    MameColor(QString rom):
        MameSetting("autocolordepth", rom) {
        setLabel("Automatic color depth");
        setValue(false);
    };
};

class MameScale: public SpinBoxSetting, public MameSetting {
public:
    MameScale(QString rom):
        SpinBoxSetting(1, 5, 1),
        MameSetting("scale", rom) {
        setLabel("Scaling");
        setValue(1);
    };
};

/* vector */
class MameAlias: public CheckBoxSetting, public MameSetting {
public:
    MameAlias(QString rom):
        MameSetting("antialias", rom) {
        setLabel("Antialiasing");
        setValue(false);
    };
};

class MameTrans: public CheckBoxSetting, public MameSetting {
public:
    MameTrans(QString rom):
        MameSetting("translucency", rom) {
        setLabel("Translucency");
        setValue(false);
    };
};

class MameRes: public ComboBoxSetting, public MameSetting {
public:
    MameRes(QString rom):
        MameSetting("vectorres", rom) {
        setLabel("Resolution");
        addSelection("Use Scale", "0");
        addSelection("640 x 480", "1");
        addSelection("800 x 600", "2");
        addSelection("1024 x 768", "3");
        addSelection("1280 x 1024", "4");
        addSelection("1600 x 1200", "5");
    };
};

class MameBeam: public SpinBoxSetting, public MameSetting {
public:
    MameBeam(QString rom):
        SpinBoxSetting(1, 15, 1),
        MameSetting("beam", rom) {
        setLabel("Scaling");
        setValue(1);
    };
};

class MameFlicker: public SpinBoxSetting, public MameSetting {
public:
    MameFlicker(QString rom):
        SpinBoxSetting(0, 10, 1),
        MameSetting("beam", rom) {
        setLabel("Scaling");
        setValue(0);
    };
};

/* sound */
class MameSound: public CheckBoxSetting, public MameSetting {
public:
    MameSound(QString rom):
        MameSetting("sound", rom) {
        setLabel("Use sound");
        setValue(true);
    };
};

class MameSamples: public CheckBoxSetting, public MameSetting {
public:
    MameSamples(QString rom):
        MameSetting("samples", rom) {
        setLabel("Use samples");
        setValue(true);
    };
};

class MameFake: public CheckBoxSetting, public MameSetting {
public:
    MameFake(QString rom):
        MameSetting("fakesound", rom) {
        setLabel("Fake sound");
        setValue(false);
    };
};

class MameVolume: public SpinBoxSetting, public MameSetting {
public:
    MameVolume(QString rom):
        SpinBoxSetting(-32, 0, 1),
        MameSetting("volume", rom) {
        setLabel("Volume");
        setValue(-16);
    };
};

/* misc */
class MameCheat: public CheckBoxSetting, public MameSetting {
public:
    MameCheat(QString rom):
        MameSetting("cheat", rom) {
        setLabel("Enable cheats");
        setValue(true);
    };
};

class MameExtraOptions: public LineEditSetting, public MameSetting {
public:
    MameExtraOptions(QString rom):
        MameSetting("extraoption", rom) {
        setLabel("Extra options");
        setValue("");
    };
};

/* input */
class MameWindows: public CheckBoxSetting, public MameSetting {
public:
    MameWindows(QString rom):
        MameSetting("winkeys", rom) {
        setLabel("Use Windows Keys");
        setValue(false);
    };
};

class MameMouse: public CheckBoxSetting, public MameSetting {
public:
    MameMouse(QString rom):
        MameSetting("mouse", rom) {
        setLabel("Use Mouse");
        setValue(false);
    };
};

class MameGrabMouse: public CheckBoxSetting, public MameSetting {
public:
    MameGrabMouse(QString rom):
        MameSetting("grabmouse", rom) {
        setLabel("Grab Mouse");
        setValue(false);
    };
};

class MameJoystickType: public ComboBoxSetting, public MameSetting {
public:
    MameJoystickType(QString rom):
        MameSetting("joytype", rom) {
        setLabel("Joystick Type");
        addSelection("No Joystick", "0");
        addSelection("i386 Joystick", "1");
        addSelection("Fm Town Pad", "2");
        addSelection("X11 Input Extension Joystick", "3");
        addSelection("new i386 linux 1.x.x Joystick", "4");
        addSelection("NetBSD USB Joystick", "5");
        addSelection("PS2-Linux native pad", "6");
        addSelection("SDL Joystick", "7");
    };
};

class MameAnalogJoy: public CheckBoxSetting, public MameSetting {
public:
    MameAnalogJoy(QString rom):
        MameSetting("analogjoy", rom) {
        setLabel("Use joystick as analog");
        setValue(false);
    };
};

MameSettingsDlg::MameSettingsDlg(QString romname, Prefs *prefs)
{
    QString title = "Mame Game Settings - " + romname + " - ";
    if (romname != "default")
    {
        VerticalConfigurationGroup *toplevel =
                                        new VerticalConfigurationGroup(false);
        toplevel->setLabel(title + " - top level");

        toplevel->addChild(new MameDefaultOptions(romname));
        addChild(toplevel);
    }

    VerticalConfigurationGroup *video1 = new VerticalConfigurationGroup(false);
    video1->setLabel(title + "video page 1");
    video1->addChild(new MameFullscreen(romname, prefs));
    video1->addChild(new MameSkip(romname));
    video1->addChild(new MameLeft(romname));
    video1->addChild(new MameRight(romname));
    video1->addChild(new MameFlipx(romname));
    video1->addChild(new MameFlipy(romname));
    addChild(video1);

    VerticalConfigurationGroup *video2 = new VerticalConfigurationGroup(false);
    video2->setLabel(title + "video page 2");
    video2->addChild(new MameExtraArt(romname));
    video2->addChild(new MameScan(romname));
    video2->addChild(new MameColor(romname));
    video2->addChild(new MameScale(romname));
    addChild(video2);

    VerticalConfigurationGroup *vector1 = new VerticalConfigurationGroup(false);
    vector1->setLabel(title + "vector");
    vector1->addChild(new MameAlias(romname));
    vector1->addChild(new MameTrans(romname));
    vector1->addChild(new MameRes(romname));
    vector1->addChild(new MameBeam(romname));
    vector1->addChild(new MameFlicker(romname));
    addChild(vector1);

    VerticalConfigurationGroup *sound1 = new VerticalConfigurationGroup(false);
    sound1->setLabel(title + "sound");
    sound1->addChild(new MameSound(romname));
    sound1->addChild(new MameSamples(romname));
    sound1->addChild(new MameFake(romname));
    sound1->addChild(new MameVolume(romname));
    addChild(sound1);

    VerticalConfigurationGroup *input1 = new VerticalConfigurationGroup(false);
    input1->setLabel(title + "input");
    input1->addChild(new MameWindows(romname));
    input1->addChild(new MameMouse(romname));
    input1->addChild(new MameGrabMouse(romname));
    input1->addChild(new MameJoystickType(romname));
    input1->addChild(new MameAnalogJoy(romname));
    addChild(input1);

    VerticalConfigurationGroup *misc1 = new VerticalConfigurationGroup(false);
    misc1->setLabel(title + "misc");
    misc1->addChild(new MameCheat(romname));
    misc1->addChild(new MameExtraOptions(romname));
    addChild(misc1);
}

