/** -*- Mode: c++ -*-
 *  Class ASIChannel
 */

// MythTV includes
#include "mythlogging.h"
#include "mpegtables.h"
#include "asichannel.h"

#define LOC     QString("ASIChan[%1](%2): ").arg(GetCardID()).arg(GetDevice())

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
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Open()");

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
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Close()");
    m_isopen = false;
}
