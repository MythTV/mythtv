/** -*- Mode: c++ -*-
 *  Class ASIChannel
 */

// MythTV includes
#include "mythverbose.h"
#include "mpegtables.h"
#include "asichannel.h"

#define LOC     QString("ASIChan(%1): ").arg(GetDevice())
#define LOC_ERR QString("ASIChan(%1), Error: ").arg(GetDevice())

ASIChannel::ASIChannel(TVRec *parent, const QString &device) :
    DTVChannel(parent), m_device(device), m_isopen(false)
{
    m_tuner_types.push_back(DTVTunerType::kTunerTypeASI);
}

ASIChannel::~ASIChannel(void)
{
    if (IsOpen())
        Close();
}

bool ASIChannel::Open(void)
{
    VERBOSE(VB_CHANNEL, LOC + "Open()");

    if (m_device.isEmpty())
        return false;

    if (m_isopen)
        return true;

    if (!InitializeInputs())
        return false;

    if (m_inputs.find(m_currentInputID) == m_inputs.end())
        return false;

    m_isopen = true;

    return true;
}

void ASIChannel::Close()
{
    VERBOSE(VB_CHANNEL, LOC + "Close()");
    m_isopen = false;
}
