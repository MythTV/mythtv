#include <deque>
using namespace std;

#include <cmath>
#include <cstdlib>

#include <QApplication>
#include <QPixmap>
#include <QList>
#include <QFile>

#include "uilistbtntype.h"
#include "xmlparse.h"
#include "util.h"
#include "mythcorecontext.h"
#include "mythfontproperties.h"
#include "mythuihelper.h"
#include "x11colors.h"
#include "mythverbose.h"

#ifdef USING_MINGW
#undef LoadImage
#endif

#define LOC QString("XMLParse: ")
#define LOC_WARN QString("XMLParse, Warning: ")
#define LOC_ERR QString("XMLParse, Error: ")

XMLParse::XMLParse(void) : wmult(0.0), hmult(0.0), usetrans(-1)
{
    allTypes = new vector<LayerSet *>;
    ui = GetMythUI();
}

XMLParse::~XMLParse()
{
    vector<LayerSet *>::iterator i = allTypes->begin();
    for (; i != allTypes->end(); i++)
    {
        LayerSet *type = (*i);
        if (type)
            delete type;
    }
    delete allTypes;
}

bool XMLParse::LoadTheme(QDomElement &ele, QString winName, QString specialfile)
{
    usetrans = gCoreContext->GetNumSetting("PlayBoxTransparency", 1);

    fontSizeType = gCoreContext->GetSetting("ThemeFontSizeType", "default");

    QStringList searchpath = ui->GetThemeSearchPath();
    for (QStringList::const_iterator ii = searchpath.begin();
        ii != searchpath.end(); ii++)
    {
        QString themefile = *ii + specialfile + "ui.xml";
        if (doLoadTheme(ele, winName, themefile))
        {
            VERBOSE(VB_GENERAL, LOC + QString("LoadTheme using '%1'")
                    .arg(themefile));
            return true;
        }
    }

    return false;
}

bool XMLParse::doLoadTheme(QDomElement &ele, QString winName, QString themeFile)
{
    QDomDocument doc;
    QFile f(themeFile);

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
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Parsing: %1 at line: %2 column: %3")
                .arg(themeFile).arg(errorLine).arg(errorColumn) +
                QString("\n\t\t\t%1").arg(errorMsg));
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
            if (e.tagName() == "window")
            {
                QString name = e.attribute("name", "");
                if (name.isNull() || name.isEmpty())
                {
                    VERBOSE(VB_IMPORTANT, LOC_WARN + "Window needs a name");
                    return false;
                }

                if (name == winName)
                {
                    ele = e;
                    return true;
                }
            }
            else
            {
                VERBOSE(VB_IMPORTANT, LOC_WARN +
                        QString("Unknown element: %1").arg(e.tagName()));
                return false;
            }
        }
        n = n.nextSibling();
    }

    return false;
}

QString XMLParse::getFirstText(QDomElement &element)
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

void XMLParse::parseFont(QDomElement &element)
{
    QString name;
    QString face;
    QString bold;
    QString ital;
    QString under;

    int size = -1;
    int sizeSmall = -1;
    int sizeBig = -1;
    QPoint shadowOffset = QPoint(0, 0);
    QString color = "#ffffff";
    QString dropcolor = "#000000";
    QString hint;
    QFont::StyleHint styleHint = QFont::Helvetica;

    bool haveSizeSmall = false;
    bool haveSizeBig = false;
    bool haveSize = false;
    bool haveFace = false;
    bool haveColor = false;
    bool haveDropColor = false;
    bool haveBold = false;
    bool haveShadow = false;
    bool haveItal = false;
    bool haveUnder = false;

    fontProp *baseFont = NULL;

    name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "Font needs a name");
        return;
    }

    QString base =  element.attribute("base", "");
    if (base.size())
    {
        baseFont = GetFont(base);
        if (!baseFont)
        {
            VERBOSE(VB_IMPORTANT, LOC_WARN +
                    QString("Specified base font '%1' "
                            "does not exist for font '%2'")
                    .arg(base).arg(face));
            return;
        }
    }

    face = element.attribute("face", "");
    if (face.isNull() || face.isEmpty())
    {
        if (!baseFont)
        {
            VERBOSE(VB_IMPORTANT, LOC_WARN + "Font needs a face");
            return;
        }
    }
    else
    {
        haveFace = true;
    }

    hint = element.attribute("stylehint", "");
    if (hint.size())
    {
        styleHint = (QFont::StyleHint)hint.toInt();
    }

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "size")
            {
                haveSize = true;
                size = getFirstText(info).toInt();
            }
            else if (info.tagName() == "size:small")
            {
                haveSizeSmall = true;
                sizeSmall = getFirstText(info).toInt();
            }
            else if (info.tagName() == "size:big")
            {
                haveSizeBig = true;
                sizeBig = getFirstText(info).toInt();
            }
            else if (info.tagName() == "color")
            {
                haveColor = true;
                color = getFirstText(info);
            }
            else if (info.tagName() == "dropcolor")
            {
                haveDropColor = true;
                dropcolor = getFirstText(info);
            }
            else if (info.tagName() == "shadow")
            {
                haveShadow = true;
                shadowOffset = parsePoint(getFirstText(info));
                shadowOffset.setX((int)(shadowOffset.x() * wmult));
                shadowOffset.setY((int)(shadowOffset.y() * hmult));
            }
            else if (info.tagName() == "bold")
            {
                haveBold = true;
                bold = getFirstText(info);
            }
            else if (info.tagName() == "italics")
            {
                haveItal = true;
                ital = getFirstText(info);
            }
            else if (info.tagName() == "underline")
            {
                haveUnder = true;
                under = getFirstText(info);
            }
            else
            {
                VERBOSE(VB_IMPORTANT, LOC_WARN +
                        QString("Unknown tag '%1' in font")
                        .arg(info.tagName()));
                return;
            }
        }
    }

    fontProp *testFont = GetFont(name, false);
    if (testFont)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("MythTV already has a font called: '%1'").arg(name));
        return;
    }

    fontProp newFont;

    if (baseFont)
        newFont = *baseFont;

    if ( haveSizeSmall && fontSizeType == "small")
    {
        if (sizeSmall > 0)
            size = sizeSmall;
    }
    else if (haveSizeBig && fontSizeType == "big")
    {
        if (sizeBig > 0)
            size = sizeBig;
    }

    if (size < 0 && !baseFont)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Font size must be > 0");
        return;
    }

    if (baseFont && !haveSize)
        size = baseFont->face.pointSize();
    else
        size = GetMythMainWindow()->NormalizeFontSize(size);

    // If we don't have to, don't load the font.
    if (!haveFace && baseFont)
    {
        newFont.face = baseFont->face;
        if (haveSize)
            newFont.face.setPointSize(size);
    }
    else
    {
        QFont temp(face, size);
        temp.setStyleHint(styleHint, QFont::PreferAntialias);

        if (!temp.exactMatch())
            temp = QFont(QFontInfo(QApplication::font()).family(), size);

        newFont.face = temp;
    }

    if (baseFont && !haveBold)
        newFont.face.setBold(baseFont->face.bold());
    else
    {
        if (bold.toLower() == "yes")
            newFont.face.setBold(true);
        else
            newFont.face.setBold(false);
    }

    if (baseFont && !haveItal)
        newFont.face.setItalic(baseFont->face.italic());
    else
    {
        if (ital.toLower() == "yes")
            newFont.face.setItalic(true);
        else
            newFont.face.setItalic(false);
    }

    if (baseFont && !haveUnder)
        newFont.face.setUnderline(baseFont->face.underline());
    else
    {
        if (under.toLower() == "yes")
            newFont.face.setUnderline(true);
        else
            newFont.face.setUnderline(false);
    }

    if (haveColor)
    {
        QColor foreColor(color);
        newFont.color = foreColor;
    }

    if (haveDropColor)
    {
        QColor dropColor(dropcolor);
        newFont.dropColor = dropColor;
    }

    if (haveShadow)
        newFont.shadowOffset = shadowOffset;

    fontMap[name] = newFont;
}

void XMLParse::parseImage(LayerSet *container, QDomElement &element)
{
    int context = -1;
    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "Image needs a name");
        return;
    }

    QString order = element.attribute("draworder", "");
    if (order.isNull() || order.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "Image needs an order");
        return;
    }

    QString filename = "";
    QPoint pos = QPoint(0, 0);

    QPoint scale = QPoint(-1, -1);
    QPoint skipin = QPoint(0, 0);

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "context")
            {
                context = getFirstText(info).toInt();
            }
            else if (info.tagName() == "filename")
            {
                filename = getFirstText(info);
            }
            else if (info.tagName() == "position")
            {
                pos = parsePoint(getFirstText(info));
                pos.setX((int)(pos.x() * wmult));
                pos.setY((int)(pos.y() * hmult));
            }
            else if (info.tagName() == "staticsize")
            {
                scale = parsePoint(getFirstText(info));
            }
            else if (info.tagName() == "skipin")
            {
                skipin = parsePoint(getFirstText(info));
                skipin.setX((int)(skipin.x() * wmult));
                skipin.setY((int)(skipin.y() * hmult));
            }
            else
            {
                VERBOSE(VB_IMPORTANT, LOC_WARN +
                        QString("Unknown: %1 in image").arg(info.tagName()));
                return;
            }
        }
    }

    UIImageType *image = new UIImageType(name, filename, order.toInt(), pos);
    image->SetScreen(wmult, hmult);
    if (scale.x() != -1 || scale.y() != -1)
        image->SetSize(scale.x(), scale.y());
    image->SetSkip(skipin.x(), skipin.y());
    QString flex = element.attribute("fleximage", "");
    if (flex.size())
    {
        if (flex.toLower() == "yes")
            image->SetFlex(true);
        else
            image->SetFlex(false);
    }

    image->LoadImage();

    QString visible = element.attribute("visible", "");
    if (visible.size())
    {
        if (visible.toLower() == "yes")
            image->show();
        else
            image->hide();
    }

    if (context != -1)
    {
        image->SetContext(context);
    }
    image->SetParent(container);
    container->AddType(image);
    container->bumpUpLayers(order.toInt());
}

