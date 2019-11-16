/** -*- Mode: c++ -*-
 *  Class ASIChannel
 */

// C/C++ includes
#include <utility>

// MythTV includes
#include "mythlogging.h"
#include "mpegtables.h"
#include "asichannel.h"

#define LOC     QString("ASIChan[%1](%2): ").arg(GetInputID()).arg(ASIChannel::GetDevice())

ASIChannel::ASIChannel(TVRec *parent, QString device) :
    DTVChannel(parent), m_device(std::move(device))
{
    m_tuner_types.emplace_back(DTVTunerType::kTunerTypeASI);
}

ASIChannel::~ASIChannel(void)
{
    if (ASIChannel::IsOpen())
        ASIChannel::Close();
}

bool ASIChannel::Open(void)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Open()");

    if (m_device.isEmpty())
        return false;

    if (m_isopen)
        return true;

    if (!InitializeInput())
        return false;

    if (!m_inputid)
        return false;

    m_isopen = true;

    return true;
}

void ASIChannel::Close()
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Close()");
    m_isopen = false;
}
