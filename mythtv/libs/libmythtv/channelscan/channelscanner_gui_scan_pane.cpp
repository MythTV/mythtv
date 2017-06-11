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

#include "channelscanner_gui_scan_pane.h"
#include "mythlogging.h"
#include "mythuiprogressbar.h"
#include "mythuitext.h"
#include "mythuibuttonlist.h"
#include "mythuibutton.h"

ChannelScannerGUIScanPane::ChannelScannerGUIScanPane(
    bool lock, bool strength,
    bool snr, bool rotorpos,
    MythScreenStack *parent) :
    MythScreenType(parent, "channelscanner"),
    m_showSignalLock(lock), m_showSignalStrength(strength),
    m_showSignalNoise(snr), m_showRotorPos(rotorpos),
    log(NULL),
    ss(NULL), sn(NULL), pos(NULL),
    progressBar(NULL), sl(NULL), sta(NULL),
    m_scanProgressText(NULL)
{
}

bool ChannelScannerGUIScanPane::Create()
{
    if (!XMLParseBase::LoadWindowFromXML("config-ui.xml", "channelscanner",
                                         this))
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to load channelscanner screen");
        return false;
    }

    bool error = false;
    UIUtilE::Assign(this, sta, "status", &error);
    UIUtilE::Assign(this, log, "log", &error);
    UIUtilE::Assign(this, progressBar, "scanprogress", &error);

    if (error)
        return false;

    UIUtilW::Assign(this, m_scanProgressText, "scanprogresstext");
    UIUtilW::Assign(this, sl, "signallock");
    if (sl)
        sl->SetVisible(m_showSignalLock);

    UIUtilW::Assign(this, pos, "rotorprogress");
    if (pos)
    {
        pos->SetVisible(m_showRotorPos);
        pos->SetTotal(65535);
    }

    UIUtilW::Assign(this, ss, "signalstrength");
    if (ss)
    {
        ss->SetVisible(m_showSignalStrength);
        ss->SetTotal(65535);
    }

    UIUtilW::Assign(this, sn, "signalnoise");
    if (sn)
    {
        sn->SetVisible(m_showSignalNoise);
        sn->SetTotal(65535);
    }

    sta->SetText(tr("Tuning"));
    progressBar->SetTotal(65535);

    MythUIButton *exitButton = NULL;
    UIUtilW::Assign(this, exitButton, "exit");
    if (exitButton)
        connect(exitButton, SIGNAL(Clicked()), SLOT(Close()));

    BuildFocusList();

    return true;
}

void ChannelScannerGUIScanPane::SetStatusRotorPosition(int value)
{
    if (pos)
        pos->SetUsed(value);
}

void ChannelScannerGUIScanPane::SetStatusSignalToNoise(int value)
{
    if (sn)
        sn->SetUsed(value);
}

void ChannelScannerGUIScanPane::SetStatusSignalStrength(int value)
{
    if (ss)
        ss->SetUsed(value);
}

void ChannelScannerGUIScanPane::SetStatusLock(int value)
{
    if (sl)
        sl->SetText((value) ? tr("Locked") : tr("No Lock"));
}

void ChannelScannerGUIScanPane::SetStatusText(const QString &value)
{
    if (sta)
        sta->SetText(value);
}

void ChannelScannerGUIScanPane::SetStatusTitleText(const QString &value)
{
    QString msg = tr("Scan Progress %1").arg(value);
    if (m_scanProgressText)
        m_scanProgressText->SetText(msg);
}

void ChannelScannerGUIScanPane::AppendLine(const QString &text)
{
    if (log)
    {
        MythUIButtonListItem *listItem = new MythUIButtonListItem(log, text);
        log->SetItemCurrent(listItem);
    }
}

void ChannelScannerGUIScanPane::SetScanProgress(double value)
{
    if (progressBar)
        progressBar->SetUsed((uint)(value * 65535));
}