void XMLParse::parseRepeatedImage(LayerSet *container, QDomElement &element)
{
    int orientation = 0;
    int context = -1;
    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "Repeated Image needs a name");
        return;
    }

    QString order = element.attribute("draworder", "");
    if (order.isNull() || order.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "Repeated Image needs an order");
        return;
    }

    QString filename = "";
    QPoint pos = QPoint(0, 0);

    QPoint scale = QPoint(-1, -1);
    QPoint skipin = QPoint(0, 0);

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "context")
            {
                context = getFirstText(info).toInt();
            }
            else if (info.tagName() == "filename")
            {
                filename = getFirstText(info);
            }
            else if (info.tagName() == "position")
            {
                pos = parsePoint(getFirstText(info));
                pos.setX((int)(pos.x() * wmult));
                pos.setY((int)(pos.y() * hmult));
            }
            else if (info.tagName() == "staticsize")
            {
                scale = parsePoint(getFirstText(info));
            }
            else if (info.tagName() == "skipin")
            {
                skipin = parsePoint(getFirstText(info));
                skipin.setX((int)(skipin.x() * wmult));
                skipin.setY((int)(skipin.y() * hmult));
            }
            else if (info.tagName() == "orientation")
            {
                QString orient_string = getFirstText(info).toLower();
                if (orient_string == "lefttoright")
                {
                    orientation = 0;
                }
                if (orient_string == "righttoleft")
                {
                    orientation = 1;
                }
                if (orient_string == "bottomtotop")
                {
                    orientation = 2;
                }
                if (orient_string == "toptobottom")
                {
                    orientation = 3;
                }
            }
            else
            {
                VERBOSE(VB_IMPORTANT, LOC_WARN +
                        QString("Unknown: %1 in repeated image")
                        .arg(info.tagName()));
                return;
            }
        }
    }

    UIRepeatedImageType *image = new UIRepeatedImageType(name, filename, order.toInt(), pos);
    image->SetScreen(wmult, hmult);
    if (scale.x() != -1 || scale.y() != -1)
        image->SetSize(scale.x(), scale.y());
    image->SetSkip(skipin.x(), skipin.y());
    QString flex = element.attribute("fleximage", "");
    if (flex.size())
    {
        if (flex.toLower() == "yes")
            image->SetFlex(true);
        else
            image->SetFlex(false);
    }

    image->LoadImage();
    if (context != -1)
    {
        image->SetContext(context);
    }
    image->setOrientation(orientation);
    image->SetParent(container);
    container->AddType(image);
    container->bumpUpLayers(order.toInt());
}

bool XMLParse::parseDefaultCategoryColors(QMap<QString, QString> &catColors)
{
    QFile f;
    QStringList searchpath = ui->GetThemeSearchPath();
    for (QStringList::const_iterator ii = searchpath.begin();
        ii != searchpath.end(); ii++)
    {
        f.setFileName(*ii + "categories.xml");
        if (f.open(QIODevice::ReadOnly))
            break;
    }
    if (f.handle() == -1)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + QString("Unable to open '%1'")
                .arg(f.fileName()));
        return false;
    }

    QDomDocument doc;
    QString errorMsg;
    int errorLine = 0;
    int errorColumn = 0;

    if (!doc.setContent(&f, false, &errorMsg, &errorLine, &errorColumn))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Parsing colors: %1 at line: %2 column: %3")
                .arg(f.fileName()).arg(errorLine).arg(errorColumn) +
                QString("\n\t\t\t%1").arg(errorMsg));
        f.close();
        return false;
    }

    f.close();

    QDomElement element = doc.documentElement();
    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull() && info.tagName() == "catcolor")
        {
            QString cat = "";
            QString col = "";
            cat = info.attribute("category");
            col = info.attribute("color");

            catColors[cat.toLower()] = col;
        }
    }

    return true;
}

void XMLParse::parseImageGrid(LayerSet *container, QDomElement &element)
{
    int context = -1;
    QString align = "";
    QString activeFont = "";
    QString inactiveFont = "";
    QString selectedFont = "";
    QString color = "";
    QString textposition = "bottom";
    QRect area;
    int textheight = 0;
    int rowcount = 3;
    int columncount = 3;
    int padding = 10;
    bool cutdown = true;
    bool multiline = false;
    bool showChecks = false;
    bool showSelected = false;
    bool showScrollArrows = false;
    QString defaultImage = "";
    QString normalImage = "";
    QString selectedImage = "";
    QString highlightedImage = "";

    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "Image Grid needs a name");
        return;
    }

    QString order = element.attribute("draworder", "");
    if (order.isNull() || order.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "Image Grid needs an order");
        return;
    }

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "context")
            {
                context = getFirstText(info).toInt();
            }
            else if (info.tagName() == "activefont")
            {
                activeFont = getFirstText(info);
            }
            else if (info.tagName() == "inactivefont")
            {
                inactiveFont = getFirstText(info);
            }

            else if (info.tagName() == "selectedfont")
            {
                selectedFont = getFirstText(info);
            }
            else if (info.tagName() == "area")
            {
                area = parseRect(getFirstText(info));
                normalizeRect(area);
            }
            else if (info.tagName() == "columncount")
            {
                columncount = getFirstText(info).toInt();
            }
            else if (info.tagName() == "rowcount")
            {
                rowcount = getFirstText(info).toInt();
            }
            else if (info.tagName() == "padding")
            {
                padding = getFirstText(info).toInt();
            }
            else if (info.tagName() == "align")
            {
                align = getFirstText(info);
            }
            else if (info.tagName() == "cutdown")
            {
                if (getFirstText(info).toLower() == "no")
                    cutdown = false;
            }
            else if (info.tagName() == "showchecks")
            {
                if (getFirstText(info).toLower() == "yes")
                    showChecks = true;
            }
            else if (info.tagName() == "showselected")
            {
                if (getFirstText(info).toLower() == "yes")
                    showSelected = true;
            }
            else if (info.tagName() == "showscrollarrows")
            {
                if (getFirstText(info).toLower() == "yes")
                    showScrollArrows = true;
            }
            else if (info.tagName() == "textheight")
            {
                textheight = getFirstText(info).toInt();
            }
            else if (info.tagName() == "textposition")
            {
                textposition = getFirstText(info);
            }
            else if (info.tagName() == "multiline")
            {
                if (getFirstText(info).toLower() == "yes")
                    multiline = true;
            }
            else if (info.tagName() == "image")
            {
                QString imgname = "";
                QString file = "";

                imgname = info.attribute("function", "");
                if (imgname.isNull() || imgname.isEmpty())
                {
                    VERBOSE(VB_IMPORTANT, LOC_WARN +
                            "Image in a image grid needs a function");
                    return;
                }

                file = info.attribute("filename", "");
                if (file.isNull() || file.isEmpty())
                {
                    VERBOSE(VB_IMPORTANT, LOC_WARN +
                            "Image in a image grid needs a filename");
                    return;
                }

                if (imgname.toLower() == "normal")
                {
                    normalImage = file;
                }

                if (imgname.toLower() == "selected")
                {
                    selectedImage = file;
                }

                if (imgname.toLower() == "highlighted")
                {
                    highlightedImage = file;
                }

                if (imgname.toLower() == "default")
                {
                    defaultImage = file;
                }
            }
            else
            {
                VERBOSE(VB_IMPORTANT, LOC_WARN +
                        QString("Unknown tag '%1' in image grid")
                        .arg(info.tagName()));
                return;
            }
        }
    }
    fontProp *activeProp = GetFont(activeFont);
    if (!activeProp)
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                QString("Unknown active font: '%1' in image grid: '%2'")
                .arg(activeFont).arg(name));
        return;
    }

    fontProp *selectedProp = GetFont(selectedFont);
    if (!selectedProp)
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                QString("Unknown selected font: '%1' in image grid: '%2'")
                .arg(selectedFont).arg(name));
        return;
    }

    fontProp *inactiveProp = GetFont(inactiveFont);
    if (!inactiveProp)
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                QString("Unknown inactive font: '%1' in image grid: '%2'")
                .arg(inactiveFont).arg(name));
        return;
    }

    UIImageGridType *grid = new UIImageGridType(name, order.toInt());
    grid->SetScreen(wmult, hmult);
    grid->setActiveFont(activeProp);
    grid->setSelectedFont(selectedProp);
    grid->setInactiveFont(inactiveProp);
    grid->setCutDown(cutdown);
    grid->setShowChecks(showChecks);
    grid->setShowSelected(showSelected);
    grid->setShowScrollArrows(showScrollArrows);
    grid->setArea(area);
    grid->setTextHeight(textheight);
    grid->setPadding(padding);
    grid->setRowCount(rowcount);
    grid->setColumnCount(columncount);
    grid->setDefaultImage(defaultImage);
    grid->setNormalImage(normalImage);
    grid->setSelectedImage(selectedImage);
    grid->setHighlightedImage(highlightedImage);

    int jst = Qt::AlignLeft | Qt::AlignTop;
    if (multiline == true)
        jst = Qt::TextWordWrap;

    if (align.size())
    {
        if (align.toLower() == "center")
            grid->setJustification(Qt::AlignCenter | jst);
        else if (align.toLower() == "right")
            grid->setJustification(Qt::AlignRight | jst);
        else if (align.toLower() == "allcenter")
            grid->setJustification(Qt::AlignHCenter | Qt::AlignVCenter | jst);
        else if (align.toLower() == "vcenter")
            grid->setJustification(Qt::AlignVCenter | jst);
        else if (align.toLower() == "hcenter")
            grid->setJustification(Qt::AlignHCenter | jst);
    }
    else
        grid->setJustification(jst);

    if (textposition == "top")
        grid->setTextPosition(UIImageGridType::textPosTop);
    else
        grid->setTextPosition(UIImageGridType::textPosBottom);

    if (context != -1)
    {
        grid->SetContext(context);
    }
    container->AddType(grid);
    grid->calculateScreenArea();
    grid->recalculateLayout();
}

fontProp *XMLParse::GetFont(const QString &text, bool checkGlobal)
{
    fontProp *ret;
    if (fontMap.contains(text))
        ret = &fontMap[text];
    else if (checkGlobal && globalFontMap.contains(text))
        ret = &globalFontMap[text];
    else
        ret = NULL;
    return ret;
}

void XMLParse::normalizeRect(QRect &rect)
{
    rect.setWidth((int)(rect.width() * wmult));
    rect.setHeight((int)(rect.height() * hmult));
    rect.moveTopLeft(QPoint((int)(rect.x() * wmult),
                             (int)(rect.y() * hmult)));
    rect = rect.normalized();
}

QPoint XMLParse::parsePoint(QString text)
{
    int x, y;
    QPoint retval(0, 0);
    QByteArray tmp = text.toLocal8Bit();
    if (sscanf(tmp.constData(), "%d,%d", &x, &y) == 2)
        retval = QPoint(x, y);
    return retval;
}

QRect XMLParse::parseRect(QString text)
{
    int x, y, w, h;
    QRect retval(0, 0, 0, 0);
    QByteArray tmp = text.toLocal8Bit();
    if (sscanf(tmp.constData(), "%d,%d,%d,%d", &x, &y, &w, &h) == 4)
        retval = QRect(x, y, w, h);

    return retval;
}


