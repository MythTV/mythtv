/* -*- Mode: c++ -*-
 * vim: set expandtab tabstop=4 shiftwidth=4:
 */

// MythTV headers
#include "paneanalog.h"
#include "videosource.h"

PaneAnalog::PaneAnalog(const QString &target, StandardSetting *setting) :
    m_freqTable(new TransFreqTableSelector(0))
{
    setVisible(false);
    setting->addTargetedChildren(target, {this, m_freqTable});
}

void PaneAnalog::SetSourceID(uint sourceid)
{
    m_freqTable->SetSourceID(sourceid);
}

QString PaneAnalog::GetFrequencyTable(void) const
{
    return m_freqTable->getValue();
}
