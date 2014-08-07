/** -*- Mode: c++ -*-
 *  Class ExternalChannel
 */

// MythTV includes
#include "mythlogging.h"
#include "mpegtables.h"
#include "ExternalChannel.h"
#include "tv_rec.h"

#define LOC  QString("ExternChan[%1](%2): ").arg(GetCardID()).arg(GetDevice())

ExternalChannel::ExternalChannel(TVRec *parent, const QString & device) :
    DTVChannel(parent), m_device(device), m_stream_handler(0)
{
}

ExternalChannel::~ExternalChannel(void)
{
    if (IsOpen())
        Close();
}

bool ExternalChannel::Open(void)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Open()");

    if (m_device.isEmpty())
        return false;

    if (IsOpen())
    {
        if (m_stream_handler->IsAppOpen())
            return true;

        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Valid stream handler, but app is not open!  Resetting."));
        Close();
    }


    if (!InitializeInputs())
        return false;

    if (m_inputs.find(m_currentInputID) == m_inputs.end())
        return false;

    m_stream_handler = ExternalStreamHandler::Get(m_device);
    if (!m_stream_handler || m_stream_handler->HasError())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Open failed");
        return false;
    }

    LOG(VB_RECORD, LOG_INFO, LOC + "Opened");
    return true;
}

void ExternalChannel::Close()
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Close()");

    if (IsOpen())
    {
        ExternalStreamHandler::Return(m_stream_handler);
        m_stream_handler = 0;
    }
}

bool ExternalChannel::Tune(const QString &channum)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("Tune(%1)").arg(channum));

    if (!IsOpen())
    {
        LOG(VB_CHANNEL, LOG_ERR, LOC + "Tune failed, not open");
        return false;
    }

    if (!m_stream_handler->HasTuner())
        return true;

    QString result;

    LOG(VB_CHANNEL, LOG_INFO, LOC + "Tuning to " + channum);

    if (!m_stream_handler->ProcessCommand("TuneChannel " + channum, 5000, result))
    {
        LOG(VB_CHANNEL, LOG_ERR, LOC + QString
            ("Failed to Tune %1: %2").arg(channum).arg(result));
        return false;
    }

    return true;
}

bool ExternalChannel::EnterPowerSavingMode(void)
{
    Close();
    return true;
}