void XMLParse::parseContainer(QDomElement &element, QString &newname, int &context, QRect &area)
{
    context = -1;
    QString debug = "";
    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "Container needs a name");
        return;
    }

    LayerSet *container = GetSet(name);
    if (container)
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                QString("Container: '%1' already exists").arg(name));
        return;
    }
    newname = name;

    container = new LayerSet(name);

    layerMap[name] = container;

    bool ok = true;
    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "debug")
            {
                debug = getFirstText(info);
                if (debug.toLower() == "yes")
                    container->SetDebug(true);
            }
            else if (info.tagName() == "context")
            {
                context = getFirstText(info).toInt();
            }
            else if (info.tagName() == "image")
            {
                parseImage(container, info);
            }
            else if (info.tagName() == "repeatedimage")
            {
                parseRepeatedImage(container, info);
            }
            else if (info.tagName() == "listarea")
            {
                parseListArea(container, info);
            }
            else if (info.tagName() == "listbtnarea")
            {
                parseListBtnArea(container, info);
            }
            else if (info.tagName() == "listtreearea")
            {
                parseListTreeArea(container, info);
            }
            else if (info.tagName() == "textarea")
            {
                parseTextArea(container, info);
            }
            else if (info.tagName() == "remoteedit")
            {
                parseRemoteEdit(container, info);
            }
            else if (info.tagName() == "statusbar")
            {
                parseStatusBar(container, info);
            }
            else if (info.tagName() == "managedtreelist")
            {
                parseManagedTreeList(container, info);
            }
            else if (info.tagName() == "pushbutton")
            {
                parsePushButton(container, info);
            }
            else if (info.tagName() == "textbutton")
            {
                parseTextButton(container, info);
            }
            else if (info.tagName() == "checkbox")
            {
                parseCheckBox(container, info);
            }
            else if (info.tagName() == "selector")
            {
                parseSelector(container, info);
            }
            else if (info.tagName() == "blackhole")
            {
                parseBlackHole(container, info);
            }
            else if (info.tagName() == "area")
            {
                area = parseRect(getFirstText(info));
                normalizeRect(area);
                container->SetAreaRect(area);
            }
            else if (info.tagName() == "keyboard")
            {
                parseKeyboard(container, info);
            }
            else if (info.tagName() == "imagegrid")
            {
                parseImageGrid(container, info);
            }
            else
            {
                VERBOSE(VB_IMPORTANT,
                        QString("Container '%1' contains unknown child: '%2'")
                        .arg(name).arg(info.tagName()));
                ok = false;
            }
        }
    }
    if (!ok)
    {
        VERBOSE(VB_IMPORTANT, QString("Could not parse container '%1'. "
                                      "Ignoring.").arg(name));
        return;
    }

    if (context != -1)
        container->SetContext(context);

//    container->SetAreaRect(area);
    allTypes->push_back(container);
}

void XMLParse::parseTextArea(LayerSet *container, QDomElement &element)
{
    int context = -1;
    QRect area = QRect(0, 0, 0, 0);
    QRect altArea = QRect(0, 0, 0, 0);
    QPoint shadowOffset = QPoint(0, 0);
    QString font = "";
    QString cutdown = "";
    QString value = "";
    QString statictext = "";
    QString multiline = "";
    int draworder = 0;

    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "Text area needs a name");
        return;
    }

    QString layerNum = element.attribute("draworder", "");
    if (layerNum.isNull() || layerNum.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "Text area needs a draworder");
        return;
    }
    draworder = layerNum.toInt();

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "context")
            {
                context = getFirstText(info).toInt();
            }
            else if (info.tagName() == "area")
            {
                area = parseRect(getFirstText(info));
                normalizeRect(area);
            }
            else if (info.tagName() == "altarea")
            {
                altArea = parseRect(getFirstText(info));
                normalizeRect(altArea);
            }
            else if (info.tagName() == "font")
            {
                font = getFirstText(info);
            }
            else if (info.tagName() == "value")
            {
                if ((value.isNull() || value.isEmpty()) &&
                    info.attribute("lang","").isEmpty())
                {
                    value = qApp->translate(
                        "ThemeUI", getFirstText(info).toLatin1().constData());
                }
                else if (info.attribute("lang","").toLower() ==
                         ui->GetLanguageAndVariant())
                {
                    value = getFirstText(info);
                }
                else if (info.attribute("lang","").toLower() ==
                         ui->GetLanguage())
                {
                    value = getFirstText(info);
                }
            }
            else if (info.tagName() == "cutdown")
            {
                cutdown = getFirstText(info);
            }
            else if (info.tagName() == "multiline")
            {
                multiline = getFirstText(info);
            }
            else if (info.tagName() == "shadow")
            {
                shadowOffset = parsePoint(getFirstText(info));
                shadowOffset.setX((int)(shadowOffset.x() * wmult));
                shadowOffset.setY((int)(shadowOffset.y() * hmult));
            }
            else
            {
                VERBOSE(VB_IMPORTANT, LOC_WARN +
                        QString("Unknown tag '%1' in textarea")
                        .arg(info.tagName()));
                return;
            }
        }
    }

    fontProp *testfont = GetFont(font);
    if (!testfont)
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                QString("Unknown font: '%1' in textarea '%2'")
                .arg(font).arg(name));
        return;
    }

    UITextType *text = new UITextType(name, testfont, value, draworder, area,
                                      altArea);
    text->SetScreen(wmult, hmult);
    if (context != -1)
    {
        text->SetContext(context);
    }
    if (multiline.toLower() == "yes")
        text->SetJustification(Qt::TextWordWrap);
    if (value.size())
        text->SetText(value);
    if (cutdown.toLower() == "no")
        text->SetCutDown(false);

    QString align = element.attribute("align", "");
    if (align.size())
    {
        int jst = (Qt::AlignTop | Qt::AlignLeft);
        if (multiline.toLower() == "yes")
        {
            jst = Qt::TextWordWrap;
        }
        if (align.toLower() == "center")
            text->SetJustification(jst | Qt::AlignCenter);
        else if (align.toLower() == "right")
            text->SetJustification(jst | Qt::AlignRight);
        else if (align.toLower() == "left")
            text->SetJustification(jst | Qt::AlignLeft);
        else if (align.toLower() == "allcenter")
            text->SetJustification(jst | Qt::AlignHCenter | Qt::AlignVCenter);
        else if (align.toLower() == "vcenter")
            text->SetJustification(jst | Qt::AlignVCenter);
        else if (align.toLower() == "hcenter")
            text->SetJustification(jst | Qt::AlignHCenter);
    }
    align = "";
    text->SetParent(container);
    text->calculateScreenArea();
    container->AddType(text);
}

void XMLParse::parseRemoteEdit(LayerSet *container, QDomElement &element)
{
    int context = -1;
    QRect area = QRect(0, 0, 0, 0);
    QString font = "";
    QString value = "";
    int draworder = 0;
    QColor selectedColor = QColor(0, 255, 255);
    QColor unselectedColor = QColor(100, 100, 100);
    QColor specialColor = QColor(255, 0, 0);

    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "RemoteEdit needs a name");
        return;
    }

    QString layerNum = element.attribute("draworder", "");
    if (layerNum.isNull() || layerNum.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "RemoteEdit needs a draworder");
        return;
    }
    draworder = layerNum.toInt();

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "context")
            {
                context = getFirstText(info).toInt();
            }
            else if (info.tagName() == "area")
            {
                area = parseRect(getFirstText(info));
                normalizeRect(area);
            }
            else if (info.tagName() == "font")
            {
                font = getFirstText(info);
            }
            else if (info.tagName() == "value")
            {
                if ((value.isNull() || value.isEmpty()) &&
                    info.attribute("lang","").isEmpty())
                {
                    value = qApp->translate(
                        "ThemeUI", getFirstText(info).toLatin1().constData());
                }
                else if (info.attribute("lang","").toLower() ==
                         ui->GetLanguageAndVariant())
                {
                    value = getFirstText(info);
                }
                else if (info.attribute("lang","").toLower() ==
                         ui->GetLanguage())
                {
                    value = getFirstText(info);
                }
            }
            else if (info.tagName() == "charcolors")
            {
                QString selected = "";
                QString unselected = "";
                QString special = "";
                selected = info.attribute("selected");
                unselected = info.attribute("unselected");
                special = info.attribute("special");

                if (!selected.isEmpty())
                    selectedColor = createColor(selected);

                if (!unselected.isEmpty())
                    unselectedColor = createColor(unselected);

                if (!special.isEmpty())
                    specialColor = createColor(special);
            }

            else
            {
                VERBOSE(VB_IMPORTANT, LOC_WARN +
                        QString("Unknown tag '%1' in RemoteEdit")
                        .arg(info.tagName()));
                return;
            }
        }
    }

    fontProp *testfont = GetFont(font);
    if (!testfont)
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                QString("Unknown font '%1' in RemoteEdit '%2'")
                .arg(font).arg(name));
        return;
    }

    UIRemoteEditType *edit = new UIRemoteEditType(name, testfont, value, draworder, area);
    edit->SetScreen(wmult, hmult);
    if (context != -1)
    {
        edit->SetContext(context);
    }
    if (value.size())
        edit->setText(value);

    edit->SetParent(container);
    edit->setCharacterColors(unselectedColor, selectedColor, specialColor);
    edit->calculateScreenArea();
    container->AddType(edit);
}

