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
        setLabel(QObject::tr("Game display order"));
        setValue("system year genre gamename");
        setHelpText(QObject::tr("Order in which to sort the games "
                    "- this is for all systems."));
    };
};

class GameShowFavorites: public CheckBoxSetting, public GlobalSetting {
public:
    GameShowFavorites():
        GlobalSetting("GameShowFavorites") {
        setLabel(QObject::tr("Show Only Favorites"));
        setValue(false);
        setHelpText(QObject::tr("Limit games listed to only those tagged "
                    "as \"favorite\""));
    };
};

class MameBinary: public LineEditSetting, public GlobalSetting {
public:
    MameBinary():
        GlobalSetting("XMameBinary") {
        setLabel(QObject::tr("MAME binary location"));
        setValue("/usr/games/xmame");
        setHelpText(QObject::tr("Location of the XMAME emulator binary."));
    };
};

class MameRomPath: public LineEditSetting, public GlobalSetting {
public:
    MameRomPath():
        GlobalSetting("MameRomLocation") {
        setLabel(QObject::tr("MAME ROM location"));
        setValue("/usr/lib/games/xmame/roms");
        setHelpText(QObject::tr("Location of the MAME games."));
    };
};

class MameCatFile: public LineEditSetting, public GlobalSetting {
public:
    MameCatFile():
        GlobalSetting("XMameCatFile") {
        setLabel(QObject::tr("catver.ini file"));
        setValue("/usr/lib/games/xmame/catver.ini");
        setHelpText(QObject::tr("Path to the catver.ini file"));
    };
};

class MameScreensLocation: public LineEditSetting, public GlobalSetting {
public:
    MameScreensLocation():
        GlobalSetting("MameScreensLocation") {
        setLabel(QObject::tr("Mame screenshots path"));
        setValue("/var/lib/mythgame/screens");
        setHelpText(QObject::tr("Directory where MAME screenshots are kept."));
    };
};

class MameScoresLocation: public LineEditSetting, public GlobalSetting {
public:
    MameScoresLocation():
        GlobalSetting("MameScoresLocation") {
        setLabel(QObject::tr("MAME hiscores path"));
        setValue("/var/lib/mythgame/hiscore.dat");
        setHelpText(QObject::tr("The path to the MAME hiscore.dat file"));
    };
};

class MameFlyersLocation: public LineEditSetting, public GlobalSetting {
public:
    MameFlyersLocation():
        GlobalSetting("MameFlyersLocation") {
        setLabel(QObject::tr("MAME flyers path"));
        setValue("/var/lib/mythgame/flyers");
        setHelpText(QObject::tr("Directory where MAME flyers are kept."));
    };
};

class MameCabinetsLocation: public LineEditSetting, public GlobalSetting {
public:
    MameCabinetsLocation():
        GlobalSetting("MameCabinetsLocation") {
        setLabel(QObject::tr("MAME cabinets path"));
        setValue("/usr/lib/games/xmame/cab");
        setHelpText(QObject::tr("Directory where MAME cabinets are kept."));
    };
};

class MameHistoryLocation: public LineEditSetting, public GlobalSetting {
public:
    MameHistoryLocation():
        GlobalSetting("MameHistoryLocation") {
        setLabel(QObject::tr("MAME history path"));
        setValue("/var/lib/mythgame/history.dat");
        setHelpText(QObject::tr("The path to the MAME history.dat file."));
    };
};

class MameCheatLocation: public LineEditSetting, public GlobalSetting {
public:
    MameCheatLocation():
        GlobalSetting("MameCheatLocation") {
        setLabel(QObject::tr("MAME cheat files path"));
        setValue("");
        setHelpText(QObject::tr("The path to the MAME cheat.dat file."));
    };
};

class MameImageDownloader: public LineEditSetting, public GlobalSetting {
public:
    MameImageDownloader():
        GlobalSetting("MameImageDownloader") {
        setLabel(QObject::tr("MAME image downloader"));
        setValue("");
        setHelpText(QObject::tr("The path to the MAME image downloader "
                    "helper.  (See the contrib/ dir in the mythgame source.)"));
    };
};

class MameAutomaticallyDownloadImages: public CheckBoxSetting, public GlobalSetting {
public:
    MameAutomaticallyDownloadImages():
        GlobalSetting("MameAutomaticallyDownloadImages") {
        setLabel(QObject::tr("Automatically download images"));
        setValue(true);
        setHelpText(QObject::tr("Attempt to automatically download ROM images "
                    "if they don't exist."));
    };
};

