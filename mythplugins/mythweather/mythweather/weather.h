#ifndef WEATHER_H_
#define WEATHER_H_

// QT headers
#include <QList>

// MythTV headers
#include <mythscreentype.h>
#include <mythuitext.h>
#include <mythmainwindow.h>

// MythWeather headers
#include "weatherUtils.h"

class SourceManager;
class WeatherScreen;

using ScreenList = QList<WeatherScreen*>;

class Weather : public MythScreenType
{
    Q_OBJECT

  public:
    Weather(MythScreenStack *parent, const QString &name, SourceManager *srcMan);
    ~Weather();

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *) override; // MythScreenType

    bool UpdateData();
    bool SetupScreens();

  public slots:
    void setupScreens();

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
    WeatherScreen *nextScreen();
    WeatherScreen *prevScreen();
    void clearScreens();
    void showScreen(WeatherScreen *ws);
    void hideScreen(void);

    MythScreenStack *m_weatherStack {nullptr};

    bool    m_firstRun              {true};
    int     m_nextpageInterval      {10};

    QTimer *m_nextpage_Timer        {nullptr};

    bool    m_firstSetup            {true};

    bool    m_createdSrcMan         {false};
    SourceManager *m_srcMan         {nullptr};
    ScreenList m_screens; //screens in correct display order
    int        m_cur_screen         {0};

    ScreenListMap m_allScreens; //screens parsed from xml
    WeatherScreen *m_currScreen     {nullptr};
    bool m_paused                   {false};

    MythUIText *m_pauseText         {nullptr};
    MythUIText *m_headerText        {nullptr};
    MythUIText *m_updatedText       {nullptr};
};

#endif

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
