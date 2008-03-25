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
 *     functionality
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

#ifndef SCANWIZARDHELPERS_H
#define SCANWIZARDHELPERS_H

#include "settings.h"

#include <qwaitcondition.h>
#include <qevent.h>

class TransFreqTableSelector;
class TransLabelSetting;
class ScanWizardScanner;
class ScanWizard;
class OptionalTypeSetting;
class VideoSourceSelector;
class OFDMPane;
class QPSKPane;
class DVBS2Pane;
class ATSCPane;
class QAMPane;
class AnalogPane;
class STPane;
class DVBUtilsImportPane;

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
    ScanProgressPopup(bool lock, bool strength, bool snr);

    virtual DialogCode exec(void);

    void SetStatusSignalToNoise(int value);
    void SetStatusSignalStrength(int value);
    void SetStatusLock(int value);
    void SetScanProgress(double value);

    void SetStatusText(const QString &value);
    void SetStatusTitleText(const QString &value);

  private slots:
    void PopupDone(int);

  private:
    ~ScanProgressPopup();

    bool               done;
    QWaitCondition     wait;

    ScanSignalMeter   *ss;
    ScanSignalMeter   *sn;
    ScanSignalMeter   *progressBar;

    TransLabelSetting *sl;
    TransLabelSetting *sta;
};

class ScannerEvent : public QEvent
{
    friend class QObject; // quiet OSX gcc warning

  public:
    enum TYPE 
    {
        ScanComplete,
        ScanShutdown,
        AppendTextToLog,
        SetStatusText,
        SetStatusTitleText,
        SetPercentComplete,
        SetStatusSignalToNoise,
        SetStatusSignalStrength,
        SetStatusSignalLock,
    };

    ScannerEvent(TYPE t) : QEvent( (QEvent::Type)(t + QEvent::User)) { ; }

    QString strValue()              const { return str; }
    void    strValue(const QString& _str) { str = _str; }

    int     intValue()        const { return intvalue; }
    void    intValue(int _intvalue) { intvalue = _intvalue; }

    TYPE    eventType()       const { return (TYPE)(type()-QEvent::User); }

  private:
    ~ScannerEvent() { }

  private:
    QString str;
    int     intvalue;
};

// ///////////////////////////////
// Settings Below Here
// ///////////////////////////////

class MultiplexSetting : public ComboBoxSetting, public TransientStorage
{
    Q_OBJECT

  public:
    MultiplexSetting() : ComboBoxSetting(this), sourceid(0)
        { setLabel(tr("Transport")); }

    virtual void load(void);

    void SetSourceID(uint _sourceid);

  protected:
    uint sourceid;
};

class IgnoreSignalTimeout : public CheckBoxSetting, public TransientStorage
{
  public:
    IgnoreSignalTimeout() : CheckBoxSetting(this)
    {
        setLabel(QObject::tr("Ignore Signal Timeout"));
        setHelpText(
            QObject::tr("This option allows you to slow down the scan for "
                        "broken drivers, such as the DVB drivers for the "
                        "Leadtek LR6650 DVB card."));
    }
};

class InputSelector : public ComboBoxSetting, public TransientStorage
{
    Q_OBJECT

  public:
    InputSelector(uint _default_cardid, const QString &_default_inputname);

    virtual void load(void);

    uint GetCardID(void) const;

    QString GetInputName(void) const;

    static bool Parse(const QString &cardid_inputname,
                      uint          &cardid,
                      QString       &inputname);

  public slots:
    void SetSourceID(const QString &_sourceid);

  private:
    uint    sourceid;
    uint    default_cardid;
    QString default_inputname;
};

class ScanCountry : public ComboBoxSetting, public TransientStorage
{
    Q_OBJECT

  public:
    enum Country
    {
        AU,
        FI,
        SE,
        UK,
        DE,
        ES,
    };

    ScanCountry();
};

