#include <mythtv/mythcontext.h>

#include "gamesettings.h"
#include <qfile.h>
#include <qdialog.h>
#include <qcursor.h>
#include <qdir.h>
#include <qimage.h>

// General Settings

class GameTreeLevels: public GlobalLineEdit {
public:
    GameTreeLevels():
        GlobalLineEdit("GameTreeLevels") {
        setLabel(QObject::tr("Game display order"));
        setValue("system year genre gamename");
        setHelpText(QObject::tr("Order in which to sort the games "
                    "- this is for all systems. Available choices: "
                    "system, year, genre and gamename"));
    };
};

class GameShowFavorites: public GlobalCheckBox {
public:
    GameShowFavorites():
        GlobalCheckBox("GameShowFavorites") {
        setLabel(QObject::tr("Show Only Favorites"));
        setValue(false);
        setHelpText(QObject::tr("Limit games listed to only those tagged "
                    "as \"favorite\""));
    };
};

class MameBinary: public GlobalLineEdit {
public:
    MameBinary():
        GlobalLineEdit("XMameBinary") {
        setLabel(QObject::tr("MAME binary location"));
        setValue("/usr/games/xmame");
        setHelpText(QObject::tr("Location of the XMAME emulator binary."));
    };
};

class MameRomPath: public GlobalLineEdit {
public:
    MameRomPath():
        GlobalLineEdit("MameRomLocation") {
        setLabel(QObject::tr("MAME ROM location"));
        setValue("/usr/lib/games/xmame/roms");
        setHelpText(QObject::tr("Location of the MAME games."));
    };
};

class MameCatFile: public GlobalLineEdit {
public:
    MameCatFile():
        GlobalLineEdit("XMameCatFile") {
        setLabel(QObject::tr("catver.ini file"));
        setValue("/usr/lib/games/xmame/catver.ini");
        setHelpText(QObject::tr("Path to the catver.ini file"));
    };
};

class MameScreensLocation: public GlobalLineEdit {
public:
    MameScreensLocation():
        GlobalLineEdit("MameScreensLocation") {
        setLabel(QObject::tr("Mame screenshots path"));
        setValue("/var/lib/mythgame/screens");
        setHelpText(QObject::tr("Directory where MAME screenshots are kept."));
    };
};

class MameScoresDirectory: public GlobalLineEdit {
public:
    MameScoresDirectory():
        GlobalLineEdit("MameScoresDirectory") {
        setLabel(QObject::tr("MAME hiscores path"));
        setValue("/var/lib/mythgame/hiscore");
        setHelpText(QObject::tr("Directory where MAME hiscores are kept."));
    };
};

class MameScoresFile: public GlobalLineEdit {
public:
    MameScoresFile():
            GlobalLineEdit("MameScoresFile") {
            setLabel(QObject::tr("MAME hiscores file"));
            setValue("/var/lib/mythgame/hiscore.dat");
            setHelpText(QObject::tr("Path to the MAME hiscore.dat file"));
        };
};

class MameFlyersLocation: public GlobalLineEdit {
public:
    MameFlyersLocation():
        GlobalLineEdit("MameFlyersLocation") {
        setLabel(QObject::tr("MAME flyers path"));
        setValue("/var/lib/mythgame/flyers");
        setHelpText(QObject::tr("Directory where MAME flyers are kept."));
    };
};

class MameCabinetsLocation: public GlobalLineEdit {
public:
    MameCabinetsLocation():
        GlobalLineEdit("MameCabinetsLocation") {
        setLabel(QObject::tr("MAME cabinets path"));
        setValue("/usr/lib/games/xmame/cab");
        setHelpText(QObject::tr("Directory where MAME cabinets are kept."));
    };
};

class MameHistoryLocation: public GlobalLineEdit {
public:
    MameHistoryLocation():
        GlobalLineEdit("MameHistoryLocation") {
        setLabel(QObject::tr("MAME history path"));
        setValue("/var/lib/mythgame/history.dat");
        setHelpText(QObject::tr("The path to the MAME history.dat file."));
    };
};

class MameCheatLocation: public GlobalLineEdit {
public:
    MameCheatLocation():
        GlobalLineEdit("MameCheatLocation") {
        setLabel(QObject::tr("MAME cheat files path"));
        setValue("");
        setHelpText(QObject::tr("The path to the MAME cheat.dat file."));
    };
};

class MameImageDownloader: public GlobalLineEdit {
public:
    MameImageDownloader():
        GlobalLineEdit("MameImageDownloader") {
        setLabel(QObject::tr("MAME image downloader"));
        setValue("");
        setHelpText(QObject::tr("The path to the MAME image downloader "
                    "helper.  (See the contrib/ dir in the mythgame source.)"));
    };
};

