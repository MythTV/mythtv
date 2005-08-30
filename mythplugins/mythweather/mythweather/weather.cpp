/*
        MythWeather
        Version 0.8
        January 8th, 2003

        By John Danner & Dustin Doris

        Note: Portions of this code taken from MythMusic

*/

#include <qlabel.h>
#include <fstream>
#include <iostream>
#include <qlayout.h>
#include <qdatetime.h>
#include <qlistview.h>
#include <qtimer.h>
#include <qimage.h>
#include <qregexp.h>
#include <qnetwork.h>
#include <qapplication.h>
#include <qcursor.h>
#include <qstringlist.h>
#include <qheader.h>
#include <qpixmap.h>
#include <unistd.h>
#include <qurl.h>
#include <qdir.h>

#include "weather.h"

#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>
#include <mythtv/httpcomms.h>



using namespace std;

Weather::Weather(int appCode, MythMainWindow *parent, const char *name)
       : MythDialog(parent, name)
{
    timeoutCounter = 0;
    stopProcessing = false;

    allowkeys = true;
    wantAnimated = gContext->GetNumSetting("WeatherWantAnimated", 1);   
    weatherTimeoutInt = gContext->GetNumSetting("WeatherTimeout", 20);
    weatherTimeoutInt *= 1000;
    
    debug = false;
    if (appCode == 1)
        debug = true;
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
    curConfig = 1;
    changeTemp = false;
    changeLoc = false;
    changeAgg = false;
    noACCID = false;

    wData = NULL;
    fullRect = QRect(0, 0, size().width(), size().height());
    newlocRect = QRect(0, 0, size().width(), size().height());


    theme = new XMLParse();
    theme->SetWMult(wmult);
    theme->SetHMult(hmult);
    theme->LoadTheme(xmldata, "weather", "weather-");
    LoadWindow(xmldata);

    LayerSet *container = theme->GetSet("weatherpages");
    AnimatedImage = NULL;
    if (container)
    {
       AnimatedImage = (UIAnimatedImageType *)container->GetType("animatedradarimg");
       if (AnimatedImage)
       {
           if (debug == true)
             cout << "MythWeather: UIAnimatedImage found" << endl;
           AnimatedImage->SetWindow((MythDialog*) this);
           AnimatedImage->Pause();
           //connect(Image, SIGNAL(requestUpdate(const QRect &)),this,
           //    SLOT(updateAnimated(const QRect &)));
       }
       else
       {
          cerr << "MythWeather: The theme you are using does not contain an 'AnimatedImage' element" << endl;
          cerr << "Please contact the theme creator and ask if they could please update it." << endl;
          cerr << "Will fallback to using a static radar image." << endl;
       }
    }

    updateBackground();

    QString accid = gContext->GetShareDir() + "mythweather/accid.dat";

    accidFile.open(accid, ios::in | ios::binary);
    if (!accidFile)
    {
        noACCID = true;
        if (debug == true)
             cerr << "MythWeather: ACCID Data File Error (file missing!)" << endl;
    }
    else
        loadAccidBreaks();

    config_Aggressiveness = (gContext->GetSetting("WeatherAggressiveLvl")).toInt();
    if (config_Aggressiveness > 15)
        config_Aggressiveness = 15;

    if (debug == true)
        cerr << "MythWeather: Reading 'locale' from context.\n";
    setLocation(gContext->GetSetting("locale"));
    config_Location = locale;

    if (locale.length() == 0)
    {
        if (debug == true)
                cerr << "MythWeather: --- No locale set, entering setup\n";
        readReadme = true;
    }
    else
    {
        QString temp = findNamebyAccid(locale);
        if (debug == true)
                cerr << "MythWeather: --- Locale: " << locale << endl;
    }

    if (debug == true)
        cerr << "MythWeather: Reading 'SIUnits' from context.\n";
    
    QString convertFlag = gContext->GetSetting("SIUnits");
    if (convertFlag.upper() == "YES")
    {
        config_Units = 2;
        if (debug == true)
                cerr << "MythWeather: --- Converting Data\n";
        convertData = true;
    }
    else
        config_Units = 1;

    updateInterval = 30;
    nextpageInterval = 10;
    nextpageIntArrow = 20;
    currentPage = 0;

    if (debug == true)
        cerr << "MythWeather: Loading Weather Types.\n";
    loadWeatherTypes();
   
    con_attempt = 0;

    showtime_timeout();
    showLayout(0);

    if (debug == true)
        cerr << "MythWeather: Setting up timers.\n";
    QTimer *showtime_Timer = new QTimer(this);
    connect(showtime_Timer, SIGNAL(timeout()), SLOT(showtime_timeout()) );
    showtime_Timer->start((int)(60 * 1000));

    update_Timer = new QTimer(this);
    connect(update_Timer, SIGNAL(timeout()), SLOT(update_timeout()) );
    if (readReadme == false)
        update_Timer->start((int)(10));   

    nextpage_Timer = new QTimer(this);
    connect(nextpage_Timer, SIGNAL(timeout()), SLOT(nextpage_timeout()) );

    urlTimer = new QTimer(this);
    connect(urlTimer, SIGNAL(timeout()), SLOT(weatherTimeout()));

    setNoErase();

    if (debug == true)
        cerr << "MythWeather: Finish Object Initialization.\n";

    if (readReadme == true || appCode == 2)
    {
        lastCityNum = (int)(accidBreaks[0]) - 1;
        setupPage();
    }
}

Weather::~Weather()
{
    accidFile.close();
    delete nextpage_Timer;
    delete update_Timer;
    delete urlTimer;
    delete theme;

    if (wData)
        delete [] wData;
}

void Weather::keyPressEvent(QKeyEvent *e)
{
    if (!allowkeys)
        return;

    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Weather", e, actions);

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "LEFT")
            cursorLeft();
        else if (action == "RIGHT")
            cursorRight();
        else if (action == "UP")
            upKey();
        else if (action == "DOWN")
            dnKey();
        else if (action == "PAGEUP")
            pgupKey();
        else if (action == "PAGEDOWN")
            pgdnKey();
        else if (action == "SELECT")
            resetLocale();
        else if (action == "PAUSE")
            holdPage();
        else if (action == "MENU")
            setupPage();
        else if (action == "INFO")
            convertFlip();
        else if (action == "0" || action == "1" || action == "2" ||
                 action == "3" || action == "4" || action == "5" ||
                 action == "6" || action == "7" || action == "8" ||
                 action == "9")
        {
            newLocaleX(action.toInt());
        }
        else
            handled = false;
    }

    if (!handled)
        MythDialog::keyPressEvent(e);
}