class ScanTypeSetting : public ComboBoxSetting, public TransientStorage
{
    Q_OBJECT
  public:
    enum Type
    {
        Error_Open = 0,
        Error_Probe,
        // Scans that check each frequency in a predefined list
        FullScan_Analog,
        FullScan_ATSC,
        FullScan_OFDM,
        // Scans starting on one frequency that adds each transport
        // seen in the Network Information Tables to the scan.
        NITAddScan_OFDM,
        NITAddScan_QPSK,
        NITAddScan_QAM,
        // Scan of all transports already in the database
        FullTransportScan,
        // Scan of one transport already in the database
        TransportScan,
        // IPTV import of channels from M3U URL
        IPTVImport,
        // Imports lists from dvb-utils scanners
        DVBUtilsImport,
    };

    ScanTypeSetting() : ComboBoxSetting(this), hw_cardid(0)
        { setLabel(QObject::tr("Scan Type")); }

  protected slots:
    void SetInput(const QString &cardids_inputname);

  protected:
    uint    hw_cardid;
};

class ScanOptionalConfig : public TriggeredConfigurationGroup 
{
    Q_OBJECT

  public:
    ScanOptionalConfig(ScanTypeSetting *_scan_type);

    QString GetATSCFormat(const QString&)    const;
    QString GetFrequencyStandard(void)       const;
    QString GetModulation(void)              const;
    QString GetFrequencyTable(void)          const;
    bool    DoIgnoreSignalTimeout(void)      const;
    QString GetFilename(void)                const;
    uint    GetMultiplex(void)               const;
    bool    DoDeleteChannels(void)           const;
    bool    DoRenameChannels(void)           const;
    QMap<QString,QString> GetStartChan(void) const;

    void SetDefaultATSCFormat(const QString &atscFormat);

  public slots:
    void SetSourceID(const QString&);
    void triggerChanged(const QString&);

  private:
    ScanTypeSetting     *scanType;
    ScanCountry         *country;
    IgnoreSignalTimeout *ignoreSignalTimeoutAll;
    OFDMPane            *paneOFDM;
    QPSKPane            *paneQPSK;
    DVBS2Pane           *paneDVBS2;
    ATSCPane            *paneATSC;
    QAMPane             *paneQAM;
    AnalogPane          *paneAnalog;
    STPane              *paneSingle;
    DVBUtilsImportPane  *paneDVBUtilsImport;
};

class ScanWizardConfig: public VerticalConfigurationGroup
{
    Q_OBJECT

  public:
    ScanWizardConfig(ScanWizard *_parent,
                     uint        default_sourceid,
                     uint        default_cardid,
                     QString     default_inputname);

    uint    GetSourceID(void)     const;
    QString GetATSCFormat(void)   const;
    QString GetModulation(void)   const { return scanConfig->GetModulation(); }
    int     GetScanType(void)     const { return scanType->getValue().toInt();}
    uint    GetCardID(void)       const { return input->GetCardID();          }
    QString GetInputName(void)    const { return input->GetInputName();       }
    QString GetFilename(void)     const { return scanConfig->GetFilename();   }
    uint    GetMultiplex(void)    const { return scanConfig->GetMultiplex();  }
    bool DoDeleteChannels(void) const { return scanConfig->DoDeleteChannels();}
    bool DoRenameChannels(void) const { return scanConfig->DoRenameChannels();}
    QString GetFrequencyStandard(void) const
        { return scanConfig->GetFrequencyStandard(); }
    QString GetFrequencyTable(void) const
        { return scanConfig->GetFrequencyTable(); }
    QMap<QString,QString> GetStartChan(void) const
        { return scanConfig->GetStartChan(); }
    bool    DoIgnoreSignalTimeout(void) const
        { return scanConfig->DoIgnoreSignalTimeout(); }

    void SetDefaultATSCFormat(const QString &atscFormat)
        { scanConfig->SetDefaultATSCFormat(atscFormat); }

  protected:
    VideoSourceSelector *videoSource;
    InputSelector       *input;
    ScanTypeSetting     *scanType;
    ScanOptionalConfig  *scanConfig;
};

class LogList: public ListBoxSetting, public TransientStorage
{
  public:
    LogList();

    void updateText(const QString& status);
  protected:
    int n;
};

