/** -*- Mode: c++ -*-
 *  Class ASIChannel
 */

// C/C++ includes
#include <utility>

// MythTV includes
#include "libmythbase/mythlogging.h"

#include "asichannel.h"
#include "mpeg/mpegtables.h"

#define LOC     QString("ASIChan[%1](%2): ").arg(GetInputID()).arg(ASIChannel::GetDevice())

ASIChannel::ASIChannel(TVRec *parent, QString device) :
    DTVChannel(parent), m_device(std::move(device))
{
    m_tunerTypes.emplace_back(DTVTunerType::kTunerTypeASI);
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

    if (m_isOpen)
        return true;

    if (!InitializeInput())
        return false;

    if (!m_inputId)
        return false;

    m_isOpen = true;

    return true;
}

void ASIChannel::Close()
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Close()");
    m_isOpen = false;
}
