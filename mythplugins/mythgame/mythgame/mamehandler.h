#ifndef MAMEHANDLER_H_
#define MAMEHANDLER_H_

#include "gamehandler.h"
#include "mamerominfo.h"
#include "mametypes.h"
#include <qapplication.h>
#include <map>

using namespace std;

class MythContext;

class MameHandler : public GameHandler
{
  public:
    MameHandler(MythContext *context) : GameHandler(context) {
                    systemname = "Mame";
                    SetGeneralPrefs();
                    SetDefaultSettings();
                  }
    virtual ~MameHandler();

    void error(const QString &e);
    void start_game(RomInfo *romdata);
    void edit_settings(QWidget *parent,RomInfo *romdata);
    void edit_system_settings(QWidget *parent,RomInfo *romdata);
    RomInfo* create_rominfo(RomInfo* parent);
    QString Systemname() { return systemname; }
    void processGames();

    static MameHandler* getHandler(MythContext *context);

  protected:
    bool check_xmame_exe();
    void makecmd_line(const char * game, QString* exec, MameRomInfo * romentry);
    void SetGeneralPrefs();
    void SetGameSettings(GameSettings &game_settings, MameRomInfo *rominfo);
    void SaveGameSettings(GameSettings &game_settings, MameRomInfo *romdata);
    void SetDefaultSettings();
    void LoadCatfile(map<QString, QString>* pCatMap);

    GameSettings defaultSettings;
    bool xmame_version_ok;
    int supported_games;

  private:
    static MameHandler* pInstance;
};

#endif
