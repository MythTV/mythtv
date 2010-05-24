
// Own header
#include "xmlparsebase.h"

// C++/C headers
#include <typeinfo>

// QT headers
#include <QFile>
#include <QDomDocument>
#include <QString>
#include <QBrush>
#include <QLinearGradient>

// libmyth headers
#include "mythverbose.h"

// Mythui headers
#include "mythmainwindow.h"
#include "mythuihelper.h"

/* ui type includes */
#include "mythscreentype.h"
#include "mythuiimage.h"
#include "mythuitext.h"
#include "mythuitextedit.h"
#include "mythuiclock.h"
#include "mythuibuttonlist.h"
#include "mythuibutton.h"
#include "mythuispinbox.h"
#include "mythuicheckbox.h"
#include "mythuiprogressbar.h"
#include "mythuigroup.h"
#include "mythuiwebbrowser.h"
#include "mythuiguidegrid.h"
#include "mythuishape.h"
#include "mythuibuttontree.h"
#include "mythuieditbar.h"
#include "mythfontproperties.h"

#define LOC      QString("XMLParseBase: ")
#define LOC_WARN QString("XMLParseBase, Warning: ")
#define LOC_ERR  QString("XMLParseBase, Error: ")

void VERBOSE_XML(
    unsigned int verbose_type,
    const QString &filename, const QDomElement &element, QString msg)
{
    VERBOSE(verbose_type,
            QString("%1\n\t\t\t"
                    "Location: %2 @ %3\n\t\t\t"
                    "Name: '%4'\tType: '%5'")
            .arg(msg).arg(filename).arg(element.lineNumber())
            .arg(element.attribute("name", "")).arg(element.tagName()));
}

QString XMLParseBase::getFirstText(QDomElement &element)
{
    for (QDomNode dname = element.firstChild(); !dname.isNull();
         dname = dname.nextSibling())
    {
        QDomText t = dname.toText();
        if (!t.isNull())
            return t.data();
    }
    return QString();
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

MythPoint XMLParseBase::parsePoint(const QString &text, bool normalize)
{
    MythPoint retval;
    QStringList values = text.split(',', QString::SkipEmptyParts);
    if (values.size() == 2)
        retval = MythPoint(values[0], values[1]);

     if (normalize)
         retval.NormPoint();

    return retval;
}

MythPoint XMLParseBase::parsePoint(QDomElement &element, bool normalize)
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
        retval = MythRect(values[0], values[1], values[2], values[3]);

     if (normalize)
         retval.NormRect();

    return retval;
}

MythRect XMLParseBase::parseRect(QDomElement &element, bool normalize)
{
    return parseRect(getFirstText(element), normalize);
}

int XMLParseBase::parseAlignment(const QString &text)
{
    int alignment = 0;

    QStringList values = text.split(',');

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
        else if (align == "justify")
            alignment |= Qt::AlignJustify;
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

QBrush XMLParseBase::parseGradient(const QDomElement &element)
{
    QLinearGradient gradient;
    QString gradientStart = element.attribute("start", "");
    QString gradientEnd = element.attribute("end", "");
    int gradientAlpha = element.attribute("alpha", "100").toInt();
    QString direction = element.attribute("direction", "vertical");

    float x1, y1, x2, y2 = 0.0;
    if (direction == "vertical")
    {
        x1 = 0.5;
        x2 = 0.5;
        y1 = 0.0;
        y2 = 1.0;
    }
    else if (direction == "diagonal")
    {
        x1 = 0.0;
        x2 = 1.0;
        y1 = 0.0;
        y2 = 1.0;
    }
    else
    {
        x1 = 0.0;
        x2 = 1.0;
        y1 = 0.5;
        y2 = 0.5;
    }

    gradient.setCoordinateMode(QGradient::ObjectBoundingMode);
    gradient.setStart(x1, y1);
    gradient.setFinalStop(x2, y2);

    QGradientStops stops;

    if (!gradientStart.isEmpty())
    {
        QColor startColor = QColor(gradientStart);
        startColor.setAlpha(gradientAlpha);
        QGradientStop stop(0.0, startColor);
        stops.append(stop);
    }

    if (!gradientEnd.isEmpty())
    {
        QColor endColor = QColor(gradientEnd);
        endColor.setAlpha(gradientAlpha);
        QGradientStop stop(1.0, endColor);
        stops.append(stop);
    }

    for (QDomNode child = element.firstChild(); !child.isNull();
        child = child.nextSibling())
    {
        QDomElement childElem = child.toElement();
        if (childElem.tagName() == "stop")
        {
            float position = childElem.attribute("position", "0").toFloat();
            QString color = childElem.attribute("color", "");
            int alpha = childElem.attribute("alpha", "-1").toInt();
            if (alpha < 0)
                alpha = gradientAlpha;
            QColor stopColor = QColor(color);
            stopColor.setAlpha(alpha);
            QGradientStop stop((position / 100), stopColor);
            stops.append(stop);
        }
    }

    gradient.setStops(stops);

    return QBrush(gradient);
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

void XMLParseBase::ParseChildren(const QString &filename,
                                 QDomElement &element,
                                 MythUIType *parent,
                                 bool showWarnings)
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
            if (parent->ParseElement(filename, info, showWarnings))
            {
            }
            else if (type == "font" || type == "fontdef")
            {
                bool global = (GetGlobalObjectStore() == parent);
                MythFontProperties *font = MythFontProperties::ParseFromXml(
                    filename, info, parent, global, showWarnings);

                if (!global && font)
                {
                    QString name = info.attribute("name");
                    parent->AddFont(name, font);
                }

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
                     type == "progressbar" ||
                     type == "webbrowser" ||
                     type == "guidegrid" ||
                     type == "shape" ||
                     type == "editbar")
            {
                ParseUIType(filename, info, type, parent, NULL, showWarnings);
            }
            else
            {
                VERBOSE_XML(VB_IMPORTANT, filename, info,
                            LOC_ERR + "Unknown widget type");
            }
        }
    }
}

