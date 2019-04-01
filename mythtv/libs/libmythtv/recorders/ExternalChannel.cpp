/** -*- Mode: c++ -*-
 *  Class ExternalChannel
 */

// MythTV includes
#include "mythlogging.h"
#include "mpegtables.h"
#include "ExternalChannel.h"
#include "tv_rec.h"

#define LOC  QString("ExternChan[%1](%2): ").arg(m_inputid).arg(m_loc)

ExternalChannel::~ExternalChannel(void)
{
    if (ExternalChannel::IsOpen())
        ExternalChannel::Close();
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


    if (!InitializeInput())
        return false;

    if (!m_inputid)
        return false;

    m_stream_handler = ExternalStreamHandler::Get(m_device, GetInputID(),
                                                  GetMajorID());
    if (!m_stream_handler || m_stream_handler->HasError())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Open failed");
        return false;
    }

    GetDescription();
    LOG(VB_RECORD, LOG_INFO, LOC + "Opened");
    return true;
}

void ExternalChannel::Close()
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Close()");

    if (ExternalChannel::IsOpen())
    {
        ExternalStreamHandler::Return(m_stream_handler, GetInputID());
        m_stream_handler = nullptr;
    }
}

QString ExternalChannel::UpdateDescription(void)
{
    if (m_stream_handler)
        m_loc = m_stream_handler->UpdateDescription();
    else
        m_loc = GetDevice();

    return m_loc;
}

QString ExternalChannel::GetDescription(void)
{
    if (m_stream_handler)
        m_loc = m_stream_handler->GetDescription();
    else
        m_loc = GetDevice();

    return m_loc;
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

    if (!m_stream_handler->ProcessCommand("TuneChannel:" + channum, result,
                                          20000))
    {
        LOG(VB_CHANNEL, LOG_ERR, LOC + QString
            ("Failed to Tune %1: %2").arg(channum).arg(result));
        return false;
    }

    UpdateDescription();

    return true;
}

bool ExternalChannel::Tune(const QString &freqid, int /*finetune*/)
{
    return ExternalChannel::Tune(freqid);
}

bool ExternalChannel::EnterPowerSavingMode(void)
{
    Close();
    return true;
}
