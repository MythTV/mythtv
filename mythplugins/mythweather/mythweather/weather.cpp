/*
        MythWeather
        Version 0.8
	January 8th, 2003

        By John Danner & Dustin Doris

        Note: Portions of this code taken from MythMusic

*/

#include <qlabel.h>
#include <qaccel.h>
#include <fstream>
#include <iostream>
#include <qlayout.h>
#include <qdatetime.h>
#include <qlistview.h>
#include <qtimer.h>
#include <qimage.h>
#include <qregexp.h>
#include <qapplication.h>
#include <qsqldatabase.h>
#include <qcursor.h>
#include <qstringlist.h>
#include <qheader.h>
#include <qpixmap.h>
#include <unistd.h>

#include "weather.h"
#include "weathercomms.h"

using namespace std;

#include <mythtv/mythcontext.h>

Weather::Weather(QSqlDatabase *db, QWidget *parent, const char *name)
       : MythDialog(parent, name)
{
    config = db;
    debug = false;
    validArea = true;
    convertData = false;
    gotLetter = false;
    readReadme = false;
    pastTime = false;
    firstRun = true;
    conError = false;
    inSetup = false;
    deepSetup = false;
    curLetter = 0;
    curCity = 0;
    changeTemp = false;
    changeLoc = false;
    changeAgg = false;
    noACCID = false;


    if (debug == true)
        cout << "MythWeather: Reading InstrallPrefix from context.\n";

    baseDir = gContext->GetInstallPrefix();
    if (debug == true)
        cout << "MythWeather: baseDir = " << baseDir << endl;

    QString accid = baseDir + "/share/mythtv/mythweather/accid.dat";

    accidFile.open(accid, ios::in | ios::binary);
    if (!accidFile)
    {
	noACCID = true;
	if (debug == true)
	     cout << "MythWeather: ACCID Data File Error (file missing!)" << endl;
    }
    else
    	loadAccidBreaks();

    config_Aggressiveness = (gContext->GetSetting("WeatherAggressiveLvl")).toInt();
    if (config_Aggressiveness > 15)
	config_Aggressiveness = 15;

    if (debug == true)
	cout << "MythWeather: Reading 'locale' from context.\n";
    locale = gContext->GetSetting("locale");
    config_Location = locale;

    if (locale.length() == 0)
    {
	if (debug == true)
		cout << "MythWeather: --- No locale set, entering setup\n";
	readReadme = true;
    }
    else
    {
	QString temp = findNamebyAccid(locale);
	if (debug == true)
		cout << "MythWeather: --- Locale: " << locale << endl;
    }

    if (debug == true)
	cout << "MythWeather: Reading 'SIUnits' from context.\n";
    
    QString convertFlag = gContext->GetSetting("SIUnits");
    if (convertFlag.upper() == "YES")
    {
	config_Units = 2;
	if (debug == true)
		cout << "MythWeather: --- Converting Data\n";
   	convertData = true;
    }
    else
	config_Units = 1;

    updateInterval = 30;
    nextpageInterval = 10;
    nextpageIntArrow = 20;

    setupColorScheme();

    if (debug == true)
	cout << "MythWeather: Loading Weather Types.\n";
    loadWeatherTypes();
   
    con_attempt = 0;

    if (debug == true)
	cout << "MythWeather: Setting up initial layout.\n";
    firstLayout();

    if (debug == true)
	cout << "MythWeather: Creating page frames and layouts.\n";
    page0Dia = new QFrame(this);
    page1Dia = new QFrame(this);
    page2Dia = new QFrame(this);
    page3Dia = new QFrame(this);
    page4Dia = new QFrame(this);
    page5Dia = new QFrame(this);

    page0Dia->setPaletteForegroundColor(main_fgColor);
    page1Dia->setPaletteForegroundColor(main_fgColor);
    page2Dia->setPaletteForegroundColor(main_fgColor);
    page3Dia->setPaletteForegroundColor(main_fgColor);
    page4Dia->setPaletteForegroundColor(main_fgColor);
    page5Dia->setPaletteForegroundColor(main_fgColor);
   
    page0 = new QHBoxLayout(page0Dia, 0);
    page1 = new QHBoxLayout(page1Dia, 0);
    page2 = new QHBoxLayout(page2Dia, 0);
    page3 = new QHBoxLayout(page3Dia, 0);
    page4 = new QHBoxLayout(page4Dia, 0);
    page5 = new QHBoxLayout(page5Dia, 0);

    currentPage = 0;
    setupLayout(0);
    setupLayout(1);
    setupLayout(2);
    setupLayout(3);
    setupLayout(4);
    setupLayout(5);

    if (debug == true)
	cout << "MythWeather: Creating final layout.\n";
    lastLayout();

    showLayout(0);

    if (debug == true)
	cout << "MythWeather: Setting up timers.\n";
    QTimer *showtime_Timer = new QTimer(this);
    connect(showtime_Timer, SIGNAL(timeout()), SLOT(showtime_timeout()) );
    showtime_Timer->start(1000);

    update_Timer = new QTimer(this);
    connect(update_Timer, SIGNAL(timeout()), SLOT(update_timeout()) );
    if (readReadme == false)
	update_Timer->start((int)(10));   

    nextpage_Timer = new QTimer(this);
    connect(nextpage_Timer, SIGNAL(timeout()), SLOT(nextpage_timeout()) );

    status_Timer = new QTimer(this);
    connect(status_Timer, SIGNAL(timeout()), SLOT(status_timeout()) );

    if (debug == true)
	cout << "MythWeather: Setting up accelerator keys.\n";
    accel = new QAccel(this);
    accel->connectItem(accel->insertItem(Key_Left), this, SLOT(cursorLeft()));
    accel->connectItem(accel->insertItem(Key_Right), this, SLOT(cursorRight()));
    accel->connectItem(accel->insertItem(Key_P), this, SLOT(holdPage()));
    accel->connectItem(accel->insertItem(Key_Space), this, SLOT(convertFlip()));
    accel->connectItem(accel->insertItem(Key_Enter), this, SLOT(convertFlip()));
    accel->connectItem(accel->insertItem(Key_Return), this, SLOT(convertFlip()));
    accel->connectItem(accel->insertItem(Key_M), this, SLOT(resetLocale()));
    accel->connectItem(accel->insertItem(Key_I), this, SLOT(setupPage()));
    accel->connectItem(accel->insertItem(Key_Up), this, SLOT(upKey()));
    accel->connectItem(accel->insertItem(Key_Down), this, SLOT(dnKey()));
    accel->connectItem(accel->insertItem(Key_PageUp), this, SLOT(pgupKey()));
    accel->connectItem(accel->insertItem(Key_PageDown), this, SLOT(pgdnKey()));
    accel->connectItem(accel->insertItem(Key_0), this, SLOT(newLocale0()));
    accel->connectItem(accel->insertItem(Key_1), this, SLOT(newLocale1()));
    accel->connectItem(accel->insertItem(Key_2), this, SLOT(newLocale2()));
    accel->connectItem(accel->insertItem(Key_3), this, SLOT(newLocale3()));
    accel->connectItem(accel->insertItem(Key_4), this, SLOT(newLocale4()));
    accel->connectItem(accel->insertItem(Key_5), this, SLOT(newLocale5()));
    accel->connectItem(accel->insertItem(Key_6), this, SLOT(newLocale6()));
    accel->connectItem(accel->insertItem(Key_7), this, SLOT(newLocale7()));
    accel->connectItem(accel->insertItem(Key_8), this, SLOT(newLocale8()));
    accel->connectItem(accel->insertItem(Key_9), this, SLOT(newLocale9()));

    if (debug == true)
	cout << "MythWeather: Theming widget, show(), showFullScreen(), setActiveWindow(), setFocus();\n";
    show();
    showFullScreen();
    setActiveWindow();
    setFocus();

    if (debug == true)
	cout << "MythWeather: Finish Object Initialization.\n";

    if (readReadme == true)
    {
	lastCityNum = (int)(accidBreaks[0]) - 1;
    	setupPage();
    }

}

Weather::~Weather()
{
	accidFile.close();

	delete [] aggrNum;
	delete [] letterList;
	delete [] cityList;
}

void Weather::loadAccidBreaks()
{
	for (int i = 0; i < 26; i++)
	{
		if (accidFile.eof())
		{
			noACCID = true;
			if (debug == true)
			    cout << "MythWeather: ACCID Data File Error (unexpected eof)" << endl;
		}

		accidFile >> accidBreaks[i];
		if (accidFile.eof())
		{
			i = 26;
		}
		accidFile >> accidBreaks[i + 26];
		if (accidFile.eof())
		{
                        i = 26;
		}
	}

	startData = accidFile.tellg() + (streampos)1;


}

void Weather::saveConfig()
{
	QString config_accid; 
	QString agWriter;
	QString units;

	if (changeLoc == true)
	{
	    if (cityList[4].text().stripWhiteSpace().length() != 0)
	    {
	    	config_accid = findAccidbyName(cityList[4].text().stripWhiteSpace());
	    	gContext->SetSetting("locale", config_accid);
	    	locale = config_accid;
	    	setSetting("locale", locale, false);
	    }
	}

	if (changeTemp == true)
	{

	    if (config_Units == 2)
	    {
		units = "YES";
		gContext->SetSetting("SIUnits", "YES");
		convertData = true;
	    }
	    else
	    {
		units = "NO";
		gContext->SetSetting("SIUnits", "NO");
		convertData = false;
	    }
	    setSetting("SIUnits", units, false);
    	}

 	if (changeAgg == true)
	{
	    agWriter = QString("%1").arg(config_Aggressiveness);
	    gContext->SetSetting("WeatherAggressiveLvl", agWriter);
	    setSetting("WeatherAggressiveLvl", agWriter, false);
	}

	config_accid = "";
}

void Weather::setSetting(QString value, QString data, bool global)
{
	QString thequery;

	if (global == false)
		thequery = QString("SELECT * FROM settings WHERE value=\"%1\" AND hostname=\"%2\";")
			   .arg(value).arg(gContext->GetHostName());
	else
		thequery = QString("SELECT * FROM settings WHERE value=\"%1\";")
                           .arg(value);

	QSqlQuery query = config->exec(thequery);
        int rows = query.numRowsAffected();
		
	if (rows > 0)
	{

	   if (global == false)
	        thequery = QString("UPDATE settings SET data=\"%1\" WHERE value=\"%2\" AND hostname=\"%3\";")
                        .arg(data).arg(value).arg(gContext->GetHostName());
	   else
	        thequery = QString("UPDATE settings SET data=\"%1\" WHERE value=\"%2\";")
                        .arg(data).arg(value);
	
           query = config->exec(thequery);
           rows = query.numRowsAffected();

           if (rows == -1)
           {
                cerr << "MythWeather: Error executing query!\n";
                cerr << "MythWeather: QUERY = " << thequery << endl;
                return;
           }
	}	
	else
	{
	   if (global == false)
		thequery = QString("INSERT INTO settings VALUES ('%1', '%2', '%3');")
	 		   .arg(value).arg(data).arg(gContext->GetHostName());
	   else
		thequery = QString("INSERT INTO settings VALUES ('%1', '%2');").arg(value).arg(data);

	   QSqlQuery query = config->exec(thequery);

	   rows = query.numRowsAffected();

           if (rows == -1)
           {
                cerr << "MythWeather: Error executing query!\n";
                cerr << "MythWeather: QUERY = " << thequery << endl;
                return;
           }
	}
}

QString Weather::findAccidbyName(QString name)
{
    QString accid;
    if (noACCID == false)
    {
        char temp[1024];
        char *hold;

        accidFile.seekg(startData);

        while (!accidFile.eof())
        {
                accidFile.getline(temp, 1023);

                if (strstr(temp, name) != NULL)
                {
                        hold = strtok(temp, "::");
                        hold = strtok(NULL, "::");
			accid = hold;
                        hold = strtok(NULL, "::");

			accidFile.seekg(startData);

                        return accid;
                }
        }

	accidFile.seekg(startData);
	accidFile.clear();
     }

     accid = "<NOTFOUND>";

     return name;

}

