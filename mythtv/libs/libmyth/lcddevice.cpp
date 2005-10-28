/*
    lcddevice.cpp
    
    a MythTV project object to control an
    LCDproc server
    
    (c) 2002, 2003 Thor Sigvaldason, Dan Morphis and Isaac Richards
*/

#include "lcddevice.h"
#include "mythcontext.h"
#include "mythdialogs.h"

#include <unistd.h>
#include <sys/wait.h>	// For WIFEXITED on Mac OS X
#include <cmath>

#include <qapplication.h>
#include <qregexp.h>


/*
  LCD_DEVICE_DEBUG control how much debug info we get
  0 = none
  1 = LCDServer info 
  2 = screen switch info
  5 = every command received
  10 = every command sent and error received
 */

#define LCD_DEVICE_DEBUG 0

LCD::LCD()
   :QObject(NULL, "LCD")
{
    // Constructor for LCD
    //
    // Note that this does *not* include opening the socket and initiating 
    // communications with the LDCd daemon.

#if LCD_DEVICE_DEBUG > 0
    VERBOSE(VB_ALL, "lcddevice: An LCD object now exists (LCD() was called)");
#endif

    GetLEDMask = NULL;

    socket = new QSocket(this);
    connect(socket, SIGNAL(error(int)), this, SLOT(veryBadThings(int)));
    connect(socket, SIGNAL(readyRead()), this, SLOT(serverSendingData()));

    lcd_ready = false;
    
    hostname = "localhost";
    port = 6545;
    
    connected = false;
    send_buffer = "";
    
    retryTimer = new QTimer(this);
    connect(retryTimer, SIGNAL(timeout()), this, SLOT(restartConnection()));    

    LEDTimer = new QTimer(this);
    connect(LEDTimer, SIGNAL(timeout()), this, SLOT(outputLEDs()));
}

bool LCD::m_server_unavailable = false;
class LCD * LCD::m_lcd = NULL;

class LCD * LCD::Get(void)
{
    if (m_lcd == NULL && m_server_unavailable == false)
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
        m_server_unavailable = false;
    }

    lcd_host = gContext->GetSetting("LCDServerHost", "localhost");
    lcd_port = gContext->GetNumSetting("LCDServerPort", 6545);

    if (lcd_host.length() > 0 && lcd_port > 1024)
    {
        class LCD * lcd = LCD::Get();
        if (lcd->connectToHost(lcd_host, lcd_port) == false) 
        {
            delete m_lcd;
            m_lcd = NULL;
            m_server_unavailable = false;
        }
    }
}

bool LCD::connectToHost(const QString &lhostname, unsigned int lport)
{
#if LCD_DEVICE_DEBUG > 0    
    VERBOSE(VB_ALL, "lcddevice: connecting to host: " 
            << lhostname << " - port: " << lport);
#endif
            
    // Open communications
    // Store the hostname and port in case we need to reconnect.

    int timeout = 1000;
    hostname = lhostname;
    port = lport;
    
    // Don't even try to connect if we're currently disabled.
    if (!gContext->GetNumSetting("LCDEnable", 0))
    {
        connected = false;
        m_server_unavailable = true;
        return connected;
    }

    // check if the 'mythlcdserver' is running
    int res = system("ret=`ps cax | grep -c mythlcdserver`; exit $ret");
    if (WIFEXITED(res))
        res = WEXITSTATUS(res);
    
    if (res == 0)
    {
        // we need to start the mythlcdserver 
        system(gContext->GetInstallPrefix() + "/bin/mythlcdserver&");
        // we need to give the server time to startup
        sleep(1);
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
                lcd_ready = true;
                connected = true;
                os << "HELLO\n";
                break;
            }
        }
    }

    if (connected == false)
        m_server_unavailable = true;

    return connected;
}

