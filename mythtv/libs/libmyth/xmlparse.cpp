#include <iostream>
using namespace std;

#include "xmlparse.h"


XMLParse::XMLParse(void)
{
    allTypes = new vector<LayerSet *>;
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
}

bool XMLParse::LoadTheme(QDomElement &ele, QString winName, QString specialfile)
{
    QString themepath = gContext->FindThemeDir("") + gContext->GetSetting("Theme");
    QString themefile = themepath + "/" + specialfile + "ui.xml";

    QDomDocument doc;
    QFile f(themefile);

    if (!f.open(IO_ReadOnly))
    {
        themepath = gContext->FindThemeDir("") + "default/";
        themefile = themepath + "/" + specialfile + "ui.xml";
        f.setName(themefile);
        if (!f.open(IO_ReadOnly))
        {
            cerr << "Can't open: " << themefile << endl;
            return false;
        }
    }

    QString errorMsg;
    int errorLine = 0;
    int errorColumn = 0;

    if (!doc.setContent(&f, false, &errorMsg, &errorLine, &errorColumn))
    {
        cerr << "Error parsing: " << themefile << endl;
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
            if (e.tagName() == "window")
            {
                QString name = e.attribute("name", "");
                if (name.isNull() || name.isEmpty())
                {
                    cerr << "Window needs a name\n";
                    exit(0);
                }

                if (name == winName)
                {
		    ele = e;
                    return true;
                }
            }
            else
            {
                cerr << "Unknown element: " << e.tagName() << endl;
                exit(0);
            }
        }
        n = n.nextSibling();
    }

    return true;
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
    int size = -1;
    QPoint shadowOffset = QPoint(0, 0);
    QString color = "#ffffff";
    QString dropcolor = "#000000";

    name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        cerr << "Font needs a name\n";
        exit(0);
    }

    face = element.attribute("face", "");
    if (face.isNull() || face.isEmpty())
    {
        cerr << "Font needs a face\n";
        exit(0);
    }

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "size")
            {
                size = getFirstText(info).toInt();
            }
            else if (info.tagName() == "color")
            {
                color = getFirstText(info);
            }
            else if (info.tagName() == "dropcolor")
            {
                dropcolor = getFirstText(info);
            }
            else if (info.tagName() == "shadow")
            {
                shadowOffset = parsePoint(getFirstText(info));
                shadowOffset.setX((int)(shadowOffset.x() * wmult));
                shadowOffset.setY((int)(shadowOffset.y() * hmult));
            }
            else if (info.tagName() == "bold")
            {
                bold = getFirstText(info);
            }
            else
            {
                cerr << "Unknown tag " << info.tagName() << " in font\n";
                exit(0);
            }
        }
    }

    fontProp *testFont = GetFont(name);
    if (testFont)
    {
        cerr << "Error: already have a font called: " << name << endl;
        exit(0);
    }

    if (size < 0)
    {
        cerr << "Error: font size must be > 0\n";
        exit(0);
    }

    size = (int)(size * hmult);
    QFont temp(face, size);
    if (bold.lower() == "yes")
        temp.setBold(true);

    QColor foreColor(color);
    QColor dropColor(dropcolor);

    fontProp newFont;
    newFont.face = temp;
    newFont.color = foreColor;
    newFont.dropColor = dropColor;
    newFont.shadowOffset = shadowOffset;

    fontMap[name] = newFont;
}

void XMLParse::parseImage(LayerSet *container, QDomElement &element)
{
    int context = -1;
    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        cerr << "Image needs a name\n";
        exit(0);
    }

    QString order = element.attribute("draworder", "");
    if (name.isNull() || name.isEmpty())
    {
        cerr << "Image needs an order\n";
        exit(0);
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
            }
            else
            {
                cerr << "Unknown: " << info.tagName() << " in image\n";
                exit(0);
            }
        }
    }

    UIImageType *image = new UIImageType(name, filename, order.toInt(), pos);
    image->SetScreen(wmult, hmult);
    if (scale.x() != -1 || scale.y() != -1)
        image->SetSize(scale.x(), scale.y());
    image->SetSkip(skipin.x(), skipin.y());
    QString flex = element.attribute("fleximage", "");
    if (!flex.isNull() && !flex.isEmpty())
    {
        if (flex.lower() == "yes")
            image->SetFlex(true);
        else
            image->SetFlex(false);
    }

    image->LoadImage();
    if (context != -1)
    {
        image->SetContext(context);
    }
    container->AddType(image);
}

fontProp *XMLParse::GetFont(const QString &text)
{
    fontProp *ret;
    if (fontMap.contains(text))
        ret = &fontMap[text];
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
}

QPoint XMLParse::parsePoint(QString text)
{
    int x, y;
    QPoint retval(0, 0);
    if (sscanf(text.data(), "%d,%d", &x, &y) == 2)
        retval = QPoint(x, y);
    return retval;
}

