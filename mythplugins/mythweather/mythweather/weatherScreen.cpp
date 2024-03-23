// C++ headers
#include <vector>

// MythTV headers
#include <libmyth/mythcontext.h>

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
               MythScreenType (parent, screenDefn->m_title),
    m_screenDefn(screenDefn),
               m_name(m_screenDefn->m_name),
    m_id(id)
{

    QStringList types = m_screenDefn->m_dataTypes;

    for (const QString& type : std::as_const(types))
    {
        m_dataValueMap[type] =  "";
    }
}

bool WeatherScreen::Create()
{
    // Load the theme for this screen
    bool foundtheme = LoadWindowFromXML("weather-ui.xml", m_name, this);
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
            LOG(VB_GENERAL, LOG_DEBUG, i.key());
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

void WeatherScreen::newData(const QString& /*loc*/, units_t /*units*/, DataMap data)
{
    DataMap::iterator itr = data.begin();
    while (itr != data.end())
    {
        setValue(itr.key(), *itr);
        ++itr;
    }

    // This may seem like overkill, but it is necessary to actually update the
    // static and animated maps when they are redownloaded on an update
    if (!prepareScreen())
        LOG(VB_GENERAL, LOG_ERR, "Theme is missing a required widget!");

    emit screenReady(this);
}

QString WeatherScreen::getTemperatureUnit()
{
    if (m_units == ENG_UNITS)
        return QString::fromUtf8("°") + "F";
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
            LOG(VB_GENERAL, LOG_ERR, "Widget not found " + itr.key());

            if (name == "copyright")
            {
                LOG(VB_GENERAL, LOG_WARNING,
                    QString("No copyright widget found, skipping screen %1.")
                        .arg(m_name));
                return false;
            }
            if (name == "copyrightlogo")
            {
                LOG(VB_GENERAL, LOG_WARNING,
                    QString("No copyrightlogo widget found, skipping screen %1.")
                        .arg(m_name));
                return false;
            }
        }

        if( !widget || checkOnly )
        {
            ++itr;
            continue;
        }

        if (auto *w2 = dynamic_cast<MythUIText *>(widget))
        {
            w2->SetText(itr.value());
        }
        else if (auto *w3 = dynamic_cast<MythUIImage *>(widget))
        {
            w3->SetFilename(itr.value());
            w3->Load();
        }

        prepareWidget(widget);
        ++itr;
    }

    m_prepared = true;
    return true;
}

void WeatherScreen::prepareWidget(MythUIType *widget)
{
    /*
     * Basically so we don't do it twice since some screens (Static Map) mess
     * with image dimensions
     */
    auto *img = dynamic_cast<MythUIImage *>(widget);
    if (img != nullptr)
    {
        img->Load();
    }
}

QString WeatherScreen::formatDataItem(const QString &key, const QString &value)
{
    if (key.startsWith("relative_humidity") || key.startsWith("pop"))
        return value + " %";

    if (key == "pressure")
        return value + (m_units == ENG_UNITS ? " in" : " mb");

    if (key == "visibility")
        return value + (m_units == ENG_UNITS ? " mi" : " km");

    if (key.startsWith("temp") ||
        key.startsWith("appt") ||
        key.startsWith("low") ||
        key.startsWith("high"))
    {
       if ((value == "NA") || (value == "N/A"))
          return {};
       return value + getTemperatureUnit();
    }

    if (key.startsWith("wind_gust") ||
        key.startsWith("wind_spdgst") ||
        key.startsWith("wind_speed"))
        return value + (m_units == ENG_UNITS ? " mph" : " km/h");

    /*The days of the week will be translated if the script sends elements from
     the enum DaysOfWeek.*/
    if (key.startsWith("date"))
    {
        bool isNumber = false;
        (void)value.toInt( &isNumber);

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

    if (key == "copyrightlogo" && value == "none")
        return {};

    return value;
}

QString WeatherScreen::prepareDataItem(const QString &key, const QString &value)
{
    return formatDataItem(key, value);
}

bool WeatherScreen::keyPressEvent(QKeyEvent *event)
{
    return GetFocusWidget() && GetFocusWidget()->keyPressEvent(event);
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