void LCD::sendToServer(const QString &someText)
{
    // Check the socket, make sure the connection is still up
    if (socket->state() == QSocket::Idle)
    {
        if (!lcd_ready)
            return;

        lcd_ready = false;

        // Ack, connection to server has been severed try to re-establish the 
        // connection
        retryTimer->start(10000, false);
        VERBOSE(VB_ALL, "lcddevice: Connection to LCDServer died unexpectedly.\n\t\t\t"
                         "Trying to reconnect every 10 seconds. . .");
        return;
    }

    QTextStream os(socket);
   
    last_command = someText;
 
    if (connected)
    {
#if LCD_DEVICE_DEBUG > 9
        VERBOSE(VB_ALL, "lcddevice: Sending to Server: " << someText);
#endif
        // Just stream the text out the socket

        os << someText << "\n" << flush;
    }
    else
    {
        // Buffer this up in the hope that the connection will open soon

        send_buffer += someText;
        send_buffer += "\n";
    }
}

void LCD::restartConnection()
{
    // Reset the flag
    lcd_ready = false;
    connected = false;
    m_server_unavailable = false;

    // Retry to connect. . .  Maybe the user restarted LCDd?
    connectToHost(hostname, port);
}

void LCD::serverSendingData()
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
        lineFromServer.simplifyWhiteSpace();

#if LCD_DEVICE_DEBUG > 4
        // Make debugging be less noisy
        if (lineFromServer != "OK\n")
            VERBOSE(VB_ALL, "lcddevice: Received from server: " << lineFromServer);
#endif

        aList = QStringList::split(" ", lineFromServer);
        if (aList[0] == "CONNECTED")
        {
            // We got "CONNECTED", which is a response to "HELLO"
            lcd_ready = true;
            
            // get lcd width & height
            if (aList.count() != 3)
            {
                VERBOSE(VB_ALL, "lcddevice: received bad no. of arguments "
                                "in CONNECTED response from LCDServer");
            }
            
            bool bOK;
            lcd_width = aList[1].toInt(&bOK);
            if (!bOK)
            {
                VERBOSE(VB_ALL, "lcddevice: received bad int for width"
                                "in CONNECTED response from LCDServer");
            }
             
            lcd_height = aList[2].toInt(&bOK);
            if (!bOK)
            {
                VERBOSE(VB_ALL, "lcddevice: received bad int for height"
                                "in CONNECTED response from LCDServer");
            }
            
            init();
        }
        else if (aList[0] == "HUH?")
        {
            VERBOSE(VB_ALL, "lcddevice: WARNING: Something is getting passed"
                            "to LCDServer that it doesn't understand");
            VERBOSE(VB_ALL, "lcddevice: last command: " << last_command);
        }
        else if (aList[0] == "KEY")
           handleKeyPress(aList.last().stripWhiteSpace());
    }
}

void LCD::handleKeyPress(QString key_pressed)
{
    int key = 0;

    QChar mykey = key_pressed.at(0);
    if (mykey == lcd_keystring.at(0))
        key = Qt::Key_Up;
    else if (mykey == lcd_keystring.at(1))
        key = Qt::Key_Down;
    else if (mykey == lcd_keystring.at(2))
        key = Qt::Key_Left;
    else if (mykey == lcd_keystring.at(3))
        key = Qt::Key_Right;
    else if (mykey == lcd_keystring.at(4))
        key = Qt::Key_Space;
    else if (mykey == lcd_keystring.at(5))
        key = Qt::Key_Escape;

    QApplication::postEvent(gContext->GetMainWindow(),
                            new ExternalKeycodeEvent(key));
}

void LCD::init()
{
    // Stop the timer
    retryTimer->stop();
    
    // Get LCD settings
    lcd_showmusic = (gContext->GetSetting("LCDShowMusic", "1") == "1");
    lcd_showtime = (gContext->GetSetting("LCDShowTime", "1") == "1");
    lcd_showchannel = (gContext->GetSetting("LCDShowChannel", "1") == "1");
    lcd_showgeneric = (gContext->GetSetting("LCDShowGeneric", "1") == "1");
    lcd_showvolume = (gContext->GetSetting("LCDShowVolume", "1") == "1");
    lcd_showmenu = (gContext->GetSetting("LCDShowMenu", "1") == "1");
    lcd_showrecstatus = (gContext->GetSetting("LCDShowRecStatus", "1") == "1");
    lcd_keystring = gContext->GetSetting("LCDKeyString", "ABCDEF");    
    
    connected = TRUE;
    lcd_ready = true;
 
    // send buffer if there's anything in there
    if (send_buffer.length() > 0)
    {
        sendToServer(send_buffer);
        send_buffer = "";
    }
}

