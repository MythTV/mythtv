
#include "mythsingledownload.h"
#include "mythversion.h"
#include "mythlogging.h"

/*
 * For simple one-at-a-time downloads, this routine (found at
 * http://www.insanefactory.com/2012/08/qt-http-request-in-a-single-function/
 * ) works well.
 */

#define LOC QString("MythSingleDownload: ")

bool MythSingleDownload::DownloadURL(const QUrl &url, QByteArray *buffer,
                                     std::chrono::seconds timeout,
                                     uint redirs, qint64 maxsize, QString *final_url)
{
    m_lock.lock();

    QEventLoop   event_loop;
    m_buffer = buffer;

    // the HTTP request
    QNetworkRequest req(url);
    req.setRawHeader("User-Agent",
                     "MythTV v" + QByteArray(MYTH_BINARY_VERSION) + " MythSingleDownload");
    m_replylock.lock();
    m_reply = m_mgr.get(req);
    m_replylock.unlock();

    req.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                     QNetworkRequest::AlwaysNetwork);

    // "quit()" the event-loop, when the network request "finished()"
    connect(&m_timer, &QTimer::timeout, &event_loop, &QEventLoop::quit);
    connect(m_reply, &QNetworkReply::finished, &event_loop, &QEventLoop::quit);
    connect(m_reply, &QNetworkReply::downloadProgress, this, &MythSingleDownload::Progress);

    // Configure timeout and size limit
    m_maxsize = maxsize;
    m_timer.setSingleShot(true);
    m_timer.start(timeout);  // 30 secs. by default

    bool ret = event_loop.exec() != 0; // blocks stack until quit() is called

    disconnect(&m_timer, &QTimer::timeout, &event_loop, &QEventLoop::quit);
    disconnect(m_reply, &QNetworkReply::finished, &event_loop, &QEventLoop::quit);
    disconnect(m_reply, &QNetworkReply::downloadProgress, this, &MythSingleDownload::Progress);

    if (ret)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "eventloop failed");
    }

    m_replylock.lock();
    if (!m_timer.isActive())
    {
        m_errorstring = "timed-out";
        m_reply->abort();
        ret = false;
    }
    else
    {
        m_timer.stop();
        m_errorcode = m_reply->error();

        QString redir = m_reply->attribute(
            QNetworkRequest::RedirectionTargetAttribute).toUrl().toString();

        if (!redir.isEmpty())
        {
            if (redirs <= 3)
            {
                LOG(VB_GENERAL, LOG_INFO, QString("%1 -> %2").arg(url.toString(), redir));
                m_replylock.unlock();
                m_lock.unlock();
                return DownloadURL(redir, buffer, timeout, redirs + 1, maxsize, final_url);
            }

            LOG(VB_GENERAL, LOG_ERR, QString("%1: too many redirects").arg(url.toString()));
            ret = false;
        }
        else if (m_errorcode == QNetworkReply::NoError)
        {
            if (final_url != nullptr)
                *final_url = url.toString();
            *m_buffer += m_reply->readAll();
            m_errorstring.clear();
            ret = true;
        }
        else
        {
            m_errorstring = m_reply->errorString();
            ret = false;
        }
    }

    m_replylock.unlock();
    m_lock.unlock();

    delete m_reply;
    m_reply = nullptr;
    m_buffer = nullptr;

    return ret;
}

void MythSingleDownload::Cancel(void)
{
    QMutexLocker  replylock(&m_replylock);
    if (m_reply)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "Aborting download");
        m_reply->abort();
    }
}

void MythSingleDownload::Progress(qint64 bytesRead, qint64 /*totalBytes*/)
{
    if (m_maxsize && bytesRead>=m_maxsize)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Reached specified max file size (%1 bytes)").arg(m_maxsize));
        {
            QMutexLocker  replylock(&m_replylock);
            *m_buffer += m_reply->read(m_maxsize);
        }
        m_maxsize=0;
        Cancel();
    }
}