QString Weather::findNamebyAccid(QString accid)
{
    QString name;
    if (noACCID == false)
    {
	int cnt = 0;
	char temp[1024];
	char *hold;

	accidFile.seekg(startData);

	while (!accidFile.eof())
	{
		accidFile.getline(temp, 1023);
		cnt++;

		if (strstr(temp, accid) != NULL)
		{
			streampos curp;
			int rt = 0;
			
			hold = strtok(temp, "::");
                	hold = strtok(NULL, "::");
                	hold = strtok(NULL, "::");

			curp = accidFile.tellg();

			for (int i = 0; i < 26; i++)
			{
				if (curp > accidBreaks[i + 26] && curp < accidBreaks[i + 27])
				{
					curLetter = i;
					cnt = cnt - rt;
					i = 26;
				}
				else
				{
					rt = rt + accidBreaks[i]; 
				}
			}

			curCity = cnt - 1;

			name = hold;
			accidFile.seekg(startData);
			return name;
		}
	}

	accidFile.seekg(startData);
	accidFile.clear();
    }
    name = "<NOTFOUND>";

    return name;
}

void Weather::loadCityData(int dat)
{
    if (noACCID == false)
    {
	int start = 0;
	int end = 9;

	if (dat < 0)
		dat = 0;
	if (dat > lastCityNum)
		dat = lastCityNum;

	char temporary[1024];
	char *hold;
	accidFile.seekg(startData + accidBreaks[curLetter + 26], ios::beg);

	if (dat > 4) 
	{
	    for (int i = 0; i < (dat - 4); i++)
	    {
		accidFile.getline(temporary,1023);
		if (accidFile.eof())
                {
                        accidFile.seekg(-25, ios::end);
                        accidFile.clear();
                }
	    }
	}
	if (dat < 4)
	{
	    if (curLetter != 0)
	    	backupCity(4 - dat);
	}


	if (curLetter == 0 && dat < 4)
	{
		start = 4 - dat;
		for (int k = 0; k < start; k++)
		      cityNames[k] = "";
	}

	for (int j = start; j < end; j++)
	{
		accidFile.getline(temporary, 1023);
/*
		if (accidFile.eof())
                {
                        accidFile.seekg(-25, ios::end);
                        accidFile.clear();
                }
*/

                hold = strtok(temporary, "::");
                hold = strtok(NULL, "::");
                hold = strtok(NULL, "::");

	
		if (hold != NULL)
		{
		    if (strcmp(hold, "XXXXXXXXXX") == 0)
                    {
			accidFile.seekg(-25, ios::end);
			accidFile.clear();
                        for (int k = j; k < 9; k++)
                                cityNames[k] = "";

                        j = end;
                    }
		    else
		    {

		        cityNames[j] = hold;

		        if ((int)hold[0] != (curLetter + 65))
                        {
                             cityNames[j] = "";
                        }

		     }
		}
		else
		{	
			cityNames[j] = "";
		}
	}
    }

}

void Weather::updateLetters()
{
	int h;
	int cnt = 0;
	QString temp;

	for (int j = curLetter - 4; j < (curLetter + 5); j++)
        {
		if (j == curLetter)
			lastCityNum = (int)(accidBreaks[curLetter]) - 1;

                h = j;
                if (h < 0)
			h = h + 26;
		if (h > 25)
			h = h - 26;

		h = h + 65;
			
                temp = QString(" %1 ").arg((char)(h));
                letterList[cnt].setText(temp);
                cnt++;
        }

	loadCityData(0);
	showCityName();

}

void Weather::backupCity(int num)
{
	char temporary[1024];
	char temp2[1024];
	char *np;
	int prev;
	num++;

	for (int i = num; i > 0; i--)
	{
		accidFile.getline(temporary, 1023);
		strcpy(temp2, temporary);

		np = strtok(temp2, "::");

		if (np != NULL)
		{
		    prev = atol(np);

		    prev = -1 * (prev + strlen(temporary) + 1);
		}

		accidFile.seekg((long)prev, ios::cur);
	}

	accidFile.getline(temporary, 1023);


}

void Weather::showCityName()
{
	int cnt = 0;
	for (int i = 0; i < 9; i++)
	{
		cityList[i].setText("  " + cityNames[i]);
		cnt++;
	}
}

void Weather::processEvents()
{
	qApp->processEvents();
}

QString Weather::getLocation()
{
	return locale;
}

void Weather::setupColorScheme()
{

    Settings *themed = gContext->qtconfig();
    QString curColor = "";
    curColor = themed->GetSetting("time_bgColor");
    if (curColor != "")
        topbot_bgColor = QColor(curColor);

    curColor = themed->GetSetting("time_fgColor");
    if (curColor != "")
        topbot_fgColor = QColor(curColor);

    curColor = themed->GetSetting("prog_bgColor");
    if (curColor != "")
        main_bgColor = QColor(curColor);

    curColor = themed->GetSetting("prog_fgColor");
    if (curColor != "")
        main_fgColor = QColor(curColor);

    curColor = themed->GetSetting("curProg_fgColor");
    if (curColor != "")
        lohi_fgColor = QColor(curColor);
}

void Weather::loadWeatherTypes()
{
   wData = new weatherTypes[128];
   char temporary[256];
   int wCount = 0;

   ifstream weather_data(baseDir + "/share/mythtv/mythweather/weathertypes.dat", ios::in);
   if (weather_data == NULL)
   {
	cerr << "MythWeather: Error reading " << baseDir << "/share/mythtv/mythweather/weathertypes.dat...exiting...\n";
	exit(-1); 
   }

   QString tempStr;

   while (!weather_data.eof())
   {
	weather_data.getline(temporary, 1023);
	tempStr = temporary;
	if (tempStr.length() > 0)
	{

 	QStringList datas = QStringList::split(",", tempStr);

	wData[wCount].typeNum = datas[0].toInt();
	wData[wCount].typeName = datas[1];
	wData[wCount].typeIcon = baseDir + "/share/mythtv/mythweather/images/" + datas[2];

	wCount++;
	}
   }

}

void Weather::showtime_timeout()
{
   QDateTime new_time(QDate::currentDate(), QTime::currentTime());
   QString curTime = new_time.toString("ddd MMM d        h:mm:ss ap       ");
   curTime = curTime.upper();

   dtime->setText(curTime);
 
}

void Weather::newLocale0()
{
    if (inSetup == false)
    {
	newLocaleHold = newLocaleHold + "0";
	lbLocale->setText("New Location: " + newLocaleHold);
	if (newLocaleHold.length() == 5)
	{
		locale = newLocaleHold; 
		newLocaleHold = "";
		update_timeout();	
	}
    }
}

void Weather::newLocale1()
{
    if (inSetup == true && deepSetup == true && curConfig == 2 && gotLetter == true)
    {
	changeLoc = true;
        curCity = curCity - 25;
        if (curCity < 0)
               curCity = 0;

        loadCityData(curCity);
        showCityName();
    }
    else
    {
        newLocaleHold = newLocaleHold + "1";
        lbLocale->setText("New Location: " + newLocaleHold);
        if (newLocaleHold.length() == 5)
        {
                locale = newLocaleHold;
		newLocaleHold = "";
                update_timeout();
        }
    }
}
void Weather::newLocale2()
{
    if (inSetup == true && deepSetup == true && curConfig == 2 && gotLetter == true)
    {
	changeLoc = true;
        curCity = curCity - 50;
        if (curCity < 0)
               curCity = 0;

        loadCityData(curCity);
        showCityName();
    }
    else
    {
        newLocaleHold = newLocaleHold + "2";
        lbLocale->setText("New Location: " + newLocaleHold);
        if (newLocaleHold.length() == 5)
        {
                locale = newLocaleHold;
		newLocaleHold = "";
                update_timeout();
        }
    }
}
void Weather::newLocale3()
{
    if (inSetup == true && deepSetup == true && curConfig == 2 && gotLetter == true)
    {
	changeLoc = true;
        curCity = curCity - 100;
        if (curCity < 0)
               curCity = 0;

        loadCityData(curCity);
        showCityName();
    }
    else
    {
        newLocaleHold = newLocaleHold + "3";
        lbLocale->setText("New Location: " + newLocaleHold);
        if (newLocaleHold.length() == 5)
        {
                locale = newLocaleHold;
		newLocaleHold = "";
                update_timeout();
        }
    }
}
void Weather::newLocale4()
{
    if (inSetup == true && deepSetup == true && curConfig == 2 && gotLetter == true)
    {
	changeLoc = true;
        curCity = 0;

        loadCityData(curCity);
        showCityName();
    }
    else
    {
        newLocaleHold = newLocaleHold + "4";
        lbLocale->setText("New Location: " + newLocaleHold);
        if (newLocaleHold.length() == 5)
        {
                locale = newLocaleHold;
		newLocaleHold = "";
                update_timeout();
        }
    }
}
void Weather::newLocale5()
{
    if (inSetup == true && deepSetup == true && curConfig == 2 && gotLetter == true)
    {
	changeLoc = true;
        curCity = (int)(lastCityNum / 2);

        loadCityData(curCity);
        showCityName();
    }
    else
    {
        newLocaleHold = newLocaleHold + "5";
        lbLocale->setText("New Location: " + newLocaleHold);
        if (newLocaleHold.length() == 5)
        {
                locale = newLocaleHold;
		newLocaleHold = "";
                update_timeout();
        }
    }
}
void Weather::newLocale6()
{
    if (inSetup == true && deepSetup == true && curConfig == 2 && gotLetter == true)
    {
	changeLoc = true;
        curCity = lastCityNum;

        loadCityData(curCity);
        showCityName();
    }
    else
    {
        newLocaleHold = newLocaleHold + "6";
        lbLocale->setText("New Location: " + newLocaleHold);
        if (newLocaleHold.length() == 5)
        {
                locale = newLocaleHold;
		newLocaleHold = "";
                update_timeout();
        }
    }
}
void Weather::newLocale7()
{
    if (inSetup == true && deepSetup == true && curConfig == 2 && gotLetter == true)
    {
	changeLoc = true;
	curCity = curCity + 25;
	if (curCity > lastCityNum)
                curCity = lastCityNum;

        loadCityData(curCity);
        showCityName();
    }
    else
    {
        newLocaleHold = newLocaleHold + "7";
        lbLocale->setText("New Location: " + newLocaleHold);
        if (newLocaleHold.length() == 5)
        {
                locale = newLocaleHold;
		newLocaleHold = "";
                update_timeout();
        }
    }
}
void Weather::newLocale8()
{
    if (inSetup == true && deepSetup == true && curConfig == 2 && gotLetter == true)
    {
	changeLoc = true;
        curCity = curCity + 50;
	if (curCity > lastCityNum)
                curCity = lastCityNum;

        loadCityData(curCity);
        showCityName();
    }
    else
    {
        newLocaleHold = newLocaleHold + "8";
        lbLocale->setText("New Location: " + newLocaleHold);
        if (newLocaleHold.length() == 5)
        {
                locale = newLocaleHold;
		newLocaleHold = "";
                update_timeout();
        }
    }
}

void Weather::newLocale9()
{
    if (inSetup == true && deepSetup == true && curConfig == 2 && gotLetter == true)
    {
   	changeLoc = true;
        curCity = curCity + 100;
        if (curCity > lastCityNum)
                curCity = lastCityNum;

       	loadCityData(curCity);
        showCityName();
    }
    else
    {
        newLocaleHold = newLocaleHold + "9";
        lbLocale->setText("New Location: " + newLocaleHold);
        if (newLocaleHold.length() == 5)
        {
                locale = newLocaleHold;
		newLocaleHold = "";
                update_timeout();
        }
    }
}