QRect XMLParse::parseRect(QString text)
{
    int x, y, w, h;
    QRect retval(0, 0, 0, 0);
    if (sscanf(text.data(), "%d,%d,%d,%d", &x, &y, &w, &h) == 4)
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
        cerr << "Container needs a name\n";
        exit(0);
    }

    LayerSet *container = GetSet(name);
    if (container)
    {
        cerr << "Container: " << name << " already exists\n";
        exit(0);
    }
    newname = name;

    container = new LayerSet(name);

    layerMap[name] = container;

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "debug")
            {
                debug = getFirstText(info);
                if (debug.lower() == "yes")
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
            else if (info.tagName() == "listarea")
            {
                parseListArea(container, info);
            }
            else if (info.tagName() == "textarea")
            {
                parseTextArea(container, info);
            }
            else if (info.tagName() == "statusbar")
            {
                parseStatusBar(container, info);
            }
            else if (info.tagName() == "area")
            {
                area = parseRect(getFirstText(info));
                normalizeRect(area);
            }
            else
            {
                cerr << "Unknown container child: " << info.tagName() << endl;
                exit(0);
            }
        }
    }
    if (context != -1)
        container->SetContext(context);

    container->SetAreaRect(area);
    allTypes->push_back(container);
}

void XMLParse::parseTextArea(LayerSet *container, QDomElement &element)
{
    int context = -1;
    QRect area = QRect(0, 0, 0, 0);
    QPoint shadowOffset = QPoint(0, 0);
    QString font = "";
    QString value = "";
    QString statictext = "";
    QString multiline = "";
    int draworder = 0;

    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        cerr << "Text area needs a name\n";
        exit(0);
    }

    QString layerNum = element.attribute("draworder", "");
    if (layerNum.isNull() && layerNum.isEmpty())
    {
        cerr << "Text area needs a draworder\n";
        exit(0);
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
                value = getFirstText(info);
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
                cerr << "Unknown tag in listarea: " << info.tagName() << endl;
                exit(0);
            }
        }
    }

    fontProp *testfont = GetFont(font);
    if (!testfont)
    {
        cerr << "Unknown font: " << font << " in textarea: " << name << endl;
        exit(0);
    }

    UITextType *text = new UITextType(name, testfont, value, draworder, area);
    text->SetScreen(wmult, hmult);
    if (context != -1)
    {
        text->SetContext(context);
    }
    if (multiline.lower() == "yes")
        text->SetJustification(Qt::WordBreak);
    if (!value.isNull() && !value.isEmpty())
        text->SetText(value);
    container->AddType(text);

    QString align = element.attribute("align", "");
    if (!align.isNull() && !align.isEmpty())
    {
        int jst = text->GetJustification();
        if (align.lower() == "center")
        {
            text->SetJustification(jst | Qt::AlignCenter);
        }
        else if (align.lower() == "right")
        {
            text->SetJustification(jst | Qt::AlignRight);
        }
    }
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
    int colCnt = -1;

    QPoint uparrow_loc;
    QPoint dnarrow_loc;
    QPoint select_loc;
    QPixmap uparrow_img;
    QPixmap dnarrow_img;
    QPixmap select_img;

    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        cerr << "List area needs a name\n";
        exit(0);
    }

    QString layerNum = element.attribute("draworder", "");
    if (layerNum.isNull() && layerNum.isEmpty())
    {
        cerr << "List area needs a draworder\n";
        exit(0);
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
            else if (info.tagName() == "forcecolor")
            {
                force_color = getFirstText(info);
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
            else if (info.tagName() == "image")
            {
                QString imgname = "";
                QString imgpoint = "";
                QString imgfile = "";

                imgname = info.attribute("function", "");
                if (imgname.isNull() || imgname.isEmpty())
                {
                    cerr << "Image needs a function\n";
                    exit(0);
                }

                imgfile = info.attribute("filename", "");
                if (imgfile.isNull() || imgfile.isEmpty())
                {
                    cerr << "Image needs a filename\n";
                    exit(0);
                }

                imgpoint = info.attribute("location", "");
                if (imgpoint.isNull() && imgpoint.isEmpty())
                {
                    cerr << "Image needs a location\n";
                    exit(0);
                }

                if (imgname.lower() == "selectionbar")
                {
                    resizeImage(&select_img, imgfile);
                    select_loc = parsePoint(imgpoint);
                    select_loc.setX((int)(select_loc.x() * wmult));
                    select_loc.setY((int)(select_loc.y() * hmult));
                }
                if (imgname.lower() == "uparrow")
                {
                    resizeImage(&uparrow_img, imgfile);
                    uparrow_loc = parsePoint(imgpoint);
                    uparrow_loc.setX((int)(uparrow_loc.x() * wmult));
                    uparrow_loc.setY((int)(uparrow_loc.y() * hmult));
                }
                if (imgname.lower() == "downarrow")
                {
                    resizeImage(&dnarrow_img, imgfile);
                    dnarrow_loc = parsePoint(imgpoint);
                    dnarrow_loc.setX((int)(dnarrow_loc.x() * wmult));
                    dnarrow_loc.setY((int)(dnarrow_loc.y() * hmult));
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
                    cerr << "Column needs a number\n";
                    exit(0);
                }

                colwidth = info.attribute("width", "");
                if (colwidth.isNull() && colwidth.isEmpty())
                {
                    cerr << "Column needs a width\n";
                    exit(0);
                }

                colcontext = info.attribute("context", "");
                if (!colcontext.isNull() && !colcontext.isEmpty())
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
                cerr << "Unknown tag in listarea: " << info.tagName() << endl;
                exit(0);
            }
        }
    }

    fontProp *testactfont = GetFont(act_font);
    if (!testactfont)
    {
        cerr << "Unknown font: " << act_font << " in listarea: " << name << endl;
        exit(0);
    }
    fontProp *testinfont = GetFont(in_font);
    if (!testinfont)
    {
        cerr << "Unknown font: " << in_font << " in listarea: " << name << endl;
        exit(0);
    }

    UIFontPairType *text = new UIFontPairType(testactfont, testinfont);

    UIListType *list = new UIListType(name, area, draworder, text);
    list->SetCount(item_cnt);
    list->SetScreen(wmult, hmult);
    list->SetColumnPad(padding);
    if (force_color.length() > 0)
        list->SetForceColor(force_color);
    list->SetImageSelection(select_img, select_loc);
    list->SetImageUpArrow(uparrow_img, uparrow_loc);
    list->SetImageDownArrow(dnarrow_img, dnarrow_loc);
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

    container->AddType(list);

}