void Weather::updateBackground(void)
{
    QPixmap bground(size());
    bground.fill(this, 0, 0);
    QPixmap pix(fullRect.size());
    pix.fill(this, fullRect.topLeft());

    QPainter tmp(&bground);

    LayerSet *container = theme->GetSet("background");
    if (container)
    {
        container->Draw(&tmp, 0, 0);
    }

    tmp.end();
    realBackground = bground;

    QPainter start_tmp(&pix);

    container = theme->GetSet("startup");
    if (container)
    {
        container->Draw(&start_tmp, 0, 0);
        container->Draw(&start_tmp, 1, 0);
        container->Draw(&start_tmp, 2, 0);
        container->Draw(&start_tmp, 3, 0);
        container->Draw(&start_tmp, 4, 0);
        container->Draw(&start_tmp, 5, 0);
        container->Draw(&start_tmp, 6, 0);
        container->Draw(&start_tmp, 7, 0);
        container->Draw(&start_tmp, 8, 0);
    }   

    start_tmp.end();

    setPaletteBackgroundPixmap(pix);
}

void Weather::LoadWindow(QDomElement &element)
{

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement e = child.toElement();
        if (!e.isNull())
        {
            if (e.tagName() == "font")
            {
                theme->parseFont(e);
            }
            else if (e.tagName() == "container")
            {
                QRect area;
                QString name;
                int context;
                theme->parseContainer(e, name, context, area);
            }
            else
            {
                cerr << "Unknown element: " << e.tagName() << endl;
                exit(0);
            }
        }
    }
}

void Weather::paintEvent(QPaintEvent *e)
{
    QRect r = e->rect();
    QPainter p(this);

    if (r.intersects(fullRect))
        updatePage(&p);
}

void Weather::updatePage(QPainter *dr)
{
    QRect pr = fullRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);

  if (inSetup == false)
  {
    LayerSet *container = NULL;

    container = NULL;
    container = theme->GetSet("weatherpages");
    if (container && currentPage > 0)
    {
        container->Draw(&tmp, 0, currentPage);
        container->Draw(&tmp, 1, currentPage);
        container->Draw(&tmp, 2, currentPage);
        container->Draw(&tmp, 3, currentPage);
        container->Draw(&tmp, 4, currentPage);
        container->Draw(&tmp, 5, currentPage);
        container->Draw(&tmp, 6, currentPage);
        container->Draw(&tmp, 7, currentPage);
        container->Draw(&tmp, 8, currentPage);
    }

    container = theme->GetSet("newlocation");
    if (container && newLocaleHold.length() > 0)
    {
        container->Draw(&tmp, 0, 0);
        container->Draw(&tmp, 1, 0);
        container->Draw(&tmp, 2, 0);
        container->Draw(&tmp, 3, 0);
        container->Draw(&tmp, 4, 0);
        container->Draw(&tmp, 5, 0);
        container->Draw(&tmp, 6, 0);
        container->Draw(&tmp, 7, 0);
        container->Draw(&tmp, 8, 0);
    }

  }
  else
  {
    LayerSet *container = theme->GetSet("setup");
    if (container)
    {
        container->Draw(&tmp, 0, 0);
        container->Draw(&tmp, 1, 0);
        container->Draw(&tmp, 2, 0);
        container->Draw(&tmp, 3, 0);
        container->Draw(&tmp, 4, 0);
        container->Draw(&tmp, 5, 0);
        container->Draw(&tmp, 6, 0);
        container->Draw(&tmp, 7, 0);
        container->Draw(&tmp, 8, 0);
    }
  }
 
  tmp.end();
  dr->drawPixmap(pr.topLeft(), pix);

}

