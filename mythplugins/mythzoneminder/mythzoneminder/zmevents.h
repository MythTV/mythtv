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
#include <QKeyEvent>
#include <QPixmap>

// MythTV
#include <libmythui/mythdialogbox.h>
#include <libmythui/mythscreentype.h>
#include <libmythui/mythuibuttonlist.h>

// zm
#include <zmdefines.h>

class ZMEvents : public MythScreenType
{
    Q_OBJECT

public:
    explicit ZMEvents(MythScreenStack *parent)
        : MythScreenType(parent, "zmevents"),
          m_eventList(new std::vector<Event*>) {}
    ~ZMEvents() override;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType

  private slots:
    void getEventList(void);
    void playPressed(void);
    void deletePressed(void);
    void deleteAll(void);
    void doDeleteAll(bool doDelete);
    void changeView(void);
    void toggleShowContinuous(void);
    void eventChanged(MythUIButtonListItem *item);
    static void eventVisible(MythUIButtonListItem *item);
    void cameraChanged(void);
    void dateChanged(void);
    void playerExited(void);

  private:
    void updateUIList();
    void getCameraList(void);
    void getDateList(void);
    void setGridLayout(int layout);
    void ShowMenu(void) override; // MythScreenType

    bool                 m_oldestFirst    {false};
    bool                 m_showContinuous {false};
    int                  m_layout         {-1};

    std::vector<Event *>     *m_eventList {nullptr};
    QStringList          m_dateList;
    size_t               m_savedPosition  {0};
    int                  m_currentCamera  {-1};
    int                  m_currentDate    {-1};

    MythUIText          *m_eventNoText    {nullptr};

    MythUIButtonList    *m_eventGrid      {nullptr};

    MythUIButton        *m_playButton     {nullptr};
    MythUIButton        *m_deleteButton   {nullptr};

    MythUIButtonList    *m_cameraSelector {nullptr};
    MythUIButtonList    *m_dateSelector   {nullptr};

    MythDialogBox       *m_menuPopup      {nullptr};
};

#endif
