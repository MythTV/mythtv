/*
    lcdprocclient.cpp
    
    a MythTV project object to control an
    LCDproc server
    
    (c) 2002, 2003 Thor Sigvaldason, Dan Morphis and Isaac Richards
*/
#include <qapplication.h>
#include <qregexp.h>
#include <unistd.h>
#include <cmath>

#include "lcdprocclient.h"
#include "mythcontext.h"
#include "mythdialogs.h"
#include "mythdbcon.h"
#include "tv.h"
#include "lcdserver.h"
#include "lcddevice.h"


#define LCD_START_COL 3

#define LCD_VERSION_4 1
#define LCD_VERSION_5 2

#define LCD_RECSTATUS_TIME  10000
#define LCD_TIME_TIME       5000
#define LCD_SCROLLLIST_TIME 2000

LCDProcClient::LCDProcClient(LCDServer *lparent) :
    QObject(NULL, "LCDProcClient")
{
    // Constructor for LCDProcClient
    //
    // Note that this does *not* include opening the socket and initiating 
    // communications with the LDCd daemon.

    if (debug_level > 0)
        VERBOSE(VB_GENERAL, "LCDProcClient: An LCDProcClient object now exists");

    socket = new QSocket(this);
    connect(socket, SIGNAL(error(int)), this, SLOT(veryBadThings(int)));
    connect(socket, SIGNAL(readyRead()), this, SLOT(serverSendingData()));

    m_parent = lparent;
     
    lcd_ready = false;
    
    lcdWidth = 5;
    lcdHeight = 1;
    cellWidth = 1;
    cellHeight = 1;    
   
    hostname = "";
    port = 13666;

    timeFlash = false;
    scrollingText = "";
    progress = 0.0;
    generic_progress = 0.0;
    volume_level = 0.0;
    connected = false;
    send_buffer = "";
    lcdMenuItems = new QPtrList<LCDMenuItem>;
    lcdMenuItems->setAutoDelete(true);

    lcdTextItems = new QPtrList<LCDTextItem>;
    lcdTextItems->setAutoDelete(true);

    timeTimer = new QTimer(this);
    connect(timeTimer, SIGNAL(timeout()), this, SLOT(outputTime()));    
   
    scrollWTimer = new QTimer(this);
    connect(scrollWTimer, SIGNAL(timeout()), this, SLOT(scrollWidgets()));
    
    preScrollWTimer = new QTimer(this);
    connect(preScrollWTimer, SIGNAL(timeout()), this, 
            SLOT(beginScrollingWidgets()));
    
    popMenuTimer = new QTimer(this);
    connect(popMenuTimer, SIGNAL(timeout()), this, SLOT(unPopMenu()));

    menuScrollTimer = new QTimer(this);
    connect(menuScrollTimer, SIGNAL(timeout()), this, SLOT(scrollMenuText()));

    menuPreScrollTimer = new QTimer(this);
    connect(menuPreScrollTimer, SIGNAL(timeout()), this, 
            SLOT(beginScrollingMenuText()));

    checkConnectionsTimer = new QTimer(this);
    connect(checkConnectionsTimer, SIGNAL(timeout()), this, 
            SLOT(checkConnections()));
    checkConnectionsTimer->start(10000, false);            
    
    recStatusTimer = new QTimer(this);
    connect(recStatusTimer, SIGNAL(timeout()), this, SLOT(outputRecStatus()));

    scrollListTimer = new QTimer(this);
    connect(scrollListTimer, SIGNAL(timeout()), this, SLOT(scrollList()));

    showMessageTimer = new QTimer(this);
    connect(showMessageTimer, SIGNAL(timeout()), this, 
            SLOT(removeStartupMessage()));

    gContext->addListener(this);
    
    isRecording = false;
}

