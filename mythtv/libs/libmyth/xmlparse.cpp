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
    delete allTypes;
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
            cerr << "XMLParse::LoadTheme(): Can't open: " << themefile << endl;
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
    if (order.isNull() || order.isEmpty())
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
                skipin.setX((int)(skipin.x() * wmult));
                skipin.setY((int)(skipin.y() * hmult));
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

void XMLParse::parseRepeatedImage(LayerSet *container, QDomElement &element)
{
    int context = -1;
    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        cerr << "Repeated Image needs a name\n";
        exit(0);
    }

    QString order = element.attribute("draworder", "");
    if (order.isNull() || order.isEmpty())
    {
        cerr << "Repeated Image needs an order\n";
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
                skipin.setX((int)(skipin.x() * wmult));
                skipin.setY((int)(skipin.y() * hmult));
            }
            else
            {
                cerr << "Unknown: " << info.tagName() << " in repeated image\n";
                exit(0);
            }
        }
    }

    UIRepeatedImageType *image = new UIRepeatedImageType(name, filename, order.toInt(), pos);
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
    image->SetParent(container);
    image->calculateScreenArea();
    container->AddType(image);
}

void XMLParse::parseGuideGrid(LayerSet *container, QDomElement &element)
{
    int context = -1;
    QString align = "";
    QString font = "";
    QString color = "";
    QString seltype = "";
    QString selcolor = "";
    QString reccolor = "";
    QRect area;
    QPoint textoff = QPoint(0, 0);
    bool cutdown = true;
    bool multiline = false;
    QMap<QString, QString> catColors;
    QMap<int, QString> recImgs;
    QMap<int, QString> arrows;

    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        cerr << "Guide needs a name\n";
        exit(0);
    }

    QString order = element.attribute("draworder", "");
    if (order.isNull() || order.isEmpty())
    {
        cerr << "Guide needs an order\n";
        exit(0);
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
            else if (info.tagName() == "font")
            {
                font = getFirstText(info);
            }
            else if (info.tagName() == "solidcolor")
            {
                color = getFirstText(info);
            }
            else if (info.tagName() == "area")
            {
                area = parseRect(getFirstText(info));
                normalizeRect(area);
            }
            else if (info.tagName() == "align")
            {
                align = getFirstText(info);
            }
            else if (info.tagName() == "cutdown")
            {
                if (getFirstText(info).lower() == "no")
                   cutdown = false;
            }
            else if (info.tagName() == "textoffset")
            {
                textoff = parsePoint(getFirstText(info));
                textoff.setX((int)(textoff.x() * wmult));
                textoff.setY((int)(textoff.y() * hmult));
            }
            else if (info.tagName() == "recordingcolor")
            {
                reccolor = getFirstText(info);
            }
            else if (info.tagName() == "multiline")
            {
                if (getFirstText(info).lower() == "yes")
                   multiline = true;
            }
            else if (info.tagName() == "selector")
            {
                QString typ = "";
                QString col = "";
                typ = info.attribute("type");
                col = info.attribute("color");

                selcolor = col;
                seltype = typ;
            }
            else if (info.tagName() == "recordstatus")
            {
                QString typ = "";
                QString img = "";
                int inttype = 0;
                typ = info.attribute("type");
                img = info.attribute("image");

                if (typ == "SingleRecord")
                    inttype = 1;
                else if (typ == "TimeslotRecord")
                    inttype = 2;
                else if (typ == "ChannelRecord")
                    inttype = 3;
                else if (typ == "AllRecord") 
                    inttype = 4;

                recImgs[inttype] = img;
            }
            else if (info.tagName() == "arrow")
            {
                QString dir = "";
                QString imag = "";
                dir = info.attribute("direction");
                imag = info.attribute("image");

                if (dir == "left")
                    arrows[0] = imag;
                else 
                    arrows[1] = imag;
            }
            else if (info.tagName() == "catcolor")
            {
                QString cat = "";
                QString col = "";
                cat = info.attribute("category");
                col = info.attribute("color");

                catColors[cat] = col;
            }
            else
            {
                cerr << "Unknown: " << info.tagName() << " in bar\n";
                exit(0);
            }
        }
    }
    fontProp *testfont = GetFont(font);
    if (!testfont)
    {
        cerr << "Unknown font: " << font << " in guidegrid: " << name << endl;
        exit(0);
    }

    UIGuideType *guide = new UIGuideType(name, order.toInt());
    guide->SetScreen(wmult, hmult);
    guide->SetFont(testfont);
    guide->SetSolidColor(color);
    guide->SetCutDown(cutdown);
    guide->SetArea(area);
    guide->SetCategoryColors(catColors);
    guide->SetTextOffset(textoff);
    guide->SetRecordingColor(reccolor);
    guide->SetSelectorColor(selcolor);
    for (int i = 1; i < 5; i++)
        guide->LoadImage(i, recImgs[i]);
    if (seltype.lower() == "box")
        guide->SetSelectorType(1);
    else
        guide->SetSelectorType(2); // solid

    guide->SetArrow(0, arrows[0]);
    guide->SetArrow(1, arrows[1]);

    int jst = Qt::AlignLeft | Qt::AlignTop;
    if (multiline == true)
        jst = Qt::WordBreak;

    if (!align.isNull() && !align.isEmpty())
    {
        if (align.lower() == "center")
            guide->SetJustification(Qt::AlignCenter | jst);
        else if (align.lower() == "right")
            guide->SetJustification(Qt::AlignRight | jst);
        else if (align.lower() == "allcenter")
            guide->SetJustification(Qt::AlignHCenter | Qt::AlignVCenter | jst);
        else if (align.lower() == "vcenter")
            guide->SetJustification(Qt::AlignVCenter | jst);
    }
    else 
        guide->SetJustification(jst);

    align = "";

    if (context != -1)
    {
        guide->SetContext(context);
    }
    container->AddType(guide);
}

