#include "iptvtuningdata.h"

#include "libmythbase/mythlogging.h"

bool IPTVTuningData::IsValid(void) const
{
    bool ret = (m_dataUrl.isValid() && (IsUDP() || IsRTP() || IsRTSP() || IsHLS() || IsHTTPTS()));

    LOG(VB_CHANNEL, LOG_DEBUG, QString("IPTVTuningdata (%1): IsValid = %2")
        .arg(m_dataUrl.toString(),
             ret ? "true" : "false"));

    return ret;
}

bool IPTVTuningData::CanReadHTTP(QByteArray &buffer) const
{
    // Check needed for use in the unit test
    if (QCoreApplication::instance() == nullptr)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("CanReadHTTP - No QCoreApplication!!"));
        return false;
    }

    QString url = m_dataUrl.toString();
    MythSingleDownload downloader;
    downloader.DownloadURL(m_dataUrl, &buffer, 5s, 0, 2000);
    if (buffer.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("CanReadHTTP - Failed, error:%1 url:%2")
            .arg(downloader.ErrorString(), url));
        return false;
    }
    return true;
}
