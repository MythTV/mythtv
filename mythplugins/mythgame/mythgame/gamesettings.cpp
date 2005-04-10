#include <mythtv/mythcontext.h>

#include "gamesettings.h"
#include <qfile.h>
#include <qdialog.h>
#include <qcursor.h>
#include <qdir.h>
#include <qimage.h>

// General Settings

static HostLineEdit *GameAllTreeLevels()
{
    HostLineEdit *gc = new HostLineEdit("GameAllTreeLevels");
    gc->setLabel(QObject::tr("Game display order"));
    gc->setValue("system year genre gamename");
    gc->setHelpText(QObject::tr("Order in which to sort the games "
                    "- this is for all systems. Available choices: "
                    "system, year, genre and gamename"));
    return gc;
};

static HostLineEdit *GameFavTreeLevels()
{
    HostLineEdit *gc = new HostLineEdit("GameFavTreeLevels");
    gc->setLabel(QObject::tr("Favourite display order"));
    gc->setValue("gamename");
    gc->setHelpText(QObject::tr("Order in which to sort the games "
                    "marked as favourites "
                    "- this is for all systems. Available choices: "
                    "system, year, genre and gamename"));
    return gc;
};

static HostLineEdit *MameBinary()
{
    HostLineEdit *gc = new HostLineEdit("XMameBinary");
    gc->setLabel(QObject::tr("MAME binary location"));
    gc->setValue("/usr/games/xmame");
    gc->setHelpText(QObject::tr("Location of the XMAME emulator binary."));
    return gc;
};

static HostLineEdit *MameRomPath()
{
    HostLineEdit *gc = new HostLineEdit("MameRomLocation");
    gc->setLabel(QObject::tr("MAME ROM location"));
    gc->setValue("/usr/lib/games/xmame/roms");
    gc->setHelpText(QObject::tr("Location of the MAME games."));
    return gc;
};

static HostLineEdit *MameCatFile()
{
    HostLineEdit *gc = new HostLineEdit("XMameCatFile");
    gc->setLabel(QObject::tr("catver.ini file"));
    gc->setValue("/usr/lib/games/xmame/catver.ini");
    gc->setHelpText(QObject::tr("Path to the catver.ini file"));
    return gc;
};

static HostLineEdit *MameScreensLocation()
{
    HostLineEdit *gc = new HostLineEdit("MameScreensLocation");
    gc->setLabel(QObject::tr("Mame screenshots path"));
    gc->setValue("/var/lib/mythgame/screens");
    gc->setHelpText(QObject::tr("Directory where MAME screenshots are kept."));
    return gc;
};

static HostLineEdit *MameScoresDirectory()
{
    HostLineEdit *gc = new HostLineEdit("MameScoresDirectory");
    gc->setLabel(QObject::tr("MAME hiscores path"));
    gc->setValue("/var/lib/mythgame/hiscore");
    gc->setHelpText(QObject::tr("Directory where MAME hiscores are kept."));
    return gc;
};

static HostLineEdit *MameScoresFile()
{
    HostLineEdit *gc = new HostLineEdit("MameScoresFile");
    gc->setLabel(QObject::tr("MAME hiscores file"));
    gc->setValue("/var/lib/mythgame/hiscore.dat");
    gc->setHelpText(QObject::tr("Path to the MAME hiscore.dat file"));
    return gc;
};

static HostLineEdit *MameFlyersLocation()
{
    HostLineEdit *gc = new HostLineEdit("MameFlyersLocation");
    gc->setLabel(QObject::tr("MAME flyers path"));
    gc->setValue("/var/lib/mythgame/flyers");
    gc->setHelpText(QObject::tr("Directory where MAME flyers are kept."));
    return gc;
};

static HostLineEdit *MameCabinetsLocation()
{
    HostLineEdit *gc = new HostLineEdit("MameCabinetsLocation");
    gc->setLabel(QObject::tr("MAME cabinets path"));
    gc->setValue("/usr/lib/games/xmame/cab");
    gc->setHelpText(QObject::tr("Directory where MAME cabinets are kept."));
    return gc;
};

static HostLineEdit *MameHistoryLocation()
{
    HostLineEdit *gc = new HostLineEdit("MameHistoryLocation");
    gc->setLabel(QObject::tr("MAME history path"));
    gc->setValue("/var/lib/mythgame/history.dat");
    gc->setHelpText(QObject::tr("The path to the MAME history.dat file."));
    return gc;
};

static HostLineEdit *MameCheatLocation()
{
    HostLineEdit *gc = new HostLineEdit("MameCheatLocation");
    gc->setLabel(QObject::tr("MAME cheat files path"));
    gc->setValue("");
    gc->setHelpText(QObject::tr("The path to the MAME cheat.dat file."));
    return gc;
};

