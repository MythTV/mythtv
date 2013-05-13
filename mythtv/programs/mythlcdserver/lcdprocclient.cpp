/*
    lcdprocclient.cpp

    a MythTV project object to control an
    LCDproc server

    (c) 2002, 2003 Thor Sigvaldason, Dan Morphis and Isaac Richards
*/

// c/c++
#include <unistd.h>
#include <stdlib.h>
#include <cmath>

//qt
#include <QCoreApplication>
#include <QEvent>
#include <QTimer>

// mythtv
#include "mythcontext.h"
#include "mythdbcon.h"
#include "mythdate.h"
#include "tv.h"
#include "compat.h"

//mythlcdserver
#include "lcdprocclient.h"
#include "lcdserver.h"
#include "lcddevice.h"

#define LCD_START_COL 3

#define LCD_VERSION_4 1
#define LCD_VERSION_5 2

#define LCD_RECSTATUS_TIME  10000
#define LCD_TIME_TIME       3000
#define LCD_SCROLLLIST_TIME 2000

int lcdStartCol = LCD_START_COL;

LCDProcClient::LCDProcClient(LCDServer *lparent) : QObject(NULL)
{
    // Constructor for LCDProcClient
    //
    // Note that this does *not* include opening the socket and initiating
    // communications with the LDCd daemon.

    if (debug_level > 0)
        LOG(VB_GENERAL, LOG_INFO,
            "LCDProcClient: An LCDProcClient object now exists");

    socket = new QTcpSocket(this);
    connect(socket, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(veryBadThings(QAbstractSocket::SocketError)));
    connect(socket, SIGNAL(readyRead()), this, SLOT(serverSendingData()));

    m_parent = lparent;

    lcd_ready = false;

    lcdWidth = 5;
    lcdHeight = 1;
    cellWidth = 1;
    cellHeight = 1;
    lcdStartCol = LCD_START_COL;
    if (lcdWidth < 12)
    {
        if (lcdHeight == 1)
           lcdStartCol = 0;
        else
           lcdStartCol = 1;
    }

    hostname = "";
    port = 13666;

    timeFlash = false;
    scrollingText = "";
    progress = 0.0;
    generic_progress = 0.0;
    volume_level = 0.0;
    connected = false;
    send_buffer = "";
    lcdMenuItems = new QList<LCDMenuItem>;
    lcdTextItems = new QList<LCDTextItem>;

    timeTimer = new QTimer(this);
    connect(timeTimer, SIGNAL(timeout()), this, SLOT(outputTime()));

    scrollWTimer = new QTimer(this);
    connect(scrollWTimer, SIGNAL(timeout()), this, SLOT(scrollWidgets()));

    preScrollWTimer = new QTimer(this);
    preScrollWTimer->setSingleShot(true);
    connect(preScrollWTimer, SIGNAL(timeout()), this,
            SLOT(beginScrollingWidgets()));

    popMenuTimer = new QTimer(this);
    popMenuTimer->setSingleShot(true);
    connect(popMenuTimer, SIGNAL(timeout()), this, SLOT(unPopMenu()));

    menuScrollTimer = new QTimer(this);
    connect(menuScrollTimer, SIGNAL(timeout()), this, SLOT(scrollMenuText()));

    menuPreScrollTimer = new QTimer(this);
    connect(menuPreScrollTimer, SIGNAL(timeout()), this,
            SLOT(beginScrollingMenuText()));

    checkConnectionsTimer = new QTimer(this);
    connect(checkConnectionsTimer, SIGNAL(timeout()), this,
            SLOT(checkConnections()));
    checkConnectionsTimer->start(10000);

    recStatusTimer = new QTimer(this);
    connect(recStatusTimer, SIGNAL(timeout()), this, SLOT(outputRecStatus()));

    scrollListTimer = new QTimer(this);
    connect(scrollListTimer, SIGNAL(timeout()), this, SLOT(scrollList()));

    showMessageTimer = new QTimer(this);
    showMessageTimer->setSingleShot(true);
    connect(showMessageTimer, SIGNAL(timeout()), this,
            SLOT(removeStartupMessage()));

    updateRecInfoTimer = new QTimer(this);
    updateRecInfoTimer->setSingleShot(true);
    connect(updateRecInfoTimer, SIGNAL(timeout()), this,
            SLOT(updateRecordingList()));

    gCoreContext->addListener(this);

    isRecording = false;
}

bool LCDProcClient::SetupLCD ()
{
    QString lcd_host;
    int lcd_port;

    lcd_host = gCoreContext->GetSetting("LCDHost", "localhost");
    lcd_port = gCoreContext->GetNumSetting("LCDPort", 13666);

    if (lcd_host.length() > 0 && lcd_port > 1024)
        connectToHost(lcd_host, lcd_port);

    return connected;
}

bool LCDProcClient::connectToHost(const QString &lhostname, unsigned int lport)
{
    // Open communications
    // Store the hostname and port in case we need to reconnect.

    int timeout = 1000;
    hostname = lhostname;
    port = lport;

    // Don't even try to connect if we're currently disabled.
    if (!gCoreContext->GetNumSetting("LCDEnable", 0))
    {
        connected = false;
        return connected;
    }

    if (!connected)
    {
        QTextStream os(socket);
        socket->connectToHost(hostname, port);

        while (--timeout && socket->state() != QAbstractSocket::ConnectedState)
        {
            qApp->processEvents();
            usleep(1000);

            if (socket->state() == QAbstractSocket::ConnectedState)
            {
                connected = true;
                os << "hello\n";
                break;
            }
        }
    }

    return connected;
}