void LCD::veryBadThings(int anError)
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

    VERBOSE(VB_IMPORTANT, QString("Could not connect to LCDServer: %1").arg(err));
    socket->clearPendingData();
    socket->close();
}

void LCD::stopAll()
{
    if (!lcd_ready)
        return;

#if LCD_DEVICE_DEBUG > 1
    VERBOSE(VB_ALL, "lcddevice: stopAll");
#endif
       
    sendToServer("STOP_ALL");    
}

void LCD::setChannelProgress(float value)
{
    if (!lcd_ready || !lcd_showchannel)
        return;

    if (value < 0.0)
        value = 0.0;
    else if (value > 1.0)
        value = 1.0;
        
    sendToServer("SET_CHANNEL_PROGRESS " + QString().setNum(value));    
}

void LCD::setGenericProgress(float value)
{
    if (!lcd_ready || !lcd_showgeneric)
        return;

    value = min(max(0.0f, value), 1.0f);
    QString msg = QString("SET_GENERIC_PROGRESS 0 %1").arg(value);
}

void LCD::setGenericBusy()
{
    if (!lcd_ready || !lcd_showgeneric)
        return;

    sendToServer("SET_GENERIC_PROGRESS 1 0.0");
}

void LCD::setMusicProgress(QString time, float value)
{
    if (!lcd_ready || !lcd_showmusic)
        return;

    value = min(max(0.0f, value), 1.0f);
    sendToServer("SET_MUSIC_PROGRESS " + quotedString(time) + " " + 
            QString().setNum(value));    
}

void LCD::setMusicShuffle(int shuffle)
{
    if (!lcd_ready || !lcd_showmusic)
        return;

    sendToServer(QString("SET_MUSIC_PLAYER_PROP SHUFFLE %1").arg(shuffle));
}

void LCD::setMusicRepeat(int repeat)
{
    if (!lcd_ready || !lcd_showmusic)
        return;

    sendToServer(QString("SET_MUSIC_PLAYER_PROP REPEAT %1").arg(repeat));
}

void LCD::setVolumeLevel(float value)
{
    if (!lcd_ready || !lcd_showvolume)
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
    LEDTimer->start(10000, FALSE); 
}

void LCD::outputLEDs()
{
    QString aString;
    int mask = 0;
    if (0 && GetLEDMask)
        mask = GetLEDMask();
    aString = "UPDATE_LEDS ";
    aString += QString::number(mask);
    sendToServer(aString);
}

void LCD::switchToTime()
{
    if (!lcd_ready || !lcd_showtime)
        return;

#if LCD_DEVICE_DEBUG > 1
    VERBOSE(VB_ALL, "lcddevice: switchToTime");
#endif
       
    sendToServer("SWITCH_TO_TIME");    
}

void LCD::switchToMusic(const QString &artist, const QString &album, const QString &track)
{
    if (!lcd_ready || !lcd_showmusic)
        return;

#if LCD_DEVICE_DEBUG > 1
    VERBOSE(VB_ALL, "lcddevice: switchToMusic");
#endif
    
    sendToServer("SWITCH_TO_MUSIC " + quotedString(artist) + " " 
            + quotedString(album) + " " 
            + quotedString(track));
}

void LCD::switchToChannel(QString channum, QString title, QString subtitle)
{
    if (!lcd_ready || !lcd_showchannel)
        return;

#if LCD_DEVICE_DEBUG > 1
    VERBOSE(VB_ALL, "lcddevice: switchToChannel");
#endif
    
    sendToServer("SWITCH_TO_CHANNEL " + quotedString(channum) + " " 
            + quotedString(title) + " " 
            + quotedString(subtitle));
}

