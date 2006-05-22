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

#include "scanwizardhelpers.h"
#include "scanwizardscanner.h"
#include "scanwizard.h"
#include "sourceutil.h"
#include "cardutil.h"

ScanWizard::ScanWizard(int sourceid)
    : paneOFDM(new OFDMPane()),     paneQPSK(new QPSKPane()),
      paneATSC(new ATSCPane()),     paneQAM(new QAMPane()),
      paneSingle(new STPane()),
#ifdef FE_GET_EXTENDED_INFO
      paneDVBS2(new DVBS2Pane()),
#endif
      nVideoDev(-1),                nCardType(CardUtil::ERROR_PROBE),
      nCaptureCard(-1),
      configPane(new ScanWizardScanType(this, sourceid)),
      scannerPane(new ScanWizardScanner(this))
{
    addChild(configPane);
    addChild(scannerPane);
}

MythDialog* ScanWizard::dialogWidget(MythMainWindow *parent,
                                     const char *widgetName)
{
    MythWizard* wizard = (MythWizard*)
        ConfigurationWizard::dialogWidget(parent, widgetName);

    connect(wizard, SIGNAL(selected(    const QString&)),
            this,   SLOT(  pageSelected(const QString&)));

    return wizard;
}

void ScanWizard::pageSelected(const QString& strTitle)
{
    if (strTitle == ScanWizardScanner::strTitle)
       scannerPane->scan();
}

void ScanWizard::captureCard(const QString &str)
{
    int new_cardid = str.toUInt();
    //Work out what kind of card we've got
    //We need to check against the last capture card so that we don't
    //try and probe a card which is already open by scan()
    if ((nCaptureCard != new_cardid) ||
        (nCardType == CardUtil::ERROR_OPEN))
    {
        nCaptureCard    = new_cardid;
        QString subtype = CardUtil::ProbeSubTypeName(nCaptureCard, 0);
        nCardType       = CardUtil::toCardType(subtype);
        QString fmt     = SourceUtil::GetChannelFormat(videoSource());
        paneATSC->SetDefaultFormat(fmt);
        emit cardTypeChanged(QString::number(nCardType));
    }
}

uint ScanWizard::videoSource(void) const
{
    return configPane->videoSource->getValue().toInt();
}

int ScanWizard::captureCard(void) const
{
    return configPane->capturecard->getValue().toInt();
}

int ScanWizard::scanType(void) const
{
    return configPane->scanType->getValue().toInt();
}

bool ScanWizard::ignoreSignalTimeout(void) const
{
    bool ts0 = (ScanTypeSetting::TransportScan == scanType());
    bool vl0 = paneSingle->ignoreSignalTimeout();

    bool ts1 = (ScanTypeSetting::FullTransportScan == scanType());
    bool vl1 = (configPane->scanConfig->
                ignoreSignalTimeoutAll->getValue().toInt());

    return (ts0) ? vl0 : ((ts1) ? vl1 : false);
}

QString ScanWizard::country(void) const
{
    return configPane->scanConfig->country->getValue();
}

QString ScanWizard::filename(void) const
{
    return configPane->scanConfig->filename->getValue();
}