void LCDProcClient::sendToServer(const QString &someText)
{
    // Check the socket, make sure the connection is still up
    if (socket->state() != QAbstractSocket::ConnectedState)
    {
        if (!lcd_ready)
            return;

        lcd_ready = false;

        //Stop everything
        stopAll();

        // Ack, connection to server has been severed try to re-establish the
        // connection
        LOG(VB_GENERAL, LOG_ERR,
            "LCDProcClient: Connection to LCDd died unexpectedly.");
        return;
    }

    QTextStream os(socket);
    os.setCodec("ISO 8859-1");

    last_command = someText;

    if (connected)
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

        send_buffer += someText;
        send_buffer += "\n";
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
      aString += prioTop;
      break;
    case URGENT:
      aString += prioUrgent;
      break;
    case HIGH:
      aString += prioHigh;
      break;
    case MEDIUM:
      aString += prioMedium;
      break;
    case LOW:
      aString += prioLow;
      break;
    case OFF:
      aString += prioOff;
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
        if (pVersion == LCD_VERSION_4)
        {
            msg = "widget_add " + screen + " heartbeat";
        }
        if (pVersion == LCD_VERSION_5)
        {
            msg = "screen_set " + screen + " heartbeat on";
        }
    }
    else
    {
        if (pVersion == LCD_VERSION_4)
        {
            msg = "widget_del " + screen + " heartbeat";
        }
        if (pVersion == LCD_VERSION_5)
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
    if (socket->state() != QAbstractSocket::ConnectedState)
    {
        if (debug_level > 0)
            LOG(VB_GENERAL, LOG_INFO,
                "LCDProcClient: connecting to LCDProc server");

        lcd_ready = false;
        connected = false;

        // Retry to connect. . .  Maybe the user restarted LCDd?
        connectToHost(hostname, port);
    }
}

void LCDProcClient::serverSendingData()
{
    QString lineFromServer, tempString;
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

    while(socket->canReadLine())
    {
        lineFromServer = socket->readLine();
        lineFromServer = lineFromServer.replace( QRegExp("\n"), "" );
        lineFromServer = lineFromServer.replace( QRegExp("\r"), "" );

        if (debug_level > 0)
        // Make debugging be less noisy
            if (lineFromServer != "success")
                LOG(VB_NETWORK, LOG_INFO,
                    "LCDProcClient: Received from server: " + lineFromServer);

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
            LOG(VB_GENERAL, LOG_WARNING, "last command: " + last_command);
        }
        else if (aList.first() == "key")
        {
           if (m_parent)
               m_parent->sendKeyPress(aList.last().trimmed());
        }
    }
}

void LCDProcClient::init()
{
    QString aString;
    lcd_keystring = "";

    connected = true;

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
    for (unsigned int i = 1; i <= lcdHeight; i++)
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

    lcd_ready = true;
    loadSettings();

    // default to showing time
    switchToTime();

    updateRecordingList();

    // do we need to show the startup message
    if (!startup_message.isEmpty())
        showStartupMessage();

    // send buffer if there's anything in there
    if (send_buffer.length() > 0)
    {
        sendToServer(send_buffer);
        send_buffer = "";
    }
}

QString LCDProcClient::expandString(const QString &aString)
{
    if (pVersion != LCD_VERSION_5)
        return aString;

    QString bString;

    // if version 5 then white space seperate the list of characters
    for (int x = 0; x < aString.length(); x++)
    {
        bString += aString.at(x) + QString(" ");
    }

    return bString;
}

void LCDProcClient::loadSettings()
{
    if (!lcd_ready)
        return;

    QString aString;
    QString old_keystring = lcd_keystring;

    timeformat = gCoreContext->GetSetting("LCDTimeFormat", "");
    if (timeformat.isEmpty())
        timeformat = gCoreContext->GetSetting("TimeFormat", "h:mm AP");

    dateformat = gCoreContext->GetSetting("DateFormat", "dd.MM.yyyy");

    // Get LCD settings
    lcd_showmusic=(gCoreContext->GetSetting("LCDShowMusic", "1")=="1");
    lcd_showmusic_items=(gCoreContext->GetSetting("LCDShowMusicItems", "ArtistAlbumTitle"));
    lcd_showtime=(gCoreContext->GetSetting("LCDShowTime", "1")=="1");
    lcd_showchannel=(gCoreContext->GetSetting("LCDShowChannel", "1")=="1");
    lcd_showgeneric=(gCoreContext->GetSetting("LCDShowGeneric", "1")=="1");
    lcd_showvolume=(gCoreContext->GetSetting("LCDShowVolume", "1")=="1");
    lcd_showmenu=(gCoreContext->GetSetting("LCDShowMenu", "1")=="1");
    lcd_showrecstatus=(gCoreContext->GetSetting("LCDShowRecStatus", "1")=="1");
    lcd_backlighton=(gCoreContext->GetSetting("LCDBacklightOn", "1")=="1");
    lcd_heartbeaton=(gCoreContext->GetSetting("LCDHeartBeatOn", "1")=="1");
    aString = gCoreContext->GetSetting("LCDPopupTime", "5");
    lcd_popuptime = aString.toInt() * 1000;
    lcd_bigclock = (gCoreContext->GetSetting("LCDBigClock", "1")=="1");
    lcd_keystring = gCoreContext->GetSetting("LCDKeyString", "ABCDEF");

    if (!old_keystring.isEmpty())
    {
        aString = "client_del_key " + expandString(old_keystring);
        sendToServer(aString);
    }

    aString = "client_add_key " + expandString(lcd_keystring);
    sendToServer(aString);

    setHeartbeat ("Time", lcd_heartbeaton);
    if (lcd_backlighton)
        sendToServer("screen_set Time backlight on");
    else
        sendToServer("screen_set Time backlight off");

    setHeartbeat ("Menu", lcd_heartbeaton);
    sendToServer("screen_set Menu backlight on");

    setHeartbeat ("Music", lcd_heartbeaton);
    sendToServer("screen_set Music backlight on");

    setHeartbeat ("Channel", lcd_heartbeaton);
    sendToServer("screen_set Channel backlight on");

    setHeartbeat ("Generic", lcd_heartbeaton);
    sendToServer("screen_set Generic backlight on");

    setHeartbeat ("Volume", lcd_heartbeaton);
    sendToServer("screen_set Volume backlight on");

    setHeartbeat ("RecStatus", lcd_heartbeaton);
    sendToServer("screen_set RecStatus backlight on");
}

void LCDProcClient::showStartupMessage(void)
{
    QList<LCDTextItem> textItems;

    QStringList list = formatScrollerText(startup_message);

    int startrow = 1;
    if (list.count() < (int)lcdHeight)
        startrow = ((lcdHeight - list.count()) / 2) + 1;

    for (int x = 0; x < list.count(); x++)
    {
        if (x == (int)lcdHeight)
            break;
        textItems.append(LCDTextItem(x + startrow, ALIGN_LEFT, list[x],
                    "Generic", false));
    }

    switchToGeneric(&textItems);

    showMessageTimer->start(startup_showtime * 1000);
}

void LCDProcClient::removeStartupMessage(void)
{
    switchToTime();
}

