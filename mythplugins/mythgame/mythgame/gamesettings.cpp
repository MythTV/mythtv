#include <mythtv/mythcontext.h>

#include "gamesettings.h"
#include <qfile.h>
#include <qdialog.h>
#include <qcursor.h>
#include <qdir.h>
#include <qimage.h>

// General Settings

class GameTreeLevels: public LineEditSetting, public GlobalSetting {
public:
    GameTreeLevels():
        GlobalSetting("GameTreeLevels") {
        setLabel("Game display order");
        setValue("system year genre gamename");
        setHelpText("Order in which to sort the games - this is for all "
                    "systems.");
    };
};

class DisplayMode: public ComboBoxSetting, public GlobalSetting {
public:
    DisplayMode():
        GlobalSetting("ShotCount") {
        setLabel("Display mode");
        addSelection("Tree", "0");
        addSelection("Tree and Images", "5");
        setHelpText("Mode to display the games in.");
    };
};

class MameBinary: public LineEditSetting, public GlobalSetting {
public:
    MameBinary():
        GlobalSetting("XMameBinary") {
        setLabel("MAME binary location");
        setValue("/usr/local/bin/xmame.x11");
        setHelpText("Location of the XMAME emulator binary.");
    };
};

class MameRomPath: public LineEditSetting, public GlobalSetting {
public:
    MameRomPath():
        GlobalSetting("MameRomLocation") {
        setLabel("MAME ROM location");
        setValue("/usr/lib/games/xmame/roms");
        setHelpText("Location of the MAME games.");
    };
};

class MameCatFile: public LineEditSetting, public GlobalSetting {
public:
    MameCatFile():
        GlobalSetting("XMameCatFile") {
        setLabel("catver.ini file");
        setValue("/usr/lib/games/xmame/catver.ini");
        setHelpText("Path to the catver.ini file");
    };
};

class MameScreensLocation: public LineEditSetting, public GlobalSetting {
public:
    MameScreensLocation():
        GlobalSetting("MameScreensLocation") {
        setLabel("Mame screenshots path");
        setValue("/screens");
        setHelpText("Directory where MAME screenshots are kept.");
    };
};

class MameScoresLocation: public LineEditSetting, public GlobalSetting {
public:
    MameScoresLocation():
        GlobalSetting("MameScoresLocation") {
        setLabel("MAME hiscores path");
        setValue("");
        setHelpText("Directory where MAME hiscores are kept.");
    };
};

class MameFlyersLocation: public LineEditSetting, public GlobalSetting {
public:
    MameFlyersLocation():
        GlobalSetting("MameFlyersLocation") {
        setLabel("MAME flyers path");
        setValue("/flyers");
        setHelpText("Directory where MAME flyers are kept.");
    };
};

class MameCabinetsLocation: public LineEditSetting, public GlobalSetting {
public:
    MameCabinetsLocation():
        GlobalSetting("MameCabinetsLocation") {
        setLabel("MAME cabinets path");
        setValue("/cabs");
        setHelpText("Directory where MAME cabinets are kept.");
    };
};

class MameHistoryLocation: public LineEditSetting, public GlobalSetting {
public:
    MameHistoryLocation():
        GlobalSetting("MameHistoryLocation") {
        setLabel("MAME history path");
        setValue("");
        setHelpText("Directory where MAME history files are kept.");
    };
};

class MameCheatLocation: public LineEditSetting, public GlobalSetting {
public:
    MameCheatLocation():
        GlobalSetting("MameCheatLocation") {
        setLabel("MAME cheat files path");
        setValue("");
        setHelpText("Directory where MAME cheat files are kept.");
    };
};

class MameShowDisclaimer: public CheckBoxSetting, public GlobalSetting {
public:
    MameShowDisclaimer():
        GlobalSetting("MameShowDisclaimer") {
        setLabel("Show disclaimer");
        setValue(true);
        setHelpText("Set to show the disclaimer or not");
    };
};

class MameShowGameInfo: public CheckBoxSetting, public GlobalSetting {
public:
    MameShowGameInfo():
        GlobalSetting("MameShowGameInfo") {
        setLabel("Show game info");
        setValue(true);
        setHelpText("Set to show the game info or not");
    };
};

class NesBinary: public LineEditSetting, public GlobalSetting {
public:
    NesBinary():
        GlobalSetting("NesBinary") {
        setLabel("NES binary location");
        setValue("/usr/local/bin/fceu");
        setHelpText("Location of the NES emulator binary.");
    };
};

class NesRomPath: public LineEditSetting, public GlobalSetting {
public:
    NesRomPath():
        GlobalSetting("NesRomLocation") {
        setLabel("NES ROM location");
        setValue("/home/media/games/nes/roms");
        setHelpText("Location of the NES games.");
    };
};

