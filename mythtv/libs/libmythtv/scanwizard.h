/* -*- Mode: c++ -*-
 * $Id$
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

#ifndef SCANWIZARD_H
#define SCANWIZARD_H

#include <qsqldatabase.h>

#include <mythwizard.h>
#include "scanwizardhelpers.h"
#include "settings.h"
#ifdef USING_DVB
#include "dvbtypes.h"
#endif

class OFDMPane;
class QPSKPane;
class ATSCPane;
class QAMPane;
class SIScan;
class ChannelBase;
class Channel;
class DVBChannel;
class DVBSignalMonitor;
class ScanOptionalConfig;
class ScanCountry;
class OptionalTypeSetting;
class ScanWizard;
class ScanWizardScanner;
class AnalogScan;

class ScanWizard: public ConfigurationWizard 
{
    Q_OBJECT
    friend class ScanWizardScanType;
    friend class ScanWizardScanner;
    friend class ScanOptionalConfig;
  public:
    ScanWizard();

    MythDialog* dialogWidget(MythMainWindow *parent, const char *widgetName);

  protected slots:
    void pageSelected(const QString& strTitle);
    void captureCard(const QString& str);

  signals:
    void scan();
    void cardTypeChanged(const QString&);

  protected:
    int videoSource()  const { return page1->videoSource->getValue().toInt(); }
    int transport()    const
        { return page1->scanConfig->transport->getValue().toInt(); }
    QString country()  const { return page1->scanConfig->country->getValue(); }
    int captureCard()  const { return page1->capturecard->getValue().toInt(); }
    int scanType()     const { return page1->scanType->getValue().toInt();    }
    QString filename() const { return page1->scanConfig->filename->getValue();}

    QSqlDatabase *db;
    OFDMPane     *paneOFDM;
    QPSKPane     *paneQPSK;
    ATSCPane     *paneATSC;
    QAMPane      *paneQAM;
    int           nVideoDev;
    unsigned      nCardType;
    int           nCaptureCard;
    ScanWizardScanType *page1;
};

class ScanWizardScanner :  public VerticalConfigurationGroup
{
    Q_OBJECT
  public:
    static const QString strTitle;

    ScanWizardScanner(ScanWizard *_parent);
    ~ScanWizardScanner() { finish(); }

    int transportToTuneTo() { return nTransportToTuneTo;}

  protected slots:
    void scan(void);
    void cancelScan(void);
    void scanComplete(void);
    void transportScanComplete(void);
    void updateText(const QString& status);

    void dvbLock(int);
    void dvbSNR(int);
    void dvbSignalStrength(int);

    void TableLoaded(void);

    void serviceScanPctComplete(int pct);

  protected:
    void finish();
    void HandleTuneComplete(void);
    void customEvent(QCustomEvent *e);
    static void *SpawnScanner(void *param);
    static void *SpawnTune(void *param);
    DVBChannel *GetDVBChannel();
    Channel *GetChannel();

    ScanWizard        *parent;
    AnalogScan        *analogScan;
    LogList           *log;
    bool               scanthread_running;
    bool               tunerthread_running;
    pthread_t          tuner_thread;
    ChannelBase       *channel;
    ScanProgressPopup *popupProgress;
    int                nTransportToTuneTo;
    SIScan            *scanner;

    int                nScanType;
    int                nVideoSource;

#ifdef USING_DVB
    pthread_t          scanner_thread;
    DVBChannel        *dvbchannel;
    DVBSignalMonitor  *monitor;
    dvb_channel_t      chan_opts;
#endif
};

#endif
