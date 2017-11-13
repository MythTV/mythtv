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
#include "mythdialogbox.h"
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

ChannelScannerGUI::ChannelScannerGUI(void)
    : m_scanStage(NULL)
{
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
        if (m_scanStage)
            m_scanStage->SetScanProgress(1.0);

        InformUser(tr("Scan complete"));

        // HACK: make channel insertion work after [21644]
        post_event(scanMonitor, ScannerEvent::ScanShutdown,
                   kDialogCodeAccepted);
    }
    else if (scanEvent->type() == ScannerEvent::ScanShutdown ||
             scanEvent->type() == ScannerEvent::ScanErrored)
    {
        ScanDTVTransportList transports;
        if (sigmonScanner)
        {
            sigmonScanner->StopScanner();
            transports = sigmonScanner->GetChannelList();
        }

#ifdef USING_VBOX
        bool success = (iptvScanner != NULL || vboxScanner != NULL);
#else
        bool success = iptvScanner != NULL;
#endif

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
                Process(transports, success);
            }
        }
    }
    else if (scanEvent->type() ==  ScannerEvent::AppendTextToLog)
    {
        if (m_scanStage)
            m_scanStage->AppendLine(scanEvent->strValue());
        m_messageList += scanEvent->strValue();
    }

    if (!m_scanStage)
        return;

    if (scanEvent->type() == ScannerEvent::SetStatusText)
        m_scanStage->SetStatusText(scanEvent->strValue());
    else if (scanEvent->type() == ScannerEvent::SetStatusTitleText)
        m_scanStage->SetStatusTitleText(scanEvent->strValue());
    else if (scanEvent->type() == ScannerEvent::SetPercentComplete)
        m_scanStage->SetScanProgress(scanEvent->intValue() * 0.01);
    else if (scanEvent->type() == ScannerEvent::SetStatusRotorPosition)
        m_scanStage->SetStatusRotorPosition(scanEvent->intValue());
    else if (scanEvent->type() == ScannerEvent::SetStatusSignalLock)
        m_scanStage->SetStatusLock(scanEvent->intValue());
    else if (scanEvent->type() == ScannerEvent::SetStatusSignalToNoise)
        m_scanStage->SetStatusSignalToNoise(scanEvent->intValue());
    else if (scanEvent->type() == ScannerEvent::SetStatusSignalStrength)
        m_scanStage->SetStatusSignalStrength(scanEvent->intValue());
}

void ChannelScannerGUI::Process(const ScanDTVTransportList &_transports, bool success)
{
    ChannelImporter ci(true, true, true, true, true,
                       freeToAirOnly, serviceRequirements, success);
    ci.Process(_transports);
}

void ChannelScannerGUI::InformUser(const QString &error)
{
    ShowOkPopup(error);
}

void ChannelScannerGUI::quitScanning(void)
{
    m_scanStage = NULL;

    if (scanMonitor)
    {
        post_event(scanMonitor, ScannerEvent::ScanShutdown,
                   MythDialog::Rejected);
    }
}

void ChannelScannerGUI::MonitorProgress(bool lock, bool strength,
                                        bool snr,  bool rotor)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    m_scanStage = new ChannelScannerGUIScanPane(lock, strength, snr, rotor, mainStack);

    if (m_scanStage->Create())
    {
        connect(m_scanStage, SIGNAL(Exiting()), SLOT(quitScanning()));

        for (uint i = 0; i < (uint) m_messageList.size(); i++)
            m_scanStage->AppendLine(m_messageList[i]);
        mainStack->AddScreen(m_scanStage);
    }
    else
    {
        delete m_scanStage;
        m_scanStage = NULL;
    }
}