void XMLParse::parseListArea(LayerSet *container, QDomElement &element)
{
    int context = -1;
    int item_cnt = 0;
    QRect area = QRect(0, 0, 0, 0);
    QString force_color = "";
    QString act_font = "", in_font = "";
    QString statictext = "";
    int padding = 0;
    int draworder = 0;
    QMap<int, int> columnWidths;
    QMap<int, int> columnContexts;
    QMap<QString, QString> fontFunctions;
    QMap<QString, fontProp> theFonts;
    int colCnt = -1;

    QRect fill_select_area = QRect(0, 0, 0, 0);
    QColor fill_select_color = QColor(255, 255, 255);
    int fill_type = -1;

    QPoint uparrow_loc;
    QPoint dnarrow_loc;
    QPoint select_loc;
    QPoint rightarrow_loc;
    QPoint leftarrow_loc;

    QPixmap *uparrow_img = NULL;
    QPixmap *dnarrow_img = NULL;
    QPixmap *select_img = NULL;
    QPixmap *right_img = NULL;
    QPixmap *left_img = NULL;

    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "List area needs a name");
        return;
    }

    QString layerNum = element.attribute("draworder", "");
    if (layerNum.isNull() || layerNum.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "List area needs a draworder");
        return;
    }
    draworder = layerNum.toInt();

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "context")
            {
                context = getFirstText(info).toInt();
            }
            else if (info.tagName() == "area")
            {
                area = parseRect(getFirstText(info));
                normalizeRect(area);
            }
            else if (info.tagName() == "activefont")
            {
                act_font = getFirstText(info);
            }
            else if (info.tagName() == "inactivefont")
            {
                in_font = getFirstText(info);
            }
            else if (info.tagName() == "items")
            {
                item_cnt = getFirstText(info).toInt();
            }
            else if (info.tagName() == "columnpadding")
            {
                padding = getFirstText(info).toInt();
            }
            else if (info.tagName() == "fcnfont")
            {
                QString fontname = "";
                QString fontfcn = "";

                fontname = info.attribute("name", "");
                fontfcn = info.attribute("function", "");

                if (fontname.isNull() || fontname.isEmpty())
                {
                    VERBOSE(VB_IMPORTANT, LOC_WARN + "FcnFont needs a name");
                    return;
                }

                if (fontfcn.isNull() || fontfcn.isEmpty())
                {
                    VERBOSE(VB_IMPORTANT, LOC_WARN +
                            "FcnFont needs a function");
                    return;
                }
                fontFunctions[fontfcn] = fontname;
            }
            else if (info.tagName() == "fill")
            {
                QString fillfcn = "";
                QString fillcolor = "";
                QString fillarea = "";
                QString filltype = "";

                fillfcn = info.attribute("function", "");
                fillcolor = info.attribute("color", "#ffffff");
                fillarea = info.attribute("area", "");
                filltype = info.attribute("type", "");

                if (fillfcn.isNull() || fillfcn.isEmpty())
                {
                    VERBOSE(VB_IMPORTANT, LOC_WARN + "Fill needs a function");
                    return;
                }
                if (fillcolor.isNull() || fillcolor.isEmpty())
                {
                    VERBOSE(VB_IMPORTANT, LOC_WARN + "Fill needs a color");
                    return;
                }
                if (filltype == "5050")
                    fill_type = 1;
                if (fill_type == -1)
                    fill_type = 1;

                if (fillarea.isNull() || fillarea.isEmpty())
                {
                    fill_select_area = area;
                }
                else
                {
                    fill_select_area = parseRect(fillarea);
                    normalizeRect(fill_select_area);
                }
                fill_select_color = createColor(fillcolor);

            }
            else if (info.tagName() == "image")
            {
                QString imgname = "";
                QString imgpoint = "";
                QString imgfile = "";

                imgname = info.attribute("function", "");
                if (imgname.isNull() || imgname.isEmpty())
                {
                    VERBOSE(VB_IMPORTANT, LOC_WARN + "Image needs a function");
                    return;
                }

                imgfile = info.attribute("filename", "");
                if (imgfile.isNull() || imgfile.isEmpty())
                {
                    VERBOSE(VB_IMPORTANT, LOC_WARN + "Image needs a filename");
                    return;
                }


                imgpoint = info.attribute("location", "");
                if (imgpoint.isNull() || imgpoint.isEmpty())
                {
                    VERBOSE(VB_IMPORTANT, LOC_WARN + "Image needs a location");
                    return;
                }

                if (imgname.toLower() == "selectionbar")
                {
                    select_img = ui->LoadScalePixmap(imgfile);
                    select_loc = parsePoint(imgpoint);
                    select_loc.setX((int)(select_loc.x() * wmult));
                    select_loc.setY((int)(select_loc.y() * hmult));
                }
                if (imgname.toLower() == "uparrow")
                {
                    uparrow_img = ui->LoadScalePixmap(imgfile);
                    uparrow_loc = parsePoint(imgpoint);
                    uparrow_loc.setX((int)(uparrow_loc.x() * wmult));
                    uparrow_loc.setY((int)(uparrow_loc.y() * hmult));
                }
                if (imgname.toLower() == "downarrow")
                {
                    dnarrow_img = ui->LoadScalePixmap(imgfile);
                    dnarrow_loc = parsePoint(imgpoint);
                    dnarrow_loc.setX((int)(dnarrow_loc.x() * wmult));
                    dnarrow_loc.setY((int)(dnarrow_loc.y() * hmult));
                }
                if (imgname.toLower() == "leftarrow")
                {
                    left_img = ui->LoadScalePixmap(imgfile);
                    leftarrow_loc = parsePoint(imgpoint);
                    leftarrow_loc.setX((int)(leftarrow_loc.x() * wmult));
                    leftarrow_loc.setY((int)(leftarrow_loc.y() * hmult));

                }
                else if (imgname.toLower() == "rightarrow")
                {
                    right_img = ui->LoadScalePixmap(imgfile);
                    rightarrow_loc = parsePoint(imgpoint);
                    rightarrow_loc.setX((int)(rightarrow_loc.x() * wmult));
                    rightarrow_loc.setY((int)(rightarrow_loc.y() * hmult));
                }
            }
            else if (info.tagName() == "column")
            {
                QString colnum = "";
                QString colwidth = "";
                QString colcontext = "";
                colnum = info.attribute("number", "");
                if (colnum.isNull() || colnum.isEmpty())
                {
                    VERBOSE(VB_IMPORTANT, LOC_WARN + "Column needs a number");
                    return;
                }

                colwidth = info.attribute("width", "");
                if (colwidth.isNull() || colwidth.isEmpty())
                {
                    VERBOSE(VB_IMPORTANT, LOC_WARN + "Column needs a width");
                    return;
                }

                colcontext = info.attribute("context", "");
                if (colcontext.size())
                {
                    columnContexts[colnum.toInt()] = colcontext.toInt();
                }
                else
                    columnContexts[colnum.toInt()] = -1;

                columnWidths[colnum.toInt()] = (int)(wmult*colwidth.toInt());
                if (colnum.toInt() > colCnt)
                    colCnt = colnum.toInt();
            }
            else
            {
                VERBOSE(VB_IMPORTANT, LOC_WARN +
                        QString("Unknown tag '%1' in listarea")
                        .arg(info.tagName()));
                return;
            }
        }
    }

    UIListType *list = new UIListType(name, area, draworder);
    list->SetCount(item_cnt);
    list->SetScreen(wmult, hmult);
    list->SetColumnPad(padding);
    if (select_img)
    {
        list->SetImageSelection(*select_img, select_loc);
        delete select_img;
    }

    if (uparrow_img)
    {
        list->SetImageUpArrow(*uparrow_img, uparrow_loc);
        delete uparrow_img;
    }

    if (dnarrow_img)
    {
        list->SetImageDownArrow(*dnarrow_img, dnarrow_loc);
        delete dnarrow_img;
    }

    if (right_img)
    {
        list->SetImageRightArrow(*right_img, rightarrow_loc);
        delete right_img;
    }

    if (left_img)
    {
        list->SetImageLeftArrow(*left_img, leftarrow_loc);
        delete left_img;
    }

    typedef QMap<QString,QString> fontdata;
    fontdata::Iterator it;
    for ( it = fontFunctions.begin(); it != fontFunctions.end(); ++it )
    {
        fontProp *testFont = GetFont(*it);
        if (testFont)
            theFonts[*it] = *testFont;
    }
    if (theFonts.size() > 0)
        list->SetFonts(fontFunctions, theFonts);
    if (fill_type != -1)
        list->SetFill(fill_select_area, fill_select_color, fill_type);
    if (context != -1)
    {
        list->SetContext(context);
    }

    for (int i = 0; i <= colCnt; i++)
    {
         if (columnWidths[i] > 0)
            list->SetColumnWidth(i, columnWidths[i]);
        if (columnContexts[i] != -1)
            list->SetColumnContext(i, columnContexts[i]);
        else
            list->SetColumnContext(i, context);
    }

    if (colCnt == -1)
        list->SetColumnContext(1, -1);

    list->SetParent(container);
    list->calculateScreenArea();
    container->AddType(list);

    // the selection image is only drawn on layer 8
    container->bumpUpLayers(8);
}

LayerSet *XMLParse::GetSet(const QString &text)
{
    LayerSet *ret = NULL;
    if (layerMap.contains(text))
        ret = layerMap[text];

    return ret;
}

void XMLParse::parseStatusBar(LayerSet *container, QDomElement &element)
{
    int context = -1;
    int orientation = 0;
    int imgFillSpace = 0;
    QPixmap *imgFiller = NULL;
    QPixmap *imgContainer = NULL;

    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "StatusBar needs a name");
        return;
    }

    QString order = element.attribute("draworder", "");
    if (order.isNull() || order.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "StatusBar needs an order");
        return;
    }

    QPoint pos = QPoint(0, 0);

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "context")
            {
                context = getFirstText(info).toInt();
            }
            else if (info.tagName() == "container")
            {
                QString confile = getFirstText(info);
                QString flex = info.attribute("fleximage", "");
                if (flex.size())
                {
                    if (flex.toLower() == "yes")
                    {
                        int pathStart = confile.lastIndexOf('/');
                        if (usetrans == 1)
                        {
                            if (pathStart < 0 )
                                confile = "trans-" + confile;
                            else
                                confile.replace(pathStart, 1, "/trans-");
                        }
                        else
                        {
                            if (pathStart < 0 )
                                confile = "solid-" + confile;
                            else
                                confile.replace(pathStart, 1, "/solid-");
                        }

                        imgContainer = ui->LoadScalePixmap(confile);
                    }
                    else
                        imgContainer = ui->LoadScalePixmap(confile);
                }
                else
                {
                    imgContainer = ui->LoadScalePixmap(confile);
                }
            }
            else if (info.tagName() == "position")
            {
                pos = parsePoint(getFirstText(info));
                pos.setX((int)(pos.x() * wmult));
                pos.setY((int)(pos.y() * hmult));
            }
            else if (info.tagName() == "fill")
            {
                QString fillfile = getFirstText(info);
                imgFillSpace = info.attribute("whitespace", "").toInt();

                QString flex = info.attribute("fleximage", "");
                if (flex.size())
                {
                    if (flex.toLower() == "yes")
                    {
                        if (usetrans == 1)
                            fillfile = "trans-" + fillfile;
                        else
                            fillfile = "solid-" + fillfile;

                        imgFiller = ui->LoadScalePixmap(fillfile);
                     }
                     else
                        imgFiller = ui->LoadScalePixmap(fillfile);
                }
                else
                {
                    imgFiller = ui->LoadScalePixmap(fillfile);
                }
            }
            else if (info.tagName() == "orientation")
            {
                QString orient_string = getFirstText(info).toLower();
                if (orient_string == "lefttoright")
                {
                    orientation = 0;
                }
                if (orient_string == "righttoleft")
                {
                    orientation = 1;
                }
                if (orient_string == "bottomtotop")
                {
                    orientation = 2;
                }
                if (orient_string == "toptobottom")
                {
                    orientation = 3;
                }
            }
            else
            {
                VERBOSE(VB_IMPORTANT, LOC_WARN +
                        QString("Unknown tag '%1' in statusbar")
                        .arg(info.tagName()));
                return;
            }
        }
    }

    UIStatusBarType *sb = new UIStatusBarType(name, pos, order.toInt());
    sb->SetScreen(wmult, hmult);
    sb->SetFiller(imgFillSpace);
    if (imgContainer)
    {
        sb->SetContainerImage(*imgContainer);
        delete imgContainer;
    }
    if (imgFiller)
    {
        sb->SetFillerImage(*imgFiller);
        delete imgFiller;
    }
    if (context != -1)
    {
        sb->SetContext(context);
    }
    sb->SetParent(container);
    sb->calculateScreenArea();
    sb->setOrientation(orientation);
    container->AddType(sb);
    container->bumpUpLayers(order.toInt());
}

