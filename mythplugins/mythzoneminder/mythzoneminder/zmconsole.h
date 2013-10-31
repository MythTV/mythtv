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

// qt
#include <QKeyEvent>

// libmythui
#include <mythuibuttonlist.h>
#include <mythuibutton.h>
#include <mythuicheckbox.h>
#include <mythscreentype.h>
#include <mythdialogbox.h>

// zm
#include "zmdefines.h"

class FunctionDialog : public MythScreenType
{
    Q_OBJECT

  public:
    FunctionDialog(MythScreenStack *parent, Monitor *monitor);

    bool Create();

  signals:
     void haveResult(bool);

  private slots:
    void setMonitorFunction(void);

  private:
    Monitor          *m_monitor;
    MythUIText       *m_captionText;
    MythUIButtonList *m_functionList;
    MythUICheckBox   *m_enabledCheck;
    MythUIButton     *m_okButton;
};

class ZMConsole : public MythScreenType
{
    Q_OBJECT

  public:
    ZMConsole(MythScreenStack *parent);
    ~ZMConsole();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);

  private slots:
    void updateTime();
    void updateStatus();
    void getDaemonStatus();
    void getMonitorStatus(void);
    void showEditFunctionPopup();
    void functionChanged(bool changed);

  private:
    void updateMonitorList();
    void setMonitorFunction(const QString &function, int enabled);

    std::vector<Monitor *> *m_monitorList;

    MythUIButtonList  *m_monitor_list;
    MythUIText        *m_running_text;
    MythUIText        *m_status_text;
    MythUIText        *m_time_text;
    MythUIText        *m_date_text;
    MythUIText        *m_load_text;
    MythUIText        *m_disk_text;

    FunctionDialog    *m_functionDialog;
    MythScreenStack   *m_popupStack;

    QTimer            *m_timeTimer;
    QString            m_timeFormat;

    QString            m_daemonStatus;
    QString            m_cpuStat;
    QString            m_diskStat;

    QTimer            *m_updateTimer;
};

#endif
