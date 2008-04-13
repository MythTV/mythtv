#ifndef WEATHER_H_
#define WEATHER_H_

// QT headers
//Added by qt3to4:
#include <Q3PtrList>

// MythTV headers
#include <mythtv/libmythui/mythscreentype.h>
#include <mythtv/libmythui/mythuitext.h>
#include <mythtv/libmythui/mythmainwindow.h>

// MythWeather headers
#include "weatherUtils.h"

class SourceManager;
class WeatherScreen;

class Weather : public MythScreenType
{
    Q_OBJECT

  public:
    Weather(MythScreenStack *parent, const char *name, SourceManager *srcMan);
    ~Weather();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);

    bool UpdateData();

  private slots:
    void update_timeout() {}
    void nextpage_timeout();
    void weatherTimeout() {}
    void cursorLeft();
    void cursorRight();
    void holdPage();
    void setupPage();
    void screenReady(WeatherScreen *ws);

  private:
    void setupScreens();
    WeatherScreen *nextScreen();
    WeatherScreen *prevScreen();
    void showScreen(WeatherScreen *ws);
    void hideScreen(void);

    MythScreenStack *m_mainStack;
    MythScreenStack *m_weatherStack;
    int m_timeoutCounter;
    int m_wantAnimated;
    bool m_stopProcessing;

    units_t m_units;
    bool m_firstRun;
    int m_updateInterval;
    int m_nextpageInterval;
    int m_nextpageIntArrow;
    QTimer *m_nextpage_Timer;

    bool m_gotDataHook;

    SourceManager *m_srcMan;
    Q3PtrList<WeatherScreen> m_screens; //screens in correct display order

    ScreenListMap m_allScreens; //screens parsed from xml
    WeatherScreen *m_currScreen;
    bool m_paused;

    MythUIText *m_pauseText;
    MythUIText *m_headerText;
};

#endif
