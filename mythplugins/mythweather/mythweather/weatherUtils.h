#ifndef _WEATHERUTILS_H_
#define _WEATHERUTILS_H_

// QT headers
#include <QMap>
#include <QString>
#include <Q3Dict>
#include <QDomElement>
#include <QFile>

// MythTV headers
#include <mythtv/mythcontext.h>

#define SI_UNITS 0
#define ENG_UNITS 1
#define DEFAULT_UPDATE_TIMEOUT (5*60*1000)
#define DEFAULT_SCRIPT_TIMEOUT (60*1000)

typedef unsigned char units_t;
typedef QMap<QString, QString> DataMap;

struct TypeListInfo
{
    QString name;
    QString location;
    struct ScriptInfo *src;
};

struct ScreenListInfo
{
    QString name;
    Q3Dict<TypeListInfo> types;
    QStringList dataTypes;
    QString helptxt;
    QStringList sources;
    units_t units;
    bool hasUnits;
    bool multiLoc;
};

typedef QMap<QString, ScreenListInfo *> ScreenListMap;

ScreenListMap loadScreens();
QStringList loadScreen(QDomElement ScreenListInfo);

#endif
