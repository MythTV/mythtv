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
#include <cmath>

#include <qapplication.h>
#include <qregexp.h>

//#define LCD_DEVICE_DEBUG
#define LCD_START_COL 3

LCD::LCD()
   :QObject(NULL, "LCD")
{
    // Constructor for LCD
    //
    // Note that this does *not* include opening the socket and initiating 
    // communications with the LDCd daemon.

#ifdef LCD_DEVICE_DEBUG
    cout << "lcddevice: An LCD object now exists (LCD() was called)" << endl ;
#endif

    GetLEDMask = NULL;

    socket = new QSocket(this);
    connect(socket, SIGNAL(error(int)), this, SLOT(veryBadThings(int)));
    connect(socket, SIGNAL(readyRead()), this, SLOT(serverSendingData()));

    lcd_ready = false;
    
    lcdWidth = 5;
    lcdHeight = 1;
    cellWidth = 1;
    cellHeight = 1;    
   
    hostname = "";
    port = 13666;

    timeFlash = false;
    scrollingText = "";
    scrollWidget = "";
    scrollRow = 0;
    progress = 0.0;
    generic_progress = 0.0;
    volume_level = 0.0;
    connected = FALSE;
    send_buffer = "";
    lcdMenuItems = new QPtrList<LCDMenuItem>;

    timeTimer = new QTimer(this);
    connect(timeTimer, SIGNAL(timeout()), this, SLOT(outputTime()));    
   
    LEDTimer = new QTimer(this);
    connect(LEDTimer, SIGNAL(timeout()), this, SLOT(outputLEDs()));
    LEDTimer->start(1000, FALSE);      // start the timer here
 
    scrollTimer = new QTimer(this);
    connect(scrollTimer, SIGNAL(timeout()), this, SLOT(scrollText()));
    
    preScrollTimer = new QTimer(this);
    connect(preScrollTimer, SIGNAL(timeout()), this, 
            SLOT(beginScrollingText()));
    
    musicTimer = new QTimer(this);
    connect(musicTimer, SIGNAL(timeout()), this, SLOT(outputMusic()));
    
    channelTimer = new QTimer(this);
    connect(channelTimer, SIGNAL(timeout()), this, SLOT(outputChannel()));

    genericTimer = new QTimer(this);
    connect(genericTimer, SIGNAL(timeout()), this, SLOT(outputGeneric()));

    popMenuTimer = new QTimer(this);
    connect(popMenuTimer, SIGNAL(timeout()), this, SLOT(unPopMenu()));

    menuScrollTimer = new QTimer(this);
    connect(menuScrollTimer, SIGNAL(timeout()), this, SLOT(scrollMenuText()));

    menuPreScrollTimer = new QTimer(this);
    connect(menuPreScrollTimer, SIGNAL(timeout()), this, 
            SLOT(beginScrollingMenuText()));

    retryTimer = new QTimer(this);
    connect(retryTimer, SIGNAL(timeout()), this, SLOT(restartConnection()));
}

bool LCD::m_server_unavailable = false;
class LCD * LCD::m_lcd = NULL;

class LCD * LCD::Get(void)
{
    if (m_lcd == NULL && m_server_unavailable == false)
        m_lcd = new LCD;
    return m_lcd;
}

bool LCD::connectToHost(const QString &lhostname, unsigned int lport)
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
        m_server_unavailable = true;
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

    if (connected == false)
        m_server_unavailable = true;

    return connected;
}

void LCD::beginScrollingText()
{
    unsigned int i;
    QString aString;

    // After the topline text has been on the screen for "a while"
    // start scrolling it.
    
    aString = "";
    for (i = 0; i < lcdWidth; i++)
        scrollingText.prepend(" ");

    scrollPosition = lcdWidth;
    scrollTimer->start(400, false);
}

