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

#ifndef ZMCONSOLE_H
#define ZMCONSOLE_H


#include <mythtv/uitypes.h>
#include <mythtv/uilistbtntype.h>
#include <mythtv/xmlparse.h>
#include <mythtv/mythdialogs.h>

class QProcess;

typedef struct
{
    QString name;
    QString zmcStatus;
    QString zmaStatus;
    int events;
} Monitor;


class ZMConsole : public MythThemedDialog
{
    Q_OBJECT

public:

    ZMConsole(MythMainWindow *parent,
              const QString &window_name, const QString &theme_filename,
              const char *name = 0);
    ~ZMConsole();

  private slots:
    void updateTime();
    void updateStatus();
    void readFromStdout();
    void processExited();
    void getDaemonStatus();
    void getMonitorList();

  private:
    void wireUpTheme(void);
    UITextType* getTextType(QString name);
    void keyPressEvent(QKeyEvent *e);
    void updateMonitorList();
    void monitorListDown(bool page);
    void monitorListUp(bool page);
    void runCommand(QString command);
    void getMonitorStatus(int id, QString type, QString device, QString channel,
                          QString function, QString &zmcStatus, QString &zmaStatus);
    void getStats(void);

    int                  m_currentMonitor;
    int                  m_monitorListSize;
    vector<Monitor *>   *m_monitorList;
    UIListType          *m_monitor_list;

    UITextType          *m_status_text;
    UITextType          *m_time_text;
    UITextType          *m_date_text;
    UITextType          *m_load_text;
    UITextType          *m_disk_text;

    fontProp             *m_runningFont;
    fontProp             *m_stoppedFont;

    QTimer              *m_timeTimer;
    QString              m_timeFormat;

    QProcess            *m_process;
    QString              m_status;
    bool                 m_processRunning;

    QString              m_daemonStatus;

    QTimer              *m_updateTimer;
};

#endif