class MameAutomaticallyDownloadImages: public GlobalCheckBox {
public:
    MameAutomaticallyDownloadImages():
        GlobalCheckBox("MameAutomaticallyDownloadImages") {
        setLabel(QObject::tr("Automatically download images"));
        setValue(true);
        setHelpText(QObject::tr("Attempt to automatically download ROM images "
                    "if they don't exist."));
    };
};

class MameShowDisclaimer: public GlobalCheckBox {
public:
    MameShowDisclaimer():
        GlobalCheckBox("MameShowDisclaimer") {
        setLabel(QObject::tr("Show disclaimer"));
        setValue(true);
        setHelpText(QObject::tr("Set to show the disclaimer or not"));
    };
};

class MameShowGameInfo: public GlobalCheckBox {
public:
    MameShowGameInfo():
        GlobalCheckBox("MameShowGameInfo") {
        setLabel(QObject::tr("Show game info"));
        setValue(true);
        setHelpText(QObject::tr("Set to show the game info or not"));
    };
};

class NesBinary: public GlobalLineEdit {
public:
    NesBinary():
        GlobalLineEdit("NesBinary") {
        setLabel(QObject::tr("NES binary location"));
        setValue("/usr/bin/snes9x");
        setHelpText(QObject::tr("Location of the NES emulator binary."));
    };
};

class NesRomPath: public GlobalLineEdit {
public:
    NesRomPath():
        GlobalLineEdit("NesRomLocation") {
        setLabel(QObject::tr("NES ROM location"));
        setValue("/usr/lib/games/nes/roms");
        setHelpText(QObject::tr("Location of the NES games."));
    };
};

class NesCRCFile: public GlobalLineEdit {
public:
    NesCRCFile():
        GlobalLineEdit("NesCRCFile") {
        setLabel(QObject::tr("NES CRC file"));
        setValue("/usr/lib/games/nes/nes.crc");
        setHelpText(QObject::tr("This is the same file that comes with the "
                    "xmame(xmess) distribution: xmame-0.XX/CRC/nes.crc"));
    };
};

class NesScreensLocation: public GlobalLineEdit {
public:
    NesScreensLocation():
        GlobalLineEdit("NesScreensLocation") {
        setLabel(QObject::tr("NES screenshots path"));
        setValue("/usr/lib/games/nes/screens");
        setHelpText(QObject::tr("Directory where NES screenshots are kept."));
    };
};

class SnesEmulator: public GlobalComboBox {
public:
    SnesEmulator():
        GlobalComboBox("SnesEmulator") {
        setLabel(QObject::tr("SNES Emulator"));
        addSelection(QObject::tr("SNES9x"), "SNES9x");
        addSelection(QObject::tr("zSNES"), "zSNES");
        setHelpText(QObject::tr("Which emulator to use"));
    };
};

class SnesBinary: public GlobalLineEdit {
public:
    SnesBinary():
        GlobalLineEdit("SnesBinary") {
        setLabel(QObject::tr("SNES binary location"));
        setValue("/usr/bin/snes9x");
        setHelpText(QObject::tr("Location of the snes9x emulator binary."));
    };
};

class SnesRomPath: public GlobalLineEdit {
public:
    SnesRomPath():
        GlobalLineEdit("SnesRomLocation") {
        setLabel(QObject::tr("SNES ROM location"));
        setValue("/usr/lib/games/snes/roms");
        setHelpText(QObject::tr("Location of the SNES games."));
    };
};

class SnesScreensLocation: public GlobalLineEdit {
public:
    SnesScreensLocation():
        GlobalLineEdit("SnesScreensLocation") {
        setLabel(QObject::tr("SNES screenshots path"));
        setValue("/usr/lib/games/snes/screens");
        setHelpText(QObject::tr("Directory where SNES screenshots are kept. "
                    "Looks for screenshots with file names matching the "
		    "rom file name."));
    };
};

class AtariBinary: public GlobalLineEdit {
public:
    AtariBinary():
        GlobalLineEdit("AtariBinary") {
        setLabel(QObject::tr("Atari binary location"));
        setValue("/usr/bin/stella.sdl");
        setHelpText(QObject::tr("Location of the Atari emulator binary."));
    };
};

class AtariRomPath: public GlobalLineEdit {
public:
    AtariRomPath():
        GlobalLineEdit("AtariRomLocation") {
        setLabel(QObject::tr("Atari ROM location"));
        setValue("/usr/lib/games/atari/roms");
        setHelpText(QObject::tr("Location of the Atari games."));
    };
};

