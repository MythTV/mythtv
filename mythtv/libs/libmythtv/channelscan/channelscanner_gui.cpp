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

// System headers
#include <unistd.h>

// Std C++
#include <algorithm>
using namespace std;

// MythTV headers
#include "scanwizard.h"
#include "channelscanner_gui.h"
#include "channelscanner_gui_scan_pane.h"
#include "channelimporter.h"
#include "loglist.h"
#include "channelscan_sm.h"

#include "channelbase.h"
#include "dtvsignalmonitor.h"
#include "dvbsignalmonitor.h"
#include "mpegstreamdata.h"

#define LOC QString("ChScanGUI: ")

QString ChannelScannerGUI::kTitle = QString::null;

// kTitle must be initialized after the Qt translation system is initialized...
static void init_statics(void)
{
    static QMutex lock;
    static bool do_init = true;
    QMutexLocker locker(&lock);
    if (do_init)
    {
        ChannelScannerGUI::kTitle = ChannelScannerGUI::tr("Scanning");
        do_init = false;
    }
}

ChannelScannerGUI::ChannelScannerGUI(void)
    : StackedConfigurationGroup(false, true, false, false),
      scanStage(NULL), doneStage(new LogList())
{
    init_statics();

    setLabel(kTitle);

    addChild(doneStage);
}

ChannelScannerGUI::~ChannelScannerGUI()
{
    Teardown();
    if (scanMonitor)
    {
        post_event(scanMonitor, ScannerEvent::ScanShutdown,
                   MythDialog::Rejected);
    }
}

void ChannelScannerGUI::HandleEvent(const ScannerEvent *scanEvent)
{
    if (scanEvent->type() == ScannerEvent::ScanComplete)
    {
        if (scanStage)
            scanStage->SetScanProgress(1.0);
        raise(doneStage);

        // HACK: make channel insertion work after [21644]
        post_event(scanMonitor, ScannerEvent::ScanShutdown,
                   kDialogCodeAccepted);
    }
    else if (scanEvent->type() == ScannerEvent::ScanShutdown ||
             scanEvent->type() == ScannerEvent::ScanErrored)
    {
        if (scanEvent->ConfigurableValue())
        {
            setLabel(scanEvent->ConfigurableValue()->getLabel());
            raise(scanEvent->ConfigurableValue());
        }

        ScanDTVTransportList transports;
        if (sigmonScanner)
        {
            sigmonScanner->StopScanner();
            transports = sigmonScanner->GetChannelList();
        }

        bool wasIPTV = iptvScanner != NULL;
        Teardown();

        if (scanEvent->type() == ScannerEvent::ScanErrored)
        {
            QString error = scanEvent->strValue();
            InformUser(error);
            return;
        }
        else
        {
            int ret = scanEvent->intValue();
            if (!transports.empty() || (MythDialog::Rejected != ret))
            {
                Process(transports, wasIPTV);
            }
        }
    }
    else if (scanEvent->type() ==  ScannerEvent::AppendTextToLog)
    {
        if (scanStage)
            scanStage->AppendLine(scanEvent->strValue());
        doneStage->AppendLine(scanEvent->strValue());
        messageList += scanEvent->strValue();
    }

    if (!scanStage)
        return;

    if (scanEvent->type() == ScannerEvent::SetStatusText)
        scanStage->SetStatusText(scanEvent->strValue());
    else if (scanEvent->type() == ScannerEvent::SetStatusTitleText)
        scanStage->SetStatusTitleText(scanEvent->strValue());
    else if (scanEvent->type() == ScannerEvent::SetPercentComplete)
        scanStage->SetScanProgress(scanEvent->intValue() * 0.01);
    else if (scanEvent->type() == ScannerEvent::SetStatusRotorPosition)
        scanStage->SetStatusRotorPosition(scanEvent->intValue());
    else if (scanEvent->type() == ScannerEvent::SetStatusSignalLock)
        scanStage->SetStatusLock(scanEvent->intValue());
    else if (scanEvent->type() == ScannerEvent::SetStatusSignalToNoise)
        scanStage->SetStatusSignalToNoise(scanEvent->intValue());
    else if (scanEvent->type() == ScannerEvent::SetStatusSignalStrength)
        scanStage->SetStatusSignalStrength(scanEvent->intValue());
}

void ChannelScannerGUI::Process(const ScanDTVTransportList &_transports, bool success)
{
    ChannelImporter ci(true, true, true, true, true,
                       freeToAirOnly, serviceRequirements, success);
    ci.Process(_transports);
}

void ChannelScannerGUI::InformUser(const QString &error)
{
    MythPopupBox::showOkPopup(GetMythMainWindow(),
                              tr("ScanWizard"), error);
}

void ChannelScannerGUI::quitScanning(void)
{
    if (scanMonitor)
    {
        post_event(scanMonitor, ScannerEvent::ScanShutdown,
                   MythDialog::Rejected, doneStage);
    }
}

void ChannelScannerGUI::MonitorProgress(bool lock, bool strength,
                                        bool snr,  bool rotor)
{
    scanStage = new ChannelScannerGUIScanPane(
        lock, strength, snr, rotor, this,SLOT(quitScanning(void)));
    for (uint i = 0; i < (uint) messageList.size(); i++)
        scanStage->AppendLine(messageList[i]);

    addChild(scanStage);
    raise(scanStage);
}
