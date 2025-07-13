
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
#include <QRadialGradient>

// libmythbase headers
#include "libmythbase/mythconfig.h"
#include "libmythbase/mythlogging.h"

// Mythui headers
#include "mythmainwindow.h"
#include "mythuihelper.h"

/* ui type includes */
#include "mythscreentype.h"
#include "mythuiimage.h"
#include "mythuiprocedural.h"
#include "mythuitext.h"
#include "mythuitextedit.h"
#include "mythuiclock.h"
#include "mythuibuttonlist.h"
#include "mythuibutton.h"
#include "mythuispinbox.h"
#include "mythuicheckbox.h"
#include "mythuiprogressbar.h"
#include "mythuiscrollbar.h"
#include "mythuigroup.h"

#if CONFIG_QTWEBENGINE
#include "mythuiwebbrowser.h"
#endif

#include "mythuiguidegrid.h"
#include "mythuishape.h"
#include "mythuibuttontree.h"
#include "mythuivideo.h"
#include "mythuieditbar.h"
#include "mythfontproperties.h"

#define LOC      QString("XMLParseBase: ")

QString XMLParseBase::getFirstText(QDomElement &element)
{
    for (QDomNode dname = element.firstChild(); !dname.isNull();
         dname = dname.nextSibling())
    {
        QDomText t = dname.toText();
        if (!t.isNull())
            return t.data();
    }
    return {};
}

bool XMLParseBase::parseBool(const QString &text)
{
    QString s = text.toLower();
    return (s == "yes" || s == "true" || (s.toInt() != 0));
}

bool XMLParseBase::parseBool(QDomElement &element)
{
    return parseBool(getFirstText(element));
}

MythPoint XMLParseBase::parsePoint(const QString &text, bool normalize)
{
    MythPoint retval;
    QStringList values = text.split(',', Qt::SkipEmptyParts);
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
    int x = 0;
    int y = 0;
    QSize retval;

    QStringList tmp = text.split(",");
    bool x_ok = false;
    bool y_ok = false;
    if (tmp.size() >= 2)
    {
        x = tmp[0].toInt(&x_ok);
        y = tmp[1].toInt(&y_ok);
    }

    if (x_ok && y_ok)
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
    QStringList values = text.split(',', Qt::SkipEmptyParts);
    if (values.size() == 4)
        retval = MythRect(values[0], values[1], values[2], values[3]);
    if (values.size() == 5)
        retval = MythRect(values[0], values[1], values[2], values[3],
            values[4]);

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
    int alignment = Qt::AlignLeft | Qt::AlignTop;

    QStringList values = text.split(',');

    QStringList::Iterator it;
    for ( it = values.begin(); it != values.end(); ++it )
    {

        QString align = *it;
        align = align.trimmed();
        align = align.toLower();

        if (align == "center" || align == "allcenter")
        {
            alignment &= ~(Qt::AlignHorizontal_Mask | Qt::AlignVertical_Mask);
            alignment |= Qt::AlignCenter;
            break;
        }
        if (align == "justify")
        {
            alignment &= ~Qt::AlignHorizontal_Mask;
            alignment |= Qt::AlignJustify;
        }
        else if (align == "left")
        {
            alignment &= ~Qt::AlignHorizontal_Mask;
            alignment |= Qt::AlignLeft;
        }
        else if (align == "hcenter")
        {
            alignment &= ~Qt::AlignHorizontal_Mask;
            alignment |= Qt::AlignHCenter;
        }
        else if (align == "right")
        {
            alignment &= ~Qt::AlignHorizontal_Mask;
            alignment |= Qt::AlignRight;
        }
        else if (align == "top")
        {
            alignment &= ~Qt::AlignVertical_Mask;
            alignment |= Qt::AlignTop;
        }
        else if (align == "vcenter")
        {
            alignment &= ~Qt::AlignVertical_Mask;
            alignment |= Qt::AlignVCenter;
        }
        else if (align == "bottom")
        {
            alignment &= ~Qt::AlignVertical_Mask;
            alignment |= Qt::AlignBottom;
        }
    }

    return alignment;
}

int XMLParseBase::parseAlignment(QDomElement &element)
{
    return parseAlignment(getFirstText(element));
}

