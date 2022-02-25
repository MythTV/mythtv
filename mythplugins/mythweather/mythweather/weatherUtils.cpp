// QT headers
#include <QCoreApplication>
#include <QString>
#include <QStringList>

// MythTV
#include <libmythbase/mythdirs.h>
#include <libmythui/mythmainwindow.h>
#include <libmythui/mythuihelper.h>

// MythWeather headers
#include "weatherUtils.h"

static QString getScreenTitle(const QString &screenName)
{
    if (screenName == "Current Conditions")
        return QCoreApplication::translate("(Weather Screens)", 
                                           "Current Conditions");
    if (screenName == "Three Day Forecast")
        return QCoreApplication::translate("(Weather Screens)",
                                           "Three Day Forecast");
    if (screenName == "18 Hour Forecast")
        return QCoreApplication::translate("(Weather Screens)",
                                           "18 Hour Forecast");
    if (screenName == "Severe Weather Alerts")
        return QCoreApplication::translate("(Weather Screens)",
                                           "Severe Weather Alerts");
    if (screenName == "Six Day Forecast")
        return QCoreApplication::translate("(Weather Screens)",
                                           "Six Day Forecast");
    if (screenName == "Static Map")
        return QCoreApplication::translate("(Weather Screens)",
                                           "Static Map");
    if (screenName == "Animated Map")
        return QCoreApplication::translate("(Weather Screens)",
                                           "Animated Map");

    return screenName;
}

ScreenListMap loadScreens()
{
    ScreenListMap screens;
    QStringList searchpath = GetMythUI()->GetThemeSearchPath();

    // Check the theme first if it has its own weather-screens.xml

    QStringList::iterator it;
    for (it = searchpath.begin(); it != searchpath.end(); ++it)
    {
        QString filename = (*it) + "weather-screens.xml";
        if (doLoadScreens(filename, screens))
        {
            LOG(VB_GENERAL, LOG_INFO,
                QString("Loading from: %1").arg(filename));
            break;
        }
    }

    //  Also load from the default file in case the theme file doesn't
    //  exist or the theme file doesn't define all the screens

    QString filename = GetShareDir() + "mythweather/weather-screens.xml";

    if (!doLoadScreens(filename, screens))
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Unable to parse weather-screens.xml"));
    }

    return screens;
}

bool doLoadScreens(const QString &filename, ScreenListMap &screens)
{
    QFile f(filename);
    QDomDocument doc;

    if (!f.open(QIODevice::ReadOnly))
    {
        return false;
    }

    if ( !doc.setContent( &f ) )
    {
        f.close();
        return false;
    }
    f.close();

    QDomElement docElem = doc.documentElement();

    for (QDomNode n = docElem.firstChild(); !n.isNull();
            n = n.nextSibling())
    {
        QDomElement e = n.toElement();
        if (!e.isNull())
        {
            if ( (e.tagName() == "screen") && !screens.contains(e.attribute("name")) )
            {
                screens[e.attribute("name")].m_multiLoc = false;
                screens[e.attribute("name")].m_name = e.attribute("name");
                screens[e.attribute("name")].m_title =
                                            getScreenTitle(e.attribute("name"));
                QString hasUnits = e.attribute("hasunits");
                screens[e.attribute("name")].m_hasUnits =
                    hasUnits.toLower() != "no";
                screens[e.attribute("name")].m_dataTypes = loadScreen(e);
            }
        }
    }
    return true;
}

QStringList loadScreen(const QDomElement& ScreenListInfo)
{

    QStringList typesList;

    for (QDomNode n = ScreenListInfo.firstChild(); !n.isNull();
            n = n.nextSibling())
    {
        QDomElement e = n.toElement();
        if (!e.isNull())
        {
            if (e.tagName() == "datum")
            {
                QString name = e.attribute("name");
                typesList << name;
            }
        }
    }

    return typesList;
}
