#ifndef GAMEDETAILS_H_
#define GAMEDETAILS_H_

#include <QString>

#include "mythscreentype.h"
#include "rominfo.h"

class GameDetailsPopup : public MythScreenType
{
    Q_OBJECT

  public:
    GameDetailsPopup(MythScreenStack *parent, const RomInfo *romInfo);
    ~GameDetailsPopup();

    bool Create(void);
    void SetReturnEvent(QObject *retobject, const QString &resultid);

  private slots:
    void Play(void);

  private:
    void        handleText(const QString &name, const QString &value);
    void        handleImage(const QString &name, const QString &filename);

    const RomInfo *m_romInfo;
    QString        m_id;
    QObject       *m_retObject;

    MythUIText  *m_gameName;
    MythUIText  *m_gameType;
    MythUIText  *m_romName;
    MythUIText  *m_crc;
    MythUIText  *m_romPath;
    MythUIText  *m_genre;
    MythUIText  *m_year;
    MythUIText  *m_country;
    MythUIText  *m_plot;
    MythUIText  *m_publisher;
    MythUIText  *m_allSystems;
    MythUIImage *m_fanartImage;
    MythUIImage *m_boxImage;

    MythUIButton *m_playButton;
    MythUIButton *m_doneButton;
};

#endif

