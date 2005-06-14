/*
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
#include "settings.h"
#ifdef USING_DVB
#include "dvbtypes.h"
#endif

class OFDMPane;
class QPSKPane;
class ATSCPane;
class QAMPane;
class SIScan;
class DVBChannel;
class DVBSignalMonitor;
class ScanOptionalConfig;
class ScanCountry;
class OptionalTypeSetting;
class ScanWizard;
class ScanWizardScanner;
class AnalogScan;

class VideoSourceSetting: public ComboBoxSetting
{
public:
    VideoSourceSetting();
    virtual void load();
    virtual void save() {}
};

class TransportSetting : public ComboBoxSetting
{
    Q_OBJECT
protected:
    int nSourceID;

public:
    TransportSetting();
    virtual void load();
    virtual void save() {}

    void refresh();
public slots:
    void sourceID(const QString& str);
};

class CaptureCardSetting: public ComboBoxSetting
{
    Q_OBJECT
protected:
    int nSourceID;

public:
    CaptureCardSetting();
    virtual void load();
    virtual void save() { }
    void refresh();

public slots:
    void sourceID(const QString& str);
};

class ScanCountry: public ComboBoxSetting,TransientStorage
{
public:
    enum Country{AU,FI,SE,UK,DE};

    ScanCountry();
};

class ScanFileImport : public LineEditSetting, TransientStorage
{
public:
    ScanFileImport() : LineEditSetting()
    {
        setLabel(QObject::tr("File location"));
        setHelpText(QObject::tr("Location of the channels.conf file."));
    }
};


class ScanTypeSetting : public ComboBoxSetting, public TransientStorage
{
    Q_OBJECT
public:
    enum Type {Error_Open,Error_Probe,
               FullScan_Analog,FullScan_ATSC,FullScan_OFDM,
               FullTunedScan_OFDM,FullTunedScan_QPSK,FullTunedScan_QAM,
               FullTransportScan,TransportScan, Import};

    ScanTypeSetting()
    {
        setLabel(QObject::tr("Scan Type"));
        refresh("");
    }
protected slots:
    void refresh(const QString&);
};

class ScanOptionalConfig: public VerticalConfigurationGroup,
                   public TriggeredConfigurationGroup 
{
    Q_OBJECT
public:
    ScanOptionalConfig(ScanWizard* wizard,ScanTypeSetting* scanType);

    TransportSetting *transport;
    ScanCountry *country;
    ScanFileImport *filename;
protected slots:
    void triggerChanged(const QString&);
};

class ScanWizardScanType: public VerticalConfigurationGroup
{
    friend class ScanWizard;
public:
    ScanWizardScanType(ScanWizard *_parent);

protected:
    ScanWizard *parent;

    ScanOptionalConfig *scanConfig;
    CaptureCardSetting *capturecard;
    VideoSourceSetting *videoSource;
    ScanTypeSetting *scanType;
};

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
    QSqlDatabase* db;

    OFDMPane *paneOFDM;
    QPSKPane *paneQPSK;
    ATSCPane *paneATSC;
    QAMPane  *paneQAM;
    int nVideoDev;
    unsigned nCardType;
    int nCaptureCard;

    ScanWizardScanType *page1;
    int videoSource() {return page1->videoSource->getValue().toInt();}
    int transport() {return page1->scanConfig->transport->getValue().toInt();}
    int country() {return page1->scanConfig->country->getValue().toInt();}
    int captureCard() {return page1->capturecard->getValue().toInt();}
    int scanType() {return page1->scanType->getValue().toInt();}
    QString filename() {return page1->scanConfig->filename->getValue();}
};

class ScanSignalMeter: public ProgressSetting, public TransientStorage {
public:
    ScanSignalMeter(int steps): ProgressSetting(steps) {};
};

class LogList: public ListBoxSetting, public TransientStorage
{
public:
    LogList();

    void updateText(const QString& status);
protected:
    int n;
};

class ScanProgressPopup: public ConfigurationPopupDialog, public VerticalConfigurationGroup
{
    Q_OBJECT
protected:
    ScanSignalMeter *ss;
    ScanSignalMeter *sn;
    TransLabelSetting *sl;
    TransLabelSetting *sta;

    ScanSignalMeter *progressBar;

public:
    ScanProgressPopup(ScanWizardScanner *parent,bool signalmonitors = true);
    ~ScanProgressPopup();
    void exec(ScanWizardScanner *parent);
    void signalToNoise(int value);
    void signalStrength(int value);
    void dvbLock(int value);
    void status(const QString& value);

    void progress(int value) { progressBar->setValue(value);}
    void incrementProgress() { progress(progressBar->getValue().toInt()+1);}
};

class ScanWizardScanner :  public VerticalConfigurationGroup
{
    Q_OBJECT
public:
    static const QString strTitle;

    ScanWizardScanner(ScanWizard *_parent);
    ~ScanWizardScanner();

    int transportToTuneTo() { return nTransportToTuneTo;}

protected slots:
    void scan();
    void cancelScan();
    void scanComplete();
    void transportScanComplete();
    void updateText(const QString& status);

    void TableLoaded();

    void dvbLock(int);
    void dvbSNR(int);
    void dvbSignalStrength(int);

    void serviceScanPctComplete(int pct);
protected:

    class ScannerEvent : public QCustomEvent
    {
    protected:
        QString str;
        int intvalue;

    public:
        enum TYPE {ServiceScanComplete,Update,TableLoaded,ServicePct,
                   DVBSNR,DVBSignalStrength,DVBLock,TuneComplete };
        enum TUNING { OK, ERROR_TUNE};

        ScannerEvent(TYPE t) : QCustomEvent(t+QEvent::User) {}

        QString strValue() const {return str;}
        void strValue(const QString& _str) {str=_str;}

        int intValue() const {return intvalue;}
        void intValue(int _intvalue) {intvalue = _intvalue;}

        TYPE eventType() {return (TYPE)(type()-QEvent::User);}
    };

    ScanWizard *parent;
    AnalogScan *analogScan;
    LogList *log;

    bool scanthread_running;
    bool tunerthread_running;
#ifdef USING_DVB
    pthread_t scanner_thread;
    pthread_t tuner_thread;
    DVBChannel* dvbchannel;
    DVBSignalMonitor* monitor;
    SIScan* scanner;
    dvb_channel_t chan_opts;
#endif
    ScanProgressPopup *popupProgress;
    int nTransportToTuneTo;
    int nScanType;
    int nVideoSource;

    void finish();
    static void *SpawnScanner(void *param);
    static void *SpawnTune(void *param);

    void customEvent( QCustomEvent * e );
};

#endif
