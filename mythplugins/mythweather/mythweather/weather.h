/*
        MythWeather
        Version 0.8
        January 8th, 2003

        By John Danner & Dustin Doris

        Note: Portions of this code taken from MythMusic

*/


#ifndef WEATHER_H_
#define WEATHER_H_

#include <qsqldatabase.h>
#include <qwidget.h>
#include <qdialog.h>
#include <qstringlist.h>
#include <qlayout.h>
#include <fstream>

#include <mythtv/oldsettings.h>
#include <mythtv/mythwidgets.h>

class QLabel;
class QListView;
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
    Weather(QSqlDatabase *db, QWidget *parent = 0, const char *name = 0);
    ~Weather();

    bool UpdateData();
    void processEvents();
    QString getLocation();

private slots:
    void update_timeout();
    void showtime_timeout();
    void nextpage_timeout();
    void status_timeout();
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
    void newLocale0();
    void newLocale1();
    void newLocale2();
    void newLocale3();
    void newLocale4();
    void newLocale5();
    void newLocale6();
    void newLocale7();
    void newLocale8();
    void newLocale9();


  private:
    QSqlDatabase *config;
    QAccel *accel;

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
    QString baseDir;

    int con_attempt;
    QTimer *nextpage_Timer;
    QTimer *update_Timer;
    QTimer *status_Timer;

    void saveConfig();
    QString findAccidbyName(QString);
    QString findNamebyAccid(QString);
    void loadCityData(int);
    void fillList();
    void updateLetters();
    void loadAccidBreaks();
    bool GetWeatherData();
    bool gotDataHook;
    void setWeatherTypeIcon(QString[]);
    void setWeatherIcon(QString);
    void backupCity(int);
    void updateAggr();
    void showCityName();
    void setSetting(QString, QString, bool);
    //void loadCityLetter(int);

    QString GetString(QString);
    int GetInt(QString);
    float GetFloat(QString);

    void loadWeatherTypes();
    weatherTypes *wData;
  
    void firstLayout();
    void lastLayout();
    void setupLayout(int);
    void showLayout(int);
    void setupColorScheme();

    QFrame *page0Dia;
    QFrame *page1Dia;
    QFrame *page2Dia;
    QFrame *page3Dia;
    QFrame *page4Dia;
    QFrame *page5Dia;

    QFrame *unitType;
    QFrame *location;
    QFrame *aggressv;
 
    QVBoxLayout *mid;
    QVBoxLayout *main;
    QHBoxLayout *page0; 
    QHBoxLayout *page1;
    QHBoxLayout *page2;
    QHBoxLayout *page3;
    QHBoxLayout *page4;
    QHBoxLayout *page5;

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

    QLabel *letterList;
    QLabel *cityList;
    QLabel *aggrNum;
    QLabel *ImpUnits;
    QLabel *SIUnits;
    QLabel *lbUnits;
    QLabel *lbLocal;
    QLabel *lbAggr;
    QLabel *lbPic0;
    QLabel *lbPic1;
    QLabel *lbPic2;
    QLabel *lbPic3;
    QLabel *lbPic4;
    QLabel *lbPic5;
    QLabel *date1;
    QLabel *date2;
    QLabel *date3;
    QLabel *desc1;
    QLabel *desc2;
    QLabel *desc3;
    QLabel *high1;
    QLabel *high2;
    QLabel *high3;
    QLabel *low1;
    QLabel *low2;
    QLabel *low3;
    QLabel *prec1;
    QLabel *prec2;
    QLabel *prec3;
    QLabel *lbTemp;
    QLabel *tempType;
    QLabel *lbCond;
    QLabel *lbDesc;
    QLabel *lbTDesc;
    QLabel *lbHumid;
    QLabel *lbPress;
    QLabel *lbWind;
    QLabel *lbVisi;
    QLabel *lbWindC;
    QLabel *lbUVIndex;
    QLabel *lbLocale;
    QLabel *lbStatus;
    QLabel *lbUpdated;
    QLabel *hdPart1;
    QLabel *hdPart2;
    QLabel *dtime;

    QString httpData;
    QString oldhttpData;

    QColor topbot_bgColor;
    QColor topbot_fgColor;
    QColor main_bgColor;
    QColor main_fgColor;
    QColor lohi_fgColor;
};


#endif