class ScanFrequencyTable: public ComboBoxSetting, public TransientStorage
{
  public:
    ScanFrequencyTable() : ComboBoxSetting(this)
    {
        addSelection(QObject::tr("Broadcast"),        "us",          true);
        addSelection(QObject::tr("Cable")     + " " +
                     QObject::tr("High"),             "uscablehigh", false);
        addSelection(QObject::tr("Cable HRC") + " " +
                     QObject::tr("High"),             "ushrchigh",   false);
        addSelection(QObject::tr("Cable IRC") + " " +
                     QObject::tr("High"),             "usirchigh",   false);
        addSelection(QObject::tr("Cable"),            "uscable",     false);
        addSelection(QObject::tr("Cable HRC"),        "ushrc",       false);
        addSelection(QObject::tr("Cable IRC"),        "usirc",       false);

        setLabel(QObject::tr("Frequency Table"));
        setHelpText(QObject::tr("Frequency table to use.") + " " +
                    QObject::tr(
                        "The option of scanning only \"High\" "
                        "frequency channels is useful because most "
                        "digital channels are on the higher frequencies."));
    }
};

class ScanATSCModulation: public ComboBoxSetting, public TransientStorage
{
  public:
    ScanATSCModulation() : ComboBoxSetting(this)
    {
        addSelection(QObject::tr("Terrestrial")+" (8-VSB)","vsb8",  true);
        addSelection(QObject::tr("Cable") + " (QAM-256)", "qam256", false);
        addSelection(QObject::tr("Cable") + " (QAM-128)", "qam128", false);
        addSelection(QObject::tr("Cable") + " (QAM-64)",  "qam64",  false);

        setLabel(QObject::tr("Modulation"));
        setHelpText(
            QObject::tr("Modulation, 8-VSB, QAM-256, etc.") + " " +
            QObject::tr("Most cable systems in the United States use "
                        "QAM-256 or QAM-64, but some mixed systems "
                        "may use 8-VSB for over-the-air channels."));
    }
};

class ScanATSCChannelFormat: public ComboBoxSetting, public TransientStorage
{
  public:
    ScanATSCChannelFormat() : ComboBoxSetting(this)
    {
        addSelection(QObject::tr("(5_1) Underscore"), "%1_%2", true);
        addSelection(QObject::tr("(5-1) Minus"),      "%1-%2", false);
        addSelection(QObject::tr("(5.1) Period"),     "%1.%2", false);
        addSelection(QObject::tr("(501) Zero"),       "%10%2", false);
        addSelection(QObject::tr("(51) None"),        "%1%2",  false);
        setLabel(QObject::tr("ATSC Channel Separator"));
        setHelpText(QObject::tr("What to use to separate ATSC major "
                                "and minor channels."));
    }
};

class ScanOldChannelTreatment: public ComboBoxSetting, public TransientStorage
{
  public:
    ScanOldChannelTreatment(bool rename = true) : ComboBoxSetting(this)
    {
        addSelection(QObject::tr("Minimal Updates"),    "minimal");
        if (rename)
            addSelection(QObject::tr("Rename to Match"),    "rename");
        addSelection(QObject::tr("Delete"),             "delete");
        setLabel(QObject::tr("Existing Channel Treatment"));
        setHelpText(QObject::tr("How to treat existing channels."));
    }
};

class ScanFrequency: public LineEditSetting, public TransientStorage
{
  public:
    ScanFrequency(bool in_kHz = false) : LineEditSetting(this)
    {
        QString units = (in_kHz) ? "kHz" : "Hz";
        setLabel(QObject::tr("Frequency (%1)").arg(units));
        setHelpText(
            QObject::tr(
                "Frequency (Option has no default).\n"
                "The frequency for this channel in %1.").arg(units));
    };
};

class ScanSymbolRate: public ComboBoxSetting, public TransientStorage
{
  public:
    ScanSymbolRate() : ComboBoxSetting(this, true)
    {
        setLabel(QObject::tr("Symbol Rate"));
        setHelpText(
             QObject::tr(
                "Symbol Rate (symbols/second).\n"
                "Most dvb-s transponders transmit at 27.5 "
                "million symbols per second."));
        addSelection("3333000");
        addSelection("22000000");
        addSelection("27500000", "27500000", true);
        addSelection("28000000");
        addSelection("28500000");
        addSelection("29900000");
    };
};