class NesCRCFile: public LineEditSetting, public GlobalSetting {
public:
    NesCRCFile():
        GlobalSetting("NesCRCFile") {
        setLabel("NES CRC file");
        setValue("/home/media/games/nes/nes.crc");
        setHelpText("This is the same file that comes with the xmame(xmess) "
                    "distribution: xmame-0.XX/CRC/nes.crc");
    };
};

class NesScreensLocation: public LineEditSetting, public GlobalSetting {
public:
    NesScreensLocation():
        GlobalSetting("NesScreensLocation") {
        setLabel("NES screenshots path");
        setValue("/home/media/games/nes/screens");
        setHelpText("Directory where NES screenshots are kept.");
    };
};

class SnesBinary: public LineEditSetting, public GlobalSetting {
public:
    SnesBinary():
        GlobalSetting("SnesBinary") {
        setLabel("SNES binary location");
        setValue("/usr/X11R6/bin/snes9x");
        setHelpText("Location of the snes9x emulator binary.");
    };
};

class SnesRomPath: public LineEditSetting, public GlobalSetting {
public:
    SnesRomPath():
        GlobalSetting("SnesRomLocation") {
        setLabel("SNES ROM location");
        setValue("/home/media/games/snes/roms");
        setHelpText("Location of the SNES games.");
    };
};

class SnesScreensLocation: public LineEditSetting, public GlobalSetting {
public:
    SnesScreensLocation():
        GlobalSetting("SnesScreensLocation") {
        setLabel("SNES screenshots path");
        setValue("/home/media/games/snes/screens");
        setHelpText("Directory where SNES screenshots are kept.  Looks for "
                    "screenshots with file names matching the rom file name.");
    };
};

class PCList: public LineEditSetting, public GlobalSetting {
public:
    PCList():
        GlobalSetting("PCGameList") {
        setLabel("PC Game List xml file");
        setValue("/usr/games/gamelist.xml");
        setHelpText("Path to the Game List xml file. (see README for details)");
    };
};

class PCScreensLocation: public LineEditSetting, public GlobalSetting {
public:
    PCScreensLocation():
        GlobalSetting("PCScreensLocation") {
        setLabel("PC screenshots path");
        setValue("/usr/games/screens");
        setHelpText("Directory where screenshots are kept.  Looks for "
                    "screenshots with file names matching the game name in "
                    "the Game List xml file.");
    };
};

MythGameSettings::MythGameSettings()
{
    VerticalConfigurationGroup *general = new VerticalConfigurationGroup(false);
    general->setLabel("MythGame Settings -- General");
    general->addChild(new GameTreeLevels());
    general->addChild(new DisplayMode());
    addChild(general);

    VerticalConfigurationGroup *mame = new VerticalConfigurationGroup(false);
    mame->setLabel("MythGame Settings -- xmame (page 1)");
    mame->addChild(new MameBinary());
    mame->addChild(new MameRomPath());
    mame->addChild(new MameCatFile());
    mame->addChild(new MameScreensLocation());
    mame->addChild(new MameScoresLocation());
    mame->addChild(new MameFlyersLocation());
    addChild(mame);

    VerticalConfigurationGroup *mame2 = new VerticalConfigurationGroup(false);
    mame2->setLabel("MythGame Settings -- xmame (page 2)");
    mame2->addChild(new MameCabinetsLocation());
    mame2->addChild(new MameHistoryLocation());
    mame2->addChild(new MameCheatLocation());
    mame2->addChild(new MameShowDisclaimer());
    mame2->addChild(new MameShowGameInfo());
    addChild(mame2);

    VerticalConfigurationGroup *nes = new VerticalConfigurationGroup(false);
    nes->setLabel("MythGame Settings -- NES Emulation");
    nes->addChild(new NesBinary());
    nes->addChild(new NesRomPath());
    nes->addChild(new NesCRCFile());
    nes->addChild(new NesScreensLocation());
    addChild(nes);

    VerticalConfigurationGroup *snes = new VerticalConfigurationGroup(false);
    snes->setLabel("MythGame Settings -- SNES Emulation");
    snes->addChild(new SnesBinary());
    snes->addChild(new SnesRomPath());
    snes->addChild(new SnesScreensLocation());
    addChild(snes);

    VerticalConfigurationGroup *pc = new VerticalConfigurationGroup(false);
    pc->setLabel("MythGame Settings -- PC games");
    pc->addChild(new PCList());
    pc->addChild(new PCScreensLocation());
    addChild(pc);
}
