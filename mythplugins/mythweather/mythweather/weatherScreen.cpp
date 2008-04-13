// QT headers
#include <vector>

// MythTV headers
#include <mythtv/mythcontext.h>

// MythWeather headers
#include "weather.h"
#include "weatherScreen.h"

WeatherScreen *WeatherScreen::loadScreen(MythScreenStack *parent,
                                         ScreenListInfo *screenDefn, int id)
{
    QString key = screenDefn->name;
    if (key == "Current Conditions")
        return new CurrCondScreen(parent, "Current Conditions", screenDefn, id);
    if (key == "Three Day Forecast")
        return new ThreeDayForecastScreen(parent,"Three Day Forecast" , screenDefn, id);
    if (key == "Six Day Forecast")
        return new SixDayForecastScreen(parent, "Six Day Forecast", screenDefn, id);
    if (key == "Severe Weather Alerts")
        return new SevereWeatherScreen(parent, "Severe Weather Alerts", screenDefn, id);
    if (key == "Static Map")
        return new StaticImageScreen(parent,"Static Map" , screenDefn, id);
    if (key == "Animated Map")
        return new AnimatedImageScreen(parent, "Animated Map", screenDefn, id);

    return new WeatherScreen(parent, "Unknown Screen", screenDefn, id);
}

WeatherScreen::WeatherScreen(MythScreenStack *parent, const char *name,
                             ScreenListInfo *screenDefn, int id)
    : MythScreenType (parent, name)
{
    m_screenDefn = screenDefn;
    m_name = m_screenDefn->name;
    m_id = id;
    m_prepared = false;

    m_inuse = false;

    QStringList types = m_screenDefn->dataTypes;

    for (int i = 0; i < types.size(); ++i)
    {
        m_dataValueMap[types.at(i)] =  "";
    }
}

WeatherScreen::~WeatherScreen()
{
}

bool WeatherScreen::Create()
{

    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("weather-ui.xml", m_name, this);

    if (!foundtheme)
        return false;

    return true;
}

bool WeatherScreen::canShowScreen()
{
    if (!inUse())
        return false;

    bool ok = true;
    QMapIterator<QString, QString> i(m_dataValueMap);
    while (i.hasNext()) {
        i.next();
        if (i.key().isEmpty())
        {
            VERBOSE(VB_GENERAL, i.key());
            ok = false;
        }
    }

    return ok;
}


void WeatherScreen::setValue(const QString &key, const QString &value)
{
    if (m_dataValueMap.contains(key))
        m_dataValueMap[key] = prepareDataItem(key, value);
}

void WeatherScreen::newData(QString loc, units_t units, DataMap data)
{
    (void) loc;
    (void) units;

    DataMap::iterator itr = data.begin();
    while (itr != data.end())
    {
        setValue(itr.key(), itr.data());
        ++itr;
    }

    if (!m_prepared)
        prepareScreen();

    emit screenReady(this);
}

void WeatherScreen::prepareScreen()
{
    QMap<QString, QString>::iterator itr = m_dataValueMap.begin();
    while (itr != m_dataValueMap.end())
    {
        MythUIType *widget = GetChild(itr.key());
        if (!widget)
        {
            VERBOSE(VB_GENERAL, "Widget not found " + itr.key());
            ++itr;
            continue;
        }

        if (dynamic_cast<MythUIText *>(widget))
        {
            ((MythUIText *) widget)->SetText(itr.value());
        }
        else if (dynamic_cast<MythUIImage *>(widget))
        {
            ((MythUIImage *) widget)->SetFilename(itr.value());
            ((MythUIImage *) widget)->Load();
        }

        prepareWidget(widget);
        ++itr;
    }

    m_prepared = true;
}

void WeatherScreen::prepareWidget(MythUIType *widget)
{
    MythUIImage *img;
    /*
     * Basically so we don't do it twice since some screens (Static Map) mess
     * with image dimensions
     */
    if ((img = dynamic_cast<MythUIImage *>(widget)))
    {
        img->Load();
    }
}

QString WeatherScreen::prepareDataItem(const QString &key, const QString &value)
{
    (void) key;
    return value;
}

bool WeatherScreen::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget() && GetFocusWidget()->keyPressEvent(event))
        return true;

    return false;
}

CurrCondScreen::CurrCondScreen(MythScreenStack *parent, const char *name,
                               ScreenListInfo *screenDefn, int id)
    : WeatherScreen(parent, name, screenDefn, id)
{
}

QString CurrCondScreen::prepareDataItem(const QString &key,
                                        const QString &value)
{
    if (key == "relative_humidity")
        return value + " %";

    if (key == "pressure")
        return value + (m_units == ENG_UNITS ? " in" : " mb");

    if (key == "visibility")
        return value + (m_units == ENG_UNITS ? " mi" : " km");

    if (key == "appt")
        return value == "NA" ? value : value + (m_units == ENG_UNITS ? "°F" : "°C");

    if (key == "temp")
    {
       if ( (value == "NA") || (value == "N/A") )
          return value;
       else
          return value + (m_units == ENG_UNITS ? "°F" : "°C");
    }

    if (key == "wind_gust" || key == "wind_spdgst" || key == "wind_speed")
        return value + (m_units == ENG_UNITS ? " mph" : " kph");

    return value;
}

