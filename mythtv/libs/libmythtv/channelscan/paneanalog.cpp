/* -*- Mode: c++ -*-
 * vim: set expandtab tabstop=4 shiftwidth=4:
 */

// MythTV headers
#include "paneanalog.h"
#include "videosource.h"

PaneAnalog::PaneAnalog(const QString &target, StandardSetting *setting) :
    m_freq_table(new TransFreqTableSelector(0))
{
    setVisible(false);
    setting->addTargetedChildren(target, {this, m_freq_table});
}

void PaneAnalog::SetSourceID(uint sourceid)
{
    m_freq_table->SetSourceID(sourceid);
}

QString PaneAnalog::GetFrequencyTable(void) const
{
    return m_freq_table->getValue();
}