void Weather::holdPage()
{
  if (inSetup == false)
  {
   if (nextpage_Timer->isActive() == false)
   {
 	nextpage_Timer->start((int)(1000 * nextpageInterval));
	QString txtLocale = city + ", ";
    	if (state.length() == 0)
	{
        	txtLocale += country + " (" + locale;
		if (validArea == false)
			txtLocale += " is invalid)";
		else
			txtLocale += ")";
	}
    	else
	{
        	txtLocale += state + ", " + country + " (" + locale;
                if (validArea == false)
                        txtLocale += " is invalid)";
                else
                        txtLocale += ")";
	}

	if (readReadme == true)
		txtLocale += "   No Location Set, Please read the README";

    	lbLocale->setText(txtLocale);
   }
   else
   {
	nextpage_Timer->stop();
	lbLocale->setText(lbLocale->text() + "  - PAUSED -");
   }
  }
}

void Weather::resetLocale()
{
     if (inSetup == false)
     {
	locale = gContext->GetSetting("locale");
	update_timeout();
     }
}

void Weather::setupPage()
{
    if (inSetup == false)
    {
	if (noACCID == false)
	     lbUpdated->setText("Configuring MythWeather...");
	else
	     lbUpdated->setText("Missing ACCID data file!");

	lbLocale->setText("Use the right arrow key to select unit conversion...");
	inSetup = true;
  	nextpage_Timer->stop();
	showLayout(5);
    }
    else
    {
	unitType->show();
        location->hide();
        aggressv->hide();
        lbUnits->setPaletteBackgroundColor(topbot_bgColor);
        lbLocal->setPaletteBackgroundColor(main_bgColor);
        lbAggr->setPaletteBackgroundColor(main_bgColor);

	lbLocale->setText("Configuration Changes Saved.");
	lbUpdated->setText("Updating...");
	inSetup = false;
	deepSetup = false;
	curConfig = 1;
	gotLetter = false;
	
	saveConfig();

	if (readReadme == true)
	{
		readReadme = false;
		update_Timer->start((int)(10));
		showLayout(0);
	}
	else
	{
		firstRun = true;	

		if (changeLoc == true || changeTemp == true)
		{
			lbLocale->setText("New Locale: " + cityList[4].text());
			update_Timer->changeInterval((int)(100));
		}
		else
		{
			QString txtLocale = city + ", ";
    			if (state.length() == 0)
    			{
        			txtLocale += country + " (" + locale;
                		if (validArea == false)
                        		txtLocale += " is invalid)";
                		else
                        		txtLocale += ")";
    			}
    			else
    			{
        			txtLocale += state + ", " + country + " (" + locale;
                		if (validArea == false)
                        		txtLocale += " is invalid)";
                		else
                        		txtLocale += ")";
    			}
			lbLocale->setText(txtLocale);
			lbUpdated->setText("Last Update: " + updated);
		}

		nextpage_Timer->changeInterval((int)(1000 * nextpageInterval));
		if (validArea == true)
			showLayout(1);
		else
			showLayout(0);
	}

	changeTemp = false;
	changeLoc = false;
	changeAgg = false;
    }
}

void Weather::convertFlip()
{
    if (inSetup == false)
    {
	if (convertData == false)
        {
		if (debug == true)	
			cout << "MythWeather: Converting weather data.\n";
		convertData = true;
	}
	else
 	{
		if (debug == true)
			cout << "MythWeather: Not converting weather data.\n";
		convertData = false;
	}

	update_timeout();
    }
}

void Weather::upKey()
{
   if (inSetup == true)
   {
	if (deepSetup == false)
	{
	curConfig--;
	if (curConfig == 0)
		curConfig = 3;

	switch (curConfig)
        {
                case 1:
			unitType->show();
			location->hide();
			aggressv->hide();
			lbLocale->setText("Use the right arrow key to select unit conversion...");
                        lbUnits->setPaletteBackgroundColor(topbot_bgColor);
                        lbLocal->setPaletteBackgroundColor(main_bgColor);
                        lbAggr->setPaletteBackgroundColor(main_bgColor);
                        break;
                case 2:
			unitType->hide();
                        location->show();
                        aggressv->hide();
			lbLocale->setText("Use the right arrow key to select your location...");
                        lbLocal->setPaletteBackgroundColor(topbot_bgColor);
                        lbUnits->setPaletteBackgroundColor(main_bgColor);
                        lbAggr->setPaletteBackgroundColor(main_bgColor);
                        break;
                case 3:
			unitType->hide();
                        location->hide();
                        aggressv->show();
			lbLocale->setText("Use the right arrow key to select the aggressiveness level...");
                        lbAggr->setPaletteBackgroundColor(topbot_bgColor);
                        lbUnits->setPaletteBackgroundColor(main_bgColor);
                        lbLocal->setPaletteBackgroundColor(main_bgColor);
                        break;
	}
	}
	else
	{
	    if (curConfig == 1)
	    {
		changeTemp = true;
		if (config_Units == 1)
		{
			config_Units = 2;
			SIUnits->setPaletteBackgroundColor(topbot_bgColor);
			ImpUnits->setPaletteBackgroundColor(main_bgColor);
			
		}	
		else
		{
			config_Units = 1;
			ImpUnits->setPaletteBackgroundColor(topbot_bgColor);
			SIUnits->setPaletteBackgroundColor(main_bgColor);
		}
	    }

	    if (curConfig == 2)
	    {
		if (gotLetter == false)
		{
			curLetter--;
			if (curLetter < 0)
				curLetter = 25;
	
			curCity = 0;

			updateLetters();
		}
		else
		{
    			changeLoc = true;

			if (cityList[3].text().length() > 2)
                   	{

				curCity--;
				if (curCity < 0)
					curCity = 0;

				loadCityData(curCity);
				showCityName();
			}
		}
		
	    }

 	    if (curConfig == 3)
	    {
    		changeAgg = true;
		config_Aggressiveness--;
		if (config_Aggressiveness < 1)
			config_Aggressiveness = 15 + config_Aggressiveness;
		if (config_Aggressiveness > 15)
			config_Aggressiveness = config_Aggressiveness - 15;
	
		updateAggr();
	    }
	}
   }
}

void Weather::pgupKey()
{
    if (inSetup == true && deepSetup == true && curConfig == 2 && gotLetter == true)
    {
    	changeLoc =true;
	curCity = curCity - 9;
        if (curCity < 0)
               curCity = 0;

        loadCityData(curCity);
        showCityName();
    }
}

void Weather::pgdnKey()
{
    if (inSetup == true && deepSetup == true && curConfig == 2 && gotLetter == true)
    {
    	changeLoc =true;
        curCity = curCity + 9;
	if (curCity > lastCityNum)
		curCity = lastCityNum;

        loadCityData(curCity);
        showCityName();
    }

}

void Weather::dnKey()
{
   if (inSetup == true)
   {
	if (deepSetup == false)
	{
	curConfig++;
	if (curConfig == 4)
		curConfig = 1;

	switch (curConfig)
	{
		case 1:
			unitType->show();
                        location->hide();
                        aggressv->hide();
			lbLocale->setText("Use the right arrow key to select unit conversion...");
      			lbUnits->setPaletteBackgroundColor(topbot_bgColor);
			lbLocal->setPaletteBackgroundColor(main_bgColor);
			lbAggr->setPaletteBackgroundColor(main_bgColor);
			break;
		case 2:
			unitType->hide();
                        location->show();
                        aggressv->hide();
			lbLocale->setText("Use the right arrow key to select your location...");
        		lbLocal->setPaletteBackgroundColor(topbot_bgColor);
			lbUnits->setPaletteBackgroundColor(main_bgColor);
			lbAggr->setPaletteBackgroundColor(main_bgColor);
			break;
		case 3:
			unitType->hide();
                        location->hide();
                        aggressv->show();
			lbLocale->setText("Use the right arrow key to select the aggressiveness level...");
        		lbAggr->setPaletteBackgroundColor(topbot_bgColor);
			lbUnits->setPaletteBackgroundColor(main_bgColor);
			lbLocal->setPaletteBackgroundColor(main_bgColor);
			break;
	}
	}
	else
	{
	    if (curConfig == 1)
            {
		changeTemp = true;
                if (config_Units == 1)
                {
                        config_Units = 2;
                        SIUnits->setPaletteBackgroundColor(topbot_bgColor);
                        ImpUnits->setPaletteBackgroundColor(main_bgColor);

                }
                else
                {
                        config_Units = 1;
                        ImpUnits->setPaletteBackgroundColor(topbot_bgColor);
                        SIUnits->setPaletteBackgroundColor(main_bgColor);
                }
            }
	    if (curConfig == 2)
	    {
                if (gotLetter == false)
                {
                        curLetter++;
                        if (curLetter > 25)
                                curLetter = 0;

			curCity = 0;
                        updateLetters();
                }
		else
		{
   		   changeLoc = true;

		   lastCityNum = (int)(accidBreaks[curLetter]) - 1;

		   if (cityList[5].text().length() > 2)
		   {

			curCity++;
                        if (curCity > lastCityNum)
                                curCity = lastCityNum;

			loadCityData(curCity);
                        showCityName();
 		    }
		
		}

  	    }
	    if (curConfig == 3)
	    {
    		changeAgg = true;
		config_Aggressiveness++;
                if (config_Aggressiveness < 1)
                        config_Aggressiveness = 15 + config_Aggressiveness;
                if (config_Aggressiveness > 15)
                        config_Aggressiveness = config_Aggressiveness - 15;

		updateAggr();
 	   }
	}
	
   }
}

void Weather::updateAggr()
{
	QString temp;
        int h;
        int cnt = 0;

        for (int i = config_Aggressiveness - 4; i < (config_Aggressiveness + 5); i++)
        {
                h = i;
                if (i < 1)
                        h = 15 + i;
                if (i > 15)
                        h = i - 15;

                if (h == 1)
                        temp = " 1  High Speed Connection";
                else if (h == 8)
                        temp = " 8  Medium Speed Connection";
                else if (h == 15)
                        temp = " 15 Low Speed Connection";
                else
                        temp = QString(" %1 ").arg(h);
                aggrNum[cnt].setText(temp);
                cnt++;
       	}
}

void Weather::cursorLeft()
{
   if (inSetup == false)
   {
   	nextpage_Timer->changeInterval((int)(1000 * nextpageIntArrow));
   	int tp = currentPage;
   	tp--;
	
   	if (tp == 0)
   	     tp = 4;

   	if (tp == 3 && pastTime == true)
		tp = 2;

   	if (tp == 4 && pastTime == false)
		tp = 3;

   	showLayout(tp);
   }
   else if (deepSetup == true)
   {
	if (curConfig == 1)
	{
		lbLocale->setText("Use the right arrow key to select unit conversion...");
		lbUnits->setPaletteBackgroundColor(topbot_bgColor);
		if (config_Units == 2)
			SIUnits->setPaletteBackgroundColor(QColor(0, 0, 0));
		else
                        ImpUnits->setPaletteBackgroundColor(QColor(0, 0, 0));
	
	}
	if (curConfig == 2)
	{
		if (gotLetter == false)
                {
			if (noACCID == false)
                              lbUpdated->setText("Configuring MythWeather...");

			lbLocale->setText("Use the right arrow key to select your location...");
                	lbLocal->setPaletteBackgroundColor(topbot_bgColor);
			
			letterList[4].setPaletteBackgroundColor(QColor(0, 0, 0));
			deepSetup = false;
                }
                else
                {
                        letterList[4].setPaletteBackgroundColor(topbot_bgColor);
                        cityList[4].setPaletteBackgroundColor(QColor(0, 0, 0));
			gotLetter = false;
                }
	}
	if (curConfig == 3)
	{
		lbLocale->setText("Use the right arrow key to select the aggressiveness level...");
		lbAggr->setPaletteBackgroundColor(topbot_bgColor);
		aggrNum[4].setPaletteBackgroundColor(QColor(0, 0, 0));
	}

	if (curConfig != 2)
		 deepSetup = false;
   } 
}

