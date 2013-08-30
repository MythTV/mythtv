/* -*- Mode: c++ -*-
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 *
 */

#ifndef SCANWIZARD_H
#define SCANWIZARD_H

// MythTV headers
#include "mythtvexp.h"
#include "mythdbcon.h"
#include "mythwizard.h"
#include "settings.h"

class ScanWizardConfig;
class ChannelScannerGUI;

class MTV_PUBLIC ScanWizard : public QObject, public ConfigurationWizard
{
    Q_OBJECT

  public:
    ScanWizard(uint    default_sourceid  = 0,
               uint    default_cardid    = 0,
               QString default_inputname = QString::null);

    MythDialog *dialogWidget(MythMainWindow *parent, const char *widgetName);

  protected slots:
    void SetPage(const QString &pageTitle);
    void SetInput(const QString &cardid_inputname);

  protected:
    ~ScanWizard() { }

  protected:
    uint               lastHWCardID;
    uint               lastHWCardType;
    ScanWizardConfig  *configPane;
    ChannelScannerGUI *scannerPane;
};

#endif // SCANWIZARD_H