void XMLParse::parseBar(LayerSet *container, QDomElement &element)
{
    int context = -1;
    QString align = "";
    QString orientation = "horizontal";
    QString filename = "";
    QString font = "";
    QPoint textoff = QPoint(0, 0);
    QPoint iconoff = QPoint(0, 0);
    QPoint iconsize = QPoint(0, 0);
    QRect area;

    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        cerr << "Bar needs a name\n";
        exit(0);
    }

    QString order = element.attribute("draworder", "");
    if (order.isNull() || order.isEmpty())
    {
        cerr << "Bar needs an order\n";
        exit(0);
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
            else if (info.tagName() == "orientation")
            {
                orientation = getFirstText(info);
            }
            else if (info.tagName() == "area")
            {
                area = parseRect(getFirstText(info));
                normalizeRect(area);
            }
            else if (info.tagName() == "imagefile")
            {
                filename = getFirstText(info);
            }
            else if (info.tagName() == "font")
            {
                font = getFirstText(info);
            }
            else if (info.tagName() == "align")
            {
                align = getFirstText(info);
            }
            else if (info.tagName() == "textoffset")
            {
                textoff = parsePoint(getFirstText(info));
                textoff.setX((int)(textoff.x() * wmult));
                textoff.setY((int)(textoff.y() * hmult));
            }
            else if (info.tagName() == "iconoffset")
            {
                iconoff = parsePoint(getFirstText(info));
                iconoff.setX((int)(iconoff.x() * wmult));
                iconoff.setY((int)(iconoff.y() * hmult));
            }
            else if (info.tagName() == "iconsize")
            {
                iconsize = parsePoint(getFirstText(info));
                iconsize.setX((int)(iconsize.x() * wmult));
                iconsize.setY((int)(iconsize.y() * hmult));
            }
            else
            {
                cerr << "Unknown: " << info.tagName() << " in bar\n";
                exit(0);
            }
        }
    }
    fontProp *testfont = GetFont(font);
    if (!testfont)
    {
        cerr << "Unknown font: " << font << " in bar: " << name << endl;
        exit(0);
    }

    UIBarType *bar = new UIBarType(name, filename, order.toInt(), area);
    bar->SetScreen(wmult, hmult);
    bar->SetFont(testfont);
    bar->SetTextOffset(textoff);
    bar->SetIconOffset(iconoff);
    bar->SetIconSize(iconsize);
    if (orientation == "horizontal") 
       bar->SetOrientation(1);
    else if (orientation == "vertical")
       bar->SetOrientation(2);

    if (!align.isNull() && !align.isEmpty())
    {
        if (align.lower() == "center")
            bar->SetJustification(Qt::AlignCenter);
        else if (align.lower() == "right")
            bar->SetJustification(Qt::AlignRight);
        else if (align.lower() == "allcenter")
            bar->SetJustification(Qt::AlignHCenter | Qt::AlignVCenter);
        else if (align.lower() == "vcenter")
            bar->SetJustification(Qt::AlignVCenter);
    }
    align = "";

    if (context != -1)
    {
        bar->SetContext(context);
    }
    container->AddType(bar);
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
            else if (info.tagName() == "repeatedimage")
            {
                parseRepeatedImage(container, info);
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
            else if (info.tagName() == "area")
            {
                area = parseRect(getFirstText(info));
                normalizeRect(area);
                container->SetAreaRect(area);
            }
            else if (info.tagName() == "bar")
            {
                parseBar(container, info);
            }
            else if (info.tagName() == "guidegrid")
            {
                parseGuideGrid(container, info);
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

//    container->SetAreaRect(area);
    allTypes->push_back(container);
}

void XMLParse::parseTextArea(LayerSet *container, QDomElement &element)
{
    int context = -1;
    QRect area = QRect(0, 0, 0, 0);
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
                if ((value.isNull() || value.isEmpty()) && 
                    info.attribute("lang","") == "")
                {
                    value = getFirstText(info);
                }
                else if (info.attribute("lang","").lower() == 
                         gContext->GetLanguage())
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
    if (cutdown.lower() == "no")
        text->SetCutDown(false);

    QString align = element.attribute("align", "");
    if (!align.isNull() && !align.isEmpty())
    {
        int jst = (Qt::AlignTop | Qt::AlignLeft);
        if (multiline.lower() == "yes")
        {
            jst = Qt::WordBreak;
        }
        if (align.lower() == "center")
            text->SetJustification(jst | Qt::AlignCenter);
        else if (align.lower() == "right")
            text->SetJustification(jst | Qt::AlignRight);
        else if (align.lower() == "allcenter")
            text->SetJustification(jst | Qt::AlignHCenter | Qt::AlignVCenter);
        else if (align.lower() == "vcenter")
            text->SetJustification(jst | Qt::AlignVCenter);
    }
    align = "";
    text->SetParent(container);
    text->calculateScreenArea();
    container->AddType(text);
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
                    cerr << "FcnFont needs a name\n";
                    exit(0);
                }

                if (fontfcn.isNull() || fontfcn.isEmpty())
                {   
                    cerr << "FcnFont needs a function\n";
                    exit(0);
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
                    cerr << "Fill needs a function\n";
                    exit(0);
                }
                if (fillcolor.isNull() || fillcolor.isEmpty())
                {
                    cerr << "Fill needs a color\n";
                    exit(0);
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
                fill_select_color = QColor(fillcolor);
 
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

    UIListType *list = new UIListType(name, area, draworder);
    list->SetCount(item_cnt);
    list->SetScreen(wmult, hmult);
    list->SetColumnPad(padding);
    list->SetImageSelection(select_img, select_loc);
    list->SetImageUpArrow(uparrow_img, uparrow_loc);
    list->SetImageDownArrow(dnarrow_img, dnarrow_loc);
    typedef QMap<QString,QString> fontdata;
    fontdata::Iterator it;
    for ( it = fontFunctions.begin(); it != fontFunctions.end(); ++it )
    {   
        fontProp *testFont = GetFont(it.data());
        if (testFont)
            theFonts[it.data()] = *testFont;
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
    if (order.isNull() || order.isEmpty())
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

void XMLParse::parseManagedTreeList(LayerSet *container, QDomElement &element)
{
    QRect area;
    QRect binarea;
    int bins = 1;

    QPixmap uparrow_img;
    QPixmap downarrow_img;
    QPixmap leftarrow_img;
    QPixmap rightarrow_img;

    QPixmap select_img;

    //
    //  A Map to store the geometry of
    //  the bins as we parse
    //

    typedef QMap<int, QRect> CornerMap;
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
        cerr << "ManagedTreeList needs a name\n";
        exit(0);
    }

    QString order = element.attribute("draworder", "");
    if (order.isNull() || order.isEmpty())
    {
        cerr << "ManagedTreeList needs an order\n";
        exit(0);
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
            else if(info.tagName() == "image")
            {
                QString imgname = "";
                QString file = "";

                imgname = info.attribute("function", "");
                if (imgname.isNull() || imgname.isEmpty())
                {
                    cerr << "Image needs a function\n";
                    exit(0);
                }

                file = info.attribute("filename", "");
                if (file.isNull() || file.isEmpty())
                {
                    cerr << "Image needs a filename\n";
                    exit(0);
                }

                if (imgname.lower() == "selectionbar")
                {
                    QString baseDir = gContext->GetInstallPrefix();
                    QString themeDir = gContext->FindThemeDir("");
                    themeDir = themeDir + gContext->GetSetting("Theme") + "/";
                    baseDir = baseDir + "/share/mythtv/themes/default/";
                    QFile checkFile(themeDir + file);
                    if (checkFile.exists())
                    {
                        file = themeDir + file;
                    }
                    else
                    {
                        file = baseDir + file;
                    }
                    if(!select_img.load(file))
                    {
                        cerr << "xmparse.o: I can't find a file called " << file << endl ;
                    }
                }
                else if (imgname.lower() == "uparrow")
                {
                    QString baseDir = gContext->GetInstallPrefix();
                    QString themeDir = gContext->FindThemeDir("");
                    themeDir = themeDir + gContext->GetSetting("Theme") + "/";
                    baseDir = baseDir + "/share/mythtv/themes/default/";
                    QFile checkFile(themeDir + file);
                    if (checkFile.exists())
                    {
                        file = themeDir + file;
                    }
                    else
                    {
                        file = baseDir + file;
                    }
                    if(!uparrow_img.load(file))
                    {
                        cerr << "xmparse.o: I can't find a file called " << file << endl ;
                    }
                }
                else if (imgname.lower() == "downarrow")
                {
                    QString baseDir = gContext->GetInstallPrefix();
                    QString themeDir = gContext->FindThemeDir("");
                    themeDir = themeDir + gContext->GetSetting("Theme") + "/";
                    baseDir = baseDir + "/share/mythtv/themes/default/";
                    QFile checkFile(themeDir + file);
                    if (checkFile.exists())
                    {
                        file = themeDir + file;
                    }
                    else
                    {
                        file = baseDir + file;
                    }
                    if(!downarrow_img.load(file))
                    {
                        cerr << "xmparse.o: I can't find a file called " << file << endl ;
                    }
                }
                else if (imgname.lower() == "leftarrow")
                {
                    QString baseDir = gContext->GetInstallPrefix();
                    QString themeDir = gContext->FindThemeDir("");
                    themeDir = themeDir + gContext->GetSetting("Theme") + "/";
                    baseDir = baseDir + "/share/mythtv/themes/default/";
                    QFile checkFile(themeDir + file);
                    if (checkFile.exists())
                    {
                        file = themeDir + file;
                    }
                    else
                    {
                        file = baseDir + file;
                    }
                    if(!leftarrow_img.load(file))
                    {
                        cerr << "xmparse.o: I can't find a file called " << file << endl ;
                    }
                }
                else if (imgname.lower() == "rightarrow")
                {
                    QString baseDir = gContext->GetInstallPrefix();
                    QString themeDir = gContext->FindThemeDir("");
                    themeDir = themeDir + gContext->GetSetting("Theme") + "/";
                    baseDir = baseDir + "/share/mythtv/themes/default/";
                    QFile checkFile(themeDir + file);
                    if (checkFile.exists())
                    {
                        file = themeDir + file;
                    }
                    else
                    {
                        file = baseDir + file;
                    }
                    if(!rightarrow_img.load(file))
                    {
                        cerr << "xmparse.o: I can't find a file called " << file << endl ;
                    }
                }
                else
                {
                    cerr << "xmlparse.o: I don't know what to do with an image tag who's function is " << imgname << endl;
                }
            }
            else if(info.tagName() == "bin")
            {
                QString whichbin_string = info.attribute("number", "");
                int whichbin = whichbin_string.toInt();
                if(whichbin < 1)
                {
                    cerr << "xmlparse.o: Bad setting for bin number in bin tag" << endl;
                    exit(0);
                }
                if(whichbin > bins + 1)
                {
                    cerr << "xmlparse.o: Attempt to set bin with a reference larger than number of bins" << endl;
                    exit(0);
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
                                cerr << "FcnFont needs a name\n";
                                exit(0);
                            }

                            if (fontfcn.isNull() || fontfcn.isEmpty())
                            {   
                                cerr << "FcnFont needs a function\n";
                                exit(0);
                            }
                            QString a_string = QString("bin%1-%2").arg(whichbin).arg(fontfcn);
                            fontFunctions[a_string] = fontname;
                        }
                        else
                        {
                            cerr << "Unknown tag in bin: " << info.tagName() << endl;
                            exit(0);
                        }
                    }
                }
            }
            else
            {
                cerr << "Unknown: " << info.tagName() << " in ManagedTreeList\n";
                exit(0);
            }
        }
    }


    UIManagedTreeListType *mtl = new UIManagedTreeListType(name);
    mtl->setArea(area);
    mtl->setBins(bins);
    mtl->setBinAreas(bin_corners);
    mtl->SetOrder(order.toInt());
    mtl->SetParent(container);

    //
    //  Perform moegreen/jdanner font magic
    //

    typedef QMap<QString,QString> fontdata;
    fontdata::Iterator it;
    for ( it = fontFunctions.begin(); it != fontFunctions.end(); ++it )
    {   
        fontProp *testFont = GetFont(it.data());
        if (testFont)
            theFonts[it.data()] = *testFont;
    }

    if (theFonts.size() > 0)
    {
        mtl->setFonts(fontFunctions, theFonts);
    }
    mtl->setHighlightImage(select_img);
    mtl->setArrowImages(uparrow_img, downarrow_img, leftarrow_img, rightarrow_img);
    mtl->makeHighlights();
    mtl->calculateScreenArea();
    container->AddType(mtl);
}

