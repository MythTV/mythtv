#ifndef MAMEHANDLER_H_
#define MAMEHANDLER_H_

#include "gamehandler.h"
#include "mamerominfo.h"
#include "mametypes.h"
#include <qapplication.h>
#include <map>

#include <mythtv/mythdialogs.h>

using namespace std;

class MameHandler : public GameHandler
{
  public:
    MameHandler() : GameHandler() {
                    systemname = "Mame";
                    SetGeneralPrefs();
                    SetDefaultSettings();
                  }
    virtual ~MameHandler();

    void error(const QString &e);
    void start_game(RomInfo *romdata);
    void edit_settings(RomInfo *romdata);
    void edit_system_settings(RomInfo *romdata);
    RomInfo* create_rominfo(RomInfo* parent);
    QString Systemname() { return systemname; }
    void processGames();

    static MameHandler* getHandler(void);

  protected:
    bool check_xmame_exe();
    void makecmd_line(const char * game, QString* exec, MameRomInfo * romentry);
    void SetGeneralPrefs();
    void SetGameSettings(GameSettings &game_settings, MameRomInfo *rominfo);
    void SetDefaultSettings();
    void LoadCatfile(map<QString, QString>* pCatMap);

    GameSettings defaultSettings;
    bool xmame_version_ok;
    int supported_games;

  private:
    static MameHandler* pInstance;
};

#endif
