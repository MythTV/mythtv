#ifndef ATARIHANDLER_H_
#define ATARIHANDLER_H_

#include <map>
#include <qapplication.h>
#include "gamehandler.h"

#include <mythtv/mythdialogs.h>

using namespace std;

class AtariHandler : public GameHandler
{
  public:
    AtariHandler() : GameHandler()
        {
            systemname = "Atari";
        }
    virtual ~AtariHandler() {};

    void start_game(RomInfo* romdata);
    void edit_settings(RomInfo* romdata);
    void edit_system_settings(RomInfo* romdata);
    void processGames();
    RomInfo* create_rominfo(RomInfo* parent);
    
    static AtariHandler* getHandler();

  private:
    static AtariHandler* pInstance;

    bool IsValidRom(QString Path);
    QString GetGameName(QString Path);
    void GetMetadata(QString GameName, QString* Genre, int* Year);
};

#endif