bool LCDProcClient::SetupLCD () 
{
    QString lcd_host;
    int lcd_port;

    lcd_host = gContext->GetSetting("LCDHost", "localhost");
    lcd_port = gContext->GetNumSetting("LCDPort", 13666);

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
    if (!gContext->GetNumSetting("LCDEnable", 0))
    {
        connected = false;
        return connected;
    }

    if (!connected)
    {
        QTextStream os(socket);
        socket->connectToHost(hostname, port);

        while (--timeout && socket->state() != QSocket::Idle)
        {
            qApp->lock();
            qApp->processEvents();
            qApp->unlock();
            usleep(1000);

            if (socket->state() == QSocket::Connected)
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
    if (socket->state() == QSocket::Idle)
    {
        if (!lcd_ready)
            return;

        lcd_ready = false;

        //Stop everything
        stopAll();

        // Ack, connection to server has been severed try to re-establish the 
        // connection
        VERBOSE(VB_IMPORTANT, "LCDProcClient: Connection to LCDd died unexpectedly.");
        return;
    }

    QTextStream os(socket);
   
    last_command = someText;
 
    if (connected)
    {
        if (debug_level > 9)
            VERBOSE(VB_NETWORK, "LCDProcClient: Sending to Server: " << someText);
        
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

    aString = "screen_set ";
    aString += screen;
    aString += " priority ";

    switch (priority) {
    case TOP: 
      aString += QString::number (prioTop);
      break;
    case URGENT: 
      aString += QString::number (prioUrgent);
      break;
    case HIGH:
      aString += QString::number (prioHigh);
      break;
    case MEDIUM: 
      aString += QString::number (prioMedium);
      break;
    case LOW:
      aString += QString::number (prioLow);
      break;
    case OFF:
      aString += QString::number (prioOff);
      break;

    }

    sendToServer (aString);
}

void LCDProcClient::setHeartbeat (const QString &screen, bool onoff) {
    QString msg;
    if (onoff) {
        if (pVersion == LCD_VERSION_4) {
            msg = "widget_add " + screen + " heartbeat";
	}
	if (pVersion == LCD_VERSION_5) {
            msg = "screen_set " + screen + " heartbeat on";
	}
    } else {
        if (pVersion == LCD_VERSION_4) {
	    msg = "widget_del " + screen + " heartbeat";
	}
	if (pVersion == LCD_VERSION_5) {
            msg = "screen_set " + screen + " heartbeat off";
	}
    }
    sendToServer (msg);
}

void LCDProcClient::checkConnections()
{
    if (debug_level > 0)
        VERBOSE(VB_GENERAL, "LCDProcClient: checking connections");

    // check connection to mythbackend
    if (!gContext->IsConnectedToMaster())
    {
        if (debug_level > 0)
           VERBOSE(VB_GENERAL, "LCDProcClient: connecting to master server");
        gContext->ConnectToMasterServer(false);
    }

    //check connection to LCDProc server
    if (socket->state() == QSocket::Idle)
    {
        if (debug_level > 0)
           VERBOSE(VB_GENERAL, "LCDProcClient: connecting to LCDProc server");

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
                VERBOSE(VB_NETWORK, "LCDProcClient: Received from server: " 
                    << lineFromServer);

        aList = QStringList::split(" ", lineFromServer);
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
                VERBOSE(VB_IMPORTANT, "LCDProcClient: WARNING: Second parameter "
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
            VERBOSE(VB_IMPORTANT, "LCDProcClient: WARNING: Something is getting" 
                                  "passed to LCDd that it doesn't understand");
            VERBOSE(VB_IMPORTANT, "last command: " << last_command);
        }
        else if (aList.first() == "key")
        {   
           if (m_parent)
               m_parent->sendKeyPress(aList.last().stripWhiteSpace());
        }
    }
}

void LCDProcClient::init()
{
    QString aString;
    lcd_keystring = "";

    connected = TRUE;

    // This gets called when we receive the "connect" string from the server
    // indicating that "hello" was succesful
    sendToServer("client_set name Myth");
 
    // Create all the screens and widgets (when we change activity in the myth 
    // program, we just swap the priorities of the screens to show only the 
    // "current one")
    sendToServer("screen_add Time");
    setPriority("Time", MEDIUM);
    sendToServer("widget_add Time timeWidget string");
    sendToServer("widget_add Time topWidget string");

    // The Menu Screen    
    // I'm not sure how to do multi-line widgets with lcdproc so we're going 
    // the ghetto way
    sendToServer("screen_add Menu");
    setPriority("Menu", LOW);
    sendToServer("widget_add Menu topWidget string");
    for (unsigned int i = 1; i < lcdHeight; i++) 
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
    sendToServer("widget_add RecStatus progressBar hbar");
    
    lcd_ready = true;
 
    loadSettings();
    
    // default to showing time
    switchToTime();
    
    updateRecordingList();
    
    // do we need to show the startup message
    if (startup_message != "")
        showStartupMessage();
 
    // send buffer if there's anything in there
    if (send_buffer.length() > 0)
    {
        sendToServer(send_buffer);
        send_buffer = "";
    }
}

void LCDProcClient::loadSettings()
{
    if (!lcd_ready)
        return;
            
    QString aString;
    QString old_keystring = lcd_keystring;
     
    timeformat = gContext->GetSetting("TimeFormat", "h:mm AP");
   
    // Get LCD settings
    lcd_showmusic=(gContext->GetSetting("LCDShowMusic", "1")=="1");
    lcd_showmusic_items=(gContext->GetSetting("LCDShowMusicItems", "ArtistAlbumTitle")); 
    lcd_showtime=(gContext->GetSetting("LCDShowTime", "1")=="1");
    lcd_showchannel=(gContext->GetSetting("LCDShowChannel", "1")=="1");
    lcd_showgeneric=(gContext->GetSetting("LCDShowGeneric", "1")=="1");
    lcd_showvolume=(gContext->GetSetting("LCDShowVolume", "1")=="1");
    lcd_showmenu=(gContext->GetSetting("LCDShowMenu", "1")=="1");
    lcd_showrecstatus=(gContext->GetSetting("LCDShowRecStatus", "1")=="1");
    lcd_backlighton=(gContext->GetSetting("LCDBacklightOn", "1")=="1");
    lcd_heartbeaton=(gContext->GetSetting("LCDHeartBeatOn", "1")=="1");
    aString = gContext->GetSetting("LCDPopupTime", "5");
    lcd_popuptime = aString.toInt() * 1000;
    lcd_keystring = gContext->GetSetting("LCDKeyString", "ABCDEF");

    if (old_keystring != "")
    {
        aString = "client_del_key " + lcd_keystring;
        sendToServer(aString);
    }
    
    aString = "client_add_key " + lcd_keystring;
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
    QPtrList<LCDTextItem> textItems;
    textItems.setAutoDelete(true);

    QStringList list = formatScrollerText(startup_message);
    
    int startrow = 1;
    if (list.count() < lcdHeight)
        startrow = ((lcdHeight - list.count()) / 2) + 1;
    
    for (uint x = 0; x < list.count(); x++)
    {
        if (x == lcdHeight)
            break; 
        textItems.append(new LCDTextItem(x + startrow, ALIGN_LEFT, list[x], 
                    "Generic", false));
    }
    
    switchToGeneric(&textItems);
    
    showMessageTimer->start(startup_showtime * 1000, true);
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
        prioTop = 252;
        prioUrgent = 248;
        prioHigh = 240;
        prioMedium = 128;
        prioLow = 64;
        prioOff = 0;
    }
    else
    {
        pVersion = LCD_VERSION_4;
        prioTop = 64;
        prioUrgent = 128;
        prioHigh = 240;
        prioMedium = 248;
        prioLow = 252;
        prioOff = 255;
    }
}

void LCDProcClient::describeServer()
{
    if (debug_level > 0)
    {
        VERBOSE(VB_GENERAL, 
            QString("LCDProcClient: The server is %1x%2 with each cell being %3x%4." )
            .arg(lcdWidth).arg(lcdHeight).arg(cellWidth).arg(cellHeight));
        VERBOSE(VB_GENERAL, 
            QString("LCDProcClient: LCDd version %1, protocol version %2.")
            .arg(serverVersion).arg(protocolVersion));
    }

    if (debug_level > 1)
    {
        VERBOSE(VB_GENERAL, QString("LCDProcClient: MythTV LCD settings:"));
        VERBOSE(VB_GENERAL, QString("LCDProcClient: - showmusic      : %1")
            .arg(lcd_showmusic));
        VERBOSE(VB_GENERAL, QString("LCDProcClient: - showmusicitems : %1")
            .arg(lcd_showmusic_items));
        VERBOSE(VB_GENERAL, QString("LCDProcClient: - showtime       : %1")
            .arg(lcd_showtime));
        VERBOSE(VB_GENERAL, QString("LCDProcClient: - showchannel    : %1")
            .arg(lcd_showchannel));
        VERBOSE(VB_GENERAL, QString("LCDProcClient: - showrecstatus  : %1")
            .arg(lcd_showrecstatus));
        VERBOSE(VB_GENERAL, QString("LCDProcClient: - showgeneric    : %1")
            .arg(lcd_showgeneric));
        VERBOSE(VB_GENERAL, QString("LCDProcClient: - showvolume     : %1")
            .arg(lcd_showvolume));
        VERBOSE(VB_GENERAL, QString("LCDProcClient: - showmenu       : %1")
            .arg(lcd_showmenu));
        VERBOSE(VB_GENERAL, QString("LCDProcClient: - backlighton    : %1")
            .arg(lcd_backlighton));
        VERBOSE(VB_GENERAL, QString("LCDProcClient: - heartbeaton    : %1")
            .arg(lcd_heartbeaton));
        VERBOSE(VB_GENERAL, QString("LCDProcClient: - popuptime      : %1")
            .arg(lcd_popuptime));
    }
}

void LCDProcClient::veryBadThings(int anError)
{
    // Deal with failures to connect and inabilities to communicate

    QString err;

    if (anError == QSocket::ErrConnectionRefused)
        err = "connection refused.";
    else if (anError == QSocket::ErrHostNotFound)
        err = "host not found.";
    else if (anError == QSocket::ErrSocketRead)
        err = "socket read failed.";
    else
        err = "unknown error.";

    VERBOSE(VB_IMPORTANT, QString("Could not connect to LCDd: %1").arg(err));
    socket->clearPendingData();
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
    if (scrollListItem >= scrollListItems.count())
        scrollListItem = 0;
}

void LCDProcClient::stopAll()
{
    // The usual reason things would get this far and then lcd_ready being 
    // false is the connection died and we're trying to re-establish the 
    // connection
    if (debug_level > 1)
        VERBOSE(VB_GENERAL, "LCDProcClient: stopAll");
    
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

    timeTimer->start(1000, FALSE);
    outputTime();
    activeScreen = "Time";
    isTimeVisible = true;
    
    if (lcd_showrecstatus && isRecording)
        recStatusTimer->start(LCD_TIME_TIME, FALSE);
}

void LCDProcClient::outputText(QPtrList<LCDTextItem> *textItems)
{
    if (!lcd_ready)
        return;

    QPtrListIterator<LCDTextItem> it( *textItems );
    LCDTextItem *curItem;
    QString num;
    unsigned int counter = 1;

    // Do the definable scrolling in here.
    // Use asignScrollingWidgets(curItem->getText(), "textWidget" + num);
    // When scrolling is set, alignment has no effect
    while ((curItem = it.current()) != 0 && counter < lcdHeight)
    {
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
    scrollListTimer->start(LCD_SCROLLLIST_TIME, FALSE);
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
    lcdTextItems->append(new LCDTextItem(theRow, ALIGN_LEFT, theText,
                                          theScreen, true, theWidget));
}

void LCDProcClient::formatScrollingWidgets()
{

    scrollWTimer->stop();
    preScrollWTimer->stop();

    if (lcdTextItems->isEmpty())
        return; // Weird...

    unsigned int max_len = 0;
    QPtrListIterator<LCDTextItem> it(*lcdTextItems);
    LCDTextItem *curItem;

    // Get the length of the longest item to scroll
    for(; (curItem = it.current()) != 0; ++it) {
        if (curItem->getText().length() > max_len)
            max_len = curItem->getText().length();
    }

    // Make all scrollable items the same lenght and do the initial output
    it.toFirst();
    while ((curItem = it.current()) != 0)
    {
        ++it;
        if (curItem->getText().length() > lcdWidth)
        {
            QString temp, temp2;
            temp = temp.fill(QChar(' '), max_len - curItem->getText().length());
            temp2 = temp2.fill(QChar(' '), lcdWidth);
            curItem->setText(temp2 + curItem->getText() + temp);
            outputLeftText(scrollScreen,
                        curItem->getText().mid(lcdWidth, max_len),
                        curItem->getWidget(), curItem->getRow());
        }
        else {
            curItem->setScrollable(false);
            outputCenteredText(scrollScreen, curItem->getText(),
                        curItem->getWidget(), curItem->getRow());
        }
    }
 
    if (max_len <= lcdWidth)
        // We're done, no scrolling
        return;

    preScrollWTimer->start(2000, TRUE);
}

void LCDProcClient::beginScrollingWidgets()
{
    scrollPosition = lcdWidth;
    preScrollWTimer->stop();
    scrollWTimer->start(400, false);
}

void LCDProcClient::scrollWidgets()
{
    if (activeScreen != scrollScreen)
        return;

    if (lcdTextItems->isEmpty())
        return; // Weird...

    QPtrListIterator<LCDTextItem> it(*lcdTextItems);
    LCDTextItem *curItem;

    unsigned int len = 0;
    for(; (curItem = it.current()) != 0; ++it) {
        if (curItem->getScroll()) {
            // Note that all scrollable items have the same lenght!
            len = curItem->getText().length();
 
            outputLeftText(scrollScreen,
                        curItem->getText().mid(scrollPosition, lcdWidth),
                        curItem->getWidget(), curItem->getRow());
        }
    }
    if (len == 0) {
        // Shouldn't happen, but....
        cerr << "LCDProcClient::scrollWidgets called without scrollable items"
             << endl;
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
    if (lcd_showmusic_items == "ArtistAlbumTitle") {
        aString += " [";
        aString += album;
        aString += "] ";
    } else if (lcdHeight < 4) {
        aString += " - ";
    } 

    if (lcdHeight < 4) {
    	aString += track;
    }
    else {
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

    if (lcdHeight == 2)
    {
        aString = channum + "|" + title;
        if (subtitle != "")
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
        
    progress = 0.0;
    outputChannel();
}

void LCDProcClient::startGeneric(QPtrList<LCDTextItem> * textItems)
{
    LCDTextItem *curItem;
    QPtrListIterator<LCDTextItem> it( *textItems );
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
    if ((curItem = it.current()) == 0)
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
    assignScrollingWidgets(curItem->getText(), "Generic", "textWidget1",
                                                        curItem->getRow());

    outputGeneric();

    // Pop off the first item so item one isn't written twice
    if (textItems->removeFirst() != 0)
        outputText(textItems);
    formatScrollingWidgets();
}

void LCDProcClient::startMenu(QPtrList<LCDMenuItem> *menuItems, QString app_name, 
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
    outputCenteredText("Menu", app_name, "topWidget", 1);

    QPtrListIterator<LCDMenuItem> it(*menuItems);
    LCDMenuItem *curItem;

    // First loop through and figure out where the selected item is in the
    // list so we know how many above and below to display
    unsigned int selectedItem = 0;
    unsigned int counter = 0;
    bool oneSelected = false;

    while ((curItem = it.current()) != 0)
    {
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
        popMenuTimer->start(lcd_popuptime, true);

    // QPtrListIterator doesn't contain a deep copy constructor. . .
    // This will contain a copy of the menuItems for scrolling purposes
    QPtrListIterator<LCDMenuItem> itTemp(*menuItems);
    lcdMenuItems->clear();
    counter = 1;
    while ((curItem = itTemp.current()) != 0)
    {
        ++itTemp;
        lcdMenuItems->append(new LCDMenuItem(curItem->isSelected(),
                             curItem->isChecked(), curItem->ItemName(),
                             curItem->getIndent()));
        ++counter;
    }

    // If there is only two lines on the display, then just write the selected
    //  item and leave
    if (lcdHeight == 2)
    {
        it.toFirst();
        while ((curItem = it.current()) != 0)
        {
            ++it;
            if (curItem->isSelected())
            {
                // Set the scroll flag if necessary, otherwise set it to false
                if (curItem->ItemName().length()  > (lcdWidth - LCD_START_COL))
                {
                    menuPreScrollTimer->start(2000, true);
                    curItem->setScroll(true);
                }
                else
                {
                    menuPreScrollTimer->stop();
                    curItem->setScroll(false);
                }

                aString  = "widget_set Menu menuWidget1 1 2 \">";

                switch (curItem->isChecked())
                {
                    case CHECKED: aString += "X "; break;
                    case UNCHECKED: aString += "O "; break;
                    case NOTCHECKABLE: aString += "  "; break;
                    default: break;
                }

                aString += curItem->ItemName().left(lcdWidth - LCD_START_COL) +
                           "\"";
                sendToServer(aString);
                return;
            }
        }

        return;
    }

    // Reset things
    counter = 1;
    curItem = it.toFirst();

    // Move the iterator to selectedItem lcdHeight/2, if > 1, -1.
    unsigned int midPoint = (lcdHeight/2) - 1;
    if (selectedItem > midPoint && it.count() >= lcdHeight-1)
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
    if (counter + midPoint > it.count() - midPoint && counter > midPoint)
    {
        it -= (counter+(lcdHeight/2)- 1) - (it.count () - midPoint);
    }

    counter = 1;
    while ((curItem = it.current()) != 0)
    {
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

        aString += curItem->ItemName().left(lcdWidth - LCD_START_COL) + "\"";
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

    menuPreScrollTimer->start(2000, true);
}

void LCDProcClient::beginScrollingMenuText()
{
    // If there are items to scroll, wait 2 seconds for the user to read whats 
    // already there

    if (!lcdMenuItems)
        return;

    menuScrollPosition = 1;

    QPtrListIterator<LCDMenuItem> it( *lcdMenuItems );
    LCDMenuItem *curItem;

    QString temp;
    // Loop through and prepend everything with enough spaces
    // for smooth scrolling, and update the position
    while ((curItem = it.current()) != 0)
    {
        ++it;
        // Don't setup for smooth scrolling if the item isn't long enough
        // (It causes problems with items being scrolled when they shouldn't)
        if (curItem->ItemName().length()  > (lcdWidth - LCD_START_COL))
        {
            temp = temp.fill(QChar(' '), lcdWidth - curItem->getIndent() - 
                             LCD_START_COL);
            curItem->setItemName(temp + curItem->ItemName());
            curItem->setScrollPos(curItem->getIndent() + temp.length());
            curItem->setScroll(true);
        }
        else
            curItem->setScroll(false);
    }

    // Can get segfaults if we try to start a timer thats already running. . .
    menuScrollTimer->stop();
    menuScrollTimer->start(250, FALSE);
}

void LCDProcClient::scrollMenuText()
{
    if (!lcdMenuItems)
        return;

    QString aString, bString;
    QPtrListIterator<LCDMenuItem> it(*lcdMenuItems);
    LCDMenuItem *curItem;

    ++menuScrollPosition;

    // First loop through and figure out where the selected item is in the
    // list so we know how many above and below to display
    unsigned int selectedItem = 0;
    unsigned int counter = 0;
    bool oneSelected = false;

    while ((curItem = it.current()) != 0)
    {
        ++it;
        if (curItem->isSelected() && !oneSelected)
        {
            selectedItem = counter + 1;
            oneSelected  = true;
            break;
        }
        ++counter;
    }

    // If there is only two lines on the display, then just write the selected
    // item and leave
    curItem = it.toFirst();
    if (lcdHeight == 2)
    {
        it.toFirst();
        while ((curItem = it.current()) != 0)
        {
            ++it;
            if (curItem->isSelected())
            {
                curItem->incrementScrollPos();
                if (curItem->getScrollPos() > curItem->ItemName().length())
                {
                    // Scroll slower second and subsequent times through
                    menuScrollTimer->stop();
                    menuScrollTimer->start(500, FALSE);
                    curItem->setScrollPos(curItem->getIndent());
                }

                // Stop the timer if this item really doesn't need to scroll.
                // This should never have to get invoked because in theory
                // the code in startMenu has done its job. . .
                if (curItem->ItemName().length()  < (lcdWidth - LCD_START_COL))
                    menuScrollTimer->stop();

                aString  = "widget_set Menu menuWidget1 1 2 \">";

                switch(curItem->isChecked())
                {
                    case CHECKED: aString += "X "; break;
                    case UNCHECKED: aString += "O "; break;
                    case NOTCHECKABLE: aString += "  "; break;
                    default: break;
                }

                // Indent this item if nessicary
                aString += bString.fill(' ', curItem->getIndent());

                aString += curItem->ItemName().mid(curItem->getScrollPos(), 
                                                   (lcdWidth - LCD_START_COL));
                aString += "\"";
                sendToServer(aString);
                return;
            }
        }

        return;
    }

    // Find the longest line, if menuScrollPosition is longer then this, then 
    // reset them all
    curItem = it.toFirst();
    unsigned int longest_line = 0;
    unsigned int max_scroll_pos = 0;

    while ((curItem = it.current()) != 0)
    {
        ++it;
        if (curItem->ItemName().length() > longest_line)
            longest_line = curItem->ItemName().length();

        if (curItem->getScrollPos() > max_scroll_pos)
            max_scroll_pos = curItem->getScrollPos();
    }

    // If max_scroll_pos > longest_line then reset
    if (max_scroll_pos > longest_line)
    {
        // Scroll slower second and subsequent times through
        menuScrollTimer->stop();
        menuScrollTimer->start(500, FALSE);
        menuScrollPosition = 0;

        curItem = it.toFirst();
        while ((curItem = it.current()) != 0)
        {
            ++it;
            curItem->setScrollPos(curItem->getIndent());
        }
    }

    // Reset things
    counter = 1;
    curItem = it.toFirst();

    // Move the iterator to selectedItem -1
    if (selectedItem != 1 && it.count() >= lcdHeight)
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
    if (counter == it.count())
        --it;

    bool stopTimer = true;

    counter = 1;
    while ((curItem = it.current()) != 0 && counter <= lcdHeight)
    {
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

            if (curItem->getScrollPos() <= longest_line)
                aString += curItem->ItemName().mid(curItem->getScrollPos(), 
                                                   (lcdWidth-LCD_START_COL));

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
    outputCenteredText("Volume", "Myth " + app_name + " Volume");
    volume_level = 0.0;

    outputVolume();
}

void LCDProcClient::unPopMenu()
{
    // Stop the scrolling timer
    menuScrollTimer->stop();
    setPriority("Menu", OFF);
}

void LCDProcClient::setLevels(int numbLevels, float *values)
{
    if (!lcd_ready)
        return;

    // Set the EQ levels

    for(int i = 0; i < 10; i++)
    {
        if (i >= numbLevels)
            EQlevels[i] = 0.0;
        else
        {
            EQlevels[i] = values[i];
            if (EQlevels[i] < 0.0)
                EQlevels[i] = 0.0;
            else if (EQlevels[i] > 1.0)
                EQlevels[i] = 1.0;
        }
    } 
}

void LCDProcClient::setChannelProgress(float value)
{
    if (!lcd_ready)
        return;

    progress = value;

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

void LCDProcClient::outputTime()
{
    if (isRecording)
        outputCenteredText("Time", tr("RECORDING"), "topWidget", 1);
    else
        outputCenteredText("Time", "", "topWidget", 1);

    QString aString;
    int x, y;
    
    if (lcdHeight < 3)
        y = 2;
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
    
    if (isTimeVisible)
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
    else
    {
        // switch to the time screen
        setPriority("Time", MEDIUM);
        setPriority("RecStatus", LOW);

        timeTimer->start(1000, FALSE);
        scrollWTimer->stop();
        scrollListTimer->stop();
        recStatusTimer->start(LCD_TIME_TIME, FALSE);

        outputTime();
        activeScreen = "Time";
        isTimeVisible = true;

        return;
    }
    
    QString aString, status;
    QStringList list;
    
    TunerStatus *tuner = tunerList.at(lcdTunerNo);

    scrollListItems.clear();
    if (lcdHeight >= 4)
    {
        outputCenteredText("RecStatus", tr("RECORDING"), "textWidget1", 1);
        
        status = tuner->title;
        if (tuner->subTitle != "") 
            status += " (" + tuner->subTitle + ")";
    
        list = formatScrollerText(status);
        assignScrollingList(list, "RecStatus", "textWidget2", 2);

        status = tuner->startTime.toString("hh:mm") + " to " + 
                tuner->endTime.toString("hh:mm");                        
        outputCenteredText("RecStatus", status, "textWidget3", 3);
        
        int length = tuner->startTime.secsTo(tuner->endTime);
        int delta = tuner->startTime.secsTo(QDateTime::currentDateTime());
        double rec_progress = (double) delta / length;
        
        aString = "widget_set RecStatus progressBar 1 ";
        aString += QString::number(lcdHeight);
        aString += " ";
        aString += QString::number((int)rint(rec_progress * lcdWidth * 
                                cellWidth));
        sendToServer(aString);
        
        listTime = list.count() * LCD_SCROLLLIST_TIME * 2;
    }
    else
    {
        status = tr("RECORDING|");
        status += tuner->title;
        if (tuner->subTitle != "") 
            status += "|(" + tuner->subTitle + ")";

        status += "|" + tuner->startTime.toString("hh:mm") + " to " + 
                tuner->endTime.toString("hh:mm");                        
        
        list = formatScrollerText(status);
        assignScrollingList(list, "RecStatus", "textWidget1", 1);

        int length = tuner->startTime.secsTo(tuner->endTime);
        int delta = tuner->startTime.secsTo(QDateTime::currentDateTime());
        double rec_progress = (double) delta / length;
        
        aString = "widget_set RecStatus progressBar 1 ";
        aString += QString::number(lcdHeight);
        aString += " ";
        aString += QString::number((int)rint(rec_progress * lcdWidth * 
                                cellWidth));
        sendToServer(aString);
        
        listTime = list.count() * LCD_SCROLLLIST_TIME * 2;
    }
    
    if (listTime < LCD_TIME_TIME)
        listTime = LCD_TIME_TIME;
        
    recStatusTimer->start(listTime, FALSE);
    
    if (lcdTunerNo < (int) tunerList.count() - 1)
        lcdTunerNo++;
    else
        lcdTunerNo = 0;
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
    
    for (uint x = 0; x < text.length(); x++)
    {
        if (separators.contains(text[x]) > 0)    
            lastSplit = line.length();
        
        line += text[x];
        if (line.length() > lcdWidth || text[x] == '|')
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

        if (music_repeat == 1)
        {
            repeat = "R:1 ";
        }
        else if (music_repeat == 2)
        {
            repeat = "R:* ";
        }

        if (shuffle.length() != 0 || repeat.length() != 0) {
            aString.sprintf("%s%s", shuffle.ascii(), repeat.ascii());

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
    QString aString;
    aString = "widget_set Channel progressBar 1 ";
    aString += QString::number(lcdHeight);
    aString += " ";
    aString += QString::number((int)rint(progress * lcdWidth * cellWidth));
    sendToServer(aString);
}

void LCDProcClient::outputGeneric()
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

void LCDProcClient::outputVolume()
{
    QString aString;
    aString = "widget_set Volume progressBar 1 ";
    aString += QString::number(lcdHeight);
    aString += " ";
    aString += QString::number((int)rint(volume_level * lcdWidth * cellWidth));
    sendToServer(aString);

    aString = QString::number((int)(volume_level * 100));
    aString += "%";
    outputRightText("Volume", aString, "botWidget", 3);
}

void LCDProcClient::switchToTime()
{
    if (!lcd_ready)
        return;
    
    stopAll();
    
    if (debug_level > 1)
        VERBOSE(VB_GENERAL, "LCDProcClient: switchToTime");

    startTime();
}

void LCDProcClient::switchToMusic(const QString &artist, const QString &album, const QString &track)
{
    if (!lcd_ready)
        return;
    
    stopAll();
    
    if (debug_level > 1)
        VERBOSE(VB_GENERAL, "LCDProcClient: switchToMusic") ;
    
    startMusic(artist, album, track);
}

void LCDProcClient::switchToChannel(QString channum, QString title, QString subtitle)
{
    if (!lcd_ready)
        return;

    stopAll();
    
    if (debug_level > 1)
        VERBOSE(VB_GENERAL, "LCDProcClient: switchToChannel");
    
    startChannel(channum, title, subtitle);
}

void LCDProcClient::switchToMenu(QPtrList<LCDMenuItem> *menuItems, QString app_name, 
                       bool popMenu)
{
    if (!lcd_ready)
        return;
    
    //stopAll();
    if (debug_level > 1)
        VERBOSE(VB_GENERAL, "LCDProcClient: switchToMenu");
    
    startMenu(menuItems, app_name, popMenu);
}

void LCDProcClient::switchToGeneric(QPtrList<LCDTextItem> *textItems)
{
    if (!lcd_ready)
        return;
    stopAll();

    if (debug_level > 1)
        VERBOSE(VB_GENERAL, "LCDProcClient: switchToGeneric");

    startGeneric(textItems);
}

void LCDProcClient::switchToVolume(QString app_name)
{
    if (!lcd_ready)
        return;

    stopAll();
    
    if (debug_level > 1)
        VERBOSE(VB_GENERAL, "LCDProcClient: switchToVolume");

    startVolume(app_name);
}

void LCDProcClient::switchToNothing()
{
    if (!lcd_ready)
        return;
    
    stopAll();
    
    if (debug_level > 1)
        VERBOSE(VB_GENERAL, "LCDProcClient: switchToNothing");
}

void LCDProcClient::shutdown()
{
    if (debug_level > 1)
        VERBOSE(VB_GENERAL, "LCDProcClient: shutdown");

    stopAll();

    //  Remove all the widgets and screens for a clean exit from the server
    
    sendToServer("widget_del Channel progressBar");
    sendToServer("widget_del Channel topWidget");
    sendToServer("screen_del Channel");

    sendToServer("widget_del Generic progressBar");
    sendToServer("widget_del Generic textWidget1");
    sendToServer("widget_del Generic textWidget2");
    sendToServer("widget_del Generic textWidget3");
    sendToServer("screen_del Generic");

    sendToServer("widget_del Volume progressBar");
    sendToServer("widget_del Volume topWidget");
    sendToServer("screen_del Volume");

    sendToServer("screen_del Menu topWidget");
    sendToServer("screen_del Menu menuWidget1");
    sendToServer("screen_del Menu menuWidget2");
    sendToServer("screen_del Menu menuWidget3");
    sendToServer("screen_del Menu menuWidget4");
    sendToServer("screen_del Menu menuWidget5");

    sendToServer("widget_del Music progressBar");
    sendToServer("widget_del Music infoWidget");
    sendToServer("widget_del Music timeWidget");
    sendToServer("widget_del Music topWidget");
    sendToServer("screen_del Music");
    
    sendToServer("widget_del Time timeWidget");
    sendToServer("widget_del Time topWidget");
    sendToServer("screen_del Time");
    
    sendToServer("widget_del RecStatus textWidget1");
    sendToServer("widget_del RecStatus textWidget2");
    sendToServer("widget_del RecStatus textWidget3");
    sendToServer("widget_del RecStatus progressBar");

    socket->close();

    lcd_ready = false;
    connected = false;
}

LCDProcClient::~LCDProcClient()
{
    if (debug_level > 1)
        VERBOSE(VB_GENERAL, "LCDProcClient: An LCD device is being snuffed out"
                            "of existence (~LCDProcClient() was called)");

    if (socket)
    {
        delete socket;
        lcd_ready = false;
    }
    
    if (lcdMenuItems)
        delete lcdMenuItems;
        
    gContext->removeListener(this);     
}

void LCDProcClient::customEvent(QCustomEvent *e)
{
    if ((MythEvent::Type)(e->type()) == MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent *) e;
       
        if (me->Message().left(21) == "RECORDING_LIST_CHANGE")
        {
            if (lcd_showrecstatus)
            {    
                // we can't query the backend from inside the customEvent
                // so fire the recording list update from a timer  
                QTimer::singleShot(500, this, SLOT(updateRecordingList()));
            }    
        }
    }    
}

void LCDProcClient::updateRecordingList(void)
{
    tunerList.clear();
    isRecording = false;

    if (!gContext->IsConnectedToMaster())
    {
        if (!gContext->ConnectToMasterServer(false))
        {
            VERBOSE(VB_IMPORTANT, "LCDProcClient: Cannot get recording status "
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
    
    QStringList strlist;
    
    // are we currently recording
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT cardid FROM capturecard "
        "WHERE parentid='0' ORDER BY cardid");
    QString Status = "";

    if (query.exec() && query.isActive() && query.numRowsAffected())
    {
        while(query.next())
        {
            QString status = "";
            int cardid = query.value(0).toInt();
            int state = kState_ChangingState;
            QString channelName = "";
            QString title = "";
            QString subtitle = "";
            QDateTime dtStart = QDateTime();
            QDateTime dtEnd = QDateTime();

            QString cmd = QString("QUERY_REMOTEENCODER %1").arg(cardid);

            while (state == kState_ChangingState)
            {
                strlist = cmd;
                strlist << "GET_STATE";
                gContext->SendReceiveStringList(strlist);

                state = strlist[0].toInt();
                if (state == kState_ChangingState)
                    usleep(500);
            }
              
            if (state == kState_RecordingOnly || state == kState_WatchingRecording)
            {
                isRecording = true;
                
                strlist = QString("QUERY_RECORDER %1").arg(cardid);
                strlist << "GET_RECORDING";
                gContext->SendReceiveStringList(strlist);
                title = strlist[0];
                subtitle = strlist[1];
                channelName = strlist[7];
                dtStart.setTime_t((uint)atoi(strlist[11].ascii()));
                dtEnd.setTime_t((uint)atoi(strlist[12].ascii())); 
            }
            else
                continue;
        
            TunerStatus *tuner = new TunerStatus;
            tuner->id = cardid;
            tuner->isRecording = (state == kState_RecordingOnly || 
                                  state == kState_WatchingRecording);
            tuner->channel = channelName;
            tuner->title = title;
            tuner->subTitle = subtitle;
            tuner->startTime = dtStart;
            tuner->endTime = dtEnd;
            tunerList.append(tuner); 
        }        
    }
    
    lcdTunerNo = 0;
    
    if  (activeScreen == "Time" || activeScreen == "RecStatus")
        startTime();          
}
/* vim: set expandtab tabstop=4 shiftwidth=4: */