void Weather::loadAccidBreaks()
{
        for (int i = 0; i < 26; i++)
        {
                if (accidFile.eof())
                {
                        noACCID = true;
                        if (debug == true)
                            cerr << "MythWeather: ACCID Data File Error (unexpected eof)" << endl;
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
            if (cfgCity.stripWhiteSpace().length() != 0)
            {
                 config_accid = findAccidbyName(cfgCity.stripWhiteSpace());
                 gContext->SetSetting("locale", config_accid);
                 setLocation(config_accid);
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

        MSqlQuery query(MSqlQuery::InitCon());
        query.exec(thequery);

        int rows = query.numRowsAffected();
        if (rows > 0)
        {

           if (global == false)
                thequery = QString("UPDATE settings SET data=\"%1\" WHERE value=\"%2\" AND hostname=\"%3\";")
                        .arg(data).arg(value).arg(gContext->GetHostName());
           else
                thequery = QString("UPDATE settings SET data=\"%1\" WHERE value=\"%2\";")
                        .arg(data).arg(value);
        
           query.exec(thequery);
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

           query.exec(thequery);

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
                hold = strtok(temp, "::");
                hold = strtok(NULL, "::");
                accid = hold;
                hold = strtok(NULL, "::");	

                if (strcmp(hold, name) == 0)
                {
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
        accidFile.seekg((streamoff)startData + 
                        accidBreaks[curLetter + 26], ios::beg);

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

   LayerSet *container = theme->GetSet("setup");
   if (container)
   {
       UIListType *ltype = (UIListType *)container->GetType("alpha");
       if (ltype)
       {
           ltype->ResetList();
           ltype->SetItemCurrent(4);
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
                ltype->SetItemText(cnt, temp);
                cnt++;
           }

           loadCityData(0);
           showCityName();
       }
   }
   update(fullRect);
}

void Weather::backupCity(int num)
{
        char temporary[1024];
        char temp2[1024];
        char *np;
        int prev = 0;
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
   LayerSet *container = theme->GetSet("setup");
   if (container)
   {
       UIListType *ltype = (UIListType *)container->GetType("mainlist");
       if (ltype)
       {
           ltype->ResetList();
           ltype->SetItemCurrent(4);
           int cnt = 0;
           for (int i = 0; i < 9; i++)
           {
                ltype->SetItemText(i, tr(cityNames[i].left(cityNames[i].find("(",0)-1)) 
                                + cityNames[i].mid(cityNames[i].find("(",0)-1));
                cnt++;
           }
           cfgCity = cityNames[4];
       }
   }
   update(fullRect);
}

void Weather::processEvents()
{
        qApp->processEvents();
}

QString Weather::getLocation()
{
        return locale;
}

void Weather::setLocation(QString newLocale)
{
    locale = newLocale;

    if (locale == "" || locale.length() < 2)
        return;

    LayerSet* container = theme->GetSet("weatherpages");
    UITextType *texttype;
    UIImageType *imagetype;

    if (!AnimatedImage)
        return;

    // locale starts with "US" or locale is a zip code
    if (locale.left(2) == "US" || locale.left(5).contains(QRegExp("[0-9]{5,5}")) > 0)
    {
        // update page to show US radar images
        if (AnimatedImage)
        {
            AnimatedImage->SetSize(765, 500);
            AnimatedImage->SetSkip(0, 58);
        }

        if (container)
        {
            texttype = (UITextType *) container->GetType("header5");
            if (texttype)
                texttype->SetText(tr("doppler radar"));

            for (int x = 1; x <= 9; x++)
            {
                texttype = (UITextType *) container->GetType(QString("maplabel-%1")
                           .arg(x));
                if (texttype)
                  texttype->show();
            }

            for (int x = 10; x <= 12; x++)
            {
                texttype = (UITextType *) container->GetType(QString("maplabel-%1")
                           .arg(x));
                if (texttype)
                    texttype->hide();
            }

            imagetype = (UIImageType *) container->GetType("logo");
            if (imagetype)
            {
                imagetype->SetSize(80, 60);
                imagetype->LoadImage();
            }

            imagetype = (UIImageType *) container->GetType("radarbk");
            if (imagetype)
            {
                imagetype->SetImage("mw-map.png");
                imagetype->LoadImage();
            }
        }
    }
    else
    {
       // show satellite images
       if (AnimatedImage)
        {
            AnimatedImage->SetSize(765, 500 - 58);
            AnimatedImage->SetSkip(0, 0);
        }

        if (container)
        {
            texttype = (UITextType *) container->GetType("header5");
            if (texttype)
                texttype->SetText(tr("satellite image"));

            for (int x = 1; x <= 9; x++)
            {
                texttype = (UITextType *) container->GetType(QString("maplabel-%1")
                           .arg(x));
                if (texttype)
                    texttype->hide();
            }

            for (int x = 10; x <= 12; x++)
            {
                texttype = (UITextType *) container->GetType(QString("maplabel-%1")
                           .arg(x));
                if (texttype)
                    texttype->show();
            }

            imagetype = (UIImageType *) container->GetType("logo");
            if (imagetype)
            {
                imagetype->SetSize(100, 80);
                imagetype->LoadImage();
            }

            imagetype = (UIImageType *) container->GetType("radarbk");
            if (imagetype)
            {
                imagetype->SetImage("mw-map-sat.png");
                imagetype->LoadImage();
            }
        }
    }
}

void Weather::loadWeatherTypes()
{
   wData = new weatherTypes[128];
   char temporary[256];
   int wCount = 0;

   ifstream weather_data(gContext->GetShareDir() +
                         "mythweather/weathertypes.dat", ios::in);
   if (weather_data == NULL)
   {
        cerr << "MythWeather: Error reading " << gContext->GetShareDir() << "mythweather/weathertypes.dat...exiting...\n";
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
        wData[wCount].typeIcon = datas[2];

        wCount++;
        }
   }

}

void Weather::showtime_timeout()
{
   QDateTime new_time(QDate::currentDate(), QTime::currentTime());
   QString curTime = new_time.toString("h:mm ap");
   QString curDate;
   if (QString(gContext->GetSetting("Language")) == "JA")
     curDate = new_time.toString("M/d (ddd)");
   else
     curDate = new_time.toString("ddd MMM d");
   QString temp = "";
   curTime = curTime.upper();
   curDate = curDate.upper();

   LayerSet *container = theme->GetSet("weatherpages");
   if (container)
   {
       SetText(container, "currenttime", curTime);
       SetText(container, "currentdate", curDate);
   }

}

void Weather::newLocaleX(int newDigit)
{
    if (inSetup == true && deepSetup == true && curConfig == 2 && gotLetter == true)
    {
        changeLoc = true;

        switch (newDigit)
    {
            case 0: break;
            case 1: curCity = curCity - 25;           break;
            case 2: curCity = curCity - 50;           break;
            case 3: curCity = curCity - 100;          break;
            case 4: curCity = 0;                      break;
            case 5: curCity = (int)(lastCityNum / 2); break;
            case 6: curCity = lastCityNum;            break;
            case 7: curCity = curCity + 25;           break;
            case 8: curCity = curCity + 50;           break;
            case 9: curCity = curCity + 100;          break;

            default: cerr << "MythWeather: Non-digit passed to newLocaleX" << endl; break;
        }

        if (curCity < 0)
        curCity = 0;

        if (curCity > lastCityNum)
                curCity = lastCityNum;

        loadCityData(curCity);
        showCityName();
    }
    else if (inSetup == false)
    {
        switch (newDigit)
        {
            case 0: newLocaleHold = newLocaleHold + "0"; break;
            case 1: newLocaleHold = newLocaleHold + "1"; break;
            case 2: newLocaleHold = newLocaleHold + "2"; break;
            case 3: newLocaleHold = newLocaleHold + "3"; break;
            case 4: newLocaleHold = newLocaleHold + "4"; break;
            case 5: newLocaleHold = newLocaleHold + "5"; break;
            case 6: newLocaleHold = newLocaleHold + "6"; break;
            case 7: newLocaleHold = newLocaleHold + "7"; break;
            case 8: newLocaleHold = newLocaleHold + "8"; break;
            case 9: newLocaleHold = newLocaleHold + "9"; break;

            default: cerr << "MythWeather: Non-digit passed to newLocaleX" << endl; break;
        } 

        LayerSet *container = theme->GetSet("newlocation");
        if (container)
        {
            UITextType *type = (UITextType *)container->GetType("locationhold");
            if (type)
                type->SetText(newLocaleHold);
        }
        if (newLocaleHold.length() == 5)
        {
                setLocation(newLocaleHold);
                newLocaleHold = "";
                update(newlocRect);
                update_timeout();
        }
        update(newlocRect);
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
                        txtLocale += tr(" is invalid)");
                else
                        txtLocale += ")";
        }
        else
        {
                txtLocale += state + ", " + country + " (" + locale;
                if (validArea == false)
                        txtLocale += tr(" is invalid)");
                else
                        txtLocale += ")";
        }

        if (readReadme == true)
                txtLocale += tr("   No Location Set, Please read the README");

        LayerSet *container = theme->GetSet("weatherpages");
        if (container)
            SetText(container, "location", tr(txtLocale.left(txtLocale.find("(",0)-1)) 
                                + txtLocale.mid(txtLocale.find("(",0)-1));
   }
   else
   {
        nextpage_Timer->stop();
        LayerSet *container = theme->GetSet("weatherpages");
        if (container)
        {
            UITextType *type = (UITextType *)container->GetType("location");
            if (type)
            {
                QString cur = type->GetText();
                SetText(container, "location", cur + tr(" - PAUSED -"));
            }
        }
   }
   update(fullRect);
  }
}

void Weather::resetLocale()
{
     if (inSetup == false)
     {
        setLocation(gContext->GetSetting("locale"));
        update_timeout();
     }
}

void Weather::setupPage()
{
    if (inSetup == false)
    {
        if (firstRun == true)
            setPaletteBackgroundPixmap(realBackground);

        LayerSet *container = theme->GetSet("setup");
        if (container)
        {
            UITextType *type = (UITextType *)container->GetType("error");
            if (type)
            {
               if (noACCID == false)
                   type->SetText(tr("Configuring MythWeather..."));
               else
                   type->SetText("Missing ACCID data file!");
            }
        
            type = (UITextType *)container->GetType("help");
            if (type)
                type->SetText(tr("Use the right arrow key to select unit conversion..."));
   
            UIListType *ltype = (UIListType *)container->GetType("options");
            if (ltype)
            {
                ltype->ResetList();
                ltype->SetActive(true);
                ltype->SetItemText(0, tr("Weather Units"));
                ltype->SetItemText(1, tr("Location"));
                ltype->SetItemText(2, tr("Aggressiveness"));
                ltype->SetItemCurrent(0);
            }
            ltype = (UIListType *)container->GetType("mainlist");
            if (ltype)
            {
                ltype->ResetList();
                ltype->SetItemText(0, tr("Imperial (Fahrenheit, in, etc)"));
                ltype->SetItemText(1, tr("Metric (Celsius, kPa, etc)"));
                ltype->SetItemCurrent(config_Units - 1);
            }
            ltype = (UIListType *)container->GetType("alpha");
            if (ltype)
                ltype->ResetList();
        }

        inSetup = true;
        nextpage_Timer->stop();
        showLayout(5);
    }
    else
    {
        inSetup = false;
        deepSetup = false;
        curConfig = 1;
        gotLetter = false;
        
        saveConfig();

        if (readReadme == true)
        {
                LayerSet *container = theme->GetSet("weatherpages");
                if (container)
                {
                    SetText(container, "location", tr("Configuration Saved..."));
                    SetText(container, "updatetime", tr("Retrieving weather data..."));
                }
                readReadme = false;
                update_Timer->start((int)(10));
                showLayout(1);
        }
        else
        {
                firstRun = true;	

                if (changeLoc == true || changeTemp == true)
                        update_Timer->changeInterval((int)(100));
                else
                {
                        QString txtLocale = city + ", ";
                        if (state.length() == 0)
                        {
                                txtLocale += country + " (" + locale;
                                if (validArea == false)
                                        txtLocale += tr(" is invalid)");
                                else
                                        txtLocale += ")";
                        }
                        else
                        {
                                txtLocale += state + ", " + country + " (" + locale;
                                if (validArea == false)
                                        txtLocale += tr(" is invalid)");
                                else
                                        txtLocale += ")";
                        }
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
                        cerr << "MythWeather: Converting weather data.\n";
                convertData = true;
        }
        else
        {
                if (debug == true)
                        cerr << "MythWeather: Not converting weather data.\n";
                convertData = false;
        }

        update_timeout();
        update(fullRect);
    }
    else
    {
        setupPage();
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

            LayerSet *container = theme->GetSet("setup");
            if (container)
            {
                UIListType *ltype = (UIListType *)container->GetType("options");
                if (ltype)
                    ltype->SetItemCurrent(curConfig - 1);

                ltype = (UIListType *)container->GetType("mainlist");
                if (ltype)
                    ltype->ResetList();
                ltype = (UIListType *)container->GetType("alpha");
                if (ltype)
                    ltype->ResetList();
            }
            UITextType *type = NULL;
            switch (curConfig)
            {
                case 1:
                       if (container)
                       {
                           type = (UITextType *)container->GetType("help");
                           if (type)
                               type->SetText(tr("Use the right arrow key to select unit conversion..."));

                           UIListType *ltype = (UIListType *)container->GetType("mainlist");
                           if (ltype)
                           {
                               ltype->SetItemText(0, tr("Imperial (Fahrenheit, in, etc)"));
                               ltype->SetItemText(1, tr("Metric (Celsius, kPa, etc)"));
                               ltype->SetItemCurrent(config_Units - 1);
                           }
                       }
                       break;
                case 2:
                       if (container)
                       {
                           type = (UITextType *)container->GetType("help");
                           if (type)
                               type->SetText(tr("Use the right arrow key to select your location..."));
                       }
                       updateLetters();          
                       loadCityData(curCity);
                       showCityName();
                       break;
                case 3:
                       if (container)
                       {
                           type = (UITextType *)container->GetType("help");
                           if (type)
                               type->SetText(tr("Use the right arrow key to select the aggressiveness level..."));
                       }
                       updateAggr();
                       break;
            }
        }
        else
        {
            if (curConfig == 1)
            {
                LayerSet *container = theme->GetSet("setup");
                if (container)
                {
                    UIListType *type = (UIListType *)container->GetType("mainlist");
                    changeTemp = true;
                    if (config_Units == 1 && container)
                    {
                        type->SetItemCurrent(1);
                        config_Units = 2;
                    }	
                    else if (container)
                    {
                        type->SetItemCurrent(0);
                        config_Units = 1;
                    }
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

                    LayerSet *container = theme->GetSet("setup");
                    if (container)
                    {
                        UIListType *type = (UIListType *)container->GetType("mainlist");

                        if ((type->GetItemText(3)).length() > 2)
                        {

                                curCity--;
                                if (curCity < 0)
                                        curCity = 0;

                                loadCityData(curCity);
                                showCityName();
                        }
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
       update(fullRect);
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
   
            LayerSet *container = theme->GetSet("setup");
            if (container)
            {
                UIListType *ltype = (UIListType *)container->GetType("options");
                if (ltype)
                    ltype->SetItemCurrent(curConfig - 1);
                ltype = (UIListType *)container->GetType("mainlist");
                if (ltype)
                    ltype->ResetList();
                ltype = (UIListType *)container->GetType("alpha");
                if (ltype)
                    ltype->ResetList();
            }
            UITextType *type = NULL;
            switch (curConfig)
            {
                case 1:
                       if (container)
                       {
                           type = (UITextType *)container->GetType("help");
                           if (type)
                               type->SetText(tr("Use the right arrow key to select unit conversion..."));

                           UIListType *ltype = (UIListType *)container->GetType("mainlist");
                           if (ltype)
                           {
                               ltype->SetItemText(0, "Imperial (Fahrenheit, in, etc)");
                               ltype->SetItemText(1, "Metric (Celsius, kPa, etc)");
                               ltype->SetItemCurrent(config_Units - 1);
                           }
                       }
                       break;
                case 2:
                       if (container)
                       {
                           type = (UITextType *)container->GetType("help");
                           if (type)
                               type->SetText(tr("Use the right arrow key to select your location..."));
                           
                       }
                       updateLetters();
                       loadCityData(curCity);
                       showCityName();
                       break;
                case 3:
                       if (container)
                       {
                           type = (UITextType *)container->GetType("help");
                           if (type)
                               type->SetText(tr("Use the right arrow key to select the aggressiveness level..."));
                       }
                       updateAggr();
                       break;
            }
        }
        else
        {
            if (curConfig == 1)
            {
                LayerSet *container = theme->GetSet("setup");
                if (container)
                {
                    UIListType *type = (UIListType *)container->GetType("mainlist");
                    changeTemp = true;
                    if (config_Units == 1 && container)
                    {
                        type->SetItemCurrent(1);
                        config_Units = 2;
                    }
                    else if (container)
                    {
                        type->SetItemCurrent(0);
                        config_Units = 1;
                    }
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
                   LayerSet *container = theme->GetSet("setup");
                   if (container)
                   {
                       UIListType *type = (UIListType *)container->GetType("mainlist");

                       if ((type->GetItemText(5)).length() > 2)
                       {

                           curCity++;
                           if (curCity > lastCityNum)
                               curCity = lastCityNum;

                           loadCityData(curCity);
                           showCityName();
                       }
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
        update(fullRect);
   }
}

void Weather::updateAggr()
{
    LayerSet *container = theme->GetSet("setup");
    if (container)
    {
        UIListType *type = (UIListType *)container->GetType("mainlist");
        type->ResetList();
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
                        temp = tr(" 1  High Speed Connection");
                else if (h == 8)
                        temp = tr(" 8  Medium Speed Connection");
                else if (h == 15)
                        temp = tr(" 15 Low Speed Connection");
                else
                        temp = QString(" %1 ").arg(h);
                if (i == config_Aggressiveness)
                    type->SetItemCurrent(cnt);
                type->SetItemText(cnt, 2, temp);
                cnt++;
        }
    }
}

void Weather::cursorRight()
{
   if (inSetup == false)
   {
        if (nextpage_Timer->isActive())
            nextpage_Timer->changeInterval((int)(1000 * nextpageIntArrow));
        int tp = currentPage;
        tp++;

        if (tp == 6)
             tp = 1;

        if (tp == 3 && pastTime == true)
                tp = 4;

        if (tp == 4 && pastTime == false)
                tp = 5;

        showLayout(tp);
   }
   else if (deepSetup == false)
   {
      LayerSet *container = theme->GetSet("setup");
      UIListType *ltype = NULL;
      ltype = (UIListType *)container->GetType("options");
      if (ltype)
      {
          ltype->SetItemCurrent(-1);
          ltype->SetActive(false);
      }
      deepSetup = true;
      if (curConfig == 2)
      {
          ltype = (UIListType *)container->GetType("alpha");
          if (ltype)
          {
              ltype->SetItemCurrent(4);
              ltype->SetActive(true);
          }
          ltype = (UIListType *)container->GetType("mainlist");
          if (ltype)
          {
              ltype->SetItemCurrent(-1);
              ltype->SetActive(false);
              loadCityData(curCity);
              showCityName();
          }
          
      }
      else if (curConfig == 3)
      {
          UIListType *ltype = NULL;
          ltype = (UIListType *)container->GetType("mainlist");
          if (ltype)
          {
              ltype->SetItemCurrent(4);
              ltype->SetActive(true);
          }
          ltype = (UIListType *)container->GetType("options");
          if (ltype)
          {
              ltype->SetActive(false);
          }
      }
      else
      {
          UIListType *ltype = NULL;
          ltype = (UIListType *)container->GetType("mainlist");
          if (ltype)
          {
              if (config_Units == 1)
                  ltype->SetItemCurrent(0);
              else
                  ltype->SetItemCurrent(1);

              ltype->SetActive(true);
          }
          ltype = (UIListType *)container->GetType("options");
          if (ltype)
          {
              ltype->SetActive(false);
          }
      }
   }
   else if (deepSetup == true)
   {
      LayerSet *container = theme->GetSet("setup");
      if (container && curConfig == 2 && gotLetter == false)
      {
          UIListType *ltype = NULL;

          gotLetter = true;
          ltype = (UIListType *)container->GetType("alpha");
          if (ltype)
          {
              ltype->SetActive(false);
          }
          ltype = (UIListType *)container->GetType("mainlist");
          if (ltype)
          {
              ltype->SetItemCurrent(4);
              ltype->SetActive(true);
          }
          loadCityData(curCity);
          showCityName();
      } 
   }
   update(fullRect);
}

void Weather::cursorLeft()
{
   if (inSetup == false)
   {
        if (nextpage_Timer->isActive())
            nextpage_Timer->changeInterval((int)(1000 * nextpageIntArrow));
        int tp = currentPage;
        tp--;
        
        if (tp == 0)
             tp = 5;

        if (tp == 3 && pastTime == true)
                tp = 2;

        if (tp == 4 && pastTime == false)
                tp = 3;

        showLayout(tp);
   }
   else if (deepSetup == true)
   {
      LayerSet *container = theme->GetSet("setup");
      if (container)
      {
          UIListType *ltype = NULL;

          if (curConfig != 2)
          {
              ltype = (UIListType *)container->GetType("mainlist");
              if (ltype)
              {
                  ltype->SetActive(false);
              }
              ltype = (UIListType *)container->GetType("options");
              if (ltype)
              {
                  ltype->SetItemCurrent(curConfig - 1);
                  ltype->SetActive(true);
              }
              deepSetup = false;
          }
          else
          {
              if (gotLetter == false)
              {
                  deepSetup = false;
                  ltype = (UIListType *)container->GetType("alpha");
                  if (ltype)
                  {
                      ltype->SetActive(false);
                  }
                  ltype = (UIListType *)container->GetType("options");
                  if (ltype)
                  {
                      ltype->SetItemCurrent(curConfig - 1);
                      ltype->SetActive(true);
                  }
              }
              else
              {
                  ltype = (UIListType *)container->GetType("alpha");
                  if (ltype)
                  {
                      ltype->SetItemCurrent(4);
                      ltype->SetActive(true);
                  }
                  ltype = (UIListType *)container->GetType("mainlist");
                  if (ltype)
                  {
                      ltype->SetActive(false);
                  }
                  gotLetter = false;
              }
          }
      } 
   }
   update(fullRect);
}

void Weather::nextpage_timeout()
{
   nextpage_Timer->changeInterval((int)(1000 * nextpageInterval));

   int tp = currentPage;
   tp++;
   if (tp > 5) 
        tp = 1;

   if (tp == 3 && pastTime == true)
        tp = 4;
   if (tp == 4 && pastTime == false)
        tp = 5;

   showLayout(tp);

}


void Weather::update_timeout()
{
    bool result = false;
    if (debug == true)
        cerr << "MythWeather: update_timeout() : Updating....\n";

    LayerSet *container = theme->GetSet("weatherpages");

    if (firstRun == true)
    {
        if (container)
            SetText(container, "updatetime", tr("Obtaining initial weather data..."));
        if (debug == true)
                cerr << "MythWeather: First run, reset timer to updateInterval.\n";
        update_Timer->changeInterval((int)(1000 * 60 * updateInterval));
    }
    else
    {
        if (container)
           SetText(container, "updatetime", tr("Updating weather data..."));
    }
    update(fullRect);

    result = UpdateData();

    if (firstRun == true)
        setPaletteBackgroundPixmap(realBackground);

    if (result == false)
    {
        showLayout(1);
        return;
    }

    if (result == true)
    {

    if (pastTime == true && currentPage == 3 && inSetup == false)
        nextpage_timeout();
    if (pastTime == false && currentPage == 4 && inSetup == false)
        nextpage_timeout();

    if (firstRun == true && inSetup == false)
        nextpage_Timer->start((int)(1000 * nextpageInterval));

    container = theme->GetSet("weatherpages");
    if (container)
    {
        SetText(container, "curtemp", curTemp);

        if (convertData == false)
            SetText(container, "units", "oF");
        else
            SetText(container, "units", "oC");

        if (convertData == false)
            SetText(container, "barometer", barometer + " in");
        else
            SetText(container, "barometer", barometer + " kPa");

        SetText(container, "humidity", curHumid + "%");

        if (winddir == "CALM")
            SetText(container, "winddata", tr("Calm"));
        else if (winddir == "VAR")
        {
            if (convertData == false)
                SetText(container, "winddata", curWind + " mph");
            else
                SetText(container, "winddata", curWind + " Km/h");
        }
        else
        {
            if (convertData == false)
                SetText(container, "winddata", tr(winddir) + tr(" at ") + curWind + " mph");
            else
                SetText(container, "winddata", tr(winddir) + tr(" at ") + curWind + " Km/h");
        }

        if (visibility.toFloat() != 999.00)
        {
            if (visibility == "")
                SetText(container, "visibility", "???");
            else if (convertData == false)
                SetText(container, "visibility", visibility + " mi");
            else
                SetText(container, "visibility", visibility + " km");
        }
        else
            SetText(container, "visibility", tr("Unlimited"));

        if (convertData == false)
            SetText(container, "windchill", curFeel + tr(" F"));
        else
            SetText(container, "windchill", curFeel + tr(" C"));

        if (uvIndex.toInt() < 3)
            SetText(container, "uvlevel", uvIndex + tr(" (minimal)"));
        else if (uvIndex.toInt() < 6)
            SetText(container, "uvlevel", uvIndex + tr(" (moderate)"));
        else if (uvIndex.toInt() < 8)
            SetText(container, "uvlevel", uvIndex + tr(" (high)"));
        else if (uvIndex.toInt() >= 8)
            SetText(container, "uvlevel", uvIndex + tr(" (extreme)"));

        UIImageType *itype = (UIImageType *)container->GetType("currentpic");
        if (itype)
        {
            itype->SetImage(curIcon);
            itype->LoadImage();
        }
    }

    QString txtLocale = city + ", ";
    if (state.length() == 0)
    {
        txtLocale += country + " (" + locale;
                if (validArea == false)
                        txtLocale += tr(" is invalid)");
                else
                        txtLocale += ")";
    }
    else
    {
        txtLocale += state + ", " + country + " (" + locale;
                if (validArea == false)
                        txtLocale += tr(" is invalid)");
                else
                        txtLocale += ")";
    }

    if (readReadme == true)
                txtLocale += tr("   No Location Set, Please read the README");

    container = theme->GetSet("weatherpages");
    if (container)
    {
        SetText(container, "location", tr(txtLocale.left(txtLocale.find("(",0)-1)) 
                                + txtLocale.mid(txtLocale.find("(",0)-1));
        SetText(container, "curcond", description);
    }

    QString todayDesc;

    todayDesc = tr("Today a high of ") + highTemp[0] + tr(" and a low of ");
    todayDesc += lowTemp[0] + tr(". Currently there is a humidity of ");
    todayDesc += curHumid + tr("% and the winds are");

    if (winddir == "CALM")
         todayDesc += tr(" calm.");
    else
    {
        if (convertData == false)
                todayDesc += tr(" coming in at ") + curWind + tr(" mph from the ") + tr(winddir) + ".";
        else 
                todayDesc += tr(" coming in at ") + curWind + tr(" Km/h from the ") + tr(winddir) + ".";
    }
  
    if (visibility.toFloat() == 999.00)
        todayDesc += tr(" Visibility will be unlimited for today.");
    else
    {
        if (visibility == "")
           todayDesc += tr(" Visibility conditions are unknown.");
        else if (convertData == false)
           todayDesc += tr(" There will be a visibility of ") + visibility + tr(" miles.");
        else
           todayDesc += tr(" There will be a visibility of ") + visibility + tr(" km.");
    }

    container = theme->GetSet("weatherpages");
    if (container)
    {
        SetText(container, "today-desc", todayDesc);
    }

    QString tomDesc;
 
    QString tomDate;
    if (date[0].upper() == "SUN")
        tomDate = tr("Sunday");
    else if (date[0].upper() == "MON")
        tomDate = tr("Monday");
    else if (date[0].upper() == "TUE")
        tomDate = tr("Tuesday");
    else if (date[0].upper() == "WED")
        tomDate = tr("Wednesday");
    else if (date[0].upper() == "THU")
        tomDate = tr("Thursday");
    else if (date[0].upper() == "FRI")
        tomDate = tr("Friday");
    else if (date[0].upper() == "SAT")
        tomDate = tr("Saturday");
    else
        tomDate = tr("Date Error");

    tomDesc = tr("Tomorrow expect a high of ") + highTemp[0] + tr(" and a low of ");
    tomDesc += lowTemp[0] + tr(".");

    QString tomCond = tr("Expected conditions: ") + weatherType[0];
 
    container = theme->GetSet("weatherpages");
    if (container)
    {
        SetText(container, "forecastdate", tomDate);
        SetText(container, "tempforecast", tomDesc);
        SetText(container, "condforecast", tomCond);

        UIImageType *itype = NULL;
        itype = (UIImageType *)container->GetType("todaypic");
        if (itype)
        {
            itype->SetImage(weatherIcon[0]);
            itype->LoadImage();
        }
 
        // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        //   
        //   WEATHER OUTLOOK SECTION
        //
        // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        itype = (UIImageType *)container->GetType("pic1");
        if (itype)
        {
            itype->SetImage(weatherIcon[1]);
            itype->LoadImage();
        }
        itype = (UIImageType *)container->GetType("pic2");
        if (itype)
        {
            itype->SetImage(weatherIcon[2]);
            itype->LoadImage();
        }
        itype = (UIImageType *)container->GetType("pic3");
        if (itype)
        {
            itype->SetImage(weatherIcon[3]);
            itype->LoadImage();
        }
        SetText(container, "date1", tr(date[1]));
        SetText(container, "date2", tr(date[2]));
        SetText(container, "date3", tr(date[3]));
        SetText(container, "desc1", weatherType[1]);
        SetText(container, "desc2", weatherType[2]);
        SetText(container, "desc3", weatherType[3]);
        SetText(container, "high1", highTemp[1]);
        SetText(container, "high2", highTemp[2]);
        SetText(container, "high3", highTemp[3]);
        SetText(container, "low1", lowTemp[1]);
        SetText(container, "low2", lowTemp[2]);
        SetText(container, "low3", lowTemp[3]);
    }

    container = theme->GetSet("weatherpages");
    if (container)
    {
         SetText(container, "updatetime", tr("Weather data from: ") + updated);
    }

    if (firstRun == true)
    {
        firstRun = false;
        if (inSetup == false)
                nextpage_timeout();
    }

    }
    

}

void Weather::SetText(LayerSet *container, QString widget, QString text)
{
    UITextType *myWidget = NULL;
    if (container)
    {
        QString temp = "";
        myWidget = (UITextType *)container->GetType(widget);
        if (myWidget)
            myWidget->SetText(text);

        for (int i = 0; i < 6; i++)
        {
             temp.sprintf("-%d", i);
             temp = widget + temp;
             myWidget = (UITextType *)container->GetType(temp);
             if (myWidget)
                 myWidget->SetText(text);
        }
    }
}

void Weather::showLayout(int pageNum)
{
   currentPage = pageNum;

   if (AnimatedImage)
   {
       if (currentPage == 5)
           AnimatedImage->UnPause();
       else
           AnimatedImage->Pause();
   }

   update(fullRect);
}

bool Weather::UpdateData()
{

    timeoutCounter = 0;
    LayerSet *container = theme->GetSet("weatherpages");
    if (container)
    {
        SetText(container, "updatetime", tr("Updating..."));
    }
    
    bool haveData = false;
    bool wantRetry = true;
    
    allowkeys = false;
    if (debug == true)
        cerr << "MythWeather: COMMS : GetWeatherData() ...\n";
    
    while((haveData == false)&& (wantRetry == true))
    {
        stopProcessing = false;
        urlTimer->start(weatherTimeoutInt);
        haveData = GetWeatherData();
        urlTimer->stop();
        
        if(haveData == false)
        {
            QString msg;
            if(wantAnimated)
            {
                
                msg = QString(tr("Myth was unable to retrieve your weather data within the "
                              "time allowed (%1 seconds).\nPress OK to try again with a larger "
                              "timeout value. Press Cancel to try again without animated radar maps."))
                              .arg(weatherTimeoutInt / 1000);
                
                wantRetry = MythPopupBox::showOkCancelPopup(gContext->GetMainWindow(),
                                                            "Timeout Reached", msg, true);
                if(!wantRetry)
                {
                    wantAnimated = 0;
                    wantRetry = true;
                }
            }
            else
            {
                msg = QString(tr("Myth was unable to retrieve your weather data within the "
                              "time allowed (%1 seconds).\nPress OK to try again with a larger "
                              "timeout value. Press Cancel to abort."))
                               .arg(weatherTimeoutInt / 1000);
                
                wantRetry = MythPopupBox::showOkCancelPopup(gContext->GetMainWindow(),
                                                            "Timeout Reached", msg, true);
            }
            

            if(wantRetry)
                weatherTimeoutInt += (weatherTimeoutInt / 2);
                                                        
        }
    }
    
    if (haveData == false)
    {
        VERBOSE(VB_IMPORTANT, "MythWeather: Failed to get weather data."); 
        accept();
        return false;
    }
    
    update(fullRect);
    allowkeys = true;

    if (haveData == true)
    {

        //updated = GetString("this.swLastUp");
        updated = QDateTime::currentDateTime().toString(gContext->GetSetting("dateformat") +
                  " " + gContext->GetSetting("timeformat"));
        city = GetString("this.swCity");
        state = GetString("this.swSubDiv");
        country = GetString("this.swCountry");
        curTemp = GetString("this.swTemp");
        if (curTemp.length() == 0)
        {
            curTemp = "-na-";
            updated = updated + tr(" (Not All Information Available)");
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
            switch (doDate.dayOfWeek())
            {
                case 1: date[i - 5] = QString("MON"); break;
                case 2: date[i - 5] = QString("TUE"); break;
                case 3: date[i - 5] = QString("WED"); break;
                case 4: date[i - 5] = QString("THU"); break;
                case 5: date[i - 5] = QString("FRI"); break;
                case 6: date[i - 5] = QString("SAT"); break;
                case 7: date[i - 5] = QString("SUN"); break;
            }
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
                wt[i] = tr(wData[j].typeName);
                weatherIcon[i] = "weather/" + wData[j].typeIcon;
                isSet = true;
                j = 128;
            }
        }

        if (isSet == false)
        {
            wt[i] = tr("Unknown") + " [" + wt[i] + "]";
            weatherIcon[i] = "weather/unknown.png";
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
            curIcon = "weather/" + wData[j].typeIcon;
            description = tr(wData[j].typeName);
            return;
        }
        
        if (txtType.toInt() == wData[j].typeNum)
        {
            curIcon = "weather/" + wData[j].typeIcon;
            description = tr(wData[j].typeName);
            return;
        }
    }
    
    curIcon = "weather/unknown.png";
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

    QString weatherDataURL = "http://www.msnbc.com/m/chnk/d/weather_d_src.asp?acid=" + locale;

    VERBOSE(VB_NETWORK, QString("Grabbing weather from: %1").arg(weatherDataURL));

    LayerSet *container = theme->GetSet("weatherpages");
    if (container)
        SetText(container, "updatetime", updated);


    httpData = HttpComms::getHttp(weatherDataURL, weatherTimeoutInt, 3, 3);

    if (httpData.find("this.swAcid = \"\";") != -1 ||
        httpData.find("<html>") != -1 ||
        httpData.find("Microsoft VBScript runtime") != -1 ||
        httpData.find("Internal Server Error") != -1  ||
        httpData.find("Bad Request", 0) != -1)
    {
        VERBOSE(VB_IMPORTANT, "MythWeather: Invalid area ID or server error.");
        if (debug)
            cerr << "MythWeather: HTTP Data Dump: " + httpData << endl;
        
        if (container)
            SetText(container, "updatetime", tr("*** Invalid Area ID or Server Error ***"));

        return false;
    }

     // try to get some animated radar maps
     if (wantAnimated && GetAnimatedRadarMap())
         return true;

     // could not get animated radar maps so try to find a static map instead
     if (!GetStaticRadarMap())
        return false;

     return true;
}

bool Weather::GetStaticRadarMap()
{
    QString weatherMapLink1URL = QString("http://www.weather.com/weather/map/%1"
                                    "?from=LAPmaps&setcookie=1 HTTP/1.1\n"
                                    "Connection: close\nHost: www.weather.com\n\n\n")
                                    .arg(locale);

    QString tempData = HttpComms::getHttp(weatherMapLink1URL, weatherTimeoutInt, 3, 3);

    QString mapLoc = parseData(tempData, "if (isMinNS4) var mapNURL = \"", "\";");
    if (mapLoc == "<NULL>")
        return true;
     
    QString weatherMapLink2URL = "http://www.weather.com/" + mapLoc;
     
    if (debug)
        cerr << "MythWeather: Grabbing Weather Map Link (part 2) From: " 
             << weatherMapLink2URL << endl;
    
    VERBOSE(VB_NETWORK, QString("Grabbing weather map(2) from: %1").arg(weatherMapLink2URL));
              
    QString tempData2 = HttpComms::getHttp(weatherMapLink2URL, weatherTimeoutInt, 3, 3);

    QString imageLoc = parseData(tempData2, "<IMG NAME=\"mapImg\" SRC=\"http://image.weather.com", "\"");
    if (imageLoc == "<NULL>")
    {
        VERBOSE(VB_IMPORTANT, "MythWeather: Warning: Failed to find link to map image.");
        return false;
    }

    QString fileprefix = MythContext::GetConfDir();

    QDir dir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);

    fileprefix += "/MythWeather";

    dir = QDir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);

    if (debug)
        cerr << "MythWeather: Map File Prefix: " << fileprefix << endl;

    if (debug)
        cerr << "MythWeather: Copying Map File from Server (" << imageLoc << ")...";
    
    VERBOSE(VB_NETWORK, QString("MythWeather: Copying map file from server (%1)").arg(imageLoc));
     
    QString sURL("http://image.weather.com/" + imageLoc);
    QString sFilename(fileprefix + "radar.jpg");

    if (!HttpComms::getHttpFile(sFilename, sURL, weatherTimeoutInt))
        cerr << "Failed to download image from:" << sURL << endl;

    if (debug)
        cerr << "Done.\n";

    LayerSet *container = theme->GetSet("weatherpages");
    if (container)
    {
        UIImageType *type = (UIImageType *)container->GetType("radarimg");
        if (type)
        {
            if (debug)
                cerr << "MythWeather: Full path to radar image: " << sFilename << endl;
            
            type->SetImage(sFilename);
            type->LoadImage();
        }
    }
    
    return true;
}

bool Weather::GetAnimatedRadarMap()
{
     if (AnimatedImage == NULL)
         return false;

     // find radar maps url's
     QString sURL = "http://www.weather.com/weather/map/" + locale +
        "?name=index_large_animated&day=1";
     QString tempData = "";

     if (debug)
        cerr << "MythWeather: Grabbing Weather Map Link (part 1) From: "
              << sURL << endl;
     tempData = HttpComms::getHttp(sURL, weatherTimeoutInt, 3, 3);

     QString mapLoc = parseData(tempData, "if (isMinNS4) var mapNURL = \"", "\";");
     if (mapLoc == "<NULL>")
         return false;

     mapLoc = "http://www.weather.com/" + mapLoc;
     if (debug)
        cerr << "MythWeather: Grabbing Weather Map Link (part 2) From: "
           << mapLoc << endl;

     tempData = HttpComms::getHttp(mapLoc, weatherTimeoutInt, 3, 3);

     QString imageLoc = parseData(tempData, "var thisMap = ['", "']");
     if (imageLoc == "<NULL>")
     {
         if (debug)
             cerr << "MythWeather: Warning: Failed to find link to map image.\n";

         return false;
     }

     QString fileprefix = MythContext::GetConfDir();

     QDir dir(fileprefix);
     if (!dir.exists())
         dir.mkdir(fileprefix);

     fileprefix += "/MythWeather";

     dir = QDir(fileprefix);
     if (!dir.exists())
         dir.mkdir(fileprefix);

     if (debug)
         cerr << "MythWeather: Map File Prefix: " << fileprefix << endl;

     // delete existing radar maps
     for (int x = 1; x <= 6; x++)
         QFile::remove(QString(fileprefix + "/radar%1.jpg").arg(x));

     if (debug)
         cerr << "MythWeather: Copying Map Files from Server (" << imageLoc << ")...\n";

     for (int x = 1; x <= 6; x++)
     {
         QString sFile = QString(fileprefix + "/radar%1.jpg").arg(x);
         sURL = QString("http://image.weather.com" + imageLoc + "%1L.jpg").arg(x);
         if (!HttpComms::getHttpFile(sFile, sURL, weatherTimeoutInt))
             cerr << "Failed to download image from:" << sURL << endl;
     }

     if (debug)
         cerr << "MythWeather: Download radar images done.\n";

     if (AnimatedImage)
     {
         AnimatedImage->SetFilename(fileprefix + "/radar%1.jpg");
         AnimatedImage->LoadImages();
     }
     return true;
}

QString Weather::parseData(QString data, QString beg, QString end)
{
    QString ret;

    if (debug == true)
    {
        cout << "MythWeather: Parse HTML : Looking for: " << beg << ", ending with: " << end << endl;
        if (data.length() == 0)
        {
            VERBOSE(VB_IMPORTANT, "MythWeather: Parse HTML: No data!");
            ret = "<NULL>";
            return ret;
        }
    }
    
    int start = data.find(beg) + beg.length();
    int endint = data.find(end, start + 1);
    if (start != -1 && endint != -1)
    {
        ret = data.mid(start, endint - start);
        if (debug == true)
            cout << "MythWeather: Parse HTML : Returning : " << ret << endl;
        return ret;
    }
    else
    {
        if (debug == true)
            VERBOSE(VB_IMPORTANT, "MythWeather: Parse HTML: Parse Failed...returning <NULL>");
        ret = "<NULL>";
        return ret;
    }
}

void Weather::weatherTimeout()
{
     stopProcessing = true;
     return;
}