ThreeDayForecastScreen::ThreeDayForecastScreen(MythScreenStack *parent,
                                               const char *name,
                                               ScreenListInfo *screenDefn, int id)
    : WeatherScreen(parent, name, screenDefn, id)
{
}

QString ThreeDayForecastScreen::prepareDataItem(const QString &key,
                                        const QString &value)
{
    if (key.contains("low",FALSE) || key.contains("high",FALSE) )
    {
       if ( (value == "NA") || (value == "N/A") )
          return value;
       else
          return value + (m_units == ENG_UNITS ? "°F" : "°C");
    }

    return value;
}

SixDayForecastScreen::SixDayForecastScreen(MythScreenStack *parent,
                                            const char *name,
                                            ScreenListInfo *screenDefn, int id)
    : WeatherScreen(parent, name, screenDefn, id)
{
}

QString SixDayForecastScreen::prepareDataItem(const QString &key,
                                        const QString &value)
{
    if (key.contains("low",FALSE) || key.contains("high",FALSE) )
    {
       if ( (value == "NA") || (value == "N/A") )
          return value;
       else
          return value + (m_units == ENG_UNITS ? "°F" : "°C");
    }

    return value;
}

SevereWeatherScreen::SevereWeatherScreen(MythScreenStack *parent, const char *name,
                                         ScreenListInfo *screenDefn, int id)
    : WeatherScreen(parent, name, screenDefn, id)
{
}

StaticImageScreen::StaticImageScreen(MythScreenStack *parent, const char *name,
                                     ScreenListInfo *screenDefn, int id)
    : WeatherScreen(parent, name, screenDefn, id)
{
}

QString StaticImageScreen::prepareDataItem(const QString &key,
                                           const QString &value)
{
    QString ret = value;
    if (key == "map")
    {
        /*
         * Image value format:
         * /path/to/file-WIDTHxHEIGHT
         * if no dimension, scale to max size
         */
        bool hasdim = (value.findRev('-') > value.findRev('/'));
        if (hasdim)
        {
            QStringList dim = QStringList::split('x',
                    value.right(value.length() - value.findRev('-') - 1));
            ret = value.left(value.findRev('-'));
            m_imgsize.setWidth(dim[0].toInt());
            m_imgsize.setHeight(dim[1].toInt());
        }
    }

    return ret;
}

void StaticImageScreen::prepareWidget(MythUIType *widget)
{
    if (widget->objectName() == "map")
    {
        /*
         * Scaling the image down and centering it
         */
        MythUIImage *img = (MythUIImage *) widget;
        QRect max = img->GetArea();

        if (m_imgsize.width() > -1 && m_imgsize.height() > -1)
        {
            if (max.width() > -1 && max.height() > -1)
            {
                m_imgsize.scale(QSize(max.width(),max.height()),
                                Qt::KeepAspectRatio);

                QPoint orgPos = img->GetPosition();

                int newx = orgPos.x() + (max.width() - m_imgsize.width()) / 2;
                int newy = orgPos.y() + (max.height() - m_imgsize.height()) / 2;
                img->SetPosition(QPoint(newx, newy));
            }
            img->SetSize(m_imgsize.width(), m_imgsize.height());
        }
        img->Load();
    }
}

AnimatedImageScreen::AnimatedImageScreen(MythScreenStack *parent, const char *name,
                                         ScreenListInfo *screenDefn, int id)
    : WeatherScreen(parent, name, screenDefn, id)
{
}

QString AnimatedImageScreen::prepareDataItem(const QString &key,
                                             const QString &value)
{
    QString ret = value;
    if (key == "animatedimage")
    {
        /*
         * Image value format:
         * /path/to/file-IMAGECOUNT-WIDTHxHEIGHT
         * if no dimension, scale to max size
         */

        bool hasdim = value.find(QRegExp("-[0-9]{1,}x[0-9]{1,}$"));
        if (hasdim)
        {
            QStringList dim = QStringList::split('x',
                    value.right(value.length() - value.findRev('-') - 1));
            ret = value.left(value.findRev('-'));
            m_imgsize.setWidth(dim[0].toInt());
            m_imgsize.setHeight(dim[1].toInt());
        }

        QString cnt = ret.right(ret.length() - ret.findRev('-') - 1);
        m_count = cnt.toInt();
        ret = ret.left(ret.findRev('-'));
    }

    return ret;
}

void AnimatedImageScreen::prepareWidget(MythUIType *widget)
{
    if (widget->objectName() == "animatedimage")
    {
        /*
         * Scaling the image down and centering it
         */
        MythUIImage *img = (MythUIImage *) widget;
        QRect max = img->GetArea();

        if (m_imgsize.width() > -1 && m_imgsize.height() > -1)
        {
            if (max.width() > -1 && max.height() > -1)
            {
                m_imgsize.scale(QSize(max.width(),max.height()),
                                Qt::KeepAspectRatio);

                QPoint orgPos = img->GetPosition();

                int newx = orgPos.x() + (max.width() - m_imgsize.width()) / 2;
                int newy = orgPos.y() + (max.height() - m_imgsize.height()) / 2;
                img->SetPosition(QPoint(newx, newy));
            }
            img->SetSize(m_imgsize.width(), m_imgsize.height());
        }

        img->SetImageCount(0, m_count);
        // TODO this slows things down A LOT!!
        img->Load();
    }
    return;
}
