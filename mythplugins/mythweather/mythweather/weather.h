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

class QLabel;
class QListView;
class QNetworkOperation;
class Settings;
class WeatherSock;

struct weatherTypes {
	int typeNum;
	QString typeName;
	QString typeIcon;
};

class Weather : public MythDialog
{
    Q_OBJECT
  public:
    Weather(int appCode, MythMainWindow *parent, const char *name = 0);
    ~Weather();

    bool UpdateData();
    void processEvents();
    QString getLocation();
    void setLocation(QString newLocale);
  private slots:
    void update_timeout();
    void showtime_timeout();
    void nextpage_timeout();
    void weatherTimeout();
    void cursorLeft();
    void cursorRight();
    void upKey();
    void dnKey();
    void pgupKey();
    void pgdnKey();
    void holdPage();
    void setupPage();
    void convertFlip();
    void resetLocale();
    void newLocaleX(int);

  protected:
    void paintEvent(QPaintEvent *);
    void keyPressEvent(QKeyEvent *e);

  private:
    int timeoutCounter;
    int wantAnimated;
    bool stopProcessing;
    QString parseData(QString data, QString beg, QString end);
    void LoadWindow(QDomElement &);
    void parseContainer(QDomElement &);
    XMLParse *theme;
    QDomElement xmldata;
    UIAnimatedImageType* AnimatedImage;

    void SetText(LayerSet *, QString, QString);

    void updateBackground();
    void updatePage(QPainter *);

    ifstream accidFile;
    streampos startData;
    streampos curPos;
    long accidBreaks[52];
    int prevPos;

    bool noACCID;
    bool changeTemp;
    bool changeLoc;
    bool changeAgg;
    int config_Units;
    int config_Aggressiveness;
    int curConfig;
    int curPage;
    bool debug;
    bool deepSetup;
    bool gotLetter;
    bool inSetup;
    bool validArea;
    bool readReadme;
    bool pastTime;
    bool convertData;
    bool firstRun;
    bool conError;
    int updateInterval;
    int nextpageInterval;
    int nextpageIntArrow;
    int lastCityNum;
    int curLetter;
    int curCity;

    QString cityNames[9];
    QString newLocaleHold;
    QString cfgCity;

    int con_attempt;
    QTimer *nextpage_Timer;
    QTimer *update_Timer;
    QTimer *urlTimer;

    void saveConfig();
    QString findAccidbyName(QString);
    QString findNamebyAccid(QString);
    void loadCityData(int);
    void fillList();
    void updateLetters();
    void loadAccidBreaks();
    bool GetWeatherData();
    bool GetAnimatedRadarMap();
    bool GetStaticRadarMap();
    bool gotDataHook;
    void setWeatherTypeIcon(QString[]);
    void setWeatherIcon(QString);
    void backupCity(int);
    void updateAggr();
    void showCityName();
    void setSetting(QString, QString, bool);

    QString GetString(QString);
    int GetInt(QString);
    float GetFloat(QString);

    void loadWeatherTypes();
    weatherTypes *wData;
  
    void showLayout(int);

    int currentPage;

    QString config_Location;
    QString locale;
    QString city;
    QString state;
    QString country;
    QString measure;
    QString curTemp;
    QString curIcon;
    QString curWind;
    QString winddir;
    QString barometer;  
    QString curHumid;
    QString curFeel;
    QString uvIndex;
    QString visibility;
    QString updated;
    QString description;
    QString date[5];
    QString weatherIcon[5];
    QString weatherType[5];
    QString highTemp[5];
    QString lowTemp[5];
    QString precip[5];

    QString httpData;
    QString oldhttpData;

    QRect fullRect;
    QRect newlocRect;

    QPixmap realBackground;
    bool allowkeys;  
    
    int weatherTimeoutInt;
};


#endif
