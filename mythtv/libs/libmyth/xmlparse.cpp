#include <deque>
using namespace std;

#include <cmath>
#include <cstdlib>

#include <QApplication>
#include <QCoreApplication>
#include <QPixmap>
#include <QList>
#include <QFile>

#include "xmlparse.h"
#include "mythdate.h"
#include "mythcorecontext.h"
#include "mythfontproperties.h"
#include "mythuihelper.h"
#include "x11colors.h"
#include "mythlogging.h"
#include "mythcorecontext.h"

#ifdef _WIN32
#undef LoadImage
#endif

#define LOC QString("XMLParse: ")

XMLParse::XMLParse(void) : wmult(0.0), hmult(0.0)
{
    allTypes = new vector<LayerSet *>;
    ui = GetMythUI();
}

XMLParse::~XMLParse()
{
    vector<LayerSet *>::iterator i = allTypes->begin();
    for (; i != allTypes->end(); ++i)
    {
        LayerSet *type = (*i);
        if (type)
            delete type;
    }
    delete allTypes;
}

bool XMLParse::LoadTheme(QDomElement &ele, QString winName, QString specialfile)
{
    fontSizeType = gCoreContext->GetSetting("ThemeFontSizeType", "default");

    QStringList searchpath = ui->GetThemeSearchPath();
    for (QStringList::const_iterator ii = searchpath.begin();
        ii != searchpath.end(); ++ii)
    {
        QString themefile = *ii + specialfile + "ui.xml";
        if (doLoadTheme(ele, winName, themefile))
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + QString("LoadTheme using '%1'")
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
#if 0
        LOG(VB_GENERAL, LOG_ERR, "XMLParse::LoadTheme(): Can't open: " +
                                      themeFile);
#endif
        return false;
    }

    QString errorMsg;
    int errorLine = 0;
    int errorColumn = 0;

    if (!doc.setContent(&f, false, &errorMsg, &errorLine, &errorColumn))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
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
                    LOG(VB_GENERAL, LOG_WARNING, LOC + "Window needs a name");
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
                LOG(VB_GENERAL, LOG_WARNING, LOC +
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
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Font needs a name");
        return;
    }

    QString base =  element.attribute("base", "");
    if (base.size())
    {
        baseFont = GetFont(base);
        if (!baseFont)
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC +
                QString("Specified base font '%1' does not exist for font '%2'")
                    .arg(base).arg(face));
            return;
        }
    }

    face = element.attribute("face", "");
    if (face.isNull() || face.isEmpty())
    {
        if (!baseFont)
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC + "Font needs a face");
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
                LOG(VB_GENERAL, LOG_WARNING, LOC +
                    QString("Unknown tag '%1' in font") .arg(info.tagName()));
                return;
            }
        }
    }

    fontProp *testFont = GetFont(name, false);
    if (testFont)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
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
        LOG(VB_GENERAL, LOG_ERR, LOC + "Font size must be > 0");
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
    QPoint retval(0,0);
    QStringList tmp = text.split(',', QString::SkipEmptyParts);
    bool x_ok = false, y_ok = false;
    if (tmp.size() >= 2)
    {
        retval = QPoint(tmp[0].toInt(&x_ok), tmp[1].toInt(&y_ok));
        if (!x_ok || !y_ok)
            retval = QPoint(0,0);
    }
    return retval;
}

QRect XMLParse::parseRect(QString text)
{
    bool x_ok = false, y_ok = false;
    bool w_ok = false, h_ok = false;
    QRect retval(0, 0, 0, 0);
    QStringList tmp = text.split(',', QString::SkipEmptyParts);
    if (tmp.size() >= 4)
    {
        retval = QRect(tmp[0].toInt(&x_ok), tmp[1].toInt(&y_ok),
                       tmp[2].toInt(&w_ok), tmp[3].toInt(&h_ok));
        if (!x_ok || !y_ok || !w_ok || !h_ok)
            retval = QRect(0,0,0,0);
    }
    return retval;
}


void XMLParse::parseContainer(QDomElement &element, QString &newname, int &context, QRect &area)
{
    context = -1;
    QString debug = "";
    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Container needs a name");
        return;
    }

    LayerSet *container = GetSet(name);
    if (container)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
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
            else
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    QString("Container '%1' contains unknown child: '%2'")
                        .arg(name).arg(info.tagName()));
                ok = false;
            }
        }
    }
    if (!ok)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Could not parse container '%1'. Ignoring.") .arg(name));
        return;
    }

    if (context != -1)
        container->SetContext(context);

//    container->SetAreaRect(area);
    allTypes->push_back(container);
}

