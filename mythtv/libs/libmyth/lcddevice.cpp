/*

    lcddevice.cpp
    
    a MythTV project object to control an
    LCDproc server
    
    (c) 2002, 2003 Thor Sigvaldason and Isaac Richards

*/

#include "lcddevice.h"

LCD::LCD()
	:QObject()
{
    //
    //  Constructor for LCD
    //
    //  Note that this does *not* include opening the 
    //  socket and initiating communications with the
    //	LDCd daemon.
    //

#ifdef LCD_DEVICE_DEBUG
    cout << "lcddevice: An LCD object now exists (LCD() was called)" << endl ;
#endif

	socket = new QSocket(this);
	connect(socket, SIGNAL(error(int)), this, SLOT(veryBadThings(int)));
	connect(socket, SIGNAL(readyRead()), this, SLOT(serverSendingData()));

	//
	//	Some some backstop values
	//
	
	lcdWidth = 5;
	lcdHeight = 1;
	cellWidth = 1;
	cellHeight = 1;	
	
	scrollingText = "";
	progress = 0.0;
	connected = FALSE;
	send_buffer = "";

	timeTimer = new QTimer(this);
	connect(timeTimer, SIGNAL(timeout()), this, SLOT(outputTime()));	
	
	scrollTimer = new QTimer(this);
	connect(scrollTimer, SIGNAL(timeout()), this, SLOT(scrollText()));
	
	preScrollTimer = new QTimer(this);
	connect(preScrollTimer, SIGNAL(timeout()), this, SLOT(beginScrollingText()));
	
	musicTimer = new QTimer(this);
	connect(musicTimer, SIGNAL(timeout()), this, SLOT(outputMusic()));
	
	channelTimer = new QTimer(this);
	connect(channelTimer, SIGNAL(timeout()), this, SLOT(outputChannel()));

	popMenuTimer = new QTimer(this);
	connect(popMenuTimer, SIGNAL(timeout()), this, SLOT(unPopMenu()));

}

void LCD::connectToHost(QString hostname, unsigned int port)
{
	//
	//	Open communications
	//

	if(!connected)
	{
#ifdef LCD_DEVICE	
	    QTextStream os(socket);
		socket->connectToHost(hostname, port);
		os << "hello\n";
		
#else
		hostname = hostname;
		port = port;
#endif
	}
}

void LCD::beginScrollingText()
{
	unsigned int i;
	QString aString;
	//
	//	After the topline text has
	//	been on the screen for "a while"
	//	start scrolling it.
	//
	
	aString = "";
	for(i = 0; i < lcdWidth; i++)
	{
		scrollingText.prepend(" ");
	}
	scrollPosition = lcdWidth;
	scrollTimer->start(400, FALSE);
}

void LCD::sendToServer(QString someText)
{
#ifdef LCD_DEVICE
	QTextStream os(socket);
	
	if(connected)
	{
#ifdef LCD_DEVICE_DEBUG
    	cout << "lcddevice: Sending to Server: " << someText << endl ;
#endif

		//
		//	Just stream the text out the socket
		//

		os << someText << "\n";
	}
	else
	{
	    //  Buffer this up in the hope that
	    //  the connection will open soon
	    //
	    //  I wonder how large a QString can get?

	    send_buffer += someText;
	    send_buffer += "\n";
	}
#else
	someText = someText;
#endif
}

