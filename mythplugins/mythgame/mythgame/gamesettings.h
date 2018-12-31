#ifndef GAMESETTINGS_H
#define GAMESETTINGS_H

// MythTV headers
#include <standardsettings.h>

const QString GetGameTypeName(const QString &GameType);
const QString GetGameTypeExtensions(const QString &GameType);

struct MPUBLIC GameGeneralSettings : public GroupSetting
{
    GameGeneralSettings();
};

struct PlayerId : public AutoIncrementSetting
{
    PlayerId(uint id) : AutoIncrementSetting("gameplayers", "gameplayerid")
    { setValue(id); }

    int Value() const { return getValue().toInt(); }
};

class GamePlayerSetting : public GroupSetting
{
public:
    GamePlayerSetting(QString name, uint id = 0);

    void Save()         override;                   // StandardSetting
    bool canDelete()    override { return true; }   // GroupSetting
    void deleteEntry()  override;                   // GroupSetting

private:
    PlayerId m_id;
};

class MPUBLIC GamePlayersList : public GroupSetting
{
public:
    GamePlayersList();

private:
    void Load() override; // StandardSetting
    void NewPlayerDialog();
    void CreateNewPlayer(QString name);
};

#endif