void Weather::cursorRight()
{
   if (inSetup == false)
   {
   	nextpage_Timer->changeInterval((int)(1000 * nextpageIntArrow));
   	int tp = currentPage;
   	tp++;
   	if (tp > 4)
        	tp = 1;

   	if (tp == 3 && pastTime == true)
   	     tp = 4;
   	if (tp == 4 && pastTime == false)
		tp = 1;

   	showLayout(tp);
   }
   else if (deepSetup == false)
   {
	if (curConfig == 1)
	{
		lbLocale->setText("Use the up and down arrow keys to select unit conversion...");
		lbUnits->setPaletteBackgroundColor(QColor(0, 0, 0));
		if (config_Units == 2)
                        SIUnits->setPaletteBackgroundColor(topbot_bgColor);
                else
                        ImpUnits->setPaletteBackgroundColor(topbot_bgColor);
	}
	if (curConfig == 2)
	{
		lbLocale->setText("Up and down arrow keys select the first letter in your city name...");
		lbLocal->setPaletteBackgroundColor(QColor(0, 0, 0));

		letterList[4].setPaletteBackgroundColor(topbot_bgColor);
                cityList[4].setPaletteBackgroundColor(QColor(0, 0, 0));
	}
	if (curConfig == 3)
	{
		lbLocale->setText("Use the up and down arrow keys to select the aggressiveness level...");
		lbAggr->setPaletteBackgroundColor(QColor(0, 0, 0));
		aggrNum[4].setPaletteBackgroundColor(topbot_bgColor);
	}

	deepSetup = true;
   }
   else if (deepSetup == true)
   {
	if (curConfig == 2)
	{
		if (noACCID == false)
                     lbUpdated->setText("More Keys: 1,7 +/- 25   2,8 +/- 50   3,9 +/- 100   4-Start  5-Mid  6-End");

		lbLocale->setText("Use the up and down arrow keys to select your city...");
		gotLetter = true;
                letterList[4].setPaletteBackgroundColor(QColor(0, 0, 0));
                cityList[4].setPaletteBackgroundColor(topbot_bgColor);
	}
   }
}

void Weather::status_timeout()
{
	int len = lbStatus->text().length();
	len++;
	
	if (len == 7)
		len = 0;

	QString holdNum = " ";

	if (len > 0)
	{
		for (int i = 0; i < (len - 1); i++)
		{
			holdNum += "-";
		}
	}

	lbStatus->setText(holdNum);

}

void Weather::nextpage_timeout()
{

   nextpage_Timer->changeInterval((int)(1000 * nextpageInterval));

   int tp = currentPage;
   tp++;
   if (tp > 4) 
	tp = 1;

   if (tp == 3 && pastTime == true)
	tp = 4;
   if (tp == 4 && pastTime == false)
	tp = 1;

   showLayout(tp);


}


void Weather::update_timeout()
{
    bool result = false;
    if (debug == true)
	cout << "MythWeather: update_timeout() : Updating....\n";

    if (firstRun == true)
    {
	if (debug == true)
		cout << "MythWeather: First run, reset timer to updateInterval.\n";
    	update_Timer->changeInterval((int)(1000 * 60 * updateInterval));
    }

    result = UpdateData();

    if (result == true)
    {

    if (pastTime == true && currentPage == 3 && inSetup == false)
	nextpage_timeout();
    if (pastTime == false && currentPage == 4 && inSetup == false)
	nextpage_timeout();

    if (firstRun == true && inSetup == false)
	nextpage_Timer->start((int)(1000 * nextpageInterval));

    QFont timeFont("Arial", (int)(18 * hmult), QFont::Bold);
    QFontMetrics fontMet(timeFont);

    lbTemp->setText(curTemp);
    if (convertData == false)
    {
	tempType->setText("oF");
   	tempType->setMaximumWidth((int)(3*fontMet.width(tempType->text())));
    }
    else
    {
	tempType->setText("oC");
	tempType->setMaximumWidth((int)(2*fontMet.width(tempType->text())));
    }

    QString txtLocale = city + ", ";
    if (state.length() == 0)
    {
	txtLocale += country + " (" + locale;
                if (validArea == false)
                        txtLocale += " is invalid)";
                else
                        txtLocale += ")";
    }
    else
    {
	txtLocale += state + ", " + country + " (" + locale;
                if (validArea == false)
                        txtLocale += " is invalid)";
                else
                        txtLocale += ")";
    }
 
    if (readReadme == true)
                txtLocale += "   No Location Set, Please read the README";

    lbLocale->setText(txtLocale);
    lbCond->setText(description);

    QString todayDesc;

    todayDesc = "Today a high of " + highTemp[0] + " and a low of ";
    todayDesc += lowTemp[0] + ". Currently there is a humidity of ";
    todayDesc += curHumid + "% and the winds are";

    if (winddir == "CALM")
         todayDesc += " calm.";
    else
    {
	if (convertData == false)
		todayDesc += " coming in at " + curWind + " mph from the " + winddir + ".";
	else 
		todayDesc += " coming in at " + curWind + " Km/h from the " + winddir + ".";
    }
  
    if (visibility.toFloat() == 999.00)
	todayDesc += " Visibility will be unlimited for today.";
    else
    {
	if (convertData == false)
	   todayDesc += " There will be a visibility of " + visibility + " miles.";
	else
           todayDesc += " There will be a visibility of " + visibility + " km.";
    }


    lbDesc->setText(todayDesc);

    QString tomDesc;
 
    tomDesc = "Forecast for " + date[0] + "...\n\n";

    tomDesc += "Tomorrow expect a high of " + highTemp[0] + " and a low of ";
    tomDesc += lowTemp[0] + ".\n\nExpected conditions: " + weatherType[0];
 
    lbTDesc->setText(tomDesc);

    lbUpdated->setText("Last Update: " + updated);

    date1->setText(date[1]);
    desc1->setText(weatherType[1]);
    high1->setText(highTemp[1]);
    low1->setText(lowTemp[1]);

    date2->setText(date[2]);
    desc2->setText(weatherType[2]);
    high2->setText(highTemp[2]);
    low2->setText(lowTemp[2]);

    date3->setText(date[3]);
    desc3->setText(weatherType[3]);
    high3->setText(highTemp[3]);
    low3->setText(lowTemp[3]);

    lbHumid->setText(curHumid + "%  ");
    if (convertData == false)
    	lbPress->setText(barometer + " in  ");
    else
	lbPress->setText(barometer + " kPa ");

    if (winddir == "CALM")
        lbWind->setText("Calm  ");
    else
    {
        if (convertData == false)
                lbWind->setText(winddir + " at " + curWind + " mph  ");
        else
                lbWind->setText(winddir + " at " + curWind + " Km/h  ");
    }

    if (visibility.toFloat() != 999.00)
    {
	if (convertData == false)
            lbVisi->setText(visibility + " mi  ");
	else
	    lbVisi->setText(visibility + " km  ");
    }
    else
        lbVisi->setText("Unlimited  ");

    if (convertData == false)
    	lbWindC->setText(curFeel + " F   ");
    else
	lbWindC->setText(curFeel + " C   ");

    if (uvIndex.toInt() < 3)
         lbUVIndex->setText(uvIndex + " (minimal)  ");
    else if (uvIndex.toInt() < 6)
         lbUVIndex->setText(uvIndex + " (moderate)  ");
    else if (uvIndex.toInt() < 8)
         lbUVIndex->setText(uvIndex + " (high)  ");
    else if (uvIndex.toInt() >= 8)
         lbUVIndex->setText(uvIndex + " (extreme)  ");

    QImage *tempimageA = new QImage();
    QPixmap pic1;
    if (tempimageA->load(weatherIcon[1]))
    {
    QImage tmp2a;
    tmp2a = tempimageA->smoothScale((int)(200 * wmult),
                                 (int)(150 * hmult));
    pic1.convertFromImage(tmp2a);
    }
    delete tempimageA;

    QImage *tempimageB = new QImage();
    QPixmap pic2;
    if (tempimageB->load(weatherIcon[2]))
    {
    QImage tmp2b;
    tmp2b = tempimageB->smoothScale((int)(200 * wmult),
                                 (int)(150 * hmult));
    pic2.convertFromImage(tmp2b);
    }
    delete tempimageB;

    QImage *tempimageC = new QImage();
    QPixmap pic3;
    if (tempimageC->load(weatherIcon[3]))
    {
    QImage tmp2c;
    tmp2c = tempimageC->smoothScale((int)(200 * wmult),
                                 (int)(150 * hmult));
    pic3.convertFromImage(tmp2c);
    }
    delete tempimageC;

    QImage *tempimageD = new QImage();
    QPixmap pic4;
    if (tempimageD->load(curIcon))
    {
    QImage tmp2d;
    tmp2d = tempimageD->smoothScale((int)(200 * wmult),
                                 (int)(150 * hmult));
    pic4.convertFromImage(tmp2d);
    }
    delete tempimageD;

    QImage *tempimageE = new QImage();
    QPixmap pic5;
    if (tempimageE->load(weatherIcon[0]))
    {
    QImage tmp2e;
    tmp2e = tempimageE->smoothScale((int)(200 * wmult),
                                 (int)(150 * hmult));
    pic5.convertFromImage(tmp2e);
    }
    delete tempimageE;

    lbPic1->setPixmap(pic1);
    lbPic2->setPixmap(pic2);
    lbPic3->setPixmap(pic3);
    lbPic4->setPixmap(pic4);
    lbPic5->setPixmap(pic5);

    if (firstRun == true)
    {
    	firstRun = false;
	if (inSetup == false)
		nextpage_timeout();
    }

    }
    

}

void Weather::showLayout(int pageNum)
{
   QString pageDesc;
   if (pageNum == 0)
	hdPart1->setText("please wait...");
   if (pageNum == 1)
	hdPart1->setText("current conditions");
   if (pageNum == 2)
      	hdPart1->setText("extended forecast");
   if (pageNum == 3)
	hdPart1->setText("today's forecast");
   if (pageNum == 4)
	hdPart1->setText("tomorrow's forecast");
   if (pageNum == 5)
	hdPart1->setText("weather setup");

   switch (currentPage)
   {
	case 0:	page0Dia->hide();
		break;
	case 1:	page1Dia->hide();
		break;
	case 2: page2Dia->hide();
		break;
	case 3: page3Dia->hide();
		break;	
	case 4: page4Dia->hide();
		break;
 	case 5: page5Dia->hide();
		break;
   }

   currentPage = pageNum;

   switch (currentPage)
   {
	case 0:
		page0Dia->show();
		break;
	case 1: 
		page1Dia->show();
                break;
        case 2: 
		page2Dia->show();
                break;
        case 3: 
		page3Dia->show();
                break;
        case 4:
		page4Dia->show();
                break;
	case 5:
		page5Dia->show();
		break;
	default: 
		 page1Dia->show();
   }

}

