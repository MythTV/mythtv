#ifndef _WEATHERUTILS_H_
#define _WEATHERUTILS_H_

// QT headers
#include <QMap>
#include <QMultiHash>
#include <QString>
#include <QDomElement>
#include <QFile>
#include <QMetaType>

// MythTV headers
#include <mythcontext.h>

#define SI_UNITS 0
#define ENG_UNITS 1
#define DEFAULT_UPDATE_TIMEOUT (5*60*1000)
#define DEFAULT_SCRIPT_TIMEOUT (60)

class ScriptInfo;

typedef unsigned char units_t;
typedef QMap<QString, QString> DataMap;

class TypeListInfo
{
  public:

    TypeListInfo(const TypeListInfo& info)
        : name(info.name), location(info.location), src(info.src)
    {
        name.detach();
        location.detach();
    }

    TypeListInfo(const QString &_name)
        : name(_name), location(QString::null), src(NULL)
    {
        name.detach();
    }
    TypeListInfo(const QString &_name, const QString &_location)
        : name(_name), location(_location), src(NULL)
    {
        name.detach();
        location.detach();
    }
    TypeListInfo(const QString &_name, const QString &_location,
                 ScriptInfo *_src)
        : name(_name), location(_location), src(_src)
    {
        name.detach();
        location.detach();
    }

  public:
    QString name;
    QString location;
    ScriptInfo *src;
};
typedef QMultiHash<QString, TypeListInfo> TypeListMap;

class ScreenListInfo
{
  public:
    ScreenListInfo() :
        units(SI_UNITS),
        hasUnits(false),
        multiLoc(false)
    {
        updating = false;
    }

    ScreenListInfo(const ScreenListInfo& info) :
        name(info.name),
        title(info.title),
        types(info.types),
        dataTypes(info.dataTypes),
        helptxt(info.helptxt),
        sources(info.sources),
        units(info.units),
        hasUnits(info.hasUnits),
        multiLoc(info.multiLoc),
        updating(info.updating)
    {
      types.detach();
    }

    TypeListInfo GetCurrentTypeList(void) const;

  public:
    QString name;
    QString title;
    TypeListMap types;
    QStringList dataTypes;
    QString helptxt;
    QStringList sources;
    units_t units;
    bool hasUnits;
    bool multiLoc;
    bool updating;
};

Q_DECLARE_METATYPE(ScreenListInfo *);

typedef QMap<QString, ScreenListInfo> ScreenListMap;

ScreenListMap loadScreens();
QStringList loadScreen(QDomElement ScreenListInfo);
bool doLoadScreens(const QString &filename, ScreenListMap &screens);

#endif