void LCDProcClient::setStartupMessage(QString msg, uint messagetime)
{
    startup_message = msg;
    startup_showtime = messagetime;
}

void LCDProcClient::setWidth(unsigned int x)
{
    if (x < 1 || x > 80)
        return;
    else
        lcdWidth = x;
}

void LCDProcClient::setHeight(unsigned int x)
{
    if (x < 1 || x > 80)
        return;
    else
        lcdHeight = x;
}

void LCDProcClient::setCellWidth(unsigned int x)
{
    if (x < 1 || x > 16)
        return;
    else
        cellWidth = x;
}

void LCDProcClient::setCellHeight(unsigned int x)
{
    if (x < 1 || x > 16)
        return;
    else
        cellHeight = x;
}

void LCDProcClient::setVersion(const QString &sversion, const QString &pversion)
{
    protocolVersion = pversion;
    serverVersion = sversion;

    // the pVersion number is used internally to designate which protocol
    // version LCDd is using:

    if (serverVersion.startsWith ("CVS-current") ||
            serverVersion.startsWith ("0.5"))
    {
        // Latest CVS versions of LCDd has priorities switched
        pVersion = LCD_VERSION_5;
        prioTop = "input";
        prioUrgent = "alert";
        prioHigh = "foreground";
        prioMedium = "info";
        prioLow = "background";
        prioOff = "hidden";
    }
    else
    {
        pVersion = LCD_VERSION_4;
        prioTop = "64";
        prioUrgent = "128";
        prioHigh = "240";
        prioMedium = "248";
        prioLow = "252";
        prioOff = "255";
    }
}

void LCDProcClient::describeServer()
{
    if (debug_level > 0)
    {
        LOG(VB_GENERAL, LOG_INFO,
            QString("LCDProcClient: The server is %1x%2 with each cell "
                    "being %3x%4.")
            .arg(lcdWidth).arg(lcdHeight).arg(cellWidth).arg(cellHeight));
        LOG(VB_GENERAL, LOG_INFO,
            QString("LCDProcClient: LCDd version %1, protocol version %2.")
            .arg(serverVersion).arg(protocolVersion));
    }

    if (debug_level > 1)
    {
        LOG(VB_GENERAL, LOG_INFO,
            QString("LCDProcClient: MythTV LCD settings:"));
        LOG(VB_GENERAL, LOG_INFO,
            QString("LCDProcClient: - showmusic      : %1")
                .arg(lcd_showmusic));
        LOG(VB_GENERAL, LOG_INFO,
            QString("LCDProcClient: - showmusicitems : %1")
                .arg(lcd_showmusic_items));
        LOG(VB_GENERAL, LOG_INFO,
            QString("LCDProcClient: - showtime       : %1")
                .arg(lcd_showtime));
        LOG(VB_GENERAL, LOG_INFO,
            QString("LCDProcClient: - showchannel    : %1")
                .arg(lcd_showchannel));
        LOG(VB_GENERAL, LOG_INFO,
            QString("LCDProcClient: - showrecstatus  : %1")
                .arg(lcd_showrecstatus));
        LOG(VB_GENERAL, LOG_INFO,
            QString("LCDProcClient: - showgeneric    : %1")
                .arg(lcd_showgeneric));
        LOG(VB_GENERAL, LOG_INFO,
            QString("LCDProcClient: - showvolume     : %1")
                .arg(lcd_showvolume));
        LOG(VB_GENERAL, LOG_INFO,
            QString("LCDProcClient: - showmenu       : %1")
                .arg(lcd_showmenu));
        LOG(VB_GENERAL, LOG_INFO,
            QString("LCDProcClient: - backlighton    : %1")
                .arg(lcd_backlighton));
        LOG(VB_GENERAL, LOG_INFO,
            QString("LCDProcClient: - heartbeaton    : %1")
                .arg(lcd_heartbeaton));
        LOG(VB_GENERAL, LOG_INFO,
            QString("LCDProcClient: - popuptime      : %1")
                    .arg(lcd_popuptime));
    }
}

void LCDProcClient::veryBadThings(QAbstractSocket::SocketError error)
{
    // Deal with failures to connect and inabilities to communicate
    LOG(VB_GENERAL, LOG_ERR, QString("Could not connect to LCDd: %1")
            .arg(socket->errorString()));
    socket->close();
}

void LCDProcClient::scrollList()
{
    if (scrollListItems.count() == 0)
        return;

    if (activeScreen != scrollListScreen)
        return;

    outputLeftText(scrollListScreen, scrollListItems[scrollListItem],
                    scrollListWidget, scrollListRow);

    scrollListItem++;
    if ((int)scrollListItem >= scrollListItems.count())
        scrollListItem = 0;
}

void LCDProcClient::stopAll()
{
    // The usual reason things would get this far and then lcd_ready being
    // false is the connection died and we're trying to re-establish the
    // connection
    if (debug_level > 1)
        LOG(VB_GENERAL, LOG_INFO, "LCDProcClient: stopAll");

    if (lcd_ready)
    {
        setPriority("Time", OFF);
        setPriority("Music", OFF);
        setPriority("Channel", OFF);
        setPriority("Generic", OFF);
        setPriority("Volume", OFF);
        setPriority("Menu", OFF);
        setPriority("RecStatus", OFF);
    }

    timeTimer->stop();
    preScrollWTimer->stop();
    scrollWTimer->stop();
    popMenuTimer->stop();
    menuScrollTimer->stop();
    menuPreScrollTimer->stop();
    recStatusTimer->stop();
    scrollListTimer->stop();

    unPopMenu();
}

void LCDProcClient::startTime()
{
    setPriority("Time", MEDIUM);
    setPriority("RecStatus", LOW);

    timeTimer->start(1000);
    outputTime();
    activeScreen = "Time";
    isTimeVisible = true;

    if (lcd_showrecstatus && isRecording)
        recStatusTimer->start(LCD_TIME_TIME);
}

