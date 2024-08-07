/* -*- Mode: c++ -*-
 * vim: set expandtab tabstop=4 shiftwidth=4:
 *
 * Original Project
 *      MythTV      http://www.mythtv.org
 *
 * Copyright (c) 2008 Daniel Kristjansson
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

// Qt headers
#include <QCoreApplication>
#include <iostream>

// MythTv headers
#include "channelscanner_cli.h"
#include "channelscan_sm.h"
#include "channelimporter.h"

#define LOC      QString("ChScanCLI: ")

void ChannelScannerCLI::HandleEvent(const ScannerEvent *scanEvent)
{
    if ((scanEvent->type() == ScannerEvent::kScanComplete) ||
        (scanEvent->type() == ScannerEvent::kScanShutdown) ||
        (scanEvent->type() == ScannerEvent::kScanErrored))
    {
        std::cout<<std::endl;

        if (scanEvent->type() == ScannerEvent::kScanShutdown)
            std::cerr<<"HandleEvent(void) -- scan shutdown"<<std::endl;
        else
            std::cerr<<"HandleEvent(void) -- scan complete"<<std::endl;

        ScanDTVTransportList transports;
        if (m_sigmonScanner)
        {
            m_sigmonScanner->StopScanner();
            transports = m_sigmonScanner->GetChannelList(m_addFullTS);
        }

        Teardown();

        if (scanEvent->type() == ScannerEvent::kScanErrored)
        {
            QString error = scanEvent->strValue();
            InformUser(error);
        }
        else if (!transports.empty())
        {
            Process(transports);
        }

        m_done = true;
        QCoreApplication::exit(0);
    }
    else if (scanEvent->type() == ScannerEvent::kAppendTextToLog)
    {
        m_statusLastLog = scanEvent->strValue();
    }
    else if (scanEvent->type() == ScannerEvent::kSetStatusText)
    {
        m_statusText = scanEvent->strValue();
    }
    else if (scanEvent->type() == ScannerEvent::kSetPercentComplete)
    {
        m_statusComplete = scanEvent->intValue();
    }
    else if (scanEvent->type() == ScannerEvent::kSetStatusSignalLock)
    {
        m_statusLock = scanEvent->boolValue();
    }
    else if (scanEvent->type() == ScannerEvent::kSetStatusSignalToNoise)
    {
        m_statusSnr = scanEvent->intValue() / 65535.0;
}
#if 0 // THESE_ARE_CURRENTLY_IGNORED
    else if (scanEvent->type() == ScannerEvent::kSetStatusTitleText)
    {
        ;
    }
    else if (scanEvent->type() == ScannerEvent::kSetStatusRotorPosition)
    {
        ;
    }
    else if (scanEvent->type() == ScannerEvent::kSetStatusSignalStrength)
    {
        ;
    }
#endif

    //cout<<"HERE<"<<verboseMask<<">"<<endl;
    QString msg;
    if (VERBOSE_LEVEL_NONE() || VERBOSE_LEVEL_CHECK(VB_CHANSCAN, LOG_INFO))
    {
        msg = QString("%1% S/N %2 %3 : %4 (%5) %6")
            .arg(m_statusComplete, 3)
            .arg(m_statusSnr, 3, 'f', 1)
            .arg((m_statusLock) ? "l" : "L",
                 qPrintable(m_statusText),
                 qPrintable(m_statusLastLog))
            .arg("", 20);
    }
    //cout<<msg.toLatin1().constData()<<endl;

    if (VERBOSE_LEVEL_CHECK(VB_CHANSCAN, LOG_INFO))
    {
        static QString s_oldMsg;
        if (msg != s_oldMsg)
        {
            LOG(VB_CHANSCAN, LOG_INFO, LOC + msg);
            s_oldMsg = msg;
        }
    }
    else if (VERBOSE_LEVEL_NONE())
    {
        if (msg.length() > 80)
            msg = msg.left(77) + "...";
        std::cout<<"\r"<<msg.toLatin1().constData()<<"\r";
        std::cout<<std::flush;
    }
}

void ChannelScannerCLI::InformUser(const QString &error)
{
    if (VERBOSE_LEVEL_NONE())
    {
        std::cerr<<"ERROR: "<<error.toLatin1().constData()<<std::endl;
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + error);
    }
    post_event(m_scanMonitor, ScannerEvent::kScanComplete, 0);
}

void ChannelScannerCLI::Process(const ScanDTVTransportList &_transports)
{
    ChannelImporter ci(false, m_interactive, !m_onlysavescan, !m_onlysavescan, true,
                       m_freeToAirOnly, m_channelNumbersOnly, m_completeOnly,
                       m_fullSearch, m_removeDuplicates, m_serviceRequirements);
    ci.Process(_transports, m_sourceid);
}

/*
 * The parameters are required by the parent class.
 */
void ChannelScannerCLI::MonitorProgress(
    bool /*lock*/, bool /*strength*/, bool /*snr*/, bool /*rotor*/)
{
    if (VERBOSE_LEVEL_NONE())
        std::cout<<"\r0%"<<std::flush;
}
