#include <fstream>
#include <iostream>
#include <qtimer.h>
#include <qheader.h>
#include <unistd.h>

#include "weathercomms.h"

using namespace std;

WeatherSock::WeatherSock(Weather *con, bool tDebug, int tAgg)
{
        debug = tDebug;
	parent = con;
	httpData = "";
 	gotDataHook = false;
	timeout_cnt = 0;
	invalid_cnt = 0;
        imgSize = -1;
        writeImage = false;
   	connectType = 0;
	reset_cnt = 0;
        httpSock = NULL;
	if (tAgg < 1)
		aggressiveness = 1;
	else
		aggressiveness = tAgg;

	if (debug == true)
		cout << "MythWeather: COMMS : Using Aggressiveness value of " << aggressiveness << endl;

	myStatus = false;
	currentError = 0;
	error = 0;
	breakout = false;
}

QString WeatherSock::getData()
{
	return weatherData;
}

bool WeatherSock::getStatus()
{
	return myStatus;
}

int WeatherSock::getIntStatus()
{
	int curStatus = 0;

        if (httpSock->state() == QSocket::HostLookup)
		curStatus = 1;
        else if (httpSock->state() == QSocket::Connecting)
		curStatus = 2;
        else if (httpSock->state() == QSocket::Connected)
		curStatus = 3;
        else if (httpSock->state() == QSocket::Closing)
		curStatus = 4;

	return curStatus;
}

int WeatherSock::checkError()
{
	return currentError;
}

int WeatherSock::verifyData()
{
	if (httpData.find("'ZIPCHOICE_IS_WEATHER'", 0) > 0)
	{
		return 2;
	}

	if (httpData.find("<html>", 0) > 0 ||
            httpData.find("Microsoft VBScript runtime", 0) > 0 ||
            httpData.find("Internal Server Error", 0) > 0 ||
            httpData.find("Bad Request", 0) > 0 ||
	    httpData.find("this.swAcid = \"\";", 0) > 0)
	{
		invalid_cnt++;
		if (debug == true)
			cout << "MythWeather: COMMS : Setting invalid_cnt = " << invalid_cnt << endl;

		if (invalid_cnt > 2)
		{
			if (debug == true)
				cout << "MythWeather: COMMS : Invalid Data Count > 2...\n";
			return 1;		// A Bad AREA ID
		}
		else
		{
			sleep(1);
			return -1;		// Error, try again
		}
	}

	return 0;
}

QString WeatherSock::strStatus()
{
	QString stat;

	if (httpSock->state() == QSocket::Idle)
		stat = "No Connection Pending.";
	else if (httpSock->state() == QSocket::HostLookup)
		stat = "DNS Lookup...";
	else if (httpSock->state() == QSocket::Connecting)
		stat = "Attempting Connection...";
	else if (httpSock->state() == QSocket::Connected)
		stat = "Connected! Requesting Data...";
	else if (httpSock->state() == QSocket::Closing)
		stat = "Closing Connection...";

	return stat;
}

void WeatherSock::startConnect()
{
        int multiplyer = 1;
        if (connectType == 3)
	    multiplyer = 8;
	breakout = false;
	timeout_cnt = 0;
	int curState;
	int tmpState = -99;
	int errs = 0;

	if (debug == true)
        	cout << "MythWeather: COMMS : Connecting to host...\n";

	makeConnection();

	curState = httpSock->state();

        while (breakout == false)
        {
		if (tmpState != curState)
		{
			if (debug == true)
				cout << "MythWeather: COMMS : State Transition : " << strStatus() << endl;
			curState = httpSock->state();
			timeout_cnt = 0;
		}	
         	parent->processEvents();
         	usleep(50);
		
		tmpState = httpSock->state();
		timeout_cnt++;

		if (timeout_cnt > (int)(5*aggressiveness) && getIntStatus() == 1)
			resetConnection();

		if (timeout_cnt > (int)(10*aggressiveness) && getIntStatus() == 2)
			resetConnection();

		if (timeout_cnt > (int)(15*multiplyer*aggressiveness) && getIntStatus() == 3)
			resetConnection();

		if (gotDataHook == true)
		{
                        if (connectType == 0)
 			{
			    errs = verifyData();
			    if (errs < 0)
			    {
				if (debug == true)
					cout << "MythWeather: COMMS : Received an error, resetting...\n";
				resetConnection();
			    }
			    if (errs >= 0)
			    {
				if (error != 3)
					error = errs;
				breakout = true;
			        weatherData = httpData;
			    }
                        }
 	 	  	else if (connectType == 1)
                        {
                            mapLoc = parseData("if (isMinNS4) var mapNURL = \"", "\";");
			    breakout = true;
                        }
                        else if (connectType == 2)
                        {
                            imageLoc = parseData("<IMG NAME=\"mapImg\" SRC=\"http://image.weather.com", "\"");
                            breakout = true;
                        }
                        else if (connectType == 3)
                        {
			    image_out.flush();
			    image_out.close();
			    breakout = true;
                        }
		}
	}

	currentError = (int)(error * 10);
        if (currentError == 0)
        {
            if (debug == true)
                cout << "MythWeather: No Error, Continuing to next stage...\n";
            connectType++;
            if (connectType < 4)
                startConnect();
            if (connectType == 4)
 	    {
                myStatus = true;
 		return;
	    }
        }
        else
        {
	    myStatus = true;

            return;
	}
}

