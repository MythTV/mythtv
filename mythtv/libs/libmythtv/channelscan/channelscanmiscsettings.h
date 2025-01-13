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

#ifndef CHANNEL_SCAN_MISC_SETTINGS_H
#define CHANNEL_SCAN_MISC_SETTINGS_H

#include "libmythui/standardsettings.h"
#include "channelscantypes.h"

class TransLabelSetting;
class ScanWizard;
class OptionalTypeSetting;
class PaneDVBT;
class PaneDVBS;
class PaneDVBS2;
class PaneATSC;
class PaneDVBC;
class PaneAnalog;
class STPane;
class DVBUtilsImportPane;

// ///////////////////////////////
// Settings Below Here
// ///////////////////////////////

class IgnoreSignalTimeout : public TransMythUICheckBoxSetting
{
  public:
    IgnoreSignalTimeout()
    {
        setLabel(QObject::tr("Ignore Signal Timeout"));
        setHelpText(
            QObject::tr("This option allows you to slow down the scan for "
                        "broken drivers, such as the DVB drivers for the "
                        "Leadtek LR6650 DVB card."));
    }
};

class FollowNITSetting : public TransMythUICheckBoxSetting
{
  public:
    FollowNITSetting()
    {
        setLabel(QObject::tr("Search new Transports"));
        setHelpText(
            QObject::tr("Digital transmissions may signal other available "
                        "Transports. If this option is enabled the scanner "
                        "scans all signaled transports for new/updated "
                        "channels."));
    }
};

class DesiredServices : public TransMythUIComboBoxSetting
{
  public:
    DesiredServices()
    {
        setLabel(QObject::tr("Desired Services"));
        setHelpText(QObject::tr(
                        "TV - Adds A/V services only, "
                        "TV+Radio - Adds all services with audio, "
                        "All - Adds all services "
                        "(including data only services)."));
        addSelection(QObject::tr("TV"),       "tv", true);
        addSelection(QObject::tr("TV+Radio"), "audio");
        addSelection(QObject::tr("All"),      "all");
    };

    ServiceRequirements GetServiceRequirements(void) const
    {
        QString val = getValue();
        int ret = kRequireVideo | kRequireAudio;
        if (val == "tv")
            ret = kRequireVideo | kRequireAudio;
        else if (val == "audio")
            ret = kRequireAudio;
        else if (val == "all")
            ret = 0;
        return (ServiceRequirements) ret;
    }
};

class FreeToAirOnly : public TransMythUICheckBoxSetting
{
  public:
    FreeToAirOnly()
    {
        setLabel(QObject::tr("Unencrypted Only"));
        setHelpText(
            QObject::tr(
                "If set, only non-encrypted channels will be "
                "added during the scan."));
        setValue(true);
    };
};

class ChannelNumbersOnly : public TransMythUICheckBoxSetting
{
  public:
    ChannelNumbersOnly()
    {
        setLabel(QObject::tr("Logical Channel Numbers required"));
        setHelpText(
            QObject::tr(
                "If set, only services with a Logical Channel Number will "
                "be added during the scan. This will filter out services "
                "for set-top-box firmware download and video-on-demand "
                "that can be present on DVB-C networks."));
        setValue(false);
    };
};

class CompleteChannelsOnly : public TransMythUICheckBoxSetting
{
  public:
    CompleteChannelsOnly()
    {
        setLabel(QObject::tr("Complete scan data required"));
        setHelpText(
            QObject::tr(
                "If set, only services that are present in the PAT, the PMT "
                "and the SDT and that have a name will be added during the scan. "
                "On satellites there are many incomplete channels, "
                "such as feeds and offline channels, "
                "that are not useful in a MythTV system. "
                "These are filtered out by this option."));
        setValue(true);
    };
};

class FullChannelSearch : public TransMythUICheckBoxSetting
{
  public:
    FullChannelSearch()
    {
        setLabel(QObject::tr("Full search for old channels"));
        setHelpText(
            QObject::tr(
                "If set, compare all channels in the database with the channels found in "
                "the scan; otherwise only the channels in the same transport are compared. "
                "This option is useful when you want to keep channel data such as "
                "the xmltvid and the icon path when doing a rescan "
                "after channels have been rearranged across transports."));
        setValue(true);
    };
};

class RemoveDuplicates : public TransMythUICheckBoxSetting
{
  public:
    RemoveDuplicates()
    {
        setLabel(QObject::tr("Remove duplicates"));
        setHelpText(
            QObject::tr(
                "If set, select the transport stream multiplex with the best signal "
                "when identical transports are received on different frequencies. "
                "This option is useful for DVB-T2 and ATSC/OTA when a transport "
                "can sometimes be received from different transmitters."));
        setValue(true);
    };
};

class AddFullTS : public TransMythUICheckBoxSetting
{
  public:
    AddFullTS()
    {
        setLabel(QObject::tr("Add full Transport Stream channels"));
        setHelpText(
            QObject::tr(
                "If set, create MPTS channels, which allow "
                "recording of the full, unaltered, transport stream."));
        setValue(false);
    };
};

