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

#include "zmdefines.h"
//Added by qt3to4:
#include <QKeyEvent>

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
    void getDaemonStatus();
    void getMonitorStatus(void);

  private:
    void wireUpTheme(void);
    UITextType* getTextType(QString name);
    void keyPressEvent(QKeyEvent *e);
    void updateMonitorList();
    void monitorListDown(bool page);
    void monitorListUp(bool page);

    int                m_currentMonitor;
    int                m_monitorListSize;
    vector<Monitor *> *m_monitorList;
    UIListType        *m_monitor_list;

    UITextType        *m_status_text;
    UITextType        *m_time_text;
    UITextType        *m_date_text;
    UITextType        *m_load_text;
    UITextType        *m_disk_text;

    fontProp          *m_runningFont;
    fontProp          *m_stoppedFont;

    QTimer            *m_timeTimer;
    QString            m_timeFormat;

    QString            m_daemonStatus;
    QString            m_cpuStat;
    QString            m_diskStat;

    QTimer            *m_updateTimer;
};

#endif
