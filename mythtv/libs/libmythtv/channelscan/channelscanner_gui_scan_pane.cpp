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

#include "libmythbase/mythlogging.h"
#include "libmythui/mythuibutton.h"
#include "libmythui/mythuibuttonlist.h"
#include "libmythui/mythuiprogressbar.h"
#include "libmythui/mythuitext.h"

#include "channelscanner_gui_scan_pane.h"

bool ChannelScannerGUIScanPane::Create()
{
    if (!XMLParseBase::LoadWindowFromXML("config-ui.xml", "channelscanner",
                                         this))
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to load channelscanner screen");
        return false;
    }

    bool error = false;
    UIUtilE::Assign(this, m_statusText, "status", &error);
    UIUtilE::Assign(this, m_log, "log", &error);

    UIUtilE::Assign(this, m_progressBar, "scanprogress", &error);
    if (error)
        return false;

    // Percent done
    UIUtilW::Assign(this, m_progressText, "progresstext");
    // Found status
    UIUtilW::Assign(this, m_scanProgressText, "scanprogresstext");

    UIUtilW::Assign(this, m_signalLockedText, "signallock");
    if (m_signalLockedText)
        m_signalLockedText->SetVisible(m_showSignalLock);

    UIUtilW::Assign(this, m_rotatorPositionText, "rotorprogresstext");
    if (m_rotatorPositionText)
        m_rotatorPositionText->SetVisible(m_showRotorPos);

    UIUtilW::Assign(this, m_rotatorPositionBar,  "rotorprogressbar");
    if (m_rotatorPositionBar)
    {
        m_rotatorPositionBar->SetVisible(m_showRotorPos);
        m_rotatorPositionBar->SetTotal(65535);
    }

    UIUtilW::Assign(this, m_signalStrengthText, "signalstrengthtext");
    if (m_signalStrengthText)
        m_signalStrengthText->SetVisible(m_showSignalStrength);

    UIUtilW::Assign(this, m_signalStrengthBar,  "signalstrength");
    if (m_signalStrengthBar)
    {
        m_signalStrengthBar->SetVisible(m_showSignalStrength);
        m_signalStrengthBar->SetTotal(65535);
    }

    UIUtilW::Assign(this, m_signalNoiseText, "signalnoisetext");
    if (m_signalNoiseText)
        m_signalNoiseText->SetVisible(m_showSignalNoise);

    UIUtilW::Assign(this, m_signalNoiseBar,  "signalnoise");
    if (m_signalNoiseBar)
    {
        m_signalNoiseBar->SetVisible(m_showSignalNoise);
        m_signalNoiseBar->SetTotal(65535);
    }

    m_statusText->SetText(tr("Tuning"));
    m_progressBar->SetTotal(65535);

    MythUIButton *exitButton = nullptr;
    UIUtilW::Assign(this, exitButton, "exit");
    if (exitButton)
        connect(exitButton, &MythUIButton::Clicked, this, &MythScreenType::Close);

    BuildFocusList();

    return true;
}

void ChannelScannerGUIScanPane::SetStatusRotorPosition(int value)
{
    if (m_rotatorPositionText)
        m_rotatorPositionText->SetText(QString("%1%")
                               .arg(static_cast<uint>(value * 100 / 65535)));
    if (m_rotatorPositionBar)
        m_rotatorPositionBar->SetUsed(value);
}

void ChannelScannerGUIScanPane::SetStatusSignalToNoise(int value)
{
    if (m_signalNoiseText)
        m_signalNoiseText->SetText(QString("%1%")
                               .arg(static_cast<uint>(value * 100 / 65535)));
    if (m_signalNoiseBar)
        m_signalNoiseBar->SetUsed(value);
}

void ChannelScannerGUIScanPane::SetStatusSignalStrength(int value)
{
    if (m_signalStrengthText)
        m_signalStrengthText->SetText(QString("%1%")
                               .arg(static_cast<uint>(value * 100 / 65535)));
    if (m_signalStrengthBar)
        m_signalStrengthBar->SetUsed(value);
}

void ChannelScannerGUIScanPane::SetStatusLock(int value)
{
    if (m_signalLockedText)
        m_signalLockedText->SetText((value) ? tr("Locked") : tr("No Lock"));
}

void ChannelScannerGUIScanPane::SetStatusText(const QString &value)
{
    if (m_statusText)
        m_statusText->SetText(value);
}

void ChannelScannerGUIScanPane::SetStatusTitleText(const QString &value)
{
    if (m_scanProgressText)
        m_scanProgressText->SetText(tr("%1").arg(value));
}

void ChannelScannerGUIScanPane::AppendLine(const QString &text)
{
    if (m_log)
    {
        auto *listItem = new MythUIButtonListItem(m_log, text);
        m_log->SetItemCurrent(listItem);
    }
}

void ChannelScannerGUIScanPane::SetScanProgress(double value)
{
    if (m_progressText)
        m_progressText->SetText(QString("%1%")
                               .arg(static_cast<uint>(value * 100)));
    if (m_progressBar)
        m_progressBar->SetUsed(static_cast<uint>(value * 65535));
}