static HostLineEdit *MameImageDownloader()
{
    HostLineEdit *gc = new HostLineEdit("MameImageDownloader");
    gc->setLabel(QObject::tr("MAME image downloader"));
    gc->setValue("");
    gc->setHelpText(QObject::tr("The path to the MAME image downloader "
                    "helper.  (See the contrib/ dir in the mythgame source.)"));
    return gc;
};

static HostCheckBox *MameAutomaticallyDownloadImages()
{
    HostCheckBox *gc = new HostCheckBox("MameAutomaticallyDownloadImages");
    gc->setLabel(QObject::tr("Automatically download images"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("Attempt to automatically download ROM images "
                    "if they don't exist."));
    return gc;
};

static HostCheckBox *MameShowDisclaimer()
{
    HostCheckBox *gc = new HostCheckBox("MameShowDisclaimer");
    gc->setLabel(QObject::tr("Show disclaimer"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("Set to show the disclaimer or not"));
    return gc;
};

static HostCheckBox *MameShowGameInfo()
{
    HostCheckBox *gc = new HostCheckBox("MameShowGameInfo");
    gc->setLabel(QObject::tr("Show game info"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("Set to show the game info or not"));
    return gc;
};

static HostLineEdit *NesBinary()
{
    HostLineEdit *gc = new HostLineEdit("NesBinary");
    gc->setLabel(QObject::tr("NES binary location"));
    gc->setValue("/usr/bin/snes9x");
    gc->setHelpText(QObject::tr("Location of the NES emulator binary."));
    return gc;
};

static HostLineEdit *NesRomPath()
{
    HostLineEdit *gc = new HostLineEdit("NesRomLocation");
    gc->setLabel(QObject::tr("NES ROM location"));
    gc->setValue("/usr/lib/games/nes/roms");
    gc->setHelpText(QObject::tr("Location of the NES games."));
    return gc;
};

static HostLineEdit *NesCRCFile()
{
    HostLineEdit *gc = new HostLineEdit("NesCRCFile");
    gc->setLabel(QObject::tr("NES CRC file"));
    gc->setValue("/usr/lib/games/nes/nes.crc");
    gc->setHelpText(QObject::tr("This is the same file that comes with the "
                    "xmame(xmess) distribution: xmame-0.XX/CRC/nes.crc"));
    return gc;
};

static HostLineEdit *NesScreensLocation()
{
    HostLineEdit *gc = new HostLineEdit("NesScreensLocation");
    gc->setLabel(QObject::tr("NES screenshots path"));
    gc->setValue("/usr/lib/games/nes/screens");
    gc->setHelpText(QObject::tr("Directory where NES screenshots are kept."));
    return gc;
};

static HostComboBox *SnesEmulator()
{
    HostComboBox *gc = new HostComboBox("SnesEmulator");
    gc->setLabel(QObject::tr("SNES Emulator"));
    gc->addSelection(QObject::tr("SNES9x"), "SNES9x");
    gc->addSelection(QObject::tr("zSNES"), "zSNES");
    gc->setHelpText(QObject::tr("Which emulator to use"));
    return gc;
};

static HostLineEdit *SnesBinary()
{
    HostLineEdit *gc = new HostLineEdit("SnesBinary");
    gc->setLabel(QObject::tr("SNES binary location"));
    gc->setValue("/usr/bin/snes9x");
    gc->setHelpText(QObject::tr("Location of the snes9x emulator binary."));
    return gc;
};

static HostLineEdit *SnesRomPath()
{
    HostLineEdit *gc = new HostLineEdit("SnesRomLocation");
    gc->setLabel(QObject::tr("SNES ROM location"));
    gc->setValue("/usr/lib/games/snes/roms");
    gc->setHelpText(QObject::tr("Location of the SNES games."));
    return gc;
};

static HostLineEdit *SnesScreensLocation()
{
    HostLineEdit *gc = new HostLineEdit("SnesScreensLocation");
    gc->setLabel(QObject::tr("SNES screenshots path"));
    gc->setValue("/usr/lib/games/snes/screens");
    gc->setHelpText(QObject::tr("Directory where SNES screenshots are kept. "
                    "Looks for screenshots with file names matching the "
                    "rom file name."));
    return gc;
};

static HostLineEdit *AtariBinary()
{
    HostLineEdit *gc = new HostLineEdit("AtariBinary");
    gc->setLabel(QObject::tr("Atari binary location"));
    gc->setValue("/usr/bin/stella.sdl");
    gc->setHelpText(QObject::tr("Location of the Atari emulator binary."));
    return gc;
};

static HostLineEdit *AtariRomPath()
{
    HostLineEdit *gc = new HostLineEdit("AtariRomLocation");
    gc->setLabel(QObject::tr("Atari ROM location"));
    gc->setValue("/usr/lib/games/atari/roms");
    gc->setHelpText(QObject::tr("Location of the Atari games."));
    return gc;
};

static HostLineEdit *Odyssey2Binary()
{
    HostLineEdit *gc = new HostLineEdit("Odyssey2Binary");
    gc->setLabel(QObject::tr("Odyssey2 binary location"));
    gc->setValue("/usr/bin/o2em");
    gc->setHelpText(QObject::tr("Location of the Odyssey2 emulator binary."));
    return gc;
};

static HostLineEdit *Odyssey2RomPath()
{
    HostLineEdit *gc = new HostLineEdit("Odyssey2RomLocation");
    gc->setLabel(QObject::tr("Odyssey2 ROM location"));
    gc->setValue("/usr/lib/games/odyssey2/roms");
    gc->setHelpText(QObject::tr("Location of the Odyssey2 games."));
    return gc;
};

static HostLineEdit *PCList()
{
    HostLineEdit *gc = new HostLineEdit("PCGameList");
    gc->setLabel(QObject::tr("PC Game List xml file"));
    gc->setValue("/usr/games/gamelist.xml");
    gc->setHelpText(QObject::tr("Path to the Game List xml file. (see "
		                "README for details)"));
    return gc;
};

static HostLineEdit *PCScreensLocation()
{
    HostLineEdit *gc = new HostLineEdit("PCScreensLocation");
    gc->setLabel(QObject::tr("PC screenshots path"));
    gc->setValue("/var/lib/mythgame/screens");
    gc->setHelpText(QObject::tr("Directory where screenshots are kept. "
                    "Looks for screenshots with file names matching the "
                    "game name in the Game List xml file."));
    return gc;
};

MythGameSettings::MythGameSettings()
{
    VerticalConfigurationGroup *general = new VerticalConfigurationGroup(false);
    general->setLabel(QObject::tr("MythGame Settings -- General"));
    general->addChild(GameAllTreeLevels());
    general->addChild(GameFavTreeLevels());
    addChild(general);

    VerticalConfigurationGroup *mame = new VerticalConfigurationGroup(false);
    mame->setLabel(QObject::tr("MythGame Settings -- xmame (page 1)"));
    mame->addChild(MameBinary());
    mame->addChild(MameRomPath());
    mame->addChild(MameCatFile());
    mame->addChild(MameScreensLocation());
    mame->addChild(MameScoresDirectory());
    mame->addChild(MameScoresFile());
    mame->addChild(MameFlyersLocation());
    addChild(mame);

    VerticalConfigurationGroup *mame2 = new VerticalConfigurationGroup(false);
    mame2->setLabel(QObject::tr("MythGame Settings -- xmame (page 2)"));
    mame2->addChild(MameCabinetsLocation());
    mame2->addChild(MameHistoryLocation());
    mame2->addChild(MameCheatLocation());
    mame2->addChild(MameImageDownloader());
    mame2->addChild(MameAutomaticallyDownloadImages());
    mame2->addChild(MameShowDisclaimer());
    mame2->addChild(MameShowGameInfo());
    addChild(mame2);

    VerticalConfigurationGroup *nes = new VerticalConfigurationGroup(false);
    nes->setLabel(QObject::tr("MythGame Settings -- NES Emulation"));
    nes->addChild(NesBinary());
    nes->addChild(NesRomPath());
    nes->addChild(NesCRCFile());
    nes->addChild(NesScreensLocation());
    addChild(nes);

    VerticalConfigurationGroup *snes = new VerticalConfigurationGroup(false);
    snes->setLabel(QObject::tr("MythGame Settings -- SNES Emulation"));
    snes->addChild(SnesEmulator());
    snes->addChild(SnesBinary());
    snes->addChild(SnesRomPath());
    snes->addChild(SnesScreensLocation());
    addChild(snes);

    VerticalConfigurationGroup *atari = new VerticalConfigurationGroup(false);
    atari->setLabel(QObject::tr("MythGame Settings -- Atari Emulation"));
    atari->addChild(AtariBinary());
    atari->addChild(AtariRomPath());
    addChild(atari);

    VerticalConfigurationGroup *odyssey2 = new VerticalConfigurationGroup(false);
    odyssey2->setLabel(QObject::tr("MythGame Settings -- Odyssey2 Emulation"));
    odyssey2->addChild(Odyssey2Binary());
    odyssey2->addChild(Odyssey2RomPath());
    addChild(odyssey2);

    VerticalConfigurationGroup *pc = new VerticalConfigurationGroup(false);
    pc->setLabel(QObject::tr("MythGame Settings -- PC games"));
    pc->addChild(PCList());
    pc->addChild(PCScreensLocation());
    addChild(pc);
}