class MameShowDisclaimer: public CheckBoxSetting, public GlobalSetting {
public:
    MameShowDisclaimer():
        GlobalSetting("MameShowDisclaimer") {
        setLabel(QObject::tr("Show disclaimer"));
        setValue(true);
        setHelpText(QObject::tr("Set to show the disclaimer or not"));
    };
};

class MameShowGameInfo: public CheckBoxSetting, public GlobalSetting {
public:
    MameShowGameInfo():
        GlobalSetting("MameShowGameInfo") {
        setLabel(QObject::tr("Show game info"));
        setValue(true);
        setHelpText(QObject::tr("Set to show the game info or not"));
    };
};

class NesBinary: public LineEditSetting, public GlobalSetting {
public:
    NesBinary():
        GlobalSetting("NesBinary") {
        setLabel(QObject::tr("NES binary location"));
        setValue("/usr/bin/snes9x");
        setHelpText(QObject::tr("Location of the NES emulator binary."));
    };
};

class NesRomPath: public LineEditSetting, public GlobalSetting {
public:
    NesRomPath():
        GlobalSetting("NesRomLocation") {
        setLabel(QObject::tr("NES ROM location"));
        setValue("/usr/lib/games/nes/roms");
        setHelpText(QObject::tr("Location of the NES games."));
    };
};

class NesCRCFile: public LineEditSetting, public GlobalSetting {
public:
    NesCRCFile():
        GlobalSetting("NesCRCFile") {
        setLabel(QObject::tr("NES CRC file"));
        setValue("/usr/lib/games/nes/nes.crc");
        setHelpText(QObject::tr("This is the same file that comes with the "
                    "xmame(xmess) distribution: xmame-0.XX/CRC/nes.crc"));
    };
};

class NesScreensLocation: public LineEditSetting, public GlobalSetting {
public:
    NesScreensLocation():
        GlobalSetting("NesScreensLocation") {
        setLabel(QObject::tr("NES screenshots path"));
        setValue("/usr/lib/games/nes/screens");
        setHelpText(QObject::tr("Directory where NES screenshots are kept."));
    };
};

class SnesBinary: public LineEditSetting, public GlobalSetting {
public:
    SnesBinary():
        GlobalSetting("SnesBinary") {
        setLabel(QObject::tr("SNES binary location"));
        setValue("/usr/bin/snes9x");
        setHelpText(QObject::tr("Location of the snes9x emulator binary."));
    };
};

class SnesRomPath: public LineEditSetting, public GlobalSetting {
public:
    SnesRomPath():
        GlobalSetting("SnesRomLocation") {
        setLabel(QObject::tr("SNES ROM location"));
        setValue("/usr/lib/games/snes/roms");
        setHelpText(QObject::tr("Location of the SNES games."));
    };
};

class SnesScreensLocation: public LineEditSetting, public GlobalSetting {
public:
    SnesScreensLocation():
        GlobalSetting("SnesScreensLocation") {
        setLabel(QObject::tr("SNES screenshots path"));
        setValue("/usr/lib/games/snes/screens");
        setHelpText(QObject::tr("Directory where SNES screenshots are kept. "
                    "Looks for screenshots with file names matching the "
		    "rom file name."));
    };
};

class PCList: public LineEditSetting, public GlobalSetting {
public:
    PCList():
        GlobalSetting("PCGameList") {
        setLabel(QObject::tr("PC Game List xml file"));
        setValue("/usr/games/gamelist.xml");
        setHelpText(QObject::tr("Path to the Game List xml file. (see "
		    "README for details)"));
    };
};

class PCScreensLocation: public LineEditSetting, public GlobalSetting {
public:
    PCScreensLocation():
        GlobalSetting("PCScreensLocation") {
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
    mame->addChild(new MameScoresLocation());
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
    snes->addChild(new SnesBinary());
    snes->addChild(new SnesRomPath());
    snes->addChild(new SnesScreensLocation());
    addChild(snes);

    VerticalConfigurationGroup *pc = new VerticalConfigurationGroup(false);
    pc->setLabel(QObject::tr("MythGame Settings -- PC games"));
    pc->addChild(new PCList());
    pc->addChild(new PCScreensLocation());
    addChild(pc);
}
