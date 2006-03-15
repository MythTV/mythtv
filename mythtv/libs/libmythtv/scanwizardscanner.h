/* -*- Mode: c++ -*-
 * vim: set expandtab tabstop=4 shiftwidth=4:
 *
 * Original Project
 *      MythTV      http://www.mythtv.org
 *
 * Author(s):
 *      John Pullan  (john@pullan.org)
 *
 * Description:
 *     Collection of classes to provide dvb channel scanning
 *     functionallity
 *
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

#ifndef _SCANWIZARDSCANNER_H_
#define _SCANWIZARDSCANNER_H_

// Standard UNIX C headers
#include <pthread.h>

// Qt headers
#include <qstring.h>

// MythTV headers
#include "settings.h"
#ifdef USING_DVB
#include "dvbtypes.h"
#endif

class ScanWizard;
class AnalogScan;
class LogList;
class SIScan;
class ScanProgressPopup;

class ChannelBase;
class Channel;
class DVBChannel;
class SignalMonitorValue;

class ScanWizardScanner : public VerticalConfigurationGroup
{
    friend class ScanWizard;
    Q_OBJECT
  public:
    static const QString strTitle;

    ScanWizardScanner(ScanWizard *_parent);
    ~ScanWizardScanner() { finish(); }

    void scan(void);

  protected slots:
    void cancelScan(void);
    void scanComplete(void);
    void transportScanComplete(void);
    void updateText(const QString& status);
    void updateStatusText(const QString& status);

    void dvbLock(const SignalMonitorValue&);
    void dvbSNR(const SignalMonitorValue&);
    void dvbSignalStrength(const SignalMonitorValue&);

    void TableLoaded(void);

    void serviceScanPctComplete(int pct);

  protected:
    void Import(uint sourceid, int cardtype, const QString &file);
    void PreScanCommon(uint cardid, uint sourceid);
    void TunedScanCommon(uint cardid, uint sourceid, bool ok);
    void ScanAnalog(uint cardid, uint sourceid);

    void dvbLock(int);
    void dvbSNR(int);
    void dvbSignalStrength(int);
    void finish(void);
    void HandleTuneComplete(void);
    void customEvent(QCustomEvent *e);

    DVBChannel *GetDVBChannel(void);
    Channel    *GetChannel(void);

    static void *SpawnTune(void *param);

  private:
    ScanWizard        *parent;
    LogList           *log;
    bool               tunerthread_running;
    pthread_t          tuner_thread;
    ChannelBase       *channel;
    ScanProgressPopup *popupProgress;

    SIScan            *scanner;
    AnalogScan        *analogScanner;

    int                nScanType;
    int                nMultiplexToTuneTo;
    uint               nVideoSource;

    // tranport info
    uint               frequency;
    QString            modulation;
#ifdef USING_DVB
    dvb_channel_t      chan_opts;
#endif // USING_DVB
};

#endif // _SCANWIZARDSCANNER_H_