MythUIType *XMLParseBase::ParseUIType(
    const QString &filename,
    QDomElement &element, const QString &type,
    MythUIType *parent,
    MythScreenType *screen,
    bool showWarnings)
{
    QString name = element.attribute("name", "");
    if (name.isEmpty())
    {
        VERBOSE_XML(VB_IMPORTANT, filename, element,
                    LOC_ERR + "This element requires a name");
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
            VERBOSE_XML(VB_IMPORTANT, filename, element,
                        LOC_ERR + QString(
                            "Couldn't find object '%1' to inherit '%2' from")
                        .arg(inherits).arg(name));
            return NULL;
        }
    }

    if (type == "imagetype")
        uitype = new MythUIImage(parent, name);
    else if (type == "textarea")
        uitype = new MythUIText(parent, name);
    else if (type == "group")
        uitype = new MythUIGroup(parent, name);
    else if (type == "textedit")
        uitype = new MythUITextEdit(parent, name);
    else if (type == "button")
        uitype = new MythUIButton(parent, name);
    else if (type == "buttonlist2" || type == "buttonlist")
        uitype = new MythUIButtonList(parent, name);
    else if (type == "buttontree")
        uitype = new MythUIButtonTree(parent, name);
    else if (type == "spinbox")
        uitype = new MythUISpinBox(parent, name);
    else if (type == "checkbox")
        uitype = new MythUICheckBox(parent, name);
    else if (type == "statetype")
        uitype = new MythUIStateType(parent, name);
    else if (type == "clock")
        uitype = new MythUIClock(parent, name);
    else if (type == "progressbar")
        uitype = new MythUIProgressBar(parent, name);
    else if (type == "webbrowser")
        uitype = new MythUIWebBrowser(parent, name);
    else if (type == "guidegrid")
        uitype = new MythUIGuideGrid(parent, name);
    else if (type == "shape")
        uitype = new MythUIShape(parent, name);
    else if (type == "editbar")
        uitype = new MythUIEditBar(parent, name);
    else if (type == "window" && parent == GetGlobalObjectStore())
        uitype = new MythScreenType(parent, name);
    else
    {
        VERBOSE_XML(VB_IMPORTANT, filename, element,
                    LOC_ERR + "Unknown widget type.");
        return NULL;
    }

    if (!uitype)
    {
        VERBOSE_XML(VB_IMPORTANT, filename, element,
                    LOC_ERR + "Failed to instantiate widget type.");
        return NULL;
    }

    if (olduitype)
    {
        if (typeid(*olduitype) != typeid(*uitype))
        {
            VERBOSE_XML(VB_IMPORTANT, filename, element, LOC_ERR +
                        QString("Duplicate name: '%1' in parent '%2'")
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
            VERBOSE_XML(VB_IMPORTANT, filename, element, LOC_ERR +
                        QString("Type of new widget '%1' doesn't "
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
            if (uitype->ParseElement(filename, info, showWarnings))
            {
            }
            else if (info.tagName() == "font" || info.tagName() == "fontdef")
            {
                bool global = (GetGlobalObjectStore() == parent);
                MythFontProperties *font = MythFontProperties::ParseFromXml(
                    filename, info, parent, global, showWarnings);

                if (!global && font)
                {
                    QString name = info.attribute("name");
                    uitype->AddFont(name, font);
                }

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
                     info.tagName() == "progressbar" ||
                     info.tagName() == "webbrowser" ||
                     info.tagName() == "guidegrid" ||
                     info.tagName() == "shape" ||
                     info.tagName() == "editbar")
            {
                ParseUIType(filename, info, info.tagName(),
                            uitype, screen, showWarnings);
            }
            else
            {
                VERBOSE_XML(VB_IMPORTANT, filename, info,
                            LOC_ERR + "Unknown widget type.");
            }
        }
    }

    uitype->Finalize();
    return uitype;
}

bool XMLParseBase::WindowExists(const QString &xmlfile,
                                const QString &windowname)
{
    const QStringList searchpath = GetMythUI()->GetThemeSearchPath();
    QStringList::const_iterator it = searchpath.begin();
    for (; it != searchpath.end(); ++it)
    {
        QString themefile = *it + xmlfile;
        QFile f(themefile);

        if (!f.open(QIODevice::ReadOnly))
            continue;

        QDomDocument doc;
        QString errorMsg;
        int errorLine = 0;
        int errorColumn = 0;

        if (!doc.setContent(&f, false, &errorMsg, &errorLine, &errorColumn))
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("Location: '%1' @ %2 column: %3"
                            "\n\t\t\tError: %4")
                    .arg(qPrintable(themefile)).arg(errorLine).arg(errorColumn)
                    .arg(qPrintable(errorMsg)));
            f.close();
            continue;
        }

        f.close();

        QDomElement docElem = doc.documentElement();
        QDomNode n = docElem.firstChild();
        while (!n.isNull())
        {
            QDomElement e = n.toElement();
            if (!e.isNull())
            {
                if (e.tagName() == "window")
                {
                    QString name = e.attribute("name", "");
                    if (name == windowname)
                        return true;
                }
            }
            n = n.nextSibling();
        }
    }

    return false;
}

