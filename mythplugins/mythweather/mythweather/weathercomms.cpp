#include <fstream>
#include <iostream>
#include <qtimer.h>
#include <qheader.h>
#include <unistd.h>

#include "weathercomms.h"

using namespace std;

WeatherSock::WeatherSock(Weather *con, bool tDebug)
{
	debug = tDebug;
	parent = con;
	httpData = "";
 	gotDataHook = false;
	timeout_cnt = 0;
	invalid_cnt = 0;
	aggressiveness = 1;
	myStatus = false;
	currentError = 0;
}

QString WeatherSock::getData()
{
	return httpData;
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
	bool breakout = false;
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

		if (timeout_cnt > (int)(15*aggressiveness) && getIntStatus() == 3)
			resetConnection();

		if (gotDataHook == true)
		{
			errs = verifyData();
			if (errs < 0)
			{
				if (debug == true)
					cout << "MythWeather: COMMS : Received an error, resetting...\n";
				resetConnection();
			}
			if (errs >= 0)
				breakout = true;
		}
	}

	currentError = (int)(errs * 10);
	myStatus = true;
	

        return;
}

void WeatherSock::resetConnection()
{
	if (debug == true)
		cout << "MythWeather: COMMS : TIMEOUT : " << strStatus() << " --> Resetting" << endl;
        httpSock->close();
	parent->processEvents();
	usleep(100);
	parent->processEvents();
        httpData = "";
	gotDataHook = false;
        timeout_cnt = 0;
	disconnect(httpSock, 0, 0, 0);
	delete httpSock;
        makeConnection();
	
}

void WeatherSock::makeConnection()
{
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

	httpSock->connectToHost("www.msnbc.com", 80);
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
	if (debug == true)
		cout << "MythWeather: COMMS : Reading Data From Host [";

    	while (httpSock->canReadLine()) 
	{
		if (debug == true)
        		cout << ".";
        	httpData = httpData + httpSock->readLine();
    	}
	if (debug == true)
    		cout << "]\n";
}

void WeatherSock::socketConnected()
{
	QString locale = parent->getLocation();
    	QTextStream os(httpSock);
    	os << "GET /m/chnk/d/weather_d_src.asp?acid="
       	   << locale << " HTTP/1.1\n"
           << "Connection: close\n"
           << "Host: www.msnbc.com\n\n\n";
    	httpData = "";
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
