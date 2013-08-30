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

#ifndef _MISC_SETTINGS_H_
#define _MISC_SETTINGS_H_

#include "settings.h"
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

class FollowNITSetting : public CheckBoxSetting, public TransientStorage
{
  public:
    FollowNITSetting() : CheckBoxSetting(this)
    {
        setLabel(QObject::tr("Search new Transports"));
        setHelpText(
            QObject::tr("Digital transmissions may signal other available "
                        "Transports. If this option is enabled the scanner "
                        "scans all signaled transports for new/updated "
                        "channels."));
    }
};

class DesiredServices : public ComboBoxSetting, public TransientStorage
{
  public:
    DesiredServices() : ComboBoxSetting(this)
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

class FreeToAirOnly : public CheckBoxSetting, public TransientStorage
{
  public:
    FreeToAirOnly() : CheckBoxSetting(this)
    {
        setValue(true);
        setLabel(QObject::tr("Unencrypted Only"));
        setHelpText(
            QObject::tr(
                "If set, only non-encrypted channels will be "
                "added during the scan."));
    };
};

class TrustEncSISetting : public CheckBoxSetting, public TransientStorage
{
  public:
    TrustEncSISetting() : CheckBoxSetting(this)
    {
        setLabel(QObject::tr("Test Decryptability"));
        setHelpText(
            QObject::tr("Test all channels to see if they can be decrypted "
                        "with installed CAM/smartcard. Sometimes the encrypted "
                        "flag is set spuriously. Attention: Enabling this "
                        "option increases the scan time for each encrypted "
                        "channel by a couple of seconds."));
    }
};

class ScanFrequencykHz: public LineEditSetting, public TransientStorage
{
  public:
    ScanFrequencykHz() : LineEditSetting(this)
    {
        setLabel(QObject::tr("Frequency"));
        setHelpText(QObject::tr("Frequency (Option has no default)\n"
                                "The frequency for this channel in kHz."));
    };
};

class ScanFrequency: public LineEditSetting, public TransientStorage
{
  public:
    ScanFrequency() : LineEditSetting(this)
    {
        setLabel(QObject::tr("Frequency"));
        setHelpText(QObject::tr("Frequency (Option has no default)\n"
                                "The frequency for this channel in Hz."));
    };
};

class ScanSymbolRateDVBS: public ComboBoxSetting, public TransientStorage
{
  public:
    ScanSymbolRateDVBS() : ComboBoxSetting(this, true)
    {
        setLabel(QObject::tr("Symbol Rate"));
        setHelpText(
             QObject::tr(
                "Symbol Rate (symbols/second).\n"
                "Most DVB-S transponders transmit at 27.5 "
                "million symbols per second."));
        addSelection("3333000");
        addSelection("22000000");
        addSelection("27500000", "27500000", true);
        addSelection("28000000");
        addSelection("28500000");
        addSelection("29900000");
    }
};

class ScanSymbolRateDVBC: public ComboBoxSetting, public TransientStorage
{
  public:
    ScanSymbolRateDVBC() : ComboBoxSetting(this, true)
    {
        setLabel(QObject::tr("Symbol Rate"));
        setHelpText(
             QObject::tr(
                "Symbol Rate (symbols/second).\n"
                "Most DVB-C transports transmit at 6.9 or 6.875 "
                "million symbols per second."));
        addSelection("3450000");
        addSelection("5000000");
        addSelection("5900000");
        addSelection("6875000");
        addSelection("6900000", "6900000", true);
        addSelection("6950000");
    }
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
        addSelection("3/5");
        addSelection("9/10");
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

class ScanModSys: public ComboBoxSetting, public TransientStorage
{
    public:
    ScanModSys() : ComboBoxSetting(this)
    {
        setLabel(QObject::tr("Mod Sys"));
        setHelpText(QObject::tr("Modulation system (Default: DVB-S)"));
        addSelection("DVB-S");
        addSelection("DVB-S2");
    };
};

class ScanRollOff: public ComboBoxSetting, public TransientStorage
{
    public:
    ScanRollOff() : ComboBoxSetting(this)
    {
        setLabel(QObject::tr("Roll-off"));
        setHelpText(QObject::tr("Roll-off factor (Default: 0.35)"));
        addSelection("0.35");
        addSelection("0.20");
        addSelection("0.25");
        addSelection(QObject::tr("Auto"),"auto");
    };
};

class PaneError : public HorizontalConfigurationGroup
{
  public:
    PaneError(const QString &error) :
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

#endif // _MISC_SETTINGS_H_
