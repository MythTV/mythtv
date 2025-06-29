/*
    lcdprocclient.cpp

    a MythTV project object to control an
    LCDproc server

    (c) 2002, 2003 Thor Sigvaldason, Dan Morphis and Isaac Richards
*/

// c/c++
#include <algorithm>
#include <chrono> // for milliseconds
#include <cmath>
#include <cstdlib>
#include <thread> // for sleep_for
#include <utility>

//qt
#include <QCoreApplication>
#include <QEvent>
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
#include <QStringConverter>
#endif
#include <QTimer>

// mythtv
#include "libmythbase/compat.h"
#include "libmythbase/lcddevice.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythdbcon.h"
#include "libmythbase/mythlogging.h"
#include "libmythtv/tv.h"

// mythlcdserver
#include "lcdprocclient.h"
#include "lcdserver.h"

static constexpr uint8_t LCD_START_COL { 3 };

static constexpr uint8_t LCD_VERSION_4 { 1 };
static constexpr uint8_t LCD_VERSION_5 { 2 };

static constexpr std::chrono::milliseconds LCD_TIME_TIME       { 3s };
static constexpr std::chrono::milliseconds LCD_SCROLLLIST_TIME { 2s };

uint8_t lcdStartCol = LCD_START_COL;

LCDProcClient::LCDProcClient(LCDServer *lparent)
              : QObject(nullptr),
                m_socket(new QTcpSocket(this)),
                m_timeTimer (new QTimer(this)),
                m_scrollWTimer (new QTimer(this)),
                m_preScrollWTimer (new QTimer(this)),
                m_menuScrollTimer (new QTimer(this)),
                m_menuPreScrollTimer (new QTimer(this)),
                m_popMenuTimer (new QTimer(this)),
                m_checkConnectionsTimer (new QTimer(this)),
                m_recStatusTimer (new QTimer(this)),
                m_scrollListTimer (new QTimer(this)),
                m_showMessageTimer (new QTimer(this)),
                m_updateRecInfoTimer (new QTimer(this)),
                m_lcdTextItems (new QList<LCDTextItem>),
                m_lcdMenuItems (new QList<LCDMenuItem>),
                m_parentLCDServer (lparent)
{
    // Constructor for LCDProcClient
    //
    // Note that this does *not* include opening the socket and initiating
    // communications with the LDCd daemon.

    if (debug_level > 0)
        LOG(VB_GENERAL, LOG_INFO,
            "LCDProcClient: An LCDProcClient object now exists");

    connect(m_socket, &QAbstractSocket::errorOccurred, this, &LCDProcClient::veryBadThings);
    connect(m_socket, &QIODevice::readyRead, this, &LCDProcClient::serverSendingData);

    lcdStartCol = LCD_START_COL;
    if ( m_lcdWidth < 12)
    {
        if ( m_lcdHeight == 1)
           lcdStartCol = 0;
        else
           lcdStartCol = 1;
    }

    connect( m_timeTimer, &QTimer::timeout, this, &LCDProcClient::outputTime);

    connect( m_scrollWTimer, &QTimer::timeout, this, &LCDProcClient::scrollWidgets);

    m_preScrollWTimer->setSingleShot(true);
    connect( m_preScrollWTimer, &QTimer::timeout, this,
            &LCDProcClient::beginScrollingWidgets);

    m_popMenuTimer->setSingleShot(true);
    connect( m_popMenuTimer, &QTimer::timeout, this, &LCDProcClient::unPopMenu);

    connect( m_menuScrollTimer, &QTimer::timeout, this, &LCDProcClient::scrollMenuText);

    connect( m_menuPreScrollTimer, &QTimer::timeout, this,
            &LCDProcClient::beginScrollingMenuText);

    connect( m_checkConnectionsTimer, &QTimer::timeout, this,
            &LCDProcClient::checkConnections);
    m_checkConnectionsTimer->start(10s);

    connect( m_recStatusTimer, &QTimer::timeout, this, &LCDProcClient::outputRecStatus);

    connect( m_scrollListTimer, &QTimer::timeout, this, &LCDProcClient::scrollList);

    m_showMessageTimer->setSingleShot(true);
    connect( m_showMessageTimer, &QTimer::timeout, this,
            &LCDProcClient::removeStartupMessage);

    m_updateRecInfoTimer->setSingleShot(true);
    connect( m_updateRecInfoTimer, &QTimer::timeout, this,
            &LCDProcClient::updateRecordingList);

    gCoreContext->addListener(this);
}

bool LCDProcClient::SetupLCD ()
{
    QString lcd_host = gCoreContext->GetSetting("LCDHost", "localhost");
    int lcd_port = gCoreContext->GetNumSetting("LCDPort", 13666);

    if (lcd_host.length() > 0 && lcd_port > 1024)
        connectToHost(lcd_host, lcd_port);

    return m_connected;
}

bool LCDProcClient::connectToHost(const QString &lhostname, unsigned int lport)
{
    // Open communications
    // Store the hostname and port in case we need to reconnect.

    m_hostname = lhostname;
    m_port = lport;

    // Don't even try to connect if we're currently disabled.
    if (!gCoreContext->GetBoolSetting("LCDEnable", false))
    {
        m_connected = false;
        return m_connected;
    }

    if (!m_connected )
    {
        QTextStream os(m_socket);
        m_socket->connectToHost(m_hostname, m_port);

        int timeout = 1000;
        while (--timeout && m_socket->state() != QAbstractSocket::ConnectedState)
        {
            qApp->processEvents();
            std::this_thread::sleep_for(1ms);

            if (m_socket->state() == QAbstractSocket::ConnectedState)
            {
                m_connected = true;
                os << "hello\n";
                break;
            }
        }
    }

    return m_connected;
}

void LCDProcClient::sendToServer(const QString &someText)
{
    // Check the socket, make sure the connection is still up
    if (m_socket->state() != QAbstractSocket::ConnectedState)
    {
        if (!m_lcdReady )
            return;

        m_lcdReady = false;

        //Stop everything
        stopAll();

        // Ack, connection to server has been severed try to re-establish the
        // connection
        LOG(VB_GENERAL, LOG_ERR,
            "LCDProcClient: Connection to LCDd died unexpectedly.");
        return;
    }

    QTextStream os(m_socket);
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    os.setCodec("ISO 8859-1");
#else
    os.setEncoding(QStringConverter::Latin1);
#endif

    m_lastCommand = someText;

    if ( m_connected )
    {
        if (debug_level > 9)
            LOG(VB_NETWORK, LOG_INFO,
                "LCDProcClient: Sending to Server: " + someText);

        // Just stream the text out the socket

        os << someText << "\n";
    }
    else
    {
        // Buffer this up in the hope that the connection will open soon

        m_sendBuffer += someText;
        m_sendBuffer += "\n";
    }
}

void LCDProcClient::setPriority(const QString &screen, PRIORITY priority)
{
    QString aString;
    int err  = 0;
    aString = "screen_set ";
    aString += screen;
    aString += " priority ";

    switch (priority) {
    case TOP:
      aString += m_prioTop;
      break;
    case URGENT:
      aString += m_prioUrgent;
      break;
    case HIGH:
      aString += m_prioHigh;
      break;
    case MEDIUM:
      aString += m_prioMedium;
      break;
    case LOW:
      aString += m_prioLow;
      break;
    case OFF:
      aString += m_prioOff;
      break;
    default:
      err = 1;
      break;
    }
    if (err == 0)
      sendToServer (aString);
}

void LCDProcClient::setHeartbeat (const QString &screen, bool onoff)
{
    QString msg;
    if (onoff)
    {
        if ( m_pVersion == LCD_VERSION_4)
        {
            msg = "widget_add " + screen + " heartbeat";
        }
        if ( m_pVersion == LCD_VERSION_5)
        {
            msg = "screen_set " + screen + " heartbeat on";
        }
    }
    else
    {
        if ( m_pVersion == LCD_VERSION_4)
        {
            msg = "widget_del " + screen + " heartbeat";
        }
        if ( m_pVersion == LCD_VERSION_5)
        {
            msg = "screen_set " + screen + " heartbeat off";
        }
    }
    sendToServer (msg);
}

void LCDProcClient::checkConnections()
{
    if (debug_level > 0)
        LOG(VB_GENERAL, LOG_INFO, "LCDProcClient: checking connections");

    // check connection to mythbackend
    if (!gCoreContext->IsConnectedToMaster())
    {
        if (debug_level > 0)
            LOG(VB_GENERAL, LOG_INFO,
                "LCDProcClient: connecting to master server");
        if (!gCoreContext->ConnectToMasterServer(false))
            LOG(VB_GENERAL, LOG_ERR,
                "LCDProcClient: connecting to master server failed");
    }

    //check connection to LCDProc server
    if (m_socket->state() != QAbstractSocket::ConnectedState)
    {
        if (debug_level > 0)
            LOG(VB_GENERAL, LOG_INFO,
                "LCDProcClient: connecting to LCDProc server");

        m_lcdReady = false;
        m_connected = false;

        // Retry to connect. . .  Maybe the user restarted LCDd?
        connectToHost( m_hostname, m_port);
    }
}

