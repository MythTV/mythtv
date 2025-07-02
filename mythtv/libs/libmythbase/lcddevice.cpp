/*
    lcddevice.cpp

    a MythTV project object to control an
    LCDproc server

    (c) 2002, 2003 Thor Sigvaldason, Dan Morphis and Isaac Richards
*/

// C++ headers
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h> // for usleep()

// Qt headers
#include <QApplication>
#include <QTextStream>
#include <QByteArray>
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
#include <QStringConverter>
#else
#include <QTextCodec>
#endif
#include <QTcpSocket>
#include <QTimer>

// MythTV headers
#include "lcddevice.h"
#include "mythlogging.h"
#include "compat.h"
#include "mythdb.h"
#include "mythdirs.h"
#include "mythevent.h"
#include "mythsocket.h"
#include "mythsystemlegacy.h"
#include "exitcodes.h"


#define LOC QString("LCDdevice: ")

LCD::LCD()
    : m_retryTimer(new QTimer(this)), m_ledTimer(new QTimer(this))
{
    m_sendBuffer.clear(); m_lastCommand.clear();
    m_lcdShowMusicItems.clear(); m_lcdKeyString.clear();

    setObjectName("LCD");

    // Constructor for LCD
    //
    // Note that this does *not* include opening the socket and initiating
    // communications with the LDCd daemon.

    LOG(VB_GENERAL, LOG_DEBUG, LOC +
        "An LCD object now exists (LCD() was called)");

    connect(m_retryTimer, &QTimer::timeout,   this, &LCD::restartConnection);
    connect(m_ledTimer,   &QTimer::timeout,   this, &LCD::outputLEDs);
    connect(this, &LCD::sendToServer, this, &LCD::sendToServerSlot, Qt::QueuedConnection);
}

bool LCD::m_enabled = false;
bool LCD::m_serverUnavailable = false;
LCD *LCD::m_lcd = nullptr;

LCD *LCD::Get(void)
{
    if (m_enabled && m_lcd == nullptr && !m_serverUnavailable)
        m_lcd = new LCD;
    return m_lcd;
}

void LCD::SetupLCD (void)
{
    QString lcd_host;

    if (m_lcd)
    {
        delete m_lcd;
        m_lcd = nullptr;
        m_serverUnavailable = false;
    }

    lcd_host = GetMythDB()->GetSetting("LCDServerHost", "localhost");
    int lcd_port = GetMythDB()->GetNumSetting("LCDServerPort", 6545);
    m_enabled = GetMythDB()->GetBoolSetting("LCDEnable", false);

    // workaround a problem with Ubuntu not resolving localhost properly
    if (lcd_host == "localhost")
        lcd_host = "127.0.0.1";

    if (m_enabled && lcd_host.length() > 0 && lcd_port > 1024)
    {
        LCD *lcd = LCD::Get();
        if (!lcd->connectToHost(lcd_host, lcd_port))
        {
            delete m_lcd;
            m_lcd = nullptr;
            m_serverUnavailable = false;
        }
    }
}

bool LCD::connectToHost(const QString &lhostname, unsigned int lport)
{
    QMutexLocker locker(&m_socketLock);

    LOG(VB_NETWORK, LOG_DEBUG, LOC +
        QString("connecting to host: %1 - port: %2")
            .arg(lhostname).arg(lport));

    // Open communications
    // Store the hostname and port in case we need to reconnect.
    m_hostname = lhostname;
    m_port = lport;

    // Don't even try to connect if we're currently disabled.
    m_enabled = GetMythDB()->GetBoolSetting("LCDEnable", false);
    if (!m_enabled)
    {
        m_connected = false;
        m_serverUnavailable = true;
        return m_connected;
    }

    // check if the 'mythlcdserver' is running
    uint flags = kMSRunShell | kMSDontBlockInputDevs | kMSDontDisableDrawing;
    if (myth_system("ps ch -C mythlcdserver -o pid > /dev/null", flags) == 1)
    {
        // we need to start the mythlcdserver
        LOG(VB_GENERAL, LOG_NOTICE, "Starting mythlcdserver");

        if (!startLCDServer())
        {
            LOG(VB_GENERAL, LOG_ERR, "Failed start MythTV LCD Server");
            return m_connected;
        }

        usleep(500000);
    }

    for (int count = 1; count <= 10 && !m_connected; count++)
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Connecting to lcd server: "
                "%1:%2 (try %3 of 10)").arg(m_hostname).arg(m_port)
                                       .arg(count));

        delete m_socket;
        m_socket = new QTcpSocket();

        QObject::connect(m_socket, &QIODevice::readyRead,
                         this, &LCD::ReadyRead);
        QObject::connect(m_socket, &QAbstractSocket::disconnected,
                         this, &LCD::Disconnected);

        m_socket->connectToHost(m_hostname, m_port);
        if (m_socket->waitForConnected())
        {
            m_lcdReady = false;
            m_connected = true;
            QTextStream os(m_socket);
            os << "HELLO\n";
            os.flush();

            break;
        }
        m_socket->close();

        usleep(500000);
    }

    if (!m_connected)
        m_serverUnavailable = true;

    return m_connected;
}