void LCD::serverSendingData()
{
	QString					lineFromServer, tempString;
	QStringList 			aList;
	QStringList::Iterator 	it;
	
	//
	//	This gets activated automatically
	//	by the QSocket class whenever
	//	there's something to read.
	//
	//	We currently spend most of our time (except for
	//	the first line sent back) ignoring it.
	//
	//	Note that if anyone has an LCDproc type
	//	lcd with buttons on it, this is where
	//	we would want to catch button presses
	//	and make the rest of mythTV/mythMusic
	//	do something (change tracks, channels, etc.)
	//
	
	while(socket->canReadLine())
	{
		lineFromServer = socket->readLine();


#ifdef LCD_DEVICE_DEBUG		
		cout << "lcddevice: Received from server: " << lineFromServer ;
#endif


		aList = QStringList::split(" ", lineFromServer);
		if(aList.first() == "connect")
		{
			//
			//	We got a connect,
			//	which is a response
			//	to "hello" (first interaction)
			//
			//	Need to parse out
			//	some data according
			//	the LCDproc client/server
			//	spec (which is poorly
			//	documented)
			//
						
			it = aList.begin();
			it++;
			if((*it) != "LCDproc")
			{
				cerr << "lcddevice: WARNING: Second parameter returned from LCDd was not \"LCDproc\"" << endl ;
			}
			
			//
			//	Skip through some stuff
			//
			
			it++; //	server version
			it++; //	the string "protocol"
			it++; //	protocol version
			it++; //	the string "lcd"
			it++; //	the string "wid";
			it++; //	Ah, the LCD width
			

			tempString = *it;
			setWidth(tempString.toInt());

			it++; //	the string "hgt"
			it++; //	the LCD height
			
			tempString = *it;
			setHeight(tempString.toInt());

			it++; //	the string "cellwid"
			it++; //	Cell width in pixels;

			tempString = *it;
			setCellWidth(tempString.toInt());


			it++; //	the string "cellhgt"
			it++; //	Cell height in pixels;

			tempString = *it;
			setCellHeight(tempString.toInt());

			init();

#ifdef LCD_DEVICE_DEBUG
			describeServer();	
#endif
		}
		if(aList.first() == "huh?")
		{
			cerr << "lcddevice: WARNING: Something is getting passed to LCDd that it doesn't understand" << endl;
		}
	}
}

void LCD::init()
{
	QString aString, bString;
	int i;
	
	connected = TRUE;
	//
	//	This gets called when we receive the 
	//	"connect" string from the server
	//	indicating that "hello" was succesful
	//
	sendToServer("client_set name Myth");
	
	//
	//	Create all the screens and widgets
	//	(when we change activity in the
	//	myth program, we just swap the
	//	priorities of the screens to
	//	show only the "current one")
	//

	sendToServer("screen_add Time");
	sendToServer("widget_del Time heartbeat");
	sendToServer("screen_set Time priority 255");
	sendToServer("widget_add Time timeWidget string");
	sendToServer("widget_add Time topWidget string");

	//
	//	The Pop Menu Screen	
	//

	sendToServer("screen_add Menu");
	sendToServer("widget_del Menu heartbeat");
	sendToServer("screen_set Menu priority 255");
	sendToServer("widget_add Menu menuWidget string");
	sendToServer("widget_add Menu topWidget string");

	//
	//	The Music Screen
	//

	sendToServer("screen_add Music");
	sendToServer("widget_del Music heartbeat");
	sendToServer("screen_set Music priority 255");
	sendToServer("widget_add Music topWidget string");
	
	//
	//	Have to put in 10 bars for equalizer
	//
	
	for(i = 0; i < 10; i++)
	{
		aString = "widget_add Music vbar";
		bString.setNum(i);
		aString += bString;
		aString += " vbar";
		sendToServer(aString);
	}

	//
	//	The Channel Screen
	//
	
	sendToServer("screen_add Channel");
	sendToServer("widget_del Channel heartbeat");
	sendToServer("screen_set Channel priority 255");
	sendToServer("widget_add Channel topWidget string");
	sendToServer("widget_add Channel progressBar hbar");
		
	
	
	switchToTime();	// clock is on by default
	
	//
	//  send buffer if there's anything in there
	//
	
	if(send_buffer.length() > 0)
	{
	    sendToServer(send_buffer);
	    send_buffer = "";
	}
}

void LCD::setWidth(unsigned int x)
{
	if(x < 1 || x > 80)
	{
		return;
	}
	else
	{
		lcdWidth = x;
	}
}

void LCD::setHeight(unsigned int x)
{
	if(x < 1 || x > 80)
	{
		return;
	}
	else
	{
		lcdHeight = x;
	}
}

void LCD::setCellWidth(unsigned int x)
{
	if(x < 1 || x > 16)
	{
		return;
	}
	else
	{
		cellWidth = x;
	}
}

void LCD::setCellHeight(unsigned int x)
{
	if(x < 1 || x > 16)
	{
		return;
	}
	else
	{
		cellHeight = x;
	}
}

void LCD::describeServer()
{
	cout	<< "lcddevice: The server is " 
			<< lcdWidth
			<< "x"
			<< lcdHeight
			<< " with each cell being "
			<< cellWidth
			<< "x"
			<< cellHeight
			<< endl;
}