QBrush XMLParseBase::parseGradient(const QDomElement &element)
{
    QBrush brush;
    QString gradientStart = element.attribute("start", "");
    QString gradientEnd = element.attribute("end", "");
    int gradientAlpha = element.attribute("alpha", "255").toInt();
    QString direction = element.attribute("direction", "vertical");

    QGradientStops stops;

    if (!gradientStart.isEmpty())
    {
        auto startColor = QColor(gradientStart);
        startColor.setAlpha(gradientAlpha);
        QGradientStop stop(0.0, startColor);
        stops.append(stop);
    }

    for (QDomNode child = element.firstChild(); !child.isNull();
        child = child.nextSibling())
    {
        QDomElement childElem = child.toElement();
        if (childElem.tagName() == "stop")
        {
            double position = childElem.attribute("position", "0").toDouble();
            QString color = childElem.attribute("color", "");
            int alpha = childElem.attribute("alpha", "-1").toInt();
            if (alpha < 0)
                alpha = gradientAlpha;
            auto stopColor = QColor(color);
            stopColor.setAlpha(alpha);
            QGradientStop stop((position / 100), stopColor);
            stops.append(stop);
        }
    }

    if (!gradientEnd.isEmpty())
    {
        auto endColor = QColor(gradientEnd);
        endColor.setAlpha(gradientAlpha);
        QGradientStop stop(1.0, endColor);
        stops.append(stop);
    }

    if (direction == "radial")
    {
        QRadialGradient gradient;
        gradient.setCoordinateMode(QGradient::ObjectBoundingMode);
        double x1 = 0.5;
        double y1 = 0.5;
        double radius = 0.5;
        gradient.setCenter(x1,y1);
        gradient.setFocalPoint(x1,y1);
        gradient.setRadius(radius);
        gradient.setStops(stops);
        brush = QBrush(gradient);
    }
    else // Linear
    {
        QLinearGradient gradient;
        gradient.setCoordinateMode(QGradient::ObjectBoundingMode);
        double x1 = 0.0;
        double y1 = 0.0;
        double x2 = 0.0;
        double y2 = 0.0;
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
        else // Horizontal
        {
            x1 = 0.0;
            x2 = 1.0;
            y1 = 0.5;
            y2 = 0.5;
        }

        gradient.setStart(x1, y1);
        gradient.setFinalStop(x2, y2);
        gradient.setStops(stops);
        brush = QBrush(gradient);
    }


    return brush;
}

QString XMLParseBase::parseText(QDomElement &element)
{
    QString text = getFirstText(element);

    // Escape xml-escaped newline
    text.replace("\\n", QString("<newline>"));

    // Remove newline whitespace added by
    // xml formatting
    QStringList lines = text.split('\n');
    QStringList::iterator lineIt;

    for (lineIt = lines.begin(); lineIt != lines.end(); ++lineIt)
    {
        (*lineIt) = (*lineIt).trimmed();
    }

    text = lines.join(" ");

    text.replace(QString("<newline>"), QString("\n"));

    return text;
}

static MythUIType *globalObjectStore = nullptr;
static QStringList loadedBaseFiles;

MythUIType *XMLParseBase::GetGlobalObjectStore(void)
{
    if (!globalObjectStore)
        globalObjectStore = new MythUIType(nullptr, "global store");
    return globalObjectStore;
}

void XMLParseBase::ClearGlobalObjectStore(void)
{
    delete globalObjectStore;
    globalObjectStore = nullptr;
    GetGlobalObjectStore();

    // clear any loaded base xml files which will force a reload the next time they are used
    loadedBaseFiles.clear();
}

void XMLParseBase::ParseChildren(const QString &filename,
                                 QDomElement &element,
                                 MythUIType *parent,
                                 bool showWarnings)
{
    if (!parent)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Parent is NULL");
        return;
    }

    QMap<QString, QString> dependsMap;
    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            QString type = info.tagName();
            if (type == "fontdef")
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
                     type == "procedural" ||
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
                     type == "scrollbar" ||
                     type == "webbrowser" ||
                     type == "guidegrid" ||
                     type == "shape" ||
                     type == "editbar" ||
                     type == "video")
            {
                ParseUIType(filename, info, type, parent, nullptr, showWarnings, dependsMap);
            }
            else
            {
                // This will print an error if there is no match.
                parent->ParseElement(filename, info, showWarnings);
            }
        }
    }
    parent->SetDependsMap(dependsMap);
    parent->ConnectDependants(true);
    parent->Finalize();
}