void LCD::sendToServerSlot(const QString &someText)
{
    QMutexLocker locker(&m_socketLock);

    if (!m_socket || !m_lcdReady)
        return;

    if (m_socket->thread() != QThread::currentThread())
    {
        LOG(VB_GENERAL, LOG_ERR,
            "Sending to LCDServer from wrong thread.");
        return;
    }

    // Check the socket, make sure the connection is still up
    if (QAbstractSocket::ConnectedState != m_socket->state())
    {
        m_lcdReady = false;

        // Ack, connection to server has been severed try to re-establish the
        // connection
        m_retryTimer->setSingleShot(false);
        m_retryTimer->start(10s);
        LOG(VB_GENERAL, LOG_ERR,
            "Connection to LCDServer died unexpectedly. "
            "Trying to reconnect every 10 seconds...");

        m_connected = false;
        return;
    }

    QTextStream os(m_socket);
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    os.setCodec(QTextCodec::codecForName("ISO 8859-1"));
#else
    os.setEncoding(QStringConverter::Latin1);
#endif

    m_lastCommand = someText;

    if (m_connected)
    {
        LOG(VB_NETWORK, LOG_DEBUG, LOC +
            QString("Sending to Server: %1").arg(someText));

        // Just stream the text out the socket
        os << someText << "\n";
    }
    else
    {
        // Buffer this up in the hope that the connection will open soon

        m_sendBuffer += someText;
        m_sendBuffer += '\n';
    }
}

void LCD::restartConnection()
{
    // Reset the flag
    m_lcdReady = false;
    m_connected = false;
    m_serverUnavailable = false;

    // Retry to connect. . .  Maybe the user restarted LCDd?
    connectToHost(m_hostname, m_port);
}

void LCD::ReadyRead(void)
{
    QMutexLocker locker(&m_socketLock);

    if (!m_socket)
        return;

    QString lineFromServer;
    QStringList aList;

    // This gets activated automatically by the MythSocket class whenever
    // there's something to read.
    //
    // We currently spend most of our time (except for the first line sent
    // back) ignoring it.

    int dataSize = static_cast<int>(m_socket->bytesAvailable() + 1);
    QByteArray data(dataSize + 1, 0);

    m_socket->read(data.data(), dataSize);

    lineFromServer = data;
    lineFromServer = lineFromServer.simplified();

    // Make debugging be less noisy
    if (lineFromServer != "OK")
        LOG(VB_NETWORK, LOG_DEBUG, LOC + QString("Received from server: %1")
                .arg(lineFromServer));

    aList = lineFromServer.split(' ');
    if (aList[0] == "CONNECTED")
    {
        // We got "CONNECTED", which is a response to "HELLO"
        // get lcd width & height
        if (aList.count() != 3)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "received bad no. of arguments in "
                                           "CONNECTED response from LCDServer");
        }

        bool bOK = false;
        m_lcdWidth = aList[1].toInt(&bOK);
        if (!bOK)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "received bad int for width in "
                                           "CONNECTED response from LCDServer");
        }

        m_lcdHeight = aList[2].toInt(&bOK);
        if (!bOK)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "received bad int for height in "
                                           "CONNECTED response from LCDServer");
        }

        init();
    }
    else if (aList[0] == "HUH?")
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Something is getting passed to "
                                       "LCDServer that it does not understand");
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("last command: %1").arg(m_lastCommand));
    }
    else if (aList[0] == "KEY")
    {
        handleKeyPress(aList.last().trimmed());
    }
}

