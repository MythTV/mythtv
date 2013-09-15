/*
    lcddevice.cpp

    a MythTV project object to control an
    LCDproc server

    (c) 2002, 2003 Thor Sigvaldason, Dan Morphis and Isaac Richards
*/

// ANSI C headers
#include <cstdlib>
#include <cmath>
#include <fcntl.h>
#include <errno.h>

// Qt headers
#include <QApplication>
#include <QRegExp>
#include <QTextStream>
#include <QTextCodec>
#include <QByteArray>
#include <QTcpSocket>

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
    : QObject(),
      m_socket(NULL),                 m_socketLock(QMutex::Recursive),
      m_hostname("localhost"),        m_port(6545),
      m_connected(false),

      m_retryTimer(new QTimer(this)), m_LEDTimer(new QTimer(this)),

      m_lcdWidth(0),                 m_lcdHeight(0),

      m_lcdReady(false),             m_lcdShowTime(false),
      m_lcdShowMenu(false),          m_lcdShowGeneric(false),
      m_lcdShowMusic(false),         m_lcdShowChannel(false),
      m_lcdShowVolume(false),        m_lcdShowRecStatus(false),
      m_lcdBacklightOn(false),       m_lcdHeartbeatOn(false),

      m_lcdPopupTime(0),

      m_lcdShowMusicItems(),
      m_lcdKeyString(),

      m_lcdLedMask(0),
      GetLEDMask(NULL)
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

    connect(m_retryTimer, SIGNAL(timeout()),   this, SLOT(restartConnection()));
    connect(m_LEDTimer,   SIGNAL(timeout()),   this, SLOT(outputLEDs()));
}

bool LCD::m_enabled = false;
bool LCD::m_serverUnavailable = false;
LCD *LCD::m_lcd = NULL;

LCD *LCD::Get(void)
{
    if (m_enabled && m_lcd == NULL && m_serverUnavailable == false)
        m_lcd = new LCD;
    return m_lcd;
}

