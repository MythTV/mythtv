/*
    zmclient.cpp
*/

#include <unistd.h>

// qt
#include <QTimer>

//myth
#include "mythcontext.h"
#include "mythdialogbox.h"
#include <mythdate.h>
#include "mythmainwindow.h"
#include "mythlogging.h"

//zoneminder
#include "zmclient.h"

// the protocol version we understand
#define ZM_PROTOCOL_VERSION "8"

#define BUFFER_SIZE  (2048*1536*3)

ZMClient::ZMClient()
    : QObject(NULL),
      m_socket(NULL),
      m_socketLock(QMutex::Recursive),
      m_hostname("localhost"),
      m_port(6548),
      m_bConnected(false),
      m_retryTimer(new QTimer(this)),
      m_zmclientReady(false)
{
    setObjectName("ZMClient");
    connect(m_retryTimer, SIGNAL(timeout()),   this, SLOT(restartConnection()));
}

bool  ZMClient::m_server_unavailable = false;
class ZMClient *ZMClient::m_zmclient = NULL;

class ZMClient *ZMClient::get(void)
{
    if (m_zmclient == NULL && m_server_unavailable == false)
        m_zmclient = new ZMClient;
    return m_zmclient;
}

bool ZMClient::setupZMClient(void)
{
    QString zmserver_host;
    int zmserver_port;

    if (m_zmclient)
    {
        delete m_zmclient;
        m_zmclient = NULL;
        m_server_unavailable = false;
    }

    zmserver_host = gCoreContext->GetSetting("ZoneMinderServerIP", "localhost");
    zmserver_port = gCoreContext->GetNumSetting("ZoneMinderServerPort", 6548);

    class ZMClient *zmclient = ZMClient::get();
    if (zmclient->connectToHost(zmserver_host, zmserver_port) == false)
    {
        delete m_zmclient;
        m_zmclient = NULL;
        m_server_unavailable = false;
        return false;
    }

    return true;
}

bool ZMClient::connectToHost(const QString &lhostname, unsigned int lport)
{
    QMutexLocker locker(&m_socketLock);

    m_hostname = lhostname;
    m_port = lport;

    m_bConnected = false;
    int count = 0;
    do
    {
        ++count;

        LOG(VB_GENERAL, LOG_INFO,
            QString("Connecting to zm server: %1:%2 (try %3 of 10)")
                .arg(m_hostname).arg(m_port).arg(count));
        if (m_socket)
        {
            m_socket->DecrRef();
            m_socket = NULL;
        }

        m_socket = new MythSocket();
        //m_socket->setCallbacks(this);
        if (!m_socket->ConnectToHost(m_hostname, m_port))
        {
            m_socket->DecrRef();
            m_socket = NULL;
        }
        else
        {
            m_zmclientReady = true;
            m_bConnected = true;
        }

        usleep(500000);

    } while (count < 10 && !m_bConnected);

    if (!m_bConnected)
    {
        ShowOkPopup(tr("Cannot connect to the mythzmserver - Is it running? "
                       "Have you set the correct IP and port in the settings?"));
    }

    // check the server uses the same protocol as us
    if (m_bConnected && !checkProtoVersion())
    {
        m_zmclientReady = false;
        m_bConnected = false;
    }

    if (m_bConnected == false)
        m_server_unavailable = true;

    return m_bConnected;
}

bool ZMClient::sendReceiveStringList(QStringList &strList)
{
    QStringList origStrList = strList;

    bool ok = false;
    if (m_bConnected)
        ok = m_socket->SendReceiveStringList(strList);

    if (!ok)
    {
        LOG(VB_GENERAL, LOG_NOTICE, "Connection to mythzmserver lost");

        if (!connectToHost(m_hostname, m_port))
        {
            LOG(VB_GENERAL, LOG_ERR, "Re-connection to mythzmserver failed");
            return false;
        }

        // try to resend
        strList = origStrList;
        ok = m_socket->SendReceiveStringList(strList);
        if (!ok)
        {
            m_bConnected = false;
            return false;
        }
    }

    // sanity check
    if (strList.size() < 1)
    {
        LOG(VB_GENERAL, LOG_ERR, "ZMClient response too short");
        return false;
    }

    // the server sends "UNKNOWN_COMMAND" if it did not recognise the command
    if (strList[0] == "UNKNOWN_COMMAND")
    {
        LOG(VB_GENERAL, LOG_ERR, "Somethings is getting passed to the server "
                                 "that it doesn't understand");
        return false;
    }

    // the server sends "ERROR" if it failed to process the command
    if (strList[0].startsWith("ERROR"))
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("The server failed to process the command. "
                    "The error was:- \n\t\t\t%1").arg(strList[0]));
        return false;
    }

    // we should get "OK" from the server if everything is OK
    if (strList[0] != "OK")
        return false;

    return true;
}

