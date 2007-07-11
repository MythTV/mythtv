

#ifndef _WEATHERSCREEN_H_
#define _WEATHERSCREEN_H_

#include <qobject.h>
#include <qstringlist.h>
#include <qstring.h>
#include <qmap.h>
#include <qpainter.h>
#include "defs.h"
#include <mythtv/uitypes.h>

using namespace std;
class Weather;

class WeatherScreen : public QObject {
    Q_OBJECT
    public:
        static WeatherScreen* loadScreen(Weather*, LayerSet*, int id = -1);
        static QStringList getAllDynamicTypes(LayerSet*);

        WeatherScreen(Weather*, LayerSet*, int );
        ~WeatherScreen();
        void setValue(QString, QString); 
        QString getValue(QString key) { return map[key]; };
        bool containsKey(QString key) { return map.contains(key); };
        virtual bool canShowScreen(); 
        void pause_animation();
        void unpause_animation();
        virtual void hiding();
        virtual void showing();
        void addDataItem(QString, bool required=false);
        void setUnits(units_t units) { m_units = units; };
        units_t getUnits() { return m_units; };
        LayerSet* getContainer() { return m_container; };
        void draw(QPainter*);
        virtual void toggle_pause(bool);
        virtual bool usingKeys() { return false; }; 
        bool inUse() { return m_inuse; };
        void setInUse(bool inuse) { m_inuse = inuse; };
        int getId() { return m_id; };
    signals:
        void screenReady(WeatherScreen*);
    public slots:
        virtual void clock_tick();
        virtual void newData(QString, units_t,  DataMap);
        virtual bool handleKey(QKeyEvent* e ) { (void) e; return false; };
        
    protected:
        units_t m_units;
        LayerSet* m_container;
        virtual QString prepareDataItem(QString, QString);
        virtual void prepareWidget(UIType*);
        virtual void prepareScreen();
        UIType* getType(QString);
        Weather* m_parent;
        QString m_name;
    private:
        QRect fullRect;
        QMap<QString,QString> map;
        UIAnimatedImageType* m_ai;
        bool m_inuse;
        int m_id;
};

class CurrCondScreen : public WeatherScreen {
    Q_OBJECT
    public:
        CurrCondScreen(Weather*, LayerSet*, int id);      
    protected:
        virtual QString prepareDataItem(QString, QString);

};

class ThreeDayForecastScreen : public WeatherScreen {
    Q_OBJECT
    public:
        ThreeDayForecastScreen(Weather*, LayerSet*, int id);
};


class SevereWeatherScreen : public WeatherScreen {
    Q_OBJECT
    public:
        SevereWeatherScreen(Weather*, LayerSet*, int id);
        bool usingKeys() { return true; }; 
    public slots:
        bool handleKey(QKeyEvent* e); 

    private:
        UIRichTextType* m_text;


};

class StaticImageScreen : public WeatherScreen {
    Q_OBJECT
    public:
        StaticImageScreen(Weather*, LayerSet*, int id);
    protected:
        QString prepareDataItem(QString,QString);
        void prepareWidget(UIType*);
    private:
        QSize imgsize;
        QSize max; // should match staticsize in weather-ui.xml
        QPoint orgPos; //so the image doesn't walk across the screen

};

class AnimatedImageScreen : public WeatherScreen {
    Q_OBJECT
    public:
        AnimatedImageScreen(Weather*, LayerSet*, int id);
    protected:
        QString prepareDataItem(QString,QString);
        void prepareWidget(UIType*);
    private:
        int m_count;
        QSize imgsize;
        QSize max; // should match staticsize in weather-ui.xml
        QPoint orgPos; //so the image doesn't walk across the screen

};
#endif
