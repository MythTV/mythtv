#ifndef NESHANDLER_H_
#define NESHANDLER_H_

#include <map>
#include <qapplication.h>
#include "gamehandler.h"

using namespace std;

class NesHandler : public GameHandler
{
  public:
    NesHandler() : GameHandler()
        {
            systemname = "Nes";
        }
    virtual ~NesHandler() {};

    void start_game(RomInfo* romdata);
    void edit_settings(QWidget* parent, RomInfo* romdata);
    void edit_system_settings(QWidget* parent, RomInfo* romdata);
    void processGames();
    RomInfo* create_rominfo(RomInfo* parent);
    
    static NesHandler* getHandler();

  private:
    static NesHandler* pInstance;

    bool IsNesRom(QString Path);
    QString GetGameName(QString Path);
    void LoadCRCFile(map<QString, QString> &CRCMap);
    void GetMetadata(QString GameName, QString* Genre, int* Year);
};

#endif
