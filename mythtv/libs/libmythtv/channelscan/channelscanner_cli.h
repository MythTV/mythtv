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

#ifndef _CHANNEL_SCANNER_CLI_H_
#define _CHANNEL_SCANNER_CLI_H_

// Qt headers
#include <QString>

// MythTV headers
#include "mythtvexp.h"
#include "channelscanner.h"

class MTV_PUBLIC ChannelScannerCLI : public ChannelScanner
{
  public:
    ChannelScannerCLI(bool doScanSaveOnly, bool promptsOk)
        : m_onlysavescan(doScanSaveOnly), m_interactive(promptsOk) {}
    virtual ~ChannelScannerCLI() = default;

    void HandleEvent(const ScannerEvent *scanEvent) override; // ChannelScanner

    bool IsDone(void) const { return m_done; }

  protected:
    void InformUser(const QString &error) override; // ChannelScanner
    virtual void Process(const ScanDTVTransportList&);
    void MonitorProgress(bool lock, bool strength,
                         bool snr, bool rotor) override; // ChannelScanner
  private:
    bool    m_done            {false};
    bool    m_onlysavescan;
    bool    m_interactive;
    bool    m_status_lock     {false};
    uint    m_status_complete {0};
    float   m_status_snr      {0.0};
    QString m_status_text;
    QString m_status_last_log;
};

#endif // _CHANNEL_SCANNER_CLI_H_
