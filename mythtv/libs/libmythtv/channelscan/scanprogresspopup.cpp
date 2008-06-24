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

#include "scanprogresspopup.h"

ScanProgressPopup::ScanProgressPopup(bool lock, bool strength,
                                     bool snr, bool rotorpos) :
    ConfigurationPopupDialog(),
    done(false), ss(NULL), sn(NULL), progressBar(NULL), sl(NULL), sta(NULL)
{
    setLabel(tr("Scan Progress"));

    addChild(sta = new TransLabelSetting());
    sta->setLabel(tr("Status"));
    sta->setValue(tr("Tuning"));

    if (rotorpos)
    {
        addChild(pos = new ScanSignalMeter(65535));
        pos->setLabel(tr("Rotor Movement"));
    }

    if (lock)
    {
        addChild(sl = new TransLabelSetting());
        sl->setValue("                                  "
                     "                                  ");
    }

    if (strength)
    {
        addChild(ss = new ScanSignalMeter(65535));
        ss->setLabel(tr("Signal Strength"));
    }

    if (snr)
    {
        addChild(sn = new ScanSignalMeter(65535));
        sn->setLabel(tr("Signal/Noise"));
    }

    addChild(progressBar = new ScanSignalMeter(65535));
    progressBar->setValue(0);
    progressBar->setLabel(tr("Scan"));


    TransButtonSetting *cancel = new TransButtonSetting();
    cancel->setLabel(tr("Cancel"));
    addChild(cancel);

    connect(cancel, SIGNAL(pressed(void)),
            this,   SLOT(  reject(void)));

    //Seem to need to do this as the constructor doesn't seem enough
    setUseLabel(false);
    setUseFrame(false);
}

ScanProgressPopup::~ScanProgressPopup()
{
}

void ScanProgressPopup::SetStatusRotorPosition(int value)
{
    if (pos)
        pos->setValue(value);
}

void ScanProgressPopup::SetStatusSignalToNoise(int value)
{
    if (sn)
        sn->setValue(value);
}

void ScanProgressPopup::SetStatusSignalStrength(int value)
{
    if (ss)
        ss->setValue(value);
}

void ScanProgressPopup::SetStatusLock(int value)
{
    if (sl)
        sl->setValue((value) ? tr("Locked") : tr("No Lock"));
}

void ScanProgressPopup::SetStatusText(const QString &value)
{
    if (sta)
        sta->setValue(value);
}

void ScanProgressPopup::SetStatusTitleText(const QString &value)
{
    QString msg = tr("Scan Progress") + QString(" %1").arg(value);
    setLabel(msg);
}

void ScanProgressPopup::deleteLater(void)
{
    disconnect();
    if (dialog)
    {
        VERBOSE(VB_IMPORTANT, "Programmer Error: "
                "ScanProgressPopup::DeleteDialog() never called.");
    }
    ConfigurationPopupDialog::deleteLater();
}

void ScanProgressPopup::CreateDialog(void)
{
    if (!dialog)
    {
        dialog = (ConfigPopupDialogWidget*)
            dialogWidget(gContext->GetMainWindow(),
                         "ConfigurationPopupDialog");
        dialog->ShowPopup(this, SLOT(Done(void)));
    }
}

void ScanProgressPopup::DeleteDialog(void)
{
    if (dialog)
    {
        dialog->deleteLater();
        dialog = NULL;
    }
}

DialogCode ScanProgressPopup::exec(void)
{
    if (!dialog)
    {
        VERBOSE(VB_IMPORTANT, "Programmer Error: "
                "ScanProgressPopup::CreateDialog() must be called before exec");
        return kDialogCodeRejected;
    }

    dialog->setResult(kDialogCodeRejected);

    done = false;

    // Qt4 requires a QMutex as a parameter...
    // not sure if this is the best solution.  Mutex Must be locked before wait.
    QMutex mutex;
    QMutexLocker locker(&mutex);

    while (!done)
        wait.wait(&mutex, 100);

    return dialog->result();
}

void ScanProgressPopup::Done(void)
{
    done = true;
    wait.wakeAll();
}

