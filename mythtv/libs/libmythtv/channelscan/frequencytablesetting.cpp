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

#include "frequencytablesetting.h"

// libmythbase headers
#include "mythcorecontext.h"
#include "mythlocale.h"

ScanFrequencyTable::ScanFrequencyTable() : ComboBoxSetting(this)
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

ScanCountry::ScanCountry() : ComboBoxSetting(this)
{
    QString country = "au"; // Default to first in list
    country = gCoreContext->GetLocale()->GetCountryCode().toLower();

    setLabel(QObject::tr("Country"));
    addSelection(QObject::tr("Australia"),      "au", country == "au");
    addSelection(QObject::tr("Chile"),          "cl", country == "cl");
    addSelection(QObject::tr("Czech Republic"), "cz", country == "cz");
    addSelection(QObject::tr("Denmark"),        "dk", country == "dk");
    addSelection(QObject::tr("Finland"),        "fi", country == "fi");
    addSelection(QObject::tr("France"),         "fr", country == "fr");
    addSelection(QObject::tr("Germany"),        "de", country == "de");
    addSelection(QObject::tr("Greece"),         "gr", country == "gr");
    addSelection(QObject::tr("Israel"),         "il", country == "il");
    addSelection(QObject::tr("Italy"),          "it", country == "it");
    addSelection(QObject::tr("New Zealand"),    "nz", country == "nz");
    addSelection(QObject::tr("Spain"),          "es", country == "es");
    addSelection(QObject::tr("Sweden"),         "se", country == "se");
    addSelection(QObject::tr("United Kingdom"), "gb", country == "gb");
}


ScanNetwork::ScanNetwork() : ComboBoxSetting(this)
{
    QString country = "de"; // Default to first in list
    country = gCoreContext->GetLocale()->GetCountryCode().toLower();

    setLabel(QObject::tr("Country"));
    addSelection(QObject::tr("Germany"),        "de", country == "de");
    addSelection(QObject::tr("United Kingdom"), "gb", country == "gb");
}

