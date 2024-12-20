#include <chrono> // for milliseconds
#include <thread> // for sleep_for

// MythTV headers
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythversion.h"
#include "httptsstreamhandler.h"

#define LOC QString("HTTPTSSH[%1](%2): ").arg(m_inputId).arg(m_device)

// BUFFER_SIZE is a multiple of TS_SIZE
static constexpr qint64 TS_SIZE     { 188 };
static constexpr qint64 BUFFER_SIZE { TS_SIZE * 512 };

QMap<QString, HTTPTSStreamHandler*> HTTPTSStreamHandler::s_httphandlers;
QMap<QString, uint>                 HTTPTSStreamHandler::s_httphandlers_refcnt;
QMutex                              HTTPTSStreamHandler::s_httphandlers_lock;

HTTPTSStreamHandler* HTTPTSStreamHandler::Get(const IPTVTuningData& tuning,
                                              int inputid)
{
    QMutexLocker locker(&s_httphandlers_lock);

    QString devkey = tuning.GetDeviceKey();

    QMap<QString,HTTPTSStreamHandler*>::iterator it = s_httphandlers.find(devkey);

    if (it == s_httphandlers.end())
    {
        auto* newhandler = new HTTPTSStreamHandler(tuning, inputid);
        newhandler->Start();
        s_httphandlers[devkey] = newhandler;
        s_httphandlers_refcnt[devkey] = 1;

        LOG(VB_RECORD, LOG_INFO,
            QString("HTTPTSSH[%1]: Creating new stream handler %2 for %3")
            .arg(QString::number(inputid), devkey, tuning.GetDeviceName()));
    }
    else
    {
        s_httphandlers_refcnt[devkey]++;
        uint rcount = s_httphandlers_refcnt[devkey];
        LOG(VB_RECORD, LOG_INFO,
            QString("HTTPTSSH[%1]: Using existing stream handler %2 for %3")
            .arg(QString::number(inputid), devkey, tuning.GetDeviceName()) +
            QString(" (%1 in use)").arg(rcount));
    }

    return s_httphandlers[devkey];
}

void HTTPTSStreamHandler::Return(HTTPTSStreamHandler * & ref, int inputid)
{
    QMutexLocker locker(&s_httphandlers_lock);

    QString devname = ref->m_device;

    QMap<QString,uint>::iterator rit = s_httphandlers_refcnt.find(devname);
    if (rit == s_httphandlers_refcnt.end())
        return;

    LOG(VB_RECORD, LOG_INFO, QString("HTTPTSSH[%1]: Return(%2) has %3 handlers")
        .arg(inputid).arg(devname).arg(*rit));

    if (*rit > 1)
    {
        ref = nullptr;
        (*rit)--;
        return;
    }

    QMap<QString,HTTPTSStreamHandler*>::iterator it = s_httphandlers.find(devname);
    if ((it != s_httphandlers.end()) && (*it == ref))
    {
        LOG(VB_RECORD, LOG_INFO, QString("HTTPTSSH[%1]: Closing handler for %2")
            .arg(inputid).arg(devname));
        ref->Stop();
        delete *it;
        s_httphandlers.erase(it);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("HTTPTSSH[%1] Error: Couldn't find handler for %2")
            .arg(inputid).arg(devname));
    }

    s_httphandlers_refcnt.erase(rit);
    ref = nullptr;
}

HTTPTSStreamHandler::HTTPTSStreamHandler(const IPTVTuningData& tuning,
                                         int inputid)
    : IPTVStreamHandler(tuning, inputid)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "ctor");
}

HTTPTSStreamHandler::~HTTPTSStreamHandler(void)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "dtor");
    Stop();
}

void HTTPTSStreamHandler::run(void)
{
    RunProlog();
    std::chrono::milliseconds open_sleep = 250ms;
    LOG(VB_RECORD, LOG_INFO, LOC + "run() -- begin");
    SetRunning(true, false, false);

    m_reader   = new HTTPReader(this);
    while (m_runningDesired)
    {
        if (!m_reader->DownloadStream(m_tuning.GetURL(0)))
        {
            LOG(VB_RECORD, LOG_INFO, LOC + "DownloadStream failed to receive bytes from " + m_tuning.GetURL(0).toString());
            std::this_thread::sleep_for(open_sleep);
            if (open_sleep < 10s)
                open_sleep += 250ms;
            continue;
        }
        open_sleep = 250ms;
    }

    delete m_reader;
    SetRunning(false, false, false);
    RunEpilog();
    LOG(VB_RECORD, LOG_INFO, LOC + "run() -- done");
}