void Weather::firstLayout()
{

   QFont lohiFont("Arial", (int)(16 * hmult), QFont::Bold);
   QFontMetrics lohiFM(lohiFont);

   QFont timeFont("Arial", (int)(18 * hmult), QFont::Bold);
   QFontMetrics fontMet(timeFont);

   QFont headFont("Arial", (int)(24 * hmult), QFont::Bold);
   QFontMetrics headMet(headFont);

   main = new QVBoxLayout(this, (int)(25 * hmult), (int)(10 * hmult));
   main->setResizeMode(QLayout::Minimum);
   //main->addStrut((int)(600*hmult));

   QHBoxLayout *top = new QHBoxLayout(0, 0, 0);
   mid = new QVBoxLayout(0, 0, 0);

   main->addLayout(top, 0);
   main->addLayout(mid, 0);
   //main->addLayout(bot, 0);

   //top->addStrut((int)(125*hmult));
   //mid->addStrut((int)(425*hmult));
   //bot->addStrut((int)(50*hmult));
 
   QImage *tempimage = new QImage();
   QPixmap picLogo;
   if (tempimage->load(baseDir + "/share/mythtv/mythweather/images/logo.png"))
   {
   QImage tmp2;
   tmp2 = tempimage->smoothScale((int)(157 * wmult),
                                 (int)(117 * hmult));
   picLogo.convertFromImage(tmp2);
   }
   delete tempimage;

   QLabel *logo = new QLabel(" ", this);
   logo->setMaximumWidth((int)(157*wmult));
   logo->setMinimumWidth((int)(157*wmult));
   logo->setMaximumHeight((int)(117*hmult));
   logo->setMinimumHeight((int)(117*hmult));
   logo->setBackgroundOrigin(WindowOrigin);
   logo->setPixmap(picLogo);

   top->addWidget(logo, 0, 0);

   QVBoxLayout *mainTop = new QVBoxLayout(0, (int)(6 * wmult), (int)(6 * wmult));
   QHBoxLayout *labels = new QHBoxLayout(0, (int)(6 * wmult), (int)(6 * wmult));

   hdPart1 = new QLabel("current conditions", this);
   hdPart1->setFont(headFont);
   hdPart1->setBackgroundOrigin(WindowOrigin);
   hdPart1->setMinimumHeight((int)(90*hmult));
   hdPart1->setMaximumWidth((int)(625*wmult));

   QDateTime new_time(QDate::currentDate(), QTime::currentTime());
   QString curTime = new_time.toString("ddd MMM d        h:mm:ss ap       ");
   curTime = curTime.upper();
   dtime = new QLabel(curTime, this);
   dtime->setAlignment(Qt::AlignRight | Qt::AlignTop);
   dtime->setFont(lohiFont);
   dtime->setBackgroundOrigin(WindowOrigin);

   labels->addWidget(hdPart1, 0, 0);

   mainTop->addLayout(labels, 0);
   mainTop->addWidget(dtime, 0, 0);

   top->addLayout(mainTop, 0);

   QHBoxLayout *topLine = new QHBoxLayout(0, 0, 0);
 
   lbLocale = new QLabel(" ", this);
   lbStatus = new QLabel(" ", this);

   topLine->addWidget(lbLocale, 0, 0);
   topLine->addWidget(lbStatus, 0, 0);

   QString txtLocale = "Location: " + locale;

   if (readReadme == true)
       txtLocale = "No Location Set, Please read the README";

   lbLocale->setText(txtLocale);

   lbLocale->setBackgroundOrigin(WindowOrigin);
   lbLocale->setAlignment( Qt::AlignVCenter);
   lbLocale->setPaletteBackgroundColor(topbot_bgColor);
   lbLocale->setPaletteForegroundColor(topbot_fgColor);
   lbLocale->setFont(lohiFont);
   lbLocale->setMinimumWidth((int)(700*wmult));
   lbLocale->setMaximumWidth((int)(700*wmult));
   lbLocale->setMinimumHeight((int)(1.5*lohiFM.height()*hmult));
   lbLocale->setMaximumHeight((int)(1.5*lohiFM.height()*hmult));
   lbLocale->setFrameStyle( QFrame::Panel | QFrame::Raised );

   lbStatus->setAlignment( Qt::AlignHCenter | Qt::AlignVCenter);
   lbStatus->setPaletteBackgroundColor(topbot_bgColor);
   lbStatus->setPaletteForegroundColor(topbot_fgColor);
   lbStatus->setFont(lohiFont);
   lbStatus->setMinimumWidth((int)(50*wmult));
   lbStatus->setMaximumWidth((int)(50*wmult));
   lbStatus->setMinimumHeight((int)(1.5*lohiFM.height()*hmult));
   lbStatus->setMaximumHeight((int)(1.5*lohiFM.height()*hmult));
   lbStatus->setFrameStyle( QFrame::Panel | QFrame::Raised );

   mid->addLayout(topLine, 0);

} 

void Weather::lastLayout()
{
   QFont lohiFont("Arial", (int)(16 * hmult), QFont::Bold);
   QFontMetrics lohiFM(lohiFont);

   lbUpdated = new QLabel("Updating ... ", this);
   lbUpdated->setAlignment( Qt::AlignVCenter);
   lbUpdated->setPaletteBackgroundColor(topbot_bgColor);
   lbUpdated->setPaletteForegroundColor(topbot_fgColor);
   lbUpdated->setFont(lohiFont);
   lbUpdated->setMinimumWidth((int)(750*wmult));
   lbUpdated->setMaximumWidth((int)(750*wmult));
   lbUpdated->setMinimumHeight((int)(1.5*lohiFM.height()*hmult));
   lbUpdated->setMaximumHeight((int)(1.5*lohiFM.height()*hmult));
   lbUpdated->setFrameStyle( QFrame::Panel | QFrame::Raised );

   mid->addWidget(lbUpdated, 0, 0);

}

