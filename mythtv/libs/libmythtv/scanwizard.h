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
#include "dvbtypes.h"

class ScanFrequency;
class ScanSymbolRate;
class ScanPolarity;
class ScanFec;
class ScanModulation;
class ScanInversion;
class ScanBandwidth;
class ScanConstellation;
class ScanCodeRateLP;
class ScanCodeRateHP;
class ScanTransmissionMode;
class ScanGuardInterval;
class ScanHierarchy;
class ScanATSCTransport;
class SIScan;

class ScanTypeSetting : public ComboBoxSetting, public TransientStorage
{
    Q_OBJECT
public:
    enum Type {FullScan,FullTransportScan,TransportScan} ;

    ScanTypeSetting()
    {
        setLabel(QObject::tr("Scan Type"));
        addSelection(tr("Full Scan"),QString::number(FullScan),true);
        addSelection(tr("Full Scan of Existing Transports"),QString::number(FullTransportScan));
        addSelection(tr("Existing Transport Scan"),QString::number(TransportScan));
    }
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

class VideoSourceSetting: public ComboBoxSetting
{
    Q_OBJECT
public:
    VideoSourceSetting();
    virtual void load();
    virtual void save() {}
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

class ScanATSCTransport: public ComboBoxSetting, public TransientStorage
{
public:
    enum Type {Terrestrial,Cable} ;
    ScanATSCTransport()
    {
        addSelection(tr("Terrestrial"),QString::number(Terrestrial),true);
        addSelection(tr("Cable"),QString::number(Cable));

        setLabel(QObject::tr("ATSC Transport"));
        setHelpText(QObject::tr("ATSC transport, cable or terrestrial"));
    }
};

class ScanWizardScanner;

class ScanWizard: public ConfigurationWizard {
    Q_OBJECT
public:
    ScanWizard();

    MythDialog* dialogWidget(MythMainWindow *parent, const char *widgetName);

protected slots:
    void pageSelected(const QString& strTitle);

signals:
    void scan();

private:
    QString strFrequency;
    QString strSymbolRate;
    QString strPolarity;
    QString strFec;
    QString strModulation;
    QString strInversion;
    QString strBandwidth;
    QString strConstellation;
    QString strCodeRateLP;
    QString strCodeRateHP;
    QString strTransmissionMode;
    QString strGuardInterval;
    QString strHierarchy;

    ScanTypeSetting::Type nScanType;
    unsigned nTransport;
    unsigned nCaptureCard;
    unsigned nVideoDev;
    unsigned nVideoSource;
    unsigned nCardType;
    unsigned nATSCTransport;

public slots:
    void frequency(const QString& str) { strFrequency = str; };
    void symbolRate(const QString& str) { strSymbolRate = str; };
    void polarity(const QString& str) { strPolarity = str; };
    void fec(const QString& str) { strFec = str; };
    void modulation(const QString& str) { strModulation = str; };
    void inversion(const QString& str) { strInversion = str; };
    void bandwidth(const QString& str) { strBandwidth = str; };
    void constellation(const QString& str) { strConstellation = str; };
    void codeRateLP(const QString& str) { strCodeRateLP = str; };
    void codeRateHP(const QString& str) { strCodeRateHP = str; };
    void transmissionMode(const QString& str) { strTransmissionMode = str; };
    void guardInterval(const QString& str) { strGuardInterval = str; };
    void hierarchy(const QString& str) { strHierarchy = str; };

    void scanType(ScanTypeSetting::Type _type) { nScanType=_type;}
    void videoSource(const QString& str);
    void transport(const QString& str);
    void captureCard(const QString& str);
    void atscTransport(const QString& str);

public:
    QString frequency() {return strFrequency;}
    QString symbolRate() {return strSymbolRate;}
    QString polarity() {return strPolarity;}
    QString fec() {return strFec;}
    QString modulation() {return strModulation;}
    QString inversion() {return strInversion;}
    QString bandwidth() {return strBandwidth;}
    QString constellation() {return strConstellation;}
    QString codeRateLP() {return strCodeRateLP;}
    QString codeRateHP() {return strCodeRateHP;}
    QString transmissionMode() {return strTransmissionMode;}
    QString guardInterval() {return strGuardInterval;}
    QString hierarchy() {return strHierarchy;}
    ScanTypeSetting::Type scanType() { return nScanType;}
    unsigned videoSource() {return nVideoSource;}
    unsigned transport() {return nTransport;}
    unsigned captureCard() {return nCaptureCard;}
    unsigned cardType() {return nCardType;}
    unsigned videoDev() {return nVideoDev;}
    unsigned atscTransport() {return nATSCTransport;}

signals:
    void cardTypeChanged(unsigned);
};

class ScanWizardScanType: public VerticalConfigurationGroup
{
    Q_OBJECT
public:
    ScanWizardScanType(ScanWizard *parent);

public slots:
    void scanTypeChanged(const QString& str);
signals:
    void scanTypeChanged(ScanTypeSetting::Type);
protected:
    TransportSetting *transport;
    CaptureCardSetting *capturecard;
};

class ScanSignalMeter: public ProgressSetting, public TransientStorage {
public:
    ScanSignalMeter(int steps): ProgressSetting(steps) {};
};

class OFDMPane;
class QPSKPane;
class ATSCPane;
class QAMPane;

class ScanWizardTuningPage : public HorizontalConfigurationGroup, public TriggeredConfigurationGroup
{
    Q_OBJECT
public:
    ScanWizardTuningPage(ScanWizard* parent);

protected slots:
    virtual void triggerChanged(const QString& value);
    void cardTypeChanged(unsigned nType);
    void scanType(ScanTypeSetting::Type _type);
protected:
    ATSCPane *atsc;
    OFDMPane *ofdm;
    QAMPane *qam;
    QPSKPane *qpsk;
};

class LogList: public ListBoxSetting, public TransientStorage
{
    Q_OBJECT
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
    ScanProgressPopup(ScanWizardScanner *parent);
    ~ScanProgressPopup();
    void exec(ScanWizardScanner *parent);
    void signalToNoise(int value);
    void signalStrength(int value);
    void dvbStatus(const QString& value);
    void status(const QString& value);

    void progress(int value) { progressBar->setValue(value);}
    void incrementProgress() { progress(progressBar->getValue().toInt()+1);}
};

class DVBChannel;
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

    void dvbStatus(const QString& str);
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
                   DVBSNR,DVBSignalStrength,DVBStatus,TuneComplete };
        enum TUNING { OK, ERROR_TUNE};

        ScannerEvent(TYPE t) : QCustomEvent(t+QEvent::User) {}

        QString strValue() const {return str;}
        void strValue(const QString& _str) {str=_str;}

        int intValue() const {return intvalue;}
        void intValue(int _intvalue) {intvalue = _intvalue;}

        TYPE eventType() {return (TYPE)(type()-QEvent::User);}
    };

    ScanWizard *parent;

    LogList *log;

    bool scanthread_running;
    pthread_t scanner_thread;
    pthread_t tuner_thread;
    DVBChannel* dvbchannel;
    SIScan* scanner;
    dvb_channel_t chan_opts;
    ScanProgressPopup *popupProgress;
    int nTransportToTuneTo;

    void finish();
    static void *SpawnScanner(void *param);
    static void *SpawnTune(void *param);

    void customEvent( QCustomEvent * e );
};



#endif