bool ZMClient::checkProtoVersion(void)
{
    QStringList strList("HELLO");
    if (!sendReceiveStringList(strList))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Server didn't respond to 'HELLO'!!"));

        ShowOkPopup(tr("The mythzmserver didn't respond to our request "
                       "to get the protocol version!!"));
        return false;
    }

    // sanity check
    if (strList.size() < 2)
    {
        LOG(VB_GENERAL, LOG_ERR, "ZMClient response too short");
        return false;
    }

    if (strList[1] != ZM_PROTOCOL_VERSION)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Protocol version mismatch (plugin=%1, mythzmserver=%2)")
                .arg(ZM_PROTOCOL_VERSION).arg(strList[1]));

        ShowOkPopup(QString("The mythzmserver uses protocol version %1, "
                            "but this client only understands version %2. "
                            "Make sure you are running compatible versions of "
                            "both the server and plugin.")
                            .arg(strList[1]).arg(ZM_PROTOCOL_VERSION));
        return false;
    }

    LOG(VB_GENERAL, LOG_INFO,
        QString("Using protocol version %1").arg(ZM_PROTOCOL_VERSION));
    return true;
}

void ZMClient::restartConnection()
{
    // Reset the flag
    m_zmclientReady = false;
    m_bConnected = false;
    m_server_unavailable = false;

    // Retry to connect. . .  Maybe the user restarted mythzmserver?
    connectToHost(m_hostname, m_port);
}

void ZMClient::shutdown()
{
    QMutexLocker locker(&m_socketLock);

    if (m_socket)
        m_socket->DisconnectFromHost();

    m_zmclientReady = false;
    m_bConnected = false;
}

ZMClient::~ZMClient()
{
    m_zmclient = NULL;

    if (m_socket)
    {
        m_socket->DecrRef();
        m_socket = NULL;
        m_zmclientReady = false;
    }

    if (m_retryTimer)
        delete m_retryTimer;
}

void ZMClient::getServerStatus(QString &status, QString &cpuStat, QString &diskStat)
{
    QStringList strList("GET_SERVER_STATUS");
    if (!sendReceiveStringList(strList))
        return;

    // sanity check
    if (strList.size() < 4)
    {
        LOG(VB_GENERAL, LOG_ERR, "ZMClient response too short");
        return;
    }

    status = strList[1];
    cpuStat = strList[2];
    diskStat = strList[3];
}

void ZMClient::getMonitorStatus(vector<Monitor*> *monitorList)
{
    monitorList->clear();

    QStringList strList("GET_MONITOR_STATUS");
    if (!sendReceiveStringList(strList))
        return;

    // sanity check
    if (strList.size() < 2)
    {
        LOG(VB_GENERAL, LOG_ERR, "ZMClient response too short");
        return;
    }

    bool bOK;
    int monitorCount = strList[1].toInt(&bOK);
    if (!bOK)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "ZMClient received bad int in getMonitorStatus()");
        return;
    }

    for (int x = 0; x < monitorCount; x++)
    {
        Monitor *item = new Monitor;
        item->id = strList[x * 7 + 2].toInt();
        item->name = strList[x * 7 + 3];
        item->zmcStatus = strList[x * 7 + 4];
        item->zmaStatus = strList[x * 7 + 5];
        item->events = strList[x * 7 + 6].toInt();
        item->function = strList[x * 7 + 7];
        item->enabled = strList[x * 7 + 8].toInt();
        monitorList->push_back(item);
    }
}