void LCD::switchToMenu(QPtrList<LCDMenuItem> *menuItems, QString app_name, 
                       bool popMenu)
{
    if (!lcd_ready || !lcd_showmenu)
        return;

#if LCD_DEVICE_DEBUG > 1
    VERBOSE(VB_ALL, "lcddevice: switchToMenu");
#endif

    if (menuItems->isEmpty())
        return;

    QString s = "SWITCH_TO_MENU ";
    
    s += quotedString(app_name);
    s += " " + QString(popMenu ? "TRUE" : "FALSE");

    
    QPtrListIterator<LCDMenuItem> it(*menuItems);
    LCDMenuItem *curItem;

    while ((curItem = it.current()) != 0)
    {
        ++it;
        s += " " + quotedString(curItem->ItemName());
        
        if (curItem->isChecked() == CHECKED)
            s += " CHECKED";    
        else if (curItem->isChecked() == UNCHECKED) 
            s += " UNCHECKED";
        else if (curItem->isChecked() == NOTCHECKABLE)
            s += " NOTCHECKABLE";

        s += " " + QString(curItem->isSelected() ? "TRUE" : "FALSE");
        s += " " + QString(curItem->Scroll() ? "TRUE" : "FALSE");
        QString sIndent;
        sIndent.setNum(curItem->getIndent());
        s += " " + sIndent;
    }

    sendToServer(s);
}

void LCD::switchToGeneric(QPtrList<LCDTextItem> *textItems)
{
    if (!lcd_ready || !lcd_showgeneric)
        return;

#if LCD_DEVICE_DEBUG > 1
    VERBOSE(VB_ALL, "lcddevice: switchToGeneric ");
#endif
    
    if (textItems->isEmpty())
        return;

    QString s = "SWITCH_TO_GENERIC";
    
    QPtrListIterator<LCDTextItem> it(*textItems);
    LCDTextItem *curItem;

    while ((curItem = it.current()) != 0)
    {
        ++it;
        QString sRow;
        sRow.setNum(curItem->getRow());
        s += " " + sRow;
     
        if (curItem->getAlignment() == ALIGN_LEFT)
            s += " ALIGN_LEFT";    
        else if (curItem->getAlignment() == ALIGN_RIGHT) 
            s += " ALIGN_RIGHT";
        else if (curItem->getAlignment() == ALIGN_CENTERED)
            s += " ALIGN_CENTERED";

        s += " " + quotedString(curItem->getText());
        s += " " + quotedString(curItem->getScreen());
        s += " " + QString(curItem->getScroll() ? "TRUE" : "FALSE");
    }

    sendToServer(s);
}

void LCD::switchToVolume(QString app_name)
{
    if (!lcd_ready || !lcd_showvolume)
        return;

#if LCD_DEVICE_DEBUG > 1
    VERBOSE(VB_ALL, "lcddevice: switchToVolume ");
#endif
    
    sendToServer("SWITCH_TO_VOLUME " + quotedString(app_name));
}

void LCD::switchToNothing()
{
    if (!lcd_ready)
        return;
    
#if LCD_DEVICE_DEBUG > 1
    VERBOSE(VB_ALL, "lcddevice: switchToNothing");
#endif

    sendToServer("SWITCH_TO_NOTHING");
}

void LCD::shutdown()
{
#if LCD_DEVICE_DEBUG > 1
    VERBOSE(VB_ALL, "lcddevice: shutdown");
#endif

    socket->close();

    lcd_ready = false;
    connected = false;
}

void LCD::resetServer()
{
    if (!lcd_ready)
        return;
    
#if LCD_DEVICE_DEBUG > 1
    VERBOSE(VB_ALL, "lcddevice: RESET");
#endif

    sendToServer("RESET");
}

LCD::~LCD()
{
    m_lcd = NULL;

#if LCD_DEVICE_DEBUG > 0
    VERBOSE(VB_ALL, "lcddevice: An LCD device is being snuffed out of "
                    "existence (~LCD() was called)");
#endif
    
    if (socket)
    {
        delete socket;
        lcd_ready = false;
    }
}

// this does nothing
void LCD::setLevels(int numbLevels, float *values)
{
    numbLevels = numbLevels;
    values = values;

#if LCD_DEVICE_DEBUG > 0
    VERBOSE(VB_ALL, "lcddevice: setLevels");
#endif
}

QString LCD::quotedString(const QString &s)
{
    QString sRes = s;
    sRes.replace(QRegExp("\""), QString("\"\""));
    sRes = "\"" + sRes + "\"";
  
    return(sRes);
}
