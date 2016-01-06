/* -*- Mode: c++ -*-
 * vim: set expandtab tabstop=4 shiftwidth=4:
 */

// MythTV headers
#include "paneanalog.h"
#include "videosource.h"

PaneAnalog::PaneAnalog(const QString &target, StandardSetting *setting) :
    freq_table(new TransFreqTableSelector(0))
{
    setVisible(false);
    setting->addTargetedChildren(target, {this, freq_table});
}

void PaneAnalog::SetSourceID(uint sourceid)
{
    freq_table->SetSourceID(sourceid);
}

QString PaneAnalog::GetFrequencyTable(void) const
{
    return freq_table->getValue();
}