void LCD::sendToServer(const QString &someText)
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
        retryTimer->start(10000, false);
        cerr << "lcddevice: Connection to LCDd died unexpectedly.  Trying to "
                "reconnect every 10 seconds. . ." << endl;
        return;
    }

    QTextStream os(socket);
   
    last_command = someText;
 
    if (connected)
    {
#ifdef LCD_DEVICE_DEBUG
        cout << "lcddevice: Sending to Server: " << someText << endl ;
#endif
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

#ifdef LCD_DEVICE_DEBUG        
        // Make debugging be less noisy
        if (lineFromServer != "success\n")
            cout << "lcddevice: Received from server: " << lineFromServer ;
#endif

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
                cerr << "lcddevice: WARNING: Second parameter returned from "
                        "LCDd was not \"LCDproc\"" << endl ;
            }
            
            // Skip through some stuff
            
            it++; // server version
            it++; // the string "protocol"
            it++; // protocol version
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

#ifdef LCD_DEVICE_DEBUG
            describeServer();    
#endif
        }

        if (aList.first() == "huh?")
        {
            cerr << "lcddevice: WARNING: Something is getting passed to LCDd "
                    "that it doesn't understand" << endl;
            cerr << "last command: " << last_command << endl;
        }
        else if (aList.first() == "key")
           handleKeyPress(aList.last().stripWhiteSpace());
    }
}

void LCD::handleKeyPress(QString key_pressed)
{
    int key = 0;

    char mykey = key_pressed.ascii()[0];
    switch (mykey)
    {
        case LCD_KEY_UP: key = Qt::Key_Up; break;
        case LCD_KEY_DOWN: key = Qt::Key_Down; break;
        case LCD_KEY_LEFT: key = Qt::Key_Left; break;
        case LCD_KEY_RIGHT: key = Qt::Key_Right; break;
        case LCD_KEY_YES: key = Qt::Key_Space; break;
        case LCD_KEY_NO: key = Qt::Key_Escape; break;
        default: break;
    } 

    QApplication::postEvent(gContext->GetMainWindow(),
                            new ExternalKeycodeEvent(key));
}

