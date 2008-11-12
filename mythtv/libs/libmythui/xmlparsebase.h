#ifndef XMLPARSEBASE_H_
#define XMLPARSEBASE_H_

#include <QString>

#include "mythrect.h"

class MythUIType;
class MythScreenType;
class QDomElement;

class MPUBLIC XMLParseBase
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

    static MythUIType *GetGlobalObjectStore(void);
    static void ClearGlobalObjectStore(void);

    static void ParseChildren(QDomElement &element, MythUIType *parent);

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