bool XMLParseBase::LoadWindowFromXML(const QString &xmlfile,
                                     const QString &windowname,
                                     MythUIType *parent)
{
    bool onlyLoadWindows = true;
    bool showWarnings = true;

    const QStringList searchpath = GetMythUI()->GetThemeSearchPath();
    QStringList::const_iterator it = searchpath.begin();
    for (; it != searchpath.end(); ++it)
    {
        QString themefile = *it + xmlfile;
        VERBOSE(VB_GUI, LOC + "Loading window theme from " + themefile);
        if (doLoad(windowname, parent, themefile,
                   onlyLoadWindows, showWarnings))
        {
            return true;
        }
        else
        {
            VERBOSE(VB_FILE+VB_EXTRA, LOC_ERR + "No theme file " + themefile);
        }
    }

    VERBOSE(VB_IMPORTANT, LOC_ERR +
            QString("Unable to load window '%1' from '%2'")
            .arg(windowname).arg(xmlfile));

    return false;
}

bool XMLParseBase::doLoad(const QString &windowname,
                          MythUIType *parent,
                          const QString &filename,
                          bool onlywindows,
                          bool showWarnings)
{
    QDomDocument doc;
    QFile f(filename);

    if (!f.open(QIODevice::ReadOnly))
        return false;

    QString errorMsg;
    int errorLine = 0;
    int errorColumn = 0;

    if (!doc.setContent(&f, false, &errorMsg, &errorLine, &errorColumn))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Location: '%1' @ %2 column: %3"
                        "\n\t\t\tError: %4")
                .arg(qPrintable(filename)).arg(errorLine).arg(errorColumn)
                .arg(qPrintable(errorMsg)));
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
                if (name.isEmpty())
                {
                    VERBOSE_XML(VB_IMPORTANT, filename, e,
                                LOC_ERR + "Window needs a name");
                    return false;
                }

                if (name == windowname)
                {
                    ParseChildren(filename, e, parent, showWarnings);
                    return true;
                }
            }

            if (!onlywindows)
            {
                QString type = e.tagName();
                if (type == "font" || type == "fontdef")
                {
                    bool global = (GetGlobalObjectStore() == parent);
                    MythFontProperties *font = MythFontProperties::ParseFromXml(
                        filename, e, parent, global, showWarnings);

                    if (!global && font)
                    {
                        QString name = e.attribute("name");
                        parent->AddFont(name, font);
                    }
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
                         type == "progressbar" ||
                         type == "webbrowser" ||
                         type == "guidegrid" ||
                         type == "shape" ||
                         type == "editbar")
                {
                    ParseUIType(filename, e, type, parent, NULL, showWarnings);
                }
                else
                {
                    VERBOSE_XML(VB_IMPORTANT, filename, e,
                                LOC_ERR + "Unknown widget type");
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
    bool ok = false;
    bool loadOnlyWindows = false;
    bool showWarnings = true;

    const QStringList searchpath = GetMythUI()->GetThemeSearchPath();

    QStringList::const_iterator it = searchpath.begin();
    for (; it != searchpath.end(); ++it)
    {
        QString themefile = *it + "base.xml";
        if (doLoad(QString(), GetGlobalObjectStore(), themefile,
                   loadOnlyWindows, showWarnings))
        {
            VERBOSE(VB_GUI, LOC +
                    QString("Loaded base theme from '%1'").arg(themefile));
            // Don't complain about duplicate definitions after first
            // successful load (set showWarnings to false).
            showWarnings = false;
            ok = true;
        }
        else
        {
            VERBOSE(VB_GUI|VB_FILE, LOC_WARN +
                    QString("No theme file '%1'").arg(themefile));
        }
    }
    
    return ok;
}

bool XMLParseBase::CopyWindowFromBase(const QString &windowname,
                                      MythScreenType *win)
{
    MythUIType *ui = GetGlobalObjectStore()->GetChild(windowname);
    if (!ui)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Unable to load window '%1' from base")
                .arg(windowname));
        return false;
    }

    MythScreenType *st = dynamic_cast<MythScreenType *>(ui);
    if (!st)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("UI Object '%1' is not a ScreenType")
                .arg(windowname));
        return false;
    }

    win->CopyFrom(st);
    return true;
}
