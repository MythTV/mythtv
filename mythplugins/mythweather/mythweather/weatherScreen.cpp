// C++ headers
#include <vector>
using namespace std;

// MythTV headers
#include <mythcontext.h>

// MythWeather headers
#include "weather.h"
#include "weatherScreen.h"

WeatherScreen *WeatherScreen::loadScreen(MythScreenStack *parent,
                                         ScreenListInfo *screenDefn, int id)
{
    return new WeatherScreen(parent, screenDefn, id);
}

WeatherScreen::WeatherScreen(MythScreenStack *parent,
                             ScreenListInfo *screenDefn, int id) :
               MythScreenType (parent, screenDefn->title),
    m_screenDefn(screenDefn),
    m_name(m_screenDefn->name),
    m_inuse(false),
    m_prepared(false),
    m_id(id)
{

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

    if (!prepareScreen(true))
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
        setValue(itr.key(), *itr);
        ++itr;
    }

    // This may seem like overkill, but it is necessary to actually update the
    // static and animated maps when they are redownloaded on an update
    if (!prepareScreen())
        VERBOSE(VB_IMPORTANT, "Theme is missing a required widget!");

    emit screenReady(this);
}

QString WeatherScreen::getTemperatureUnit()
{
    if (m_units == ENG_UNITS)
        return QString::fromUtf8("°") + "F";
    else
        return QString::fromUtf8("°") + "C";
}

bool WeatherScreen::prepareScreen(bool checkOnly)
{
    QMap<QString, QString>::iterator itr = m_dataValueMap.begin();
    while (itr != m_dataValueMap.end())
    {
        QString name = itr.key();
        MythUIType *widget = GetChild(name);

        if (!widget)
        {
            VERBOSE(VB_GENERAL, "Widget not found " + itr.key());

            if (name == "copyright")
            {
                VERBOSE(VB_IMPORTANT, 
                    QString("No copyright widget found, skipping screen %1.")
                    .arg(m_name));
                return false;
            }
        }

        if( !widget || checkOnly )
        {
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
    return true;
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

QString WeatherScreen::formatDataItem(const QString &key, const QString &value)
{
    if (key == "relative_humidity")
        return value + " %";

    if (key == "pressure")
        return value + (m_units == ENG_UNITS ? " in" : " mb");

    if (key == "visibility")
        return value + (m_units == ENG_UNITS ? " mi" : " km");

    if (key == "temp" || key == "appt" || key.contains("low",Qt::CaseInsensitive) ||
        key.contains("high",Qt::CaseInsensitive) ||
        key.contains("temp",Qt::CaseInsensitive))
    {
       if ( (value == "NA") || (value == "N/A") )
          return value;
       else
          return value + getTemperatureUnit();
    }

    if (key == "wind_gust" || key == "wind_spdgst" || key == "wind_speed")
        return value + (m_units == ENG_UNITS ? " mph" : " km/h");

    /*The days of the week will be translated if the script sends elements from
     the enum DaysOfWeek.*/
    if (key.startsWith("date-"))
    {
        bool isNumber;
        value.toInt( &isNumber);

        if (isNumber)
        {
            int dayOfWeek = value.toInt();

            switch (dayOfWeek)
            {
                case DAY_SUNDAY :
                    return tr("Sunday");
                    break;
                case DAY_MONDAY :
                    return tr("Monday");
                    break;
                case DAY_TUESDAY :
                    return tr("Tuesday");
                    break;
                case DAY_WENDESDAY :
                    return tr("Wednesday");
                    break;
                case DAY_THURSDAY :
                    return tr("Thursday");
                    break;
                case DAY_FRIDAY :
                    return tr("Friday");
                    break;
                case DAY_SATURDAY :
                    return tr("Saturday");
                    break;
            }
        }
    }

    return value;
}

QString WeatherScreen::prepareDataItem(const QString &key, const QString &value)
{
    return formatDataItem(key, value);
}

bool WeatherScreen::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget() && GetFocusWidget()->keyPressEvent(event))
        return true;

    return false;
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
