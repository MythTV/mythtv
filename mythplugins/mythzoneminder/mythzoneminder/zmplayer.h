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

#ifndef ZMPLAYER_H
#define ZMPLAYER_H

#include <vector>

// qt
#include <QKeyEvent>

// myth
#include <mythscreentype.h>
#include <mythuibutton.h>
#include <mythuiimage.h>
#include <mythuitext.h>

// zm
#include "zmdefines.h"

class ZMPlayer : public MythScreenType
{
    Q_OBJECT

  public:
    ZMPlayer(MythScreenStack *parent, const char *name,
             std::vector<Event *> *eventList, int *currentEvent);
    ~ZMPlayer();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);

  private slots:
    void updateFrame(void);
    void playPressed(void);
    void deletePressed(void);
    void prevPressed(void);
    void nextPressed(void);

  private:
    void getEventInfo(void);
    void getFrame(void);

    void stopPlayer(void);

    MythUIImage      *m_activeFrameImage;
    MythUIImage      *m_frameImageFS;
    MythUIImage      *m_frameImage;

    MythUIText       *m_noEventsText;
    MythUIText       *m_eventText;
    MythUIText       *m_cameraText;
    MythUIText       *m_frameText;
    MythUIText       *m_dateText;

    MythUIButton     *m_playButton;
    MythUIButton     *m_deleteButton;
    MythUIButton     *m_nextButton;
    MythUIButton     *m_prevButton;

    int              *m_currentEvent;
    std::vector<Event *>  *m_eventList;

    std::vector<Frame *>  *m_frameList;
    QTimer           *m_frameTimer;
    int               m_curFrame;
    int               m_lastFrame;

    bool              m_paused;
    bool              m_fullScreen;

    MythImage        *m_image;
};

#endif

