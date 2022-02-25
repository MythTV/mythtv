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

// MythTV
#include <libmythui/mythdialogbox.h>
#include <libmythui/mythscreentype.h>
#include <libmythui/mythuibutton.h>
#include <libmythui/mythuibuttonlist.h>
#include <libmythui/mythuicheckbox.h>

// zm
#include "zmdefines.h"

class FunctionDialog : public MythScreenType
{
    Q_OBJECT

  public:
    FunctionDialog(MythScreenStack *parent, Monitor *monitor)
        : MythScreenType(parent, "functionpopup"), m_monitor(monitor) {}

    bool Create() override; // MythScreenType

  signals:
     void haveResult(bool);

  private slots:
    void setMonitorFunction(void);

  private:
    Monitor          *m_monitor           {nullptr};
    MythUIText       *m_captionText       {nullptr};
    MythUIButtonList *m_functionList      {nullptr};
    MythUICheckBox   *m_enabledCheck      {nullptr};
    MythUICheckBox   *m_notificationCheck {nullptr};
    MythUIButton     *m_okButton          {nullptr};
};

class ZMConsole : public MythScreenType
{
    Q_OBJECT

  public:
    explicit ZMConsole(MythScreenStack *parent);
    ~ZMConsole() override;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType

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

    MythUIButtonList  *m_monitorList    {nullptr};
    MythUIText        *m_runningText    {nullptr};
    MythUIText        *m_statusText     {nullptr};
    MythUIText        *m_timeText       {nullptr};
    MythUIText        *m_dateText       {nullptr};
    MythUIText        *m_loadText       {nullptr};
    MythUIText        *m_diskText       {nullptr};

    FunctionDialog    *m_functionDialog {nullptr};
    MythScreenStack   *m_popupStack     {nullptr};

    QTimer            *m_timeTimer      {nullptr};
    QString            m_timeFormat     {"h:mm AP"};

    QString            m_daemonStatus;
    QString            m_cpuStat;
    QString            m_diskStat;

    QTimer            *m_updateTimer    {nullptr};
};

#endif