class TrustEncSISetting : public TransMythUICheckBoxSetting
{
  public:
    TrustEncSISetting()
    {
        setLabel(QObject::tr("Test Decryptability"));
        setHelpText(
            QObject::tr("Test all channels to see if they can be decrypted "
                        "with installed CAM/smartcard. Sometimes the encrypted "
                        "flag is set spuriously. Attention: Enabling this "
                        "option increases the scan time for each encrypted "
                        "channel by a couple of seconds."));
        setValue(false);
    }
};

// Scan data reference transponder
class ScanTransponder: public TransMythUIComboBoxSetting
{
  public:
    ScanTransponder() : TransMythUIComboBoxSetting(false)
    {
        setLabel(QObject::tr("Satellite tuning data"));
        setHelpText(
             QObject::tr(
                "Select a satellite from this list for the "
                "tuning data of the reference transponder."));
        addSelection("(Select Satellite)", "Select", true);

        // Satellite tuning data
        m_tdm["1 TH"] = { "1 TH", "Thor 5/6/7  0.8W", "10872000", "h", "25000000", "8PSK", "DVB-S2", "3/4" };
        m_tdm["2 E7"] = { "2 E7", "Eutelsat    7.0E", "10721000", "h", "22000000", "QPSK", "DVB-S",  "3/4" };
        m_tdm["3 HB"] = { "3 HB", "Hotbird    13.0E", "12015000", "h", "27500000", "8PSK", "DVB-S2", "3/4" };
        m_tdm["4 A1"] = { "4 A1", "Astra-1    19.2E", "11229000", "v", "22000000", "8PSK", "DVB-S2", "2/3" };
        m_tdm["5 A3"] = { "5 A3", "Astra-3    23.5E", "12031500", "h", "27500000", "QPSK", "DVB-S2", "auto"};
        m_tdm["6 A2"] = { "6 A2", "Astra-2    28.2E", "10773000", "h", "23000000", "8PSK", "DVB-S2", "3/4" };
        m_tdm["7 T3"] = { "7 T3", "Turksat-3A 42.0E", "12610000", "h", "20830000", "QPSK", "DVB-S",  "3/4" };
        m_tdm["8 T4"] = { "8 T4", "Turksat-4A 42.0E", "11916000", "v", "30000000", "QPSK", "DVB-S",  "3/4" };
        m_tdm["9 T8"] = { "9 T8", "Turksat-8K 42.0E", "12605000", "v", "34285000","16APSK","DVB-S2", "2/3" };

        for (auto &td: m_tdm)
        {
            addSelection(td.fullname, td.name);
        }
    }

  private:
    struct tuningdata {
        QString name;
        QString fullname;
        QString frequency;
        QString polarity;
        QString symbolrate;
        QString modulation;
        QString modSys;
        QString fec;
    };
    QMap<QString, struct tuningdata> m_tdm;

  public:
    QString getFrequency (const QString &satname) {return m_tdm[satname].frequency; }
    QString getPolarity  (const QString &satname) {return m_tdm[satname].polarity;  }
    QString getSymbolrate(const QString &satname) {return m_tdm[satname].symbolrate;}
    QString getModulation(const QString &satname) {return m_tdm[satname].modulation;}
    QString getModSys    (const QString &satname) {return m_tdm[satname].modSys;    }
    QString getFec       (const QString &satname) {return m_tdm[satname].fec;       }
};

class ScanFrequencykHz: public TransTextEditSetting
{
  public:
    ScanFrequencykHz()
    {
        setLabel(QObject::tr("Frequency"));
        setHelpText(QObject::tr("Frequency (Option has no default)\n"
                                "The frequency for this transport (multiplex) in kHz."));
    };
};

class ScanFrequency: public TransTextEditSetting
{
  public:
    ScanFrequency()
    {
        setLabel(QObject::tr("Frequency"));
        setHelpText(QObject::tr("Frequency (Option has no default)\n"
                                "The frequency for this transport (multiplex) in Hz."));
    };
};

class ScanDVBSSymbolRate: public TransMythUIComboBoxSetting
{
  public:
    ScanDVBSSymbolRate() : TransMythUIComboBoxSetting(true)
    {
        setLabel(QObject::tr("Symbol Rate"));
        setHelpText(
             QObject::tr(
                "Symbol Rate (symbols/second).\n"
                "Most DVB-S transponders transmit at 27500000 "
                "symbols per second."));
        addSelection("3333000");
        addSelection("22000000");
        addSelection("22500000");
        addSelection("23000000");
        addSelection("27500000", "27500000", true);
        addSelection("28000000");
        addSelection("28500000");
        addSelection("29500000");
        addSelection("29700000");
        addSelection("29900000");
    }
};

