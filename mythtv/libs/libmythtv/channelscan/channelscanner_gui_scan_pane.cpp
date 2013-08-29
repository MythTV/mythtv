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
#include "loglist.h"

ChannelScannerGUIScanPane::ChannelScannerGUIScanPane(
    bool lock, bool strength,
    bool snr, bool rotorpos,
    QObject *target, const char *slot) :
    VerticalConfigurationGroup(false, false, true, true),
    ss(NULL), sn(NULL), pos(NULL),
    progressBar(NULL), sl(NULL), sta(NULL)
{
    setLabel(tr("Scan Progress"));

    ConfigurationGroup *slg =
        new HorizontalConfigurationGroup(false, false, true, true);
    slg->addChild(sta = new TransLabelSetting());
    sta->setLabel(tr("Status"));
    sta->setValue(tr("Tuning"));

    if (lock)
    {
        slg->addChild(sl = new TransLabelSetting());
        sl->setValue("                                  "
                     "                                  ");
    }

    addChild(slg);

    if (rotorpos)
    {
        addChild(pos = new TransProgressSetting());
        pos->setLabel(tr("Rotor Movement"));
    }

    ConfigurationGroup *ssg = NULL;
    if (strength || snr)
        ssg = new HorizontalConfigurationGroup(false, false, true, true);

    if (strength)
    {
        ssg->addChild(ss = new TransProgressSetting());
        ss->setLabel(tr("Signal Strength"));
    }

    if (snr)
    {
        ssg->addChild(sn = new TransProgressSetting());
        sn->setLabel(tr("Signal/Noise"));
    }

    if (strength || snr)
        addChild(ssg);

    addChild(progressBar = new TransProgressSetting());
    progressBar->setValue(0);
    progressBar->setLabel(tr("Scan"));

    addChild(log = new LogList());

    TransButtonSetting *cancel = new TransButtonSetting();
    cancel->setLabel(tr("Stop Scan"));
    addChild(cancel);

    connect(cancel, SIGNAL(pressed(void)), target, slot);

    //Seem to need to do this as the constructor doesn't seem enough
    setUseLabel(false);
    setUseFrame(false);
}

void ChannelScannerGUIScanPane::SetStatusRotorPosition(int value)
{
    if (pos)
        pos->setValue(value);
}

void ChannelScannerGUIScanPane::SetStatusSignalToNoise(int value)
{
    if (sn)
        sn->setValue(value);
}

void ChannelScannerGUIScanPane::SetStatusSignalStrength(int value)
{
    if (ss)
        ss->setValue(value);
}

void ChannelScannerGUIScanPane::SetStatusLock(int value)
{
    if (sl)
        sl->setValue((value) ? tr("Locked") : tr("No Lock"));
}

void ChannelScannerGUIScanPane::SetStatusText(const QString &value)
{
    if (sta)
        sta->setValue(value);
}

void ChannelScannerGUIScanPane::SetStatusTitleText(const QString &value)
{
    QString msg = tr("Scan Progress %1").arg(value);
    setLabel(msg);
}

void ChannelScannerGUIScanPane::AppendLine(const QString &text)
{
    log->AppendLine(text);
}
