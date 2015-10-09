// MythTV headers
#include "httptsstreamhandler.h"
#include "mythlogging.h"

// POSIX headers
#include <unistd.h> // for usleep

#define LOC QString("HTTPTSSH(%1): ").arg(_device)

// BUFFER_SIZE is a multiple of TS_SIZE
#define TS_SIZE     188
#define BUFFER_SIZE (512 * TS_SIZE)

QMap<QString,HTTPTSStreamHandler*>  HTTPTSStreamHandler::s_handlers;
QMap<QString,uint>               HTTPTSStreamHandler::s_handlers_refcnt;
QMutex                           HTTPTSStreamHandler::s_handlers_lock;

HTTPTSStreamHandler* HTTPTSStreamHandler::Get(const IPTVTuningData& tuning)
{
    QMutexLocker locker(&s_handlers_lock);

    QString devkey = tuning.GetDeviceKey();

    QMap<QString,HTTPTSStreamHandler*>::iterator it = s_handlers.find(devkey);

    if (it == s_handlers.end())
    {
        HTTPTSStreamHandler* newhandler = new HTTPTSStreamHandler(tuning);
        newhandler->Start();
        s_handlers[devkey] = newhandler;
        s_handlers_refcnt[devkey] = 1;

        LOG(VB_RECORD, LOG_INFO,
            QString("HTTPTSSH: Creating new stream handler %1 for %2")
            .arg(devkey).arg(tuning.GetDeviceName()));
    }
    else
    {
        s_handlers_refcnt[devkey]++;
        uint rcount = s_handlers_refcnt[devkey];
        LOG(VB_RECORD, LOG_INFO,
            QString("HTTPTSSH: Using existing stream handler %1 for %2")
            .arg(devkey).arg(tuning.GetDeviceName()) +
            QString(" (%1 in use)").arg(rcount));
    }

    return s_handlers[devkey];
}

HTTPTSStreamHandler::HTTPTSStreamHandler(const IPTVTuningData& tuning) :
    IPTVStreamHandler(tuning)
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
    int open_sleep = 250000;
    LOG(VB_RECORD, LOG_INFO, LOC + "run() -- begin");
    SetRunning(true, false, false);

    m_reader   = new HTTPReader(this);
    while (_running_desired)
    {
        if (!m_reader->DownloadStream(m_tuning.GetURL(0)))
        {
            LOG(VB_RECORD, LOG_INFO, LOC + "DownloadStream failed to recieve bytes from " + m_tuning.GetURL(0).toString());
            usleep(open_sleep);
            if (open_sleep < 10000000)
                open_sleep += 250000;
            continue;
        }
        open_sleep = 250000;
    }

    delete m_reader;
    SetRunning(false, false, false);
    RunEpilog();
    LOG(VB_RECORD, LOG_INFO, LOC + "run() -- done");
}


#undef  LOC
#define LOC QString("HTTPReader(%1): ").arg(m_parent->_device)

bool HTTPReader::DownloadStream(const QUrl url)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "DownloadStream -- begin");

    QMutexLocker lock(&m_lock);
    QEventLoop   event_loop;

    m_buffer = new uint8_t[BUFFER_SIZE];
    size = 0;
    m_ok = false;

    // the HTTP request
    m_replylock.lock();
    m_reply = m_mgr.get(QNetworkRequest(url));
    m_replylock.unlock();

    connect(&m_timer, SIGNAL(timeout()), &event_loop, SLOT(quit()));
    connect(m_reply, SIGNAL(finished()), &event_loop, SLOT(quit()));
    connect(m_reply,SIGNAL(readyRead()), this,        SLOT(HttpRead()));

    // Configure timeout and size limit
    m_timer.setSingleShot(true);
    m_timer.start(10000);

    event_loop.exec(); // blocks stack until quit() is called

    disconnect(&m_timer, SIGNAL(timeout()), &event_loop, SLOT(quit()));
    disconnect(m_reply, SIGNAL(finished()), &event_loop, SLOT(quit()));
    disconnect(m_reply,SIGNAL(readyRead()), this,        SLOT(HttpRead()));

    if (m_timer.isActive())
        m_timer.stop();

    QMutexLocker  replylock(&m_replylock);
    if (!m_reply->error() == QNetworkReply::NoError)
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "DownloadStream exited with " + m_reply->errorString());
    }

    delete m_reply;
    m_reply = NULL;
    delete[] m_buffer;
    m_buffer=NULL;

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

    if(m_reply == NULL || m_buffer == NULL || size > BUFFER_SIZE)
        return;

    qint64 bytesRead = m_reply->read( reinterpret_cast<char*>(m_buffer+size), BUFFER_SIZE-size);
    size += (bytesRead > 0 ? bytesRead : 0);
    LOG(VB_RECORD, LOG_DEBUG, LOC + QString("ReadBytes: %1 bytes recieved").arg(bytesRead));
}

void HTTPReader::WriteBytes()
{
    if (size < TS_SIZE)
        return;

    QMutexLocker  replylock(&m_bufferlock);
    int remainder = 0;
    {
        QMutexLocker locker(&m_parent->_listener_lock);
        HTTPTSStreamHandler::StreamDataList::const_iterator sit;
        sit = m_parent->_stream_data_list.begin();
        for (; sit != m_parent->_stream_data_list.end(); ++sit)
        {
            remainder = sit.key()->ProcessData(m_buffer, size);
        }
    }
    LOG(VB_RECORD, LOG_DEBUG, LOC + QString("WriteBytes: %1/%2 bytes remain").arg(remainder).arg(size));
    memcpy(m_buffer, m_buffer+(size-remainder), remainder);
    size = remainder;
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