MythUIType *XMLParseBase::ParseUIType(
    const QString &filename,
    QDomElement &element, const QString &type,
    MythUIType *parent,
    MythScreenType *screen,
    bool showWarnings,
    QMap<QString, QString> &parentDependsMap)
{
    QString name = element.attribute("name", "");
    if (name.isEmpty())
    {
        VERBOSE_XML(VB_GENERAL, LOG_ERR, filename, element,
                    "This element requires a name");
        return nullptr;
    }

    MythUIType *olduitype = nullptr;

    // check for name in immediate parent as siblings cannot share names
    if (parent && parent->GetChild(name))
    {
        // if we're the global object store, assume it's just a theme overriding
        // the defaults..
        if (parent == GetGlobalObjectStore())
            return nullptr;

        // Reuse the existing child and reparse
        olduitype = parent->GetChild(name);
    }

    MythUIType *uitype = nullptr;
    MythUIType *base = nullptr;

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
            VERBOSE_XML(VB_GENERAL, LOG_ERR, filename, element,
                       QString("Couldn't find object '%1' to inherit '%2' from")
                        .arg(inherits, name));
            return nullptr;
        }
    }

    QString shadow = element.attribute("shadow", "");

    if (type == "imagetype")
        uitype = new MythUIImage(parent, name);
    else if (type == "procedural")
        uitype = new MythUIProcedural(parent, name);
    else if (type == "textarea")
        uitype = new MythUIText(parent, name);
    else if (type == "group")
        uitype = new MythUIGroup(parent, name);
    else if (type == "textedit")
        uitype = new MythUITextEdit(parent, name);
    else if (type == "button")
        uitype = new MythUIButton(parent, name);
    else if (type == "buttonlist2" || type == "buttonlist")
        uitype = new MythUIButtonList(parent, name, shadow);
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
    else if (type == "scrollbar") {
        uitype = new MythUIScrollBar(parent, name);
#if CONFIG_QTWEBENGINE
    } else if (type == "webbrowser") {
        uitype = new MythUIWebBrowser(parent, name);
#endif
    } else if (type == "guidegrid") {
        uitype = new MythUIGuideGrid(parent, name);
    } else if (type == "shape") {
        uitype = new MythUIShape(parent, name);
    } else if (type == "editbar") {
        uitype = new MythUIEditBar(parent, name);
    } else if (type == "video") {
        uitype = new MythUIVideo(parent, name);
    } else if (type == "window" && parent == GetGlobalObjectStore()) {
        uitype = new MythScreenType(parent, name);
    } else {
        VERBOSE_XML(VB_GENERAL, LOG_ERR, filename, element,
                    "Unknown widget type.");
        return nullptr;
    }

    if (!uitype)
    {
        VERBOSE_XML(VB_GENERAL, LOG_ERR, filename, element,
                    "Failed to instantiate widget type.");
        return nullptr;
    }

    if (olduitype && parent)
    {
        if (typeid(*olduitype) != typeid(*uitype))
        {
            VERBOSE_XML(VB_GENERAL, LOG_ERR, filename, element,
                        QString("Duplicate name: '%1' in parent '%2'")
                        .arg(name, parent->objectName()));
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
            VERBOSE_XML(VB_GENERAL, LOG_ERR, filename, element,
                      QString("Type of new widget '%1' doesn't match old '%2'")
                        .arg(name, inherits));
            if (parent)
                parent->DeleteChild(uitype);
            return nullptr;
        }
        uitype->CopyFrom(base);

    }

    QString dependee = element.attribute("depends", "");
    if (!dependee.isEmpty())
        parentDependsMap.insert(name, dependee);

    QFileInfo fi(filename);
    uitype->SetXMLName(name);
    uitype->SetXMLLocation(fi.fileName(), element.lineNumber());

    // If this was copied from another uitype then it already has a depends
    // map so we want to append to that one
    QMap<QString, QString> dependsMap = uitype->GetDependsMap();
    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "fontdef")
            {
                bool global = (GetGlobalObjectStore() == parent);
                MythFontProperties *font = MythFontProperties::ParseFromXml(
                    filename, info, parent, global, showWarnings);

                if (!global && font)
                {
                    QString name2 = info.attribute("name");
                    uitype->AddFont(name2, font);
                }

                delete font;
            }
            else if (info.tagName() == "imagetype" ||
                     info.tagName() == "procedural" ||
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
                     info.tagName() == "scrollbar" ||
                     info.tagName() == "webbrowser" ||
                     info.tagName() == "guidegrid" ||
                     info.tagName() == "shape" ||
                     info.tagName() == "editbar" ||
                     info.tagName() == "video")
            {
                ParseUIType(filename, info, info.tagName(),
                            uitype, screen, showWarnings, dependsMap);
            }
            else
            {
                // This will print an error if there is no match.
                uitype->ParseElement(filename, info, showWarnings);
            }
        }
    }
    uitype->SetDependsMap(dependsMap);

    uitype->Finalize();
    return uitype;
}

