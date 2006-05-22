#include <qfile.h>

#include "xmlparsebase.h"
#include "mythmainwindow.h"

#include "mythcontext.h"

/* ui type includes */
#include "mythscreentype.h"
#include "mythuiimage.h"
#include "mythuitext.h"
#include "mythlistbutton.h"

QString XMLParseBase::getFirstText(QDomElement &element)
{
    for (QDomNode dname = element.firstChild(); !dname.isNull();
         dname = dname.nextSibling())
    {
        QDomText t = dname.toText();
        if (!t.isNull())
            return t.data();
    }
    return "";
}

bool XMLParseBase::parseBool(const QString &text)
{
    QString s = text.lower();
    return (s == "yes" || s == "true" || s.toInt());
}

bool XMLParseBase::parseBool(QDomElement &element)
{
    return parseBool(getFirstText(element));
}

QPoint XMLParseBase::parsePoint(const QString &text, bool normalize)
{
    int x, y;
    QPoint retval;
    if (sscanf(text.data(), "%d,%d", &x, &y) == 2)
        retval = QPoint(x, y);

    if (normalize)
        retval = GetMythMainWindow()->NormPoint(retval);

    return retval;
}

QPoint XMLParseBase::parsePoint(QDomElement &element, bool normalize)
{
    return parsePoint(getFirstText(element), normalize);
}

QSize XMLParseBase::parseSize(const QString &text, bool normalize)
{
    int x, y;
    QSize retval;
    if (sscanf(text.data(), "%d,%d", &x, &y) == 2)
    {
        if (x == -1 || y == -1)
        {
            QRect uiSize = GetMythMainWindow()->GetUIScreenRect();
            x = uiSize.width();
            y = uiSize.height();
        }
        
        retval = QSize(x, y);
    }

    if (normalize)
        retval = GetMythMainWindow()->NormSize(retval);

    return retval;
}

QSize XMLParseBase::parseSize(QDomElement &element, bool normalize)
{
    return parseSize(getFirstText(element), normalize);
}

QRect XMLParseBase::parseRect(const QString &text, bool normalize)
{
    int x, y, w, h;
    QRect retval;
    if (sscanf(text.data(), "%d,%d,%d,%d", &x, &y, &w, &h) == 4)
        retval = QRect(x, y, w, h);

    if (normalize)
        retval = GetMythMainWindow()->NormRect(retval);

    return retval;
}

QRect XMLParseBase::parseRect(QDomElement &element, bool normalize)
{
    return parseRect(getFirstText(element), normalize);
}

static MythUIType *globalObjectStore = NULL;

MythUIType *XMLParseBase::GetGlobalObjectStore(void)
{
    if (!globalObjectStore)
        globalObjectStore = new MythUIType(NULL, "global store");
    return globalObjectStore;
}

void XMLParseBase::ClearGlobalObjectStore(void)
{
    delete globalObjectStore;
    globalObjectStore = NULL;
    GetGlobalObjectStore();
}

MythUIType *XMLParseBase::ParseChildren(QDomElement &element, 
                                        MythUIType *parent)
{
    MythUIType *ret = NULL;
    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            QString type = info.tagName();
            if (type == "font")
            {
                MythFontProperties *font;
                bool global = (GetGlobalObjectStore() == parent);
                font = MythFontProperties::ParseFromXml(info, global);
                if (!global && font)
                {
                    QString name = info.attribute("name");
                    parent->AddFont(name, font);
                }

                if (font)
                    delete font;
            }
            else if (type == "imagetype" ||
                     type == "textarea" ||
                     type == "buttonlist" ||
                     type == "horizontalbuttonlist" ||
                     type == "statetype")
            {
                ret = ParseUIType(info, type, parent);
            }
        }
    }

    return ret;
}

