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

using namespace std;

#include <mythtv/mythcontext.h>

Weather::Weather(MythContext *context, 
                         QWidget *parent, const char *name)
           : MythDialog(context, parent, name)
{
    m_context = context;
    debug = false;
    validArea = true;
    convertData = false;
    readReadme = false;
    pastTime = false;
    firstRun = true;
    conError = false;
    if (debug == true)
	cout << "MythWeather: Reading 'locale' from context.\n";
    locale = context->GetSetting("locale");
    if (locale.length() == 0)
    {
	if (debug == true)
		cout << "MythWeather: --- No locale set, using the Bahamas\n";
	readReadme = true;
	locale = "BFXX0005"; // Show the weather in the Bahamas for
			     // those who don't read the docs. heh.
    }
    else
    {
	if (debug == true)
		cout << "MythWeather: --- Locale: " << locale << endl;
    }

    if (debug == true)
	cout << "MythWeather: Reading 'SIUnits' from context.\n";
    
    QString convertFlag = context->GetSetting("SIUnits");
    if (convertFlag.upper() == "YES")
    {
	if (debug == true)
		cout << "MythWeather: --- Converting Data\n";
   	convertData = true;
    }


    if (debug == true)
	cout << "MythWeather: Reading InstrallPrefix from context.\n";
    baseDir = context->GetInstallPrefix();
    if (debug == true)
	cout << "MythWeather: baseDir = " << baseDir << endl;

    updateInterval = 30;
    nextpageInterval = 10;
    nextpageIntArrow = 20;

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

    page0Dia->setPaletteForegroundColor(QColor(255, 255, 255));
    page1Dia->setPaletteForegroundColor(QColor(255, 255, 255));
    page2Dia->setPaletteForegroundColor(QColor(255, 255, 255));
    page3Dia->setPaletteForegroundColor(QColor(255, 255, 255));
    page4Dia->setPaletteForegroundColor(QColor(255, 255, 255));
   
    page0 = new QHBoxLayout(page0Dia, 0);
    page1 = new QHBoxLayout(page1Dia, 0);
    page2 = new QHBoxLayout(page2Dia, 0);
    page3 = new QHBoxLayout(page3Dia, 0);
    page4 = new QHBoxLayout(page4Dia, 0);

    currentPage = 0;
    setupLayout(0);
    setupLayout(1);
    setupLayout(2);
    setupLayout(3);
    setupLayout(4);

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
    update_Timer->start((int)(200));   

    nextpage_Timer = new QTimer(this);
    connect(nextpage_Timer, SIGNAL(timeout()), SLOT(nextpage_timeout()) );

    status_Timer = new QTimer(this);
    connect(status_Timer, SIGNAL(timeout()), SLOT(status_timeout()) );

    if (debug == true)
	cout << "MythWeather: Setting up accelerator keys.\n";
    accel = new QAccel(this);
    accel->connectItem(accel->insertItem(Key_Left), this, SLOT(cursorLeft()));
    accel->connectItem(accel->insertItem(Key_Right), this, SLOT(cursorRight()));
    accel->connectItem(accel->insertItem(Key_Space), this, SLOT(holdPage()));
    accel->connectItem(accel->insertItem(Key_Enter), this, SLOT(convertFlip()));
    accel->connectItem(accel->insertItem(Key_Return), this, SLOT(convertFlip()));
    accel->connectItem(accel->insertItem(Key_M), this, SLOT(resetLocale()));
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
    m_context->ThemeWidget(this);
    show();
    showFullScreen();
    setActiveWindow();
    setFocus();

    if (debug == true)
	cout << "MythWeather: Finish Object Initialization.\n";

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
	newLocaleHold = newLocaleHold + "0";
	lbLocale->setText("New Location: " + newLocaleHold);
	if (newLocaleHold.length() == 5)
	{
		locale = newLocaleHold; 
		newLocaleHold = "";
		update_timeout();	
	}
}

void Weather::newLocale1()
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
void Weather::newLocale2()
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
void Weather::newLocale3()
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
void Weather::newLocale4()
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
void Weather::newLocale5()
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
void Weather::newLocale6()
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
void Weather::newLocale7()
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
void Weather::newLocale8()
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

void Weather::newLocale9()
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


void Weather::holdPage()
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

void Weather::resetLocale()
{
	locale = m_context->GetSetting("locale");
	update_timeout();
}

void Weather::convertFlip()
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

void Weather::cursorLeft()
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

