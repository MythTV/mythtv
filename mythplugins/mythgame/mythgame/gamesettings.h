#ifndef GAMESETTINGS_H
#define GAMESETTINGS_H

// MythTV headers
#include <standardsettings.h>

QString GetGameTypeName(const QString &GameType);
QString GetGameTypeExtensions(const QString &GameType);

struct MPUBLIC GameGeneralSettings : public GroupSetting
{
    Q_OBJECT
public:
    GameGeneralSettings();
};

struct PlayerId : public AutoIncrementSetting
{
    explicit PlayerId(uint id) : AutoIncrementSetting("gameplayers", "gameplayerid")
    { setValue(id); }

    int Value() const { return getValue().toInt(); }
};

class GamePlayerSetting : public GroupSetting
{
    Q_OBJECT
public:
    explicit GamePlayerSetting(const QString& name, uint id = 0);

    void Save()         override;                   // StandardSetting
    bool canDelete()    override { return true; }   // GroupSetting
    void deleteEntry()  override;                   // GroupSetting

private:
    PlayerId m_id;
};

class MPUBLIC GamePlayersList : public GroupSetting
{
    Q_OBJECT
public:
    GamePlayersList();

private:
    void Load() override; // StandardSetting
    void NewPlayerDialog() const;
    void CreateNewPlayer(const QString& name);
};

#endif
