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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
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
    ScanMonitor(ChannelScanner *cs) : channelScanner(cs) { }
    virtual void deleteLater(void);

    virtual void customEvent(QEvent*);

    // Values from 1-100 of scan completion
    void ScanPercentComplete(int pct);
    void ScanUpdateStatusText(const QString &status);
    void ScanUpdateStatusTitleText(const QString &status);
    void ScanAppendTextToLog(const QString &status);
    void ScanComplete(void);
    void ScanErrored(const QString &error);

    // SignalMonitorListener
    virtual void AllGood(void) { }
    virtual void StatusSignalLock(const SignalMonitorValue&);
    virtual void StatusChannelTuned(const SignalMonitorValue&);
    virtual void StatusSignalStrength(const SignalMonitorValue&);

    // DVBSignalMonitorListener
    virtual void StatusSignalToNoise(const SignalMonitorValue&);
    virtual void StatusBitErrorRate(const SignalMonitorValue&) { }
    virtual void StatusUncorrectedBlocks(const SignalMonitorValue&) { }
    virtual void StatusRotorPosition(const SignalMonitorValue&);

  private:
    ~ScanMonitor() { }

    ChannelScanner *channelScanner;
};

class Configurable;

class ScannerEvent : public QEvent
{
    friend class QObject; // quiet OSX gcc warning

  public:

    ScannerEvent(QEvent::Type t) :
        QEvent(t), str(""), intvalue(0), cfg_ptr(NULL) { ; }

    QString strValue()              const { return str; }
    void    strValue(const QString& _str) { str = _str; }

    int     intValue()        const { return intvalue; }
    void    intValue(int _intvalue) { intvalue = _intvalue; }

    Configurable *ConfigurableValue() const { return cfg_ptr; }
    void    ConfigurableValue(Configurable *_cfg_ptr)
        { cfg_ptr = _cfg_ptr; }

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
    ~ScannerEvent() { }

  private:
    QString str;
    int     intvalue;
    Configurable *cfg_ptr;
};

void post_event(QObject *dest, QEvent::Type type, int val);
void post_event(QObject *dest, QEvent::Type type, const QString &val);
void post_event(QObject *dest, QEvent::Type type, int val,
                Configurable *cfg);

#endif // _SCAN_MONITOR_H_
