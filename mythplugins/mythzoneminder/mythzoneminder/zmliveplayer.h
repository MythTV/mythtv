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


#include <mythtv/uitypes.h>
#include <mythtv/uilistbtntype.h>
#include <mythtv/xmlparse.h>
#include <mythtv/mythdialogs.h>

//gl stuff
#include <GL/glx.h>
#include <GL/glu.h>

typedef enum 
{ 
    IDLE,
    PREALARM,
    ALARM,
    ALERT,
    TAPE
} State;

typedef struct
{
    int size;
    bool valid;
    bool active;
    bool signal;
    State state;
    int last_write_index;
    int last_read_index;
    time_t last_image_time;
    int last_event;
    int action;
    int brightness;
    int hue;
    int colour;
    int contrast;
    int alarm_x;
    int alarm_y;
    char control_state[256];
} SharedData; 

typedef enum { TRIGGER_CANCEL, TRIGGER_ON, TRIGGER_OFF } TriggerState;

typedef struct
{
    int size;
    TriggerState trigger_state;
    int trigger_score;
    char trigger_cause[32];
    char trigger_text[256];
    char trigger_showtext[32];
} TriggerData;

typedef struct
{
    QString name;
    int image_buffer_count;
    QRect displayRect;
    int width;
    int height;
    int mon_id;
    SharedData *shared_data;
    unsigned char *shared_images;
    int last_read;
    QString status;
} MONITOR;

class Player
{
  public:
    Player(void);
    ~Player(void);

    bool startPlayer(MONITOR *mon, Window winID);
    void stopPlaying(void);
    void updateScreen(void);
    void setMonitor(int monID, Window winID);
    MONITOR *getMonitor(void) { return &m_monitor; }

  private:
    int  getMonitorData(int id, MONITOR &monitor);
    void getMonitorList(void);
    void loadZMConfig(void);

    MONITOR     m_monitor;
    bool        m_initalized;
    GLXContext  m_cx;
    Display    *m_dis;
    Window      m_win;
};

class ZMLivePlayer : public MythThemedDialog
{
    Q_OBJECT

public:

    ZMLivePlayer(int monitorID, int eventID, MythMainWindow *parent,
             const QString &window_name, const QString &theme_filename,
             const char *name = 0);
    ~ZMLivePlayer();

    void setMonitorLayout(int layout);

  private slots:
    void updateFrame(void);
    void updateMonitorStatus(void);
    void initMonitorLayout(void);

  private:
    void wireUpTheme(void);
    UITextType* getTextType(QString name);
    void keyPressEvent(QKeyEvent *e);
    void getMonitorList(void);
    void stopPlayers(void);
    void changePlayerMonitor(int playerNo);

    QTimer               *m_frameTimer;
    QTimer               *m_statusTimer;
    bool                  m_paused;
    int                   m_eventID;
    int                   m_monitorID;
    int                   m_monitorLayout;
    int                   m_monitorCount;

    vector<Player *>     *m_players;
    vector<MONITOR *>    *m_monitors;

    fontProp             *m_idleFont;
    fontProp             *m_alarmFont;
    fontProp             *m_alertFont;
};

#endif
