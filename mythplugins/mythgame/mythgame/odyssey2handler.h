#ifndef ODYSSEY2HANDLER_H_
#define ODYSSEY2HANDLER_H_

#include <map>
#include <qapplication.h>
#include "gamehandler.h"

#include <mythtv/mythdialogs.h>

using namespace std;

class Odyssey2Handler : public GameHandler
{
  public:
    Odyssey2Handler() : GameHandler()
        {
            systemname = "Odyssey2";
        }
    virtual ~Odyssey2Handler() {};

    void start_game(RomInfo* romdata);
    void edit_settings(RomInfo* romdata);
    void edit_system_settings(RomInfo* romdata);
    void processGames();
    RomInfo* create_rominfo(RomInfo* parent);
    
    static Odyssey2Handler* getHandler();

  private:
    static Odyssey2Handler* pInstance;

    bool IsValidRom(QString Path);
    QString GetGameName(QString Path);
    void GetMetadata(QString GameName, QString* Genre, int* Year);
};

#endif
