#ifndef PCHANDLER_H_
#define PCHANDLER_H_

#include "gamehandler.h"
#include "pcrominfo.h"
#include <qapplication.h>
#include <map>

using namespace std;

class PCHandler : public GameHandler
{
  public:
    PCHandler() : GameHandler() {
                    systemname = "PC";
                  }
    virtual ~PCHandler();

    void start_game(RomInfo *romdata);
    void edit_settings(MythMainWindow *parent,RomInfo *romdata);
    void edit_system_settings(MythMainWindow *parent,RomInfo *romdata);
    PCRomInfo* create_rominfo(RomInfo* parent);
    void processGames();

    static PCHandler* getHandler(void);

  private:
    static PCHandler* pInstance;
};

#endif
