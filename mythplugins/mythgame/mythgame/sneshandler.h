#ifndef SNESHANDLER_H_
#define SNESHANDLER_H_

#include "gamehandler.h"
#include "snesrominfo.h"
#include "unzip.h"
#include <qapplication.h>
#include <map>

using namespace std;

class RomHeader
{
  public:
    unsigned char RomTitle[21];
    unsigned char RomMakeup;
    unsigned char RomType;
    unsigned char RomSize;
    unsigned char SramSize;
    unsigned char Country;
    unsigned char License;
    unsigned char GameVersion;
    unsigned short InverseRomChecksum;
    unsigned short RomChecksum;
    unsigned short NonMaskableInterupt;
    unsigned short ResetVector;
};

class SnesHandler : public GameHandler
{
  public:
    SnesHandler() : GameHandler() {
                    systemname = "Snes";
                    SetDefaultSettings();
                  }
    virtual ~SnesHandler();

    void start_game(RomInfo *romdata);
    void edit_settings(RomInfo *romdata);
    void edit_system_settings(RomInfo *romdata);
    RomInfo* create_rominfo(RomInfo* parent);
    void processGames();
    void processGames(bool);

    static SnesHandler* getHandler(void);

  private:
    static SnesHandler* pInstance;
    bool IsSnesRom(QString Path,RomHeader* Header, bool bVerifyChecksum);
    bool VerifyZipRomHeader(unzFile zf, unsigned int offset, unsigned int &bytesRead, RomHeader* Header);
    bool VerifyRomHeader(FILE* pFile,unsigned int offset, RomHeader* Header);
    void SetGameSettings(SnesGameSettings &game_settings, SnesRomInfo *rominfo);
    void SetDefaultSettings();

    SnesGameSettings defaultSettings;
};

#endif