void LCDProcClient::serverSendingData()
{
    QString lineFromServer;
    QString tempString;
    QStringList aList;
    QStringList::Iterator it;

    // This gets activated automatically by the QSocket class whenever
    // there's something to read.
    //
    // We currently spend most of our time (except for the first line sent
    // back) ignoring it.
    //
    // Note that if anyone has an LCDproc type lcd with buttons on it, this is
    // where we would want to catch button presses and make the rest of
    // mythTV/mythMusic do something (change tracks, channels, etc.)

    while(m_socket->canReadLine())
    {
        lineFromServer = m_socket->readLine();
        lineFromServer = lineFromServer.remove("\n");
        lineFromServer = lineFromServer.remove("\r");

        if (debug_level > 0)
        {
        // Make debugging be less noisy
            if (lineFromServer != "success")
                LOG(VB_NETWORK, LOG_INFO,
                    "LCDProcClient: Received from server: " + lineFromServer);
        }

        aList = lineFromServer.split(" ");
        if (aList.first() == "connect")
        {
            // We got a connect, which is a response to "hello"
            //
            // Need to parse out some data according the LCDproc client/server
            // spec (which is poorly documented)
            it = aList.begin();
            it++;
            if ((*it) != "LCDproc")
            {
                LOG(VB_GENERAL, LOG_WARNING,
                    "LCDProcClient: WARNING: Second parameter "
                    "returned from LCDd was not \"LCDproc\"");
            }

            // Skip through some stuff
            it++; // server version
            QString server_version = *it;
            it++; // the string "protocol"
            it++; // protocol version
            QString protocol_version = *it;
            setVersion (server_version, protocol_version);
            it++; // the string "lcd"
            it++; // the string "wid";
            it++; // Ah, the LCD width

            tempString = *it;
            setWidth(tempString.toInt());

            it++; // the string "hgt"
            it++; // the LCD height

            tempString = *it;
            setHeight(tempString.toInt());
            it++; // the string "cellwid"
            it++; // Cell width in pixels;

            tempString = *it;
            setCellWidth(tempString.toInt());

            it++; // the string "cellhgt"
            it++; // Cell height in pixels;

            tempString = *it;
            setCellHeight(tempString.toInt());

            init();

            describeServer();
        }

        if (aList.first() == "huh?")
        {
            LOG(VB_GENERAL, LOG_WARNING,
                "LCDProcClient: WARNING: Something is getting"
                "passed to LCDd that it doesn't understand");
            LOG(VB_GENERAL, LOG_WARNING, "last command: " + m_lastCommand );
        }
        else if (aList.first() == "key")
        {
           if ( m_parentLCDServer )
                m_parentLCDServer->sendKeyPress(aList.last().trimmed());
        }
    }
}

void LCDProcClient::init()
{
    QString aString;
    m_lcdKeyString = "";

    m_connected = true;

    // This gets called when we receive the "connect" string from the server
    // indicating that "hello" was succesful
    sendToServer("client_set name Myth");

    // Create all the screens and widgets (when we change activity in the myth
    // program, we just swap the priorities of the screens to show only the
    // "current one")
    sendToServer("screen_add Time");
    setPriority("Time", MEDIUM);

    if (gCoreContext->GetSetting("LCDBigClock", "1") == "1")
    {
        // Big Clock - spans multiple lines
        sendToServer("widget_add Time rec1 string");
        sendToServer("widget_add Time rec2 string");
        sendToServer("widget_add Time rec3 string");
        sendToServer("widget_add Time recCnt string");
        sendToServer("widget_add Time d0 num");
        sendToServer("widget_add Time d1 num");
        sendToServer("widget_add Time sep num");
        sendToServer("widget_add Time d2 num");
        sendToServer("widget_add Time d3 num");
        sendToServer("widget_add Time ampm string");
        sendToServer("widget_add Time dot string");
    }
    else
    {
        sendToServer("widget_add Time timeWidget string");
        sendToServer("widget_add Time topWidget string");
    }

    // The Menu Screen
    // I'm not sure how to do multi-line widgets with lcdproc so we're going
    // the ghetto way
    sendToServer("screen_add Menu");
    setPriority("Menu", LOW);
    sendToServer("widget_add Menu topWidget string");
    for (unsigned int i = 1; i <= m_lcdHeight; i++)
    {
        aString = "widget_add Menu menuWidget";
        aString += QString::number (i);
        aString += " string";
        sendToServer(aString);
    }

    // The Music Screen
    sendToServer("screen_add Music");
    setPriority("Music", LOW);
    sendToServer("widget_add Music topWidget1 string");
    sendToServer("widget_add Music topWidget2 string");
    sendToServer("widget_add Music timeWidget string");
    sendToServer("widget_add Music infoWidget string");
    sendToServer("widget_add Music progressBar hbar");

    // The Channel Screen
    sendToServer("screen_add Channel");
    setPriority("Channel", LOW);
    sendToServer("widget_add Channel topWidget string");
    sendToServer("widget_add Channel botWidget string");
    sendToServer("widget_add Channel timeWidget string");
    sendToServer("widget_add Channel progressBar hbar");

    // The Generic Screen
    sendToServer("screen_add Generic");
    setPriority("Generic", LOW);
    sendToServer("widget_add Generic textWidget1 string");
    sendToServer("widget_add Generic textWidget2 string");
    sendToServer("widget_add Generic textWidget3 string");
    sendToServer("widget_add Generic textWidget4 string");
    sendToServer("widget_add Generic progressBar hbar");

    // The Volume Screen
    sendToServer("screen_add Volume");
    setPriority("Volume", LOW);
    sendToServer("widget_add Volume topWidget string");
    sendToServer("widget_add Volume botWidget string");
    sendToServer("widget_add Volume progressBar hbar");

    // The Recording Status Screen
    sendToServer("screen_add RecStatus");
    setPriority("RecStatus", LOW);
    sendToServer("widget_add RecStatus textWidget1 string");
    sendToServer("widget_add RecStatus textWidget2 string");
    sendToServer("widget_add RecStatus textWidget3 string");
    sendToServer("widget_add RecStatus textWidget4 string");
    sendToServer("widget_add RecStatus progressBar hbar");

    m_lcdReady = true;
    loadSettings();

    // default to showing time
    switchToTime();

    updateRecordingList();

    // do we need to show the startup message
    if (!m_startupMessage.isEmpty())
        showStartupMessage();

    // send buffer if there's anything in there
    if ( m_sendBuffer.length() > 0)
    {
        sendToServer( m_sendBuffer );
        m_sendBuffer = "";
    }
}

QString LCDProcClient::expandString(const QString &aString) const
{
    if ( m_pVersion != LCD_VERSION_5)
        return aString;

    // if version 5 then white space seperate the list of characters
    auto add_ws = [](const QString& str, auto x){ return str + x + QString(" "); };
    return std::accumulate(aString.cbegin(), aString.cend(), QString(), add_ws);
}

void LCDProcClient::loadSettings()
{
    if (!m_lcdReady )
        return;

    QString aString;
    QString old_keystring = m_lcdKeyString;

    m_timeFormat = gCoreContext->GetSetting("LCDTimeFormat", "");
    if ( m_timeFormat.isEmpty())
        m_timeFormat = gCoreContext->GetSetting("TimeFormat", "h:mm AP");

    m_dateFormat = gCoreContext->GetSetting("DateFormat", "dd.MM.yyyy");

    // Get LCD settings
    m_lcdShowMusic=(gCoreContext->GetSetting("LCDShowMusic", "1")=="1");
    m_lcdShowMusicItems=(gCoreContext->GetSetting("LCDShowMusicItems", "ArtistAlbumTitle"));
    m_lcdShowTime=(gCoreContext->GetSetting("LCDShowTime", "1")=="1");
    m_lcdShowChannel=(gCoreContext->GetSetting("LCDShowChannel", "1")=="1");
    m_lcdShowGeneric=(gCoreContext->GetSetting("LCDShowGeneric", "1")=="1");
    m_lcdShowVolume=(gCoreContext->GetSetting("LCDShowVolume", "1")=="1");
    m_lcdShowMenu=(gCoreContext->GetSetting("LCDShowMenu", "1")=="1");
    m_lcdShowRecstatus=(gCoreContext->GetSetting("LCDShowRecStatus", "1")=="1");
    m_lcdBacklightOn=(gCoreContext->GetSetting("LCDBacklightOn", "1")=="1");
    m_lcdHeartbeatOn=(gCoreContext->GetSetting("LCDHeartBeatOn", "1")=="1");
    m_lcdPopupTime = gCoreContext->GetDurSetting<std::chrono::seconds>("LCDPopupTime", 5s);
    m_lcdBigClock = (gCoreContext->GetSetting("LCDBigClock", "1")=="1");
    m_lcdKeyString = gCoreContext->GetSetting("LCDKeyString", "ABCDEF");

    if (!old_keystring.isEmpty())
    {
        aString = "client_del_key " + expandString(old_keystring);
        sendToServer(aString);
    }

    aString = "client_add_key " + expandString( m_lcdKeyString );
    sendToServer(aString);

    setHeartbeat ("Time", m_lcdHeartbeatOn );
    if ( m_lcdBacklightOn )
        sendToServer("screen_set Time backlight on");
    else
        sendToServer("screen_set Time backlight off");

    setHeartbeat ("Menu", m_lcdHeartbeatOn );
    sendToServer("screen_set Menu backlight on");

    setHeartbeat ("Music", m_lcdHeartbeatOn );
    sendToServer("screen_set Music backlight on");

    setHeartbeat ("Channel", m_lcdHeartbeatOn );
    sendToServer("screen_set Channel backlight on");

    setHeartbeat ("Generic", m_lcdHeartbeatOn );
    sendToServer("screen_set Generic backlight on");

    setHeartbeat ("Volume", m_lcdHeartbeatOn );
    sendToServer("screen_set Volume backlight on");

    setHeartbeat ("RecStatus", m_lcdHeartbeatOn );
    sendToServer("screen_set RecStatus backlight on");
}