void LCD::init()
{
    // Stop the timer
    retryTimer->stop();

    QString aString;
    int i;

    timeformat = gContext->GetSetting("TimeFormat", "h:mm AP");
    
    lcd_showmusic=(gContext->GetSetting("LCDShowMusic", "1")=="1");
    lcd_showtime=(gContext->GetSetting("LCDShowTime", "1")=="1");
    lcd_showchannel=(gContext->GetSetting("LCDShowChannel", "1")=="1");
    lcd_showgeneric=(gContext->GetSetting("LCDShowGeneric", "1")=="1");
    lcd_showvolume=(gContext->GetSetting("LCDShowVolume", "1")=="1");
    lcd_showmenu=(gContext->GetSetting("LCDShowMenu", "1")=="1");
    lcd_backlighton=(gContext->GetSetting("LCDBacklightOn", "1")=="1");
    aString = gContext->GetSetting("LCDPopupTime", "5");
    lcd_popuptime = aString.toInt() * 1000;

    connected = TRUE;

    // This gets called when we receive the "connect" string from the server
    // indicating that "hello" was succesful

    sendToServer("client_set name Myth");
    sendToServer("client_add_key ABCDEF");   
 
    // Create all the screens and widgets (when we change activity in the myth 
    // program, we just swap the priorities of the screens to show only the 
    // "current one")

    sendToServer("screen_add Time");
    sendToServer("widget_del Time heartbeat");
    sendToServer("screen_set Time priority 255");
    if (lcd_backlighton)
        sendToServer("screen_set Time backlight on");
    else
        sendToServer("screen_set Time backlight off");
    sendToServer("widget_add Time timeWidget string");
    sendToServer("widget_add Time topWidget string");

    // The Menu Screen    
    // I'm not sure how to do multi-line widgets with lcdproc so we're going 
    // the ghetto way

    sendToServer("screen_add Menu");
    sendToServer("widget_del Menu heartbeat");
    sendToServer("screen_set Menu priority 255");
    sendToServer("screen_set Menu backlight on");
    sendToServer("widget_add Menu topWidget string");
    sendToServer("widget_add Menu menuWidget1 string");
    sendToServer("widget_add Menu menuWidget2 string");
    sendToServer("widget_add Menu menuWidget3 string");
    sendToServer("widget_add Menu menuWidget4 string");
    sendToServer("widget_add Menu menuWidget5 string");

    // The Music Screen

    sendToServer("screen_add Music");
    sendToServer("widget_del Music heartbeat");
    sendToServer("screen_set Music priority 255");
    sendToServer("screen_set Music backlight on");
    sendToServer("widget_add Music topWidget string");
    
    // Have to put in 10 bars for equalizer
    
    for (i = 0; i < 10; i++)
    {
        aString = "widget_add Music vbar";
        aString += QString::number(i);
        aString += " vbar";
        sendToServer(aString);
    }

    // The Channel Screen
    
    sendToServer("screen_add Channel");
    sendToServer("widget_del Channel heartbeat");
    sendToServer("screen_set Channel priority 255");
    sendToServer("screen_set Channel backlight on");
    sendToServer("widget_add Channel topWidget string");
    sendToServer("widget_add Channel botWidget string");
    sendToServer("widget_add Channel progressBar hbar");
   
    // The Generic Screen

    sendToServer("screen_add Generic");
    sendToServer("widget_del Generic heartbeat");
    sendToServer("screen_set Generic priority 255");
    sendToServer("screen_set Generic backlight on");
    sendToServer("widget_add Generic textWidget1 string");
    sendToServer("widget_add Generic textWidget2 string");
    sendToServer("widget_add Generic textWidget3 string");
    sendToServer("widget_add Generic progressBar hbar");

    // The Volume Screen

    sendToServer("screen_add Volume");
    sendToServer("widget_del Volume heartbeat");
    sendToServer("screen_set Volume priority 255");
    sendToServer("screen_set Volume backlight on");
    sendToServer("widget_add Volume topWidget string");
    sendToServer("widget_add Volume botWidget string");
    sendToServer("widget_add Volume progressBar hbar");

    lcd_ready = true;
 
    switchToTime();    // clock is on by default
    
    // send buffer if there's anything in there
    
    if (send_buffer.length() > 0)
    {
        sendToServer(send_buffer);
        send_buffer = "";
    }
}

void LCD::setWidth(unsigned int x)
{
    if (x < 1 || x > 80)
        return;
    else
        lcdWidth = x;
}

void LCD::setHeight(unsigned int x)
{
    if (x < 1 || x > 80)
        return;
    else
        lcdHeight = x;
}

void LCD::setCellWidth(unsigned int x)
{
    if (x < 1 || x > 16)
        return;
    else
        cellWidth = x;
}

void LCD::setCellHeight(unsigned int x)
{
    if (x < 1 || x > 16)
        return;
    else
        cellHeight = x;
}

void LCD::describeServer()
{
    VERBOSE(VB_GENERAL, QString("lcddevice: The server is %1x%2 with each cell being %3x%4." )
                        .arg(lcdWidth).arg(lcdHeight).arg(cellWidth).arg(cellHeight));
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

    VERBOSE(VB_IMPORTANT, QString("Could not connect to LCDd: %1").arg(err));
    socket->clearPendingData();
    socket->close();
}

void LCD::scrollText()
{
    if (theMode == 0)
        outputLeftText("Time", scrollingText.mid(scrollPosition, lcdWidth),
                       scrollWidget, scrollRow);
    else if (theMode == 1)
        outputLeftText("Music", scrollingText.mid(scrollPosition, lcdWidth),
                       scrollWidget, scrollRow);
    else if (theMode == 2)
        outputLeftText("Channel", scrollingText.mid(scrollPosition, lcdWidth),
                       scrollWidget, scrollRow);
    else if (theMode == 3)
        outputLeftText("Generic", scrollingText.mid(scrollPosition, lcdWidth),
                       scrollWidget, scrollRow);

    scrollPosition++;
    if (scrollPosition >= scrollingText.length())
        scrollPosition = 0;
}