QString WeatherSock::parseData(QString beg, QString end)
{
    QString ret;
     
    if (debug == true)
    {
        cout << "MythWeather: Parse HTML : Looking for: " << beg << ", ending with: " << end << endl;
        if (httpData.length() == 0)
            cout << "MythWeather: Parse HTML : No Data!\n";
    }
    int start = httpData.find(beg) + beg.length();
    int endint = httpData.find(end, start + 1);
    if (start != -1 && endint != -1)
    {
        ret = httpData.mid(start, endint - start);
        if (debug == true)
            cout << "MythWeather: Parse HTML : Returning : " << ret << endl;
        return ret;
    } 
    else
    {
        if (debug == true)
            cout << "MythWeather: Parse HTML : Parse Failed...returning <NULL>\n";
        ret = "<NULL>";
        return ret;
    } 
}

void WeatherSock::resetConnection()
{
	reset_cnt++;
	if (reset_cnt > 10)
	{
		cout << "MythWeather: COMMS : TIMEOUT : Maximum Timeout - "
		     << "Try changing your aggressiveness variable.\n";
		gotDataHook = true;
		breakout = true;
		error = 3;
		myStatus = true;
	}
	else
	{
		if (debug == true)
			cout << "MythWeather: COMMS : TIMEOUT : " << strStatus() 
			     << " --> Resetting (" << reset_cnt << " out of 10)" << endl;
        	httpSock->close();
		parent->processEvents();
		usleep(100);
		parent->processEvents();
        	httpData = "";
		gotDataHook = false;
        	timeout_cnt = 0;
		disconnect(httpSock, 0, 0, 0);
		delete httpSock;
                httpSock = NULL;
        	makeConnection();
	}
}

void WeatherSock::makeConnection()
{
        image_out.flush();
        image_out.close();
        writeImage = false;
        imgSize = -1;
        if (httpSock)
            delete httpSock;

	httpSock = new QSocket();
        connect(httpSock, SIGNAL(connected()),
                SLOT(socketConnected()));

        connect(httpSock, SIGNAL(connectionClosed()),
                SLOT(socketConnectionClosed()));

        connect(httpSock, SIGNAL(delayedCloseFinished()),
                SLOT(delayedClosed()));

        connect(httpSock, SIGNAL(readyRead()),
                SLOT(socketReadyRead()));

        connect(httpSock, SIGNAL(error(int)),
                SLOT(socketError(int)));

        gotDataHook = false;
        httpData = "";

  	if (connectType == 0)
		httpSock->connectToHost("www.msnbc.com", 80);
	else if (connectType == 1 || connectType == 2)
		httpSock->connectToHost("w3.weather.com", 80);
	else if (connectType == 3)
        {
                image_out.setName("/tmp/weather.jpg");
                image_out.open(IO_WriteOnly);
		httpSock->connectToHost("image.weather.com", 80);
        }

	parent->processEvents();
}

void WeatherSock::closeConnection()
{
	httpSock->close();
	if ( httpSock->state() == QSocket::Closing ) 
	{
        	parent->connect(httpSock, SIGNAL(delayedCloseFinished()),
                SLOT(socketClosed()) );
    	} 
	else 
	{
        	socketClosed();
    	}
}