LayerSet *XMLParse::GetSet(const QString &text)
{
    LayerSet *ret = NULL;
    if (layerMap.contains(text))
        ret = layerMap[text];

    return ret;
}

void XMLParse::resizeImage(QPixmap *dst, QString file)
{
    QString baseDir = gContext->GetInstallPrefix();
    QString themeDir = gContext->FindThemeDir("");
    themeDir = themeDir + gContext->GetSetting("Theme") + "/";
    baseDir = baseDir + "/share/mythtv/themes/default/";

    QFile checkFile(themeDir + file);

    if (checkFile.exists())
         file = themeDir + file;
    else
         file = baseDir + file;

    if (hmult == 1 && wmult == 1)
    {
         dst->load(file);
    }
    else
    {
        QImage *sourceImg = new QImage();
        if (sourceImg->load(file))
        {
            QImage scalerImg;
            scalerImg = sourceImg->smoothScale((int)(sourceImg->width() * wmult),
                                               (int)(sourceImg->height() * hmult));
            dst->convertFromImage(scalerImg);
        }
        delete sourceImg;
    }
}

void XMLParse::parseStatusBar(LayerSet *container, QDomElement &element)
{
    int imgFillSpace = 0;
    QPixmap imgFiller;
    QPixmap imgContainer;
    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        cerr << "StatusBar needs a name\n";
        exit(0);
    }

    QString order = element.attribute("draworder", "");
    if (name.isNull() || name.isEmpty())
    {
        cerr << "StatusBar needs an order\n";
        exit(0);
    }

    QPoint pos = QPoint(0, 0);

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "container")
            {
                QString confile = getFirstText(info);
                QString flex = info.attribute("fleximage", "");
                if (!flex.isNull() && !flex.isEmpty())
                {
                    if (flex.lower() == "yes")
                    {
                        int trans = gContext->GetNumSetting("PlayBoxTransparency", 1);
                        if (trans == 1)
                            confile = "trans-" + confile;
                        else
                            confile = "solid-" + confile;

                        resizeImage(&imgContainer, confile);
                    }
                    else
                        resizeImage(&imgContainer, confile);
                }
                else
                {
                    resizeImage(&imgContainer, confile);
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
                if (!flex.isNull() && !flex.isEmpty())
                {
                    if (flex.lower() == "yes")
                    {
                        int trans = gContext->GetNumSetting("PlayBoxTransparency", 1);
                        if (trans == 1)
                            fillfile = "trans-" + fillfile;
                        else
                            fillfile = "solid-" + fillfile;
                        resizeImage(&imgFiller, fillfile);
                     }
                     else
                        resizeImage(&imgFiller, fillfile);
                }
                else
                {
                    resizeImage(&imgFiller, fillfile);
                }
            }
            else
            {
                cerr << "Unknown: " << info.tagName() << " in image\n";
                exit(0);
            }
        }
    }

    UIStatusBarType *sb = new UIStatusBarType(name, pos, order.toInt());
    sb->SetFiller(imgFillSpace);
    sb->SetContainerImage(imgContainer);
    sb->SetFillerImage(imgFiller);
    container->AddType(sb);
}