void XMLParse::parsePushButton(LayerSet *container, QDomElement &element)
{
    QPoint pos = QPoint(0, 0);
    QPixmap image_on, image_off, image_pushed;

    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        cerr << "PushButton needs a name\n";
        exit(0);
    }

    QString order = element.attribute("draworder", "");
    if (order.isNull() || order.isEmpty())
    {
        cerr << "PushButton needs an order\n";
        exit(0);
    }

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "position")
            {
                pos = parsePoint(getFirstText(info));
                pos.setX((int)(pos.x() * wmult));
                pos.setY((int)(pos.y() * hmult));
            }
            else if(info.tagName() == "image")
            {
                QString imgname = "";
                QString file = "";

                imgname = info.attribute("function", "");
                if (imgname.isNull() || imgname.isEmpty())
                {
                    cerr << "Image in a push button needs a function\n";
                    exit(0);
                }

                file = info.attribute("filename", "");
                if (file.isNull() || file.isEmpty())
                {
                    cerr << "Image in a push button needs a filename\n";
                    exit(0);
                }

                if (imgname.lower() == "on")
                {
                    QString baseDir = gContext->GetInstallPrefix();
                    QString themeDir = gContext->FindThemeDir("");
                    themeDir = themeDir + gContext->GetSetting("Theme") + "/";
                    baseDir = baseDir + "/share/mythtv/themes/default/";
                    QFile checkFile(themeDir + file);
                    if (checkFile.exists())
                    {
                        file = themeDir + file;
                    }
                    else
                    {
                        file = baseDir + file;
                    }
                    if(!image_on.load(file))
                    {
                        cerr << "xmparse.o: I can't find a file called " << file << endl ;
                    }
                }
                if (imgname.lower() == "off")
                {
                    QString baseDir = gContext->GetInstallPrefix();
                    QString themeDir = gContext->FindThemeDir("");
                    themeDir = themeDir + gContext->GetSetting("Theme") + "/";
                    baseDir = baseDir + "/share/mythtv/themes/default/";
                    QFile checkFile(themeDir + file);
                    if (checkFile.exists())
                    {
                        file = themeDir + file;
                    }
                    else
                    {
                        file = baseDir + file;
                    }
                    if(!image_off.load(file))
                    {
                        cerr << "xmparse.o: I can't find a file called " << file << endl ;
                    }
                }
                if (imgname.lower() == "pushed")
                {
                    QString baseDir = gContext->GetInstallPrefix();
                    QString themeDir = gContext->FindThemeDir("");
                    themeDir = themeDir + gContext->GetSetting("Theme") + "/";
                    baseDir = baseDir + "/share/mythtv/themes/default/";
                    QFile checkFile(themeDir + file);
                    if (checkFile.exists())
                    {
                        file = themeDir + file;
                    }
                    else
                    {
                        file = baseDir + file;
                    }
                    if(!image_pushed.load(file))
                    {
                        cerr << "xmparse.o: I can't find a file called " << file << endl ;
                    }
                }
            }
            else
            {
                cerr << "Unknown: " << info.tagName() << " in PushButton\n";
                exit(0);
            }
        }
    }


    UIPushButtonType *pbt = new UIPushButtonType(name, image_on, image_off, image_pushed);
    pbt->setPosition(pos);
    pbt->SetOrder(order.toInt());
    pbt->SetParent(container);
    pbt->calculateScreenArea();
    container->AddType(pbt);
}