LayerSet *XMLParse::GetSet(const QString &text)
{
    LayerSet *ret = NULL;
    if (layerMap.contains(text))
        ret = layerMap[text];

    return ret;
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
        LOG(VB_GENERAL, LOG_WARNING, LOC + "key needs a name");
        return;
    }

    type = element.attribute("type", "");
    if (type.isNull() || type.isEmpty())
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "key needs a type");
        return;
    }

    order = element.attribute("draworder", "");
    if (order.isNull() || order.isEmpty())
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "key needs an order");
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
                    LOG(VB_GENERAL, LOG_WARNING, LOC +
                        "Image in a key needs a function");
                    return;
                }

                imgname = e.attribute("filename", "");
                if (imgname.isNull() || imgname.isEmpty())
                {
                    LOG(VB_GENERAL, LOG_WARNING, LOC +
                        "Image in a key needs a filename");
                    return;
                }

                if (imgfunction.toLower() == "normal")
                {
                    normalImage = ui->LoadScalePixmap(imgname);
                    if (!normalImage)
                    {
                        LOG(VB_GENERAL, LOG_WARNING, LOC +
                            QString("Can't locate 'normal' image '%1'")
                                .arg(imgname));
                    }
                }
                else if (imgfunction.toLower() == "focused")
                {
                    focusedImage = ui->LoadScalePixmap(imgname);
                    if (!focusedImage)
                    {
                        LOG(VB_GENERAL, LOG_WARNING, LOC +
                            QString("Can't locate 'focused' image '%1'")
                                .arg(imgname));
                    }
                }
                else if (imgfunction.toLower() == "down")
                {
                    downImage = ui->LoadScalePixmap(imgname);

                    if (!downImage)
                    {
                        LOG(VB_GENERAL, LOG_WARNING, LOC +
                            QString("Can't locate 'down' image '%1'")
                                .arg(imgname));
                    }
                }
                else if (imgfunction.toLower() == "downfocused")
                {
                    downFocusedImage = ui->LoadScalePixmap(imgname);

                    if (!downFocusedImage)
                    {
                        LOG(VB_GENERAL, LOG_WARNING, LOC +
                            QString("Can't locate 'downfocused' image '%1'")
                                .arg(imgname));
                    }
                }
                else
                {
                    LOG(VB_GENERAL, LOG_WARNING, LOC +
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
                    LOG(VB_GENERAL, LOG_WARNING, LOC +
                        QString("Unknown font function '%1' in key")
                            .arg(fontFcn));
                    return;
                }
            }
            else
            {
                LOG(VB_GENERAL, LOG_WARNING, LOC +
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
        LOG(VB_GENERAL, LOG_WARNING, LOC + "keyboard needs a name");
        return;
    }

    QString order = element.attribute("draworder", "");
    if (order.isNull() || order.isEmpty())
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "keyboard needs an order");
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
                    LOG(VB_GENERAL, LOG_WARNING, LOC +
                        "Image in a keyboard needs a function");
                    return;
                }

                imgname = e.attribute("filename", "");
                if (imgname.isNull() || imgname.isEmpty())
                {
                    LOG(VB_GENERAL, LOG_WARNING, LOC +
                        "Image in a keyboard needs a filename");
                    return;
                }

                if (imgfunction.toLower() == "normal")
                {
                    normalImage = ui->LoadScalePixmap(imgname);
                    if (!normalImage)
                    {
                        LOG(VB_GENERAL, LOG_WARNING, LOC +
                            QString("Can't locate 'normal' image '%1'")
                                .arg(imgname));
                    }
                }
                else if (imgfunction.toLower() == "focused")
                {
                    focusedImage = ui->LoadScalePixmap(imgname);
                    if (!focusedImage)
                    {
                        LOG(VB_GENERAL, LOG_WARNING, LOC +
                            QString("Can't locate 'focused' image '%1'")
                                .arg(imgname));
                    }
                }
                else if (imgfunction.toLower() == "down")
                {
                    downImage = ui->LoadScalePixmap(imgname);

                    if (!downImage)
                    {
                        LOG(VB_GENERAL, LOG_WARNING, LOC +
                            QString("Can't locate 'down' image '%1'")
                                .arg(imgname));
                    }
                }
                else if (imgfunction.toLower() == "downfocused")
                {
                    downFocusedImage = ui->LoadScalePixmap(imgname);

                    if (!downFocusedImage)
                    {
                        LOG(VB_GENERAL, LOG_WARNING, LOC +
                            QString("Can't locate 'downfocused' image '%1'")
                                .arg(imgname));
                    }
                }
                else
                {
                    LOG(VB_GENERAL, LOG_WARNING, LOC +
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
                    LOG(VB_GENERAL, LOG_WARNING, LOC +
                        QString("Unknown font function '%1' in keyboard")
                            .arg(fontFcn));
                    return;
                }
            }
            else
            {
                LOG(VB_GENERAL, LOG_WARNING, LOC +
                    QString("Unknown tag '%1' in keyboard") .arg(e.tagName()));
                return;
            }
        }
    }

    if (normalFontName.isEmpty())
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Keyboard need a normal font" );
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
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("Unknown normal font '%1' in in Keyboard '%2'")
                .arg(normalFontName).arg(name));
        return;
    }

    fontProp *focusedFont = GetFont(focusedFontName);
    if (!focusedFont)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("Unknown focused font '%1' in in Keyboard '%2'")
                .arg(focusedFontName).arg(name));
        return;
    }

    fontProp *downFont = GetFont(downFontName);
    if (!downFont)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("Unknown down font '%1' in in Keyboard '%2'")
                .arg(downFontName).arg(name));
        return;
    }

    fontProp *downFocusedFont = GetFont(downFocusedFontName);
    if (!downFocusedFont)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
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
    for (; i != container->getAllTypes()->end(); ++i)
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
