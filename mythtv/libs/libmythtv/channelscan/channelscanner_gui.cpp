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

// MythTV headers
#include "mythdialogbox.h"
#include "channelscanner_gui.h"
#include "channelscanner_gui_scan_pane.h"
#include "channelimporter.h"
#include "channelscan_sm.h"

#include "channelbase.h"
#include "dtvsignalmonitor.h"
#include "dvbsignalmonitor.h"
#include "mpegstreamdata.h"

#define LOC QString("ChScanGUI: ")

static const int kCodeRejected  = 0;
static const int kCodeAccepted  = 1;

ChannelScannerGUI::~ChannelScannerGUI()
{
    Teardown();
    if (m_scanMonitor)
    {
        post_event(m_scanMonitor, ScannerEvent::ScanShutdown, kCodeRejected);
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
        post_event(m_scanMonitor, ScannerEvent::ScanShutdown, kCodeAccepted);
    }
    else if (scanEvent->type() == ScannerEvent::ScanShutdown ||
             scanEvent->type() == ScannerEvent::ScanErrored)
    {
        ScanDTVTransportList transports;
        if (m_sigmonScanner)
        {
            m_sigmonScanner->StopScanner();
            transports = m_sigmonScanner->GetChannelList(m_addFullTS);
        }

        bool success = (m_iptvScanner != nullptr);
#ifdef USING_VBOX
        success |= (m_vboxScanner != nullptr);
#endif
#if !defined( USING_MINGW ) && !defined( _MSC_VER )
        success |= (m_externRecScanner != nullptr);
#endif

        Teardown();

        if (scanEvent->type() == ScannerEvent::ScanErrored)
        {
            QString error = scanEvent->strValue();
            InformUser(error);
            return;
        }

        int ret = scanEvent->intValue();
        if (!transports.empty() || (kCodeRejected != ret))
        {
            Process(transports, success);
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

void ChannelScannerGUI::Process(const ScanDTVTransportList &_transports,
                                bool success)
{
    ChannelImporter ci(true, true, true, true, true,
                       m_freeToAirOnly, m_channelNumbersOnly, m_completeOnly,
                       m_fullSearch, m_removeDuplicates, m_serviceRequirements, success);
    ci.Process(_transports, m_sourceid);
}

void ChannelScannerGUI::InformUser(const QString &error)
{
    ShowOkPopup(error);
}

void ChannelScannerGUI::quitScanning(void)
{
    m_scanStage = nullptr;

    if (m_scanMonitor)
    {
        post_event(m_scanMonitor, ScannerEvent::ScanShutdown, kCodeRejected);
    }
}

void ChannelScannerGUI::MonitorProgress(bool lock, bool strength,
                                        bool snr,  bool rotor)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    m_scanStage = new ChannelScannerGUIScanPane(lock, strength, snr, rotor, mainStack);

    if (m_scanStage->Create())
    {
        connect(m_scanStage, &MythScreenType::Exiting, this, &ChannelScannerGUI::quitScanning);

        for (const QString& msg : qAsConst(m_messageList))
            m_scanStage->AppendLine(msg);
        mainStack->AddScreen(m_scanStage);
    }
    else
    {
        delete m_scanStage;
        m_scanStage = nullptr;
    }
}