struct TreeIcon { int i; QPixmap *img;};
void XMLParse::parseManagedTreeList(LayerSet *container, QDomElement &element)
{
    QRect area;
    QRect binarea;
    int bins = 1;
    int context = -1;
    int selectPadding = 0;
    bool selectScale = true;

    QPixmap *uparrow_img = NULL;
    QPixmap *downarrow_img = NULL;
    QPixmap *leftarrow_img = NULL;
    QPixmap *rightarrow_img = NULL;
    QPixmap *icon_img = NULL;
    QPixmap *select_img = NULL;
    QPoint selectPoint(0,0);
    QPoint upArrowPoint(0,0);
    QPoint downArrowPoint(0,0);
    QPoint rightArrowPoint(0,0);
    QPoint leftArrowPoint(0,0);


    //
    //  A Map to store the geometry of
    //  the bins as we parse
    //

    typedef QMap<int, QRect> CornerMap;
    deque<TreeIcon> iconList;
    CornerMap bin_corners;
    bin_corners.clear();

    //
    //  Some maps to store fonts as we parse
    //

    QMap<QString, QString> fontFunctions;
    QMap<QString, fontProp> theFonts;

    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "ManagedTreeList needs a name");
        return;
    }

    QString order = element.attribute("draworder", "");
    if (order.isNull() || order.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "ManagedTreeList needs an order");
        return;
    }

    QString bins_string = element.attribute("bins", "");
    if (bins_string.toInt() > 0)
    {
        bins = bins_string.toInt();
    }

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "area")
            {
                area = parseRect(getFirstText(info));
                normalizeRect(area);
            }
            else if (info.tagName() == "image")
            {
                QString imgname = "";
                QString file = "";
                int     imgnumber = -1;

                imgname = info.attribute("function", "");
                if (imgname.isNull() || imgname.isEmpty())
                {
                    VERBOSE(VB_IMPORTANT, LOC_WARN + "Image needs a function");
                    return;
                }

                file = info.attribute("filename", "");
                if (file.isNull() || file.isEmpty())
                {
                    VERBOSE(VB_IMPORTANT, LOC_WARN + "Image needs a filename");
                    return;
                }

                QString imgNumStr = info.attribute("number", "");
                imgnumber = imgNumStr.toInt();

                if (info.tagName() == "context")
                {
                    context = getFirstText(info).toInt();
                }

                if (imgname.toLower() == "selectionbar")
                {
                    QString imgpoint;
                    QString imgPad;
                    QString imgScale;
                    imgpoint = info.attribute("location", "");
                    if (imgpoint.size())
                    {
                        selectPoint = parsePoint(imgpoint);
                        selectPoint.setX((int)(selectPoint.x() * wmult));
                        selectPoint.setY((int)(selectPoint.y() * hmult));
                    }

                    imgPad = info.attribute("padding", "");
                    if (imgPad.size())
                    {
                        selectPadding = (int)(imgPad.toInt() * hmult);
                    }

                    imgScale = info.attribute("scale", "");
                    if (imgScale.toLower() == "no")
                    {
                        selectScale = false;
                    }

                    select_img = ui->LoadScalePixmap(file);
                    if (!select_img)
                    {
                        VERBOSE(VB_IMPORTANT, LOC_WARN +
                                QString("Can't locate 'select' image '%1'")
                                .arg(file));
                    }
                }
                else if (imgname.toLower() == "uparrow")
                {
                    QString imgpoint;
                    imgpoint = info.attribute("location", "");
                    if (imgpoint.size())
                    {
                        upArrowPoint = parsePoint(imgpoint);
                        upArrowPoint.setX((int)(upArrowPoint.x() * wmult));
                        upArrowPoint.setY((int)(upArrowPoint.y() * hmult));
                    }

                    uparrow_img = ui->LoadScalePixmap(file);
                    if (!uparrow_img)
                    {
                        VERBOSE(VB_IMPORTANT, LOC_WARN +
                                QString("Can't locate 'up arrow' image '%1'")
                                .arg(file));
                    }
                }
                else if (imgname.toLower() == "downarrow")
                {
                    QString imgpoint;
                    imgpoint = info.attribute("location", "");
                    if (imgpoint.size())
                    {
                        downArrowPoint = parsePoint(imgpoint);
                        downArrowPoint.setX((int)(downArrowPoint.x() * wmult));
                        downArrowPoint.setY((int)(downArrowPoint.y() * hmult));
                    }
                    downarrow_img = ui->LoadScalePixmap(file);
                    if (!downarrow_img)
                    {
                        VERBOSE(VB_IMPORTANT, LOC_WARN +
                                QString("Can't locate 'down arrow' image '%1'")
                                .arg(file));
                    }
                }
                else if (imgname.toLower() == "leftarrow")
                {
                    QString imgpoint;
                    imgpoint = info.attribute("location", "");
                    if (imgpoint.size())
                    {
                        leftArrowPoint = parsePoint(imgpoint);
                        leftArrowPoint.setX((int)(leftArrowPoint.x() * wmult));
                        leftArrowPoint.setY((int)(leftArrowPoint.y() * hmult));
                    }
                    leftarrow_img = ui->LoadScalePixmap(file);
                    if (!leftarrow_img)
                    {
                        VERBOSE(VB_IMPORTANT, LOC_WARN +
                                QString("Can't locate 'left arrow' image '%1'")
                                .arg(file));
                    }
                }
                else if (imgname.toLower() == "rightarrow")
                {
                    QString imgpoint;
                    imgpoint = info.attribute("location", "");
                    if (imgpoint.size())
                    {
                        rightArrowPoint = parsePoint(imgpoint);
                        rightArrowPoint.setX((int)(rightArrowPoint.x() * wmult));
                        rightArrowPoint.setY((int)(rightArrowPoint.y() * hmult));
                    }
                    rightarrow_img = ui->LoadScalePixmap(file);
                    if (!rightarrow_img)
                    {
                        VERBOSE(VB_IMPORTANT, LOC_WARN +
                                QString("Can't locate 'right arrow' image '%1'")
                                .arg(file));
                    }
                }
                else if ((imgname.toLower() == "icon") && (imgnumber != -1))
                {
                    icon_img = ui->LoadScalePixmap(file);
                    if (!icon_img)
                    {
                        VERBOSE(VB_IMPORTANT, LOC_WARN +
                                QString("Can't locate 'icon' image '%1'")
                                .arg(file));
                    }
                    else
                    {
                        iconList.push_back(TreeIcon());
                        iconList.back().img = icon_img;
                        iconList.back().i = imgnumber;
                    }
                }
                else
                {
                    VERBOSE(VB_IMPORTANT, LOC_WARN +
                            QString("Unknown image tag whose function is '%1'")
                            .arg(imgname));
                }
            }
            else if (info.tagName() == "bin")
            {
                QString whichbin_string = info.attribute("number", "");
                int whichbin = whichbin_string.toInt();
                if (whichbin < 1)
                {
                    VERBOSE(VB_IMPORTANT, LOC_WARN +
                            "Bad setting for bin number in bin tag");
                    return;
                }
                if (whichbin > bins + 1)
                {
                    VERBOSE(VB_IMPORTANT, LOC_WARN +
                            "Attempt to set bin with a reference "
                            "larger than number of bins" );
                    return;
                }
                for (QDomNode child = info.firstChild(); !child.isNull();
                child = child.nextSibling())
                {
                    QDomElement info = child.toElement();
                    if (!info.isNull())
                    {
                        if (info.tagName() == "area")
                        {
                            binarea = parseRect(getFirstText(info));
                            normalizeRect(binarea);
                            bin_corners[whichbin] = binarea;
                        }
                        else if (info.tagName() == "fcnfont")
                        {
                            QString fontname = "";
                            QString fontfcn = "";

                            fontname = info.attribute("name", "");
                            fontfcn = info.attribute("function", "");

                            if (fontname.isNull() || fontname.isEmpty())
                            {
                                VERBOSE(VB_IMPORTANT, LOC_WARN +
                                        "FcnFont needs a name");
                                return;
                            }

                            if (fontfcn.isNull() || fontfcn.isEmpty())
                            {
                                VERBOSE(VB_IMPORTANT, LOC_WARN +
                                        "FcnFont needs a function");
                                return;
                            }
                            QString a_string = QString("bin%1-%2")
                                .arg(whichbin).arg(fontfcn);
                            fontFunctions[a_string] = fontname;
                        }
                        else
                        {
                            VERBOSE(VB_IMPORTANT, LOC_WARN +
                                    QString("Unknown tag '%1' in tree list")
                                    .arg(info.tagName()));
                            return;
                        }
                    }
                }
            }
            else
            {
                VERBOSE(VB_IMPORTANT, LOC_WARN +
                        QString("Unknown tag '%1' in ManagedTreeList")
                        .arg(info.tagName()));
                return;
            }
        }
    }


    UIManagedTreeListType *mtl = new UIManagedTreeListType(name);
    mtl->SetScreen(wmult, hmult);
    mtl->setArea(area);
    mtl->setBins(bins);
    mtl->setBinAreas(bin_corners);
    mtl->SetOrder(order.toInt());
    mtl->SetParent(container);
    mtl->SetContext(context);

    //
    //  Perform moegreen/jdanner font magic
    //

    typedef QMap<QString,QString> fontdata;
    fontdata::Iterator it;
    for ( it = fontFunctions.begin(); it != fontFunctions.end(); ++it )
    {
        fontProp *testFont = GetFont(*it);
        if (testFont)
            theFonts[*it] = *testFont;
    }

    if (theFonts.size() > 0)
        mtl->setFonts(fontFunctions, theFonts);

    if (select_img)
    {
        mtl->setSelectScale(selectScale);
        mtl->setSelectPadding(selectPadding);
        mtl->setSelectPoint(selectPoint);
        mtl->setHighlightImage(*select_img);
        delete select_img;
    }

    if (!uparrow_img)
        uparrow_img = new QPixmap();
    if (!downarrow_img)
        downarrow_img = new QPixmap();
    if (!leftarrow_img)
        leftarrow_img = new QPixmap();
    if (!rightarrow_img)
        rightarrow_img = new QPixmap();

    mtl->setUpArrowOffset(upArrowPoint);
    mtl->setDownArrowOffset(downArrowPoint);
    mtl->setLeftArrowOffset(leftArrowPoint);
    mtl->setRightArrowOffset(rightArrowPoint);
    mtl->setArrowImages(*uparrow_img, *downarrow_img, *leftarrow_img,
                        *rightarrow_img);

    delete uparrow_img;
    delete downarrow_img;
    delete leftarrow_img;
    delete rightarrow_img;

    // Add in the icon images
    while (!iconList.empty())
    {
        mtl->addIcon(iconList.front().i, iconList.front().img);
        iconList.pop_front();
    }

    mtl->makeHighlights();
    mtl->calculateScreenArea();
    container->AddType(mtl);
}

void XMLParse::parsePushButton(LayerSet *container, QDomElement &element)
{
    int context = -1;
    QPoint pos = QPoint(0, 0);
    QPixmap *image_on = NULL;
    QPixmap *image_off = NULL;
    QPixmap *image_pushed = NULL;
    QPixmap *image_pushedon = NULL;

    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "PushButton needs a name");
        return;
    }

    QString order = element.attribute("draworder", "");
    if (order.isNull() || order.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "PushButton needs an order");
        return;
    }

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "context")
            {
                context = getFirstText(info).toInt();
            }
            else if (info.tagName() == "position")
            {
                pos = parsePoint(getFirstText(info));
                pos.setX((int)(pos.x() * wmult));
                pos.setY((int)(pos.y() * hmult));
            }
            else if (info.tagName() == "image")
            {
                QString imgname = "";
                QString file = "";

                imgname = info.attribute("function", "");
                if (imgname.isNull() || imgname.isEmpty())
                {
                    VERBOSE(VB_IMPORTANT, LOC_WARN +
                            "Image in a push button needs a function");
                    return;
                }

                file = info.attribute("filename", "");
                if (file.isNull() || file.isEmpty())
                {
                    VERBOSE(VB_IMPORTANT, LOC_WARN +
                            "Image in a push button needs a filename");
                    return;
                }

                if (imgname.toLower() == "on")
                {
                    image_on = ui->LoadScalePixmap(file);
                    if (!image_on)
                    {
                        VERBOSE(VB_IMPORTANT, LOC_WARN +
                                QString("Can't locate 'on' image '%1'")
                                .arg(file));
                    }
                }
                if (imgname.toLower() == "off")
                {
                    image_off = ui->LoadScalePixmap(file);
                    if (!image_off)
                    {
                        VERBOSE(VB_IMPORTANT, LOC_WARN +
                                QString("Can't locate 'off' image '%1'")
                                .arg(file));
                    }
                }
                if (imgname.toLower() == "pushed")
                {
                    image_pushed = ui->LoadScalePixmap(file);
                    if (!image_pushed)
                    {
                        VERBOSE(VB_IMPORTANT, LOC_WARN +
                                QString("Can't locate 'pushed' image '%1'")
                                .arg(file));
                    }
                }
                if (imgname.toLower() == "pushedon")
                {
                    image_pushedon = ui->LoadScalePixmap(file);
                    if (!image_pushedon)
                    {
                        VERBOSE(VB_IMPORTANT, LOC_WARN +
                                QString("Can't locate 'pushed on' image '%1'")
                                .arg(file));
                    }
                }
            }
            else
            {
                VERBOSE(VB_IMPORTANT, LOC_WARN +
                        QString("Unknown tag '%1' in PushButton")
                        .arg(info.tagName()));
                return;
            }
        }
    }

    if (!image_on)
        image_on = new QPixmap();
    if (!image_off)
        image_off = new QPixmap();
    if (!image_pushed)
        image_pushed = new QPixmap();
    if (!image_pushedon)
        image_pushedon = new QPixmap();

    UIPushButtonType *pbt = new UIPushButtonType(name, *image_on, *image_off,
                                                 *image_pushed,
                                                 *image_pushedon);

    delete image_on;
    delete image_off;
    delete image_pushed;
    delete image_pushedon;

    pbt->SetScreen(wmult, hmult);
    pbt->setPosition(pos);
    pbt->SetOrder(order.toInt());
    if (context != -1)
    {
        pbt->SetContext(context);
    }
    pbt->SetParent(container);
    pbt->calculateScreenArea();
    container->AddType(pbt);
}