class ScanPolarity: public ComboBoxSetting, public TransientStorage
{
  public:
    ScanPolarity() : ComboBoxSetting(this)
    {
        setLabel(QObject::tr("Polarity"));
        setHelpText(QObject::tr("Polarity (Option has no default)"));
        addSelection(QObject::tr("Horizontal"), "h",true);
        addSelection(QObject::tr("Vertical"), "v");
        addSelection(QObject::tr("Right Circular"), "r");
        addSelection(QObject::tr("Left Circular"), "l");
    };
};

class ScanInversion: public ComboBoxSetting, public TransientStorage
{
  public:
    ScanInversion() : ComboBoxSetting(this)
    {
        setLabel(QObject::tr("Inversion"));
        setHelpText(QObject::tr(
                        "Inversion (Default: Auto):\n"
                        "Most cards can autodetect this now, so "
                        "leave it at Auto unless it won't work."));
        addSelection(QObject::tr("Auto"), "a",true);
        addSelection(QObject::tr("On"), "1");
        addSelection(QObject::tr("Off"), "0");
    };
};

class ScanBandwidth: public ComboBoxSetting, public TransientStorage
{
  public:
    ScanBandwidth() : ComboBoxSetting(this)
    {
        setLabel(QObject::tr("Bandwidth"));
        setHelpText(QObject::tr("Bandwidth (Default: Auto)\n"));
        addSelection(QObject::tr("Auto"),"a",true);
        addSelection(QObject::tr("6 MHz"),"6");
        addSelection(QObject::tr("7 MHz"),"7");
        addSelection(QObject::tr("8 MHz"),"8");
    };
};

class ScanModulationSetting: public ComboBoxSetting
{
  public:
    ScanModulationSetting(Storage *_storage) : ComboBoxSetting(_storage)
    {
        addSelection(QObject::tr("Auto"),"auto",true);
        addSelection("QPSK","qpsk");
#ifdef FE_GET_EXTENDED_INFO
        addSelection("8PSK","8psk");
#endif
        addSelection("QAM 16","qam_16");
        addSelection("QAM 32","qam_32");
        addSelection("QAM 64","qam_64");
        addSelection("QAM 128","qam_128");
        addSelection("QAM 256","qam_256");
    };
};

class ScanModulation: public ScanModulationSetting, public TransientStorage
{
  public:
    ScanModulation() : ScanModulationSetting(this)
    {
        setLabel(QObject::tr("Modulation"));
        setHelpText(QObject::tr("Modulation (Default: Auto)"));
    };
};

class ScanConstellation: public ScanModulationSetting,
                         public TransientStorage
{
  public:
    ScanConstellation() : ScanModulationSetting(this)
    {
        setLabel(QObject::tr("Constellation"));
        setHelpText(QObject::tr("Constellation (Default: Auto)"));
    };
};

class ScanFecSetting: public ComboBoxSetting
{
  public:
    ScanFecSetting(Storage *_storage) : ComboBoxSetting(_storage)
    {
        addSelection(QObject::tr("Auto"),"auto",true);
        addSelection(QObject::tr("None"),"none");
        addSelection("1/2");
        addSelection("2/3");
        addSelection("3/4");
        addSelection("4/5");
        addSelection("5/6");
        addSelection("6/7");
        addSelection("7/8");
        addSelection("8/9");
    }
};

class ScanFec: public ScanFecSetting, public TransientStorage
{
  public:
    ScanFec() : ScanFecSetting(this)
    {
        setLabel(QObject::tr("FEC"));
        setHelpText(QObject::tr(
                        "Forward Error Correction (Default: Auto)"));
    }
};

class ScanCodeRateLP: public ScanFecSetting, public TransientStorage
{
  public:
    ScanCodeRateLP() : ScanFecSetting(this)
    {
        setLabel(QObject::tr("LP Coderate"));
        setHelpText(QObject::tr("Low Priority Code Rate (Default: Auto)"));
    }
};

class ScanCodeRateHP: public ScanFecSetting, public TransientStorage
{
  public:
    ScanCodeRateHP() : ScanFecSetting(this)
    {
        setLabel(QObject::tr("HP Coderate"));
        setHelpText(QObject::tr("High Priority Code Rate (Default: Auto)"));
    };
};

