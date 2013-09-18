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

#ifndef _MODULATION_SETTING_H_
#define _MODULATION_SETTING_H_

// Qt headers
#include <QCoreApplication>

// MythTV headers
#include "settings.h"

class ScanATSCModulation: public ComboBoxSetting, public TransientStorage
{

  public:
    ScanATSCModulation() : ComboBoxSetting(this)
    {
        //: %1 is the modulation (QAM-64, QAM-128. etc...)
        addSelection(QCoreApplication::translate("(ModulationSettings)",
            "Terrestrial %1").arg("(8-VSB)"), "vsb8", true);
        //: %1 is the modulation (QAM-64, QAM-128. etc...)
        addSelection(QCoreApplication::translate("(ModulationSettings)",
            "Cable %1").arg("(QAM-256)"), "qam256", false);
        addSelection(QCoreApplication::translate("(ModulationSettings)",
            "Cable %1").arg("(QAM-128)"), "qam128", false);
        addSelection(QCoreApplication::translate("(ModulationSettings)",
            "Cable %1").arg("(QAM-64)"), "qam64", false);

        setLabel(QCoreApplication::translate("(ModulationSettings)",
                                             "Modulation"));

        setHelpText(QCoreApplication::translate("(ModulationSettings)",
            "Modulation, 8-VSB, QAM-256, etc. Most cable systems in the "
            "United States use QAM-256 or QAM-64, but some mixed systems "
            "may use 8-VSB for over-the-air channels."));
    }
};

class ScanModulationSetting: public ComboBoxSetting
{
  public:
    ScanModulationSetting(Storage *_storage) : ComboBoxSetting(_storage)
    {
        addSelection(QCoreApplication::translate("(Common)",
                                                 "Auto",
                                                 "Automatic"),
                                                 "auto", true);
        addSelection("QPSK","qpsk");
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
        setLabel(QCoreApplication::translate("(ModulationSettings)",
                                             "Modulation"));

        setHelpText(QCoreApplication::translate("(ModulationSettings)",
            "Modulation (Default: Auto)"));
    };
};

class ScanConstellation: public ScanModulationSetting,
                         public TransientStorage
{
  public:
    ScanConstellation() : ScanModulationSetting(this)
    {
        setLabel(QCoreApplication::translate("(ModulationSettings)",
                                             "Constellation"));

        setHelpText(QCoreApplication::translate("(ModulationSettings)",
            "Constellation (Default: Auto)"));
    };
};

class ScanDVBSModulation: public ComboBoxSetting, public TransientStorage
{
  public:
    ScanDVBSModulation() : ComboBoxSetting(this)
    {

        addSelection("QPSK",  "qpsk", true);
        addSelection("8PSK",  "8psk");
        addSelection("QAM 16","qam_16");

        setLabel(QCoreApplication::translate("(ModulationSettings)",
                                             "Modulation"));

        setHelpText(QCoreApplication::translate("(ModulationSettings)",
            "Modulation, QPSK, 8PSK, QAM-16. Most DVB-S transponders use QPSK, "
            "while DVB-S2 use 8PSK. QAM-16 is not available for DVB-S2 "
            "transports."));
    }
};

#endif // _MODULATION_SETTING_H_