void LCDProcClient::showStartupMessage(void)
{
    QList<LCDTextItem> textItems;

    QStringList list = formatScrollerText( m_startupMessage );

    int startrow = 1;
    if (list.count() < (int) m_lcdHeight )
        startrow = (( m_lcdHeight - list.count()) / 2) + 1;

    for (int x = 0; x < list.count(); x++)
    {
        if (x == (int) m_lcdHeight )
            break;
        textItems.append(LCDTextItem(x + startrow, ALIGN_LEFT, list[x],
                    "Generic", false));
    }

    switchToGeneric(&textItems);

    m_showMessageTimer->start( m_startupShowTime );
}

void LCDProcClient::removeStartupMessage(void)
{
    switchToTime();
}

void LCDProcClient::setStartupMessage(QString msg, std::chrono::seconds messagetime)
{
    m_startupMessage = std::move(msg);
    m_startupShowTime = messagetime;
}

void LCDProcClient::setWidth(unsigned int x)
{
    if (x < 1 || x > 80)
        return;
    m_lcdWidth = x;
}

void LCDProcClient::setHeight(unsigned int x)
{
    if (x < 1 || x > 80)
        return;
    m_lcdHeight = x;
}

void LCDProcClient::setCellWidth(unsigned int x)
{
    if (x < 1 || x > 16)
        return;
    m_cellWidth = x;
}

void LCDProcClient::setCellHeight(unsigned int x)
{
    if (x < 1 || x > 16)
        return;
    m_cellHeight = x;
}

void LCDProcClient::setVersion(const QString &sversion, const QString &pversion)
{
    m_protocolVersion = pversion;
    m_serverVersion = sversion;

    // the pVersion number is used internally to designate which protocol
    // version LCDd is using:

    if ( m_serverVersion.startsWith ("CVS-current") ||
            m_serverVersion.startsWith ("0.5"))
    {
        // Latest CVS versions of LCDd has priorities switched
        m_pVersion = LCD_VERSION_5;
        m_prioTop = "input";
        m_prioUrgent = "alert";
        m_prioHigh = "foreground";
        m_prioMedium = "info";
        m_prioLow = "background";
        m_prioOff = "hidden";
    }
    else
    {
        m_pVersion = LCD_VERSION_4;
        m_prioTop = "64";
        m_prioUrgent = "128";
        m_prioHigh = "240";
        m_prioMedium = "248";
        m_prioLow = "252";
        m_prioOff = "255";
    }
}

void LCDProcClient::describeServer()
{
    if (debug_level > 0)
    {
        LOG(VB_GENERAL, LOG_INFO,
            QString("LCDProcClient: The server is %1x%2 with each cell "
                    "being %3x%4.")
            .arg( m_lcdWidth ).arg( m_lcdHeight ).arg( m_cellWidth ).arg( m_cellHeight ));
        LOG(VB_GENERAL, LOG_INFO,
            QString("LCDProcClient: LCDd version %1, protocol version %2.")
            .arg( m_serverVersion, m_protocolVersion ));
    }

    if (debug_level > 1)
    {
        LOG(VB_GENERAL, LOG_INFO,
            QString("LCDProcClient: MythTV LCD settings:"));
        LOG(VB_GENERAL, LOG_INFO,
            QString("LCDProcClient: - showmusic      : %1")
                .arg( m_lcdShowMusic ));
        LOG(VB_GENERAL, LOG_INFO,
            QString("LCDProcClient: - showmusicitems : %1")
                .arg( m_lcdShowMusicItems ));
        LOG(VB_GENERAL, LOG_INFO,
            QString("LCDProcClient: - showtime       : %1")
                .arg( m_lcdShowTime ));
        LOG(VB_GENERAL, LOG_INFO,
            QString("LCDProcClient: - showchannel    : %1")
                .arg( m_lcdShowChannel ));
        LOG(VB_GENERAL, LOG_INFO,
            QString("LCDProcClient: - showrecstatus  : %1")
                .arg( m_lcdShowRecstatus ));
        LOG(VB_GENERAL, LOG_INFO,
            QString("LCDProcClient: - showgeneric    : %1")
                .arg( m_lcdShowGeneric ));
        LOG(VB_GENERAL, LOG_INFO,
            QString("LCDProcClient: - showvolume     : %1")
                .arg( m_lcdShowVolume ));
        LOG(VB_GENERAL, LOG_INFO,
            QString("LCDProcClient: - showmenu       : %1")
                .arg( m_lcdShowMenu ));
        LOG(VB_GENERAL, LOG_INFO,
            QString("LCDProcClient: - backlighton    : %1")
                .arg( m_lcdBacklightOn ));
        LOG(VB_GENERAL, LOG_INFO,
            QString("LCDProcClient: - heartbeaton    : %1")
                .arg( m_lcdHeartbeatOn ));
        LOG(VB_GENERAL, LOG_INFO,
            QString("LCDProcClient: - popuptime      : %1")
                    .arg( m_lcdPopupTime.count() ));
    }
}

void LCDProcClient::veryBadThings(QAbstractSocket::SocketError /*error*/)
{
    // Deal with failures to connect and inabilities to communicate
    LOG(VB_GENERAL, LOG_ERR, QString("Could not connect to LCDd: %1")
            .arg(m_socket->errorString()));
    m_socket->close();
}

void LCDProcClient::scrollList()
{
    if ( m_scrollListItems.count() == 0)
        return;

    if (m_activeScreen != m_scrollListScreen )
        return;

    outputLeftText( m_scrollListScreen, m_scrollListItems[m_scrollListItem],
                     m_scrollListWidget, m_scrollListRow );

    m_scrollListItem++;
    if ((int) m_scrollListItem >= m_scrollListItems.count())
        m_scrollListItem = 0;
}

void LCDProcClient::stopAll()
{
    // The usual reason things would get this far and then lcd_ready being
    // false is the connection died and we're trying to re-establish the
    // connection
    if (debug_level > 1)
        LOG(VB_GENERAL, LOG_INFO, "LCDProcClient: stopAll");

    if ( m_lcdReady )
    {
        setPriority("Time", OFF);
        setPriority("Music", OFF);
        setPriority("Channel", OFF);
        setPriority("Generic", OFF);
        setPriority("Volume", OFF);
        setPriority("Menu", OFF);
        setPriority("RecStatus", OFF);
    }

    m_timeTimer->stop();
    m_preScrollWTimer->stop();
    m_scrollWTimer->stop();
    m_popMenuTimer->stop();
    m_menuScrollTimer->stop();
    m_menuPreScrollTimer->stop();
    m_recStatusTimer->stop();
    m_scrollListTimer->stop();

    unPopMenu();
}

void LCDProcClient::startTime()
{
    setPriority("Time", MEDIUM);
    setPriority("RecStatus", LOW);

    m_timeTimer->start(1s);
    outputTime();
    m_activeScreen = "Time";
    m_isTimeVisible = true;

    if ( m_lcdShowRecstatus && m_isRecording )
        m_recStatusTimer->start(LCD_TIME_TIME);
}

void LCDProcClient::outputText(QList<LCDTextItem> *textItems)
{
    if (!m_lcdReady )
        return;

    QList<LCDTextItem>::iterator it = textItems->begin();
    QString num;
    unsigned int counter = 1;

    // Do the definable scrolling in here.
    // Use asignScrollingWidgets(curItem->getText(), "textWidget" + num);
    // When scrolling is set, alignment has no effect
    while (it != textItems->end() && counter < m_lcdHeight )
    {
        LCDTextItem *curItem = &(*it);
        ++it;
        num.setNum(curItem->getRow());

        if (curItem->getScroll())
        {
            assignScrollingWidgets(curItem->getText(), curItem->getScreen(),
                                "textWidget" + num, curItem->getRow());
        }
        else
        {
            switch (curItem->getAlignment())
            {
                case ALIGN_LEFT:
                    outputLeftText(curItem->getScreen(), curItem->getText(),
                                   "textWidget" + num, curItem->getRow());
                    break;
                case ALIGN_RIGHT:
                    outputRightText(curItem->getScreen(), curItem->getText(),
                                    "textWidget" + num, curItem->getRow());
                    break;
                case ALIGN_CENTERED:
                    outputCenteredText(curItem->getScreen(), curItem->getText(),
                                       "textWidget" + num, curItem->getRow());
                    break;
                default: break;
            }
        }

        ++counter;
    }
}

