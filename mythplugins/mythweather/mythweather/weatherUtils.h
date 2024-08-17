#ifndef WEATHERUTILS_H
#define WEATHERUTILS_H

// C++
#include <utility>

// QT headers
#include <QDomElement>
#include <QFile>
#include <QMap>
#include <QMetaType>
#include <QMultiHash>
#include <QString>

// MythTV headers
#include <libmyth/mythcontext.h>

static constexpr uint8_t SI_UNITS  { 0 };
static constexpr uint8_t ENG_UNITS { 1 };
static constexpr std::chrono::minutes DEFAULT_UPDATE_TIMEOUT { 5min };
static constexpr std::chrono::seconds DEFAULT_SCRIPT_TIMEOUT { 60s };

class ScriptInfo;

using units_t = unsigned char;
using DataMap = QMap<QString, QString>;

class TypeListInfo
{
  public:

    TypeListInfo(const TypeListInfo& info) = default;
    explicit TypeListInfo(QString _name)
        : m_name(std::move(_name)) {}
    TypeListInfo(QString _name, QString _location)
        : m_name(std::move(_name)), m_location(std::move(_location)) {}
    TypeListInfo(QString _name, QString _location,
                 ScriptInfo *_src)
        : m_name(std::move(_name)), m_location(std::move(_location)), m_src(_src) {}

  public:
    QString     m_name;
    QString     m_location;
    ScriptInfo *m_src      {nullptr};
};
using TypeListMap = QMultiHash<QString, TypeListInfo>;

class ScreenListInfo
{
  public:
    TypeListInfo GetCurrentTypeList(void) const;

  public:
    QString     m_name;
    QString     m_title;
    TypeListMap m_types;
    QStringList m_dataTypes;
    QString     m_helptxt;
    QStringList m_sources;
    units_t     m_units    {SI_UNITS};
    bool        m_hasUnits {false};
    bool        m_multiLoc {false};
    bool        m_updating {false};
};

Q_DECLARE_METATYPE(ScreenListInfo *);

using ScreenListMap = QMap<QString, ScreenListInfo>;

ScreenListMap loadScreens();
QStringList loadScreen(const QDomElement& ScreenListInfo);
bool doLoadScreens(const QString &filename, ScreenListMap &screens);

#endif /* WEATHERUTILS_H */
