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

// System headers
#include <unistd.h>

// Std C++
#include <algorithm>
using namespace std;

// MythTV headers
#include "mythcontext.h"
#include "scanwizard.h"
#include "channelscanner_gui.h"
#include "channelimporter.h"
#include "loglist.h"
#include "channelscan_sm.h"
#include "scanprogresspopup.h"

#include "channelbase.h"
#include "dtvsignalmonitor.h"
#include "dvbsignalmonitor.h"
#include "mpegstreamdata.h"

#define LOC QString("ChScanGUI: ")
#define LOC_ERR QString("ChScanGUI, Error: ")

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
    : VerticalConfigurationGroup(false, true, false, false),
      log(new LogList()), popupProgress(NULL)
{
    init_statics();

    setLabel(kTitle);
    addChild(log);
}

ChannelScannerGUI::~ChannelScannerGUI()
{
    Teardown();

    QMutexLocker locker(&popupLock);
    StopPopup();
}

void ChannelScannerGUI::HandleEvent(const ScannerEvent *scanEvent)
{
    switch (scanEvent->eventType())
    {
        case ScannerEvent::ScanComplete:
        {
            QMutexLocker locker(&popupLock);
            if (popupProgress)
            {
                popupProgress->SetScanProgress(1.0);
                popupProgress->accept();
            }
        }
        break;

        case ScannerEvent::ScanShutdown:
        {
            if (scanEvent->ScanProgressPopupValue())
            {
                ScanProgressPopup *spp = scanEvent->ScanProgressPopupValue();
                spp->DeleteDialog();
                spp->deleteLater();
            }

            ScanDTVTransportList transports;
            if (sigmonScanner)
            {
                sigmonScanner->StopScanner();
                transports = sigmonScanner->GetChannelList();
            }

            Teardown();

            int ret = scanEvent->intValue();
            if (!transports.empty() || (MythDialog::Rejected != ret))
                Process(transports);
        }
        break;

        case ScannerEvent::AppendTextToLog:
        {
            log->updateText(scanEvent->strValue());
        }
        break;

        default:
            break;
    }

    QMutexLocker locker(&popupLock);
    if (!popupProgress)
        return;

    switch (scanEvent->eventType())
    {
        case ScannerEvent::SetStatusText:
            popupProgress->SetStatusText(scanEvent->strValue());
            break;
        case ScannerEvent::SetStatusTitleText:
            popupProgress->SetStatusTitleText(scanEvent->strValue());
            break;
        case ScannerEvent::SetPercentComplete:
            popupProgress->SetScanProgress(scanEvent->intValue() * 0.01);
            break;
        case ScannerEvent::SetStatusRotorPosition:
            popupProgress->SetStatusRotorPosition(scanEvent->intValue());
            break;
        case ScannerEvent::SetStatusSignalLock:
            popupProgress->SetStatusLock(scanEvent->intValue());
            break;
        case ScannerEvent::SetStatusSignalToNoise:
            popupProgress->SetStatusSignalToNoise(scanEvent->intValue());
            break;
        case ScannerEvent::SetStatusSignalStrength:
            popupProgress->SetStatusSignalStrength(scanEvent->intValue());
            break;
        default:
            break;
    }
}

void ChannelScannerGUI::Process(const ScanDTVTransportList &_transports)
{
    ChannelImporter ci(true, true, true, true);
    ci.Process(_transports);
}
       
void ChannelScannerGUI::InformUser(const QString &error)
{
    MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                              tr("ScanWizard"), error);
}

void *spawn_popup(void *tmp)
{
    ((ChannelScannerGUI*)(tmp))->RunPopup();
    return NULL;
}

void ChannelScannerGUI::RunPopup(void)
{
    DialogCode ret = popupProgress->exec();

    post_event(scanMonitor, ScannerEvent::ScanShutdown, ret, popupProgress);
    popupProgress = NULL;
}

void ChannelScannerGUI::StopPopup(void)
{
    if (popupProgress)
    {
        popupProgress->reject();
        popupLock.unlock();
        pthread_join(popup_thread, NULL);
        popupLock.lock();
    }
}

void ChannelScannerGUI::MonitorProgress(bool lock, bool strength,
                                        bool snr,  bool rotor)
{
    QMutexLocker locker(&popupLock);
    StopPopup();
    popupProgress = new ScanProgressPopup(lock, strength, snr, rotor);
    popupProgress->CreateDialog();
    if (pthread_create(&popup_thread, NULL, spawn_popup, this) != 0)
    {
        VERBOSE(VB_IMPORTANT, "Failed to start MonitorProgress thread");
        popupProgress->DeleteDialog();
        popupProgress->deleteLater();
        popupProgress = NULL;
    }
}
