/*
        MythWeather
        Version 0.8
        January 8th, 2003

        By John Danner & Dustin Doris

        Note: Portions of this code taken from MythMusic

*/


#ifndef WEATHER_H_
#define WEATHER_H_

#include <qwidget.h>
#include <qdialog.h>
#include <qstringlist.h>
#include <qlayout.h>
#include <fstream>
#include <qnetwork.h>
#include <qurl.h>

#include <mythtv/uitypes.h>
#include <mythtv/xmlparse.h>
#include <mythtv/oldsettings.h>
#include <mythtv/mythwidgets.h>
#include <mythtv/mythdialogs.h>
#include "weatherSource.h"
#include "sourceManager.h"
#include "weatherScreen.h"
#include "defs.h"

class QLabel;
class QListView;
class QNetworkOperation;
class Settings;
class WeatherSock;
class WeatherSource;
class SourceManager;

struct weatherTypes {
	int typeNum;
	QString typeName;
	QString typeIcon;
};


class Weather : public MythDialog
{
    Q_OBJECT
  public:
    Weather(MythMainWindow *parent, SourceManager*, const char *name = 0);
    ~Weather();

    bool UpdateData();
    void processEvents();
    QPixmap* getBackground() { return &realBackground; };
  signals:
    void clock_tick();
    
  private slots:
    void update_timeout();
    void showtime_timeout();
    void nextpage_timeout();
    void weatherTimeout();
    void cursorLeft();
    void cursorRight();
    void holdPage();
    void setupPage();
    void screenReady(WeatherScreen*);
  protected:
    void paintEvent(QPaintEvent *);
    void keyPressEvent(QKeyEvent *e);

  private:
    void setupScreens(QDomElement&);
    WeatherScreen* nextScreen();
    WeatherScreen* prevScreen();
    int timeoutCounter;
    int wantAnimated;
    bool stopProcessing;
    QString parseData(QString data, QString beg, QString end);
    void LoadWindow(QDomElement &);
    XMLParse *theme;
    QDomElement xmldata;

    void updateBackground();
    void updatePage(QPainter *);

    units_t m_units;
    bool firstRun;
    int updateInterval;
    int nextpageInterval;
    int nextpageIntArrow;
    QTimer *showtime_Timer;
    QTimer *nextpage_Timer;

    QString findAccidbyName(QString);
    QString findNamebyAccid(QString);
    void loadCityData(int);
    void fillList();
    void loadAccidBreaks();
    bool GetWeatherData();
    bool GetAnimatedRadarMap();
    bool GetStaticRadarMap();
    bool gotDataHook;
    void setWeatherTypeIcon(QString[]);
    void setWeatherIcon(QString);
    //void backupCity(int);
    void setSetting(QString, QString, bool);

    QString GetString(QString);
    int GetInt(QString);
    float GetFloat(QString);

    void loadWeatherTypes();
  
    void showLayout(WeatherScreen*);
    QRect newlocRect;
    QRect fullRect;

    QPixmap realBackground;
    bool allowkeys;  
    SourceManager *m_srcMan;
    QPtrList<WeatherScreen> screens; //screens in correct display order
    /*
     * May not be necessary, but we will keep instances around, they may be
     * helpful when doing configuration. though if I split out config to
     * different classes then these may not be necessary
     * 
     * Maybe I'll do something like I did with sources, having instance
     * indpendent info that I load when parsing xml, then creating objects as
     * necessary.
     * UPDATE: Things are currently between two places, either having only one
     * possible instance of a screen, or else having multiple instances, which
     * would require somehow creating multiple LaySet objects, probably possibly
     * by keeping a map of QDomElement's instead of Screens, and parsing them
     * for each screen required.
     */
    QMap<QString, WeatherScreen*> all_screens; //screens parsed from xml
    WeatherScreen* currScreen;
    WeatherScreen* m_startup;
    bool paused;
};

#endif