void ZMClient::getEventList(const QString &monitorName, bool oldestFirst,
                            QString date, vector<Event*> *eventList)
{
    eventList->clear();

    QStringList strList("GET_EVENT_LIST");
    strList << monitorName << (oldestFirst ? "1" : "0") ;
    strList << date;

    if (!sendReceiveStringList(strList))
        return;

    // sanity check
    if (strList.size() < 2)
    {
        LOG(VB_GENERAL, LOG_ERR, "ZMClient response too short");
        return;
    }

    bool bOK;
    int eventCount = strList[1].toInt(&bOK);
    if (!bOK)
    {
        LOG(VB_GENERAL, LOG_ERR, "ZMClient received bad int in getEventList()");
        return;
    }

    // sanity check
    if ((int)(strList.size() - 2) / 6 != eventCount)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "ZMClient got a mismatch between the number of events and "
            "the expected number of stringlist items in getEventList()");
        return;
    }

    QStringList::Iterator it = strList.begin();
    it++; it++;
    for (int x = 0; x < eventCount; x++)
    {
        eventList->push_back(
            new Event(
                (*it++).toInt(), /* eventID */
                *it++, /* eventName */
                (*it++).toInt(), /* monitorID */
                *it++, /* monitorName */
                QDateTime::fromString(*it++, Qt::ISODate), /* startTime */
                *it++ /* length */));
    }
}

void ZMClient::getEventDates(const QString &monitorName, bool oldestFirst,
                            QStringList &dateList)
{
    dateList.clear();

    QStringList strList("GET_EVENT_DATES");
    strList << monitorName << (oldestFirst ? "1" : "0") ;

    if (!sendReceiveStringList(strList))
        return;

    // sanity check
    if (strList.size() < 2)
    {
        LOG(VB_GENERAL, LOG_ERR, "ZMClient response too short");
        return;
    }

    bool bOK;
    int dateCount = strList[1].toInt(&bOK);
    if (!bOK)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "ZMClient received bad int in getEventDates()");
        return;
    }

    // sanity check
    if ((int)(strList.size() - 3) != dateCount)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "ZMClient got a mismatch between the number of dates and "
            "the expected number of stringlist items in getEventDates()");
        return;
    }

    QStringList::Iterator it = strList.begin();
    it++; it++;
    for (int x = 0; x < dateCount; x++)
    {
        dateList.append(*it++);
    }
}

void ZMClient::getFrameList(int eventID, vector<Frame*> *frameList)
{
    frameList->clear();

    QStringList strList("GET_FRAME_LIST");
    strList << QString::number(eventID);
    if (!sendReceiveStringList(strList))
        return;

    // sanity check
    if (strList.size() < 2)
    {
        LOG(VB_GENERAL, LOG_ERR, "ZMClient response too short");
        return;
    }

    bool bOK;
    int frameCount = strList[1].toInt(&bOK);
    if (!bOK)
    {
        LOG(VB_GENERAL, LOG_ERR, "ZMClient received bad int in getFrameList()");
        return;
    }

    // sanity check
    if ((int)(strList.size() - 2) / 2 != frameCount)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "ZMClient got a mismatch between the number of frames and "
            "the expected number of stringlist items in getFrameList()");
        return;
    }

    QStringList::Iterator it = strList.begin();
    it++; it++;
    for (int x = 0; x < frameCount; x++)
    {
        Frame *item = new Frame;
        item->type = *it++;
        item->delta = (*it++).toDouble();
        frameList->push_back(item);
    }
}

void ZMClient::deleteEvent(int eventID)
{
    QStringList strList("DELETE_EVENT");
    strList << QString::number(eventID);
    sendReceiveStringList(strList);
}

void ZMClient::deleteEventList(vector<Event*> *eventList)
{
    // delete events in 100 event chunks
    QStringList strList("DELETE_EVENT_LIST");
    int count = 0;
    vector<Event*>::iterator it;
    for (it = eventList->begin(); it != eventList->end(); ++it)
    {
        strList << QString::number((*it)->eventID());

        if (++count == 100)
        {
            sendReceiveStringList(strList);
            strList = QStringList("DELETE_EVENT_LIST");
            count = 0;
        }
    }

    // make sure the last chunk is deleted
    sendReceiveStringList(strList);

    // run zmaudit to clean up the orphaned db entries
    strList = QStringList("RUN_ZMAUDIT");
    sendReceiveStringList(strList);
}

