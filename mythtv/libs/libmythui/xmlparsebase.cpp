#include <iostream>
#include <typeinfo>

#include <QFile>

#include "xmlparsebase.h"
#include "mythmainwindow.h"
#include "mythuihelper.h"

#include "mythverbose.h"

/* ui type includes */
#include "mythscreentype.h"
#include "mythuiimage.h"
#include "mythuitext.h"
#include "mythuitextedit.h"
#include "mythuiclock.h"
#include "mythlistbutton.h"
#include "mythuibuttonlist.h"
#include "mythuibuttontree.h"
#include "mythuibutton.h"
#include "mythuispinbox.h"
#include "mythuicheckbox.h"
#include "mythuiprogressbar.h"
#include "mythuigroup.h"

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
    QString s = text.toLower();
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
    if (sscanf(text.toAscii().constData(), "%d,%d", &x, &y) == 2)
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
    if (sscanf(text.toAscii().constData(), "%d,%d", &x, &y) == 2)
    {
        if (x == -1 || y == -1)
        {
            QRect uiSize = GetMythMainWindow()->GetUIScreenRect();
            x = uiSize.width();
            y = uiSize.height();
            normalize = false;
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

MythRect XMLParseBase::parseRect(const QString &text, bool normalize)
{
    MythRect retval;
    QStringList values = text.split(',', QString::SkipEmptyParts);
    if (values.size() == 4)
    {
        retval = MythRect(values[0], values[1], values[2], values[3]);
    }

    if (normalize && retval.isValid())
        retval = GetMythMainWindow()->NormRect(retval);

    return retval;
}

MythRect XMLParseBase::parseRect(QDomElement &element, bool normalize)
{
    return parseRect(getFirstText(element), normalize);
}

int XMLParseBase::parseAlignment(const QString &text)
{
    int alignment = 0;

    QStringList values = text.split(",");

    QStringList::Iterator it;
    for ( it = values.begin(); it != values.end(); ++it )
    {

        QString align = *it;
        align = align.trimmed();
        align = align.toLower();

        if (align == "center" || align == "allcenter")
        {
            alignment |= Qt::AlignCenter;
            break;
        }
        else if (align == "left")
            alignment |= Qt::AlignLeft;
        else if (align == "hcenter")
            alignment |= Qt::AlignHCenter;
        else if (align == "right")
            alignment |= Qt::AlignRight;
        else if (align == "top")
            alignment |= Qt::AlignTop;
        else if (align == "vcenter")
            alignment |= Qt::AlignVCenter;
        else if (align == "bottom")
            alignment |= Qt::AlignBottom;

    }

    return alignment;
}

int XMLParseBase::parseAlignment(QDomElement &element)
{
    return parseAlignment(getFirstText(element));
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

void XMLParseBase::ParseChildren(QDomElement &element,
                                        MythUIType *parent)
{
    if (!parent)
    {
        VERBOSE(VB_IMPORTANT, "Parent is NULL");
        return;
    }

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            QString type = info.tagName();
            if (parent->ParseElement(info))
            {
            }
            else if (type == "font")
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
                     type == "group" ||
                     type == "textedit" ||
                     type == "button" ||
                     type == "buttonlist" ||
                     type == "buttonlist2" ||
                     type == "buttontree" ||
                     type == "spinbox" ||
                     type == "checkbox" ||
                     type == "statetype" ||
                     type == "clock" ||
                     type == "progressbar")
            {
                ParseUIType(info, type, parent);
            }
        }
    }
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

    MythUIType *olduitype = NULL;

    // check for name in immediate parent as siblings cannot share names
    if (parent && parent->GetChild(name))
    {
        // if we're the global object store, assume it's just a theme overriding
        // the defaults..
        if (parent == GetGlobalObjectStore())
            return NULL;

        // Reuse the existing child and reparse
        olduitype = parent->GetChild(name);
    }

    MythUIType *uitype = NULL;
    MythUIType *base = NULL;
    bool needInit = true;

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

        needInit = false;
    }

    if (type == "imagetype")
        uitype = new MythUIImage(parent, name);
    else if (type == "textarea")
        uitype = new MythUIText(parent, name);
    else if (type == "group")
        uitype = new MythUIGroup(parent, name);
    else if (type == "textedit")
        uitype = new MythUITextEdit(parent, name, needInit);
    else if (type == "button")
        uitype = new MythUIButton(parent, name, needInit);
    else if (type == "buttonlist")
        uitype = new MythListButton(parent, name);
    else if (type == "buttonlist2")
        uitype = new MythUIButtonList(parent, name);
    else if (type == "buttontree")
        uitype = new MythUIButtonTree(parent, name);
    else if (type == "spinbox")
        uitype = new MythUISpinBox(parent, name);
    else if (type == "checkbox")
        uitype = new MythUICheckBox(parent, name, needInit);
    else if (type == "statetype")
        uitype = new MythUIStateType(parent, name);
    else if (type == "clock")
        uitype = new MythUIClock(parent, name);
    else if (type == "progressbar")
        uitype = new MythUIProgressBar(parent, name);
    else if (type == "window" && parent == GetGlobalObjectStore())
        uitype = new MythScreenType(parent, name);
    else
    {
        VERBOSE(VB_IMPORTANT, QString("Unknown widget type: %1").arg(type));
        return NULL;
    }

    if (olduitype)
    {
        if (typeid(*olduitype) != typeid(*uitype))
        {
            VERBOSE(VB_IMPORTANT, QString("Duplicate name: %1 in parent %2")
                                        .arg(name).arg(parent->objectName()));
            parent->DeleteChild(olduitype);
        }
        else
        {
            parent->DeleteChild(uitype);
            uitype = olduitype;
        }
    }

    if (base)
    {
        if (typeid(*base) != typeid(*uitype))
        {
            VERBOSE(VB_IMPORTANT, QString("Type of new object '%1' doesn't "
                                          "match old '%2'")
                                         .arg(name).arg(inherits));
            parent->DeleteChild(uitype);
            return NULL;
        }
        else
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
                     info.tagName() == "group" ||
                     info.tagName() == "textedit" ||
                     info.tagName() == "button" ||
                     info.tagName() == "buttonlist" ||
                     info.tagName() == "buttonlist2" ||
                     info.tagName() == "buttontree" ||
                     info.tagName() == "spinbox" ||
                     info.tagName() == "checkbox" ||
                     info.tagName() == "statetype" ||
                     info.tagName() == "clock" ||
                     info.tagName() == "progressbar")
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
    QList<QString> searchpath = GetMythUI()->GetThemeSearchPath();
    QList<QString>::iterator i;
    for (i = searchpath.begin(); i != searchpath.end(); i++)
    {
        QString themefile = *i + xmlfile;
        if (doLoad(windowname, parent, themefile))
        {
            VERBOSE(VB_GENERAL, QString("Loading from: %1").arg(themefile));
            return true;
        }
    }

    VERBOSE(VB_IMPORTANT, QString("Unable to load window '%1' from "
                              "%2").arg(windowname).arg(xmlfile));

    return false;
}