void LCDProcClient::outputCenteredText(const QString& theScreen, QString theText, const QString& widget,
                             int row)
{
    QString aString;
    unsigned int x = 0;

    x = (( m_lcdWidth - theText.length()) / 2) + 1;

    if (x > m_lcdWidth )
        x = 1;

    aString = "widget_set ";
    aString += theScreen;
    aString += " " + widget + " ";
    aString += QString::number(x);
    aString += " ";
    aString += QString::number(row);
    aString += " \"";
    aString += theText.replace ('"', "\"");
    aString += "\"";
    sendToServer(aString);
}

void LCDProcClient::outputLeftText(const QString& theScreen, QString theText, const QString& widget,
                         int row)
{
    QString aString;
    aString = "widget_set ";
    aString += theScreen;
    aString += " " + widget + " 1 ";
    aString += QString::number(row);
    aString += " \"";
    aString += theText.replace ('"', "\"");
    aString += "\"";
    sendToServer(aString);
}

void LCDProcClient::outputRightText(const QString& theScreen, QString theText, const QString& widget,
                          int row)
{
    QString aString;
    unsigned int x = (int)( m_lcdWidth - theText.length()) + 1;

    aString = "widget_set ";
    aString += theScreen;
    aString += " " + widget + " ";
    aString += QString::number(x);
    aString += " ";
    aString += QString::number(row);
    aString += " \"";
    aString += theText.replace ('"', "\"");
    aString += "\"";
    sendToServer(aString);
}

void LCDProcClient::assignScrollingList(QStringList theList, QString theScreen,
                              QString theWidget, int theRow)
{
    m_scrollListScreen = std::move(theScreen);
    m_scrollListWidget = std::move(theWidget);
    m_scrollListRow = theRow;
    m_scrollListItems = std::move(theList);

    m_scrollListItem = 0;
    scrollList();
    m_scrollListTimer->start(LCD_SCROLLLIST_TIME);
}

//
// Prepare for scrolling one or more text widgets on a single screen.
// Notes:
//    - Before assigning the first text, call: lcdTextItems->clear();
//    - After assigning the last text, call: formatScrollingWidgets()
// That's it ;-)

void LCDProcClient::assignScrollingWidgets(const QString& theText, const QString& theScreen,
                              const QString& theWidget, int theRow)
{
    m_scrollScreen = theScreen;

    // Alignment is not used...
    m_lcdTextItems->append(LCDTextItem(theRow, ALIGN_LEFT, theText,
                                     theScreen, true, theWidget));
}

void LCDProcClient::formatScrollingWidgets()
{
    m_scrollWTimer->stop();
    m_preScrollWTimer->stop();

    if ( m_lcdTextItems->isEmpty())
        return; // Weird...

    // Get the length of the longest item to scroll
    auto longest = [](int cur, const auto & item)
        { return std::max(cur, static_cast<int>(item.getText().length())); };
    int max_len = std::accumulate(m_lcdTextItems->cbegin(), m_lcdTextItems->cend(),
                                  0, longest);

    // Make all scrollable items the same lenght and do the initial output
    auto it = m_lcdTextItems->begin();
    while (it != m_lcdTextItems->end())
    {
        LCDTextItem *curItem = &(*it);
        ++it;
        if (curItem->getText().length() > (int) m_lcdWidth )
        {
            QString temp;
            QString temp2;
            temp = temp.fill(QChar(' '), max_len - curItem->getText().length());
            temp2 = temp2.fill(QChar(' '), m_lcdWidth );
            curItem->setText(temp2 + curItem->getText() + temp);
            outputLeftText( m_scrollScreen,
                        curItem->getText().mid( m_lcdWidth, max_len),
                        curItem->getWidget(), curItem->getRow());
        }
        else
        {
            curItem->setScrollable(false);
            outputCenteredText( m_scrollScreen, curItem->getText(),
                        curItem->getWidget(), curItem->getRow());
        }
    }

    if (max_len <= (int) m_lcdWidth )
        // We're done, no scrolling
        return;

    m_preScrollWTimer->start(2s);
}

void LCDProcClient::beginScrollingWidgets()
{
    m_scrollPosition = m_lcdWidth;
    m_preScrollWTimer->stop();
    m_scrollWTimer->start(400ms);
}

void LCDProcClient::scrollWidgets()
{
    if (m_activeScreen != m_scrollScreen )
        return;

    if ( m_lcdTextItems->isEmpty())
        return; // Weird...

    unsigned int len = 0;
    for (const auto & item : std::as_const(*m_lcdTextItems))
    {
        if (item.getScroll())
        {
            // Note that all scrollable items have the same length!
            len = item.getText().length();

            outputLeftText( m_scrollScreen,
                        item.getText().mid( m_scrollPosition, m_lcdWidth ),
                        item.getWidget(), item.getRow());
        }
    }

    if (len == 0)
    {
        // Shouldn't happen, but....
        LOG(VB_GENERAL, LOG_ERR,
            "LCDProcClient::scrollWidgets called without scrollable items");
        m_scrollWTimer->stop();
        return;
    }
    m_scrollPosition++;
    if ( m_scrollPosition >= len)
        m_scrollPosition = m_lcdWidth;
}

void LCDProcClient::startMusic(QString artist, const QString& album, const QString& track)
{
    // Playing music displays:
    // For 1-line displays:
    //       <ArtistAlbumTitle>
    // For 2-line displays:
    //       <ArtistAlbumTitle>
    //       <Elapse/Remaining Time>
    // For 3-line displays:
    //       <ArtistAlbumTitle>
    //       <Elapse/Remaining Time>
    //       <Info+ProgressBar>
    // For displays with more than 3 lines:
    //       <ArtistAlbum>
    //       <Title>
    //       <Elapse/Remaining Time>
    //       <Info+ProgressBar>

    // Clear the progressBar and timeWidget before activating the Music
    // screen. This prevents the display of outdated junk.
    sendToServer("widget_set Music progressBar 1 1 0");
    sendToServer("widget_set Music timeWidget 1 1 \"\"");
    m_lcdTextItems->clear();

    m_musicProgress = 0.0F;
    QString aString = std::move(artist);
    if ( m_lcdShowMusicItems == "ArtistAlbumTitle")
    {
        aString += " [";
        aString += album;
        aString += "] ";
    }
    else if ( m_lcdHeight < 4)
    {
        aString += " - ";
    }

    if ( m_lcdHeight < 4)
    {
        aString += track;
    }
    else
    {
        assignScrollingWidgets(track, "Music", "topWidget2", 2);
    }
    assignScrollingWidgets(aString, "Music", "topWidget1", 1);
    formatScrollingWidgets();

    // OK, everything is at least somewhat initialised. Activate
    // the music screen.
    m_activeScreen = "Music";
    if ( m_lcdShowMusic )
      setPriority("Music", HIGH);
}

void LCDProcClient::startChannel(const QString& channum, const QString& title, const QString& subtitle)
{
    QString aString;

    if ( m_lcdShowChannel )
        setPriority("Channel", HIGH);

    m_activeScreen = "Channel";

    if ( m_lcdHeight <= 2)
    {
        aString = channum + "|" + title;
        if (!subtitle.isEmpty())
            aString += "|" + subtitle;
        QStringList list = formatScrollerText(aString);
        assignScrollingList(list, "Channel", "topWidget", 1);
    }
    else
    {
        aString = channum;
        m_lcdTextItems->clear();
        assignScrollingWidgets(aString, "Channel", "topWidget", 1);
        aString = title;
        if (subtitle.length() > 0)
        {
            aString += " - '";
            aString += subtitle;
            aString += "'";
        }
        assignScrollingWidgets(aString, "Channel", "botWidget", 2);
        formatScrollingWidgets();
    }

    m_channelTime = "";
    m_progress = 0.0;
    outputChannel();
}

void LCDProcClient::startGeneric(QList<LCDTextItem> *textItems)
{
    QList<LCDTextItem>::iterator it = textItems->begin();
    LCDTextItem *curItem = &(*it);

    if ( m_lcdShowGeneric )
        setPriority("Generic", TOP);

    // Clear out the LCD.  Do this before checking if its empty incase the user
    //  wants to just clear the lcd
    outputLeftText("Generic", "", "textWidget1", 1);
    outputLeftText("Generic", "", "textWidget2", 2);
    outputLeftText("Generic", "", "textWidget3", 3);

    // If nothing, return without setting up the timer, etc
    if (textItems->isEmpty())
        return;

    m_activeScreen = "Generic";

    m_busyProgress = false;
    m_busyPos = 1;
    m_busyDirection = 1;
    m_busyIndicatorSize = 2.0F;
    m_genericProgress = 0.0;

    // Todo, make scrolling definable in LCDTextItem
    ++it;


    // Weird observations:
    //  - The first item is always assumed 'scrolling', for this
    //    item, the scrollable property is ignored...
    //  - Why output line 1, progressbar, rest of lines? Why not
    //    all outputlines, progressbar? That way, outputText() can
    //    just handle the whole thing and the 'pop off' stuff can go.
    //
    m_lcdTextItems->clear();
    assignScrollingWidgets(curItem->getText(), "Generic",
                           "textWidget1", curItem->getRow());

    outputGeneric();

    // Pop off the first item so item one isn't written twice
    textItems->removeFirst();
    if (!textItems->isEmpty())
        outputText(textItems);
    formatScrollingWidgets();
}