MythUIType *XMLParseBase::ParseUIType(QDomElement &element, const QString &type,
                                      MythUIType *parent, 
                                      MythScreenType *screen)
{
    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, "element has no name");
        return NULL;
    }

    if (parent && parent->GetChild(name))
    {
        // if we're the global object store, assume it's just a theme overriding
        // the defaults..
        if (parent == GetGlobalObjectStore())
            return NULL;

        VERBOSE(VB_IMPORTANT, QString("duplicate name: %1 in parent %2")
                                     .arg(name).arg(parent->name()));
        return NULL;
    }

    // check for name in parent

    MythUIType *uitype = NULL;
    MythUIType *base = NULL;

    QString inherits = element.attribute("from", "");
    if (!inherits.isEmpty())
    {
        if (parent)
            base = parent->GetChild(inherits);

        // might remove this
        if (screen && !base)
            base = screen->GetChild(inherits);

        if (!base)
            base = GetGlobalObjectStore()->GetChild(inherits);

        if (!base)
        {
            VERBOSE(VB_IMPORTANT, QString("Couldn't find object '%1' to "
                                          "inherit '%2' from")
                                         .arg(inherits).arg(name));
            return NULL;
        }
    }

    if (type == "imagetype")
        uitype = new MythUIImage(parent, name);
    else if (type == "textarea")
        uitype = new MythUIText(parent, name);
    else if (type == "buttonlist")
        uitype = new MythListButton(parent, name);
    else if (type == "horizontalbuttonlist")
        uitype = new MythHorizListButton(parent, name);
    else if (type == "statetype")
        uitype = new MythUIStateType(parent, name);
    else if (type == "window" && parent == GetGlobalObjectStore())
        uitype = new MythScreenType(parent, name);
    else
    {
        VERBOSE(VB_IMPORTANT, QString("Unknown widget type: %1").arg(type));
        return NULL;
    }

    if (base)
    {
        if (typeid(base) != typeid(uitype))
        {
            VERBOSE(VB_IMPORTANT, QString("Type of new object '%1' doesn't "
                                          "match old '%2'")
                                         .arg(name).arg(inherits));
            delete uitype;
        }
        uitype->CopyFrom(base);
    }

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (uitype->ParseElement(info))
            {
            }
            else if (info.tagName() == "font")
            {
                MythFontProperties *font;
                bool global = (GetGlobalObjectStore() == parent);
                font = MythFontProperties::ParseFromXml(info, global);
                if (!global && font)
                {
                    QString name = info.attribute("name");
                    uitype->AddFont(name, font);
                }

                if (font)
                    delete font;
            }
            else if (info.tagName() == "imagetype" || 
                     info.tagName() == "textarea" || 
                     info.tagName() == "buttonlist" ||
                     info.tagName() == "horizontalbuttonlist" ||
                     info.tagName() == "statetype")
            {
                ParseUIType(info, info.tagName(), uitype, screen);
            }
        }
    }

    uitype->Finalize();
    return uitype;
}

bool XMLParseBase::LoadWindowFromXML(const QString &xmlfile,
                                     const QString &windowname,
                                     MythUIType *parent)
{
    QValueList<QString> searchpath = gContext->GetThemeSearchPath();
    QValueList<QString>::iterator i;
    for (i = searchpath.begin(); i != searchpath.end(); i++)
    {
        QString themefile = *i + xmlfile;
        if (doLoad(windowname, parent, themefile))
        {
            VERBOSE(VB_GENERAL, QString("Loading from: %1").arg(themefile));
            return true;
        }
    }

    return false;
}

bool XMLParseBase::doLoad(const QString &windowname,
                          MythUIType *parent, 
                          const QString &filename,
                          bool onlywindows)
{
    QDomDocument doc;
    QFile f(filename);

    if (!f.open(IO_ReadOnly))
    {
        //cerr << "XMLParse::LoadTheme(): Can't open: " << themeFile << endl;
        return false;
    }

    QString errorMsg;
    int errorLine = 0;
    int errorColumn = 0;

    if (!doc.setContent(&f, false, &errorMsg, &errorLine, &errorColumn))
    {
        cerr << "Error parsing: " << filename << endl;
        cerr << "at line: " << errorLine << "  column: " << errorColumn << endl;
        cerr << errorMsg << endl;
        f.close();
        return false;
    }

    f.close();

    QDomElement docElem = doc.documentElement();
    QDomNode n = docElem.firstChild();
    while (!n.isNull())
    {
        QDomElement e = n.toElement();
        if (!e.isNull())
        {
            if (onlywindows && e.tagName() == "window")
            {
                QString name = e.attribute("name", "");
                if (name.isNull() || name.isEmpty())
                {
                    cerr << "Window needs a name\n";
                    return false;
                }

                if (name == windowname)
                {
                    ParseChildren(e, parent);
                    return true;
                }
            }

            if (!onlywindows)
            {
                QString type = e.tagName();
                if (type == "font")
                {
                    MythFontProperties *font;
                    bool global = (GetGlobalObjectStore() == parent);
                    font = MythFontProperties::ParseFromXml(e, global);
                    if (!global && font)
                    {
                        QString name = e.attribute("name");
                        parent->AddFont(name, font);
                    }
                }
                else if (type == "imagetype" ||
                         type == "textarea" ||
                         type == "buttonlist" ||
                         type == "horizontalbuttonlist" ||
                         type == "statetype" ||
                         type == "window")
                {
                    ParseUIType(e, type, parent);
                }
            }
        }
        n = n.nextSibling();
    }

    if (onlywindows)
        return false;
    return true;
}

bool XMLParseBase::LoadBaseTheme(void)
{
    QValueList<QString> searchpath = gContext->GetThemeSearchPath();
    QValueList<QString>::iterator i;
    for (i = searchpath.begin(); i != searchpath.end(); i++)
    {
        QString themefile = *i + "base.xml";
        if (doLoad(QString::null, GetGlobalObjectStore(), themefile, false))
        {
            VERBOSE(VB_GENERAL, QString("Loading from: %1").arg(themefile));
        }
    }

    return false;
}

bool XMLParseBase::CopyWindowFromBase(const QString &windowname,
                                      MythScreenType *win)
{
    MythUIType *ui = GetGlobalObjectStore()->GetChild(windowname);
    if (!ui)
        return false;

    MythScreenType *st = dynamic_cast<MythScreenType *>(ui);
    if (!st)
        return false;

    win->CopyFrom(st);
    return true;
}