void LCD::stopAll()
{
    // The usual reason things would get this far and then lcd_ready being 
    // false is the connection died and we're trying to re-establish the 
    // connection
    if (lcd_ready)
    {
        sendToServer("screen_set Music priority 255");
        sendToServer("screen_set Channel priority 255");
        sendToServer("screen_set Generic priority 255");
        sendToServer("screen_set Volume priority 255");
        sendToServer("screen_set Menu priority 255");
    }

    preScrollTimer->stop();
    scrollTimer->stop();
    musicTimer->stop();
    channelTimer->stop();
    genericTimer->stop();
    popMenuTimer->stop();
    menuScrollTimer->stop();
    menuPreScrollTimer->stop();
    unPopMenu();
}

void LCD::startTime()
{
    sendToServer("screen_set Time priority 128");
    timeTimer->start(1000, FALSE);
    outputTime();
    theMode = 0;    
}

void LCD::outputText(QPtrList<LCDTextItem> *textItems)
{
    if (!lcd_ready)
        return;

    QPtrListIterator<LCDTextItem> it( *textItems );
    LCDTextItem *curItem;
    QString num;

    unsigned int counter = 1;

    // Do the definable scrolling in here.
    // Use asignScrollingText(curItem->getText(), "textWidget" + num);
    // When scrolling is set, alignment has no effect
    while ((curItem = it.current()) != 0 && counter < lcdHeight)
    {
        ++it;
        num.setNum(curItem->getRow());
        if (curItem->getScroll())
            assignScrollingText(curItem->getText(), "textWidget" + num, 
                                curItem->getRow());
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

void LCD::outputCenteredText(QString theScreen, QString theText, QString widget,
                             int row)
{
    QString aString;
    unsigned int x = 0;
    
    x = (int) (rint((lcdWidth - theText.length()) / 2.0) + 1);
    // Had a problem where I would get something like this:
    //    2147483633 = (int) (rint((20 - 51) / 2.0) + 1)
    // so hense this
    if (x > lcdWidth)
        x = 1;

    aString = "widget_set ";
    aString += theScreen;
    aString += " " + widget + " ";
    aString += QString::number(x);
    aString += " ";
    aString += QString::number(row);
    aString += " \"";
    aString += theText;
    aString += "\"";
    sendToServer(aString);
}

void LCD::outputLeftText(QString theScreen, QString theText, QString widget,
                         int row)
{
    QString aString;
    aString = "widget_set ";
    aString += theScreen;
    aString += " " + widget + " 1 ";
    aString += QString::number(row);
    aString += " \"";
    aString += theText;
    aString += "\"";
    sendToServer(aString);
}

void LCD::outputRightText(QString theScreen, QString theText, QString widget,
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
    aString += theText;
    aString += "\"";
    sendToServer(aString);
}

void LCD::assignScrollingText(QString theText, QString theWidget, int theRow)
{
    // Have to decide what to do with "top line" text given the size
    // of the LCD (as reported by the server)
    
    scrollWidget = theWidget;
    scrollRow = theRow;

    if (theText.length() < lcdWidth)
    {
        // This is trivial, just show the text

        if (theMode == 0)
            outputCenteredText("Time", theText, theWidget, theRow);
        else if (theMode == 1)
            outputCenteredText("Music", theText, theWidget, theRow);
        else if (theMode == 2)
            outputCenteredText("Channel", theText, theWidget, theRow);
        else if (theMode == 3)
            outputCenteredText("Generic", theText, theWidget, theRow);       
 
        scrollTimer->stop();
        preScrollTimer->stop();
    }
    else
    {
        // Setup for scrolling
        if (theMode == 0)
            outputCenteredText("Time", theText.left(lcdWidth), theWidget, 
                               theRow);
        else if (theMode == 1)
            outputCenteredText("Music", theText.left(lcdWidth), theWidget, 
                               theRow);
        else if (theMode == 2)
            outputCenteredText("Channel", theText.left(lcdWidth), theWidget,
                               theRow);
        else if (theMode == 3)
            outputCenteredText("Generic", theText.left(lcdWidth), theWidget, 
                               theRow);

        scrollingText = theText;
        scrollPosition = 0;
        scrollTimer->stop();
        preScrollTimer->start(2000, TRUE);
    }
}

void LCD::startMusic(QString artist, QString track)
{
    QString aString;
    if (lcd_showmusic)
      sendToServer("screen_set Music priority 64");
    musicTimer->start(100, FALSE);
    aString = artist;
    aString += " - ";
    aString += track;
    theMode = 1;
    assignScrollingText(aString);
}

void LCD::startChannel(QString channum, QString title, QString subtitle)
{
    QString aString;
    if (lcd_showchannel)
      sendToServer("screen_set Channel priority 64");
    aString = channum;
    theMode = 2;
    assignScrollingText(aString, "topWidget", 1);
    aString = title;
    if (subtitle.length() > 0)
    {
        aString += " - '";
        aString += subtitle;
        aString += "'";
    }
    assignScrollingText(aString, "botWidget", 2);
    if (lcdHeight > 2)
    {
        progress = 0.0;
        channelTimer->start(500, FALSE);
        outputChannel();
    }
}

void LCD::startGeneric(QPtrList<LCDTextItem> * textItems)
{
    LCDTextItem *curItem;
    QPtrListIterator<LCDTextItem> it( *textItems );
    QString aString;

    if (lcd_showgeneric)
      sendToServer("screen_set Generic priority 64");

    // Clear out the LCD.  Do this before checking if its empty incase the user
    //  wants to just clear the lcd
    outputLeftText("Generic", "", "textWidget1", 1);
    outputLeftText("Generic", "", "textWidget2", 2);
    outputLeftText("Generic", "", "textWidget3", 3);

    // If nothing, return without setting up the timer, etc
    if (textItems->isEmpty())
        return;

    genericTimer->start(500, FALSE);
    theMode = 3;
    generic_progress = 0.0;

    // Return if there are no more items
    if ((curItem = it.current()) == 0)
        return;

    // Todo, make scrolling definable in LCDTextItem
    ++it;

    theMode = 3;
    assignScrollingText(curItem->getText(), "textWidget1", curItem->getRow());

    outputGeneric();

    // Pop off the first item so item one isn't written twice
    if (textItems->removeFirst() != 0)
        outputText(textItems);
}

void LCD::startMenu(QPtrList<LCDMenuItem> *menuItems, QString app_name, 
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
        sendToServer("screen_set Menu priority 15");

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
        sendToServer("widget_set Menu menuWidget1 1 2 \"     ABORTING     ");
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
    if (counter == it.count() && counter != 1)
        --it;

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

void LCD::beginScrollingMenuText()
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

void LCD::scrollMenuText()
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

void LCD::startVolume(QString app_name)
{
    if (lcd_showvolume)
      sendToServer("screen_set Volume priority 32");
    outputCenteredText("Volume", "Myth " + app_name + " Volume");
    volume_level = 0.0;
    outputVolume();
}

void LCD::unPopMenu()
{
    // Stop the scrolling timer
    menuScrollTimer->stop();
    sendToServer("screen_set Menu priority 255");
}

void LCD::setLevels(int numbLevels, float *values)
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

void LCD::setChannelProgress(float value)
{
    if (!lcd_ready)
        return;

    progress = value;

    if (progress < 0.0)
        progress = 0.0;
    else if (progress > 1.0)
        progress = 1.0;
}

void LCD::setGenericProgress(float value)
{
    if (!lcd_ready)
        return;

    generic_progress = value;

    if (generic_progress < 0.0)
        generic_progress = 0.0;
    else if (generic_progress > 1.0)
        generic_progress = 1.0;

    outputGeneric();
}

void LCD::setVolumeLevel(float value)
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

void LCD::outputLEDs()
{
    QString aString;
    int mask = 0;
    if (0 && GetLEDMask)
        mask = GetLEDMask();
    aString = "output ";
    aString += QString::number(mask);
    sendToServer(aString);
}

void LCD::outputTime()
{
    QString aString;
    int x, y;

    if (lcdHeight < 3)
        y = 2;
    else
        y = (int) rint(lcdHeight / 2) + 1;

    x = (int) rint((lcdWidth - 5) / 2) + 1;

    aString = "widget_set Time timeWidget ";
    aString += QString::number(x);
    aString += " ";
    aString += QString::number(y);
    aString += " \"";
    if (lcd_showtime) {
        aString += QTime::currentTime().toString(timeformat).left(8) + "\"";
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

void LCD::outputMusic()
{
    QString aString;

    for(int i = 0; i < 10; i++)
    {
        aString = "widget_set Music vbar";
        aString += QString::number(i);
        aString += " ";
        aString += QString::number(i + (int) rint((lcdWidth - 10) / 2.0));
        aString += " ";
        aString += QString::number(lcdHeight);
        aString += " ";
        aString += QString::number((int)rint(EQlevels[i] * (lcdHeight - 1) * 
                                   (cellHeight -1)));
        sendToServer(aString);
    }
}

void LCD::outputChannel()
{
    QString aString;
    aString = "widget_set Channel progressBar 1 ";
    aString += QString::number(lcdHeight);
    aString += " ";
    aString += QString::number((int)rint(progress * lcdWidth * cellWidth));
    sendToServer(aString);
}

void LCD::outputGeneric()
{
    QString aString;
    aString = "widget_set Generic progressBar 1 ";
    aString += QString::number(lcdHeight);
    aString += " ";
    aString += QString::number((int)rint(generic_progress * lcdWidth * 
                               cellWidth));
    sendToServer(aString);
}

void LCD::outputVolume()
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

void LCD::switchToTime()
{
    if (!lcd_ready)
        return;

    stopAll();
    startTime();
}

void LCD::switchToMusic(const QString &artist, const QString &track)
{
    if (!lcd_ready)
        return;

    stopAll();
    startMusic(artist, track);
}

void LCD::switchToChannel(QString channum, QString title, QString subtitle)
{
    if (!lcd_ready)
        return;

    stopAll();
    startChannel(channum, title, subtitle);
}

void LCD::switchToMenu(QPtrList<LCDMenuItem> *menuItems, QString app_name, 
                       bool popMenu)
{
    if (!lcd_ready)
        return;

    stopAll();
    startMenu(menuItems, app_name, popMenu);
}

void LCD::switchToGeneric(QPtrList<LCDTextItem> *textItems)
{
    if (!lcd_ready)
        return;

    stopAll();
    startGeneric(textItems);
}

void LCD::switchToVolume(QString app_name)
{
    if (!lcd_ready)
        return;

    stopAll();
    startVolume(app_name);
}

void LCD::switchToNothing()
{
    if (!lcd_ready)
        return;

    stopAll();
}

void LCD::shutdown()
{
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

    QString aString;
    for (int i = 0 ; i < 10; i++)
    {
        aString = QString("widget_del Music vbar%1").arg(i);
        sendToServer(aString);
    }
    sendToServer("widget_del Music topWidget");
    sendToServer("screen_del Music");
    
    sendToServer("widget_del Time timeWidget");
    sendToServer("widget_del Time topWidget");
    sendToServer("screen_del Time");
    
    socket->close();

    lcd_ready = false;
    connected = false;
}

LCD::~LCD()
{
    m_lcd = NULL;

#ifdef LCD_DEVICE_DEBUG
    cout << "lcddevice: An LCD device is being snuffed out of existence "
            "(~LCD() was called)" << endl ;
#endif
    if (socket)
    {
        delete socket;
        lcd_ready = false;
    }
    if (lcdMenuItems)
        delete lcdMenuItems;
}