void LCDProcClient::startMenu(QList<LCDMenuItem> *menuItems, QString app_name,
                    bool popMenu)
{
    // Now do the menu items
    if (menuItems->isEmpty())
        return;

    QString aString;

    // Stop the scrolling if the menu has changed
    m_menuScrollTimer->stop();

    // Menu is higher priority than volume
    if ( m_lcdShowMenu )
        setPriority("Menu", URGENT);

    // Write out the app name
    if ( m_lcdHeight > 1)
        outputCenteredText("Menu", std::move(app_name), "topWidget", 1);

    QList<LCDMenuItem>::iterator it = menuItems->begin();

    // First loop through and figure out where the selected item is in the
    // list so we know how many above and below to display
    unsigned int selectedItem = 0;
    unsigned int counter = 0;
    bool oneSelected = false;

    while (it != menuItems->end())
    {
        LCDMenuItem *curItem = &(*it);
        ++it;
        if (curItem->isSelected() && !oneSelected)
        {
            selectedItem = counter + 1;
            oneSelected  = true;
            break;
        }
        ++counter;
    }

    // If there isn't one selected item, then write it on the display and return
    if (!oneSelected)
    {
        sendToServer("widget_set Menu topWidget 1 1 \"No menu item selected\"");
        sendToServer("widget_set Menu menuWidget1 1 2 \"     ABORTING     \"");
        m_menuScrollTimer->stop();
        return;
    }

    m_popMenuTimer->stop();
    // Start the unPop timer if this is a popup menu
    if (popMenu)
        m_popMenuTimer->start( m_lcdPopupTime );

    // QPtrListIterator doesn't contain a deep copy constructor. . .
    // This will contain a copy of the menuItems for scrolling purposes
    QList<LCDMenuItem>::iterator itTemp = menuItems->begin();
    m_lcdMenuItems->clear();
    counter = 1;
    while (itTemp != menuItems->end())
    {
        LCDMenuItem *curItem = &(*itTemp);
        ++itTemp;
        m_lcdMenuItems->append(LCDMenuItem(curItem->isSelected(),
                             curItem->isChecked(), curItem->ItemName(),
                             curItem->getIndent()));
        ++counter;
    }

    // If there is only one or two lines on the display, then just write the selected
    //  item and leave
    if ( m_lcdHeight <= 2)
    {
        it = menuItems->begin();
        while (it != menuItems->end())
        {
            LCDMenuItem *curItem = &(*it);
            ++it;
            if (curItem->isSelected())
            {
                // Set the scroll flag if necessary, otherwise set it to false
                if (curItem->ItemName().length()  > (int)( m_lcdWidth -lcdStartCol))
                {
                    m_menuPreScrollTimer->setSingleShot(true);
                    m_menuPreScrollTimer->start(2s);
                    curItem->setScroll(true);
                }
                else
                {
                    m_menuPreScrollTimer->stop();
                    curItem->setScroll(false);
                }
                if ( m_lcdHeight == 2)
                {
                    aString  = "widget_set Menu menuWidget1 1 2 \">";
                }
                else
                {
                    aString  = "widget_set Menu menuWidget1 1 1 \"";
                }

                if (lcdStartCol != 0)
                {
                    switch (curItem->isChecked())
                    {
                        case CHECKED: aString += "X "; break;
                        case UNCHECKED: aString += "O "; break;
                        case NOTCHECKABLE: aString += "  "; break;
                        default: break;
                    }
                }

                aString += curItem->ItemName().left( m_lcdWidth - lcdStartCol) +
                           "\"";
                sendToServer(aString);
                return;
            }
        }

        return;
    }

    // Reset things
    counter = 1;
    it = menuItems->begin();

    // Move the iterator to selectedItem lcdHeight/2, if > 1, -1.
    unsigned int midPoint = ( m_lcdHeight/2) - 1;
    if (selectedItem > midPoint && menuItems->size() >= (int) m_lcdHeight-1)
    {
        while (counter != selectedItem)
        {
            ++it;
            ++counter;
        }
        it -= midPoint;
        counter -= midPoint;
    }

    // Back up one if we're at the end so the last item shows up at the bottom
    // of the display
    if (counter + midPoint > menuItems->size() - midPoint && counter > midPoint)
    {
        it -= (counter + ( m_lcdHeight / 2) - 1) - (menuItems->size() - midPoint);
    }

    counter = 1;
    while (it != menuItems->end())
    {
        LCDMenuItem *curItem = &(*it);
        // Can't write more menu items then we have on the display
        if ((counter + 1) > m_lcdHeight )
            break;

        ++it;

        aString  = "widget_set Menu menuWidget";
        aString += QString::number(counter) + " 1 ";
        aString += QString::number(counter + 1) + " \"";

        if (curItem->isSelected())
            aString += ">";
        else
            aString += " ";

        switch (curItem->isChecked())
        {
            case CHECKED: aString += "X "; break;
            case UNCHECKED: aString += "O "; break;
            case NOTCHECKABLE: aString += "  "; break;
            default: break;
        }

        aString += curItem->ItemName().left( m_lcdWidth - lcdStartCol) + "\"";
        sendToServer(aString);

        ++counter;
    }

    // Make sure to clear out the rest of the screen
    while (counter < m_lcdHeight )
    {
        aString  = "widget_set Menu menuWidget";
        aString += QString::number(counter) + " 1 ";
        aString += QString::number(counter + 1) + " \"\"";
        sendToServer(aString);

        ++counter;
    }

    m_menuPreScrollTimer->setSingleShot(true);
    m_menuPreScrollTimer->start(2s);
}

void LCDProcClient::beginScrollingMenuText()
{
    // If there are items to scroll, wait 2 seconds for the user to read whats
    // already there

    if (!m_lcdMenuItems )
        return;

    m_menuScrollPosition = 1;

    QList<LCDMenuItem>::iterator it = m_lcdMenuItems->begin();

    QString temp;
    // Loop through and prepend everything with enough spaces
    // for smooth scrolling, and update the position
    while (it != m_lcdMenuItems->end())
    {
        LCDMenuItem *curItem = &(*it);
        ++it;
        // Don't setup for smooth scrolling if the item isn't long enough
        // (It causes problems with items being scrolled when they shouldn't)
        if (curItem->ItemName().length()  > (int)( m_lcdWidth - lcdStartCol))
        {
            temp = temp.fill(QChar(' '), m_lcdWidth - curItem->getIndent() -
                             lcdStartCol);
            curItem->setItemName(temp + curItem->ItemName());
            curItem->setScrollPos(curItem->getIndent() + temp.length());
            curItem->setScroll(true);
        }
        else
        {
            curItem->setScroll(false);
        }
    }

    // Can get segfaults if we try to start a timer thats already running. . .
    m_menuScrollTimer->stop();
    m_menuScrollTimer->start(250ms);
}

