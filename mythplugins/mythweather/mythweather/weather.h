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
#include <qsocket.h>
#include <qwidget.h>
#include <qdialog.h>
#include <qstringlist.h>
#include <qlayout.h>

#include <mythtv/mythwidgets.h>

class QLabel;
class QListView;
class MythContext;

struct weatherTypes {
	int typeNum;
	QString typeName;
	QString typeIcon;
};

class Weather : public MythDialog
{
    Q_OBJECT
  public:
    Weather(MythContext *context,
	    QWidget *parent = 0, const char *name = 0);

    void Show();
  
    void UpdateData();

private slots:
    void update_timeout();
    void showtime_timeout();
    void nextpage_timeout();
    void status_timeout();
    void cursorLeft();
    void cursorRight();
    void holdPage();
    void convertFlip();
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


    void closeConnection();
    void socketReadyRead();
    void socketConnected();
    void socketConnectionClosed();
    void socketClosed();
    void socketError( int e );


  private:
    QAccel *accel;

    bool readReadme;
    bool convertData;
    int updateInterval;
    int nextpageInterval;
    int nextpageIntArrow;

    QString newLocaleHold;
    QString baseDir;

    int con_attempt;
    QTimer *nextpage_Timer;
    QTimer *status_Timer;
    QSocket *httpSock;
    void fillList();
    void GetWeatherData();
    bool gotDataHook;
    void setWeatherTypeIcon(QString[]);
    void setWeatherIcon(QString);

    QString GetString(QString);
    int GetInt(QString);
    float GetFloat(QString);

    MythContext *m_context;

    void loadWeatherTypes();
    weatherTypes *wData;
  
    void firstLayout();
    void lastLayout();
    void setupLayout(int);
    void showLayout(int);

    QFrame *page1Dia;
    QFrame *page2Dia;
    QFrame *page3Dia;
    QFrame *page4Dia;
 
    QVBoxLayout *mid;
    QVBoxLayout *main;
    QHBoxLayout *page1;
    QHBoxLayout *page2;
    QHBoxLayout *page3;
    QHBoxLayout *page4;

    int currentPage;

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

    QLabel *lbPic1;
    QLabel *lbPic2;
    QLabel *lbPic3;
    QLabel *lbPic4;
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
};


/*

function makeWeatherObj() { this.swCity = "Cincinnati"; this.swSubDiv = "OH"; this.swCountry = "USA"; this.swRegion = "United States"; this.swTemp = "37"; this.swTempCel = Math.round((5/9)*(this.swTemp-32)); this.swCIcon = "26"; this.swWindS = "12"; this.swWindD = "W"; this.swBaro = "30.06"; this.swHumid = "89"; this.swReal = "29"; this.swUV = "0"; this.swVis = "6.00"; this.swLastUp = "01/05/2003 22:53:00"; this.swConText = "Cloudy"; this.swFore = "2|3|4|5|6|01/06/2003|01/07/2003|01/08/2003|01/09/2003|01/10/2003|

---14|34|24|24

|24|98|22|35|33|37|

---35|40|44|50

|32|60|0|10|10|10|29|27|27|29|27|42|3|3|42|3|

---22|32|35|30

|22|"; this.swAcid = "USOH0188"; }

*/
#endif