void XMLParse::parseTextButton(LayerSet *container, QDomElement &element)
{
    QString font = "";
    QPoint pos = QPoint(0, 0);
    QPixmap image_on, image_off, image_pushed;

    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        cerr << "TextButton needs a name\n";
        exit(0);
    }

    QString order = element.attribute("draworder", "");
    if (order.isNull() || order.isEmpty())
    {
        cerr << "TextButton needs an order\n";
        exit(0);
    }

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "position")
            {
                pos = parsePoint(getFirstText(info));
                pos.setX((int)(pos.x() * wmult));
                pos.setY((int)(pos.y() * hmult));
            }
            else if (info.tagName() == "font")
            {
                font = getFirstText(info);
            }
            else if(info.tagName() == "image")
            {
                QString imgname = "";
                QString file = "";

                imgname = info.attribute("function", "");
                if (imgname.isNull() || imgname.isEmpty())
                {
                    cerr << "Image in a text button needs a function\n";
                    exit(0);
                }

                file = info.attribute("filename", "");
                if (file.isNull() || file.isEmpty())
                {
                    cerr << "Image in a text button needs a filename\n";
                    exit(0);
                }

                if (imgname.lower() == "on")
                {
                    QString baseDir = gContext->GetInstallPrefix();
                    QString themeDir = gContext->FindThemeDir("");
                    themeDir = themeDir + gContext->GetSetting("Theme") + "/";
                    baseDir = baseDir + "/share/mythtv/themes/default/";
                    QFile checkFile(themeDir + file);
                    if (checkFile.exists())
                    {
                        file = themeDir + file;
                    }
                    else
                    {
                        file = baseDir + file;
                    }
                    if(!image_on.load(file))
                    {
                        cerr << "xmparse.o: I can't find a file called " << file << endl ;
                    }
                }
                if (imgname.lower() == "off")
                {
                    QString baseDir = gContext->GetInstallPrefix();
                    QString themeDir = gContext->FindThemeDir("");
                    themeDir = themeDir + gContext->GetSetting("Theme") + "/";
                    baseDir = baseDir + "/share/mythtv/themes/default/";
                    QFile checkFile(themeDir + file);
                    if (checkFile.exists())
                    {
                        file = themeDir + file;
                    }
                    else
                    {
                        file = baseDir + file;
                    }
                    if(!image_off.load(file))
                    {
                        cerr << "xmparse.o: I can't find a file called " << file << endl ;
                    }
                }
                if (imgname.lower() == "pushed")
                {
                    QString baseDir = gContext->GetInstallPrefix();
                    QString themeDir = gContext->FindThemeDir("");
                    themeDir = themeDir + gContext->GetSetting("Theme") + "/";
                    baseDir = baseDir + "/share/mythtv/themes/default/";
                    QFile checkFile(themeDir + file);
                    if (checkFile.exists())
                    {
                        file = themeDir + file;
                    }
                    else
                    {
                        file = baseDir + file;
                    }
                    if(!image_pushed.load(file))
                    {
                        cerr << "xmparse.o: I can't find a file called " << file << endl ;
                    }
                }
            }
            else
            {
                cerr << "Unknown: " << info.tagName() << " in TextButton\n";
                exit(0);
            }
        }
    }

    fontProp *testfont = GetFont(font);
    if (!testfont)
    {
        cerr << "Unknown font: " << font << " in textbutton: " << name << endl;
        exit(0);
    }

    UITextButtonType *tbt = new UITextButtonType(name, image_on, image_off, image_pushed);
    tbt->setPosition(pos);
    tbt->setFont(testfont);
    tbt->SetOrder(order.toInt());
    tbt->SetParent(container);
    tbt->calculateScreenArea();
    container->AddType(tbt);
}