void LCDProcClient::scrollMenuText()
{
    if (!m_lcdMenuItems )
        return;

    QString aString;
    QString bString;
    QList<LCDMenuItem>::iterator it = m_lcdMenuItems->begin();

    ++m_menuScrollPosition;

    // First loop through and figure out where the selected item is in the
    // list so we know how many above and below to display
    unsigned int selectedItem = 0;
    unsigned int counter = 0;

    while (it != m_lcdMenuItems->end())
    {
        LCDMenuItem *curItem = &(*it);
        ++it;
        if (curItem->isSelected())
        {
            selectedItem = counter + 1;
            break;
        }
        ++counter;
    }

    // If there is only one or two lines on the display, then just write
    // the selected item and leave
    it = m_lcdMenuItems->begin();
    if ( m_lcdHeight <= 2)
    {
        while (it != m_lcdMenuItems->end())
        {
            LCDMenuItem *curItem = &(*it);
            ++it;
            if (curItem->isSelected())
            {
                curItem->incrementScrollPos();
                if ((int)curItem->getScrollPos() > curItem->ItemName().length())
                {
                    // Scroll slower second and subsequent times through
                    m_menuScrollTimer->stop();
                    m_menuScrollTimer->start(500ms);
                    curItem->setScrollPos(curItem->getIndent());
                }

                // Stop the timer if this item really doesn't need to scroll.
                // This should never have to get invoked because in theory
                // the code in startMenu has done its job. . .
                if (curItem->ItemName().length()  < (int)( m_lcdWidth - lcdStartCol))
                    m_menuScrollTimer->stop();

                if ( m_lcdHeight == 2)
                {
                    aString  = "widget_set Menu menuWidget1 1 2 \">";
                }
                else
                {
                    aString  = "widget_set Menu menuWidget1 1 1 \"";
                }

                if ( m_lcdWidth < 12)
                {
                    switch(curItem->isChecked())
                    {
                        case CHECKED: aString += "X"; break;
                        case UNCHECKED: aString += "O"; break;
                        case NOTCHECKABLE: aString += ""; break;
                        default: break;
                    }
                }
                else
                {
                    switch(curItem->isChecked())
                    {
                        case CHECKED: aString += "X "; break;
                        case UNCHECKED: aString += "O "; break;
                        case NOTCHECKABLE: aString += "  "; break;
                        default: break;
                    }
                }

                // Indent this item if nessicary
                aString += bString.fill(' ', curItem->getIndent());

                aString += curItem->ItemName().mid(curItem->getScrollPos(),
                                                   ( m_lcdWidth - lcdStartCol));
                aString += "\"";
                sendToServer(aString);
                return;
            }
        }

        return;
    }

    // Find the longest line, if menuScrollPosition is longer then this, then
    // reset them all
    it = m_lcdMenuItems->begin();
    int longest_line = 0;
    int max_scroll_pos = 0;

    while (it != m_lcdMenuItems->end())
    {
        LCDMenuItem *curItem = &(*it);
        ++it;
        longest_line = std::max(
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
            curItem->ItemName().length(),
#else
            static_cast<int>(curItem->ItemName().length()),
#endif
            longest_line);

        if ((int)curItem->getScrollPos() > max_scroll_pos)
            max_scroll_pos = curItem->getScrollPos();
    }

    // If max_scroll_pos > longest_line then reset
    if (max_scroll_pos > longest_line)
    {
        // Scroll slower second and subsequent times through
        m_menuScrollTimer->stop();
        m_menuScrollTimer->start(500ms);
        m_menuScrollPosition = 0;

        it = m_lcdMenuItems->begin();
        while (it != m_lcdMenuItems->end())
        {
            LCDMenuItem *curItem = &(*it);
            ++it;
            curItem->setScrollPos(curItem->getIndent());
        }
    }

    // Reset things
    counter = 1;
    it = m_lcdMenuItems->begin();

    // Move the iterator to selectedItem -1
    if (selectedItem != 1 && m_lcdMenuItems->size() >= (int) m_lcdHeight )
    {
        while (counter != selectedItem)
        {
            ++it;
            ++counter;
        }
        --it;
    }

    // Back up one if were at the end so the last item shows up at the bottom
    // of the display
    if (counter > 1 && (int)counter == m_lcdMenuItems->size())
        --it;

    bool stopTimer = true;

    counter = 1;
    while (it != m_lcdMenuItems->end() && counter <= m_lcdHeight )
    {
        LCDMenuItem *curItem = &(*it);
        // Can't write more menu items then we have on the display
        if ((counter + 1) > m_lcdHeight )
            break;

        ++it;

        if (curItem->Scroll())
        {
            stopTimer = false;
            aString  = "widget_set Menu menuWidget";
            aString += QString::number(counter) + " 1 ";
            aString += QString::number(counter + 1) + " \"";

            if (curItem->isSelected())
                aString += ">";
            else
                aString += " ";

            switch (curItem->isChecked())
            {
                case CHECKED: aString += "X "; break;
                case UNCHECKED: aString += "O "; break;
                case NOTCHECKABLE: aString += "  "; break;
                default: break;
            }

            // Indent this item if nessicary
            bString = "";
            bString.fill(' ', curItem->getIndent());
            aString += bString;

            // Increment the scroll position counter for this item
            curItem->incrementScrollPos();

            if ((int)curItem->getScrollPos() <= longest_line)
                aString += curItem->ItemName().mid(curItem->getScrollPos(),
                                                   ( m_lcdWidth-lcdStartCol));

            aString += "\"";
            sendToServer(aString);
        }

        ++counter;
    }

    // If there are no items to scroll, don't waste our time
    if (stopTimer)
        m_menuScrollTimer->stop();
}

void LCDProcClient::startVolume(const QString& app_name)
{
    if ( m_lcdShowVolume )
      setPriority("Volume", TOP);
    if ( m_lcdHeight > 1)
        outputCenteredText("Volume", "MythTV " + app_name + " Volume");
    m_volumeLevel = 0.0;

    outputVolume();
}

void LCDProcClient::unPopMenu()
{
    // Stop the scrolling timer
    m_menuScrollTimer->stop();
    setPriority("Menu", OFF);
}

void LCDProcClient::setChannelProgress(const QString &time, float value)
{
    if (!m_lcdReady )
        return;

    m_progress = value;
    m_channelTime = time;

    if ( m_progress < 0.0F)
        m_progress = 0.0F;
    else if ( m_progress > 1.0F)
        m_progress = 1.0F;

    outputChannel();
}

void LCDProcClient::setGenericProgress(bool b, float value)
{
    if (!m_lcdReady )
        return;

    m_genericProgress = value;

    if ( m_genericProgress < 0.0F)
        m_genericProgress = 0.0F;
    else if ( m_genericProgress > 1.0F)
        m_genericProgress = 1.0F;

    // Note, this will let us switch to/from busy indicator by
    // alternating between being passed true or false for b.
    m_busyProgress = b;
    if ( m_busyProgress )
    {
        // If we're at either end of the line, switch direction
        if (( m_busyPos + m_busyDirection >
             (signed int) m_lcdWidth - m_busyIndicatorSize ) ||
            ( m_busyPos + m_busyDirection < 1))
        {
            m_busyDirection = -m_busyDirection;
        }
        m_busyPos += m_busyDirection;
        m_genericProgress = m_busyIndicatorSize / (float) m_lcdWidth;
    }
    else
    {
        m_busyPos = 1;
    }

    outputGeneric();
}

void LCDProcClient::setMusicProgress(QString time, float value)
{
    if (!m_lcdReady )
        return;

    m_musicProgress = value;
    m_musicTime = std::move(time);

    if ( m_musicProgress < 0.0F)
        m_musicProgress = 0.0F;
    else if ( m_musicProgress > 1.0F)
        m_musicProgress = 1.0F;

    outputMusic();
}

void LCDProcClient::setMusicRepeat (int repeat)
{
    if (!m_lcdReady )
        return;

    m_musicRepeat = repeat;

    outputMusic ();
}

void LCDProcClient::setMusicShuffle (int shuffle)
{
    if (!m_lcdReady )
        return;

    m_musicShuffle = shuffle;

    outputMusic ();
}

void LCDProcClient::setVolumeLevel(float value)
{
    if (!m_lcdReady )
        return;

    m_volumeLevel = value;

    m_volumeLevel = std::clamp(m_volumeLevel, 0.0F, 1.0F);

    outputVolume();
}

void LCDProcClient::updateLEDs(int mask)
{
    QString aString;
    aString = "output ";
    aString += QString::number(mask);
    sendToServer(aString);
}

void LCDProcClient::reset()
{
    removeWidgets();
    loadSettings();
    init();
}

void LCDProcClient::dobigclock (void)
{
    QString aString;
    QString time = QTime::currentTime().toString( m_timeFormat );
    int toffset = 0;
    int xoffset = 0;

    // kludge ahead: use illegal number (11) to clear num display type

    // kluge - Uses string length to determine time format for parsing
    // 1:00     = 4 characters  =  24-hour format, 1 digit hour
    // 12:00    = 5 characters  =  24-hour format, 2 digit hour
    // 1:00 am  = 7 characters  =  12-hour format, 1 digit hour
    // 12:00 am = 8 characters  =  12-hour format, 2 digit hour
    if ((time.length() == 8) || (time.length() == 5))
       toffset = 1;

    // if 12-hour clock, add AM/PM indicator to end of the 2nd line
    if (time.length() > 6)
    {
       aString = time.at(5 + toffset);
       aString += time.at(6 + toffset);
       xoffset = 1;
    }
    else
    {
       aString = "  ";
    }
    outputRightText("Time", aString, "ampm", m_lcdHeight - 1);

    if ( m_isRecording )
    {
         outputLeftText("Time","R","rec1",1);
         outputLeftText("Time","E","rec2",2);
         outputLeftText("Time","C","rec3",3);
         aString = QString::number((int) m_tunerList.size());
         outputLeftText("Time",aString,"recCnt",4);

    }
    else
    {
         outputLeftText("Time"," ","rec1",1);
         outputLeftText("Time"," ","rec2",2);
         outputLeftText("Time"," ","rec3",3);
         outputLeftText("Time"," ","recCnt",4);
    }

    // Add Hour 10's Digit
    aString = "widget_set Time d0 ";
    aString += QString::number( (m_lcdWidth/2) - 5 - xoffset) + " ";
    if (toffset == 0)
        aString += "11";
    else
        aString += time.at(0);
    sendToServer(aString);

    // Add Hour 1's Digit
    aString = "widget_set Time d1 ";
    aString += QString::number( (m_lcdWidth/2) - 2 - xoffset) + " ";
    aString += time.at(0 + toffset);
    sendToServer(aString);

    // Add the Colon
    aString = "widget_set Time sep ";
    aString += QString::number( (m_lcdWidth/2) + 1 - xoffset);
    aString += " 10";   // 10 means: colon
    sendToServer(aString);

    // Add Minute 10's Digit
    aString = "widget_set Time d2 ";
    aString += QString::number( (m_lcdWidth/2) + 2 - xoffset) + " ";
    aString += time.at(2 + toffset);
    sendToServer(aString);

    // Add Minute 1's Digit
    aString = "widget_set Time d3 ";
    aString += QString::number( (m_lcdWidth/2) + 5 - xoffset) + " ";
    aString += time.at(3 + toffset);
    sendToServer(aString);

    // Added a flashing dot in the bottom-right corner
    if ( m_timeFlash )
    {
        outputRightText("Time", ".", "dot", m_lcdHeight );
        m_timeFlash = false;
    }
    else
    {
        outputRightText("Time", " ", "dot", m_lcdHeight );
        m_timeFlash = true;
    }
}