#undef  LOC
#define LOC QString("HTTPReader(%1): ").arg(m_url)

bool HTTPReader::DownloadStream(const QUrl& url)
{
    m_url = url.toString();

    LOG(VB_RECORD, LOG_INFO, LOC + "DownloadStream -- begin");

    QMutexLocker lock(&m_lock);
    QEventLoop   event_loop;

    m_buffer = new uint8_t[BUFFER_SIZE];
    m_size = 0;
    m_ok = false;

    // the HTTP request
    m_replylock.lock();
    QNetworkRequest m_req = QNetworkRequest(url);
    m_req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::NoLessSafeRedirectPolicy);
    m_req.setHeader(QNetworkRequest::UserAgentHeader,
                    QString("MythTV/%1 %2/%3")
                    .arg(MYTH_VERSION_MAJMIN,
                         QSysInfo::productType(),
                         QSysInfo::productVersion()));
    m_reply = m_mgr.get(m_req);
    m_replylock.unlock();

    connect(&m_timer, &QTimer::timeout, &event_loop, &QEventLoop::quit);
    connect(m_reply, &QNetworkReply::finished, &event_loop, &QEventLoop::quit);
    connect(m_reply,&QIODevice::readyRead, this,        &HTTPReader::HttpRead);

    // Configure timeout and size limit
    m_timer.setSingleShot(true);
    m_timer.start(10s);

    event_loop.exec(); // blocks stack until quit() is called

    disconnect(&m_timer, &QTimer::timeout, &event_loop, &QEventLoop::quit);
    disconnect(m_reply, &QNetworkReply::finished, &event_loop, &QEventLoop::quit);
    disconnect(m_reply,&QIODevice::readyRead, this,        &HTTPReader::HttpRead);

    if (m_timer.isActive())
        m_timer.stop();

    QMutexLocker  replylock(&m_replylock);
    if (m_reply->error() != QNetworkReply::NoError)
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "DownloadStream exited with error " +
            QString("%1 '%2'").arg(m_reply->error()).arg(m_reply->errorString()));

        // Download is not OK when there is a network error
        m_ok = false;
    }

    delete m_reply;
    m_reply = nullptr;
    delete[] m_buffer;
    m_buffer=nullptr;

    LOG(VB_RECORD, LOG_INFO, LOC + "DownloadStream -- end");
    return m_ok;
}

void HTTPReader::HttpRead()
{
    m_timer.stop();
    m_ok = true;
    ReadBytes();
    WriteBytes();
}

void HTTPReader::ReadBytes()
{
    QMutexLocker replylock(&m_replylock);
    QMutexLocker bufferlock(&m_bufferlock);

    if(m_reply == nullptr || m_buffer == nullptr || m_size > BUFFER_SIZE)
        return;

    qint64 bytesRead = m_reply->read( reinterpret_cast<char*>(m_buffer + m_size), BUFFER_SIZE - m_size);
    m_size += (bytesRead > 0 ? bytesRead : 0);
    LOG(VB_RECORD, LOG_DEBUG, LOC + QString("ReadBytes: %1 bytes received").arg(bytesRead));
}

void HTTPReader::WriteBytes()
{
    if (m_size < TS_SIZE)
        return;

    QMutexLocker bufferlock(&m_bufferlock);
    int remainder = 0;
    {
        QMutexLocker locker(&m_parent->m_listenerLock);
        for (auto sit = m_parent->m_streamDataList.cbegin();
             sit != m_parent->m_streamDataList.cend(); ++sit)
        {
            remainder = sit.key()->ProcessData(m_buffer, m_size);
        }
    }
    LOG(VB_RECORD, LOG_DEBUG, LOC + QString("WriteBytes: %1/%2 bytes remain").arg(remainder).arg(m_size));

    /* move the remaining data to the beginning of the buffer */
    memmove(m_buffer, m_buffer + (m_size - remainder), remainder);
    m_size = remainder;
}

void HTTPReader::Cancel(void)
{
    QMutexLocker  replylock(&m_replylock);
    if (m_reply)
    {
        LOG(VB_RECORD, LOG_INFO, LOC + "Cancel: Aborting stream download");
        m_reply->abort();
    }
}