bool XMLParseBase::WindowExists(const QString &xmlfile,
                                const QString &windowname)
{
    const QStringList searchpath = GetMythUI()->GetThemeSearchPath();
    for (const auto & dir : std::as_const(searchpath))
    {
        QString themefile = dir + xmlfile;
        QFile f(themefile);

        if (!f.open(QIODevice::ReadOnly))
            continue;

        QDomDocument doc;
#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
        QString errorMsg;
        int errorLine = 0;
        int errorColumn = 0;

        if (!doc.setContent(&f, false, &errorMsg, &errorLine, &errorColumn))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Location: '%1' @ %2 column: %3"
                        "\n\t\t\tError: %4")
                    .arg(qPrintable(themefile)).arg(errorLine).arg(errorColumn)
                    .arg(qPrintable(errorMsg)));
            f.close();
            continue;
        }
#else
        auto parseResult = doc.setContent(&f);
        if (!parseResult)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Location: '%1' @ %2 column: %3"
                        "\n\t\t\tError: %4")
                    .arg(qPrintable(themefile)).arg(parseResult.errorLine)
                    .arg(parseResult.errorColumn)
                    .arg(qPrintable(parseResult.errorMessage)));
            f.close();
            continue;
        }
#endif
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
    for (const auto & dir : std::as_const(searchpath))
    {
        QString themefile = dir + xmlfile;
        LOG(VB_GUI, LOG_INFO, LOC + QString("Loading window %1 from %2").arg(windowname, themefile));
        if (doLoad(windowname, parent, themefile,
                   onlyLoadWindows, showWarnings))
        {
            return true;
        }
        LOG(VB_FILE, LOG_ERR, LOC + "No theme file " + themefile);
    }

    LOG(VB_GENERAL, LOG_ERR, LOC +
        QString("Unable to load window '%1' from '%2'")
            .arg(windowname, xmlfile));

    return false;
}

bool XMLParseBase::doLoad(const QString &windowname,
                          MythUIType *parent,
                          const QString &filename,
                          bool onlyLoadWindows,
                          bool showWarnings)
{
    QDomDocument doc;
    QFile f(filename);

    if (!f.open(QIODevice::ReadOnly))
        return false;

#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
    QString errorMsg;
    int errorLine = 0;
    int errorColumn = 0;

    if (!doc.setContent(&f, false, &errorMsg, &errorLine, &errorColumn))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Location: '%1' @ %2 column: %3"
                    "\n\t\t\tError: %4")
                .arg(qPrintable(filename)).arg(errorLine).arg(errorColumn)
                .arg(qPrintable(errorMsg)));
        f.close();
        return false;
    }
#else
    auto parseResult = doc.setContent(&f);
    if (!parseResult)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Location: '%1' @ %2 column: %3"
                    "\n\t\t\tError: %4")
                .arg(qPrintable(filename)).arg(parseResult.errorLine)
                .arg(parseResult.errorColumn)
                .arg(qPrintable(parseResult.errorMessage)));
        f.close();
        return false;
    }