void Weather::cursorRight()
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
    if (debug == true)
	cout << "MythWeather: update_timeout() : Updating....\n";

    if (firstRun == true)
    {
	if (debug == true)
		cout << "MythWeather: First run, reset timer to updateInterval.\n";
    	update_Timer->changeInterval((int)(1000 * 60 * updateInterval));
    }

    UpdateData();

    if (pastTime == true && currentPage == 3)
	nextpage_timeout();
    if (pastTime == false && currentPage == 4)
	nextpage_timeout();

    if (firstRun == true)
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
	nextpage_timeout();
    }

}

void Weather::showLayout(int pageNum)
{
   if (debug == true)
	cout << "MythWeather: showLayout() : Showing Layout #" << pageNum << endl;


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
   lbLocale->setPaletteBackgroundColor(QColor(20, 40, 255));
   lbLocale->setPaletteForegroundColor(QColor(240, 200, 0));
   lbLocale->setFont(lohiFont);
   lbLocale->setMinimumWidth((int)(700*wmult));
   lbLocale->setMaximumWidth((int)(700*wmult));
   lbLocale->setMinimumHeight((int)(1.5*lohiFM.height()*hmult));
   lbLocale->setMaximumHeight((int)(1.5*lohiFM.height()*hmult));
   lbLocale->setFrameStyle( QFrame::Panel | QFrame::Raised );

   lbStatus->setAlignment( Qt::AlignHCenter | Qt::AlignVCenter);
   lbStatus->setPaletteBackgroundColor(QColor(20, 40, 255));
   lbStatus->setPaletteForegroundColor(QColor(240, 200, 0));
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
   lbUpdated->setPaletteBackgroundColor(QColor(20, 40, 255));
   lbUpdated->setPaletteForegroundColor(QColor(240, 200, 0));
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
   	lbPic0->setPaletteBackgroundColor(QColor(20, 40, 255));
   	lbPic0->setPixmap(pic0);
   	lbPic0->setFrameStyle( QFrame::Panel | QFrame::Raised );

	ext0->addWidget(lbPic0, 0);

   }
 
   if (pageNum == 1)
   {
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
   lbHumid->setPaletteBackgroundColor(QColor(20, 40, 255));
   lbHumid->setAlignment( Qt::AlignRight | Qt::AlignVCenter );
   lbHumid->setFont(timeFont);
   if (convertData == false)
	lbPress = new QLabel(barometer + " in  ", page1Dia);
    else
	lbPress = new QLabel(barometer + " kPa ", page1Dia);
   lbPress = new QLabel(barometer + "   ", page1Dia);
   lbPress->setPaletteBackgroundColor(QColor(20, 40, 255));
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

   lbWind->setPaletteBackgroundColor(QColor(20, 40, 255));
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
   lbVisi->setPaletteBackgroundColor(QColor(20, 40, 255));
   lbVisi->setAlignment( Qt::AlignRight | Qt::AlignVCenter );
   lbVisi->setFont(timeFont);
   if (convertData == false)
   	lbWindC = new QLabel(curFeel + " F   ", page1Dia);
   else
	lbWindC = new QLabel(curFeel + " C   ", page1Dia);
   
   lbWindC->setPaletteBackgroundColor(QColor(20, 40, 255));
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
   lbUVIndex->setPaletteBackgroundColor(QColor(20, 40, 255));
   lbUVIndex->setAlignment( Qt::AlignRight  | Qt::AlignVCenter );
   lbUVIndex->setFont(timeFont);

   QLabel *lbHum = new QLabel("  Humidity", page1Dia);
   lbHum->setPaletteBackgroundColor(QColor(20, 40, 255));
   lbHum->setFont(timeFont);
   QLabel *lbPrs = new QLabel("  Pressure", page1Dia);
   lbPrs->setPaletteBackgroundColor(QColor(20, 40, 255));
   lbPrs->setFont(timeFont);
   QLabel *lbWnd = new QLabel("  Wind", page1Dia);
   lbWnd->setPaletteBackgroundColor(QColor(20, 40, 255));
   lbWnd->setFont(timeFont);
   QLabel *lbVis = new QLabel("  Visibility", page1Dia);
   lbVis->setPaletteBackgroundColor(QColor(20, 40, 255));
   lbVis->setFont(timeFont);
   QLabel *lbWCh = new QLabel("  Wind Chill", page1Dia);
   lbWCh->setPaletteBackgroundColor(QColor(20, 40, 255));
   lbWCh->setFont(timeFont);
   QLabel *lbUVI = new QLabel("  UV Index", page1Dia);
   lbUVI->setPaletteBackgroundColor(QColor(20, 40, 255));
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
   lbCond->setPaletteBackgroundColor(QColor(20, 40, 255));  
   lbCond->setFont(timeFont);
   lbCond->setMaximumHeight((int)(2*fontMet.height()));
   lbCond->setAlignment( Qt::AlignVCenter | Qt::AlignHCenter );

   QHBoxLayout *tHolder = new QHBoxLayout(0, 0, 0);
   QVBoxLayout *spacer1 = new QVBoxLayout(0, 0, 0);

   QLabel *space1 = new QLabel("  ", page1Dia);
   space1->setMaximumHeight(fontMet.height());
   space1->setMinimumHeight(fontMet.height());
   space1->setPaletteBackgroundColor(QColor(20, 40, 255));

   lbTemp = new QLabel(curTemp, page1Dia);
   lbTemp->setPaletteBackgroundColor(QColor(20, 40, 255));
   lbTemp->setFont(bigtFont);
   lbTemp->setAlignment( Qt::AlignVCenter | Qt::AlignRight );

   if (convertData == false)
   	tempType = new QLabel("oF",page1Dia);
   else
	tempType = new QLabel("oC", page1Dia);

   tempType->setMaximumWidth((int)(2*fontMet.width(tempType->text())));
   tempType->setPaletteBackgroundColor(QColor(20, 40, 255));
   tempType->setFont(timeFont);
   tempType->setAlignment(Qt::AlignLeft);

   spacer1->addWidget(space1, 0, 0);
   spacer1->addWidget(tempType, 0, 0);

   tHolder->addWidget(lbTemp, 0, 0);
   tHolder->addLayout(spacer1, 0);

   QHBoxLayout *picHold4 = new QHBoxLayout(0, 0, 0);

   QLabel *spc4a = new QLabel(" ", page1Dia);
   QLabel *spc4b = new QLabel(" ", page1Dia);
   spc4a->setPaletteBackgroundColor(QColor(20, 40, 255));
   spc4b->setPaletteBackgroundColor(QColor(20, 40, 255));
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
   lbPic4->setPaletteBackgroundColor(QColor(20, 40, 255));
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
   date1->setPaletteBackgroundColor(QColor(20, 40, 255));
   date1->setMaximumHeight((int)(2*fontMet.height()));
   date1->setFont(timeFont);

   desc1 = new QLabel(weatherType[1], page2Dia);
   desc1->setBackgroundOrigin(WindowOrigin);
   desc1->setAlignment(Qt::WordBreak | Qt::AlignHCenter | Qt::AlignVCenter);
   desc1->setMaximumWidth((int)(250*wmult));
   desc1->setPaletteBackgroundColor(QColor(20, 40, 255));
   //desc1->setMaximumHeight((int)(2*fontMet.height()));
   desc1->setFont(timeFont);

   date2 = new QLabel(date[2], page2Dia);
   date2->setBackgroundOrigin(WindowOrigin);
   date2->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
   date2->setMaximumWidth((int)(250*wmult));
   date2->setPaletteBackgroundColor(QColor(20, 40, 255));
   date2->setMaximumHeight((int)(2*fontMet.height()));
   date2->setFont(timeFont);

   desc2 = new QLabel(weatherType[2], page2Dia);
   desc2->setBackgroundOrigin(WindowOrigin);
   desc2->setAlignment(Qt::WordBreak | Qt::AlignHCenter | Qt::AlignVCenter);
   desc2->setMaximumWidth((int)(250*wmult));
   desc2->setPaletteBackgroundColor(QColor(20, 40, 255));
   //desc2->setMaximumHeight((int)(2*fontMet.height()));
   desc2->setFont(timeFont);

   date3 = new QLabel(date[3], page2Dia);
   date3->setBackgroundOrigin(WindowOrigin);
   date3->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
   date3->setMaximumWidth((int)(250*wmult));
   date3->setPaletteBackgroundColor(QColor(20, 40, 255));
   date3->setMaximumHeight((int)(2*fontMet.height()));
   date3->setFont(timeFont);

   desc3 = new QLabel(weatherType[3], page2Dia);
   desc3->setBackgroundOrigin(WindowOrigin);
   desc3->setAlignment(Qt::WordBreak | Qt::AlignHCenter | Qt::AlignVCenter);
   desc3->setMaximumWidth((int)(250*wmult));
   desc3->setPaletteBackgroundColor(QColor(20, 40, 255));
   //desc3->setMaximumHeight((int)(2*fontMet.height()));
   desc3->setFont(timeFont);

   high1 = new QLabel(highTemp[1], page2Dia);
   high1->setBackgroundOrigin(WindowOrigin);
   high1->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
   high1->setMaximumWidth((int)(250*wmult));
   high1->setPaletteBackgroundColor(QColor(20, 40, 255));
   high1->setMaximumHeight(fontMet.height());
   high1->setFont(headFont);

   high2 = new QLabel(highTemp[2], page2Dia);
   high2->setBackgroundOrigin(WindowOrigin);
   high2->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
   high2->setMaximumWidth((int)(250*wmult));
   high2->setPaletteBackgroundColor(QColor(20, 40, 255));
   high2->setMaximumHeight(fontMet.height());
   high2->setFont(headFont);

   high3 = new QLabel(highTemp[3], page2Dia);
   high3->setBackgroundOrigin(WindowOrigin);
   high3->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
   high3->setMaximumWidth((int)(250*wmult));
   high3->setPaletteBackgroundColor(QColor(20, 40, 255));
   high3->setMaximumHeight(fontMet.height());
   high3->setFont(headFont);

   low1 = new QLabel(lowTemp[1], page2Dia);
   low1->setBackgroundOrigin(WindowOrigin);
   low1->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
   low1->setMaximumWidth((int)(250*wmult));
   low1->setPaletteBackgroundColor(QColor(20, 40, 255));
   low1->setMaximumHeight(fontMet.height());
   low1->setFont(headFont);

   low2 = new QLabel(lowTemp[2], page2Dia);
   low2->setBackgroundOrigin(WindowOrigin);
   low2->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
   low2->setMaximumWidth((int)(250*wmult));
   low2->setPaletteBackgroundColor(QColor(20, 40, 255));
   low2->setMaximumHeight(fontMet.height());
   low2->setFont(headFont);

   low3 = new QLabel(lowTemp[3], page2Dia);
   low3->setBackgroundOrigin(WindowOrigin);
   low3->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
   low3->setMaximumWidth((int)(250*wmult));
   low3->setPaletteBackgroundColor(QColor(20, 40, 255));
   low3->setMaximumHeight(fontMet.height());
   low3->setFont(headFont);
  
   QHBoxLayout *picHold1 = new QHBoxLayout(0, 0, 0);

   QLabel *spc1a = new QLabel(" ", page2Dia);
   QLabel *spc1b = new QLabel(" ", page2Dia);
   spc1a->setPaletteBackgroundColor(QColor(20, 40, 255));
   spc1b->setPaletteBackgroundColor(QColor(20, 40, 255));
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
   lbPic1->setPaletteBackgroundColor(QColor(20, 40, 255));
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
   spc2a->setPaletteBackgroundColor(QColor(20, 40, 255));
   spc2b->setPaletteBackgroundColor(QColor(20, 40, 255));
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
   lbPic2->setPaletteBackgroundColor(QColor(20, 40, 255));
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
   spc3a->setPaletteBackgroundColor(QColor(20, 40, 255));
   spc3b->setPaletteBackgroundColor(QColor(20, 40, 255));
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
   lbPic3->setPaletteBackgroundColor(QColor(20, 40, 255));
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
   lbLow1->setPaletteForegroundColor(QColor(240, 200, 0));
   lbLow1->setMaximumHeight(lohiFM.height());
   lbLow1->setPaletteBackgroundColor(QColor(20, 40, 255));
   lbLow1->setFont(lohiFont);
   QLabel *lbLow2 =  new QLabel("LO", page2Dia);
   lbLow2->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
   lbLow2->setPaletteForegroundColor(QColor(240, 200, 0));
   lbLow2->setMaximumHeight(lohiFM.height());
   lbLow2->setPaletteBackgroundColor(QColor(20, 40, 255));
   lbLow2->setFont(lohiFont);
   QLabel *lbLow3 =  new QLabel("LO", page2Dia);
   lbLow3->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
   lbLow3->setPaletteForegroundColor(QColor(240, 200, 0));
   lbLow3->setMaximumHeight(lohiFM.height());
   lbLow3->setPaletteBackgroundColor(QColor(20, 40, 255));
   lbLow3->setFont(lohiFont);
   QLabel *lbHi1 = new QLabel("HI", page2Dia);
   lbHi1->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
   lbHi1->setPaletteForegroundColor(QColor(240, 200, 0));
   lbHi1->setMaximumHeight(lohiFM.height());
   lbHi1->setPaletteBackgroundColor(QColor(20, 40, 255));
   lbHi1->setFont(lohiFont);
   QLabel *lbHi2 = new QLabel("HI", page2Dia);
   lbHi2->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
   lbHi2->setPaletteForegroundColor(QColor(240, 200, 0));
   lbHi2->setMaximumHeight(lohiFM.height());
   lbHi2->setPaletteBackgroundColor(QColor(20, 40, 255));
   lbHi2->setFont(lohiFont);
   QLabel *lbHi3 = new QLabel("HI", page2Dia);
   lbHi3->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
   lbHi3->setPaletteForegroundColor(QColor(240, 200, 0));
   lbHi3->setMaximumHeight(lohiFM.height());
   lbHi3->setPaletteBackgroundColor(QColor(20, 40, 255));
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
   lbDesc->setPaletteBackgroundColor(QColor(20, 40, 255));
   lbDesc->setFont(headFont);
   lbDesc->setMinimumWidth((int)(750*wmult));
   lbDesc->setMaximumWidth((int)(750*wmult));
   lbDesc->setFrameStyle( QFrame::Panel | QFrame::Raised );

   ext3->addWidget(lbDesc, 0);
}

