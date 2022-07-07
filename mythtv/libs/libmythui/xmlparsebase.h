#ifndef XMLPARSEBASE_H_
#define XMLPARSEBASE_H_

#include <QString>
#include <QMap>

#include "mythrect.h"

class MythUIType;
class MythScreenType;
class QDomElement;
class QBrush;

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define VERBOSE_XML(type, level, filename, element, msg)                  \
    LOG(type, level, LOC + QString("%1\n\t\t\t"                           \
                             "Location: %2 @ %3\n\t\t\t"                  \
                             "Name: '%4'\tType: '%5'")                    \
            .arg(msg, filename, QString::number((element).lineNumber()),  \
                 (element).attribute("name", ""), (element).tagName()))


class MUI_PUBLIC XMLParseBase
{
  public:
    static QString getFirstText(QDomElement &element);
    static bool parseBool(const QString &text);
    static bool parseBool(QDomElement &element);
    static MythPoint parsePoint(const QString &text, bool normalize = true);
    static MythPoint parsePoint(QDomElement &element, bool normalize = true);
    static QSize parseSize(const QString &text, bool normalize = true);
    static QSize parseSize(QDomElement &element, bool normalize = true);
    static MythRect parseRect(const QString &text, bool normalize = true);
    static MythRect parseRect(QDomElement &element, bool normalize = true);
    static int parseAlignment(const QString &text);
    static int parseAlignment(QDomElement &element);
    static QBrush parseGradient(const QDomElement &element);
    static QString parseText(QDomElement &element);
    static MythUIType *GetGlobalObjectStore(void);
    static void ClearGlobalObjectStore(void);

    static void ParseChildren(
        const QString &filename, QDomElement &element,
        MythUIType *parent, bool showWarnings);

    // parse one and return it.
    static MythUIType *ParseUIType(
        const QString &filename,
        QDomElement &element, const QString &type,
        MythUIType *parent, MythScreenType *screen,
        bool showWarnings,
        QMap<QString, QString> &parentDependsMap);

    static bool WindowExists(const QString &xmlfile, const QString &windowname);
    static bool LoadWindowFromXML(const QString &xmlfile,
                                  const QString &windowname,
                                  MythUIType *parent);

    static bool LoadBaseTheme(void);
    static bool LoadBaseTheme(const QString &baseTheme);

    static bool CopyWindowFromBase(const QString &windowname,
                                   MythScreenType *win);

  private:
    static bool doLoad(const QString &windowname, MythUIType *parent,
                       const QString &filename,
                       bool onlyLoadWindows, bool showWarnings);
    static void ConnectDependants(MythUIType * parent,
                                    QMap<QString, QString> &dependsMap);

};

#endif