void LCDProcClient::outputText(QList<LCDTextItem> *textItems)
{
    if (!lcd_ready)
        return;

    QList<LCDTextItem>::iterator it = textItems->begin();
    LCDTextItem *curItem;
    QString num;
    unsigned int counter = 1;

    // Do the definable scrolling in here.
    // Use asignScrollingWidgets(curItem->getText(), "textWidget" + num);
    // When scrolling is set, alignment has no effect
    while (it != textItems->end() && counter < lcdHeight)
    {
        curItem = &(*it);
        ++it;
        num.setNum(curItem->getRow());

        if (curItem->getScroll())
            assignScrollingWidgets(curItem->getText(), curItem->getScreen(),
                                "textWidget" + num, curItem->getRow());
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

void LCDProcClient::outputCenteredText(QString theScreen, QString theText, QString widget,
                             int row)
{
    QString aString;
    unsigned int x = 0;

    x = (lcdWidth - theText.length()) / 2 + 1;

    if (x > lcdWidth)
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

void LCDProcClient::outputLeftText(QString theScreen, QString theText, QString widget,
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

void LCDProcClient::outputRightText(QString theScreen, QString theText, QString widget,
                          int row)
{
    QString aString;
    unsigned int x;

    x = (int)(lcdWidth - theText.length()) + 1;

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
    scrollListScreen = theScreen;
    scrollListWidget = theWidget;
    scrollListRow = theRow;
    scrollListItems = theList;

    scrollListItem = 0;
    scrollList();
    scrollListTimer->start(LCD_SCROLLLIST_TIME);
}

//
// Prepare for scrolling one or more text widgets on a single screen.
// Notes:
//    - Before assigning the first text, call: lcdTextItems->clear();
//    - After assigning the last text, call: formatScrollingWidgets()
// That's it ;-)

void LCDProcClient::assignScrollingWidgets(QString theText, QString theScreen,
                              QString theWidget, int theRow)
{
    scrollScreen = theScreen;

    // Alignment is not used...
    lcdTextItems->append(LCDTextItem(theRow, ALIGN_LEFT, theText,
                                     theScreen, true, theWidget));
}

void LCDProcClient::formatScrollingWidgets()
{
    scrollWTimer->stop();
    preScrollWTimer->stop();

    if (lcdTextItems->isEmpty())
        return; // Weird...

    int max_len = 0;
    QList<LCDTextItem>::iterator it = lcdTextItems->begin();
    LCDTextItem *curItem;

    // Get the length of the longest item to scroll
    for(; it != lcdTextItems->end(); ++it)
    {
        curItem = &(*it);
        if (curItem->getText().length() > max_len)
            max_len = curItem->getText().length();
    }

    // Make all scrollable items the same lenght and do the initial output
    it = lcdTextItems->begin();
    while (it != lcdTextItems->end())
    {
        curItem = &(*it);
        ++it;
        if (curItem->getText().length() > (int)lcdWidth)
        {
            QString temp, temp2;
            temp = temp.fill(QChar(' '), max_len - curItem->getText().length());
            temp2 = temp2.fill(QChar(' '), lcdWidth);
            curItem->setText(temp2 + curItem->getText() + temp);
            outputLeftText(scrollScreen,
                        curItem->getText().mid(lcdWidth, max_len),
                        curItem->getWidget(), curItem->getRow());
        }
        else
        {
            curItem->setScrollable(false);
            outputCenteredText(scrollScreen, curItem->getText(),
                        curItem->getWidget(), curItem->getRow());
        }
    }

    if (max_len <= (int)lcdWidth)
        // We're done, no scrolling
        return;

    preScrollWTimer->start(2000);
}

void LCDProcClient::beginScrollingWidgets()
{
    scrollPosition = lcdWidth;
    preScrollWTimer->stop();
    scrollWTimer->start(400);
}

void LCDProcClient::scrollWidgets()
{
    if (activeScreen != scrollScreen)
        return;

    if (lcdTextItems->isEmpty())
        return; // Weird...

    QList<LCDTextItem>::iterator it = lcdTextItems->begin();
    LCDTextItem *curItem;

    unsigned int len = 0;
    for(; it != lcdTextItems->end(); ++it)
    {
        curItem = &(*it);
        if (curItem->getScroll())
        {
            // Note that all scrollable items have the same lenght!
            len = curItem->getText().length();

            outputLeftText(scrollScreen,
                        curItem->getText().mid(scrollPosition, lcdWidth),
                        curItem->getWidget(), curItem->getRow());
        }
    }

    if (len == 0)
    {
        // Shouldn't happen, but....
        LOG(VB_GENERAL, LOG_ERR,
            "LCDProcClient::scrollWidgets called without scrollable items");
        scrollWTimer->stop();
        return;
    }
    scrollPosition++;
    if (scrollPosition >= len)
        scrollPosition = lcdWidth;
}

void LCDProcClient::startMusic(QString artist, QString album, QString track)
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
    lcdTextItems->clear();

    QString aString;
    music_progress = 0.0f;
    aString = artist;
    if (lcd_showmusic_items == "ArtistAlbumTitle")
    {
        aString += " [";
        aString += album;
        aString += "] ";
    }
    else if (lcdHeight < 4)
    {
        aString += " - ";
    }

    if (lcdHeight < 4)
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
    activeScreen = "Music";
    if (lcd_showmusic)
      setPriority("Music", HIGH);
}

void LCDProcClient::startChannel(QString channum, QString title, QString subtitle)
{
    QString aString;

    if (lcd_showchannel)
        setPriority("Channel", HIGH);

    activeScreen = "Channel";

    if (lcdHeight <= 2)
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
        lcdTextItems->clear();
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

    channel_time = "";
    progress = 0.0;
    outputChannel();
}

void LCDProcClient::startGeneric(QList<LCDTextItem> *textItems)
{
    QList<LCDTextItem>::iterator it = textItems->begin();
    LCDTextItem *curItem = &(*it);

    QString aString;

    if (lcd_showgeneric)
        setPriority("Generic", TOP);

    // Clear out the LCD.  Do this before checking if its empty incase the user
    //  wants to just clear the lcd
    outputLeftText("Generic", "", "textWidget1", 1);
    outputLeftText("Generic", "", "textWidget2", 2);
    outputLeftText("Generic", "", "textWidget3", 3);

    // If nothing, return without setting up the timer, etc
    if (textItems->isEmpty())
        return;

    activeScreen = "Generic";

    busy_progress = false;
    busy_pos = 1;
    busy_direction = 1;
    busy_indicator_size = 2.0f;
    generic_progress = 0.0;

    // Return if there are no more items
    if (textItems->isEmpty())
        return;

    // Todo, make scrolling definable in LCDTextItem
    ++it;


    // Weird observations:
    //  - The first item is always assumed 'scrolling', for this
    //    item, the scrollable property is ignored...
    //  - Why output line 1, progressbar, rest of lines? Why not
    //    all outputlines, progressbar? That way, outputText() can
    //    just handle the whole thing and the 'pop off' stuff can go.
    //
    lcdTextItems->clear();
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
    menuScrollTimer->stop();

    // Menu is higher priority than volume
    if (lcd_showmenu)
        setPriority("Menu", URGENT);

    // Write out the app name
    if (lcdHeight > 1)
    outputCenteredText("Menu", app_name, "topWidget", 1);

    QList<LCDMenuItem>::iterator it = menuItems->begin();
    LCDMenuItem *curItem;

    // First loop through and figure out where the selected item is in the
    // list so we know how many above and below to display
    unsigned int selectedItem = 0;
    unsigned int counter = 0;
    bool oneSelected = false;

    while (it != menuItems->end())
    {
        curItem = &(*it);
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
        menuScrollTimer->stop();
        return;
    }

    popMenuTimer->stop();
    // Start the unPop timer if this is a popup menu
    if (popMenu)
        popMenuTimer->start(lcd_popuptime);

    // QPtrListIterator doesn't contain a deep copy constructor. . .
    // This will contain a copy of the menuItems for scrolling purposes
    QList<LCDMenuItem>::iterator itTemp = menuItems->begin();
    lcdMenuItems->clear();
    counter = 1;
    while (itTemp != menuItems->end())
    {
        curItem = &(*itTemp);
        ++itTemp;
        lcdMenuItems->append(LCDMenuItem(curItem->isSelected(),
                             curItem->isChecked(), curItem->ItemName(),
                             curItem->getIndent()));
        ++counter;
    }

    // If there is only one or two lines on the display, then just write the selected
    //  item and leave
    if (lcdHeight <= 2)
    {
        it = menuItems->begin();
        while (it != menuItems->end())
        {
            curItem = &(*it);
            ++it;
            if (curItem->isSelected())
            {
                // Set the scroll flag if necessary, otherwise set it to false
                if (curItem->ItemName().length()  > (int)(lcdWidth - lcdStartCol))
                {
                    menuPreScrollTimer->setSingleShot(true);
                    menuPreScrollTimer->start(2000);
                    curItem->setScroll(true);
                }
                else
                {
                    menuPreScrollTimer->stop();
                    curItem->setScroll(false);
                }
                if (lcdHeight == 2)
                {
                    aString  = "widget_set Menu menuWidget1 1 2 \">";
                }
                else
                {
                    aString  = "widget_set Menu menuWidget1 1 1 \"";
                }

                if (lcdStartCol == 1)  // small display -> don't waste space for additional spaces
                {
                    switch (curItem->isChecked())
                    {
                        case CHECKED: aString += "X "; break;
                        case UNCHECKED: aString += "O "; break;
                        case NOTCHECKABLE: aString += "  "; break;
                        default: break;
                    }
                }
                else if (lcdStartCol != 0)
                {
                    switch (curItem->isChecked())
                    {
                        case CHECKED: aString += "X "; break;
                        case UNCHECKED: aString += "O "; break;
                        case NOTCHECKABLE: aString += "  "; break;
                        default: break;
                    }
                }

                aString += curItem->ItemName().left(lcdWidth - lcdStartCol) +
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
    unsigned int midPoint = (lcdHeight/2) - 1;
    if (selectedItem > midPoint && menuItems->size() >= (int)lcdHeight-1)
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
        it -= (counter + (lcdHeight / 2) - 1) - (menuItems->size() - midPoint);
    }

    counter = 1;
    while (it != menuItems->end())
    {
        curItem = &(*it);
        // Can't write more menu items then we have on the display
        if ((counter + 1) > lcdHeight)
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

        aString += curItem->ItemName().left(lcdWidth - lcdStartCol) + "\"";
        sendToServer(aString);

        ++counter;
    }

    // Make sure to clear out the rest of the screen
    while (counter < lcdHeight)
    {
        aString  = "widget_set Menu menuWidget";
        aString += QString::number(counter) + " 1 ";
        aString += QString::number(counter + 1) + " \"\"";
        sendToServer(aString);

        ++counter;
    }

    menuPreScrollTimer->setSingleShot(true);
    menuPreScrollTimer->start(2000);
}

void LCDProcClient::beginScrollingMenuText()
{
    // If there are items to scroll, wait 2 seconds for the user to read whats
    // already there

    if (!lcdMenuItems)
        return;

    menuScrollPosition = 1;

    QList<LCDMenuItem>::iterator it = lcdMenuItems->begin();
    LCDMenuItem *curItem;

    QString temp;
    // Loop through and prepend everything with enough spaces
    // for smooth scrolling, and update the position
    while (it != lcdMenuItems->end())
    {
        curItem = &(*it);
        ++it;
        // Don't setup for smooth scrolling if the item isn't long enough
        // (It causes problems with items being scrolled when they shouldn't)
        if (curItem->ItemName().length()  > (int)(lcdWidth - lcdStartCol))
        {
            temp = temp.fill(QChar(' '), lcdWidth - curItem->getIndent() -
                             lcdStartCol);
            curItem->setItemName(temp + curItem->ItemName());
            curItem->setScrollPos(curItem->getIndent() + temp.length());
            curItem->setScroll(true);
        }
        else
            curItem->setScroll(false);
    }

    // Can get segfaults if we try to start a timer thats already running. . .
    menuScrollTimer->stop();
    menuScrollTimer->start(250);
}

void LCDProcClient::scrollMenuText()
{
    if (!lcdMenuItems)
        return;

    QString aString, bString;
    QList<LCDMenuItem>::iterator it = lcdMenuItems->begin();
    LCDMenuItem *curItem;

    ++menuScrollPosition;

    // First loop through and figure out where the selected item is in the
    // list so we know how many above and below to display
    unsigned int selectedItem = 0;
    unsigned int counter = 0;
    bool oneSelected = false;

    while (it != lcdMenuItems->end())
    {
        curItem = &(*it);
        ++it;
        if (curItem->isSelected() && !oneSelected)
        {
            selectedItem = counter + 1;
            oneSelected  = true;
            break;
        }
        ++counter;
    }

    // If there is only one or two lines on the display, then just write
    // the selected item and leave
    it = lcdMenuItems->begin();
    if (lcdHeight <= 2)
    {
        while (it != lcdMenuItems->end())
        {
            curItem = &(*it);
            ++it;
            if (curItem->isSelected())
            {
                curItem->incrementScrollPos();
                if ((int)curItem->getScrollPos() > curItem->ItemName().length())
                {
                    // Scroll slower second and subsequent times through
                    menuScrollTimer->stop();
                    menuScrollTimer->start(500);
                    curItem->setScrollPos(curItem->getIndent());
                }

                // Stop the timer if this item really doesn't need to scroll.
                // This should never have to get invoked because in theory
                // the code in startMenu has done its job. . .
                if (curItem->ItemName().length()  < (int)(lcdWidth - lcdStartCol))
                    menuScrollTimer->stop();

                if (lcdHeight == 2)
                {
                    aString  = "widget_set Menu menuWidget1 1 2 \">";
                }
                else
                {
                    aString  = "widget_set Menu menuWidget1 1 1 \"";
                }

                if (lcdWidth < 12)
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
                                                   (lcdWidth - lcdStartCol));
                aString += "\"";
                sendToServer(aString);
                return;
            }
        }

        return;
    }

    // Find the longest line, if menuScrollPosition is longer then this, then
    // reset them all
    it = lcdMenuItems->begin();
    int longest_line = 0;
    int max_scroll_pos = 0;

    while (it != lcdMenuItems->end())
    {
        curItem = &(*it);
        ++it;
        if (curItem->ItemName().length() > longest_line)
            longest_line = curItem->ItemName().length();

        if ((int)curItem->getScrollPos() > max_scroll_pos)
            max_scroll_pos = curItem->getScrollPos();
    }

    // If max_scroll_pos > longest_line then reset
    if (max_scroll_pos > longest_line)
    {
        // Scroll slower second and subsequent times through
        menuScrollTimer->stop();
        menuScrollTimer->start(500);
        menuScrollPosition = 0;

        it = lcdMenuItems->begin();
        while (it != lcdMenuItems->end())
        {
            curItem = &(*it);
            ++it;
            curItem->setScrollPos(curItem->getIndent());
        }
    }

    // Reset things
    counter = 1;
    it = lcdMenuItems->begin();

    // Move the iterator to selectedItem -1
    if (selectedItem != 1 && lcdMenuItems->size() >= (int)lcdHeight)
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
    if ((int)counter == lcdMenuItems->size())
        --it;

    bool stopTimer = true;

    counter = 1;
    while (it != lcdMenuItems->end() && counter <= lcdHeight)
    {
        curItem = &(*it);
        // Can't write more menu items then we have on the display
        if ((counter + 1) > lcdHeight)
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
                                                   (lcdWidth-lcdStartCol));

            aString += "\"";
            sendToServer(aString);
        }

        ++counter;
    }

    // If there are no items to scroll, don't waste our time
    if (stopTimer)
        menuScrollTimer->stop();
}

void LCDProcClient::startVolume(QString app_name)
{
    if (lcd_showvolume)
      setPriority("Volume", TOP);
    if (lcdHeight > 1)
        outputCenteredText("Volume", "MythTV " + app_name + " Volume");
    volume_level = 0.0;

    outputVolume();
}

void LCDProcClient::unPopMenu()
{
    // Stop the scrolling timer
    menuScrollTimer->stop();
    setPriority("Menu", OFF);
}

void LCDProcClient::setChannelProgress(const QString &time, float value)
{
    if (!lcd_ready)
        return;

    progress = value;
    channel_time = time;

    if (progress < 0.0)
        progress = 0.0;
    else if (progress > 1.0)
        progress = 1.0;

    outputChannel();
}

void LCDProcClient::setGenericProgress(bool b, float value)
{
    if (!lcd_ready)
        return;

    generic_progress = value;

    if (generic_progress < 0.0)
        generic_progress = 0.0;
    else if (generic_progress > 1.0)
        generic_progress = 1.0;

    // Note, this will let us switch to/from busy indicator by
    // alternating between being passed true or false for b.
    busy_progress = b;
    if (busy_progress)
    {
        // If we're at either end of the line, switch direction
        if ((busy_pos + busy_direction >
             (signed int)lcdWidth - busy_indicator_size) ||
            (busy_pos + busy_direction < 1))
        {
            busy_direction = -busy_direction;
        }
        busy_pos += busy_direction;
        generic_progress = busy_indicator_size / (float)lcdWidth;
    }
    else
    {
        busy_pos = 1;
    }

    outputGeneric();
}

void LCDProcClient::setMusicProgress(QString time, float value)
{
    if (!lcd_ready)
        return;

    music_progress = value;
    music_time = time;

    if (music_progress < 0.0)
        music_progress = 0.0;
    else if (music_progress > 1.0)
        music_progress = 1.0;

    outputMusic();
}

void LCDProcClient::setMusicRepeat (int repeat)
{
    if (!lcd_ready)
        return;

    music_repeat = repeat;

    outputMusic ();
}

void LCDProcClient::setMusicShuffle (int shuffle)
{
    if (!lcd_ready)
        return;

    music_shuffle = shuffle;

    outputMusic ();
}

void LCDProcClient::setVolumeLevel(float value)
{
    if (!lcd_ready)
        return;

    volume_level = value;

    if (volume_level < 0.0)
        volume_level = 0.0;
    if (volume_level > 1.0)
        volume_level = 1.0;

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

void LCDProcClient::dobigclock (bool init)
{
    QString aString;
    QString time = QTime::currentTime().toString(timeformat);
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
    outputRightText("Time", aString, "ampm", lcdHeight - 1);

    if (isRecording)
    {
         outputLeftText("Time","R","rec1",1);
         outputLeftText("Time","E","rec2",2);
         outputLeftText("Time","C","rec3",3);
         aString = QString::number((int) tunerList.size());
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
    aString += QString::number(lcdWidth/2 - 5 - xoffset) + " ";
    if (toffset == 0)
        aString += "11";
    else
        aString += time.at(0);
    sendToServer(aString);

    // Add Hour 1's Digit
    aString = "widget_set Time d1 ";
    aString += QString::number(lcdWidth/2 - 2 - xoffset) + " ";
    aString += time.at(0 + toffset);
    sendToServer(aString);

    // Add the Colon
    aString = "widget_set Time sep ";
    aString += QString::number(lcdWidth/2 + 1 - xoffset);
    aString += " 10";   // 10 means: colon
    sendToServer(aString);

    // Add Minute 10's Digit
    aString = "widget_set Time d2 ";
    aString += QString::number(lcdWidth/2 + 2 - xoffset) + " ";
    aString += time.at(2 + toffset);
    sendToServer(aString);

    // Add Minute 1's Digit
    aString = "widget_set Time d3 ";
    aString += QString::number(lcdWidth/2 + 5 - xoffset) + " ";
    aString += time.at(3 + toffset);
    sendToServer(aString);

    // Added a flashing dot in the bottom-right corner
    if (timeFlash)
    {
        outputRightText("Time", ".", "dot", lcdHeight);
        timeFlash = false;
    }
    else
    {
        outputRightText("Time", " ", "dot", lcdHeight);
        timeFlash = true;
    }
}

void LCDProcClient::outputTime()
{
    if (lcd_bigclock)
        dobigclock(0);
    else
        dostdclock();
}

void LCDProcClient::dostdclock()
{
    if (!lcd_showtime)
        return;

    if (lcd_showrecstatus && isRecording)
    {
        outputCenteredText("Time", tr("RECORDING"), "topWidget", 1);
    }
    else
    {
        outputCenteredText(
            "Time", MythDate::current().toLocalTime().toString(dateformat),
            "topWidget", 1);
    }

    QString aString;
    int x, y;

    if (lcdHeight < 3)
        y = lcdHeight;
    else
        y = (int) rint(lcdHeight / 2) + 1;

    QString time = QTime::currentTime().toString(timeformat);
    x = (lcdWidth - time.length()) / 2 + 1;
    aString = "widget_set Time timeWidget ";
    aString += QString::number(x);
    aString += " ";
    aString += QString::number(y);
    aString += " \"";
    if (lcd_showtime) {
        aString += time + "\"";
        if (timeFlash)
        {
            aString = aString.replace(QRegExp(":"), " ");
            timeFlash = false;
        }
        else
            timeFlash = true;
    }
    else
        aString += " \"";
    sendToServer(aString);
}

// if one or more recordings are taking place we alternate between
// showing the time and the recording status screen
void LCDProcClient::outputRecStatus(void)
{
    if (!lcd_ready || !isRecording || !lcd_showrecstatus)
        return;

    int listTime;

    if (isTimeVisible || !lcd_showtime)
    {
        // switch to the rec status screen
        setPriority("RecStatus", MEDIUM);
        setPriority("Time", LOW);

        timeTimer->stop();
        scrollWTimer->stop();
        scrollListTimer->stop();
        listTime = LCD_RECSTATUS_TIME;
        isTimeVisible = false;
        activeScreen = "RecStatus";
    }
    else if (lcdTunerNo > (int) tunerList.size() - 1)
    {
        lcdTunerNo = 0;

        // switch to the time screen
        setPriority("Time", MEDIUM);
        setPriority("RecStatus", LOW);

        timeTimer->start(1000);
        scrollWTimer->stop();
        scrollListTimer->stop();
        recStatusTimer->start(LCD_TIME_TIME);

        outputTime();
        activeScreen = "Time";
        isTimeVisible = true;

        return;
    }  

    QString aString, status;
    QStringList list;

    TunerStatus tuner = tunerList[lcdTunerNo];

    scrollListItems.clear();
    if (lcdHeight >= 4)
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
        aString += QString::number(lcdHeight);
        aString += " ";
        aString += QString::number((int)rint(rec_progress * (lcdWidth - 13) *
                                cellWidth));
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

        if (lcdHeight > 1)
        {
            int length = tuner.startTime.secsTo(tuner.endTime);
            int delta = tuner.startTime.secsTo(MythDate::current());
            double rec_progress = (double) delta / length;

            aString = "widget_set RecStatus progressBar 1 ";
            aString += QString::number(lcdHeight);
            aString += " ";
            aString += QString::number((int)rint(rec_progress * lcdWidth *
                                    cellWidth));
            sendToServer(aString);
        }
        else
            sendToServer("widget_set RecStatus progressBar 1 1 0");

        listTime = list.count() * LCD_SCROLLLIST_TIME * 2;
    }

    if (listTime < LCD_TIME_TIME)
        listTime = LCD_TIME_TIME;

    recStatusTimer->start(listTime);
    lcdTunerNo++;
}

void LCDProcClient::outputScrollerText(QString theScreen, QString theText,
                             QString widget, int top, int bottom)
{
    QString aString;
    aString =  "widget_set " + theScreen + " " + widget;
    aString += " 1 ";
    aString += QString::number(top) + " ";
    aString += QString::number(lcdWidth) + " ";
    aString += QString::number(bottom);
    aString += " v 8 \"" + theText + "\"";

    sendToServer(aString);
}

QStringList LCDProcClient::formatScrollerText(const QString &text)
{
    QString separators = " |-_/:('<~";
    QStringList lines;

    int lastSplit = 0;
    QString line = "";

    for (int x = 0; x < text.length(); x++)
    {
        if (separators.contains(text[x]) > 0)
            lastSplit = line.length();

        line += text[x];
        if (line.length() > (int)lcdWidth || text[x] == '|')
        {
            QString formatedLine;
            formatedLine.fill(' ', lcdWidth);
            formatedLine = formatedLine.replace((lcdWidth - lastSplit) / 2,
                     lastSplit, line.left(lastSplit));

            lines.append(formatedLine);

            if (line[lastSplit] == ' ' || line[lastSplit] == '|')
                line = line.mid(lastSplit + 1);
            else
                line = line.mid(lastSplit);

            lastSplit = lcdWidth;
        }
    }

    // make sure we add the last line
    QString formatedLine;
    formatedLine.fill(' ', lcdWidth);
    formatedLine = formatedLine.replace((lcdWidth - line.length()) / 2,
                line.length(), line);

    lines.append(formatedLine);

    return lines;
}

void LCDProcClient::outputMusic()
{
    int info_width = 0;

    // See startMusic() for a discription of the Music screen contents

    outputCenteredText("Music", music_time, "timeWidget",
                                lcdHeight < 4 ? 2 : 3);

    if (lcdHeight > 2)
    {
        QString aString;
        QString shuffle = "";
        QString repeat  = "";

        if (music_shuffle == 1)
        {
            shuffle = "S:? ";
        }
        else if (music_shuffle == 2)
        {
            shuffle = "S:i ";
        }
        else if (music_shuffle == 3)
        {
            shuffle = "S:a ";
        }

        if (music_repeat == 1)
        {
            repeat = "R:1 ";
        }
        else if (music_repeat == 2)
        {
            repeat = "R:* ";
        }

        if (shuffle.length() != 0 || repeat.length() != 0)
        {
            aString = shuffle + repeat;
            info_width = aString.length();
            outputLeftText("Music", aString, "infoWidget", lcdHeight);
        }
        else
            outputLeftText("Music", "        ", "infoWidget", lcdHeight);

        aString = "widget_set Music progressBar ";
        aString += QString::number(info_width + 1);
        aString += " ";
        aString += QString::number(lcdHeight);
        aString += " ";
        aString += QString::number((int)rint(music_progress *
                                        (lcdWidth - info_width) * cellWidth));
        sendToServer(aString);
    }
}

void LCDProcClient::outputChannel()
{
    if (lcdHeight > 1)
    {
        QString aString;
        aString = "widget_set Channel progressBar 1 ";
        aString += QString::number(lcdHeight);
        aString += " ";
        aString += QString::number((int)rint(progress * lcdWidth * cellWidth));
        sendToServer(aString);

        if (lcdHeight >= 4)
            outputCenteredText("Channel", channel_time, "timeWidget", 3);
    }
    else
        sendToServer("widget_set Channel progressBar 1 1 0");
}

void LCDProcClient::outputGeneric()
{
    if (lcdHeight > 1)
    {
    QString aString;
    aString = "widget_set Generic progressBar ";
    aString += QString::number (busy_pos);
    aString += " ";
    aString += QString::number(lcdHeight);
    aString += " ";
    aString += QString::number((int)rint(generic_progress * lcdWidth *
                               cellWidth));
    sendToServer(aString);
}
    else sendToServer("widget_set Generic progressBar 1 1 0");
}

void LCDProcClient::outputVolume()
{
    QString aString;
    int line;

    if (lcdHeight > 1)
    {
        aString = "widget_set Volume progressBar 1 ";
        aString += QString::number(lcdHeight);
        aString += " ";
        aString += QString::number((int)rint(volume_level * lcdWidth * cellWidth));
        sendToServer(aString);
    }

    aString = QString::number((int)(volume_level * 100));
    aString += "%";

    if (lcdHeight > 3)
        line = 3;
    else
        line = lcdHeight;
    outputRightText("Volume", aString, "botWidget", line);
}

void LCDProcClient::switchToTime()
{
    if (!lcd_ready)
        return;

    stopAll();

    if (debug_level > 1)
        LOG(VB_GENERAL, LOG_INFO, "LCDProcClient: switchToTime");

    startTime();
}

void LCDProcClient::switchToMusic(const QString &artist, const QString &album, const QString &track)
{
    if (!lcd_ready)
        return;

    stopAll();

    if (debug_level > 1)
        LOG(VB_GENERAL, LOG_INFO, "LCDProcClient: switchToMusic") ;

    startMusic(artist, album, track);
}

void LCDProcClient::switchToChannel(QString channum, QString title, QString subtitle)
{
    if (!lcd_ready)
        return;

    stopAll();

    if (debug_level > 1)
        LOG(VB_GENERAL, LOG_INFO, "LCDProcClient: switchToChannel");

    startChannel(channum, title, subtitle);
}

void LCDProcClient::switchToMenu(QList<LCDMenuItem> *menuItems, QString app_name,
                                 bool popMenu)
{
    if (!lcd_ready)
        return;

    if (debug_level > 1)
        LOG(VB_GENERAL, LOG_INFO, "LCDProcClient: switchToMenu");

    startMenu(menuItems, app_name, popMenu);
}

void LCDProcClient::switchToGeneric(QList<LCDTextItem> *textItems)
{
    if (!lcd_ready)
        return;
    stopAll();

    if (debug_level > 1)
        LOG(VB_GENERAL, LOG_INFO, "LCDProcClient: switchToGeneric");

    startGeneric(textItems);
}

void LCDProcClient::switchToVolume(QString app_name)
{
    if (!lcd_ready)
        return;

    stopAll();

    if (debug_level > 1)
        LOG(VB_GENERAL, LOG_INFO, "LCDProcClient: switchToVolume");

    startVolume(app_name);
}

void LCDProcClient::switchToNothing()
{
    if (!lcd_ready)
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

    socket->close();

    lcd_ready = false;
    connected = false;
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

    if (lcd_bigclock)
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
        LOG(VB_GENERAL, LOG_INFO,
            "LCDProcClient: An LCD device is being snuffed out"
            "of existence (~LCDProcClient() was called)");

    if (socket)
    {
        delete socket;
        lcd_ready = false;
    }

    if (lcdMenuItems)
        delete lcdMenuItems;

    gCoreContext->removeListener(this);
}

void LCDProcClient::customEvent(QEvent *e)
{
    if ((MythEvent::Type)(e->type()) == MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent *) e;

        if (me->Message().startsWith("RECORDING_LIST_CHANGE") ||
            me->Message() == "UPDATE_PROG_INFO")
        {
            if (lcd_showrecstatus && !updateRecInfoTimer->isActive())
            {
               if (debug_level > 1)
                   LOG(VB_GENERAL, LOG_INFO,
                       "LCDProcClient: Received recording list change");

                // we can't query the backend from inside the customEvent
                // so fire the recording list update from a timer
                updateRecInfoTimer->start(500);
            }
        }
    }
}

void LCDProcClient::updateRecordingList(void)
{
    tunerList.clear();
    isRecording = false;

    if (!gCoreContext->IsConnectedToMaster())
    {
        if (!gCoreContext->ConnectToMasterServer(false))
        {
            LOG(VB_GENERAL, LOG_ERR,
                "LCDProcClient: Cannot get recording status "
                "- is the master server running?\n\t\t\t"
                "Will retry in 30 seconds");
            QTimer::singleShot(30 * 1000, this, SLOT(updateRecordingList()));

            // If we can't get the recording status and we're showing
            // it, switch back to time. Maybe it would be even better
            // to show that the backend is unreachable ?
            if (activeScreen == "RecStatus")
                switchToTime();
            return;
        }
    }

    isRecording = RemoteGetRecordingStatus(&tunerList, false);

    lcdTunerNo = 0;

    if (activeScreen == "Time" || activeScreen == "RecStatus")
        startTime();
}
/* vim: set expandtab tabstop=4 shiftwidth=4: */
