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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 *
 */

#ifndef SCANWIZARD_H
#define SCANWIZARD_H

// MythTV headers
#include "mythdbcon.h"
#include "mythwizard.h"
#include "settings.h"

class OFDMPane;
class QPSKPane;
class ATSCPane;
class QAMPane;
class STPane;

class ScanWizard : public ConfigurationWizard 
{
    Q_OBJECT
    friend class ScanWizardScanType;
    friend class ScanWizardScanner;
    friend class ScanOptionalConfig;

  public:
    ScanWizard(int sourceid = -1);

    MythDialog *dialogWidget(MythMainWindow *parent, const char *widgetName);

  protected slots:
    void pageSelected(const QString &strTitle);
    void captureCard(const QString &device);

  signals:
    void cardTypeChanged(const QString&);

  protected:
    uint videoSource(void) const;
    int captureCard(void) const;
    int scanType(void) const;
    bool ignoreSignalTimeout(void) const;
    QString country(void) const;
    QString filename(void) const;

    OFDMPane     *paneOFDM;
    QPSKPane     *paneQPSK;
    ATSCPane     *paneATSC;
    QAMPane      *paneQAM;
    STPane       *paneSingle;
    int           nVideoDev;
    unsigned      nCardType;
    int           nCaptureCard;
    ScanWizardScanType *configPane;
    ScanWizardScanner  *scannerPane;
};


#endif // SCANWIZARD_H
