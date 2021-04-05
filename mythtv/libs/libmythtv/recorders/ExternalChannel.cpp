/** -*- Mode: c++ -*-
 *  Class ExternalChannel
 */

// MythTV includes
#include "mythlogging.h"
#include "mpegtables.h"
#include "ExternalChannel.h"
#include "tv_rec.h"

#define LOC  QString("ExternChan[%1](%2): ").arg(m_inputId).arg(m_loc)

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
        if (m_streamHandler->IsAppOpen())
            return true;

        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Valid stream handler, but app is not open!  Resetting."));
        Close();
    }


    if (!InitializeInput())
        return false;

    if (!m_inputId)
        return false;

    m_streamHandler = ExternalStreamHandler::Get(m_device, GetInputID(),
                                                  GetMajorID());
    if (!m_streamHandler || m_streamHandler->HasError())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Open failed");
        Close();
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
        ExternalStreamHandler::Return(m_streamHandler, GetInputID());
        m_streamHandler = nullptr;
    }
}

QString ExternalChannel::UpdateDescription(void)
{
    if (m_streamHandler)
        m_loc = m_streamHandler->UpdateDescription();
    else
        m_loc = GetDevice();

    return m_loc;
}

QString ExternalChannel::GetDescription(void)
{
    if (m_streamHandler)
        m_loc = m_streamHandler->GetDescription();
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

    if (!m_streamHandler->HasTuner())
        return true;

    QString result;
    if (m_tuneTimeout < 0ms)
    {
        if (!m_streamHandler->ProcessCommand("LockTimeout?", result))
        {
            LOG(VB_CHANNEL, LOG_ERR, LOC + QString
                ("Failed to retrieve LockTimeout: %1").arg(result));
            m_tuneTimeout = 60s;
        }
        else
            m_tuneTimeout = std::chrono::milliseconds(result.split(":")[1].toInt());

        LOG(VB_CHANNEL, LOG_INFO, LOC + QString("Using Tune timeout of %1ms")
            .arg(m_tuneTimeout.count()));
    }

    LOG(VB_CHANNEL, LOG_INFO, LOC + "Tuning to " + channum);

    if (!m_streamHandler->ProcessCommand("TuneChannel:" + channum,
                                         result, m_tuneTimeout))
    {
        LOG(VB_CHANNEL, LOG_ERR, LOC + QString
            ("Failed to Tune %1: %2").arg(channum, result));
        return false;
    }

    UpdateDescription();
    m_backgroundTuning = result.startsWith("OK:InProgress");

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

uint ExternalChannel::GetTuneStatus(void)
{

    if (!m_backgroundTuning)
        return 3;

    LOG(VB_CHANNEL, LOG_DEBUG, LOC + QString("GetScriptStatus() %1")
        .arg(m_systemStatus));

    QString result;
    int     ret = 0;

    if (!m_streamHandler->ProcessCommand("TuneStatus?", result))
    {
        LOG(VB_CHANNEL, LOG_ERR, LOC + QString
            ("Failed to Tune: %1").arg(result));
        ret = 2;
        m_backgroundTuning = false;
    }
    else
    {
        if (result.startsWith("OK:InProgress"))
            ret = 1;
        else
        {
            ret = 3;
            m_backgroundTuning = false;
            UpdateDescription();
        }
    }

    LOG(VB_CHANNEL, LOG_DEBUG, LOC + QString("GetScriptStatus() %1 -> %2")
        .arg(m_systemStatus). arg(ret));

    return ret;
}