bool ZMClient::readData(unsigned char *data, int dataSize)
{
    qint64 read = 0;
    int errmsgtime = 0;
    MythTimer timer;
    timer.start();
    int elapsed;

    while (dataSize > 0)
    {
        qint64 sret = m_socket->Read(
            (char*) data + read, dataSize, 100 /*ms*/);
        if (sret > 0)
        {
            read += sret;
            dataSize -= sret;
            if (dataSize > 0)
            {
                timer.start();
            }
        }
        else if (sret < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, "readData: Error, readBlock");
            m_socket->DisconnectFromHost();
            return false;
        }
        else if (!m_socket->IsConnected())
        {
            LOG(VB_GENERAL, LOG_ERR,
                "readData: Error, socket went unconnected");
            m_socket->DisconnectFromHost();
            return false;
        }
        else
        {
            elapsed = timer.elapsed();
            if (elapsed  > 10000)
            {
                if ((elapsed - errmsgtime) > 10000)
                {
                    errmsgtime = elapsed;
                    LOG(VB_GENERAL, LOG_ERR,
                        QString("m_socket->: Waiting for data: %1 %2")
                            .arg(read).arg(dataSize));
                }
            }

            if (elapsed > 100000)
            {
                LOG(VB_GENERAL, LOG_ERR, "Error, readData timeout (readBlock)");
                return false;
            }
        }
    }

    return true;
}

void ZMClient::getEventFrame(Event *event, int frameNo, MythImage **image)
{
    if (*image)
    {
        (*image)->DecrRef();
        *image = NULL;
    }

    QStringList strList("GET_EVENT_FRAME");
    strList << QString::number(event->monitorID());
    strList << QString::number(event->eventID());
    strList << QString::number(frameNo);
    strList << event->startTime(Qt::LocalTime).toString("yy/MM/dd/hh/mm/ss");
    if (!sendReceiveStringList(strList))
        return;

    // sanity check
    if (strList.size() < 2)
    {
        LOG(VB_GENERAL, LOG_ERR, "ZMClient response too short");
        return;
    }

    // get frame length from data
    int imageSize = strList[1].toInt();

    // grab the image data
    unsigned char *data = new unsigned char[imageSize];
    if (!readData(data, imageSize))
    {
        LOG(VB_GENERAL, LOG_ERR,
            "ZMClient::getEventFrame(): Failed to get image data");
        delete [] data;
        return;
    }

    // get a MythImage
    *image = GetMythMainWindow()->GetCurrentPainter()->GetFormatImage();

    // extract the image data and create a MythImage from it
    if (!(*image)->loadFromData(data, imageSize, "JPEG"))
    {
        LOG(VB_GENERAL, LOG_ERR,
            "ZMClient::getEventFrame(): Failed to load image from data");
    }

    delete [] data;
}

void ZMClient::getAnalyseFrame(Event *event, int frameNo, QImage &image)
{
    QStringList strList("GET_ANALYSE_FRAME");
    strList << QString::number(event->monitorID());
    strList << QString::number(event->eventID());
    strList << QString::number(frameNo);
    strList << event->startTime(Qt::LocalTime).toString("yy/MM/dd/hh/mm/ss");
    if (!sendReceiveStringList(strList))
    {
        image = QImage();
        return;
    }

    // sanity check
    if (strList.size() < 2)
    {
        LOG(VB_GENERAL, LOG_ERR, "ZMClient response too short");
        return;
    }

    // get frame length from data
    int imageSize = strList[1].toInt();

    // grab the image data
    unsigned char *data = new unsigned char[imageSize];
    if (!readData(data, imageSize))
    {
        LOG(VB_GENERAL, LOG_ERR,
            "ZMClient::getAnalyseFrame(): Failed to get image data");
        image = QImage();
    }
    else
    {
        // extract the image data and create a QImage from it
        if (!image.loadFromData(data, imageSize, "JPEG"))
        {
            LOG(VB_GENERAL, LOG_ERR,
                "ZMClient::getAnalyseFrame(): Failed to load image from data");
            image = QImage();
        }
    }

    delete [] data;
}

