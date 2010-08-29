// QT headers
#include <QString>
#include <QStringList>

#include <mythdirs.h>
#include "mythmainwindow.h"
#include "mythuihelper.h"

// MythWeather headers
#include "weatherUtils.h"

static QString getScreenTitle(const QString &screenName)
{
    if (screenName == "Current Conditions")
        return QObject::tr("Current Conditions");
    if (screenName == "Three Day Forecast")
        return QObject::tr("Three Day Forecast");
    if (screenName == "18 Hour Forecast")
        return QObject::tr("18 Hour Forecast");
    if (screenName == "Severe Weather Alerts")
        return QObject::tr("Severe Weather Alerts");
    if (screenName == "Six Day Forecast")
        return QObject::tr("Six Day Forecast");
    if (screenName == "Static Map")
        return QObject::tr("Static Map");
    if (screenName == "Animated Map")
        return QObject::tr("Animated Map");

    return screenName;
}

ScreenListMap loadScreens()
{
    ScreenListMap screens;
    QList<QString> searchpath = GetMythUI()->GetThemeSearchPath();
    
    // Check the theme first if it has its own weather-screens.xml
    
    QList<QString>::iterator i;
    for (i = searchpath.begin(); i != searchpath.end(); i++)
    {
        QString filename = *i + "weather-screens.xml";
        if (doLoadScreens(filename, screens))
        {
            VERBOSE(VB_GENERAL, QString("Loading from: %1").arg(filename));
            break;
        }
    }

    //  Also load from the default file in case the theme file doesn't
    //  exist or the theme file doesn't define all the screens
    
    QString filename = GetShareDir() + "mythweather/weather-screens.xml";
    
    if (!doLoadScreens(filename, screens))
    {
        VERBOSE(VB_IMPORTANT, QString("Unable to parse weather-screens.xml"));
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
                screens[e.attribute("name")].multiLoc = false;
                screens[e.attribute("name")].name = e.attribute("name");
                screens[e.attribute("name")].title =
                                            getScreenTitle(e.attribute("name"));
                QString hasUnits = e.attribute("hasunits");
                if (hasUnits.toLower() == "no")
                    screens[e.attribute("name")].hasUnits = false;
                else
                    screens[e.attribute("name")].hasUnits = true;
                screens[e.attribute("name")].dataTypes = loadScreen(e);
            }
        }
    }
    return true;
}

QStringList loadScreen(QDomElement ScreenListInfo)
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
