#ifndef _WEATHERSCREEN_H_
#define _WEATHERSCREEN_H_

#include <qobject.h>
#include <qstringlist.h>
#include <qstring.h>
#include <qmap.h>
//Added by qt3to4:
#include <QKeyEvent>

#include <mythtv/uitypes.h>

#include "defs.h"

class Weather;

class WeatherScreen : public QObject
{
    Q_OBJECT

  public:
    static WeatherScreen *loadScreen(Weather *parent, LayerSet *container,
                                     int id = -1);
    static QStringList getAllDynamicTypes(LayerSet *container);

    WeatherScreen(Weather *parent, LayerSet *container, int id);
    ~WeatherScreen();

    void setValue(const QString &key, const QString &value);
    QString getValue(const QString &key) { return map[key]; }
    bool containsKey(const QString &key) { return map.contains(key); }
    virtual bool canShowScreen();
    void pause_animation();
    void unpause_animation();
    virtual void hiding();
    virtual void showing();
    void addDataItem(const QString &item, bool required = false);
    void setUnits(units_t units) { m_units = units; }
    units_t getUnits() { return m_units; }
    LayerSet *getContainer() { return m_container; }
    void draw(QPainter *p);
    virtual void toggle_pause(bool paused);
    virtual bool usingKeys() { return false; }
    bool inUse() { return m_inuse; }
    void setInUse(bool inuse) { m_inuse = inuse; }
    int getId() { return m_id; }

  signals:
    void screenReady(WeatherScreen *);

  public slots:
    virtual void clock_tick();
    virtual void newData(QString loc, units_t units, DataMap data);
    virtual bool handleKey(QKeyEvent *e) { (void) e; return false; }

  protected:
    units_t m_units;
    LayerSet *m_container;
    Weather *m_parent;
    QString m_name;

  protected:
    virtual QString prepareDataItem(const QString &key, const QString &value);
    virtual void prepareWidget(UIType *widget);
    virtual void prepareScreen();
    UIType *getType(const QString &key);

  private:
    QRect fullRect;
    QMap<QString, QString> map;
    UIAnimatedImageType *m_ai;
    bool m_inuse;
    bool m_prepared;
    int m_id;
};

class CurrCondScreen : public WeatherScreen
{
    Q_OBJECT

  public:
    CurrCondScreen(Weather *parent, LayerSet *container, int id);

  protected:
    virtual QString prepareDataItem(const QString &key, const QString &value);
};

class ThreeDayForecastScreen : public WeatherScreen
{
    Q_OBJECT

  public:
    ThreeDayForecastScreen(Weather *parent, LayerSet *container, int id);
    
  protected:
    virtual QString prepareDataItem(const QString &key, const QString &value);

};

class SixDayForecastScreen : public WeatherScreen
{
    Q_OBJECT

  public:
    SixDayForecastScreen(Weather *parent, LayerSet *container, int id);
    
  protected:
    virtual QString prepareDataItem(const QString &key, const QString &value);

};

class SevereWeatherScreen : public WeatherScreen
{
    Q_OBJECT

  public:
    SevereWeatherScreen(Weather *parent, LayerSet *container, int id);
    bool usingKeys() { return true; }

  public slots:
    bool handleKey(QKeyEvent *e);

  private:
    UIRichTextType *m_text;
};

class StaticImageScreen : public WeatherScreen
{
    Q_OBJECT

  public:
    StaticImageScreen(Weather *parent, LayerSet *container, int id);

  protected:
    QString prepareDataItem(const QString &key, const QString &value);
    void prepareWidget(UIType *widget);

  private:
    QSize imgsize;
};

class AnimatedImageScreen : public WeatherScreen
{
    Q_OBJECT

  public:
    AnimatedImageScreen(Weather *parent, LayerSet *container, int id);

  protected:
    QString prepareDataItem(const QString &key, const QString &value);
    void prepareWidget(UIType *widget);

  private:
    int m_count;
    QSize imgsize;
};

#endif
