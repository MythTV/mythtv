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

class OFDMPane;
class QPSKPane;
class ATSCPane;
class QAMPane;
class SIScan;
class DVBChannel;

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

class ScanTypeSetting : public ComboBoxSetting, public TransientStorage
{
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

class ScanWizardTuningPage;
class ScanWizardScanner;

class ScanWizardScanType: public VerticalConfigurationGroup
{
    Q_OBJECT
    friend class ScanWizard;
public:
    ScanWizardScanType(ScanWizard *_parent);

protected slots:
    void scanTypeChanged(const QString& value);

protected:
    ScanWizard *parent;

    TransportSetting *transport;
    CaptureCardSetting *capturecard;
    VideoSourceSetting *videoSource;
    ScanTypeSetting *scanType;
};

class ScanWizard: public ConfigurationWizard 
{
    Q_OBJECT
    friend class ScanWizardTuningPage;
    friend class ScanWizardScanType;
    friend class ScanWizardScanner;

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
    int transport() {return page1->transport->getValue().toInt();}
    int captureCard() {return page1->capturecard->getValue().toInt();}
    int scanType() {return page1->scanType->getValue().toInt();}
};

class ScanSignalMeter: public ProgressSetting, public TransientStorage {
public:
    ScanSignalMeter(int steps): ProgressSetting(steps) {};
};

class ScanWizardTuningPage : public HorizontalConfigurationGroup, public TriggeredConfigurationGroup
{
    Q_OBJECT
public:
    ScanWizardTuningPage(ScanWizard* parent);

protected slots:
    virtual void triggerChanged(const QString& value);
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
    bool tunerthread_running;
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
