
#include "mythsingledownload.h"
#include "mythlogging.h"

/*
 * For simple one-at-a-time downloads, this routine (found at
 * http://www.insanefactory.com/2012/08/qt-http-request-in-a-single-function/
 * ) works well.
 */

bool MythSingleDownload::DownloadURL(const QString &url, QByteArray *buffer,
                                     uint timeout)
{
    QMutexLocker  lock(&m_lock);

    // create custom temporary event loop on stack
    QEventLoop   event_loop;

    // the HTTP request
    QNetworkRequest req(url);
    m_replylock.lock();
    m_reply = m_mgr.get(req);
    m_replylock.unlock();

    // "quit()" the event-loop, when the network request "finished()"
    connect(m_reply, SIGNAL(finished()), &event_loop, SLOT(quit()));
    connect(&m_timer, SIGNAL(timeout()), &event_loop, SLOT(quit()));

    // Configure timeout
    m_timer.setSingleShot(true);
    m_timer.start(timeout);  // 30 secs. by default

    bool ret = event_loop.exec(); // blocks stack until quit() is called

    disconnect(&m_timer, SIGNAL(timeout()), &event_loop, SLOT(quit()));
    disconnect(m_reply, SIGNAL(finished()), &event_loop, SLOT(quit()));

    if (ret != 0)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("MythSingleDownload evenloop failed"));
    }

    QMutexLocker  replylock(&m_replylock);
    if (m_timer.isActive())
    {
        m_timer.stop();
        if (m_reply->error() == QNetworkReply::NoError)
        {
            *buffer += m_reply->readAll();
            delete m_reply;
            m_reply = NULL;
            return true;
        }
        else
        {
            LOG(VB_GENERAL, LOG_WARNING, QString("MythSingleDownload: %1")
                .arg(m_reply->errorString()));
            delete m_reply;
            m_reply = NULL;
            return false;
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, "MythSingleDownload: timed-out");
        m_timer.stop();
        m_reply->abort();
        delete m_reply;
        m_reply = NULL;
        return false;
    }
}

void MythSingleDownload::Cancel(void)
{
    QMutexLocker  replylock(&m_replylock);
    if (m_reply)
    {
        LOG(VB_GENERAL, LOG_INFO, "MythSingleDownload: Aborting download");
        m_reply->abort();
    }
}
