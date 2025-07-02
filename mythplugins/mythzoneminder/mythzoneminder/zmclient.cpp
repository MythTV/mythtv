/*
    zmclient.cpp
*/

// C++
#include <unistd.h>

// qt
#include <QTimer>

//MythTV
#include <libmythbase/mythcorecontext.h>
#include <libmythbase/mythdate.h>
#include <libmythbase/mythlogging.h>
#include <libmythui/mythdialogbox.h>
#include <libmythui/mythmainwindow.h>

//zoneminder
#include "zmclient.h"
#include "zmminiplayer.h"

// the protocol version we understand
static constexpr const char* ZM_PROTOCOL_VERSION { "11" };

ZMClient::ZMClient()
    : QObject(nullptr),
      m_retryTimer(new QTimer(this))
{
    setObjectName("ZMClient");
    connect(m_retryTimer, &QTimer::timeout,   this, &ZMClient::restartConnection);

    gCoreContext->addListener(this);
}

ZMClient *ZMClient::m_zmclient = nullptr;

ZMClient *ZMClient::get(void)
{
    if (!m_zmclient)
        m_zmclient = new ZMClient;
    return m_zmclient;
}

bool ZMClient::setupZMClient(void)
{
    QString zmserver_host;

    zmserver_host = gCoreContext->GetSetting("ZoneMinderServerIP", "");
    int zmserver_port = gCoreContext->GetNumSetting("ZoneMinderServerPort", -1);

    // don't try to connect if we don't have a valid host or port
    if (zmserver_host.isEmpty() || zmserver_port == -1)
    {
        LOG(VB_GENERAL, LOG_INFO, "ZMClient: no valid IP or port found for mythzmserver");
        return false;
    }

    return ZMClient::get()->connectToHost(zmserver_host, zmserver_port);
}

bool ZMClient::connectToHost(const QString &lhostname, unsigned int lport)
{
    QMutexLocker locker(&m_socketLock);

    m_hostname = lhostname;
    m_port = lport;

    m_bConnected = false;
    int count = 0;
    while (count < 2 && !m_bConnected)
    {
        ++count;

        LOG(VB_GENERAL, LOG_INFO,
            QString("Connecting to zm server: %1:%2 (try %3 of 2)")
                .arg(m_hostname).arg(m_port).arg(count));
        if (m_socket)
        {
            m_socket->DecrRef();
            m_socket = nullptr;
        }

        m_socket = new MythSocket();

        if (!m_socket->ConnectToHost(m_hostname, m_port))
        {
            m_socket->DecrRef();
            m_socket = nullptr;
        }
        else
        {
            m_zmclientReady = true;
            m_bConnected = true;
        }

        usleep(999999);
    }

    if (!m_bConnected)
    {
        if (GetNotificationCenter())
        {
            ShowNotificationError(tr("Can't connect to the mythzmserver") , "MythZoneMinder",
                               tr("Is it running? "
                                  "Have you set the correct IP and port in the settings?"));
        }
    }

    // check the server uses the same protocol as us
    if (m_bConnected && !checkProtoVersion())
    {
        m_zmclientReady = false;
        m_bConnected = false;
    }

    if (m_bConnected)
        doGetMonitorList();

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
    if (strList.empty())
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
    return strList[0] == "OK";
}

bool ZMClient::checkProtoVersion(void)
{
    QMutexLocker locker(&m_commandLock);

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
                .arg(ZM_PROTOCOL_VERSION, strList[1]));

        ShowOkPopup(QString("The mythzmserver uses protocol version %1, "
                            "but this client only understands version %2. "
                            "Make sure you are running compatible versions of "
                            "both the server and plugin.")
                            .arg(strList[1], ZM_PROTOCOL_VERSION));
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
    gCoreContext->removeListener(this);

    m_zmclient = nullptr;

    if (m_socket)
    {
        m_socket->DecrRef();
        m_socket = nullptr;
        m_zmclientReady = false;
    }

    delete m_retryTimer;
}