void WeatherSock::socketReadyRead()
{
     int data = 0;
	if (debug == true)
		cout << "MythWeather: COMMS : Reading Data From Host [";

        if (connectType != 3)
        {
    	    while (httpSock->canReadLine()) 
	    {
		  if (debug == true)
        	      cout << ".";
          	  httpData = httpData + httpSock->readLine();
    	    }
        }
        else if (connectType == 3)
        {
            if (imgSize == -1)
            {
               while (httpSock->canReadLine())
               { 
                   if (debug == true)
                       cout << ".";
                   httpData = httpData + httpSock->readLine();
                   if (httpData.find("HTTP/1.0 400 Bad Request") != -1)
                   {
                       invalid_cnt++;
                       if (invalid_cnt >= 3)
                       {
                           if (debug == true)
                               cout << "MythWeather: COMMS : 400 Bad Request Error\n";
                           gotDataHook = true;
                           closeConnection();
                       }
                   }
                   if (httpData.find("Content-Length: ") != -1)
                   {
                       int start = httpData.find("Content-Length: ") + 16;
                       int end = httpData.find("Content-Type:");
                       imgSize = httpData.mid(start, end - start).toInt();  
                       if (debug == true)
		           cout << " [image size = " << imgSize << "] ";
                   }
                   usleep(20);
                   if (imgSize > -1)
                       break;
               }
            }
            if (imgSize > -1)
            {
               int last4[4];
               int cnt = 0;
               char temp;
               while (httpSock->bytesAvailable() && imgSize > 0)
               {
                     if (writeImage == true)
                     {
 			 temp = httpSock->getch();
                         image_out.putch(temp);
                         cnt++;
                         if (cnt == 1024)
                         { 
                             if (debug == true)
			         cout << "!";
  			     cnt = 0;
                         }
                         imgSize--;
                     }
 		     else
                     {
                         data = httpSock->getch();
                         last4[3] = last4[2];
                         last4[2] = last4[1];
                         last4[1] = last4[0];
                         last4[0] = data;
                         if (last4[3] == 13 && last4[2] == 10 && 
                             last4[1] == 13 && last4[0] == 10)
                         {
                             writeImage = true;
 		         }
                     }
               }
               if (imgSize <= 0)
               {
                   if (debug == true)
                       cout << "]\n";
 		   gotDataHook = true;
		   closeConnection();
               }

            }
        }
	if (debug == true)
    		cout << "]\n";
}

void WeatherSock::socketConnected()
{
   QString locale = parent->getLocation();
   if (connectType == 0)
   {
    	QTextStream os(httpSock);
    	os << "GET /m/chnk/d/weather_d_src.asp?acid="
       	   << locale << " HTTP/1.1\n"
           << "Connection: close\n"
           << "Host: www.msnbc.com\n\n\n";
    	httpData = "";
   }
   else if (connectType == 1)
   {
        QTextStream os(httpSock);
        os << "GET /weather/map/"
	   << locale << "?from=LAPmaps&setcookie=1"
           << " HTTP/1.1\n"
           << "Connection: close\n"
           << "Host: w3.weather.com\n\n\n";
        mapLoc = "";
   } 
   else if (connectType == 2)
   {
        QTextStream os(httpSock);
        os << "GET " << mapLoc 
           << " HTTP/1.1\n"
           << "Connection: close\n"
           << "Host: w3.weather.com\n\n\n";
        imageLoc = "";
   }
   else if (connectType == 3)
   {
	QTextStream os(httpSock);
        os << "GET "
           << imageLoc
           << " HTTP/1.1\n"
           << "Host: image.weather.com\n\n\n";
        imageData = "";
   }
}

void WeatherSock::delayedClosed()
{
	if (debug == true)
		cout << "MythWeather: COMMS : Delayed Socket Close.\n";
}

void WeatherSock::socketConnectionClosed()
{
	if (debug == true)
    		cout << "MythWeather: COMMS : Connection is closed.\n";
    	gotDataHook = true;
}

void WeatherSock::socketClosed()
{
	if (debug == true)
    		cout << "MythWeather: COMMS : Socket is closed.\n";
}

void WeatherSock::socketError( int e )
{
	QString errs;

	if (e == QSocket::ErrConnectionRefused)
		errs = "Error! Connection Refused.\n";
	else if (e == QSocket::ErrHostNotFound)
		errs = "Error! Host Not Found.\n";
	else if (e == QSocket::ErrSocketRead)
		errs = "Error! Socket Read Failed.\n";

	if (debug == true)
  		cout << "MythWeather: COMMS : " << errs << " --> Error #" << e << endl;
    	closeConnection();
    	e = 0;
}

/*
Get: http://www.weather.com/weather/map/16801?from=LAPmaps
     Looking for:
     if (isMinNS4) var mapNURL = "/maps/local/local/us_close_pit/1a/index_large.html";
---------------------------------------------------------------------------------------
Get: /maps/local/local/us_close_pit/1a/index_large.html
     Looking for:
     <IMG NAME="mapImg" SRC="http://image.weather.com/web/radar/us_pit_closeradar_large_usen.jpg" WIDTH=600 HEIGHT=405 BORDER=0 ALT="Doppler Radar 600 Mile"></TD>
--------------------------------------------------------------------------------------
GET /web/radar/us_pit_closeradar_large_usen.jpg HTTP/1.1
Host:image.weather.com

HTTP/1.1 200 OK
Server: Apache
Cache-Control: max-age=900
Last-Modified: Thu, 10 Apr 2003 17:25:12 GMT
ETag: "8bcf3-7ce2-21822e00"
Accept-Ranges: bytes
Content-Length: 31970
Content-Type: image/jpeg
Expires: Thu, 10 Apr 2003 17:41:01 GMT
Date: Thu, 10 Apr 2003 17:26:09 GMT
Connection: keep-alive
*/