void Weather::setupLayout(int pageNum)
{
   if (debug == true)
	cout << "MythWeather: setupLayout() : Setting up layout for page #" << pageNum << endl;

   QFont lohiFont("Arial", (int)(16 * hmult), QFont::Bold);
   QFontMetrics lohiFM(lohiFont);

   QFont timeFont("Arial", (int)(18 * hmult), QFont::Bold);
   QFontMetrics fontMet(timeFont);

   QFont headFont("Arial", (int)(24 * hmult), QFont::Bold);
   QFontMetrics headMet(headFont);

   QFont bigtFont("Arial", (int)(48 * hmult), QFont::Bold);
   QFontMetrics bigtMet(bigtFont);

   if (pageNum == 0)
   {
	page0Dia->setMaximumWidth((int)(750*wmult));
	hdPart1->setText("current conditions");
   	QHBoxLayout *ext0  = new QHBoxLayout(0, 0, 0);

   	ext0->addStrut((int)(310*hmult));

   	page0->addLayout(ext0, 0);

 	hdPart1->setText("please wait...");

  	QImage *tempimage0 = new QImage();
   	QPixmap pic0;
   	if (tempimage0->load(baseDir + "/share/mythtv/mythweather/images/mwmain.png"))
   	{
   	QImage tmp20;
   	tmp20 = tempimage0->smoothScale((int)(750 * wmult),
   	                              (int)(296 * hmult));
   	pic0.convertFromImage(tmp20);
   	}
   	delete tempimage0;

   	lbPic0 = new QLabel(" ", page0Dia);
   	lbPic0->setPaletteBackgroundColor(main_bgColor);
   	lbPic0->setPixmap(pic0);
   	lbPic0->setFrameStyle( QFrame::Panel | QFrame::Raised );

	ext0->addWidget(lbPic0, 0);

   }
 
   if (pageNum == 1)
   {
	page1Dia->setMaximumWidth((int)(750*wmult));
   hdPart1->setText("current conditions");
   QHBoxLayout *ext1  = new QHBoxLayout(0, 0, 0);

   ext1->addStrut((int)(310*hmult));

   page1->addLayout(ext1, 0);

   QVBoxLayout *lbls = new QVBoxLayout(0, 0, 0);
   QVBoxLayout *dats = new QVBoxLayout(0, 0, 0);
   QVBoxLayout *pics = new QVBoxLayout(0, 0, 0);

   ext1->addLayout(lbls, 0);
   ext1->addLayout(dats, 0);
   ext1->addLayout(pics, 0);

/*

	pressure
	wind
	visi
	wind chill
	uv index
    QLabel *lbHumid;
    QLabel *lbPress;
    QLabel *lbWind;
    QLabel *lbVisi;
    QLabel *lbWindC;
    QLabel *lbUVIndex;

*/
   lbHumid = new QLabel(curHumid + "%  ", page1Dia);
   lbHumid->setPaletteBackgroundColor(main_bgColor);
   lbHumid->setAlignment( Qt::AlignRight | Qt::AlignVCenter );
   lbHumid->setFont(timeFont);
   if (convertData == false)
	lbPress = new QLabel(barometer + " in  ", page1Dia);
    else
	lbPress = new QLabel(barometer + " kPa ", page1Dia);
   lbPress = new QLabel(barometer + "   ", page1Dia);
   lbPress->setPaletteBackgroundColor(main_bgColor);
   lbPress->setAlignment( Qt::AlignRight | Qt::AlignVCenter );
   lbPress->setFont(timeFont);

   if (winddir == "CALM")
	lbWind = new QLabel("Calm  ", page1Dia);
   else
   {
       if (convertData == false)
		lbWind = new QLabel(winddir + " at " + curWind + " mph  ", page1Dia);
       else
		lbWind = new QLabel(winddir + " at " + curWind + " Km/h  ", page1Dia);
   }

   lbWind->setPaletteBackgroundColor(main_bgColor);
   lbWind->setAlignment( Qt::AlignRight | Qt::AlignVCenter );
   lbWind->setFont(timeFont);
   if (visibility.toFloat() != 999.00)
   {
	if (convertData == false)
   	 	lbVisi = new QLabel(visibility + " mi  ", page1Dia);
	else
		lbVisi = new QLabel(visibility + " km  ", page1Dia);
   }
   else
	lbVisi = new QLabel("Unlimited  ", page1Dia);
   lbVisi->setPaletteBackgroundColor(main_bgColor);
   lbVisi->setAlignment( Qt::AlignRight | Qt::AlignVCenter );
   lbVisi->setFont(timeFont);
   if (convertData == false)
   	lbWindC = new QLabel(curFeel + " F   ", page1Dia);
   else
	lbWindC = new QLabel(curFeel + " C   ", page1Dia);
   
   lbWindC->setPaletteBackgroundColor(main_bgColor);
   lbWindC->setAlignment( Qt::AlignRight  | Qt::AlignVCenter );
   lbWindC->setFont(timeFont);
   if (uvIndex.toInt() < 3)
   	lbUVIndex = new QLabel(uvIndex + " (minimal)  ", page1Dia);
   else if (uvIndex.toInt() < 6)
	lbUVIndex = new QLabel(uvIndex + " (moderate)  ", page1Dia);
   else if (uvIndex.toInt() < 8)
	lbUVIndex = new QLabel(uvIndex + " (high)  ", page1Dia);
   else if (uvIndex.toInt() >= 8)
	lbUVIndex = new QLabel(uvIndex + " (extreme)  ", page1Dia);
   lbUVIndex->setPaletteBackgroundColor(main_bgColor);
   lbUVIndex->setAlignment( Qt::AlignRight  | Qt::AlignVCenter );
   lbUVIndex->setFont(timeFont);

   QLabel *lbHum = new QLabel("  Humidity", page1Dia);
   lbHum->setPaletteBackgroundColor(main_bgColor);
   lbHum->setFont(timeFont);
   QLabel *lbPrs = new QLabel("  Pressure", page1Dia);
   lbPrs->setPaletteBackgroundColor(main_bgColor);
   lbPrs->setFont(timeFont);
   QLabel *lbWnd = new QLabel("  Wind", page1Dia);
   lbWnd->setPaletteBackgroundColor(main_bgColor);
   lbWnd->setFont(timeFont);
   QLabel *lbVis = new QLabel("  Visibility", page1Dia);
   lbVis->setPaletteBackgroundColor(main_bgColor);
   lbVis->setFont(timeFont);
   QLabel *lbWCh = new QLabel("  Wind Chill", page1Dia);
   lbWCh->setPaletteBackgroundColor(main_bgColor);
   lbWCh->setFont(timeFont);
   QLabel *lbUVI = new QLabel("  UV Index", page1Dia);
   lbUVI->setPaletteBackgroundColor(main_bgColor);
   lbUVI->setFont(timeFont);

   lbls->addWidget(lbHum, 0, 0);
   lbls->addWidget(lbPrs, 0, 0);
   lbls->addWidget(lbWnd, 0, 0);
   lbls->addWidget(lbVis, 0, 0);
   lbls->addWidget(lbWCh, 0, 0);
   lbls->addWidget(lbUVI, 0, 0);
   dats->addWidget(lbHumid, 0, 0);
   dats->addWidget(lbPress, 0, 0);
   dats->addWidget(lbWind, 0, 0);
   dats->addWidget(lbVisi, 0, 0);
   dats->addWidget(lbWindC, 0, 0);
   dats->addWidget(lbUVIndex, 0, 0);


   lbCond = new QLabel(description, page1Dia);
   lbCond->setPaletteBackgroundColor(main_bgColor);  
   lbCond->setFont(timeFont);
   lbCond->setMaximumHeight((int)(2*fontMet.height()));
   lbCond->setMaximumWidth((int)250);
   lbCond->setAlignment( Qt::AlignVCenter | Qt::AlignHCenter );

   QHBoxLayout *tHolder = new QHBoxLayout(0, 0, 0);
   QVBoxLayout *spacer1 = new QVBoxLayout(0, 0, 0);

   QLabel *space1 = new QLabel("  ", page1Dia);
   space1->setMaximumHeight(fontMet.height());
   space1->setMinimumHeight(fontMet.height());
   space1->setPaletteBackgroundColor(main_bgColor);

   lbTemp = new QLabel(curTemp, page1Dia);
   lbTemp->setPaletteBackgroundColor(main_bgColor);
   lbTemp->setFont(bigtFont);
   lbTemp->setAlignment( Qt::AlignVCenter | Qt::AlignRight );

   if (convertData == false)
   	tempType = new QLabel("oF",page1Dia);
   else
	tempType = new QLabel("oC", page1Dia);

   tempType->setMaximumWidth((int)(2*fontMet.width(tempType->text())));
   tempType->setPaletteBackgroundColor(main_bgColor);
   tempType->setFont(timeFont);
   tempType->setAlignment(Qt::AlignLeft);

   spacer1->addWidget(space1, 0, 0);
   spacer1->addWidget(tempType, 0, 0);

   tHolder->addWidget(lbTemp, 0, 0);
   tHolder->addLayout(spacer1, 0);

   QHBoxLayout *picHold4 = new QHBoxLayout(0, 0, 0);

   QLabel *spc4a = new QLabel(" ", page1Dia);
   QLabel *spc4b = new QLabel(" ", page1Dia);
   spc4a->setPaletteBackgroundColor(main_bgColor);
   spc4b->setPaletteBackgroundColor(main_bgColor);
   spc4a->setMaximumWidth((int)(wmult*25));
   spc4b->setMaximumWidth((int)(wmult*25));

   QImage *tempimageD = new QImage();
   QPixmap pic4;
   if (tempimageD->load(curIcon))
   {
   QImage tmp2d;
   tmp2d = tempimageD->smoothScale((int)(200 * wmult),
                                 (int)(150 * hmult));
   pic4.convertFromImage(tmp2d);
   }
   delete tempimageD;

   lbPic4 = new QLabel(" ", page1Dia);
   lbPic4->setPaletteBackgroundColor(main_bgColor);
   lbPic4->setMaximumWidth((int)(200*wmult));
   lbPic4->setMaximumHeight((int)(150*hmult));
   lbPic4->setMinimumHeight((int)(150*hmult));
   lbPic4->setPixmap(pic4);

   picHold4->addWidget(spc4a, 0, 0);
   picHold4->addWidget(lbPic4, 0, 0);
   picHold4->addWidget(spc4b, 0, 0);

   pics->addLayout(picHold4, 0);
   pics->addWidget(lbCond, 0, 0);
   pics->addLayout(tHolder, 0);
}

if (pageNum == 2)
{
   page2Dia->setMaximumWidth((int)(750*wmult));
   hdPart1->setText("extended forecast");
   QHBoxLayout *ext2  = new QHBoxLayout(0, 0, 0);

   ext2->addStrut((int)(310*wmult));

   page2->addLayout(ext2, 0);

   QVBoxLayout *day1 = new QVBoxLayout(0, 0, 0);
   QVBoxLayout *day2 = new QVBoxLayout(0, 0, 0);
   QVBoxLayout *day3 = new QVBoxLayout(0, 0, 0);

   ext2->addLayout(day1, 0);
   ext2->addLayout(day2, 0);
   ext2->addLayout(day3, 0);

   date1 = new QLabel(date[1], page2Dia);
   date1->setBackgroundOrigin(WindowOrigin);
   date1->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
   date1->setMaximumWidth((int)(250*wmult));
   date1->setPaletteBackgroundColor(main_bgColor);
   date1->setMaximumHeight((int)(2*fontMet.height()));
   date1->setFont(timeFont);

   desc1 = new QLabel(weatherType[1], page2Dia);
   desc1->setBackgroundOrigin(WindowOrigin);
   desc1->setAlignment(Qt::WordBreak | Qt::AlignHCenter | Qt::AlignVCenter);
   desc1->setMaximumWidth((int)(250*wmult));
   desc1->setPaletteBackgroundColor(main_bgColor);
   //desc1->setMaximumHeight((int)(2*fontMet.height()));
   desc1->setFont(timeFont);

   date2 = new QLabel(date[2], page2Dia);
   date2->setBackgroundOrigin(WindowOrigin);
   date2->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
   date2->setMaximumWidth((int)(250*wmult));
   date2->setPaletteBackgroundColor(main_bgColor);
   date2->setMaximumHeight((int)(2*fontMet.height()));
   date2->setFont(timeFont);

   desc2 = new QLabel(weatherType[2], page2Dia);
   desc2->setBackgroundOrigin(WindowOrigin);
   desc2->setAlignment(Qt::WordBreak | Qt::AlignHCenter | Qt::AlignVCenter);
   desc2->setMaximumWidth((int)(250*wmult));
   desc2->setPaletteBackgroundColor(main_bgColor);
   //desc2->setMaximumHeight((int)(2*fontMet.height()));
   desc2->setFont(timeFont);

   date3 = new QLabel(date[3], page2Dia);
   date3->setBackgroundOrigin(WindowOrigin);
   date3->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
   date3->setMaximumWidth((int)(250*wmult));
   date3->setPaletteBackgroundColor(main_bgColor);
   date3->setMaximumHeight((int)(2*fontMet.height()));
   date3->setFont(timeFont);

   desc3 = new QLabel(weatherType[3], page2Dia);
   desc3->setBackgroundOrigin(WindowOrigin);
   desc3->setAlignment(Qt::WordBreak | Qt::AlignHCenter | Qt::AlignVCenter);
   desc3->setMaximumWidth((int)(250*wmult));
   desc3->setPaletteBackgroundColor(main_bgColor);
   //desc3->setMaximumHeight((int)(2*fontMet.height()));
   desc3->setFont(timeFont);

   high1 = new QLabel(highTemp[1], page2Dia);
   high1->setBackgroundOrigin(WindowOrigin);
   high1->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
   high1->setMaximumWidth((int)(250*wmult));
   high1->setPaletteBackgroundColor(main_bgColor);
   high1->setMaximumHeight(fontMet.height());
   high1->setFont(headFont);

   high2 = new QLabel(highTemp[2], page2Dia);
   high2->setBackgroundOrigin(WindowOrigin);
   high2->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
   high2->setMaximumWidth((int)(250*wmult));
   high2->setPaletteBackgroundColor(main_bgColor);
   high2->setMaximumHeight(fontMet.height());
   high2->setFont(headFont);

   high3 = new QLabel(highTemp[3], page2Dia);
   high3->setBackgroundOrigin(WindowOrigin);
   high3->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
   high3->setMaximumWidth((int)(250*wmult));
   high3->setPaletteBackgroundColor(main_bgColor);
   high3->setMaximumHeight(fontMet.height());
   high3->setFont(headFont);

   low1 = new QLabel(lowTemp[1], page2Dia);
   low1->setBackgroundOrigin(WindowOrigin);
   low1->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
   low1->setMaximumWidth((int)(250*wmult));
   low1->setPaletteBackgroundColor(main_bgColor);
   low1->setMaximumHeight(fontMet.height());
   low1->setFont(headFont);

   low2 = new QLabel(lowTemp[2], page2Dia);
   low2->setBackgroundOrigin(WindowOrigin);
   low2->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
   low2->setMaximumWidth((int)(250*wmult));
   low2->setPaletteBackgroundColor(main_bgColor);
   low2->setMaximumHeight(fontMet.height());
   low2->setFont(headFont);

   low3 = new QLabel(lowTemp[3], page2Dia);
   low3->setBackgroundOrigin(WindowOrigin);
   low3->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
   low3->setMaximumWidth((int)(250*wmult));
   low3->setPaletteBackgroundColor(main_bgColor);
   low3->setMaximumHeight(fontMet.height());
   low3->setFont(headFont);
  
   QHBoxLayout *picHold1 = new QHBoxLayout(0, 0, 0);

   QLabel *spc1a = new QLabel(" ", page2Dia);
   QLabel *spc1b = new QLabel(" ", page2Dia);
   spc1a->setPaletteBackgroundColor(main_bgColor);
   spc1b->setPaletteBackgroundColor(main_bgColor);
   spc1a->setMaximumWidth((int)(25 * wmult));
   spc1b->setMaximumWidth((int)(25 * wmult));

   QImage *tempimageA = new QImage();
   QPixmap pic1;
   if (tempimageA->load(weatherIcon[1]))
   {
   QImage tmp2a;
   tmp2a = tempimageA->smoothScale((int)(200 * wmult),
                                 (int)(150 * hmult));
   pic1.convertFromImage(tmp2a);
   }
   delete tempimageA;

   lbPic1 = new QLabel(" ", page2Dia);
   lbPic1->setPaletteBackgroundColor(main_bgColor);
   lbPic1->setMaximumWidth((int)(200*wmult));
   lbPic1->setMaximumHeight((int)(150*hmult));
   lbPic1->setMinimumHeight((int)(150*hmult));
   lbPic1->setPixmap(pic1);

   picHold1->addWidget(spc1a, 0, 0);
   picHold1->addWidget(lbPic1, 0, 0);
   picHold1->addWidget(spc1b, 0, 0);

   QHBoxLayout *picHold2 = new QHBoxLayout(0, 0, 0);

   QLabel *spc2a = new QLabel(" ", page2Dia);
   QLabel *spc2b = new QLabel(" ", page2Dia);
   spc2a->setPaletteBackgroundColor(main_bgColor);
   spc2b->setPaletteBackgroundColor(main_bgColor);
   spc2a->setMaximumWidth((int)(25 * wmult));
   spc2b->setMaximumWidth((int)(25 * wmult));

   QImage *tempimageB = new QImage();
   QPixmap pic2;
   if (tempimageB->load(weatherIcon[2]))
   {
   QImage tmp2b;
   tmp2b = tempimageB->smoothScale((int)(200 * wmult),
                                   (int)(150 * hmult));
   pic1.convertFromImage(tmp2b);
   }
   delete tempimageB;

   lbPic2 = new QLabel(" ", page2Dia);
   lbPic2->setPaletteBackgroundColor(main_bgColor);
   lbPic2->setMaximumWidth((int)(200*wmult));
   lbPic2->setMaximumHeight((int)(150*hmult));
   lbPic2->setMinimumHeight((int)(150*hmult));
   lbPic2->setPixmap(pic2);

   picHold2->addWidget(spc2a, 0, 0);
   picHold2->addWidget(lbPic2, 0, 0);
   picHold2->addWidget(spc2b, 0, 0);

   QHBoxLayout *picHold3 = new QHBoxLayout(0, 0, 0);

   QLabel *spc3a = new QLabel(" ", page2Dia);
   QLabel *spc3b = new QLabel(" ", page2Dia);
   spc3a->setPaletteBackgroundColor(main_bgColor);
   spc3b->setPaletteBackgroundColor(main_bgColor);
   spc3a->setMaximumWidth((int)(25 * wmult));
   spc3b->setMaximumWidth((int)(25 * wmult));

   QImage *tempimageC = new QImage();
   QPixmap pic3;
   if (tempimageC->load(weatherIcon[3]))
   {
   QImage tmp2c;
   tmp2c = tempimageC->smoothScale((int)(200 * wmult),
                                 (int)(150 * hmult));
   pic3.convertFromImage(tmp2c);
   }
   delete tempimageC;

   lbPic3 = new QLabel(" ", page2Dia);
   lbPic3->setPaletteBackgroundColor(main_bgColor);
   lbPic3->setMaximumWidth((int)(200*wmult));
   lbPic3->setMaximumHeight((int)(150*hmult));
   lbPic3->setMinimumHeight((int)(150*hmult));
   lbPic3->setPixmap(pic3);

   picHold3->addWidget(spc3a, 0, 0);
   picHold3->addWidget(lbPic3, 0, 0);
   picHold3->addWidget(spc3b, 0, 0);

   QHBoxLayout *loHi1 = new QHBoxLayout(0, 0, 0);
   QHBoxLayout *loHi2 = new QHBoxLayout(0, 0, 0);
   QHBoxLayout *loHi3 = new QHBoxLayout(0, 0, 0);

   QLabel *lbLow1 =  new QLabel("LO", page2Dia);
   lbLow1->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
   lbLow1->setPaletteForegroundColor(lohi_fgColor);
   lbLow1->setMaximumHeight(lohiFM.height());
   lbLow1->setPaletteBackgroundColor(main_bgColor);
   lbLow1->setFont(lohiFont);
   QLabel *lbLow2 =  new QLabel("LO", page2Dia);
   lbLow2->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
   lbLow2->setPaletteForegroundColor(lohi_fgColor);
   lbLow2->setMaximumHeight(lohiFM.height());
   lbLow2->setPaletteBackgroundColor(main_bgColor);
   lbLow2->setFont(lohiFont);
   QLabel *lbLow3 =  new QLabel("LO", page2Dia);
   lbLow3->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
   lbLow3->setPaletteForegroundColor(lohi_fgColor);
   lbLow3->setMaximumHeight(lohiFM.height());
   lbLow3->setPaletteBackgroundColor(main_bgColor);
   lbLow3->setFont(lohiFont);
   QLabel *lbHi1 = new QLabel("HI", page2Dia);
   lbHi1->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
   lbHi1->setPaletteForegroundColor(lohi_fgColor);
   lbHi1->setMaximumHeight(lohiFM.height());
   lbHi1->setPaletteBackgroundColor(main_bgColor);
   lbHi1->setFont(lohiFont);
   QLabel *lbHi2 = new QLabel("HI", page2Dia);
   lbHi2->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
   lbHi2->setPaletteForegroundColor(lohi_fgColor);
   lbHi2->setMaximumHeight(lohiFM.height());
   lbHi2->setPaletteBackgroundColor(main_bgColor);
   lbHi2->setFont(lohiFont);
   QLabel *lbHi3 = new QLabel("HI", page2Dia);
   lbHi3->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
   lbHi3->setPaletteForegroundColor(lohi_fgColor);
   lbHi3->setMaximumHeight(lohiFM.height());
   lbHi3->setPaletteBackgroundColor(main_bgColor);
   lbHi3->setFont(lohiFont);

   loHi1->addWidget(lbLow1, 0, 0);
   loHi1->addWidget(lbHi1, 0, 0);

   loHi2->addWidget(lbLow2, 0, 0);
   loHi2->addWidget(lbHi2, 0, 0);
   
   loHi3->addWidget(lbLow3, 0, 0);
   loHi3->addWidget(lbHi3, 0, 0);
  
   QHBoxLayout *actLoHi1 = new QHBoxLayout(0, 0, 0);
   QHBoxLayout *actLoHi2 = new QHBoxLayout(0, 0, 0);
   QHBoxLayout *actLoHi3 = new QHBoxLayout(0, 0, 0);

   actLoHi1->addWidget(low1, 0, 0); 
   actLoHi1->addWidget(high1, 0, 0);
   actLoHi2->addWidget(low2, 0, 0);
   actLoHi2->addWidget(high2, 0, 0);
   actLoHi3->addWidget(low3, 0, 0);
   actLoHi3->addWidget(high3, 0, 0);

   day1->addWidget(date1, 0, 0);
   day1->addLayout(picHold1, 0);
   day1->addWidget(desc1, 0, 0);
   day1->addLayout(loHi1, 0);
   day1->addLayout(actLoHi1, 0);

   day2->addWidget(date2, 0, 0);
   day2->addLayout(picHold2, 0);
   day2->addWidget(desc2, 0, 0);
   day2->addLayout(loHi2, 0);
   day2->addLayout(actLoHi2, 0);

   day3->addWidget(date3, 0, 0);
   day3->addLayout(picHold3, 0);
   day3->addWidget(desc3, 0, 0);
   day3->addLayout(loHi3, 0);
   day3->addLayout(actLoHi3, 0);

}

if (pageNum == 3)
{
   page3Dia->setMaximumWidth((int)(750*wmult));
   QHBoxLayout *ext3  = new QHBoxLayout(0, 0, 0);

   ext3->addStrut((int)(310*wmult));

   page3->addLayout(ext3, 0);

   hdPart1->setText("today's forecast");
   QString todayDesc;

  todayDesc = "Today a high of " + highTemp[0] + " and a low of ";
  todayDesc += lowTemp[0] + ". Currently there is a humidity of ";
  todayDesc += curHumid + "% and the winds are";
  if (winddir == "CALM")
        todayDesc += " calm.";
   else
   {
       if (convertData == false)
               todayDesc += " coming in at " + curWind + " mph from the " + winddir + ".";
       else
               todayDesc += " coming in at " + curWind + " Km/h from the " + winddir + ".";
   }

 if (visibility.toFloat() == 999.00)
       todayDesc += " Visibility will be unlimited for today.";
  else
  {
	if (convertData == false)
       	    todayDesc += " There will be a visibility of " + visibility + " miles.";
	else
	    todayDesc += " There will be a visibility of " + visibility + " kilometers.";
   }

   lbDesc = new QLabel(todayDesc, page3Dia);
   lbDesc->setAlignment( Qt::AlignTop | Qt::AlignLeft | Qt::WordBreak);
   lbDesc->setPaletteBackgroundColor(main_bgColor);
   lbDesc->setFont(headFont);
   lbDesc->setMinimumWidth((int)(750*wmult));
   lbDesc->setMaximumWidth((int)(750*wmult));
   lbDesc->setFrameStyle( QFrame::Panel | QFrame::Raised );

   ext3->addWidget(lbDesc, 0);
}

if (pageNum == 4)
{
   page4Dia->setMaximumWidth((int)(750*wmult));
   QHBoxLayout *ext4  = new QHBoxLayout(0, 0, 0);

   ext4->addStrut((int)(310*wmult));

   page4->addLayout(ext4, 0);

   hdPart1->setText("tomorrow's forecast");

   QString tomDesc;

   tomDesc = "Forecast for " + date[0] + "...\n\n";
   tomDesc += "Tomorrow expect a high of " + highTemp[0] + " and a low of ";
   tomDesc += lowTemp[0] + ".\n\nExpected conditions: " + weatherType[0];

   QImage *tempimageE = new QImage();
   QPixmap pic5;
   if (tempimageE->load(weatherIcon[0]))
   {
   QImage tmp2e;
   tmp2e = tempimageE->smoothScale((int)(200 * wmult),
                                 (int)(150 * hmult));
   pic5.convertFromImage(tmp2e);
   }
   delete tempimageE;

   lbPic5 = new QLabel(" ", page4Dia);
   lbPic5->setPaletteBackgroundColor(main_bgColor);
   lbPic5->setPixmap(pic5);
   lbPic5->setFrameStyle( QFrame::Panel | QFrame::Raised );


   lbTDesc = new QLabel(tomDesc, page4Dia);
   lbTDesc->setAlignment( Qt::AlignTop | Qt::AlignLeft | Qt::WordBreak);
   lbTDesc->setPaletteBackgroundColor(main_bgColor);
   lbTDesc->setFont(headFont);
   lbTDesc->setMinimumWidth((int)(550*wmult));
   lbTDesc->setMaximumWidth((int)(750*wmult));
   lbTDesc->setFrameStyle( QFrame::Panel | QFrame::Raised );

   ext4->addWidget(lbPic5, 0);
   ext4->addWidget(lbTDesc, 0);
}
   if (pageNum == 5)
   {
	curConfig = 1;

 	page5Dia->setMaximumWidth((int)(750*wmult));
	page5Dia->setPaletteBackgroundColor(main_bgColor);
	page5Dia->setFrameStyle( QFrame::Panel | QFrame::Raised );
   	QHBoxLayout *ext5  = new QHBoxLayout(0, 0, 0);
	QVBoxLayout *ext5v = new QVBoxLayout(0, 10, 10);
	QLabel *spc = new QLabel("", page5Dia);

	spc->setMinimumWidth((int)2);
	spc->setMaximumWidth((int)2);
	spc->setPaletteBackgroundColor(topbot_bgColor);

   	ext5->addStrut((int)(310*wmult));

   	page5->addLayout(ext5, 0);

	lbUnits = new QLabel(" Temperature Units ", page5Dia);
	lbUnits->setPaletteBackgroundColor(topbot_bgColor);
	lbUnits->setAlignment(Qt::AlignCenter);
	lbUnits->setMaximumWidth((int)250);
	lbUnits->setMinimumWidth((int)250);
	lbUnits->setMaximumHeight((int)(2*fontMet.height()));
	lbUnits->setFrameStyle( QFrame::Panel | QFrame::Raised );
	lbLocal = new QLabel("Location", page5Dia);
	lbLocal->setAlignment(Qt::AlignCenter);
	lbLocal->setMaximumWidth((int)250);
	lbLocal->setMinimumWidth((int)250);
	lbLocal->setMaximumHeight((int)(2*fontMet.height()));
	lbLocal->setFrameStyle( QFrame::Panel | QFrame::Raised );
	lbAggr = new QLabel("Aggressiveness", page5Dia);
	lbAggr->setAlignment(Qt::AlignCenter);
	lbAggr->setMaximumWidth((int)250);
	lbAggr->setMinimumWidth((int)250);
	lbAggr->setMaximumHeight((int)(2*fontMet.height()));
	lbAggr->setFrameStyle( QFrame::Panel | QFrame::Raised );

	ext5v->addWidget(lbUnits, 0);
	ext5v->addWidget(lbLocal, 0);
	ext5v->addWidget(lbAggr, 0);

	ext5->addLayout(ext5v, 0);

	unitType = new QFrame(page5Dia);
    	location = new QFrame(page5Dia);
   	aggressv = new QFrame(page5Dia);
	unitType->setFrameStyle( QFrame::Panel | QFrame::Raised );
	location->setFrameStyle( QFrame::Panel | QFrame::Raised );
	aggressv->setFrameStyle( QFrame::Panel | QFrame::Raised );
	unitType->setPaletteBackgroundColor(main_bgColor);
	location->setPaletteBackgroundColor(main_bgColor);
	aggressv->setPaletteBackgroundColor(main_bgColor);
	unitType->setMaximumWidth((int)(498*wmult));
	unitType->setMinimumWidth((int)(498*wmult));
	location->setMaximumWidth((int)(498*wmult));
	location->setMinimumWidth((int)(498*wmult));
	aggressv->setMaximumWidth((int)(498*wmult));
	aggressv->setMinimumWidth((int)(498*wmult));

	unitType->hide();
	aggressv->hide();

	ImpUnits = new QLabel(" 1. Imperial Units (Fahrenheit, MPH)", unitType);
	SIUnits = new QLabel(" 2. SI Units (Celsius, KPH)         ", unitType);
	ImpUnits->setMaximumHeight((int)(2*fontMet.height()));
	SIUnits->setMaximumHeight((int)(2*fontMet.height()));
	ImpUnits->setFrameStyle( QFrame::Panel | QFrame::Raised );
	SIUnits->setFrameStyle( QFrame::Panel | QFrame::Raised );

	ImpUnits->setMaximumWidth((int)(398*wmult));
	ImpUnits->setMinimumWidth((int)(398*wmult));
	if (config_Units == 1)
		ImpUnits->setPaletteBackgroundColor(QColor(0, 0, 0));
	else
		SIUnits->setPaletteBackgroundColor(QColor(0, 0, 0));
	SIUnits->setMaximumWidth((int)(398*wmult));
	SIUnits->setMinimumWidth((int)(398*wmult));

	QVBoxLayout *unitBox = new QVBoxLayout(unitType, 10, 10);
	QVBoxLayout *aggrBox = new QVBoxLayout(aggressv, 0, 0);
	QHBoxLayout *locBox = new QHBoxLayout(location, 0, 0);
	QVBoxLayout *letBox = new QVBoxLayout(0, 0, 0);
	QVBoxLayout *cityBox = new QVBoxLayout(0, 0, 0);

	locBox->addLayout(letBox, 0);
	locBox->addLayout(cityBox, 0);

	unitBox->addWidget(ImpUnits, 0);
	unitBox->addWidget(SIUnits, 0);

	letterList = new QLabel[9](" ", location);
	cityList = new QLabel[9](" ", location);
	QString temp;
        int h;
        int cnt = 0;

	letterList[4].setPaletteBackgroundColor(QColor(0, 0, 0));
	cityList[4].setPaletteBackgroundColor(QColor(0, 0, 0));
	letterList[4].setFrameStyle( QFrame::Panel | QFrame::Raised );
	cityList[4].setFrameStyle( QFrame::Panel | QFrame::Raised );

	for (int j = curLetter - 4; j < curLetter + 5; j++)
        {
		h = j;
		if (h < 0)
			h = h + 91;
		else
			h = h + 65;


		temp = QString(" %1 ").arg((char)(h));
		letterList[cnt].setText(temp);
		letterList[cnt].setMaximumHeight((int)(1.5*fontMet.height()));
		letterList[cnt].setMaximumWidth((int)(50*wmult));
		letterList[cnt].setMinimumWidth((int)(50*wmult));
		letterList[cnt].setAlignment( Qt::AlignHCenter | Qt::AlignVCenter);
		letBox->addWidget(&letterList[cnt], 0, 0);
		temp = "some test city!";
		cityList[cnt].setText(temp);
                cityList[cnt].setMaximumHeight((int)(1.5*fontMet.height()));
                cityBox->addWidget(&cityList[cnt], 0, 0);
		cnt++;
	}

	loadCityData(curCity);
	showCityName();

	aggrNum = new QLabel[9](" ", aggressv);
	cnt = 0;

	aggrNum[4].setPaletteBackgroundColor(QColor(0, 0, 0));
        aggrNum[4].setFrameStyle( QFrame::Panel | QFrame::Raised );

	for (int i = config_Aggressiveness - 4; i < (config_Aggressiveness + 5); i++)
	{
		h = i;
		if (i < 1)
			h = 15 + i;
		if (i > 15)
			h = i - 15;

		if (h == 1)
			temp = " 1  High Speed Connection";
		else if (h == 8)
			temp = " 8  Medium Speed Connection";
		else if (h == 15)
			temp = " 15 Low Speed Connection";
		else 
			temp = QString(" %1 ").arg(h);
		aggrNum[cnt].setText(temp);
		aggrNum[cnt].setMaximumHeight((int)(1.5*fontMet.height()));
		aggrBox->addWidget(&aggrNum[cnt], 0, 0);
		cnt++;
	}
	


	unitType->show();
	ext5->addWidget(spc, 0);
	ext5->addWidget(unitType, 0, 0);
	ext5->addWidget(location, 0, 0);
	ext5->addWidget(aggressv, 0, 0);

   }

   switch (pageNum)
   {

	case 0:
		page0Dia->hide();
		mid->addWidget(page0Dia, 0, 0);
		break;
	case 1:
		page1Dia->hide();
		mid->addWidget(page1Dia, 0, 0);
		break;
	case 2:
		page2Dia->hide();
    		mid->addWidget(page2Dia, 0, 0);
                break;
	case 3:
		page3Dia->hide();
	   	mid->addWidget(page3Dia, 0, 0);
                break;
	case 4:
		page4Dia->hide();
 		mid->addWidget(page4Dia, 0, 0);
                break;
	case 5:
		page5Dia->hide();
		mid->addWidget(page5Dia, 0, 0);
		break;
 
   }

}