void ZMClient::getServerStatus(QString &status, QString &cpuStat, QString &diskStat)
{
    QMutexLocker locker(&m_commandLock);

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

void ZMClient::updateMonitorStatus(void)
{
    QMutexLocker clocker(&m_commandLock);

    QStringList strList("GET_MONITOR_STATUS");
    if (!sendReceiveStringList(strList))
        return;

    // sanity check
    if (strList.size() < 2)
    {
        LOG(VB_GENERAL, LOG_ERR, "ZMClient response too short");
        return;
    }

    bool bOK = false;
    int monitorCount = strList[1].toInt(&bOK);
    if (!bOK)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "ZMClient received bad int in getMonitorStatus()");
        return;
    }

    QMutexLocker locker(&m_listLock);

    for (int x = 0; x < monitorCount; x++)
    {
        int monID = strList[(x * 7) + 2].toInt();

        if (m_monitorMap.contains(monID))
        {
            Monitor *mon = m_monitorMap.find(monID).value();
            mon->name = strList[(x * 7) + 3];
            mon->zmcStatus = strList[(x * 7) + 4];
            mon->zmaStatus = strList[(x * 7) + 5];
            mon->events = strList[(x * 7) + 6].toInt();
            mon->function = strList[(x * 7) + 7];
            mon->enabled = (strList[(x * 7) + 8].toInt() != 0);
        }
    }
}

static QString stateToString(State state)
{
    QString result = "UNKNOWN";

    switch (state)
    {
        case IDLE:
            result = "IDLE";
            break;
        case PREALARM:
            result = "PREALARM";
            break;
        case ALARM:
            result = "ALARM";
            break;
        case ALERT:
            result = "ALERT";
            break;
        case TAPE:
            result = "TAPE";
            break;
        default:
            result = "UNKNOWN";
    }

    return result;
}

bool ZMClient::updateAlarmStates(void)
{
    QMutexLocker clocker(&m_commandLock);

    QStringList strList("GET_ALARM_STATES");
    if (!sendReceiveStringList(strList))
        return false;

    // sanity check
    if (strList.size() < 2)
    {
        LOG(VB_GENERAL, LOG_ERR, "ZMClient response too short");
        return false;
    }

    bool bOK = false;
    int monitorCount = strList[1].toInt(&bOK);
    if (!bOK)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "ZMClient received bad int in getAlarmStates()");
        return false;
    }

    QMutexLocker locker(&m_listLock);

    bool changed = false;
    for (int x = 0; x < monitorCount; x++)
    {
        int monID = strList[(x * 2) + 2].toInt();
        auto state = (State)strList[(x * 2) + 3].toInt();

        if (m_monitorMap.contains(monID))
        {
            Monitor *mon = m_monitorMap.find(monID).value();
            if (mon->state != state)
            {
                // alarm state has changed for this monitor
                LOG(VB_GENERAL, LOG_DEBUG,
                    QString("ZMClient monitor %1 changed state from %2 to %3")
                            .arg(mon->name, stateToString(mon->state), stateToString(state)));
                mon->previousState = mon->state;
                mon->state = state;
                changed = true;
            }
        }
    }

    return changed;
}

void ZMClient::getEventList(const QString &monitorName, bool oldestFirst,
                            const QString &date, bool includeContinuous,
                            std::vector<Event*> *eventList)
{
    QMutexLocker locker(&m_commandLock);

    eventList->clear();

    QStringList strList("GET_EVENT_LIST");
    strList << monitorName << (oldestFirst ? "1" : "0") ;
    strList << date;
    strList << (includeContinuous ? "1" : "0") ;

    if (!sendReceiveStringList(strList))
        return;

    // sanity check
    if (strList.size() < 2)
    {
        LOG(VB_GENERAL, LOG_ERR, "ZMClient response too short");
        return;
    }

    bool bOK = false;
    int eventCount = strList[1].toInt(&bOK);
    if (!bOK)
    {
        LOG(VB_GENERAL, LOG_ERR, "ZMClient received bad int in getEventList()");
        return;
    }

    // sanity check
    if ((strList.size() - 2) / 6 != eventCount)
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
    QMutexLocker locker(&m_commandLock);

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

    bool bOK = false;
    int dateCount = strList[1].toInt(&bOK);
    if (!bOK)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "ZMClient received bad int in getEventDates()");
        return;
    }

    // sanity check
    if ((strList.size() - 3) != dateCount)
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

void ZMClient::getFrameList(int eventID, std::vector<Frame*> *frameList)
{
    QMutexLocker locker(&m_commandLock);

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

    bool bOK = false;
    int frameCount = strList[1].toInt(&bOK);
    if (!bOK)
    {
        LOG(VB_GENERAL, LOG_ERR, "ZMClient received bad int in getFrameList()");
        return;
    }

    // sanity check
    if ((strList.size() - 2) / 2 != frameCount)
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
        auto *item = new Frame;
        item->type = *it++;
        item->delta = (*it++).toDouble();
        frameList->push_back(item);
    }
}

