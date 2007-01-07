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

// myth
#include <mythtv/uitypes.h>
#include <mythtv/uilistbtntype.h>
#include <mythtv/xmlparse.h>
#include <mythtv/mythdialogs.h>

// gl stuff
#include <GL/glx.h>
#include <GL/glu.h>

// zm
#include "zmdefines.h"

class ZMPlayer : public MythThemedDialog
{
    Q_OBJECT

public:

    ZMPlayer(vector<Event *> *eventList, int currentEvent, MythMainWindow *parent,
             const QString &window_name, const QString &theme_filename,
             const char *name = 0);
    ~ZMPlayer();

  private slots:
    void updateFrame(void);
    void playPressed(void);
    void deletePressed(void);
    void prevPressed(void);
    void nextPressed(void);

  private:
    void wireUpTheme(void);
    UITextType* getTextType(QString name);
    void keyPressEvent(QKeyEvent *e);
    void getEventInfo(void);
    void displayFrame(void);
    void getFrame(void);

    bool initPlayer(void);
    void stopPlayer(void);
    //void updateScreen(void);

    UIImageType          *m_frameImage;
    UIImageType          *m_frameFSImage;

    UITextType           *m_noEventsText;
    UITextType           *m_eventText;
    UITextType           *m_cameraText;
    UITextType           *m_frameText;
    UITextType           *m_dateText;

    UITextButtonType     *m_playButton;
    UITextButtonType     *m_deleteButton;
    UITextButtonType     *m_nextButton;
    UITextButtonType     *m_prevButton;

    int                   m_currentEvent;
    vector<Event *>      *m_eventList;

    vector<Frame *>      *m_frameList;
    QTimer               *m_frameTimer;
    int                   m_curFrame;
    int                   m_lastFrame;

    QString               m_eventDir;
    bool                  m_paused;
    bool                  m_bFullScreen;

    bool                  m_initalized;
    GLXContext            m_cx;
    Display              *m_dis;
    Window                m_win;
    QImage                m_image;
    QRect                 m_displayRect;
};

#endif