class ScanGuardInterval: public ComboBoxSetting, public TransientStorage
{
  public:
    ScanGuardInterval() : ComboBoxSetting(this)
    {
        setLabel(QObject::tr("Guard Interval"));
        setHelpText(QObject::tr("Guard Interval (Default: Auto)"));
        addSelection(QObject::tr("Auto"),"auto");
        addSelection("1/4");
        addSelection("1/8");
        addSelection("1/16");
        addSelection("1/32");
    };
};

class ScanTransmissionMode: public ComboBoxSetting, public TransientStorage
{
  public:
    ScanTransmissionMode() : ComboBoxSetting(this)
    {
        setLabel(QObject::tr("Trans. Mode"));
        setHelpText(QObject::tr("Transmission Mode (Default: Auto)"));
        addSelection(QObject::tr("Auto"),"a");
        addSelection("2K","2");
        addSelection("8K","8");
    };
};

class ScanHierarchy: public ComboBoxSetting, public TransientStorage
{
    public:
    ScanHierarchy() : ComboBoxSetting(this)
    {
        setLabel(QObject::tr("Hierarchy"));
        setHelpText(QObject::tr("Hierarchy (Default: Auto)"));
        addSelection(QObject::tr("Auto"),"a");
        addSelection(QObject::tr("None"), "n");
        addSelection("1");
        addSelection("2");
        addSelection("4");
    };
};

class OFDMPane : public HorizontalConfigurationGroup
{
  public:
    OFDMPane() : HorizontalConfigurationGroup(false, false, true, true)
    {
        setUseFrame(false);
        VerticalConfigurationGroup *left =
            new VerticalConfigurationGroup(false,true,true,false);
        VerticalConfigurationGroup *right =
            new VerticalConfigurationGroup(false,true,true,false);
        left->addChild(pfrequency       = new ScanFrequency());
        left->addChild(pbandwidth       = new ScanBandwidth());
        left->addChild(pinversion       = new ScanInversion());
        left->addChild(pconstellation   = new ScanConstellation());
        right->addChild(pcoderate_lp    = new ScanCodeRateLP());
        right->addChild(pcoderate_hp    = new ScanCodeRateHP());
        right->addChild(ptrans_mode     = new ScanTransmissionMode());
        right->addChild(pguard_interval = new ScanGuardInterval());
        right->addChild(phierarchy      = new ScanHierarchy());
        addChild(left);
        addChild(right);
    }

    QString frequency(void)      const { return pfrequency->getValue();     }
    QString bandwidth(void)      const { return pbandwidth->getValue();     }
    QString inversion(void)      const { return pinversion->getValue();     }
    QString constellation(void)  const { return pconstellation->getValue(); }
    QString coderate_lp(void)    const { return pcoderate_lp->getValue();   }
    QString coderate_hp(void)    const { return pcoderate_hp->getValue();   }
    QString trans_mode(void)     const { return ptrans_mode->getValue();    }
    QString guard_interval(void) const { return pguard_interval->getValue(); }
    QString hierarchy(void)      const { return phierarchy->getValue();     }

  protected:
    ScanFrequency        *pfrequency;
    ScanInversion        *pinversion;
    ScanBandwidth        *pbandwidth;
    ScanConstellation    *pconstellation;
    ScanCodeRateLP       *pcoderate_lp;
    ScanCodeRateHP       *pcoderate_hp;
    ScanTransmissionMode *ptrans_mode;
    ScanGuardInterval    *pguard_interval;
    ScanHierarchy        *phierarchy;
};

class DVBS2Pane : public HorizontalConfigurationGroup
{
  public:
    DVBS2Pane() : HorizontalConfigurationGroup(false,false,true,false) 
    {
        setUseFrame(false);
        VerticalConfigurationGroup *left =
            new VerticalConfigurationGroup(false,true);
        VerticalConfigurationGroup *right =
            new VerticalConfigurationGroup(false,true);
        left->addChild( pfrequency  = new ScanFrequency());
        left->addChild( ppolarity   = new ScanPolarity());
        left->addChild( psymbolrate = new ScanSymbolRate());
        right->addChild(pfec        = new ScanFec());
        right->addChild(pmodulation = new ScanModulation());
        right->addChild(pinversion  = new ScanInversion());
        addChild(left);
        addChild(right);     
    }