#endif

    f.close();

    QDomElement docElem = doc.documentElement();
    QDomNode n = docElem.firstChild();
    while (!n.isNull())
    {
        QDomElement e = n.toElement();
        if (!e.isNull())
        {
            if (e.tagName() == "include")
            {
                QString include = getFirstText(e);

                if (!include.isEmpty())
                     LoadBaseTheme(include);
            }

            if (onlyLoadWindows && e.tagName() == "window")
            {
                QString name = e.attribute("name", "");
                QString include = e.attribute("include", "");
                if (name.isEmpty())
                {
                    VERBOSE_XML(VB_GENERAL, LOG_ERR, filename, e,
                                "Window needs a name");
                    return false;
                }

                if (!include.isEmpty())
                    LoadBaseTheme(include);

                if (name == windowname)
                {
                    ParseChildren(filename, e, parent, showWarnings);
                    return true;
                }
            }

            if (!onlyLoadWindows)
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
                         type == "procedural" ||
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
                         type == "scrollbar" ||
                         type == "webbrowser" ||
                         type == "guidegrid" ||
                         type == "shape" ||
                         type == "editbar" ||
                         type == "video")
                {

                    // We don't want widgets in base.xml
                    // depending on each other so ignore dependsMap
                    QMap<QString, QString> dependsMap;
                    MythUIType *uitype = nullptr;
                    uitype = ParseUIType(filename, e, type, parent,
                                         nullptr, showWarnings, dependsMap);
                    if (uitype)
                        uitype->ConnectDependants(true);
                }
                else
                {
                    VERBOSE_XML(VB_GENERAL, LOG_ERR, filename, e,
                                "Unknown widget type");
                }
            }
        }
        n = n.nextSibling();
    }
    return !onlyLoadWindows;
}

bool XMLParseBase::LoadBaseTheme(void)
{
    bool ok = false;
    bool loadOnlyWindows = false;
    bool showWarnings = true;

    const QStringList searchpath = GetMythUI()->GetThemeSearchPath();
    for (const auto & dir : std::as_const(searchpath))
    {
        QString themefile = dir + "base.xml";
        if (doLoad(QString(), GetGlobalObjectStore(), themefile,
                   loadOnlyWindows, showWarnings))
        {
            LOG(VB_GUI, LOG_INFO, LOC +
                QString("Loaded base theme from '%1'").arg(themefile));
            // Don't complain about duplicate definitions after first
            // successful load (set showWarnings to false).
            showWarnings = false;
            ok = true;
        }
        else
        {
            LOG(VB_GUI | VB_FILE, LOG_WARNING, LOC +
                QString("No theme file '%1'").arg(themefile));
        }
    }

    return ok;
}

bool XMLParseBase::LoadBaseTheme(const QString &baseTheme)
{
    LOG(VB_GUI, LOG_INFO, LOC +
        QString("Asked to load base file from '%1'").arg(baseTheme));

    if (loadedBaseFiles.contains(baseTheme))
    {
        LOG(VB_GUI, LOG_INFO, LOC +
            QString("Base file already loaded '%1'").arg(baseTheme));
        return true;
    }

    bool ok = false;
    bool loadOnlyWindows = false;
    bool showWarnings = true;

    const QStringList searchpath = GetMythUI()->GetThemeSearchPath();
    for (const auto & dir : std::as_const(searchpath))
    {
        QString themefile = dir + baseTheme;
        if (doLoad(QString(), GetGlobalObjectStore(), themefile,
                   loadOnlyWindows, showWarnings))
        {
            LOG(VB_GUI, LOG_INFO, LOC +
                QString("Loaded base theme from '%1'").arg(themefile));
            // Don't complain about duplicate definitions after first
            // successful load (set showWarnings to false).
            showWarnings = false;
            ok = true;
        }
        else
        {
            LOG(VB_GUI | VB_FILE, LOG_WARNING, LOC +
                QString("No theme file '%1'").arg(themefile));
        }
    }

    if (ok)
        loadedBaseFiles.append(baseTheme);

    return ok;
}

bool XMLParseBase::CopyWindowFromBase(const QString &windowname,
                                      MythScreenType *win)
{
    MythUIType *ui = GetGlobalObjectStore()->GetChild(windowname);
    if (!ui)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Unable to load window '%1' from base") .arg(windowname));
        return false;
    }

    auto *st = dynamic_cast<MythScreenType *>(ui);
    if (!st)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("UI Object '%1' is not a ScreenType") .arg(windowname));
        return false;
    }

    win->CopyFrom(st);
    return true;
}
