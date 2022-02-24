#ifndef GAMEDETAILS_H_
#define GAMEDETAILS_H_

// Qt
#include <QString>

// MythTV
#include <libmythui/mythscreentype.h>

// MythGame
#include "rominfo.h"

class GameDetailsPopup : public MythScreenType
{
    Q_OBJECT

  public:
    GameDetailsPopup(MythScreenStack *parent, const RomInfo *romInfo) :
        MythScreenType (parent, "gamedetailspopup"),
        m_romInfo(romInfo) {}
    ~GameDetailsPopup() override = default;

    bool Create(void) override; // MythScreenType
    void SetReturnEvent(QObject *retobject, const QString &resultid);

  private slots:
    void Play(void);

  private:
    void        handleText(const QString &name, const QString &value);
    void        handleImage(const QString &name, const QString &filename);

    const RomInfo *m_romInfo     {nullptr};
    QString        m_id;
    QObject       *m_retObject   {nullptr};

    MythUIText    *m_gameName    {nullptr};
    MythUIText    *m_gameType    {nullptr};
    MythUIText    *m_romName     {nullptr};
    MythUIText    *m_crc         {nullptr};
    MythUIText    *m_romPath     {nullptr};
    MythUIText    *m_genre       {nullptr};
    MythUIText    *m_year        {nullptr};
    MythUIText    *m_country     {nullptr};
    MythUIText    *m_plot        {nullptr};
    MythUIText    *m_publisher   {nullptr};
    MythUIText    *m_allSystems  {nullptr};
    MythUIImage   *m_fanartImage {nullptr};
    MythUIImage   *m_boxImage    {nullptr};

    MythUIButton  *m_playButton  {nullptr};
    MythUIButton  *m_doneButton  {nullptr};
};

#endif