void LCD::veryBadThings(int anError)
{
	//
	//	Deal with failures to connect and
	//	inabilities to communicate
	//	(Oprah, Dr. Phil, ...)
	//
	
	cerr << "The QSocket connector in lcddevice could not connect to an LCDd..." << endl;
	if(anError == QSocket::ErrConnectionRefused)
	{
		cerr << "Why? The connection was refused." << endl ;  
	}
	if(anError == QSocket::ErrHostNotFound)
	{
		cerr << "Why? The host was not found." << endl ;  
	}
	if(anError == QSocket::ErrSocketRead)
	{
		cerr << "Why? There was an error reading from the socket." << endl ;  
	}
	
#if (QT_VERSION >= 0x030100)
	socket->clearPendingData();
	socket->close();
#else
#warning
#warning ***   You should think seriously about upgrading your Qt to 3.1 or higher   ***
#warning
    delete socket;
    socket = NULL;
#endif
	
}

void LCD::scrollText()
{
	if(theMode == 0)
	{
		outputLeftTopText("Time", scrollingText.mid(scrollPosition, lcdWidth));
	}
	if(theMode == 1)
	{
		outputLeftTopText("Music", scrollingText.mid(scrollPosition, lcdWidth));
	}
	if(theMode == 2)
	{
		outputLeftTopText("Channel", scrollingText.mid(scrollPosition, lcdWidth));
	}
	scrollPosition++;
	if(scrollPosition >= scrollingText.length())
	{
		scrollPosition = 0;
	}
}

void LCD::stopAll()
{
	sendToServer("screen_set Time priority 255");
	sendToServer("screen_set Music priority 255");
	sendToServer("screen_set Channel priority 255");
	sendToServer("screen_set Menu priority 255");
	scrollTimer->stop();
	musicTimer->stop();
	timeTimer->stop();
	channelTimer->stop();
	popMenuTimer->stop();
	unPopMenu();
}

void LCD::startTime()
{
	sendToServer("screen_set Time priority 64");
	timeTimer->start(500, FALSE);
	outputTime();
	theMode = 0;	
	assignScrollingText("                                                               MythTV");
}


void LCD::outputCenteredTopText(QString theScreen, QString theText)
{
	QString aString, bString;
	unsigned int x;
	
	x = (int) (rint((lcdWidth - theText.length()) / 2.0) + 1);
	aString = "widget_set ";
	aString += 	theScreen;
	aString += " topWidget ";
	bString.setNum(x);
	aString += bString;
	aString += " 1 \"";
	aString += theText;
	aString += "\"";
	sendToServer(aString);
}

void LCD::outputLeftTopText(QString theScreen, QString theText)
{
	QString aString;
	aString = "widget_set ";
	aString += 	theScreen;
	aString += " topWidget 1 1 \"";
	aString += theText;
	aString += "\"";
	sendToServer(aString);
}

void LCD::assignScrollingText(QString theText)
{
	//
	//	Have to decide what to do with
	//	"top line" text given the size
	//	of the LCD (as reported by
	//	the server)
	//
	
	if(theText.length() < lcdWidth)
	{
		//
		//	This is trivial, just show the text
		//
		if(theMode == 0)
		{
			outputCenteredTopText("Time", theText);
		}
		if(theMode == 1)
		{
			outputCenteredTopText("Music", theText);
		}
		if(theMode == 2)
		{
			outputCenteredTopText("Channel", theText);
		}
		scrollTimer->stop();
		preScrollTimer->stop();
	}
	else
	{
		//
		//	Setup for scrolling
		//
		if(theMode == 0)
		{
			outputCenteredTopText("Time", theText.left(lcdWidth));
		}
		if(theMode == 1)
		{
			outputCenteredTopText("Music", theText.left(lcdWidth));
		}
		if(theMode == 2)
		{
			outputCenteredTopText("Channel", theText.left(lcdWidth));
		}
		
		scrollingText = theText;
		scrollPosition = 0;
		scrollTimer->stop();
		preScrollTimer->start(2000, TRUE);
		
	}
}



void LCD::startMusic(QString artist, QString track)
{
	QString aString;
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
	sendToServer("screen_set Channel priority 64");
	channelTimer->start(500, FALSE);
	aString = channum;
	aString += ": ";
	aString += title;
	if(subtitle.length() > 0)
	{
		aString += " (";
		aString += subtitle;
		aString += ")";
	}
	theMode = 2;
	assignScrollingText(aString);
	progress = 0.0;
	outputChannel();
}

