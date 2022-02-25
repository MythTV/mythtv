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

// C++
#include <vector>

// qt
#include <QKeyEvent>

// MythTV
#include <libmythui/mythscreentype.h>
#include <libmythui/mythuibutton.h>
#include <libmythui/mythuiimage.h>
#include <libmythui/mythuitext.h>

// zm
#include "zmdefines.h"

class ZMPlayer : public MythScreenType
{
    Q_OBJECT

  public:
    ZMPlayer(MythScreenStack *parent, const char *name,
             std::vector<Event *> *eventList, size_t *currentEvent);
    ~ZMPlayer() override;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType

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

    MythUIImage      *m_activeFrameImage {nullptr};
    MythUIImage      *m_frameImageFS     {nullptr};
    MythUIImage      *m_frameImage       {nullptr};

    MythUIText       *m_noEventsText     {nullptr};
    MythUIText       *m_eventText        {nullptr};
    MythUIText       *m_cameraText       {nullptr};
    MythUIText       *m_frameText        {nullptr};
    MythUIText       *m_dateText         {nullptr};

    MythUIButton     *m_playButton       {nullptr};
    MythUIButton     *m_deleteButton     {nullptr};
    MythUIButton     *m_nextButton       {nullptr};
    MythUIButton     *m_prevButton       {nullptr};

    size_t           *m_currentEvent     {nullptr};
    std::vector<Event *>  *m_eventList   {nullptr};

    std::vector<Frame *>  *m_frameList   {nullptr};
    QTimer           *m_frameTimer       {nullptr};
    uint              m_curFrame         {0};

    bool              m_paused           {false};
    bool              m_fullScreen       {false};

    MythImage        *m_image            {nullptr};
};

#endif

