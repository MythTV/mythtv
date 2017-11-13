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

#ifndef _CHANNEL_SCANNER_GUI_SCAN_PANE_H_
#define _CHANNEL_SCANNER_GUI_SCAN_PANE_H_

// MythTV headers
#include "settings.h"
#include "mythscreentype.h"
#include "mythuiprogressbar.h"

class MythUIText;
class MythUIButtonList;

class ChannelScannerGUIScanPane : public MythScreenType
{
    Q_OBJECT

    friend class QObject; // quiet OSX gcc warning

  public:
    ChannelScannerGUIScanPane(
        bool lock, bool strength, bool snr, bool rotorpos,
        MythScreenStack *parent);

    bool Create();

    void SetStatusRotorPosition(int value);
    void SetStatusSignalToNoise(int value);
    void SetStatusSignalStrength(int value);
    void SetStatusLock(int value);
    void SetScanProgress(double value);

    void SetStatusText(const QString &value);
    void SetStatusTitleText(const QString &value);

    void AppendLine(const QString &text);

  private:
    bool m_showSignalLock;
    bool m_showSignalStrength;
    bool m_showSignalNoise;
    bool m_showRotorPos;

    MythUIProgressBar *m_signalStrengthBar;
    MythUIProgressBar *m_signalNoiseBar;
    MythUIProgressBar *m_rotatorPositionBar;
    MythUIProgressBar *m_progressBar;

    MythUIText *m_statusText;
    MythUIText *m_scanProgressText;
    MythUIText *m_signalLockedText;
    MythUIText *m_signalStrengthText;
    MythUIText *m_signalNoiseText;
    MythUIText *m_rotatorPositionText;
    MythUIText *m_progressText;

    MythUIButtonList  *m_log;
};

#endif // _CHANNEL_SCANNER_GUI_SCAN_PANE_H_