void LCD::popMenu(QString menu_item, QString menu_title)
{
	QString aString;
	sendToServer("screen_set Menu priority 16");
	popMenuTimer->start(5000, TRUE);
	outputCenteredTopText("Menu", menu_title.left(lcdWidth));

	aString  = "widget_set Menu menuWidget 1 2 \">";
	aString += menu_item.left(lcdWidth - 1);
	aString += "\"";
	sendToServer(aString);
}

void LCD::unPopMenu()
{
	sendToServer("screen_set Menu priority 255");
}

void LCD::setLevels(int numbLevels, float *values)
{
	int i;
	//
	//	Set the EQ levels
	//
	for(i = 0; i < 10; i++)
	{
		if(i >= numbLevels)
		{
			EQlevels[i] = 0.0;
		}
		else
		{
			EQlevels[i] = values[i];
			if(EQlevels[i] < 0.0)
			{
				EQlevels[i] = 0.0;
			}
			if(EQlevels[i] > 1.0)
			{
				EQlevels[i] = 1.0;
			}
		}
	} 
}

void LCD::setChannelProgress(float value)
{
	progress = value;
	if(progress < 0.0)
	{
		progress = 0.0;
	}
	if(progress > 1.0)
	{
		progress = 1.0;
	}
}


void LCD::outputTime()
{
	QString aString, bString;
	int x, y;
	//
	//	
	//

	if(lcdHeight < 3)
	{
		y = 2;
	}
	else
	{
		y = (int) rint(lcdHeight / 2) + 1;
	}
	x = (int) rint((lcdWidth - 5) / 2) + 1;

	aString = "widget_set Time timeWidget ";
	bString.setNum(x);
	aString += bString;
	aString += " ";
	bString.setNum(y);
	aString += bString;
	aString += " ";
	aString += QTime::currentTime().toString("hh:mm").left(5);
	sendToServer(aString);
}

void LCD::outputMusic()
{

	QString aString, bString;
	int i;
	//
	//	
	//
	for(i = 0; i < 10; i++)
	{
		aString = "widget_set Music vbar";
		bString.setNum(i);
		aString += bString;
		aString += " ";
		bString.setNum(i + (int) rint((lcdWidth - 10) / 2.0));
		aString += bString;
		aString += " ";
		bString.setNum(lcdHeight);
		aString += bString;
		aString += " ";
		bString.setNum((int) rint(EQlevels[i] * (lcdHeight - 1) * ( cellHeight -1)));
		aString += bString;
		sendToServer(aString);
	}
}

void LCD::outputChannel()
{
	QString aString, bString;
	aString = "widget_set Channel progressBar 1 ";
	bString.setNum(lcdHeight);
	aString += bString;
	aString += " ";
	bString.setNum((int) rint(progress * lcdWidth * cellWidth));
	aString += bString;
	sendToServer(aString);
}

void LCD::switchToTime()
{
	stopAll();
	startTime();
}

void LCD::switchToMusic(QString artist, QString track)
{
	stopAll();
	startMusic(artist, track);
}

void LCD::switchToChannel(QString channum, QString title, QString subtitle)
{
	stopAll();
	startChannel(channum, title, subtitle);
}

void LCD::switchToNothing()
{
	stopAll();
}

void LCD::shutdown()
{
#ifdef LCD_DEVICE

	stopAll();

    //
    //  Remove all the widgets and screens for
    //  a clean exit from the server
    //
    
    sendToServer("widget_del Channel progressBar");
    sendToServer("widget_del Channel topWidget");
    sendToServer("screen_del Channel");

    QString aString;
    for(int i = 0 ; i < 10; i++)
    {
        aString = QString("widget_del Music vbar%1").arg(i);
        sendToServer(aString);
    }
    sendToServer("widget_del Music topWidget");
    sendToServer("screen_del Music");
    
    sendToServer("widget_del Menu menuWidget");
    sendToServer("widget_del Menu topWidget");
    sendToServer("screen_del Menu");

    sendToServer("widget_del Time timeWidget");
    sendToServer("widget_del Time topWidget");
    sendToServer("screen_del Time");
    
	socket->close();

#endif
    connected = false;
}

LCD::~LCD()
{
#ifdef LCD_DEVICE_DEBUG
    cout << "lcddevice: An LCD device is being snuffed out of existence (~LCD() was called)" << endl ;
#endif
    if(socket)
    {
    	delete socket;
    }
}
