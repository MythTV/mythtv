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
#include <QApplication>

// MythTv headers
#include "channelscanner_cli.h"
#include "channelscan_sm.h"
#include "channelimporter.h"

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
    switch (scanEvent->eventType())
    {
        case ScannerEvent::ScanComplete:
        case ScannerEvent::ScanShutdown:
        {
            cout<<endl;

            if (ScannerEvent::ScanShutdown == scanEvent->eventType())
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

            if (!transports.empty())
                Process(transports);

            done = true;
            QApplication::exit(0);
        }
        break;

        case ScannerEvent::AppendTextToLog:
            status_last_log = scanEvent->strValue();
            break;
        case ScannerEvent::SetStatusText:
            status_text = scanEvent->strValue();
            //cout<<"."<<flush;
            break;
        case ScannerEvent::SetStatusTitleText:
            //cout<<"."<<flush;
            break;
        case ScannerEvent::SetPercentComplete:
            status_complete = scanEvent->intValue();
            break;
        case ScannerEvent::SetStatusRotorPosition:
            break;
        case ScannerEvent::SetStatusSignalLock:
            status_lock = scanEvent->intValue();
            break;
        case ScannerEvent::SetStatusSignalToNoise:
            status_snr = scanEvent->intValue() / 65535.0;
            break;
        case ScannerEvent::SetStatusSignalStrength:
            break;
        default:
            break;
    }

    printf("\r%3i%% S/N %3.1f %s : %s (%s) %20s\r",
           status_complete, status_snr,
           (status_lock) ? "l" : "L",
           status_text.ascii(), status_last_log.ascii(), "");

    cout<<flush;
}

void ChannelScannerCLI::InformUser(const QString &error)
{
    cerr<<"ERROR: "<<error.ascii()<<endl;
    post_event(scanMonitor, ScannerEvent::ScanComplete, 0);
}

void ChannelScannerCLI::Process(const ScanDTVTransportList &_transports)
{
    ChannelImporter ci(false, interactive, !onlysavescan, true);
    ci.Process(_transports);
}

void ChannelScannerCLI::MonitorProgress(
    bool lock, bool strength, bool snr, bool rotor)
{
    cout<<"\r0%"<<flush;
}
