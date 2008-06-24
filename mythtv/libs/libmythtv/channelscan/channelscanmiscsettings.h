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

#ifndef _MISC_SETTINGS_H_
#define _MISC_SETTINGS_H_

#include "settings.h"

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
