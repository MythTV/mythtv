/* -*- Mode: c++ -*-
 * vim: set expandtab tabstop=4 shiftwidth=4:
 *
 * Original Project
 *      MythTV      http://www.mythtv.org
 *
 * Copyright (c) 2004, 2005 John Pullan <john@pullan.org>
 * Copyright (c) 2005 - 2007 Daniel Kristjansson
 *
 * Description:
 *     Collection of classes to provide channel scanning functionallity
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 *
 */

#ifndef _SCAN_MONITOR_H_
#define _SCAN_MONITOR_H_

// Qt headers
#include <QObject>
#include <QEvent>

// MythTV headers
#include "signalmonitorlistener.h"

class ChannelScanner;
class SignalMonitorValue;
class QString;

class ScanMonitor :
    public QObject,
    public DVBSignalMonitorListener
{
    Q_OBJECT

    friend class QObject; // quiet OSX gcc warning

  public:
    explicit ScanMonitor(ChannelScanner *cs) : m_channelScanner(cs) { }
    virtual void deleteLater(void);

    void customEvent(QEvent*) override; // QObject

    // Values from 1-100 of scan completion
    void ScanPercentComplete(int pct);
    void ScanUpdateStatusText(const QString &status);
    void ScanUpdateStatusTitleText(const QString &status);
    void ScanAppendTextToLog(const QString &status);
    void ScanComplete(void);
    void ScanErrored(const QString &error);

    // SignalMonitorListener
    void AllGood(void) override { } // SignalMonitorListener
    void StatusSignalLock(const SignalMonitorValue&) override; // SignalMonitorListener
    void StatusChannelTuned(const SignalMonitorValue&) override; // SignalMonitorListener
    void StatusSignalStrength(const SignalMonitorValue&) override; // SignalMonitorListener

    // DVBSignalMonitorListener
    void StatusSignalToNoise(const SignalMonitorValue&) override; // DVBSignalMonitorListener
    void StatusBitErrorRate(const SignalMonitorValue&) override { } // DVBSignalMonitorListener
    void StatusUncorrectedBlocks(const SignalMonitorValue&) override { } // DVBSignalMonitorListener
    void StatusRotorPosition(const SignalMonitorValue&) override; // DVBSignalMonitorListener

  private:
    ~ScanMonitor() = default;

    ChannelScanner *m_channelScanner {nullptr};
};

class Configurable;

class ScannerEvent : public QEvent
{
    friend class QObject; // quiet OSX gcc warning

  public:

    explicit ScannerEvent(QEvent::Type type) : QEvent(type) { }

    QString strValue()              const { return m_str; }
    void    strValue(const QString& str)  { m_str = str; }

    int     intValue()       const { return m_intvalue; }
    void    intValue(int intvalue) { m_intvalue = intvalue; }

    int     boolValue()       const { return m_intvalue != 0; }

    Configurable *ConfigurableValue() const { return m_cfg_ptr; }
    void    ConfigurableValue(Configurable *cfg_ptr)
        { m_cfg_ptr = cfg_ptr; }

    static Type ScanComplete;
    static Type ScanShutdown;
    static Type ScanErrored;
    static Type AppendTextToLog;
    static Type SetStatusText;
    static Type SetStatusTitleText;
    static Type SetPercentComplete;
    static Type SetStatusRotorPosition;
    static Type SetStatusSignalToNoise;
    static Type SetStatusSignalStrength;
    static Type SetStatusSignalLock;
    static Type SetStatusChannelTuned;

  private:
    ~ScannerEvent() = default;

  private:
    QString       m_str;
    int           m_intvalue {0};
    Configurable *m_cfg_ptr  {nullptr};
};

void post_event(QObject *dest, QEvent::Type type, int val);
void post_event(QObject *dest, QEvent::Type type, const QString &val);
void post_event(QObject *dest, QEvent::Type type, int val,
                Configurable *spp);

#endif // _SCAN_MONITOR_H_