void LCD::handleKeyPress(const QString &keyPressed)
{
    int key = 0;

    QChar mykey = keyPressed.at(0);
    if (mykey == m_lcdKeyString.at(0))
        key = Qt::Key_Up;
    else if (mykey == m_lcdKeyString.at(1))
        key = Qt::Key_Down;
    else if (mykey == m_lcdKeyString.at(2))
        key = Qt::Key_Left;
    else if (mykey == m_lcdKeyString.at(3))
        key = Qt::Key_Right;
    else if (mykey == m_lcdKeyString.at(4))
        key = Qt::Key_Space;
    else if (mykey == m_lcdKeyString.at(5))
        key = Qt::Key_Escape;

    QCoreApplication::postEvent(
        (QObject *)(QApplication::activeWindow()),
        new ExternalKeycodeEvent(key));
}

void LCD::init()
{
    // Stop the timer
    m_retryTimer->stop();

    // Get LCD settings
    m_lcdShowMusic = (GetMythDB()->GetSetting("LCDShowMusic", "1") == "1");
    m_lcdShowTime = (GetMythDB()->GetSetting("LCDShowTime", "1") == "1");
    m_lcdShowChannel = (GetMythDB()->GetSetting("LCDShowChannel", "1") == "1");
    m_lcdShowGeneric = (GetMythDB()->GetSetting("LCDShowGeneric", "1") == "1");
    m_lcdShowVolume = (GetMythDB()->GetSetting("LCDShowVolume", "1") == "1");
    m_lcdShowMenu = (GetMythDB()->GetSetting("LCDShowMenu", "1") == "1");
    m_lcdShowRecStatus = (GetMythDB()->GetSetting("LCDShowRecStatus", "1") == "1");
    m_lcdKeyString = GetMythDB()->GetSetting("LCDKeyString", "ABCDEF");

    m_connected = true;
    m_lcdReady = true;

    // send buffer if there's anything in there
    if (m_sendBuffer.length() > 0)
    {
        emit sendToServer(m_sendBuffer);
        m_sendBuffer = "";
    }
}

void LCD::Disconnected(void)
{
    m_connected = false;
}

void LCD::stopAll()
{
    if (!m_lcdReady)
        return;

    LOG(VB_GENERAL, LOG_DEBUG, LOC + "stopAll");

    emit sendToServer("STOP_ALL");
}

void LCD::setSpeakerLEDs(enum LCDSpeakerSet speaker, bool on)
{
    if (!m_lcdReady)
        return;
    m_lcdLedMask &= ~SPEAKER_MASK;
    if (on)
        m_lcdLedMask |= speaker;
    emit sendToServer(QString("UPDATE_LEDS %1").arg(m_lcdLedMask));
}

void LCD::setAudioFormatLEDs(enum LCDAudioFormatSet acodec, bool on)
{
    if (!m_lcdReady)
        return;

    m_lcdLedMask &= ~AUDIO_MASK;
    if (on)
        m_lcdLedMask |= (acodec & AUDIO_MASK);

    emit sendToServer(QString("UPDATE_LEDS %1").arg(m_lcdLedMask));
}

void LCD::setVideoFormatLEDs(enum LCDVideoFormatSet vcodec, bool on)
{
    if (!m_lcdReady)
        return;

    m_lcdLedMask &= ~VIDEO_MASK;
    if (on)
        m_lcdLedMask |= (vcodec & VIDEO_MASK);

    emit sendToServer(QString("UPDATE_LEDS %1").arg(m_lcdLedMask));
}

void LCD::setVideoSrcLEDs(enum LCDVideoSourceSet vsrc, bool on)
{
    if (!m_lcdReady)
        return;
    m_lcdLedMask &= ~VSRC_MASK;
    if (on)
        m_lcdLedMask |=  vsrc;
    emit sendToServer(QString("UPDATE_LEDS %1").arg(m_lcdLedMask));
}

void LCD::setFunctionLEDs(enum LCDFunctionSet func, bool on)
{
    if (!m_lcdReady)
        return;
    m_lcdLedMask &= ~FUNC_MASK;
    if (on)
        m_lcdLedMask |=  func;
    emit sendToServer(QString("UPDATE_LEDS %1").arg(m_lcdLedMask));
}

void LCD::setVariousLEDs(enum LCDVariousFlags various, bool on)
{
    if (!m_lcdReady)
        return;
    if (on) {
        m_lcdLedMask |=  various;
        if (various == VARIOUS_SPDIF)
            m_lcdLedMask |= SPDIF_MASK;
    } else {
        m_lcdLedMask &=  ~various;
        if (various == VARIOUS_SPDIF)
            m_lcdLedMask &= ~SPDIF_MASK;
    }
    emit sendToServer(QString("UPDATE_LEDS %1").arg(m_lcdLedMask));
}