    QString frequency(void)  const { return pfrequency->getValue();  }
    QString symbolrate(void) const { return psymbolrate->getValue(); }
    QString inversion(void)  const { return pinversion->getValue();  }
    QString fec(void)        const { return pfec->getValue();        }
    QString polarity(void)   const { return ppolarity->getValue();   }
    QString modulation(void) const { return pmodulation->getValue(); }

  protected:
    ScanFrequency  *pfrequency;
    ScanSymbolRate *psymbolrate;
    ScanInversion  *pinversion;
    ScanFec        *pfec;
    ScanPolarity   *ppolarity;
    ScanModulation *pmodulation;
};

class QPSKPane : public HorizontalConfigurationGroup
{
  public:
    QPSKPane() : HorizontalConfigurationGroup(false, false, true, false)
    {
        setUseFrame(false);
        VerticalConfigurationGroup *left =
            new VerticalConfigurationGroup(false,true);
        VerticalConfigurationGroup *right =
            new VerticalConfigurationGroup(false,true);
        left->addChild(pfrequency  = new ScanFrequency(true));
        left->addChild(ppolarity   = new ScanPolarity());
        left->addChild(psymbolrate = new ScanSymbolRate());
        right->addChild(pfec       = new ScanFec());
        right->addChild(pinversion = new ScanInversion());
        addChild(left);
        addChild(right);     
    }

    QString frequency(void)  const { return pfrequency->getValue();  }
    QString symbolrate(void) const { return psymbolrate->getValue(); }
    QString inversion(void)  const { return pinversion->getValue();  }
    QString fec(void)        const { return pfec->getValue();        }
    QString polarity(void)   const { return ppolarity->getValue();   }

  protected:
    ScanFrequency  *pfrequency;
    ScanSymbolRate *psymbolrate;
    ScanInversion  *pinversion;
    ScanFec        *pfec;
    ScanPolarity   *ppolarity;
};

class QAMPane : public HorizontalConfigurationGroup
{
  public:
    QAMPane() : HorizontalConfigurationGroup(false, false, true, false)
    {
        setUseFrame(false);
        VerticalConfigurationGroup *left =
            new VerticalConfigurationGroup(false,true);
        VerticalConfigurationGroup *right =
            new VerticalConfigurationGroup(false,true);
        left->addChild(pfrequency   = new ScanFrequency());
        left->addChild(psymbolrate  = new ScanSymbolRate());
        left->addChild(pinversion   = new ScanInversion());
        right->addChild(pmodulation = new ScanModulation());
        right->addChild(pfec        = new ScanFec());
        addChild(left);
        addChild(right);     
    }

    QString frequency(void)  const { return pfrequency->getValue();  }
    QString symbolrate(void) const { return psymbolrate->getValue(); }
    QString inversion(void)  const { return pinversion->getValue();  }
    QString fec(void)        const { return pfec->getValue();        }
    QString modulation(void) const { return pmodulation->getValue(); }

  protected:
    ScanFrequency  *pfrequency;
    ScanSymbolRate *psymbolrate;
    ScanInversion  *pinversion;
    ScanModulation *pmodulation;
    ScanFec        *pfec;
};

class ATSCPane : public VerticalConfigurationGroup
{
  public:
    ATSCPane() : VerticalConfigurationGroup(false, false, true, false)
    {
        addChild(atsc_table            = new ScanFrequencyTable());
        addChild(atsc_modulation       = new ScanATSCModulation());
        addChild(atsc_format           = new ScanATSCChannelFormat());
        addChild(old_channel_treatment = new ScanOldChannelTreatment());
    }

    QString atscFreqTable(void)  const { return atsc_table->getValue();      }
    QString atscModulation(void) const { return atsc_modulation->getValue(); }
    QString GetATSCFormat(void)  const { return atsc_format->getValue();     }
    bool DoDeleteChannels(void) const
        { return old_channel_treatment->getValue() == "delete"; }
    bool DoRenameChannels(void) const
        { return old_channel_treatment->getValue() == "rename"; }

    void SetDefaultATSCFormat(const QString &d)
    {
        int val = atsc_format->getValueIndex(d);
        atsc_format->setValue(val);
    }