bool Weather::UpdateData()
{
	lbUpdated->setText("Updating...");

	bool result = false;
	accel->setEnabled(false);
	if (debug == true)
	    cout << "MythWeather: COMMS : GetWeatherData() ...\n";
	result = GetWeatherData();
	accel->setEnabled(true);

	if (result == true)
	{

	updated = GetString("this.swLastUp");

	city = GetString("this.swCity");
	state = GetString("this.swSubDiv");
	country = GetString("this.swCountry");
	curTemp = GetString("this.swTemp");
	if (curTemp.length() == 0)
	{
		curTemp = "-na-";
		updated = updated + " (Not All Information Available)";
	}
	else
	{
	    if (convertData == true)
	    {
		char tempHold[32];
		double tTemp = curTemp.toDouble();
		double nTemp = (double)(5.0/9.0)*(tTemp - 32.0);
		sprintf (tempHold, "%d", (int)nTemp);
		curTemp = tempHold;
	    }
	}

	curIcon = GetString("this.swCIcon");
	curWind = GetString("this.swWindS");
	if (convertData == true)
        {
                char tempHold[32];
                double tTemp = curWind.toDouble();
                double nTemp = (double)(tTemp*1.6);
                sprintf (tempHold, "%d", (int)nTemp);
                curWind = tempHold;
        }

	winddir = GetString("this.swWindD");

	barometer = GetString("this.swBaro");
	if (convertData == true)
        {
                char tempHold[32];
                double tTemp = barometer.toDouble();
                double nTemp = (double)(tTemp*3.386);
                sprintf (tempHold, "%.1f", nTemp);
                barometer = tempHold;
        }
	curHumid = GetString("this.swHumid");
	curFeel = GetString("this.swReal");
	if (convertData == true)
        {
                char tempHold[32];
                double tTemp = curFeel.toDouble();
                double nTemp = (5.0/9.0)*(tTemp - 32.0);
                sprintf (tempHold, "%d", (int)nTemp);
                curFeel = tempHold;
        }
	uvIndex = GetString("this.swUV");
	visibility = GetString("this.swVis");
	if (convertData == true && visibility.toDouble() != 999.0)
        {
                char tempHold[32];
                double tTemp = visibility.toDouble();
                double nTemp = (double)(tTemp*1.6);
                sprintf (tempHold, "%.1f", nTemp);
                visibility = tempHold;
        }
	description = GetString("this.swConText");
   	if (description.length() == 0)
		description = curIcon;
	
	QString forecastData;
	forecastData = GetString("this.swFore");

  	QStringList holdings = QStringList::split("|", forecastData);
  
 	int years, mons, days;

	int dayNum = (holdings[0]).toInt();
	QDate curDay(QDate::currentDate());
	int curDayNum = curDay.dayOfWeek();
	if (curDayNum == 7)
		curDayNum = 1;
	else
		curDayNum++;

	if (curDayNum != dayNum)
		pastTime = true;
	else
		pastTime = false;

	for (int i = 5; i < 9; i++)
        {
  		QStringList dates = QStringList::split("/", holdings[i]);
		years = dates[2].toInt();
		mons = dates[0].toInt();
		days = dates[1].toInt();
   		QDate doDate(years, mons, days);
		date[i - 5] = (doDate.toString("ddd")).upper();
        }

	for (int i = 9; i < 13; i++)
		weatherIcon[i - 9] = holdings[i];

	for (int i = 20; i < 24; i++)
	{
		if (convertData == true)
        	{
                	char tempHold[32];
                	double tTemp = holdings[i].toDouble();
                	double nTemp = (double)(5.0/9.0)*(tTemp - 32.0);
                	sprintf (tempHold, "%d", (int)nTemp);
                	holdings[i] = tempHold;
        	}
		highTemp[i - 20] = holdings[i];
	}

	for (int i = 40; i < 44; i++)
	{	
		if (convertData == true)
                {
                        char tempHold[32];
                        double tTemp = holdings[i].toDouble();
                        double nTemp = (double)(5.0/9.0)*(tTemp - 32.0);
                        sprintf (tempHold, "%d", (int)nTemp);
                        holdings[i] = tempHold;
                }
		lowTemp[i - 40] = holdings[i];
	}

	for (int i = 15; i < 19; i++)
		weatherType[i - 15] = holdings[i];

	setWeatherTypeIcon(weatherType);
	setWeatherIcon(description);

	return true;

	}
/*
	for (int i = 23; i < 39; i++)
		cout << i << " ---- " << holdings[i] << endl;
*/
	return false;

}

