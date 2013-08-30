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

// MythTV headers
#include "scanmonitor.h"
#include "signalmonitorvalue.h"
#include "channelscanner.h"

// Qt headers
#include <QCoreApplication>

QEvent::Type ScannerEvent::ScanComplete =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type ScannerEvent::ScanShutdown =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type ScannerEvent::ScanErrored =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type ScannerEvent::AppendTextToLog =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type ScannerEvent::SetStatusText =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type ScannerEvent::SetStatusTitleText =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type ScannerEvent::SetPercentComplete =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type ScannerEvent::SetStatusRotorPosition =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type ScannerEvent::SetStatusSignalToNoise =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type ScannerEvent::SetStatusSignalStrength =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type ScannerEvent::SetStatusSignalLock =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type ScannerEvent::SetStatusChannelTuned =
    (QEvent::Type) QEvent::registerEventType();

/// Percentage to set to after the transports have been scanned
#define TRANSPORT_PCT 6
/// Percentage to set to after the first tune
#define TUNED_PCT     3

void post_event(QObject *dest, QEvent::Type type, int val)
{
    ScannerEvent *e = new ScannerEvent(type);
    e->intValue(val);
    QCoreApplication::postEvent(dest, e);
}

void post_event(QObject *dest, QEvent::Type type, const QString &val)
{
    ScannerEvent *e = new ScannerEvent(type);
    QString tmp = val; tmp.detach();
    e->strValue(tmp);
    QCoreApplication::postEvent(dest, e);
}

void post_event(QObject *dest, QEvent::Type type, int val,
                Configurable *spp)
{
    ScannerEvent *e = new ScannerEvent(type);
    e->intValue(val);
    e->ConfigurableValue(spp);
    QCoreApplication::postEvent(dest, e);
}

void ScanMonitor::deleteLater(void)
{
    channelScanner = NULL;

    QObject::deleteLater();
}

void ScanMonitor::ScanComplete(void)
{
    post_event(this, ScannerEvent::ScanComplete, 0);
}

void ScanMonitor::ScanPercentComplete(int pct)
{
    int tmp = TRANSPORT_PCT + ((100 - TRANSPORT_PCT) * pct)/100;
    post_event(this, ScannerEvent::SetPercentComplete, tmp);
}

void ScanMonitor::ScanAppendTextToLog(const QString &str)
{
    post_event(this, ScannerEvent::AppendTextToLog, str);
}

void ScanMonitor::ScanUpdateStatusText(const QString &str)
{
    QString msg = tr("Scanning");
    if (!str.isEmpty())
        msg = QString("%1 %2").arg(msg).arg(str);

    post_event(this, ScannerEvent::SetStatusText, msg);
}

void ScanMonitor::ScanUpdateStatusTitleText(const QString &str)
{
    post_event(this, ScannerEvent::SetStatusTitleText, str);
}

void ScanMonitor::ScanErrored(const QString &error)
{
    post_event(this, ScannerEvent::ScanErrored, error);
}

void ScanMonitor::StatusRotorPosition(const SignalMonitorValue &val)
{
    post_event(this, ScannerEvent::SetStatusRotorPosition,
               val.GetNormalizedValue(0, 65535));
}

void ScanMonitor::StatusSignalLock(const SignalMonitorValue &val)
{
    post_event(this, ScannerEvent::SetStatusSignalLock, val.GetValue());
}

void ScanMonitor::StatusChannelTuned(const SignalMonitorValue &val)
{
    post_event(this, ScannerEvent::SetStatusChannelTuned, val.GetValue());
}

void ScanMonitor::StatusSignalToNoise(const SignalMonitorValue &val)
{
    post_event(this, ScannerEvent::SetStatusSignalToNoise,
               val.GetNormalizedValue(0, 65535));
}

void ScanMonitor::StatusSignalStrength(const SignalMonitorValue &val)
{
    post_event(this, ScannerEvent::SetStatusSignalStrength,
               val.GetNormalizedValue(0, 65535));
}

void ScanMonitor::customEvent(QEvent *e)
{
    if (channelScanner)
    {
        ScannerEvent *scanEvent = (ScannerEvent*) e;
        channelScanner->HandleEvent(scanEvent);
    }
}