if (pageNum == 4)
{
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
   lbPic5->setPaletteBackgroundColor(QColor(20, 40, 255));
   lbPic5->setPixmap(pic5);
   lbPic5->setFrameStyle( QFrame::Panel | QFrame::Raised );


   lbTDesc = new QLabel(tomDesc, page4Dia);
   lbTDesc->setAlignment( Qt::AlignTop | Qt::AlignLeft | Qt::WordBreak);
   lbTDesc->setPaletteBackgroundColor(QColor(20, 40, 255));
   lbTDesc->setFont(headFont);
   lbTDesc->setMinimumWidth((int)(550*wmult));
   lbTDesc->setMaximumWidth((int)(750*wmult));
   lbTDesc->setFrameStyle( QFrame::Panel | QFrame::Raised );

   ext4->addWidget(lbPic5, 0);
   ext4->addWidget(lbTDesc, 0);
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
 
   }

}

void Weather::UpdateData()
{
	int check = -1;
	int cnt = 0;
	accel->setEnabled(false);
	while (check != 0)
	{
		if (debug == true)
			cout << "MythWeather: COMMS : GetWeatherData() ...\n";
		check = GetWeatherData();
		if (check == -2)
		{
			cnt = 0;
			while (cnt < 250)
			{
				qApp->processEvents();
				usleep(50);
				cnt++;
			}
		}
		if (check == 1)
		{
			cnt = 0;
			while (cnt < 750)
			{
				qApp->processEvents();
				usleep(50);
				cnt++;
			}
		}	
		
	}
	accel->setEnabled(true);

	if (httpData.find("<html>", 0) > 0 || 
	    httpData.find("Microsoft VBScript runtime", 0) > 0 ||
	    httpData.find("Internal Server Error", 0) > 0 ||
	    httpData.find("Bad Request", 0) > 0)
	{
		if (debug == true)	
			cout << "MythWeather: COMMS : Invalid Area Data\n";
		validArea = false;
		httpData = oldhttpData;
		return;
	}
	else
	{
		if (debug == true)
			cout << "MythWeather: COMMS : Valid Area Data\n";
		validArea = true;
		oldhttpData = httpData;
	}


	//cout << "HTTP DATA: " << httpData << endl;

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
/*
	for (int i = 23; i < 39; i++)
		cout << i << " ---- " << holdings[i] << endl;
*/

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

int Weather::GetWeatherData()
{
	if (debug == true)
		cout << "MythWeather: COMMS : Setting status timer and data hook.\n";
	status_Timer->start(100);
	gotDataHook = false;

	httpSock = new QSocket(this);
	connect( httpSock, SIGNAL(connected()),
                SLOT(socketConnected()) );
        connect( httpSock, SIGNAL(connectionClosed()),
                SLOT(socketConnectionClosed()) );
        connect( httpSock, SIGNAL(readyRead()),
                SLOT(socketReadyRead()) );
        connect( httpSock, SIGNAL(error(int)),
                SLOT(socketError(int)) );

	//cerr << "MythWeather: Connecting to host..." << endl;
	if (debug == true)
		cout << "MythWeather: Connecting to host...";
	lbUpdated->setText("Contacting Host...");
	httpSock->connectToHost("www.msnbc.com", 80);

	con_attempt++;

	int num = 0;
        while (httpSock->state() == QSocket::HostLookup ||
               httpSock->state() == QSocket::Connecting)
        {
         qApp->processEvents();
         usleep(50);
         num++;
         if (num > 500)
         {
	     if (debug == true)
			cout << "Error.\n";
	     char tMsg[1024];
	     sprintf (tMsg, "Connection Timed Out ... Attempt #%d", con_attempt);
	     updated = tMsg;
   	     lbUpdated->setText(updated);
	     closeConnection();

	     if (httpData.length() == 0)
	     {
			cerr << "MythWeather: (2) No Data ... Trying Again ...\n";
			lbStatus->setText(" ");
			status_Timer->stop();
			delete httpSock;
			return -2;
 	     }
	     else
	     {
		lbStatus->setText(" ");
		status_Timer->stop();
	     	return 1;
	     }
         }
        }

        if (httpSock->state() != QSocket::Connected)
        {
		if (debug == true)
			cout << "Error.\n";
		char tMsg[1024];
	        sprintf (tMsg, "Error Connecting ... Attempt #%d", con_attempt);
             	updated = tMsg;
		lbUpdated->setText(updated);
	        closeConnection();

		if (httpData.length() == 0)
                {
			char tMsg[1024];
                	sprintf (tMsg, "Error Connecting ... Attempt #%d", con_attempt);
                	updated = tMsg;
                	lbUpdated->setText(updated);
                        cerr << "MythWeather: (1) No Data ... Trying Again ...\n";
			lbStatus->setText(" ");
			status_Timer->stop();
			con_attempt++;
			delete httpSock;
			return 1;
                }
                else
	        {
			delete httpSock;
			lbStatus->setText(" ");
	        	status_Timer->stop();
                	return 1;
	     	}
        }

  	while (gotDataHook == false)
	{
		qApp->processEvents();
		usleep(50);
		if (conError == true)
		{
			conError = false;
			delete httpSock;
			return 1;
		}
	}

	if (debug == true)
		cout << "MythWeather: COMMS : Got some data...returning\n";

	con_attempt = 0;
	lbStatus->setText(" ");
        status_Timer->stop();
 	delete httpSock;

	return 0;
}

void Weather::closeConnection()
    {

	if (debug == true)
		cout << "MythWeather: COMMS (TCP) : Connection Closed.\n";

        httpSock->close();
        if ( httpSock->state() == QSocket::Closing ) {
            // We have a delayed close.
            connect( httpSock, SIGNAL(delayedCloseFinished()),
                    SLOT(socketClosed()) );
        } else {
            // The socket is closed.
            socketClosed();
        }
    }

    void Weather::socketReadyRead()
    {
	if (debug == true)
                cout << "MythWeather: COMMS (TCP) : Socket Data Ready.\n";

	char tMsg[1024];
        sprintf (tMsg, "Reading data [%d bytes read]...", httpData.length());
        updated = tMsg;
	lbUpdated->setText(updated);
	//cerr << "MythWeather: Reading Data From Host [";
        while ( httpSock->canReadLine() ) {
	    //cerr << ".";
            httpData = httpData + httpSock->readLine();
	    sprintf (tMsg, "Reading data [%d bytes read]...", httpData.length());
            updated = tMsg;
            lbUpdated->setText(updated);
        }
	//cerr << "]\n";
    }

    void Weather::socketConnected()
    {
	if (debug == true)
                cout << "MythWeather: COMMS (TCP) : Socket Connected.\n";

	lbUpdated->setText("Connected, requesting weather data...");
	QTextStream os(httpSock);
	//cerr << "MythWeather: Connected! Requesting Weather Data ...\n";
        os << "GET /m/chnk/d/weather_d_src.asp?acid=" 
	   << locale << " HTTP/1.1\n"
	   << "Connection: close\n"
	   << "Host: www.msnbc.com\n\n\n";
	httpData = "";
    }

    void Weather::socketConnectionClosed()
    {
	if (debug == true)
                cout << "MythWeather: COMMS (TCP) : Connection Closed from Remote.\n";

	gotDataHook = true;
    }

    void Weather::socketClosed()
    {
    }

    void Weather::socketError( int e )
    {
 	if (debug == true)
                cout << "MythWeather: COMMS (TCP) : Error #" << e << ".\n";

	conError = true;
	char tMsg[1024];
        sprintf (tMsg, "Error Connecting ... Attempt #%d", con_attempt);
        updated = tMsg;
        lbUpdated->setText(updated);
        closeConnection();
	e = 0;
    }