bool XMLParseBase::doLoad(const QString &windowname,
                          MythUIType *parent,
                          const QString &filename,
                          bool onlywindows)
{
    QDomDocument doc;
    QFile f(filename);

    if (!f.open(QIODevice::ReadOnly))
    {
        //cerr << "XMLParse::LoadTheme(): Can't open: " << themeFile << endl;
        return false;
    }

    QString errorMsg;
    int errorLine = 0;
    int errorColumn = 0;

    if (!doc.setContent(&f, false, &errorMsg, &errorLine, &errorColumn))
    {
        cerr << "Error parsing: " << qPrintable(filename) << endl;
        cerr << "at line: " << errorLine << "  column: " << errorColumn << endl;
        cerr << qPrintable(errorMsg) << endl;
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
                    if (font)
                        delete font;
                }
                else if (type == "imagetype" ||
                         type == "textarea" ||
                         type == "group" ||
                         type == "textedit" ||
                         type == "button" ||
                         type == "buttonlist" ||
                         type == "buttonlist2" ||
                         type == "buttontree" ||
                         type == "spinbox" ||
                         type == "checkbox" ||
                         type == "statetype" ||
                         type == "window" ||
                         type == "clock" ||
                         type == "progressbar")
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
    QList<QString> searchpath = GetMythUI()->GetThemeSearchPath();
    QList<QString>::iterator i;
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
    {
        VERBOSE(VB_IMPORTANT, QString("Unable to load window '%1' from base")
                                      .arg(windowname));
        return false;
    }

    MythScreenType *st = dynamic_cast<MythScreenType *>(ui);
    if (!st)
    {
        VERBOSE(VB_IMPORTANT, QString("UI Object '%1' is not a ScreenType")
                                      .arg(windowname));
        return false;
    }

    win->CopyFrom(st);
    return true;
}
