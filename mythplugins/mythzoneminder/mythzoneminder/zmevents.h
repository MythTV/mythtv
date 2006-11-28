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


#include <mythtv/uitypes.h>
#include <mythtv/uilistbtntype.h>
#include <mythtv/xmlparse.h>
#include <mythtv/mythdialogs.h>


typedef struct
{
    int monitorID;
    int eventID;
    QString eventName;
    QString monitorName;
    QString startTime;
    QString length;
} Event;

class ZMEvents : public MythThemedDialog
{
    Q_OBJECT

public:
    ZMEvents(MythMainWindow *parent,
             const QString &window_name, const QString &theme_filename,
             const char *name = 0);
    ~ZMEvents();

  private slots:
    void getEventList();
    void playPressed(void);
    void deletePressed(void);
    void setCamera(int item);
    void updateTimeout(void);

  private:
    void wireUpTheme(void);
    UITextType* getTextType(QString name);
    void keyPressEvent(QKeyEvent *e);

    void updateUIList();
    void eventListDown(bool page);
    void eventListUp(bool page);
    void getCameraList(void);

    int                  m_currentEvent;
    int                  m_eventListSize;
    vector<Event *>     *m_eventList;
    UIListType          *m_event_list;
    UITextType          *m_eventNoText;

    UITextButtonType    *m_playButton;
    UITextButtonType    *m_deleteButton;

    UISelectorType      *m_cameraSelector;

    QTimer              *m_updateTimer;
};

#endif
