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

#ifndef _SCAN_PROGRESS_POPUP_H_
#define _SCAN_PROGRESS_POPUP_H_

// Qt headesr
#include <qwaitcondition.h>

// MythTV headers
#include "settings.h"

class ScanSignalMeter: public ProgressSetting, public TransientStorage
{
  public:
    ScanSignalMeter(int steps): ProgressSetting(this, steps) {};
};

class ScanProgressPopup : public ConfigurationPopupDialog
{
    Q_OBJECT

    friend class QObject; // quiet OSX gcc warning

  public:
    ScanProgressPopup(bool lock, bool strength, bool snr, bool rotorpos);
    virtual void deleteLater(void);

    void CreateDialog(void);
    virtual DialogCode exec(void);
    void DeleteDialog(void);

    void SetStatusRotorPosition(int value);
    void SetStatusSignalToNoise(int value);
    void SetStatusSignalStrength(int value);
    void SetStatusLock(int value);
    void SetScanProgress(double value)
        { progressBar->setValue((uint)(value * 65535));}

    void SetStatusText(const QString &value);
    void SetStatusTitleText(const QString &value);

  private slots:
    void Done(void);

  private:
    ~ScanProgressPopup();

    bool               done;
    QWaitCondition     wait;

    ScanSignalMeter   *ss;
    ScanSignalMeter   *sn;
    ScanSignalMeter   *pos;
    ScanSignalMeter   *progressBar;

    TransLabelSetting *sl;
    TransLabelSetting *sta;
};

#endif // _SCAN_PROGRESS_POPUP_H_