class ScanDVBCSymbolRate: public TransMythUIComboBoxSetting
{
  public:
    ScanDVBCSymbolRate() : TransMythUIComboBoxSetting(true)
    {
        setLabel(QObject::tr("Symbol Rate"));
        setHelpText(
             QObject::tr(
                "Symbol Rate (symbols/second).\n"
                "Most DVB-C transports transmit at 6900000 or 6875000 "
                "symbols per second."));
        addSelection("3450000");
        addSelection("5000000");
        addSelection("5900000");
        addSelection("6875000");
        addSelection("6900000", "6900000", true);
        addSelection("6950000");
    }
};

class ScanPolarity: public TransMythUIComboBoxSetting
{
  public:
    ScanPolarity()
    {
        setLabel(QObject::tr("Polarity"));
        setHelpText(QObject::tr("Polarity (Option has no default)"));
        addSelection(QObject::tr("Horizontal"), "h",true);
        addSelection(QObject::tr("Vertical"), "v");
        addSelection(QObject::tr("Right Circular"), "r");
        addSelection(QObject::tr("Left Circular"), "l");
    };
};

class ScanInversion: public TransMythUIComboBoxSetting
{
  public:
    ScanInversion()
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

class ScanBandwidth: public TransMythUIComboBoxSetting
{
  public:
    ScanBandwidth()
    {
        setLabel(QObject::tr("Bandwidth"));
        setHelpText(QObject::tr("Bandwidth (Default: Auto)\n"));
        addSelection(QObject::tr("Auto"),"a",true);
        addSelection(QObject::tr("6 MHz"),"6");
        addSelection(QObject::tr("7 MHz"),"7");
        addSelection(QObject::tr("8 MHz"),"8");
    };
};

class ScanFecSetting: public TransMythUIComboBoxSetting
{
  public:
    ScanFecSetting()
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
        addSelection("3/5");
        addSelection("9/10");
    }
};

class ScanFec: public ScanFecSetting
{
  public:
    ScanFec()
    {
        setLabel(QObject::tr("FEC"));
        setHelpText(QObject::tr(
                        "Forward Error Correction (Default: Auto)"));
    }
};

class ScanCodeRateLP: public ScanFecSetting
{
  public:
    ScanCodeRateLP()
    {
        setLabel(QObject::tr("LP Coderate"));
        setHelpText(QObject::tr("Low Priority Code Rate (Default: Auto)"));
    }
};

class ScanCodeRateHP: public ScanFecSetting
{
  public:
    ScanCodeRateHP()
    {
        setLabel(QObject::tr("HP Coderate"));
        setHelpText(QObject::tr("High Priority Code Rate (Default: Auto)"));
    };
};

class ScanGuardInterval: public TransMythUIComboBoxSetting
{
  public:
    ScanGuardInterval()
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

class ScanTransmissionMode: public TransMythUIComboBoxSetting
{
  public:
    ScanTransmissionMode()
    {
        setLabel(QObject::tr("Trans. Mode"));
        setHelpText(QObject::tr("Transmission Mode (Default: Auto)"));
        addSelection(QObject::tr("Auto"),"a");
        addSelection("2K","2");
        addSelection("8K","8");
    };
};

class ScanHierarchy: public TransMythUIComboBoxSetting
{
    public:
    ScanHierarchy()
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

class ScanDVBSModSys: public TransMythUIComboBoxSetting
{
    public:
    ScanDVBSModSys()
    {
        setLabel(QObject::tr("Modulation System"));
        setHelpText(QObject::tr("Modulation system (Default: DVB-S2)"));
        addSelection("DVB-S",  "DVB-S");
        addSelection("DVB-S2", "DVB-S2", true);
    };
};

class ScanDVBTModSys: public TransMythUIComboBoxSetting
{
    public:
    ScanDVBTModSys()
    {
        setLabel(QObject::tr("Modulation System"));
        setHelpText(QObject::tr("Modulation system (Default: DVB-T2)"));
        addSelection("DVB-T",  "DVB-T");
        addSelection("DVB-T2", "DVB-T2", true);
    };
};

class ScanDVBCModSys : public  TransMythUIComboBoxSetting
{
  public:
    ScanDVBCModSys()
    {
        setLabel(QObject::tr("Modulation System"));
        setHelpText(QObject::tr("Modulation System (Default: DVB-C/A)"));
        addSelection("DVB-C/A", "DVB-C/A", true);
        addSelection("DVB-C/B", "DVB-C/B");
        addSelection("DVB-C/C", "DVB-C/C");
    }
};

class ScanRollOff: public TransMythUIComboBoxSetting
{
    public:
    ScanRollOff()
    {
        setLabel(QObject::tr("Roll-off"));
        setHelpText(QObject::tr("Roll-off factor (Default: 0.35)"));
        addSelection("0.35");
        addSelection("0.20");
        addSelection("0.25");
        addSelection(QObject::tr("Auto"),"auto");
    };
};

class PaneError : public GroupSetting
{
  public:
    explicit PaneError(const QString &error)
    {
        auto* label = new TransTextEditSetting();
        label->setValue(error);
        addChild(label);
    }
};

#endif // CHANNEL_SCAN_MISC_SETTINGS_H
