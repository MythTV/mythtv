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

#ifndef _CHANNEL_SCANNER_GUI_H_
#define _CHANNEL_SCANNER_GUI_H_

// Qt headers
#include <QStringList>

// MythTV headers
#include "settings.h"
#include "channelscanner.h"

class ScanWizard;
class LogList;
class ChannelScannerGUIScanPane;
class DeleteStage;
class InsertStage;

class Channel;
class DVBChannel;
class SignalMonitorValue;

class ChannelScannerGUI :
    public StackedConfigurationGroup,
    public ChannelScanner
{
    Q_OBJECT

    friend void *spawn_popup(void*);

  public:
    ChannelScannerGUI(void);
    virtual void deleteLater(void)
        { Teardown(); StackedConfigurationGroup::deleteLater(); }

    virtual void HandleEvent(const ScannerEvent *scanEvent);

  protected:
    virtual ~ChannelScannerGUI();

    virtual void InformUser(const QString &error);

    virtual void Process(const ScanDTVTransportList&, bool success = false);

    virtual void MonitorProgress(bool lock, bool strength,
                                 bool snr, bool rotor);

  public:
    static QString kTitle;

  public slots:
    void quitScanning(void);

  private:
    ChannelScannerGUIScanPane *scanStage;
    LogList                   *doneStage;
    QStringList                messageList;
};

#endif // _CHANNEL_SCANNER_GUI_H_
