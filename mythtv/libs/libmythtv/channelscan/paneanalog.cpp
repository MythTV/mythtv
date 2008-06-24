/* -*- Mode: c++ -*-
 * vim: set expandtab tabstop=4 shiftwidth=4:
 */

// MythTV headers
#include "paneanalog.h"
#include "videosource.h"

PaneAnalog::PaneAnalog() :
    VerticalConfigurationGroup(false, false, true, false),
    freq_table(new TransFreqTableSelector(0))
{
    addChild(freq_table);
}

void PaneAnalog::SetSourceID(uint sourceid)
{
    freq_table->SetSourceID(sourceid);
}

QString PaneAnalog::GetFrequencyTable(void) const
{
    return freq_table->getValue();
}