void ZMClient::deleteEvent(int eventID)
{
    QMutexLocker locker(&m_commandLock);

    QStringList strList("DELETE_EVENT");
    strList << QString::number(eventID);
    sendReceiveStringList(strList);
}

void ZMClient::deleteEventList(std::vector<Event*> *eventList)
{
    QMutexLocker locker(&m_commandLock);

    // delete events in 100 event chunks
    QStringList strList("DELETE_EVENT_LIST");
    int count = 0;
    std::vector<Event*>::iterator it;
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
    std::chrono::milliseconds errmsgtime { 0ms };
    MythTimer timer;
    timer.start();

    while (dataSize > 0)
    {
        qint64 sret = m_socket->Read((char*) data + read, dataSize, 100ms);
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
            std::chrono::milliseconds elapsed = timer.elapsed();
            if (elapsed  > 10s)
            {
                if ((elapsed - errmsgtime) > 10s)
                {
                    errmsgtime = elapsed;
                    LOG(VB_GENERAL, LOG_ERR,
                        QString("m_socket->: Waiting for data: %1 %2")
                            .arg(read).arg(dataSize));
                }
            }

            if (elapsed > 100s)
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
    QMutexLocker locker(&m_commandLock);

    if (*image)
    {
        (*image)->DecrRef();
        *image = nullptr;
    }

    QStringList strList("GET_EVENT_FRAME");
    strList << QString::number(event->monitorID());
    strList << QString::number(event->eventID());
    strList << QString::number(frameNo);
#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
    strList << event->startTime(Qt::LocalTime).toString("yy/MM/dd/hh/mm/ss");
#else
    static const QTimeZone localtime(QTimeZone::LocalTime);
    strList << event->startTime(localtime).toString("yy/MM/dd/hh/mm/ss");
#endif
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
    auto *data = new unsigned char[imageSize];
    if (!readData(data, imageSize))
    {
        LOG(VB_GENERAL, LOG_ERR,
            "ZMClient::getEventFrame(): Failed to get image data");
        delete [] data;
        return;
    }

    // get a MythImage
    *image = GetMythMainWindow()->GetPainter()->GetFormatImage();

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
    QMutexLocker locker(&m_commandLock);

    QStringList strList("GET_ANALYSE_FRAME");
    strList << QString::number(event->monitorID());
    strList << QString::number(event->eventID());
    strList << QString::number(frameNo);
#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
    strList << event->startTime(Qt::LocalTime).toString("yy/MM/dd/hh/mm/ss");
#else
    static const QTimeZone localtime(QTimeZone::LocalTime);
    strList << event->startTime(localtime).toString("yy/MM/dd/hh/mm/ss");
#endif
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
    auto *data = new unsigned char[imageSize];
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

int ZMClient::getLiveFrame(int monitorID, QString &status, FrameData& buffer)
{
    QMutexLocker locker(&m_commandLock);

    QStringList strList("GET_LIVE_FRAME");
    strList << QString::number(monitorID);
    if (!sendReceiveStringList(strList))
    {
        if (strList.empty())
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
        status = strList[0];
        return 0;
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
    size_t imageSize = strList[3].toInt();

    if (buffer.size() < imageSize)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "ZMClient::getLiveFrame(): Live frame buffer is too small!");
        return 0;
    }

    // read the frame data
    if (imageSize == 0)
        return 0;

    if (!readData(buffer.data(), imageSize))
    {
        LOG(VB_GENERAL, LOG_ERR,
            "ZMClient::getLiveFrame(): Failed to get image data");
        return 0;
    }

    return imageSize;
}

void ZMClient::getCameraList(QStringList &cameraList)
{
    QMutexLocker locker(&m_commandLock);

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

    bool bOK = false;
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
        LOG(VB_GENERAL, LOG_ERR, QString(
            "ZMClient got a mismatch between the number of cameras (%1) and "
            "the expected number of stringlist items (%2) in getCameraList()")
            .arg(cameraCount).arg(strList.size()));
        return;
    }

    for (int x = 0; x < cameraCount; x++)
    {
        cameraList.append(strList[x + 2]);
    }
}