void XMLParse::parseTextButton(LayerSet *container, QDomElement &element)
{
    int context = -1;
    QString font = "";
    QPoint pos = QPoint(0, 0);
    QPixmap *image_on = NULL;
    QPixmap *image_off = NULL;
    QPixmap *image_pushed = NULL;

    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "TextButton needs a name");
        return;
    }

    QString order = element.attribute("draworder", "");
    if (order.isNull() || order.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "TextButton needs an order");
        return;
    }

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "context")
            {
                context = getFirstText(info).toInt();
            }
            else if (info.tagName() == "position")
            {
                pos = parsePoint(getFirstText(info));
                pos.setX((int)(pos.x() * wmult));
                pos.setY((int)(pos.y() * hmult));
            }
            else if (info.tagName() == "font")
            {
                font = getFirstText(info);
            }
            else if (info.tagName() == "image")
            {
                QString imgname = "";
                QString file = "";

                imgname = info.attribute("function", "");
                if (imgname.isNull() || imgname.isEmpty())
                {
                    VERBOSE(VB_IMPORTANT, LOC_WARN +
                            "Image in a text button needs a function");
                    return;
                }

                file = info.attribute("filename", "");
                if (file.isNull() || file.isEmpty())
                {
                    VERBOSE(VB_IMPORTANT, LOC_WARN +
                            "Image in a text button needs a filename");
                    return;
                }

                if (imgname.toLower() == "on")
                {
                    image_on = ui->LoadScalePixmap(file);
                    if (!image_on)
                    {
                        VERBOSE(VB_IMPORTANT, LOC_WARN +
                                QString("Can't locate 'on' image '%1'")
                                .arg(file));
                    }
                }
                if (imgname.toLower() == "off")
                {
                    image_off = ui->LoadScalePixmap(file);
                    if (!image_off)
                    {
                        VERBOSE(VB_IMPORTANT, LOC_WARN +
                                QString("Can't locate 'off' image '%1'")
                                .arg(file));
                    }
                }
                if (imgname.toLower() == "pushed")
                {
                    image_pushed = ui->LoadScalePixmap(file);

                    if (!image_pushed)
                    {
                        VERBOSE(VB_IMPORTANT, LOC_WARN +
                                QString("Can't locate 'pushed' image '%1'")
                                .arg(file));
                    }
                }
            }
            else
            {
                VERBOSE(VB_IMPORTANT, LOC_WARN +
                        QString("Unknown tag '%1' in textbutton")
                        .arg(info.tagName()));
                return;
            }
        }
    }

    fontProp *testfont = GetFont(font);
    if (!testfont)
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                QString("Unknown font '%1' in textbutton '%2'")
                .arg(font).arg(name));
        return;
    }

    if (!image_on)
        image_on = new QPixmap();
    if (!image_off)
        image_off = new QPixmap();
    if (!image_pushed)
        image_pushed = new QPixmap();

    UITextButtonType *tbt = new UITextButtonType(name, *image_on, *image_off,
                                                 *image_pushed);

    delete image_on;
    delete image_off;
    delete image_pushed;

    tbt->SetScreen(wmult, hmult);
    tbt->setPosition(pos);
    tbt->setFont(testfont);
    tbt->SetOrder(order.toInt());
    if (context != -1)
    {
        tbt->SetContext(context);
    }
    tbt->SetParent(container);
    tbt->calculateScreenArea();
    container->AddType(tbt);
}

void XMLParse::parseCheckBox(LayerSet *container, QDomElement &element)
{
    int context = -1;
    QPoint pos = QPoint(0, 0);
    QPixmap *image_checked = NULL;
    QPixmap *image_unchecked = NULL;
    QPixmap *image_checked_high = NULL;
    QPixmap *image_unchecked_high = NULL;

    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "CheckBox needs a name");
        return;
    }

    QString order = element.attribute("draworder", "");
    if (order.isNull() || order.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "CheckBox needs an order");
        return;
    }

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "context")
            {
                context = getFirstText(info).toInt();
            }
            else if (info.tagName() == "position")
            {
                pos = parsePoint(getFirstText(info));
                pos.setX((int)(pos.x() * wmult));
                pos.setY((int)(pos.y() * hmult));
            }
            else if (info.tagName() == "image")
            {
                QString imgname = "";
                QString file = "";

                imgname = info.attribute("function", "");
                if (imgname.isNull() || imgname.isEmpty())
                {
                    VERBOSE(VB_IMPORTANT, LOC_WARN +
                            "Image in a CheckBox needs a function");
                    return;
                }

                file = info.attribute("filename", "");
                if (file.isNull() || file.isEmpty())
                {
                    VERBOSE(VB_IMPORTANT, LOC_WARN +
                            "Image in a CheckBox needs a filename");
                    return;
                }

                if (imgname.toLower() == "checked")
                {
                    image_checked = ui->LoadScalePixmap(file);
                    if (!image_checked)
                    {
                        VERBOSE(VB_IMPORTANT, LOC_WARN +
                                QString("Can't locate 'checked' image '%1'")
                                .arg(file));
                    }
                }
                if (imgname.toLower() == "unchecked")
                {
                    image_unchecked = ui->LoadScalePixmap(file);
                    if (!image_unchecked)
                    {
                        VERBOSE(VB_IMPORTANT, LOC_WARN +
                                QString("Can't locate 'unchecked' image '%1'")
                                .arg(file));
                    }
                }
                if (imgname.toLower() == "checked_high")
                {
                    image_checked_high = ui->LoadScalePixmap(file);
                    if (!image_checked_high)
                    {
                        VERBOSE(VB_IMPORTANT, LOC_WARN +
                                QString("Can't locate 'checked high' "
                                        "image '%1'").arg(file));
                    }
                }
                if (imgname.toLower() == "unchecked_high")
                {
                    image_unchecked_high = ui->LoadScalePixmap(file);
                    if (!image_unchecked_high)
                    {
                        VERBOSE(VB_IMPORTANT, LOC_WARN +
                                QString("Can't locate 'unchecked high' "
                                        "image '%1'").arg(file));
                    }
                }
            }
            else
            {
                VERBOSE(VB_IMPORTANT, LOC_WARN +
                        QString("Unknown tag '%1' in checkbox")
                        .arg(info.tagName()));
                return;
            }
        }
    }

    if (!image_checked)
        image_checked = new QPixmap();
    if (!image_unchecked)
        image_unchecked = new QPixmap();
    if (!image_checked_high)
        image_checked_high = new QPixmap();
    if (!image_unchecked_high)
        image_unchecked_high = new QPixmap();

    UICheckBoxType *cbt = new UICheckBoxType(name,
                                             *image_checked, *image_unchecked,
                                             *image_checked_high, *image_unchecked_high);

    delete image_checked;
    delete image_unchecked;
    delete image_checked_high;
    delete image_unchecked_high;

    cbt->SetScreen(wmult, hmult);
    cbt->setPosition(pos);
    cbt->SetOrder(order.toInt());
    if (context != -1)
    {
        cbt->SetContext(context);
    }
    cbt->SetParent(container);
    cbt->calculateScreenArea();
    container->AddType(cbt);
}

void XMLParse::parseSelector(LayerSet *container, QDomElement &element)
{
    int context = -1;
    QString font = "";
    QRect area;
    QPixmap *image_on = NULL;
    QPixmap *image_off = NULL;
    QPixmap *image_pushed = NULL;

    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "Selector needs a name");
        return;
    }

    QString order = element.attribute("draworder", "");
    if (order.isNull() || order.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "Selector needs an order");
        return;
    }

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "context")
            {
                context = getFirstText(info).toInt();
            }
            else if (info.tagName() == "area")
            {
                area = parseRect(getFirstText(info));
                normalizeRect(area);
            }
            else if (info.tagName() == "font")
            {
                font = getFirstText(info);
            }
            else if (info.tagName() == "image")
            {
                QString imgname = "";
                QString file = "";

                imgname = info.attribute("function", "");
                if (imgname.isNull() || imgname.isEmpty())
                {
                    VERBOSE(VB_IMPORTANT, LOC_WARN +
                            "Image in a selector needs a function");
                    return;
                }

                file = info.attribute("filename", "");
                if (file.isNull() || file.isEmpty())
                {
                    VERBOSE(VB_IMPORTANT, LOC_WARN +
                            "Image in a selector needs a filename");
                    return;
                }

                if (imgname.toLower() == "on")
                {
                    image_on = ui->LoadScalePixmap(file);
                    if (!image_on)
                    {
                        VERBOSE(VB_IMPORTANT, LOC_WARN +
                                QString("Can't locate 'on' image '%1'")
                                .arg(file));
                    }
                }
                if (imgname.toLower() == "off")
                {
                    image_off = ui->LoadScalePixmap(file);
                    if (!image_off)
                    {
                        VERBOSE(VB_IMPORTANT, LOC_WARN +
                                QString("Can't locate 'off' image '%1'")
                                .arg(file));
                    }
                }
                if (imgname.toLower() == "pushed")
                {
                    image_pushed = ui->LoadScalePixmap(file);

                    if (!image_pushed)
                    {
                        VERBOSE(VB_IMPORTANT, LOC_WARN +
                                QString("Can't locate 'pushed' image '%1'")
                                .arg(file));
                    }
                }
            }
            else
            {
                VERBOSE(VB_IMPORTANT, LOC_WARN +
                        QString("Unknown tag '%1' in selector")
                        .arg(info.tagName()));
                return;
            }
        }
    }

    fontProp *testfont = GetFont(font);
    if (!testfont)
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                QString("Unknown font '%1' in selector '%2'")
                .arg(font).arg(name));
        return;
    }

    if (!image_on)
        image_on = new QPixmap();
    if (!image_off)
        image_off = new QPixmap();
    if (!image_pushed)
        image_pushed = new QPixmap();

    UISelectorType *st = new UISelectorType(name, *image_on, *image_off,
                                                 *image_pushed, area);

    delete image_on;
    delete image_off;
    delete image_pushed;

    st->SetScreen(wmult, hmult);
    st->setPosition(area.topLeft());
    st->setFont(testfont);
    st->SetOrder(order.toInt());
    if (context != -1)
    {
        st->SetContext(context);
    }
    st->SetParent(container);
    st->calculateScreenArea();
    container->AddType(st);
}


