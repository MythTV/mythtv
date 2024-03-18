#ifndef WEATHERSCREEN_H
#define WEATHERSCREEN_H

// QT headers
#include <QMap>
#include <QStringList>

// MythTV headers
#include <libmythui/mythscreentype.h>
#include <libmythui/mythuiimage.h>
#include <libmythui/mythuitext.h>

// MythWeather headers
#include "weatherUtils.h"

class Weather;

enum DaysOfWeek : std::uint8_t {
    DAY_SUNDAY, DAY_MONDAY, DAY_TUESDAY, DAY_WENDESDAY, DAY_THURSDAY,
    DAY_FRIDAY, DAY_SATURDAY
};

/** \class WeatherScreen
 *  \brief Weather screen
 */
class WeatherScreen : public MythScreenType
{
    Q_OBJECT

  public:
    WeatherScreen(MythScreenStack *parent, ScreenListInfo *screenDefn, int id);
    ~WeatherScreen() override = default;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType

    static WeatherScreen *loadScreen(MythScreenStack *parent,
                                         ScreenListInfo *screenDefn, int id);

    void setValue(const QString &key, const QString &value);
    QString getValue(const QString &key) { return m_dataValueMap[key]; }
    bool containsKey(const QString &key) { return m_dataValueMap.contains(key); }
    virtual bool canShowScreen();
    void setUnits(units_t units) { m_units = units; }
    units_t getUnits() const { return m_units; }
    virtual bool usingKeys() { return false; }
    bool inUse() const { return m_inuse; }
    void setInUse(bool inuse) { m_inuse = inuse; }
    int getId() const { return m_id; }

  signals:
    void screenReady(WeatherScreen *);

  public slots:
    virtual void newData(const QString& /*loc*/, units_t /*units*/, DataMap data);

  protected:
    units_t         m_units      {SI_UNITS};
    ScreenListInfo *m_screenDefn {nullptr};
    QString         m_name;

  protected:
    virtual QString prepareDataItem(const QString &key, const QString &value);
    virtual void prepareWidget(MythUIType *widget);
    virtual bool prepareScreen(bool checkOnly = false);
    virtual QString getTemperatureUnit();
    QString formatDataItem(const QString &key, const QString &value);

  private:
    QMap<QString, QString> m_dataValueMap;

    bool m_inuse    {false};
    bool m_prepared {false};
    int  m_id;
};

#endif // WEATHERSCREEN_H