int ZMClient::getMonitorCount(void)
{
    QMutexLocker locker(&m_listLock);
    return m_monitorList.count();
}

Monitor *ZMClient::getMonitorAt(int pos)
{
    QMutexLocker locker(&m_listLock);

    if (pos < 0 || pos > m_monitorList.count() - 1)
        return nullptr;

    return m_monitorList.at(pos);
}

Monitor* ZMClient::getMonitorByID(int monID)
{
    QMutexLocker locker(&m_listLock);

    if (m_monitorMap.contains(monID))
        return m_monitorMap.find(monID).value();

    return nullptr;
}

void ZMClient::doGetMonitorList(void)
{
    QMutexLocker clocker(&m_commandLock);
    QMutexLocker locker(&m_listLock);

    for (int x = 0; x < m_monitorList.count(); x++)
        delete m_monitorList.at(x);

    m_monitorList.clear();
    m_monitorMap.clear();

    QStringList strList("GET_MONITOR_LIST");
    if (!sendReceiveStringList(strList))
        return;

    // sanity check
    if (strList.size() < 2)
    {
        LOG(VB_GENERAL, LOG_ERR, "ZMClient response too short");
        return;
    }

    bool bOK = false;
    int monitorCount = strList[1].toInt(&bOK);
    if (!bOK)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "ZMClient received bad int in getMonitorList()");
        return;
    }

    // sanity check
    if ((strList.size() - 2) / 5 != monitorCount)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "ZMClient got a mismatch between the number of monitors and "
            "the expected number of stringlist items in getMonitorList()");
        return;
    }

    // get list of monitor id's that need monitoring
    QString s = gCoreContext->GetSetting("ZoneMinderNotificationMonitors");
    QStringList notificationMonitors = s.split(",");

    for (int x = 0; x < monitorCount; x++)
    {
        auto *item = new Monitor;
        item->id = strList[(x * 5) + 2].toInt();
        item->name = strList[(x * 5) + 3];
        item->width = strList[(x * 5) + 4].toInt();
        item->height = strList[(x * 5) + 5].toInt();
        item->bytes_per_pixel = strList[(x * 5) + 6].toInt();
        item->zmcStatus = "";
        item->zmaStatus = "";
        item->events = 0;
        item->status = "";
        item->showNotifications = notificationMonitors.contains(QString::number(item->id));

        m_monitorList.append(item);
        m_monitorMap.insert(item->id, item);

        LOG(VB_GENERAL, LOG_NOTICE,
                QString("Monitor: %1 (%2) is using %3 bytes per pixel")
                    .arg(item->name).arg(item->id).arg(item->bytes_per_pixel));
    }
}

void ZMClient::setMonitorFunction(const int monitorID, const QString &function, const bool enabled)
{
    QMutexLocker locker(&m_commandLock);

    QStringList strList("SET_MONITOR_FUNCTION");
    strList << QString::number(monitorID);
    strList << function;
    strList << QString::number(static_cast<int>(enabled));

    if (!sendReceiveStringList(strList))
        return;
}

void ZMClient::saveNotificationMonitors(void)
{
    QString s;

    for (int x = 0; x < m_monitorList.count(); x++)
    {
        Monitor *mon = m_monitorList.at(x);
        if (mon->showNotifications)
        {
            if (!s.isEmpty())
                s += QString(",%1").arg(mon->id);
            else
                s = QString("%1").arg(mon->id);
        }
    }

    gCoreContext->SaveSetting("ZoneMinderNotificationMonitors", s);
}

void ZMClient::customEvent (QEvent* event)
{
    if (event->type() == MythEvent::kMythEventMessage)
    {
        auto *me = dynamic_cast<MythEvent*>(event);
        if (!me)
            return;

        if (me->Message().startsWith("ZONEMINDER_NOTIFICATION"))
        {
            QStringList list = me->Message().simplified().split(' ');

            if (list.size() < 2)
                return;

            int monID = list[1].toInt();
            showMiniPlayer(monID);
        }
    }

    QObject::customEvent(event);
}

void ZMClient::showMiniPlayer(int monitorID) const
{
    if (!isMiniPlayerEnabled())
        return;

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    auto *miniPlayer = new ZMMiniPlayer(popupStack);

    miniPlayer->setAlarmMonitor(monitorID);

    if (miniPlayer->Create())
        popupStack->AddScreen(miniPlayer);
}
