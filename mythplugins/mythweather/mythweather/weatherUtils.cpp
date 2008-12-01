
// QT headers
#include <QString>
#include <QStringList>

#include <mythtv/mythdirs.h>

// MythWeather headers
#include "weatherUtils.h"

ScreenListMap loadScreens()
{
    ScreenListMap screens;
    QDomDocument doc;

    QFile f(GetShareDir() + "mythweather/weather-screens.xml");

    if (!f.open(QIODevice::ReadOnly))
    {
        VERBOSE(VB_FILE, QString("Unable to open weather-screens.xml"));
        return screens;
    }

    if ( !doc.setContent( &f ) ) {
        VERBOSE(VB_IMPORTANT, QString("Unable to parse weather-screens.xml"));
        f.close();
        return screens;
    }
    f.close();

    QDomElement docElem = doc.documentElement();

    for (QDomNode n = docElem.firstChild(); !n.isNull();
            n = n.nextSibling())
    {
        QDomElement e = n.toElement();
        if (!e.isNull())
        {
            if (e.tagName() == "screen")
            {
                ScreenListInfo *screendef = new ScreenListInfo();
                screendef->name = e.attribute("name");
                QString hasUnits = e.attribute("hasunits");
                if (hasUnits.toLower() == "no")
                    screendef->hasUnits = false;
                else
                    screendef->hasUnits = true;
                screendef->dataTypes = loadScreen(e);
                screens[screendef->name] = screendef;
            }
        }
    }

    return screens;
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
