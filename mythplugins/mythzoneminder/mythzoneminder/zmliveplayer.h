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
using namespace std;

// qt
//#include <QKeyEvent>

// mythtv
#include <mythscreentype.h>
#include <mythuiimage.h>
#include <mythuitext.h>

// mythzoneminder
#include "zmdefines.h"

class Player
{
  public:
    Player(void);
    ~Player(void);

    void updateFrame(const uchar* buffer);
    void updateStatus(void);
    void updateCamera();

    void setMonitor(Monitor *mon);
    void setWidgets(MythUIImage *image, MythUIText *status,
                    MythUIText  *camera);

    Monitor *getMonitor(void) { return &m_monitor; }

  private:
    void getMonitorList(void);

    MythUIImage *m_frameImage;
    MythUIText  *m_statusText;
    MythUIText  *m_cameraText;

    MythImage   *m_image;

    uchar       *m_rgba;

    Monitor      m_monitor;
};

class ZMLivePlayer : public MythScreenType
{
    Q_OBJECT

  public:
    ZMLivePlayer(MythScreenStack *parent);
    ~ZMLivePlayer();

    bool Create(void);

    bool keyPressEvent(QKeyEvent *);

    void setMonitorLayout(int layout, bool restore = false);

  private slots:
    void updateFrame(void);
    bool initMonitorLayout(void);
    void getMonitorList(void);

  private:
    MythUIType* GetMythUIType(const QString &name, bool optional = false);
    bool hideAll();
    void stopPlayers(void);
    void changePlayerMonitor(int playerNo);

    QTimer               *m_frameTimer;
    bool                  m_paused;
    int                   m_monitorLayout;
    int                   m_monitorCount;

    vector<Player *>     *m_players;
    vector<Monitor *>    *m_monitors;
};

#endif
