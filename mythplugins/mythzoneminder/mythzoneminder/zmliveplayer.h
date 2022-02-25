/* ============================================================
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published bythe Free Software Foundation;
 * either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * ============================================================ */

#ifndef ZMLIVEPLAYER_H
#define ZMLIVEPLAYER_H

// c++
#include <vector>

// qt
//#include <QKeyEvent>
#include <QObject>

// mythtv
#include <libmythui/mythdialogbox.h>
#include <libmythui/mythscreentype.h>
#include <libmythui/mythuiimage.h>
#include <libmythui/mythuitext.h>

// mythzoneminder
#include "zmdefines.h"

class Player
{
  public:
    Player(void) = default;
    ~Player(void);

    void updateFrame(const uchar* buffer);
    void updateStatus(void);
    void updateCamera();

    void setMonitor(const Monitor *mon);
    void setWidgets(MythUIImage *image, MythUIText *status,
                    MythUIText  *camera);

    Monitor *getMonitor(void) { return &m_monitor; }

  private:
    MythUIImage *m_frameImage {nullptr};
    MythUIText  *m_statusText {nullptr};
    MythUIText  *m_cameraText {nullptr};

    uchar       *m_rgba       {nullptr};

    Monitor      m_monitor;
};

class ZMLivePlayer : public MythScreenType
{
    Q_OBJECT

  public:
    explicit ZMLivePlayer(MythScreenStack *parent, bool isMiniPlayer = false);
    ~ZMLivePlayer() override;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType
    void customEvent(QEvent *event) override; // MythUIType

    void ShowMenu() override; // MythScreenType

    void setMonitorLayout(int layout, bool restore = false);

  protected slots:
    void updateFrame(void);
    bool initMonitorLayout(int layout);

  protected:
    MythUIType* GetMythUIType(const QString &name, bool optional = false);
    bool hideAll();
    void stopPlayers(void);
    void changePlayerMonitor(int playerNo);
    void changeView(void);

    QTimer               *m_frameTimer    {nullptr};
    bool                  m_paused        {false};
    int                   m_monitorLayout {1};
    int                   m_monitorCount  {0};

    std::vector<Player *> *m_players      {nullptr};

    bool                  m_isMiniPlayer  {false};
    int                   m_alarmMonitor  {-1};
};

#endif