int ZMClient::getLiveFrame(int monitorID, QString &status, unsigned char* buffer, int bufferSize)
{
    QStringList strList("GET_LIVE_FRAME");
    strList << QString::number(monitorID);
    if (!sendReceiveStringList(strList))
    {
        if (strList.size() < 1)
        {
            LOG(VB_GENERAL, LOG_ERR, "ZMClient response too short");
            return 0;
        }

        // the server sends a "WARNING" message if there is no new
        // frame available we can safely ignore it
        if (strList[0].startsWith("WARNING"))
        {
            return 0;
        }
        else
        {
            status = strList[0];
            return 0;
        }
    }

    // sanity check
    if (strList.size() < 4)
    {
        LOG(VB_GENERAL, LOG_ERR, "ZMClient response too short");
        return 0;
    }

    // get status
    status = strList[2];

    // get frame length from data
    int imageSize = strList[3].toInt();

    if (bufferSize < imageSize)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "ZMClient::getLiveFrame(): Live frame buffer is too small!");
        return 0;
    }

    // read the frame data
    if (imageSize == 0)
        return 0;

    if (!readData(buffer, imageSize))
    {
        LOG(VB_GENERAL, LOG_ERR,
            "ZMClient::getLiveFrame(): Failed to get image data");
        return 0;
    }

    return imageSize;
}

void ZMClient::getCameraList(QStringList &cameraList)
{
    cameraList.clear();

    QStringList strList("GET_CAMERA_LIST");
    if (!sendReceiveStringList(strList))
        return;

    // sanity check
    if (strList.size() < 2)
    {
        LOG(VB_GENERAL, LOG_ERR, "ZMClient response too short");
        return;
    }

    bool bOK;
    int cameraCount = strList[1].toInt(&bOK);
    if (!bOK)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "ZMClient received bad int in getCameraList()");
        return;
    }

    // sanity check
    if (strList.size() < cameraCount + 2)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "ZMClient got a mismatch between the number of cameras and "
            "the expected number of stringlist items in getCameraList()");
        return;
    }

    for (int x = 0; x < cameraCount; x++)
    {
        cameraList.append(strList[x + 2]);
    }
}

void ZMClient::getMonitorList(vector<Monitor*> *monitorList)
{
    monitorList->clear();

    QStringList strList("GET_MONITOR_LIST");
    if (!sendReceiveStringList(strList))
        return;

    // sanity check
    if (strList.size() < 2)
    {
        LOG(VB_GENERAL, LOG_ERR, "ZMClient response too short");
        return;
    }

    bool bOK;
    int monitorCount = strList[1].toInt(&bOK);
    if (!bOK)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "ZMClient received bad int in getMonitorList()");
        return;
    }

    // sanity check
    if ((int)(strList.size() - 2) / 5 != monitorCount)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "ZMClient got a mismatch between the number of monitors and "
            "the expected number of stringlist items in getMonitorList()");
        return;
    }

    for (int x = 0; x < monitorCount; x++)
    {
        Monitor *item = new Monitor;
        item->id = strList[x * 5 + 2].toInt();
        item->name = strList[x * 5 + 3];
        item->width = strList[x * 5 + 4].toInt();
        item->height = strList[x * 5 + 5].toInt();
        item->palette = strList[x * 5 + 6].toInt();
        item->zmcStatus = "";
        item->zmaStatus = "";
        item->events = 0;
        item->status = "";
        item->isV4L2 = (item->palette > 255);
        monitorList->push_back(item);
        if (item->isV4L2)
        {
            QString pallete;
            pallete  = (char) (item->palette & 0xff);
            pallete += (char) ((item->palette >> 8) & 0xff);
            pallete += (char) ((item->palette >> 16) & 0xff);
            pallete += (char) ((item->palette >> 24) & 0xff);
            LOG(VB_GENERAL, LOG_NOTICE,
                QString("Monitor: %1 (%2) is using palette: %3 (%4)")
                    .arg(item->name).arg(item->id).arg(item->palette)
                    .arg(pallete));
        }
        else
            LOG(VB_GENERAL, LOG_NOTICE,
                QString("Monitor: %1 (%2) is using palette: %3")
                    .arg(item->name).arg(item->id).arg(item->palette));
    }
}

void ZMClient::setMonitorFunction(const int monitorID, const QString &function, const int enabled)
{
    QStringList strList("SET_MONITOR_FUNCTION");
    strList << QString::number(monitorID);
    strList << function;
    strList << QString::number(enabled);

    if (!sendReceiveStringList(strList))
        return;
}