void XMLParse::parseBlackHole(LayerSet *container, QDomElement &element)
{
    QRect area;

    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "BlackHole needs a name");
        return;
    }

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "area")
            {
                area = parseRect(getFirstText(info));
                normalizeRect(area);
            }
            else
            {
                VERBOSE(VB_IMPORTANT, LOC_WARN +
                        QString("Unknown tag '%1' in blackhole")
                        .arg(info.tagName()));
                return;
            }
        }
    }


    UIBlackHoleType *bh = new UIBlackHoleType(name);
    bh->SetScreen(wmult, hmult);
    bh->setArea(area);
    bh->SetParent(container);
    bh->calculateScreenArea();
    container->AddType(bh);
}

void XMLParse::parseListBtnArea(LayerSet *container, QDomElement &element)
{
    int context = -1;
    QRect   area = QRect(0,0,0,0);
    QString fontActive;
    QString fontInactive;
    QString align = QString::null;
    bool    showArrow = true;
    bool    showScrollArrows = false;
    int     draworder = 0;
    QColor  grUnselectedBeg(Qt::black);
    QColor  grUnselectedEnd(80,80,80);
    uint    grUnselectedAlpha(100);
    QColor  grSelectedBeg(82,202,56);
    QColor  grSelectedEnd(52,152,56);
    uint    grSelectedAlpha(255);
    int     spacing = 2;
    int     margin = 3;

    QString name = element.attribute("name", "");
    if (name.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "ListBtn area needs a name");
        return;
    }

    QString layerNum = element.attribute("draworder", "");
    if (layerNum.isNull() || layerNum.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "ListBtn area needs a draworder");
        return;
    }

    draworder = layerNum.toInt();
    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "area")
            {
                area = parseRect(getFirstText(info));
                normalizeRect(area);
            }
            else if (info.tagName() == "context")
            {
                context = getFirstText(info).toInt();
            }
            else if (info.tagName() == "fcnfont")
            {
                QString fontName = info.attribute("name", "");
                QString fontFcn  = info.attribute("function", "");

                if (fontFcn.toLower() == "active")
                    fontActive = fontName;
                else if (fontFcn.toLower() == "inactive")
                    fontInactive = fontName;
                else
                {
                    VERBOSE(VB_IMPORTANT, LOC_WARN +
                            QString("Unknown font function '%1' "
                                    "for listbtn area").arg(fontFcn));
                    return;
                }
            }
            else if (info.tagName() == "showarrow")
            {
                if (getFirstText(info).toLower() == "no")
                    showArrow = false;
            }
            else if (info.tagName() == "align")
            {
                align = getFirstText(info);
            }
            else if (info.tagName() == "showscrollarrows")
            {
                if (getFirstText(info).toLower() == "yes")
                    showScrollArrows = true;
            }
            else if (info.tagName() == "gradient")
            {

                if (info.attribute("type","").toLower() == "selected")
                {
                    grSelectedBeg = createColor(info.attribute("start"));
                    grSelectedEnd = createColor(info.attribute("end"));
                    grSelectedAlpha = info.attribute("alpha","255").toUInt();
                }
                else if (info.attribute("type","").toLower() == "unselected")
                {
                    grUnselectedBeg = createColor(info.attribute("start"));
                    grUnselectedEnd = createColor(info.attribute("end"));
                    grUnselectedAlpha = info.attribute("alpha","100").toUInt();
                }
                else
                {
                    VERBOSE(VB_IMPORTANT, LOC_WARN +
                            "Unknown type for gradient in listbtn area");
                    return;
                }

                if (!grSelectedBeg.isValid() || !grSelectedEnd.isValid() ||
                    !grUnselectedBeg.isValid() || !grUnselectedEnd.isValid())
                {
                    VERBOSE(VB_IMPORTANT, LOC_WARN +
                            "Unknown color for gradient in listbtn area");
                    return;
                }

                if (grSelectedAlpha > 255 || grUnselectedAlpha > 255)
                {
                    VERBOSE(VB_IMPORTANT, LOC_WARN +
                            "Incorrect alpha for gradient in listbtn area");
                    return;
                }
            }
            else if (info.tagName() == "spacing")
            {
                spacing = getFirstText(info).toInt();
            }
            else if (info.tagName() == "margin")
            {
                margin = getFirstText(info).toInt();
            }
            else
            {
                VERBOSE(VB_IMPORTANT, LOC_WARN +
                        QString("Unknown tag '%1' in listbtn area")
                        .arg(info.tagName()));
                return;
            }

        }
    }

    int jst = Qt::AlignLeft | Qt::AlignVCenter;

    if (!align.isEmpty())
    {
        if (align.toLower() == "center")
            jst = Qt::AlignCenter | Qt::AlignVCenter;
        else if (align.toLower() == "right")
            jst = Qt::AlignRight  | Qt::AlignVCenter;
        else if (align.toLower() == "left")
            jst = Qt::AlignLeft   | Qt::AlignVCenter;
    }

    fontProp *fpActive = GetFont(fontActive);
    if (!fpActive)
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                QString("Unknown active font '%1' in listbtn area '%2'")
                .arg(fontActive).arg(name));
        return;
    }

    fontProp *fpInactive = GetFont(fontInactive);
    if (!fpInactive)
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                QString("Unknown inactive font '%1' in listbtn area '%2'")
                .arg(fontInactive).arg(name));
        return;
    }


    UIListBtnType *l = new UIListBtnType(name, area, draworder, showArrow,
                                         showScrollArrows);
    l->SetScreen(wmult, hmult);
    l->SetFontActive(fpActive);
    l->SetFontInactive(fpInactive);
    l->SetJustification(jst);
    l->SetItemRegColor(grUnselectedBeg, grUnselectedEnd, grUnselectedAlpha);
    l->SetItemSelColor(grSelectedBeg, grSelectedEnd, grSelectedAlpha);
    l->SetSpacing((int)(spacing*hmult));
    l->SetMargin((int)(margin*wmult));
    l->SetParent(container);
    l->SetContext(context);
    l->calculateScreenArea();

    container->AddType(l);
    container->bumpUpLayers(0);
}

void XMLParse::parseListTreeArea(LayerSet *container, QDomElement &element)
{
    int     context = -1;
    QRect   area = QRect(0,0,0,0);
    QRect   listsize = QRect(0,0,0,0);
    int     leveloffset = 0;
    QString fontActive;
    QString fontInactive;
    bool    showArrow = true;
    bool    showScrollArrows = false;
    int     draworder = 0;
    QColor  grUnselectedBeg(Qt::black);
    QColor  grUnselectedEnd(80,80,80);
    uint    grUnselectedAlpha(100);
    QColor  grSelectedBeg(82,202,56);
    QColor  grSelectedEnd(52,152,56);
    uint    grSelectedAlpha(255);
    int     spacing = 2;
    int     margin = 3;

    QString name = element.attribute("name", "");
    if (name.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "ListTreeArea area needs a name" );
        return;
    }

    QString layerNum = element.attribute("draworder", "");
    if (layerNum.isNull() || layerNum.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "ListTreeArea needs a draworder");
        return;
    }

    draworder = layerNum.toInt();
    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "area")
            {
                area = parseRect(getFirstText(info));
                normalizeRect(area);
            }
            else if (info.tagName() == "context")
            {
                context = getFirstText(info).toInt();
            }
            else if (info.tagName() == "listsize")
            {
                listsize = parseRect(getFirstText(info));
                normalizeRect(listsize);
            }
            else if (info.tagName() == "leveloffset")
            {
                leveloffset = getFirstText(info).toInt();
            }
            else if (info.tagName() == "fcnfont")
            {
                QString fontName = info.attribute("name", "");
                QString fontFcn  = info.attribute("function", "");

                if (fontFcn.toLower() == "active")
                    fontActive = fontName;
                else if (fontFcn.toLower() == "inactive")
                    fontInactive = fontName;
                else
                {
                    VERBOSE(VB_IMPORTANT, LOC_WARN +
                            QString("Unknown font function '%1' "
                                    "for ListTreeArea: ").arg(fontFcn));
                    return;
                }
            }
            else if (info.tagName() == "showarrow")
            {
                if (getFirstText(info).toLower() == "no")
                    showArrow = false;
            }
            else if (info.tagName() == "showscrollarrows")
            {
                if (getFirstText(info).toLower() == "yes")
                    showScrollArrows = true;
            }
            else if (info.tagName() == "gradient")
            {

                if (info.attribute("type","").toLower() == "selected")
                {
                    grSelectedBeg = createColor(info.attribute("start"));
                    grSelectedEnd = createColor(info.attribute("end"));
                    grSelectedAlpha = info.attribute("alpha","255").toUInt();
                }
                else if (info.attribute("type","").toLower() == "unselected")
                {
                    grUnselectedBeg = createColor(info.attribute("start"));
                    grUnselectedEnd = createColor(info.attribute("end"));
                    grUnselectedAlpha = info.attribute("alpha","100").toUInt();
                }
                else
                {
                    VERBOSE(VB_IMPORTANT, LOC_WARN +
                            "Unknown type for gradient in ListTreeArea");
                    return;
                }

                if (!grSelectedBeg.isValid() || !grSelectedEnd.isValid() ||
                    !grUnselectedBeg.isValid() || !grUnselectedEnd.isValid())
                {
                    VERBOSE(VB_IMPORTANT, LOC_WARN +
                            "Unknown color for gradient in ListTreeArea area");
                    return;
                }

                if (grSelectedAlpha > 255 || grUnselectedAlpha > 255)
                {
                    VERBOSE(VB_IMPORTANT, LOC_WARN +
                            "Incorrect alpha for gradient "
                            "in ListTreeArea area");
                    return;
                }
            }
            else if (info.tagName() == "spacing")
            {
                spacing = getFirstText(info).toInt();
            }
            else if (info.tagName() == "margin")
            {
                margin = getFirstText(info).toInt();
            }
            else
            {
                VERBOSE(VB_IMPORTANT, LOC_WARN +
                        QString("Unknown tag '%1' in listtreearea")
                        .arg(info.tagName()));
                return;
            }

        }
    }

    fontProp *fpActive = GetFont(fontActive);
    if (!fpActive)
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                QString("Unknown active font '%1' in textbutton '%2'")
                .arg(fontActive).arg(name));
        return;
    }

    fontProp *fpInactive = GetFont(fontInactive);
    if (!fpInactive)
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                QString("Unknown inactive font '%1' in textbutton '%2'")
                .arg(fontInactive).arg(name));
        return;
    }


    UIListTreeType *l = new UIListTreeType(name, area, listsize, leveloffset,
                                           draworder);

    l->SetScreen(wmult, hmult);
    l->SetFontActive(fpActive);
    l->SetFontInactive(fpInactive);
    l->SetItemRegColor(grUnselectedBeg, grUnselectedEnd, grUnselectedAlpha);
    l->SetItemSelColor(grSelectedBeg, grSelectedEnd, grSelectedAlpha);
    l->SetSpacing((int)(spacing*hmult));
    l->SetMargin((int)(margin*wmult));
    l->SetParent(container);
    l->calculateScreenArea();
    l->SetContext(context);

    container->AddType(l);
    container->bumpUpLayers(0);
}