  protected:
    ScanFrequencyTable      *atsc_table;
    ScanATSCModulation      *atsc_modulation;
    ScanATSCChannelFormat   *atsc_format;
    ScanOldChannelTreatment *old_channel_treatment;
};

class AnalogPane : public VerticalConfigurationGroup
{
  public:
    AnalogPane();

    void SetSourceID(uint sourceid);

    QString GetFrequencyTable(void) const;

    bool DoDeleteChannels(void) const
        { return old_channel_treatment->getValue() == "delete"; }
    bool DoRenameChannels(void) const
        { return old_channel_treatment->getValue() == "rename"; }

  protected:
    TransFreqTableSelector  *freq_table;
    ScanOldChannelTreatment *old_channel_treatment;
};

class STPane : public VerticalConfigurationGroup
{
  public:
    STPane() :
        VerticalConfigurationGroup(false, false, true, false),
        transport_setting(new MultiplexSetting()),
        atsc_format(new ScanATSCChannelFormat()),
        old_channel_treatment(new ScanOldChannelTreatment()),
        ignore_signal_timeout(new IgnoreSignalTimeout())
    {
        addChild(transport_setting);
        addChild(atsc_format);
        addChild(old_channel_treatment);
        addChild(ignore_signal_timeout);
    }

    QString GetATSCFormat(void) const { return atsc_format->getValue(); }
    bool DoDeleteChannels(void) const
        { return old_channel_treatment->getValue() == "delete"; }
    bool DoRenameChannels(void) const
        { return old_channel_treatment->getValue() == "rename"; }
    int  GetMultiplex(void) const
        { return transport_setting->getValue().toInt(); }
    bool ignoreSignalTimeout(void) const
        { return ignore_signal_timeout->getValue().toInt(); }

    void SetDefaultATSCFormat(const QString &d)
    {
        int val = atsc_format->getValueIndex(d);
        atsc_format->setValue(val);
    }

    void SetSourceID(uint sourceid)
        { transport_setting->SetSourceID(sourceid); }

  protected:
    MultiplexSetting        *transport_setting;
    ScanATSCChannelFormat   *atsc_format;
    ScanOldChannelTreatment *old_channel_treatment;
    IgnoreSignalTimeout     *ignore_signal_timeout;
};

class DVBUtilsImportPane : public VerticalConfigurationGroup
{
    Q_OBJECT

  public:
    DVBUtilsImportPane() :
        VerticalConfigurationGroup(false,false,true,false),
        filename(new TransLineEditSetting()),
        atsc_format(new ScanATSCChannelFormat()),
        old_channel_treatment(new ScanOldChannelTreatment()),
        ignore_signal_timeout(new IgnoreSignalTimeout())
    {
        filename->setLabel(tr("File location"));
        filename->setHelpText(tr("Location of the channels.conf file."));
        addChild(filename);

        addChild(atsc_format);
        addChild(old_channel_treatment);
        addChild(ignore_signal_timeout);
    }

    QString GetFilename(void)   const { return filename->getValue();    }
    QString GetATSCFormat(void) const { return atsc_format->getValue(); }

    bool DoDeleteChannels(void) const
        { return old_channel_treatment->getValue() == "delete"; }
    bool DoRenameChannels(void) const
        { return old_channel_treatment->getValue() == "rename"; }
    bool DoIgnoreSignalTimeout(void) const
        { return ignore_signal_timeout->getValue().toInt(); }

    void SetDefaultATSCFormat(const QString &d)
    {
        int val = atsc_format->getValueIndex(d);
        atsc_format->setValue(val);
    }

  private:
    TransLineEditSetting    *filename;
    ScanATSCChannelFormat   *atsc_format;
    ScanOldChannelTreatment *old_channel_treatment;
    IgnoreSignalTimeout     *ignore_signal_timeout;
};

class ErrorPane : public HorizontalConfigurationGroup
{
  public:
    ErrorPane(const QString &error) :
        HorizontalConfigurationGroup(false, false, true, false)
    {
        TransLabelSetting* label = new TransLabelSetting();
        label->setValue(error);
        addChild(label);
    }
};

class BlankSetting: public TransLabelSetting
{
  public:
    BlankSetting() : TransLabelSetting()
    {
        setLabel("");
    }
};

#endif // SCANWIZARDHELPERS_H
