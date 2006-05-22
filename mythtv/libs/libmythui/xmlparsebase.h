#ifndef XMLPARSEBASE_H_
#define XMLPARSEBASE_H_

#include <qdom.h>
#include <qpoint.h>
#include <qrect.h>
#include <qstring.h>

class MythUIType;
class MythScreenType;

class XMLParseBase
{
  public:
    static QString getFirstText(QDomElement &element);
    static bool parseBool(const QString &text);
    static bool parseBool(QDomElement &element);
    static QPoint parsePoint(const QString &text, bool normalize = true);
    static QPoint parsePoint(QDomElement &element, bool normalize = true);
    static QSize parseSize(const QString &text, bool normalize = true);
    static QSize parseSize(QDomElement &element, bool normalize = true);
    static QRect parseRect(const QString &text, bool normalize = true);
    static QRect parseRect(QDomElement &element, bool normalize = true);

    static MythUIType *GetGlobalObjectStore(void);
    static void ClearGlobalObjectStore(void);

    // if more than one, returns last one parsed.
    static MythUIType *ParseChildren(QDomElement &element, MythUIType *parent);

    // parse one and return it.
    static MythUIType *ParseUIType(QDomElement &element, const QString &type,
                                   MythUIType *parent,
                                   MythScreenType *screen = NULL);

    static bool LoadWindowFromXML(const QString &xmlfile, 
                                  const QString &windowname,
                                  MythUIType *parent);

    static bool LoadBaseTheme(void);

    static bool CopyWindowFromBase(const QString &windowname, 
                                   MythScreenType *win);

  private:
    static bool doLoad(const QString &windowname, MythUIType *parent,
                       const QString &filename, bool onlywindows = true);
};

#endif