class Odyssey2Binary: public GlobalLineEdit {
public:
    Odyssey2Binary():
        GlobalLineEdit("Odyssey2Binary") {
        setLabel(QObject::tr("Odyssey2 binary location"));
        setValue("/usr/bin/o2em");
        setHelpText(QObject::tr("Location of the Odyssey2 emulator binary."));
    };
};

class Odyssey2RomPath: public GlobalLineEdit {
public:
    Odyssey2RomPath():
        GlobalLineEdit("Odyssey2RomLocation") {
        setLabel(QObject::tr("Odyssey2 ROM location"));
        setValue("/usr/lib/games/odyssey2/roms");
        setHelpText(QObject::tr("Location of the Odyssey2 games."));
    };
};

class PCList: public GlobalLineEdit {
public:
    PCList():
        GlobalLineEdit("PCGameList") {
        setLabel(QObject::tr("PC Game List xml file"));
        setValue("/usr/games/gamelist.xml");
        setHelpText(QObject::tr("Path to the Game List xml file. (see "
		    "README for details)"));
    };
};

class PCScreensLocation: public GlobalLineEdit {
public:
    PCScreensLocation():
        GlobalLineEdit("PCScreensLocation") {
        setLabel(QObject::tr("PC screenshots path"));
        setValue("/var/lib/mythgame/screens");
        setHelpText(QObject::tr("Directory where screenshots are kept. "
                    "Looks for screenshots with file names matching the "
                    "game name in the Game List xml file."));
    };
};

MythGameSettings::MythGameSettings()
{
    VerticalConfigurationGroup *general = new VerticalConfigurationGroup(false);
    general->setLabel(QObject::tr("MythGame Settings -- General"));
    general->addChild(new GameTreeLevels());
    general->addChild(new GameShowFavorites());
    addChild(general);

    VerticalConfigurationGroup *mame = new VerticalConfigurationGroup(false);
    mame->setLabel(QObject::tr("MythGame Settings -- xmame (page 1)"));
    mame->addChild(new MameBinary());
    mame->addChild(new MameRomPath());
    mame->addChild(new MameCatFile());
    mame->addChild(new MameScreensLocation());
    mame->addChild(new MameScoresDirectory());
    mame->addChild(new MameScoresFile());
    mame->addChild(new MameFlyersLocation());
    addChild(mame);

    VerticalConfigurationGroup *mame2 = new VerticalConfigurationGroup(false);
    mame2->setLabel(QObject::tr("MythGame Settings -- xmame (page 2)"));
    mame2->addChild(new MameCabinetsLocation());
    mame2->addChild(new MameHistoryLocation());
    mame2->addChild(new MameCheatLocation());
    mame2->addChild(new MameImageDownloader());
    mame2->addChild(new MameAutomaticallyDownloadImages());
    mame2->addChild(new MameShowDisclaimer());
    mame2->addChild(new MameShowGameInfo());
    addChild(mame2);

    VerticalConfigurationGroup *nes = new VerticalConfigurationGroup(false);
    nes->setLabel(QObject::tr("MythGame Settings -- NES Emulation"));
    nes->addChild(new NesBinary());
    nes->addChild(new NesRomPath());
    nes->addChild(new NesCRCFile());
    nes->addChild(new NesScreensLocation());
    addChild(nes);

    VerticalConfigurationGroup *snes = new VerticalConfigurationGroup(false);
    snes->setLabel(QObject::tr("MythGame Settings -- SNES Emulation"));
    snes->addChild(new SnesEmulator());
    snes->addChild(new SnesBinary());
    snes->addChild(new SnesRomPath());
    snes->addChild(new SnesScreensLocation());
    addChild(snes);

    VerticalConfigurationGroup *atari = new VerticalConfigurationGroup(false);
    atari->setLabel(QObject::tr("MythGame Settings -- Atari Emulation"));
    atari->addChild(new AtariBinary());
    atari->addChild(new AtariRomPath());
    addChild(atari);

    VerticalConfigurationGroup *odyssey2 = new VerticalConfigurationGroup(false);
    odyssey2->setLabel(QObject::tr("MythGame Settings -- Odyssey2 Emulation"));
    odyssey2->addChild(new Odyssey2Binary());
    odyssey2->addChild(new Odyssey2RomPath());
    addChild(odyssey2);

    VerticalConfigurationGroup *pc = new VerticalConfigurationGroup(false);
    pc->setLabel(QObject::tr("MythGame Settings -- PC games"));
    pc->addChild(new PCList());
    pc->addChild(new PCScreensLocation());
    addChild(pc);
}