void LCD::SetupLCD (void)
{
    QString lcd_host;
    int lcd_port;

    if (m_lcd)
    {
        delete m_lcd;
        m_lcd = NULL;
        m_serverUnavailable = false;
    }

    lcd_host = GetMythDB()->GetSetting("LCDServerHost", "localhost");
    lcd_port = GetMythDB()->GetNumSetting("LCDServerPort", 6545);
    m_enabled = GetMythDB()->GetNumSetting("LCDEnable", 0);

    // workaround a problem with Ubuntu not resolving localhost properly
    if (lcd_host == "localhost")
        lcd_host = "127.0.0.1";

    if (m_enabled && lcd_host.length() > 0 && lcd_port > 1024)
    {
        LCD *lcd = LCD::Get();
        if (lcd->connectToHost(lcd_host, lcd_port) == false)
        {
            delete m_lcd;
            m_lcd = NULL;
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
    if (!(m_enabled = GetMythDB()->GetNumSetting("LCDEnable", 0)))
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

    if (!m_connected)
    {
        int count = 0;
        do
        {
            ++count;

            LOG(VB_GENERAL, LOG_INFO, QString("Connecting to lcd server: "
                    "%1:%2 (try %3 of 10)").arg(m_hostname).arg(m_port)
                                           .arg(count));

            if (m_socket)
                delete m_socket;

            m_socket = new QTcpSocket();

            QObject::connect(m_socket, SIGNAL(readyRead()),
                             this, SLOT(ReadyRead()));
            QObject::connect(m_socket, SIGNAL(disconnected()),
                             this, SLOT(Disconnected()));

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
        while (count < 10 && !m_connected);
    }

    if (m_connected == false)
        m_serverUnavailable = true;

    return m_connected;
}

void LCD::sendToServer(const QString &someText)
{
    QMutexLocker locker(&m_socketLock);

    if (!m_socket || !m_lcdReady)
        return;

    // Check the socket, make sure the connection is still up
    if (QAbstractSocket::ConnectedState != m_socket->state())
    {
        m_lcdReady = false;

        // Ack, connection to server has been severed try to re-establish the
        // connection
        m_retryTimer->setSingleShot(false);
        m_retryTimer->start(10000);
        LOG(VB_GENERAL, LOG_ERR,
            "Connection to LCDServer died unexpectedly. "
            "Trying to reconnect every 10 seconds...");

        m_connected = false;
        return;
    }

    QTextStream os(m_socket);
    os.setCodec(QTextCodec::codecForName("ISO 8859-1"));

    m_lastCommand = someText;

    if (m_connected)
    {
        LOG(VB_NETWORK, LOG_DEBUG, LOC +
            QString(LOC + "Sending to Server: %1").arg(someText));

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
    QStringList::Iterator it;

    // This gets activated automatically by the MythSocket class whenever
    // there's something to read.
    //
    // We currently spend most of our time (except for the first line sent
    // back) ignoring it.

    int dataSize = m_socket->bytesAvailable() + 1;
    QByteArray data(dataSize + 1, 0);

    m_socket->read(data.data(), dataSize);

    lineFromServer = data;
    lineFromServer = lineFromServer.replace( QRegExp("\n"), " " );
    lineFromServer = lineFromServer.replace( QRegExp("\r"), " " );
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

        bool bOK;
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
        handleKeyPress(aList.last().trimmed());
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
        sendToServer(m_sendBuffer);
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

    sendToServer("STOP_ALL");
}

void LCD::setSpeakerLEDs(enum LCDSpeakerSet speaker, bool on)
{
    if (!m_lcdReady)
        return;
    m_lcdLedMask &= ~SPEAKER_MASK;
    if (on)
        m_lcdLedMask |= speaker;
    sendToServer(QString("UPDATE_LEDS %1").arg(m_lcdLedMask));
}

void LCD::setAudioFormatLEDs(enum LCDAudioFormatSet acodec, bool on)
{
    if (!m_lcdReady)
        return;

    m_lcdLedMask &= ~AUDIO_MASK;
    if (on)
        m_lcdLedMask |= (acodec & AUDIO_MASK);

    sendToServer(QString("UPDATE_LEDS %1").arg(m_lcdLedMask));
}

void LCD::setVideoFormatLEDs(enum LCDVideoFormatSet vcodec, bool on)
{
    if (!m_lcdReady)
        return;

    m_lcdLedMask &= ~VIDEO_MASK;
    if (on)
        m_lcdLedMask |= (vcodec & VIDEO_MASK);

    sendToServer(QString("UPDATE_LEDS %1").arg(m_lcdLedMask));
}

void LCD::setVideoSrcLEDs(enum LCDVideoSourceSet vsrc, bool on)
{
    if (!m_lcdReady)
        return;
    m_lcdLedMask &= ~VSRC_MASK;
    if (on)
        m_lcdLedMask |=  vsrc;
    sendToServer(QString("UPDATE_LEDS %1").arg(m_lcdLedMask));
}

void LCD::setFunctionLEDs(enum LCDFunctionSet func, bool on)
{
    if (!m_lcdReady)
        return;
    m_lcdLedMask &= ~FUNC_MASK;
    if (on)
        m_lcdLedMask |=  func;
    sendToServer(QString("UPDATE_LEDS %1").arg(m_lcdLedMask));
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
    sendToServer(QString("UPDATE_LEDS %1").arg(m_lcdLedMask));
}

void LCD::setTunerLEDs(enum LCDTunerSet tuner, bool on)
{
    if (!m_lcdReady)
        return;
    m_lcdLedMask &= ~TUNER_MASK;
    if (on)
        m_lcdLedMask |=  tuner;
    sendToServer(QString("UPDATE_LEDS %1").arg(m_lcdLedMask));
}

void LCD::setChannelProgress(const QString &time, float value)
{
    if (!m_lcdReady || !m_lcdShowChannel)
        return;

    value = std::min(std::max(0.0f, value), 1.0f);
    sendToServer(QString("SET_CHANNEL_PROGRESS %1 %2").arg(quotedString(time))
        .arg(value));
}

void LCD::setGenericProgress(float value)
{
    if (!m_lcdReady || !m_lcdShowGeneric)
        return;

    value = std::min(std::max(0.0f, value), 1.0f);
    sendToServer(QString("SET_GENERIC_PROGRESS 0 %1").arg(value));
}

void LCD::setGenericBusy()
{
    if (!m_lcdReady || !m_lcdShowGeneric)
        return;

    sendToServer("SET_GENERIC_PROGRESS 1 0.0");
}

void LCD::setMusicProgress(const QString &time, float value)
{
    if (!m_lcdReady || !m_lcdShowMusic)
        return;

    value = std::min(std::max(0.0f, value), 1.0f);
    sendToServer("SET_MUSIC_PROGRESS " + quotedString(time) + ' ' + 
            QString().setNum(value));    
}

void LCD::setMusicShuffle(int shuffle)
{
    if (!m_lcdReady || !m_lcdShowMusic)
        return;

    sendToServer(QString("SET_MUSIC_PLAYER_PROP SHUFFLE %1").arg(shuffle));
}

void LCD::setMusicRepeat(int repeat)
{
    if (!m_lcdReady || !m_lcdShowMusic)
        return;

    sendToServer(QString("SET_MUSIC_PLAYER_PROP REPEAT %1").arg(repeat));
}

void LCD::setVolumeLevel(float value)
{
    if (!m_lcdReady || !m_lcdShowVolume)
        return;

    if (value < 0.0)
        value = 0.0;
    else if (value > 1.0)
        value = 1.0;

    sendToServer("SET_VOLUME_LEVEL " + QString().setNum(value));
}

void LCD::setupLEDs(int(*LedMaskFunc)(void))
{
    GetLEDMask = LedMaskFunc;
    // update LED status every 10 seconds
    m_LEDTimer->setSingleShot(false);
    m_LEDTimer->start(10000);
}

void LCD::outputLEDs()
{
    /* now implemented elsewhere for advanced icon control */
    return;
#if 0
    if (!lcd_ready)
        return;

    QString aString;
    int mask = 0;
    if (0 && GetLEDMask)
        mask = GetLEDMask();
    aString = "UPDATE_LEDS ";
    aString += QString::number(mask);
    sendToServer(aString);
#endif
}

void LCD::switchToTime()
{
    if (!m_lcdReady)
        return;

    LOG(VB_GENERAL, LOG_DEBUG, LOC + "switchToTime");

    sendToServer("SWITCH_TO_TIME");
}

void LCD::switchToMusic(const QString &artist, const QString &album, const QString &track)
{
    if (!m_lcdReady || !m_lcdShowMusic)
        return;

    LOG(VB_GENERAL, LOG_DEBUG, LOC + "switchToMusic");

    sendToServer("SWITCH_TO_MUSIC " + quotedString(artist) + ' '
            + quotedString(album) + ' '
            + quotedString(track));
}

void LCD::switchToChannel(const QString &channum, const QString &title,
                          const QString &subtitle)
{
    if (!m_lcdReady || !m_lcdShowChannel)
        return;

    LOG(VB_GENERAL, LOG_DEBUG, LOC + "switchToChannel");

    sendToServer("SWITCH_TO_CHANNEL " + quotedString(channum) + ' '
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
    const LCDMenuItem *curItem;

    while (it.hasNext())
    {
        curItem = &(it.next());
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

    sendToServer(s);
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
    const LCDTextItem *curItem;

    while (it.hasNext())
    {
        curItem = &(it.next());

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

    sendToServer(s);
}

void LCD::switchToVolume(const QString &app_name)
{
    if (!m_lcdReady || !m_lcdShowVolume)
        return;

    LOG(VB_GENERAL, LOG_DEBUG, LOC + "switchToVolume");

    sendToServer("SWITCH_TO_VOLUME " + quotedString(app_name));
}

void LCD::switchToNothing()
{
    if (!m_lcdReady)
        return;

    LOG(VB_GENERAL, LOG_DEBUG, LOC + "switchToNothing");

    sendToServer("SWITCH_TO_NOTHING");
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

    sendToServer("RESET");
}

LCD::~LCD()
{
    m_lcd = NULL;

    LOG(VB_GENERAL, LOG_DEBUG, LOC + "An LCD device is being snuffed out of "
                                     "existence (~LCD() was called)");

    if (m_socket)
    {
        delete m_socket;
        m_socket = NULL;
        m_lcdReady = false;
    }
}

QString LCD::quotedString(const QString &string)
{
    QString sRes = string;
    sRes.replace(QRegExp("\""), QString("\"\""));
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