void LCD::setTunerLEDs(enum LCDTunerSet tuner, bool on)
{
    if (!m_lcdReady)
        return;
    m_lcdLedMask &= ~TUNER_MASK;
    if (on)
        m_lcdLedMask |=  tuner;
    emit sendToServer(QString("UPDATE_LEDS %1").arg(m_lcdLedMask));
}

void LCD::setChannelProgress(const QString &time, float value)
{
    if (!m_lcdReady || !m_lcdShowChannel)
        return;

    value = std::clamp(value, 0.0F, 1.0F);
    emit sendToServer(QString("SET_CHANNEL_PROGRESS %1 %2").arg(quotedString(time))
        .arg(value));
}

void LCD::setGenericProgress(float value)
{
    if (!m_lcdReady || !m_lcdShowGeneric)
        return;

    value = std::clamp(value, 0.0F, 1.0F);
    emit sendToServer(QString("SET_GENERIC_PROGRESS 0 %1").arg(value));
}

void LCD::setGenericBusy()
{
    if (!m_lcdReady || !m_lcdShowGeneric)
        return;

    emit sendToServer("SET_GENERIC_PROGRESS 1 0.0");
}

void LCD::setMusicProgress(const QString &time, float value)
{
    if (!m_lcdReady || !m_lcdShowMusic)
        return;

    value = std::clamp(value, 0.0F, 1.0F);
    emit sendToServer("SET_MUSIC_PROGRESS " + quotedString(time) + ' ' +
            QString().setNum(value));
}

void LCD::setMusicShuffle(int shuffle)
{
    if (!m_lcdReady || !m_lcdShowMusic)
        return;

    emit sendToServer(QString("SET_MUSIC_PLAYER_PROP SHUFFLE %1").arg(shuffle));
}

void LCD::setMusicRepeat(int repeat)
{
    if (!m_lcdReady || !m_lcdShowMusic)
        return;

    emit sendToServer(QString("SET_MUSIC_PLAYER_PROP REPEAT %1").arg(repeat));
}

void LCD::setVolumeLevel(float value)
{
    if (!m_lcdReady || !m_lcdShowVolume)
        return;

    if (value < 0.0F)
        value = 0.0F;
    else if (value > 1.0F)
        value = 1.0F;

    // NOLINTNEXTLINE(readability-misleading-indentation)
    emit sendToServer("SET_VOLUME_LEVEL " + QString().setNum(value));
}

void LCD::setupLEDs(int(*LedMaskFunc)(void))
{
    m_getLEDMask = LedMaskFunc;
    // update LED status every 10 seconds
    m_ledTimer->setSingleShot(false);
    m_ledTimer->start(10s);
}

void LCD::outputLEDs()
{
    /* now implemented elsewhere for advanced icon control */
#if 0
    if (!lcd_ready)
        return;

    QString aString;
    int mask = 0;
    if (0 && m_getLEDMask)
        mask = m_getLEDMask();
    aString = "UPDATE_LEDS ";
    aString += QString::number(mask);
    emit sendToServer(aString);
#endif
}

void LCD::switchToTime()
{
    if (!m_lcdReady)
        return;

    LOG(VB_GENERAL, LOG_DEBUG, LOC + "switchToTime");

    emit sendToServer("SWITCH_TO_TIME");
}

void LCD::switchToMusic(const QString &artist, const QString &album, const QString &track)
{
    if (!m_lcdReady || !m_lcdShowMusic)
        return;

    LOG(VB_GENERAL, LOG_DEBUG, LOC + "switchToMusic");

    emit sendToServer("SWITCH_TO_MUSIC " + quotedString(artist) + ' '
            + quotedString(album) + ' '
            + quotedString(track));
}

void LCD::switchToChannel(const QString &channum, const QString &title,
                          const QString &subtitle)
{
    if (!m_lcdReady || !m_lcdShowChannel)
        return;

    LOG(VB_GENERAL, LOG_DEBUG, LOC + "switchToChannel");

    emit sendToServer("SWITCH_TO_CHANNEL " + quotedString(channum) + ' '
            + quotedString(title) + ' '
            + quotedString(subtitle));
}