void XMLParse::parseKey(LayerSet *container, QDomElement &element)
{
    QPixmap *normalImage = NULL, *focusedImage = NULL;
    QPixmap *downImage = NULL, *downFocusedImage = NULL;
    QString normalFontName = "", focusedFontName = "";
    QString downFontName = "", downFocusedFontName = "";
    QString name, order, type;
    QString normalChar = "", shiftChar = "";
    QString altChar = "", shiftaltChar = "";
    QString moveLeft = "", moveRight = "";
    QString moveUp = "", moveDown = "";
    QPoint pos = QPoint(0, 0);

    name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "key needs a name");
        return;
    }

    type = element.attribute("type", "");
    if (type.isNull() || type.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "key needs a type");
        return;
    }

    order = element.attribute("draworder", "");
    if (order.isNull() || order.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "key needs an order");
        return;
    }

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement e = child.toElement();
        if (!e.isNull())
        {
            if (e.tagName() == "position")
            {
                pos = parsePoint(getFirstText(e));
                pos.setX((int)(pos.x() * wmult));
                pos.setY((int)(pos.y() * hmult));

            }
            else if (e.tagName() == "char")
            {
                normalChar = e.attribute("normal", "");
                shiftChar = e.attribute("shift", "");
                altChar = e.attribute("alt", "");
                shiftaltChar = e.attribute("altshift", "");
            }
            else if (e.tagName() == "move")
            {
                moveLeft = e.attribute("left", "");
                moveRight = e.attribute("right", "");
                moveUp = e.attribute("up", "");
                moveDown = e.attribute("down", "");
            }
            else if (e.tagName() == "image")
            {
                QString imgname = "";
                QString imgfunction = "";

                imgfunction = e.attribute("function", "");
                if (imgfunction.isNull() || imgfunction.isEmpty())
                {
                    VERBOSE(VB_IMPORTANT, LOC_WARN +
                            "Image in a key needs a function");
                    return;
                }

                imgname = e.attribute("filename", "");
                if (imgname.isNull() || imgname.isEmpty())
                {
                    VERBOSE(VB_IMPORTANT, LOC_WARN +
                            "Image in a key needs a filename");
                    return;
                }

                if (imgfunction.toLower() == "normal")
                {
                    normalImage = ui->LoadScalePixmap(imgname);
                    if (!normalImage)
                    {
                        VERBOSE(VB_IMPORTANT, LOC_WARN +
                                QString("Can't locate 'normal' image '%1'")
                                .arg(imgname));
                    }
                }
                else if (imgfunction.toLower() == "focused")
                {
                    focusedImage = ui->LoadScalePixmap(imgname);
                    if (!focusedImage)
                    {
                        VERBOSE(VB_IMPORTANT, LOC_WARN +
                                QString("Can't locate 'focused' image '%1'")
                                .arg(imgname));
                    }
                }
                else if (imgfunction.toLower() == "down")
                {
                    downImage = ui->LoadScalePixmap(imgname);

                    if (!downImage)
                    {
                        VERBOSE(VB_IMPORTANT, LOC_WARN +
                                QString("Can't locate 'down' image '%1'")
                                .arg(imgname));
                    }
                }
                else if (imgfunction.toLower() == "downfocused")
                {
                    downFocusedImage = ui->LoadScalePixmap(imgname);

                    if (!downFocusedImage)
                    {
                        VERBOSE(VB_IMPORTANT, LOC_WARN +
                                QString("Can't locate 'downfocused' image '%1'")
                                .arg(imgname));
                    }
                }
                else
                {
                    VERBOSE(VB_IMPORTANT, LOC_WARN +
                            QString("Unknown image function '%1' in key")
                            .arg(imgfunction));
                    return;
                }
            }
            else if (e.tagName() == "fcnfont")
            {
                QString fontName = e.attribute("name", "");
                QString fontFcn  = e.attribute("function", "");

                if (fontFcn.toLower() == "normal")
                    normalFontName = fontName;
                else if (fontFcn.toLower() == "focused")
                    focusedFontName = fontName;
                else if (fontFcn.toLower() == "down")
                    downFontName = fontName;
                else if (fontFcn.toLower() == "downfocused")
                    downFocusedFontName = fontName;
                else
                {
                    VERBOSE(VB_IMPORTANT, LOC_WARN +
                            QString("Unknown font function '%1' in key")
                            .arg(fontFcn));
                    return;
                }
            }
            else
            {
                VERBOSE(VB_IMPORTANT, LOC_WARN +
                        QString("Unknown tag '%1' in key").arg(e.tagName()));
                return;
            }
        }
    }

    fontProp *normalFont = GetFont(normalFontName);
    fontProp *focusedFont = GetFont(focusedFontName);
    fontProp *downFont = GetFont(downFontName);
    fontProp *downFocusedFont = GetFont(downFocusedFontName);

    UIKeyType *key = new UIKeyType(name);
    key->SetScreen(wmult, hmult);
    key->SetParent(container);
    key->SetOrder(order.toInt());
    key->SetType(type);
    key->SetChars(normalChar, shiftChar, altChar, shiftaltChar);
    key->SetMoves(moveLeft, moveRight, moveUp, moveDown);
    key->SetPosition(pos);
    key->SetImages(normalImage, focusedImage, downImage, downFocusedImage);
    key->SetFonts(normalFont, focusedFont, downFont, downFocusedFont);

    container->AddType(key);
}

void XMLParse::parseKeyboard(LayerSet *container, QDomElement &element)
{
    QString normalFontName = "", focusedFontName = "";
    QString downFontName = "", downFocusedFontName = "";
    int context = -1;
    QRect area;
    QPixmap *normalImage = NULL, *focusedImage = NULL;
    QPixmap *downImage = NULL, *downFocusedImage = NULL;

    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "keyboard needs a name");
        return;
    }

    QString order = element.attribute("draworder", "");
    if (order.isNull() || order.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "keyboard needs an order");
        return;
    }

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement e = child.toElement();
        if (!e.isNull())
        {
            if (e.tagName() == "key")
            {
                parseKey(container, e);
            }
            else if (e.tagName() == "area")
            {
                area = parseRect(getFirstText(e));
                normalizeRect(area);
            }
            else if (e.tagName() == "context")
            {
                context = getFirstText(e).toInt();
            }

            else if (e.tagName() == "image")
            {
                QString imgname = "";
                QString imgfunction = "";

                imgfunction = e.attribute("function", "");
                if (imgfunction.isNull() || imgfunction.isEmpty())
                {
                    VERBOSE(VB_IMPORTANT, LOC_WARN +
                            "Image in a keyboard needs a function");
                    return;
                }

                imgname = e.attribute("filename", "");
                if (imgname.isNull() || imgname.isEmpty())
                {
                    VERBOSE(VB_IMPORTANT, LOC_WARN +
                            "Image in a keyboard needs a filename");
                    return;
                }

                if (imgfunction.toLower() == "normal")
                {
                    normalImage = ui->LoadScalePixmap(imgname);
                    if (!normalImage)
                    {
                        VERBOSE(VB_IMPORTANT, LOC_WARN +
                                QString("Can't locate 'normal' image '%1'")
                                .arg(imgname));
                    }
                }
                else if (imgfunction.toLower() == "focused")
                {
                    focusedImage = ui->LoadScalePixmap(imgname);
                    if (!focusedImage)
                    {
                        VERBOSE(VB_IMPORTANT, LOC_WARN +
                                QString("Can't locate 'focused' image '%1'")
                                .arg(imgname));
                    }
                }
                else if (imgfunction.toLower() == "down")
                {
                    downImage = ui->LoadScalePixmap(imgname);

                    if (!downImage)
                    {
                        VERBOSE(VB_IMPORTANT, LOC_WARN +
                                QString("Can't locate 'down' image '%1'")
                                .arg(imgname));
                    }
                }
                else if (imgfunction.toLower() == "downfocused")
                {
                    downFocusedImage = ui->LoadScalePixmap(imgname);

                    if (!downFocusedImage)
                    {
                        VERBOSE(VB_IMPORTANT, LOC_WARN +
                                QString("Can't locate 'downfocused' image '%1'")
                                .arg(imgname));
                    }
                }
                else
                {
                    VERBOSE(VB_IMPORTANT, LOC_WARN +
                            QString("Unknown image function '%1' in keyboard")
                            .arg(imgfunction));
                    return;
                }
            }
            else if (e.tagName() == "fcnfont")
            {
                QString fontName = e.attribute("name", "");
                QString fontFcn  = e.attribute("function", "");

                if (fontFcn.toLower() == "normal")
                    normalFontName = fontName;
                else if (fontFcn.toLower() == "focused")
                    focusedFontName = fontName;
                else if (fontFcn.toLower() == "down")
                    downFontName = fontName;
                else if (fontFcn.toLower() == "downfocused")
                    downFocusedFontName = fontName;
                else
                {
                    VERBOSE(VB_IMPORTANT, LOC_WARN +
                            QString("Unknown font function '%1' in keyboard")
                            .arg(fontFcn));
                    return;
                }
            }
            else
            {
                VERBOSE(VB_IMPORTANT, LOC_WARN +
                        QString("Unknown tag '%1' in keyboard")
                        .arg(e.tagName()));
                return;
            }
        }
    }

    if (normalFontName.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "Keyboard need a normal font" );
        return;
    }

    if (focusedFontName.isEmpty())
        focusedFontName = normalFontName;

    if (downFontName.isEmpty())
        downFontName = normalFontName;

    if (downFocusedFontName.isEmpty())
        downFocusedFontName = normalFontName;

    fontProp *normalFont = GetFont(normalFontName);
    if (!normalFont)
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                QString("Unknown normal font '%1' in in Keyboard '%2'")
                .arg(normalFontName).arg(name));
        return;
    }

    fontProp *focusedFont = GetFont(focusedFontName);
    if (!focusedFont)
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                QString("Unknown focused font '%1' in in Keyboard '%2'")
                .arg(focusedFontName).arg(name));
        return;
    }

    fontProp *downFont = GetFont(downFontName);
    if (!downFont)
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                QString("Unknown down font '%1' in in Keyboard '%2'")
                .arg(downFontName).arg(name));
        return;
    }

    fontProp *downFocusedFont = GetFont(downFocusedFontName);
    if (!downFocusedFont)
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                QString("Unknown down focus font '%1' in in Keyboard '%2'")
                .arg(downFocusedFontName).arg(name));
        return;
    }

    UIKeyboardType *kbd = new UIKeyboardType(name, order.toInt());
    kbd->SetScreen(wmult, hmult);
    kbd->SetParent(container);
    kbd->SetContext(context);
    kbd->SetArea(area);
    kbd->calculateScreenArea();

    container->AddType(kbd);

    vector<UIType *>::iterator i = container->getAllTypes()->begin();
    for (; i != container->getAllTypes()->end(); i++)
    {
        UIType *type = (*i);
        if (UIKeyType *keyt = dynamic_cast<UIKeyType*>(type))
        {
            kbd->AddKey(keyt);
            keyt->SetDefaultImages(normalImage, focusedImage, downImage, downFocusedImage);
            keyt->SetDefaultFonts(normalFont, focusedFont, downFont, downFocusedFont);
            keyt->calculateScreenArea();
        }
    }
}

// vim:set sw=4 expandtab:
