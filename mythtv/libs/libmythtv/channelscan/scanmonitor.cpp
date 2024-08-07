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

const QEvent::Type ScannerEvent::kScanComplete =
    (QEvent::Type) QEvent::registerEventType();
const QEvent::Type ScannerEvent::kScanShutdown =
    (QEvent::Type) QEvent::registerEventType();
const QEvent::Type ScannerEvent::kScanErrored =
    (QEvent::Type) QEvent::registerEventType();
const QEvent::Type ScannerEvent::kAppendTextToLog =
    (QEvent::Type) QEvent::registerEventType();
const QEvent::Type ScannerEvent::kSetStatusText =
    (QEvent::Type) QEvent::registerEventType();
const QEvent::Type ScannerEvent::kSetStatusTitleText =
    (QEvent::Type) QEvent::registerEventType();
const QEvent::Type ScannerEvent::kSetPercentComplete =
    (QEvent::Type) QEvent::registerEventType();
const QEvent::Type ScannerEvent::kSetStatusRotorPosition =
    (QEvent::Type) QEvent::registerEventType();
const QEvent::Type ScannerEvent::kSetStatusSignalToNoise =
    (QEvent::Type) QEvent::registerEventType();
const QEvent::Type ScannerEvent::kSetStatusSignalStrength =
    (QEvent::Type) QEvent::registerEventType();
const QEvent::Type ScannerEvent::kSetStatusSignalLock =
    (QEvent::Type) QEvent::registerEventType();
const QEvent::Type ScannerEvent::kSetStatusChannelTuned =
    (QEvent::Type) QEvent::registerEventType();

/// Percentage to set to after the transports have been scanned
static constexpr int8_t TRANSPORT_PCT  { 6 };
/// Percentage to set to after the first tune
//static constexpr int8_t TUNED_PCT    { 3 };

void post_event(QObject *dest, QEvent::Type type, int val)
{
    auto *e = new ScannerEvent(type);
    e->intValue(val);
    QCoreApplication::postEvent(dest, e);
}

void post_event(QObject *dest, QEvent::Type type, const QString &val)
{
    auto *e = new ScannerEvent(type);
    e->strValue(val);
    QCoreApplication::postEvent(dest, e);
}

void post_event(QObject *dest, QEvent::Type type, int val,
                Configurable *spp)
{
    auto *e = new ScannerEvent(type);
    e->intValue(val);
    e->ConfigurableValue(spp);
    QCoreApplication::postEvent(dest, e);
}

void ScanMonitor::deleteLater(void)
{
    m_channelScanner = nullptr;

    QObject::deleteLater();
}

void ScanMonitor::ScanComplete(void)
{
    post_event(this, ScannerEvent::kScanComplete, 0);
}

void ScanMonitor::ScanPercentComplete(int pct)
{
    int tmp = TRANSPORT_PCT + (((100 - TRANSPORT_PCT) * pct)/100);
    post_event(this, ScannerEvent::kSetPercentComplete, tmp);
}

void ScanMonitor::ScanAppendTextToLog(const QString &status)
{
    post_event(this, ScannerEvent::kAppendTextToLog, status);
}

void ScanMonitor::ScanUpdateStatusText(const QString &status)
{
    QString msg = tr("Scanning");
    if (!status.isEmpty())
        msg = QString("%1 %2").arg(msg, status);

    post_event(this, ScannerEvent::kSetStatusText, msg);
}

void ScanMonitor::ScanUpdateStatusTitleText(const QString &status)
{
    post_event(this, ScannerEvent::kSetStatusTitleText, status);
}

void ScanMonitor::ScanErrored(const QString &error)
{
    post_event(this, ScannerEvent::kScanErrored, error);
}

void ScanMonitor::StatusRotorPosition(const SignalMonitorValue &val)
{
    post_event(this, ScannerEvent::kSetStatusRotorPosition,
               val.GetNormalizedValue(0, 65535));
}

void ScanMonitor::StatusSignalLock(const SignalMonitorValue &val)
{
    post_event(this, ScannerEvent::kSetStatusSignalLock, val.GetValue());
}

void ScanMonitor::StatusChannelTuned(const SignalMonitorValue &val)
{
    post_event(this, ScannerEvent::kSetStatusChannelTuned, val.GetValue());
}

void ScanMonitor::StatusSignalToNoise(const SignalMonitorValue &val)
{
    post_event(this, ScannerEvent::kSetStatusSignalToNoise,
               val.GetNormalizedValue(0, 65535));
}

void ScanMonitor::StatusSignalStrength(const SignalMonitorValue &val)
{
    post_event(this, ScannerEvent::kSetStatusSignalStrength,
               val.GetNormalizedValue(0, 65535));
}

void ScanMonitor::customEvent(QEvent *e)
{
    if (m_channelScanner)
    {
        auto *scanEvent = dynamic_cast<ScannerEvent*>(e);
        if (scanEvent != nullptr)
            m_channelScanner->HandleEvent(scanEvent);
    }
}