void LCD::switchToMenu(QList<LCDMenuItem> &menuItems, const QString &app_name,
                       bool popMenu)
{
    if (!m_lcdReady || !m_lcdShowMenu)
        return;

    LOG(VB_GENERAL, LOG_DEBUG, LOC + "switchToMenu");

    if (menuItems.isEmpty())
        return;

    QString s = "SWITCH_TO_MENU ";

    s += quotedString(app_name);
    s += ' ' + QString(popMenu ? "TRUE" : "FALSE");


    QListIterator<LCDMenuItem> it(menuItems);

    while (it.hasNext())
    {
        const LCDMenuItem *curItem = &(it.next());
        s += ' ' + quotedString(curItem->ItemName());

        if (curItem->isChecked() == CHECKED)
            s += " CHECKED";
        else if (curItem->isChecked() == UNCHECKED)
            s += " UNCHECKED";
        else if (curItem->isChecked() == NOTCHECKABLE)
            s += " NOTCHECKABLE";

        s += ' ' + QString(curItem->isSelected() ? "TRUE" : "FALSE");
        s += ' ' + QString(curItem->Scroll() ? "TRUE" : "FALSE");
        QString sIndent;
        sIndent.setNum(curItem->getIndent());
        s += ' ' + sIndent;
    }

    emit sendToServer(s);
}

void LCD::switchToGeneric(QList<LCDTextItem> &textItems)
{
    if (!m_lcdReady || !m_lcdShowGeneric)
        return;

    LOG(VB_GENERAL, LOG_DEBUG, LOC + "switchToGeneric");

    if (textItems.isEmpty())
        return;

    QString s = "SWITCH_TO_GENERIC";

    QListIterator<LCDTextItem> it(textItems);

    while (it.hasNext())
    {
        const LCDTextItem *curItem = &(it.next());

        QString sRow;
        sRow.setNum(curItem->getRow());
        s += ' ' + sRow;

        if (curItem->getAlignment() == ALIGN_LEFT)
            s += " ALIGN_LEFT";
        else if (curItem->getAlignment() == ALIGN_RIGHT)
            s += " ALIGN_RIGHT";
        else if (curItem->getAlignment() == ALIGN_CENTERED)
            s += " ALIGN_CENTERED";

        s += ' ' + quotedString(curItem->getText());
        s += ' ' + quotedString(curItem->getScreen());
        s += ' ' + QString(curItem->getScroll() ? "TRUE" : "FALSE");
    }

    emit sendToServer(s);
}

void LCD::switchToVolume(const QString &app_name)
{
    if (!m_lcdReady || !m_lcdShowVolume)
        return;

    LOG(VB_GENERAL, LOG_DEBUG, LOC + "switchToVolume");

    emit sendToServer("SWITCH_TO_VOLUME " + quotedString(app_name));
}

void LCD::switchToNothing()
{
    if (!m_lcdReady)
        return;

    LOG(VB_GENERAL, LOG_DEBUG, LOC + "switchToNothing");

    emit sendToServer("SWITCH_TO_NOTHING");
}

void LCD::shutdown()
{
    QMutexLocker locker(&m_socketLock);

    LOG(VB_GENERAL, LOG_DEBUG, LOC + "shutdown");

    if (m_socket)
        m_socket->close();

    m_lcdReady = false;
    m_connected = false;
}

void LCD::resetServer()
{
    QMutexLocker locker(&m_socketLock);

    if (!m_lcdReady)
        return;

    LOG(VB_GENERAL, LOG_DEBUG, LOC + "RESET");

    emit sendToServer("RESET");
}

LCD::~LCD()
{
    m_lcd = nullptr;

    LOG(VB_GENERAL, LOG_DEBUG, LOC + "An LCD device is being snuffed out of "
                                     "existence (~LCD() was called)");

    if (m_socket)
    {
        delete m_socket;
        m_socket = nullptr;
        m_lcdReady = false;
    }
}

QString LCD::quotedString(const QString &string)
{
    QString sRes = string;
    sRes.replace(QString("\""), QString("\"\""));
    sRes = "\"" + sRes + "\"";

    return(sRes);
}

bool LCD::startLCDServer(void)
{
    QString command = GetAppBinDir() + "mythlcdserver";
    command += logPropagateArgs;
    uint flags = kMSDontBlockInputDevs | kMSDontDisableDrawing |
                 kMSRunBackground;

    uint retval = myth_system(command, flags);
    return( retval == GENERIC_EXIT_RUNNING );
}