void LCDProcClient::outputTime()
{
    if ( m_lcdBigClock )
        dobigclock();
    else
        dostdclock();
}

void LCDProcClient::dostdclock()
{
    if (!m_lcdShowTime )
        return;

    if ( m_lcdShowRecstatus && m_isRecording )
    {
        outputCenteredText("Time", tr("RECORDING"), "topWidget", 1);
    }
    else
    {
        outputCenteredText(
            "Time", MythDate::current().toLocalTime().toString( m_dateFormat ),
            "topWidget", 1);
    }

    QString aString;
    int x = 0;
    int y = 0;

    if ( m_lcdHeight < 3)
        y = m_lcdHeight;
    else
        y = (int) std::rint( m_lcdHeight / 2) + 1;

    QString time = QTime::currentTime().toString( m_timeFormat );
    x = (( m_lcdWidth - time.length()) / 2) + 1;
    aString = "widget_set Time timeWidget ";
    aString += QString::number(x);
    aString += " ";
    aString += QString::number(y);
    aString += " \"";
    if ( m_lcdShowTime ) {
        aString += time + "\"";
        if ( m_timeFlash )
        {
            aString = aString.replace(':', ' ');
            m_timeFlash = false;
        }
        else
        {
            m_timeFlash = true;
        }
    }
    else
    {
        aString += " \"";
    }
    sendToServer(aString);
}

// if one or more recordings are taking place we alternate between
// showing the time and the recording status screen
void LCDProcClient::outputRecStatus(void)
{
    if (!m_lcdReady || !m_isRecording || !m_lcdShowRecstatus )
        return;

    if ( m_isTimeVisible || !m_lcdShowTime )
    {
        // switch to the rec status screen
        setPriority("RecStatus", MEDIUM);
        setPriority("Time", LOW);

        m_timeTimer->stop();
        m_scrollWTimer->stop();
        m_scrollListTimer->stop();
        m_isTimeVisible = false;
        m_activeScreen = "RecStatus";

        if (m_lcdTunerNo > (int) m_tunerList.size() - 1)
            m_lcdTunerNo = 0;
    }
    else if ( m_lcdTunerNo > (int) m_tunerList.size() - 1)
    {
        m_lcdTunerNo = 0;

        // switch to the time screen
        setPriority("Time", MEDIUM);
        setPriority("RecStatus", LOW);

        m_timeTimer->start(1s);
        m_scrollWTimer->stop();
        m_scrollListTimer->stop();
        m_recStatusTimer->start(LCD_TIME_TIME);

        outputTime();
        m_activeScreen = "Time";
        m_isTimeVisible = true;

        return;
    }  

    QString aString;
    QString status;
    QStringList list;
    std::chrono::milliseconds listTime { 1 };

    TunerStatus tuner = m_tunerList[m_lcdTunerNo];

    m_scrollListItems.clear();
    if ( m_lcdHeight >= 4)
    {
        //  LINE 1 - "R" + Channel
        status = tr("R ");
        status += tuner.channame;
        outputLeftText("RecStatus", status, "textWidget1", 1);

        //  LINE 2 - "E" + Program Title
        status = tr("E ");
        status += tuner.title;
        outputLeftText("RecStatus", status, "textWidget2", 2);
        //list = formatScrollerText(status);
        //assignScrollingList(list, "RecStatus", "textWidget2", 2);

        //  LINE 3 - "C" + Program Subtitle
        status = tr("C ");
        status += tuner.subtitle;
        outputLeftText("RecStatus", status, "textWidget3", 3);
        //list = formatScrollerText(status);
        //assignScrollingList(list, "RecStatus", "textWidget3", 3);

        //  LINE 4 - hh:mm-hh:mm + Progress Bar
        status = tuner.startTime.toLocalTime().toString("hh:mm") + "-" +
            tuner.endTime.toLocalTime().toString("hh:mm");
        outputLeftText("RecStatus", status, "textWidget4", 4);

        int length = tuner.startTime.secsTo(tuner.endTime);
        int delta = tuner.startTime.secsTo(MythDate::current());
        double rec_progress = (double) delta / length;

        aString = "widget_set RecStatus progressBar 13 ";
        aString += QString::number( m_lcdHeight );
        aString += " ";
        aString += QString::number((int)rint(rec_progress * ( m_lcdWidth - 13) *
                                     m_cellWidth ));
        sendToServer(aString);

        listTime = list.count() * LCD_SCROLLLIST_TIME * 2;
    }
    else
    {
        status = tr("RECORDING|");
        status += tuner.title;
        if (!tuner.subtitle.isEmpty())
            status += "|(" + tuner.subtitle + ")";

        status += "|" + tuner.startTime.toLocalTime().toString("hh:mm")
            + " to " +
            tuner.endTime.toLocalTime().toString("hh:mm");

        list = formatScrollerText(status);
        assignScrollingList(list, "RecStatus", "textWidget1", 1);

        if ( m_lcdHeight > 1)
        {
            int length = tuner.startTime.secsTo(tuner.endTime);
            int delta = tuner.startTime.secsTo(MythDate::current());
            double rec_progress = (double) delta / length;

            aString = "widget_set RecStatus progressBar 1 ";
            aString += QString::number( m_lcdHeight );
            aString += " ";
            aString += QString::number((int)rint(rec_progress * m_lcdWidth *
                                         m_cellWidth ));
            sendToServer(aString);
        }
        else
        {
            sendToServer("widget_set RecStatus progressBar 1 1 0");
        }

        listTime = list.count() * LCD_SCROLLLIST_TIME * 2;
    }

    if (listTime < LCD_TIME_TIME)
        listTime = LCD_TIME_TIME;

    m_recStatusTimer->start(listTime);
    m_lcdTunerNo++;
}

void LCDProcClient::outputScrollerText(const QString& theScreen, const QString& theText,
                             const QString& widget, int top, int bottom)
{
    QString aString;
    aString =  "widget_set " + theScreen + " " + widget;
    aString += " 1 ";
    aString += QString::number(top) + " ";
    aString += QString::number( m_lcdWidth ) + " ";
    aString += QString::number(bottom);
    aString += " v 8 \"" + theText + "\"";

    sendToServer(aString);
}

QStringList LCDProcClient::formatScrollerText(const QString &text) const
{
    QString separators = " |-_/:('<~";
    QStringList lines;

    int lastSplit = 0;
    QString line = "";

    for (const auto& x : std::as_const(text))
    {
        if (separators.contains(x))
            lastSplit = line.length();

        line += x;
        if (line.length() > (int) m_lcdWidth || x == '|')
        {
            QString formatedLine;
            formatedLine.fill(' ', m_lcdWidth );
            formatedLine = formatedLine.replace(( m_lcdWidth - lastSplit) / 2,
                     lastSplit, line.left(lastSplit));

            lines.append(formatedLine);

            if (line[lastSplit] == ' ' || line[lastSplit] == '|')
                line = line.mid(lastSplit + 1);
            else
                line = line.mid(lastSplit);

            lastSplit = m_lcdWidth;
        }
    }

    // make sure we add the last line
    QString formatedLine;
    formatedLine.fill(' ', m_lcdWidth );
    formatedLine = formatedLine.replace(( m_lcdWidth - line.length()) / 2,
                line.length(), line);

    lines.append(formatedLine);

    return lines;
}

void LCDProcClient::outputMusic()
{
    // See startMusic() for a discription of the Music screen contents

    outputCenteredText("Music", m_musicTime, "timeWidget",
                                m_lcdHeight < 4 ? 2 : 3);

    if ( m_lcdHeight > 2)
    {
        QString aString;
        QString shuffle = "";
        QString repeat  = "";
        int info_width = 0;

        if ( m_musicShuffle == 1)
        {
            shuffle = "S:? ";
        }
        else if ( m_musicShuffle == 2)
        {
            shuffle = "S:i ";
        }
        else if ( m_musicShuffle == 3)
        {
            shuffle = "S:a ";
        }

        if ( m_musicRepeat == 1)
        {
            repeat = "R:1 ";
        }
        else if ( m_musicRepeat == 2)
        {
            repeat = "R:* ";
        }

        if (shuffle.length() != 0 || repeat.length() != 0)
        {
            aString = shuffle + repeat;
            info_width = aString.length();
            outputLeftText("Music", aString, "infoWidget", m_lcdHeight );
        }
        else
        {
            outputLeftText("Music", "        ", "infoWidget", m_lcdHeight );
        }

        aString = "widget_set Music progressBar ";
        aString += QString::number(info_width + 1);
        aString += " ";
        aString += QString::number( m_lcdHeight );
        aString += " ";
        aString += QString::number((int)std::rint( m_musicProgress *
                                        ( m_lcdWidth - info_width) * m_cellWidth ));
        sendToServer(aString);
    }
}

