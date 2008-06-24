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

#ifndef _CHANNEL_SCANNER_GUI_H_
#define _CHANNEL_SCANNER_GUI_H_

// POSIX headers
#include <pthread.h>

// Qt headers
#include <qstring.h>

// MythTV headers
#include "settings.h"
#include "channelscanner.h"
//#include "channelimporter_helpers.h"

class ScanWizard;
class LogList;
class ScanProgressPopup;

class Channel;
class DVBChannel;
class SignalMonitorValue;

class ChannelScannerGUI :
    public VerticalConfigurationGroup,
    public ChannelScanner
{
    friend void *spawn_popup(void*);

  public:
    ChannelScannerGUI(void);
    virtual void deleteLater(void)
        { Teardown(); VerticalConfigurationGroup::deleteLater(); }

    virtual void HandleEvent(const ScannerEvent *scanEvent);

  protected:
    virtual ~ChannelScannerGUI();

    virtual void InformUser(const QString &error);

    virtual void Process(const ScanDTVTransportList&);

    virtual void MonitorProgress(bool lock, bool strength,
                                 bool snr, bool rotor);
    void RunPopup(void);
    void StopPopup(void);

  public:
    static QString kTitle;

  private:
    LogList            *log;

    ScanProgressPopup  *popupProgress;
    pthread_t           popup_thread;
    mutable QMutex      popupLock;
};

#endif // _CHANNEL_SCANNER_GUI_H_
