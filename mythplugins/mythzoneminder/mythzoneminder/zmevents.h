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

#ifndef ZMEVENTS_H
#define ZMEVENTS_H

// qt
#include <QPixmap>
#include <QKeyEvent>

// mythtv
#include <mythuibuttonlist.h>
#include <mythscreentype.h>
#include <mythdialogbox.h>

// zm
#include <zmdefines.h>

class ZMEvents : public MythScreenType
{
    Q_OBJECT

public:
    ZMEvents(MythScreenStack *parent);
    ~ZMEvents();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);

  private slots:
    void getEventList(void);
    void playPressed(void);
    void deletePressed(void);
    void deleteAll(void);
    void doDeleteAll(bool doDelete);
    void changeView(void);
    void toggleShowContinuous(void);
    void eventChanged(MythUIButtonListItem *item);
    void eventVisible(MythUIButtonListItem *item);
    void cameraChanged(void);
    void dateChanged(void);
    void playerExited(void);

  private:
    void updateUIList();
    void getCameraList(void);
    void getDateList(void);
    void setGridLayout(int layout);
    void showMenu(void);

    bool                 m_oldestFirst;
    bool                 m_showContinuous;
    int                  m_layout;

    std::vector<Event *>     *m_eventList;
    QStringList          m_dateList;
    int                  m_savedPosition;
    int                  m_currentCamera;
    int                  m_currentDate;

    MythUIText          *m_eventNoText;

    MythUIButtonList    *m_eventGrid;

    MythUIButton        *m_playButton;
    MythUIButton        *m_deleteButton;

    MythUIButtonList    *m_cameraSelector;
    MythUIButtonList    *m_dateSelector;

    MythDialogBox       *m_menuPopup;
};

#endif
