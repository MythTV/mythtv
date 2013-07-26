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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 *
 */

// Qt headers
#include <QCoreApplication>
#include <iostream>

using namespace std;

// MythTv headers
#include "channelscanner_cli.h"
#include "channelscan_sm.h"
#include "channelimporter.h"

#define LOC      QString("ChScanCLI: ")

ChannelScannerCLI::ChannelScannerCLI(bool doScanSaveOnly, bool promptsOk) :
    done(false), onlysavescan(doScanSaveOnly), interactive(promptsOk),
    status_lock(false), status_complete(0), status_snr(0),
    status_text(""), status_last_log("")
{
}

ChannelScannerCLI::~ChannelScannerCLI()
{
}

void ChannelScannerCLI::HandleEvent(const ScannerEvent *scanEvent)
{
    if ((scanEvent->type() == ScannerEvent::ScanComplete) ||
        (scanEvent->type() == ScannerEvent::ScanShutdown) ||
        (scanEvent->type() == ScannerEvent::ScanErrored))
    {
        cout<<endl;

        if (scanEvent->type() == ScannerEvent::ScanShutdown)
            cerr<<"HandleEvent(void) -- scan shutdown"<<endl;
        else
            cerr<<"HandleEvent(void) -- scan complete"<<endl;

        ScanDTVTransportList transports;
        if (sigmonScanner)
        {
            sigmonScanner->StopScanner();
            transports = sigmonScanner->GetChannelList();
        }

        Teardown();

        if (scanEvent->type() == ScannerEvent::ScanErrored)
        {
            QString error = scanEvent->strValue();
            InformUser(error);
        }
        else if (sigmonScanner && !transports.empty())
            Process(transports);

        done = true;
        QCoreApplication::exit(0);
    }
    else if (scanEvent->type() == ScannerEvent::AppendTextToLog)
        status_last_log = scanEvent->strValue();
    else if (scanEvent->type() == ScannerEvent::SetStatusText)
        status_text = scanEvent->strValue();
    else if (scanEvent->type() == ScannerEvent::SetPercentComplete)
        status_complete = scanEvent->intValue();
    else if (scanEvent->type() == ScannerEvent::SetStatusSignalLock)
        status_lock = scanEvent->intValue();
    else if (scanEvent->type() == ScannerEvent::SetStatusSignalToNoise)
        status_snr = scanEvent->intValue() / 65535.0;
#if THESE_ARE_CURRENTLY_IGNORED
    else if (scanEvent->type() == ScannerEvent::SetStatusTitleText)
        ;
    else if (scanEvent->type() == ScannerEvent::SetStatusRotorPosition)
        ;
    else if (scanEvent->type() == ScannerEvent::SetStatusSignalStrength)
        ;
#endif

    //cout<<"HERE<"<<verboseMask<<">"<<endl;
    QString msg;
    if (VERBOSE_LEVEL_NONE || VERBOSE_LEVEL_CHECK(VB_CHANSCAN, LOG_INFO))
    {
        msg.sprintf("%3i%% S/N %3.1f %s : %s (%s) %20s",
                    status_complete, status_snr,
                    (status_lock) ? "l" : "L",
                    status_text.toLatin1().constData(),
                    status_last_log.toLatin1().constData(), "");
    }
    //cout<<msg.toLatin1().constData()<<endl;

    if (VERBOSE_LEVEL_CHECK(VB_CHANSCAN, LOG_INFO))
    {
        static QString old_msg;
        if (msg != old_msg)
        {
            LOG(VB_CHANSCAN, LOG_INFO, LOC + msg);
            old_msg = msg;
        }
    }
    else if (VERBOSE_LEVEL_NONE)
    {
        if (msg.length() > 80)
            msg = msg.left(77) + "...";
        cout<<"\r"<<msg.toLatin1().constData()<<"\r";
        cout<<flush;
    }
}

void ChannelScannerCLI::InformUser(const QString &error)
{
    if (VERBOSE_LEVEL_NONE)
    {
        cerr<<"ERROR: "<<error.toLatin1().constData()<<endl;
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + error);
    }
    post_event(scanMonitor, ScannerEvent::ScanComplete, 0);
}

void ChannelScannerCLI::Process(const ScanDTVTransportList &_transports)
{
    ChannelImporter ci(false, interactive, !onlysavescan, !onlysavescan, true,
                       freeToAirOnly, serviceRequirements);
    ci.Process(_transports);
}

void ChannelScannerCLI::MonitorProgress(
    bool lock, bool strength, bool snr, bool rotor)
{
    if (VERBOSE_LEVEL_NONE)
        cout<<"\r0%"<<flush;
}