void Weather::setWeatherTypeIcon(QString wt[5])
{
	bool isSet = false;
        int start = 1;
	if (pastTime == true)
		start = 0;

	for (int i = start; i < 5; i++)
	{
		isSet = false;
		for (int j = 0; j < 128; j++)
        	{
                	if (wt[i].toInt() == wData[j].typeNum)
                	{
				wt[i] = wData[j].typeName;
                	        weatherIcon[i] = wData[j].typeIcon;
				isSet = true;
				j = 128;
                	}
        	}

		if (isSet == false)
		{
			wt[i] = "Unknown [" + wt[i] + "]";
                        weatherIcon[i] = baseDir + "/share/mythtv/mythweather/images/unknown.png";
		}
	}
}

void Weather::setWeatherIcon(QString txtType)
{
	for (int j = 0; j < 128; j++)
	{
		if (txtType.replace(QRegExp("  "),  "" ) == 
		    (wData[j].typeName).replace(QRegExp("  "),  "" ))
		{
			curIcon = wData[j].typeIcon;
			return;
		}
		if (txtType.toInt() == wData[j].typeNum)
		{
			curIcon = wData[j].typeIcon;
			description = wData[j].typeName;
                        return;
		}
	}
	
	curIcon = baseDir + "/share/mythtv/mythweather/images/unknown.png";
	
}

QString Weather::GetString(QString tag)
{
	QString data;
        int start = 0;
        int len = 0;

        start = httpData.find(tag, 0);
	len = httpData.find("\"", start + tag.length() + 4) - start - tag.length() - 4;
	
	data = httpData.mid(start + tag.length() + 4, len);

return data;

}

int Weather::GetInt(QString tag)
{
	QString data;
        int start = 0;
        int len = 0;

        start = httpData.find(tag, 0);
        len = httpData.find("\"", start + tag.length() + 4) - start - tag.length() - 4;
        data = httpData.mid(start + tag.length() + 4, len);
	int ret = data.toInt();

return ret;
}

float Weather::GetFloat(QString tag)
{
	QString data;
        int start = 0;
        int len = 0;

        start = httpData.find(tag, 0);
        len = httpData.find("\"", start + tag.length() + 4) - start - tag.length() - 4;
        data = httpData.mid(start + tag.length() + 4, len);
        float ret = data.toFloat();

return ret;

}

bool Weather::GetWeatherData()
{
	int tAgg = (gContext->GetSetting("WeatherAggressiveLvl")).toInt();

	if (debug == true)
		cout << "MythWeather: Setting status timer and data hook.\n";

	status_Timer->start(100);
	WeatherSock *internetData = new WeatherSock(this, debug, tAgg);
	internetData->startConnect();
	
	while (internetData->getStatus() == false)
        {
                qApp->processEvents();
                usleep(100);
        }

	lbStatus->setText(" ");
	status_Timer->stop();

	if (internetData->checkError() == 10)
	{
		lbUpdated->setText("!!! 3 Failed Attempted !!! Waiting 5 minutes and trying again.");
		update_Timer->changeInterval((int)(1000 * 60 * 5));
		cout << "MythWeather: Invalid Area or Fatal Error.\n";
		delete internetData;
		return false;
	}
	else if (internetData->checkError() == 20)
	{
		lbUpdated->setText("*** Invalid Area ID Entered *** Please select a valid area id.");
		lbLocale->setText(locale + " is invalid");
                update_Timer->stop();
                cout << "MythWeather: Invalid Area ID.\n";
                delete internetData;
                return false;
	}
	else if (internetData->checkError() == 30)
	{
		lbUpdated->setText("!!! Timeout Limit Reached !!! Change your aggressiveness level.");
                update_Timer->changeInterval((int)(1000 * 60 * 5));
                cout << "MythWeather: Timeout error, change agressiveness variable.\n";
                delete internetData;
                return false;
	}
	else
		httpData = internetData->getData();

	delete internetData;

	return true;
}