void LCDProcClient::outputChannel()
{
    if ( m_lcdHeight > 1)
    {
        QString aString;
        aString = "widget_set Channel progressBar 1 ";
        aString += QString::number( m_lcdHeight );
        aString += " ";
        aString += QString::number((int)std::rint( m_progress * m_lcdWidth * m_cellWidth ));
        sendToServer(aString);

        if ( m_lcdHeight >= 4)
            outputCenteredText("Channel", m_channelTime, "timeWidget", 3);
    }
    else
    {
        sendToServer("widget_set Channel progressBar 1 1 0");
    }
}

void LCDProcClient::outputGeneric()
{
    if ( m_lcdHeight > 1)
    {
        QString aString;
        aString = "widget_set Generic progressBar ";
        aString += QString::number ( m_busyPos );
        aString += " ";
        aString += QString::number( m_lcdHeight );
        aString += " ";
        aString += QString::number((int)std::rint( m_genericProgress * m_lcdWidth *
                                                   m_cellWidth ));
        sendToServer(aString);
    }
    else
    {
        sendToServer("widget_set Generic progressBar 1 1 0");
    }
}

void LCDProcClient::outputVolume()
{
    QString aString;
    int line = 3;

    if ( m_lcdHeight > 1)
    {
        aString = "widget_set Volume progressBar 1 ";
        aString += QString::number( m_lcdHeight );
        aString += " ";
        aString += QString::number((int)std::rint( m_volumeLevel * m_lcdWidth * m_cellWidth ));
        sendToServer(aString);
    }

    aString = QString::number((int)( m_volumeLevel * 100));
    aString += "%";

    if ( m_lcdHeight > 3)
        line = 3;
    else
        line = m_lcdHeight;
    outputRightText("Volume", aString, "botWidget", line);
}

void LCDProcClient::switchToTime()
{
    if (!m_lcdReady )
        return;

    stopAll();

    if (debug_level > 1)
        LOG(VB_GENERAL, LOG_INFO, "LCDProcClient: switchToTime");

    startTime();
}

void LCDProcClient::switchToMusic(const QString &artist, const QString &album, const QString &track)
{
    if (!m_lcdReady )
        return;

    stopAll();

    if (debug_level > 1)
        LOG(VB_GENERAL, LOG_INFO, "LCDProcClient: switchToMusic") ;

    startMusic(artist, album, track);
}

void LCDProcClient::switchToChannel(const QString& channum, const QString& title, const QString& subtitle)
{
    if (!m_lcdReady )
        return;

    stopAll();

    if (debug_level > 1)
        LOG(VB_GENERAL, LOG_INFO, "LCDProcClient: switchToChannel");

    startChannel(channum, title, subtitle);
}

void LCDProcClient::switchToMenu(QList<LCDMenuItem> *menuItems, const QString& app_name,
                                 bool popMenu)
{
    if (!m_lcdReady )
        return;

    if (debug_level > 1)
        LOG(VB_GENERAL, LOG_INFO, "LCDProcClient: switchToMenu");

    startMenu(menuItems, app_name, popMenu);
}

void LCDProcClient::switchToGeneric(QList<LCDTextItem> *textItems)
{
    if (!m_lcdReady )
        return;
    stopAll();

    if (debug_level > 1)
        LOG(VB_GENERAL, LOG_INFO, "LCDProcClient: switchToGeneric");

    startGeneric(textItems);
}

void LCDProcClient::switchToVolume(const QString& app_name)
{
    if (!m_lcdReady )
        return;

    stopAll();

    if (debug_level > 1)
        LOG(VB_GENERAL, LOG_INFO, "LCDProcClient: switchToVolume");

    startVolume(app_name);
}

void LCDProcClient::switchToNothing()
{
    if (!m_lcdReady )
        return;

    stopAll();

    if (debug_level > 1)
        LOG(VB_GENERAL, LOG_INFO, "LCDProcClient: switchToNothing");
}

void LCDProcClient::shutdown()
{
    if (debug_level > 1)
        LOG(VB_GENERAL, LOG_INFO, "LCDProcClient: shutdown");

    stopAll();

    // Remove all the widgets and screens for a clean exit from the server
    removeWidgets();

    m_socket->close();

    m_lcdReady = false;
    m_connected = false;
}

void LCDProcClient::removeWidgets()
{
    sendToServer("widget_del Channel progressBar");
    sendToServer("widget_del Channel topWidget");
    sendToServer("widget_del Channel timeWidget");
    sendToServer("screen_del Channel");

    sendToServer("widget_del Generic progressBar");
    sendToServer("widget_del Generic textWidget1");
    sendToServer("widget_del Generic textWidget2");
    sendToServer("widget_del Generic textWidget3");
    sendToServer("screen_del Generic");

    sendToServer("widget_del Volume progressBar");
    sendToServer("widget_del Volume topWidget");
    sendToServer("screen_del Volume");

    sendToServer("widget_del Menu topWidget");
    sendToServer("widget_del Menu menuWidget1");
    sendToServer("widget_del Menu menuWidget2");
    sendToServer("widget_del Menu menuWidget3");
    sendToServer("widget_del Menu menuWidget4");
    sendToServer("widget_del Menu menuWidget5");
    sendToServer("screen_del Menu");

    sendToServer("widget_del Music progressBar");
    sendToServer("widget_del Music infoWidget");
    sendToServer("widget_del Music timeWidget");
    sendToServer("widget_del Music topWidget");
    sendToServer("screen_del Music");

    if ( m_lcdBigClock )
    {
        sendToServer("widget_del Time rec1");
        sendToServer("widget_del Time rec2");
        sendToServer("widget_del Time rec3");
        sendToServer("widget_del Time recCnt");
        sendToServer("widget_del Time d0");
        sendToServer("widget_del Time d1");
        sendToServer("widget_del Time sep");
        sendToServer("widget_del Time d2");
        sendToServer("widget_del Time d3");
        sendToServer("widget_del Time ampm");
        sendToServer("widget_del Time dot");
    }
    else
    {
        sendToServer("widget_del Time timeWidget");
        sendToServer("widget_del Time topWidget");
    }

    sendToServer("screen_del Time");

    sendToServer("widget_del RecStatus textWidget1");
    sendToServer("widget_del RecStatus textWidget2");
    sendToServer("widget_del RecStatus textWidget3");
    sendToServer("widget_del RecStatus textWidget4");
    sendToServer("widget_del RecStatus progressBar");
}

LCDProcClient::~LCDProcClient()
{
    if (debug_level > 1)
    {
        LOG(VB_GENERAL, LOG_INFO,
            "LCDProcClient: An LCD device is being snuffed out"
            "of existence (~LCDProcClient() was called)");
    }

    if (m_socket)
    {
        delete m_socket;
        m_lcdReady = false;
    }

    delete m_lcdMenuItems;

    gCoreContext->removeListener(this);
}

void LCDProcClient::customEvent(QEvent *e)
{
    if (e->type() == MythEvent::kMythEventMessage)
    {
        auto *me = dynamic_cast<MythEvent *>(e);
        if (me == nullptr)
            return;

        if (me->Message().startsWith("RECORDING_LIST_CHANGE") ||
            me->Message() == "UPDATE_PROG_INFO")
        {
            if ( m_lcdShowRecstatus && !m_updateRecInfoTimer->isActive())
            {
               if (debug_level > 1)
                   LOG(VB_GENERAL, LOG_INFO,
                       "LCDProcClient: Received recording list change");

                // we can't query the backend from inside the customEvent
                // so fire the recording list update from a timer
                m_updateRecInfoTimer->start(500ms);
            }
        }
    }
}

void LCDProcClient::updateRecordingList(void)
{
    m_tunerList.clear();
    m_isRecording = false;

    if (!gCoreContext->IsConnectedToMaster())
    {
        if (!gCoreContext->ConnectToMasterServer(false))
        {
            LOG(VB_GENERAL, LOG_ERR,
                "LCDProcClient: Cannot get recording status "
                "- is the master server running?\n\t\t\t"
                "Will retry in 30 seconds");
            QTimer::singleShot(30s, this, &LCDProcClient::updateRecordingList);

            // If we can't get the recording status and we're showing
            // it, switch back to time. Maybe it would be even better
            // to show that the backend is unreachable ?
            if (m_activeScreen == "RecStatus")
                switchToTime();
            return;
        }
    }

    m_isRecording = RemoteGetRecordingStatus(&m_tunerList, false);

    m_lcdTunerNo = 0;

    if (m_activeScreen == "Time" || m_activeScreen == "RecStatus")
        startTime();
}
/* vim: set expandtab tabstop=4 shiftwidth=4: */
