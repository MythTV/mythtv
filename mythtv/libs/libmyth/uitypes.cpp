
#include <iostream>
using namespace std;

#include "uitypes.h"
#include "mythdialogs.h"
#include "mythcontext.h"

LayerSet::LayerSet(const QString &name)
{
    m_name = name;
    m_context = -1;
    m_debug = false;
    numb_layers = -1;
    allTypes = new vector<UIType *>;
}

LayerSet::~LayerSet()
{
    vector<UIType *>::iterator i = allTypes->begin();
    for (; i != allTypes->end(); i++)
    {
        UIType *type = (*i);
        if (type)
            delete type;
    }
    delete allTypes;
}

void LayerSet::AddType(UIType *type)
{
    type->SetDebug(m_debug);
    typeList[type->Name()] = type;
    allTypes->push_back(type);
    type->SetParent(this);
    bumpUpLayers(type->getOrder());
}

UIType *LayerSet::GetType(const QString &name)
{
    UIType *ret = NULL;
    if (typeList.contains(name))
        ret = typeList[name];

    return ret;
}

void LayerSet::bumpUpLayers(int a_number)
{
    if(a_number > numb_layers)
    {
        numb_layers = a_number;
    }
}

void LayerSet::Draw(QPainter *dr, int drawlayer, int context)
{
  if (m_context == context || m_context == -1)
  {
    vector<UIType *>::iterator i = allTypes->begin();
    for (; i != allTypes->end(); i++)
    {
        if (m_debug == true)
            cerr << "-LayerSet::Draw\n";
        UIType *type = (*i);
        type->Draw(dr, drawlayer, context);
    }
  }
}

void LayerSet::ClearAllText(void)
{
    vector<UIType *>::iterator i = allTypes->begin();
    for (; i != allTypes->end(); i++)
    {
        UIType *type = (*i);
        if (UITextType *item = dynamic_cast<UITextType *>(type))
        {
            QString defText = item->GetDefaultText();
            if ((defText == "" ) ||
                (defText.contains(QRegExp("%"))))
                item->SetText(QString(""));
        }
    }
}

void LayerSet::SetTextByRegexp(QMap<QString, QString> &regexpMap)
{
    vector<UIType *>::iterator i = allTypes->begin();
    for (; i != allTypes->end(); i++)
    {
        UIType *type = (*i);
        if (UITextType *item = dynamic_cast<UITextType *>(type))
        {
            QMap<QString, QString>::Iterator riter = regexpMap.begin();
            QString new_text = item->GetDefaultText();
            QString full_regex;

            if ((new_text == "") &&
                (regexpMap.contains(item->Name())))
            {
                new_text = regexpMap[item->Name()];
            }
            else if (new_text.contains(QRegExp("%.*%")))
                for (; riter != regexpMap.end(); riter++)
                {
                   full_regex = "%" + riter.key().upper() + 
                     "(\\|([^%|]*))?" + "(\\|([^%|]*))?" + "(\\|([^%]*))?%";
                   if (riter.data() != "")
                       new_text.replace(QRegExp(full_regex), 
                                        "\\2" + riter.data() + "\\4");
                   else
                       new_text.replace(QRegExp(full_regex), "\\6");
                }

            if (new_text != "")
                item->SetText(new_text);
        }
    }
}

void LayerSet::UseAlternateArea(bool useAlt)
{
    vector<UIType *>::iterator i = allTypes->begin();
    for (; i != allTypes->end(); i++)
    {
        UIType *type = (*i);
        if (UITextType *item = dynamic_cast<UITextType *>(type))
            item->UseAlternateArea(useAlt);
    }
}

// **************************************************************

UIType::UIType(const QString &name)
       : QObject(NULL, name)
{
    m_parent = NULL;
    m_name = name;
    m_debug = false;
    m_context = -1;
    m_order = -1;
    has_focus = false;
    takes_focus = false;
    screen_area = QRect(0,0,0,0);
}

void UIType::SetOrder(int order)
{
    m_order = order;
    if(m_parent)
    {
        m_parent->bumpUpLayers(order);
    }
}

UIType::~UIType()
{
}

void UIType::Draw(QPainter *dr, int drawlayer, int context)
{
    dr->end();
    drawlayer = 0;
    context = 0;
}

void UIType::SetParent(LayerSet *parent) 
{ 
    m_parent = parent; 
}

QString UIType::Name() 
{ 
    return m_name; 
}

bool UIType::takeFocus()
{
    if(takes_focus)
    {
        has_focus = true;
        refresh();
        emit takingFocus();
        return true;
    }
    has_focus = false;
    return false;
}

void UIType::looseFocus()
{
    emit loosingFocus();
    has_focus = false;
    refresh();
}

void UIType::calculateScreenArea()
{
    screen_area = QRect(0,0,0,0);
}

void UIType::refresh()
{
    emit requestUpdate(screen_area);
}

QString UIType::cutDown(QString info, QFont *testFont, bool multiline, int overload_width, int overload_height)
{
    QFontMetrics fm(*testFont);
    // QRect curFontSize;
    int maxwidth = screen_area.width();
    if (overload_width != -1)
        maxwidth = overload_width;
    int maxheight = screen_area.height();
    if (overload_height != -1)
        maxheight = overload_height;

    if (multiline == false)
    {
        int curFontWidth = fm.width(info);
        if (curFontWidth > maxwidth)
        {
            QString testInfo = "";
            curFontWidth = fm.width(testInfo);
            int tmaxwidth = maxwidth - fm.width("LLL");
            int count = 0;

            while (curFontWidth < tmaxwidth)
            {
                testInfo = info.left(count);
                curFontWidth = fm.width(testInfo);
                count = count + 1;
            }
            info = testInfo + "...";
        }
    }
    else
    {
        bool changed = false;
        int justification = Qt::AlignLeft | Qt::WordBreak;
	QRect curFontSize = fm.boundingRect(0, 0, maxwidth, maxheight, justification, info);
        while (curFontSize.height() > maxheight)
	{
            changed = true;
	    if (info.findRev(" ") == -1)
            {
                info = info + "...";
	        return info;
	    }
            info = info.left(info.findRev(" "));
	    curFontSize = fm.boundingRect(0, 0, maxwidth, maxheight, justification, info);
	}
	if (changed == true)
        {
            info = info.left(info.length() - 3);
	    info = info + "...";
	}
    }

    return info;
}


// **************************************************************

UIBarType::UIBarType(const QString &name, QString imgfile,
                     int dorder, QRect displayrect)
         : UIType(name)
{
    m_name = name;

    m_filename = imgfile;

    m_displaysize = displayrect;
    m_order = dorder;
    m_justification = (Qt::AlignLeft | Qt::AlignVCenter);
  
    m_textoffset = QPoint(0, 0);
    m_iconoffset = QPoint(0, 0);

}

UIBarType::~UIBarType()
{
} 

void UIBarType::SetText(int num, QString myText)
{
    textData[num] = myText;
}

void UIBarType::SetIcon(int num, QString myIcon)
{
    LoadImage(num, myIcon);
}

void UIBarType::SetIcon(int num, QPixmap myIcon)
{
    QImage sourceImg = myIcon.convertToImage();
    if (!sourceImg.isNull())
    {
        QImage scalerImg;

        scalerImg = sourceImg.smoothScale((int)(m_iconsize.x()),
                                               (int)(m_iconsize.y()));
        iconData[num].convertFromImage(scalerImg);
    }
    else
        iconData[num].resize(0, 0);
}

void UIBarType::LoadImage(int loc, QString myFile)
{
    if (m_size == 0)
    {
        cerr << "uitypes.cpp:UIBarType:LoadImage:m_size == 0";
        return;
    }
    QString filename = m_filename;
    if (loc != -1)
        filename = myFile;

    QString file;

    QString themeDir = gContext->GetThemeDir();
    QString baseDir = gContext->GetShareDir() + "themes/default/";

    QFile checkFile(themeDir + filename);

    if (checkFile.exists())
        file = themeDir + filename;
    else
        file = baseDir + filename;
    checkFile.setName(file);
    if (!checkFile.exists())
        file = "/tmp/" + filename;

    checkFile.setName(file);
    if (!checkFile.exists())
        file = filename;

    if (m_debug == true)
        cerr << "     -Filename: " << file << endl;

    QImage *sourceImg = new QImage();
    if (sourceImg->load(file))
    {
        QImage scalerImg;
        int doX = 0;
        int doY = 0;
        if (m_orientation == 1)
        {
            doX = (int)((m_displaysize.width() / m_size) * m_wmult);
            doY = (int)(m_displaysize.height() * m_hmult);
        }
        else if (m_orientation == 2)
        {
            doX = (int)(m_displaysize.width() * m_wmult);
            doY = (int)((m_displaysize.height() / m_size) * m_hmult);
        }
        if (loc != -1)
        {
            doX = m_iconsize.x();
            doY = m_iconsize.y();
        }

        scalerImg = sourceImg->smoothScale(doX, doY);
        if (loc == -1)
            m_image.convertFromImage(scalerImg);
        else
            iconData[loc].convertFromImage(scalerImg);

        if (m_debug == true)
            cerr << "     -Image: " << file << " loaded.\n";
    }
    else
    {
      if (m_debug == true)
          cerr << "     -Image: " << file << " failed to load.\n";
      iconData[loc].resize(0, 0);
    }

    delete sourceImg;

}

void UIBarType::Draw(QPainter *dr, int drawlayer, int context)
{
  if (m_context == context || m_context == -1)
  {
    if (drawlayer == m_order)
    {
      if (m_debug == true)
         cerr << "    +UIBarType::Size is " << m_size << endl;

      if (m_size < 0)
         cerr << "uitypes.cpp:UIBarType:Size is < 0!\n";
 
      int xdrop = 0;
      int ydrop = 0;
      int drawx = 0;
      int drawy = 0;

      if (m_orientation == 1)
      {
         xdrop = (int)(m_displaysize.width() / m_size);
         ydrop = m_displaysize.height();
      }
      else
      {
         xdrop = m_displaysize.width();
         ydrop = (int)(m_displaysize.height() / m_size);
      }

      for (int i = 0; i < m_size; i++)
      {
        if (m_debug == true) 
            cerr << "    +UIBarType::Drawing Item # " << i << endl;
        QPoint fontdrop = m_font->shadowOffset;
        QString msg = textData[i];

        if (m_orientation == 1)
        {
            drawx = m_displaysize.left() + (int)(i * xdrop);
            drawy = m_displaysize.top();
        }
        else
        {
            drawx = m_displaysize.left();
            drawy = m_displaysize.top() + (int)(i * ydrop);
        }
        dr->drawPixmap(drawx, drawy, m_image);
        if (!iconData[i].isNull() && iconData[i].width() > 0)
        {
            dr->drawPixmap(drawx + m_iconoffset.x(), 
                           drawy + (int)(ydrop / 2) - (int)(m_iconsize.y() / 2), 
                           iconData[i]);
        }

        dr->setFont(m_font->face);
        if (fontdrop.x() != 0 || fontdrop.y() != 0)
        {
            dr->setBrush(m_font->dropColor);
            dr->setPen(QPen(m_font->dropColor, (int)(2 * m_wmult)));
            if (m_orientation == 1)
            {
                drawx = m_displaysize.left() + fontdrop.x() + (int)(i * xdrop);
                drawy = m_displaysize.top();
            }
            else
            {
                drawx = m_displaysize.left();
                drawy = m_displaysize.top() + fontdrop.y() + (int)(i * ydrop);
            }
            if (m_debug == true)
                cerr << "    +UIBarType::Drawing Shadow @ (" << drawx 
                     << ", " << drawy << ")" << endl;
            dr->drawText(drawx + m_textoffset.x(), drawy + m_textoffset.y(),
                           xdrop - m_textoffset.x(), ydrop - (int)(2 * m_textoffset.y()),
                           m_justification, msg);
        }

        dr->setBrush(m_font->color);
        dr->setPen(QPen(m_font->color, (int)(2 * m_wmult)));
        if (m_orientation == 1)
        {
            drawx = m_displaysize.left() + (int)(i * xdrop);
            drawy = m_displaysize.top();
        }
        else
        {
            drawx = m_displaysize.left();
            drawy = m_displaysize.top() + (int)(i * ydrop);
        }
        if (m_debug == true) 
        {
            cerr << "    +UIBarType::Drawing @ (" << drawx << ", " << drawy << ")" << endl;
            cerr << "     +UIBarType::Data = " << msg << endl;
        }
        dr->drawText(drawx + m_textoffset.x(), drawy + m_textoffset.y(),
                     xdrop - m_textoffset.x(), ydrop - (int)(2 * m_textoffset.y()),
                     m_justification, msg);

        if (m_debug == true)
            cerr << "   +UIBarType::Draw() <- inside Layer\n";
      }
    }
    else
        if (m_debug == true)
        {
             cerr << "   +UIBarType::Draw() <- outside (layer = " << drawlayer
                  << ", widget layer = " << m_order << "\n";
        }      
  }
}

// **************************************************************

UIGuideType::UIGuideType(const QString &name, int order)
           : UIType(name)
{
    m_name = name;
    m_order = order;

    m_cutdown = true;
    m_count = 0;
    m_justification = (Qt::AlignLeft | Qt::AlignTop);
    m_textoffset = QPoint(0, 0);


    m_area = QRect(0, 0, 0, 0);
    m_screenloc = QPoint(0, 0);
    m_window = NULL;
    m_font = NULL;
    m_solidcolor = "";
    m_selcolor = "";
    m_seltype = 1;
    m_reccolor = "";
    m_filltype = 6;
    curArea = QRect(0, 0, 0, 0);
    m_count = 0;
    countMap.clear();
    categoryColors.clear();
    drawArea.clear();
    dataMap.clear();
    categoryMap.clear();
    recType.clear();
    recStat.clear();
    arrowUsage.clear();
}

UIGuideType::~UIGuideType()
{
}

void UIGuideType::Draw(QPainter *dr, int drawlayer, int context)
{
  if (m_context == context || m_context == -1)
  {
    if (drawlayer == m_order)
    {
        if (m_count == 0)
        {
            cout << "uitypes.cpp:UIGuideType:m_count == 0\n";
            return;
        }
        if (m_debug)
            cerr << "UIGuideType::Draw\n";
 
        typedef QMap<int, QString> DataMap;
        QString msg = "";
        DataMap::Iterator it;
        DataMap::Iterator start;
        DataMap::Iterator end;
        int i = -1;
 
        start = dataMap.begin();
        end = dataMap.end();
        for (it = start; it != end; ++it)
        {
            i = it.key();

            if (recStat[i] == 0)
                drawBackground(dr, i);
            else if (recStat[i] == 1)
                drawBox(dr, i, m_reccolor);
            else
                drawBox(dr, i, m_concolor);

            drawText(dr, i);
            drawRecType(dr, i);
        }
        drawCurrent(dr);
    }
  }
}

/*
QString UIGuideType::cutDown(QString info, QFont *testFont, int maxwidth, int maxheight)
{
    QFontMetrics fm(*testFont);
    QRect curFontSize;

    curFontSize = fm.boundingRect(0, 0, maxwidth, maxheight, m_justification, info);

    if (curFontSize.height() > maxheight)
    {
        QString testInfo = info;
        curFontSize = fm.boundingRect(0, 0, maxwidth, maxheight, m_justification, testInfo);
        int count = info.length();

        while (curFontSize.height() >= maxheight)
        {
            testInfo = info.left(count);
            curFontSize = fm.boundingRect(0, 0, maxwidth, maxheight, m_justification, testInfo);
            count = count--;
            if (count < 0)
                break;
        }
        testInfo = testInfo + "...";
        info = testInfo;
    }
    else if (curFontSize.width() > maxwidth)
    {
        int curFontWidth = fm.width(info);
        if (curFontWidth > maxwidth)
        {
            QString testInfo = "";
            curFontWidth = fm.width(testInfo);
            int tmaxwidth = maxwidth - fm.width("LLL");
            int count = 0;

            while (curFontWidth < tmaxwidth)
            {
                testInfo = info.left(count);
                curFontWidth = fm.width(testInfo);
                count = count + 1;
            }
            testInfo = testInfo + "...";
            info = testInfo;
        }
    }
    return info;

}
*/

void UIGuideType::drawCurrent(QPainter *dr)
{
    int num = 0;
    int breakin = 1;
    QRect area = curArea;

    area.setTop(area.top() + breakin);
    area.setLeft(area.left() + breakin);
    area.setHeight(area.height() - (int)(2*breakin));
    area.setWidth(area.width() - (int)(2*breakin));

  if (m_seltype == 2)
  {
    if (m_filltype == 1)
    {
        dr->setBrush( QBrush( QColor(m_selcolor), Qt::Dense4Pattern) );
        dr->setPen(QPen(QColor(m_selcolor), (int)(2 * m_wmult)));
        dr->drawRoundRect(area);
    }
    else if (m_filltype == 2)
    {
        dr->setBrush(QColor(m_selcolor));
        dr->setPen(QPen(QColor(m_selcolor), (int)(2 * m_wmult)));
        dr->drawRoundRect(area);
    }
    else if (m_filltype == 3)
    {
        dr->setBrush( QBrush( QColor(m_selcolor), Qt::Dense4Pattern) );
        dr->setPen(QPen(QColor(m_selcolor), (int)(2 * m_wmult)));
        dr->fillRect(area, QBrush( QColor(m_selcolor), Qt::Dense4Pattern));
    }
    else if (m_filltype == 4)
    {
        dr->setBrush( QBrush(QColor(m_selcolor)) );
        dr->setPen(QPen(QColor(m_selcolor), (int)(2 * m_wmult)));
        dr->fillRect(area, QBrush(QColor(m_selcolor)));
    }
    else if (m_filltype == 5)
    {
        dr->setBrush(QBrush(m_selcolor));
        dr->setPen(QPen(QColor(m_selcolor), (int)(2 * m_wmult)));
        dr->fillRect(area, QBrush(m_selcolor));
    }
    else if (m_filltype == 6)
        Blender(dr, area, num, m_selcolor);
  }
  else
  {
    dr->setBrush(QBrush(m_selcolor));
    dr->setPen(QPen(QColor(m_selcolor), (int)(2 * m_wmult)));
    dr->drawLine(area.left(), area.top(), area.right(), area.top());
    dr->drawLine(area.left(), area.top(), area.left(), area.bottom());
    dr->drawLine(area.left(), area.bottom(), area.right(), area.bottom());
    dr->drawLine(area.right(), area.top(), area.right(), area.bottom());

    dr->drawLine(area.left(), area.top() + 1, area.right(), area.top() + 1);
    dr->drawLine(area.left() + 1, area.top(), area.left() + 1, area.bottom());
    dr->drawLine(area.left(), area.bottom() - 1, area.right(), area.bottom() - 1);
    dr->drawLine(area.right() - 1, area.top(), area.right() - 1, area.bottom());
  }
 }

void UIGuideType::drawRecType(QPainter *dr, int num)
{
    int breakin = 2;
    QRect area = drawArea[num];
    area.setTop(area.top() + breakin);
    area.setLeft(area.left() + breakin);
    area.setHeight(area.height() - (int)(2*breakin));
    area.setWidth(area.width() - (int)(2*breakin));
 
    if (recType[num] != 0)
    {
        QPixmap recImg = recImages[recType[num]];

        dr->drawPixmap(area.right() - recImg.width(), 
                       area.bottom() - recImg.height(), recImg);
    }
    if (arrowUsage[num] != 0)
    {
        QPixmap user;
        if (arrowUsage[num] == 1 || arrowUsage[num] == 3)
        {
            user = arrowImages[0];

             dr->drawPixmap(area.left(), 
                            area.top() + (int)(area.height() / 2) - (int)(user.height() / 2), 
                            user);
        }
        if (arrowUsage[num] == 2 || arrowUsage[num] == 3)
        {
            user = arrowImages[1];

            dr->drawPixmap(area.right() - user.width(),
                            area.top() + (int)(area.height() / 2) - (int)(user.height() / 2),
                            user);

        }
    }
}

void UIGuideType::drawBox(QPainter *dr, int num, QString color)
{
    int breakin = 1;
    QRect area = drawArea[num];
    area.setTop(area.top() + breakin);
    area.setLeft(area.left() + breakin);
    area.setHeight(area.height() - (int)(2*breakin));
    area.setWidth(area.width() - (int)(2*breakin));

    if (m_filltype == 1)
    {
        dr->setBrush( QBrush( QColor(color), Qt::Dense4Pattern) );
        dr->setPen(QPen(QColor(color), (int)(2 * m_wmult)));
        dr->drawRoundRect(area);
    }
    else if (m_filltype == 2)
    {
        dr->setBrush(QColor(color));
        dr->setPen(QPen(QColor(color), (int)(2 * m_wmult)));
        dr->drawRoundRect(area);
    }
    else if (m_filltype == 3)
    {
        dr->setBrush( QBrush( QColor(color), Qt::Dense4Pattern) );
        dr->setPen(QPen(QColor(color), (int)(2 * m_wmult)));
        dr->fillRect(area, QBrush( QColor(color), Qt::Dense4Pattern));
    }
    else if (m_filltype == 4)
    {
        dr->setBrush( QBrush(QColor(color)) );
        dr->setPen(QPen(QColor(color), (int)(2 * m_wmult)));
        dr->fillRect(area, QBrush(QColor(color)));
    }
    else if (m_filltype == 5)
    {
        dr->setBrush(QBrush(color));
        dr->setPen(QPen(QColor(color), (int)(2 * m_wmult)));
        dr->fillRect(area, QBrush(color));
    }
    else if (m_filltype == 6)
        Blender(dr, area, num, color);
 }

void UIGuideType::drawBackground(QPainter *dr, int num)
{
    int breakin = 1;
    QRect area = drawArea[num];
    area.setTop(area.top() + breakin);
    area.setLeft(area.left() + breakin);
    area.setHeight(area.height() - (int)(2*breakin));
    area.setWidth(area.width() - (int)(2*breakin));

    if (m_filltype == 1)
    {
        dr->setBrush( QBrush( QColor(m_solidcolor), Qt::Dense4Pattern) );
        dr->setPen(QPen(QColor(m_solidcolor), (int)(2 * m_wmult)));
        dr->drawRoundRect(area);
    }
    else if (m_filltype == 2)
    {
        dr->setBrush(QColor(m_solidcolor));
        dr->setPen(QPen(QColor(m_solidcolor), (int)(2 * m_wmult)));
        dr->drawRoundRect(area);
    }
    else if (m_filltype == 3)
    {
        dr->setBrush( QBrush( QColor(m_solidcolor), Qt::Dense4Pattern) );
        dr->setPen(QPen(QColor(m_solidcolor), (int)(2 * m_wmult)));
        dr->fillRect(area, QBrush( QColor(m_solidcolor), Qt::Dense4Pattern));

        dr->setBrush( QBrush(QColor("#003367")) );
        dr->setPen(QPen(QColor("#003367"), (int)(2 * m_wmult)));
        dr->drawLine(area.left() - 1, area.top() - 1, area.right() + 1, area.top() - 1);
        dr->drawLine(area.left(), area.top(), area.right(), area.top());

        dr->setBrush( QBrush(QColor("#003367")) );
        dr->setPen(QPen(QColor("#003367"), (int)(2 * m_wmult)));
        dr->drawLine(area.left() - 1, area.top(), area.left() - 1, area.bottom());
        dr->drawLine(area.left(), area.top() + 1, area.left(), area.bottom() - 1);

        dr->setBrush( QBrush(QColor("#002536")) );
        dr->setPen(QPen(QColor("#002536"), (int)(2 * m_wmult)));
        dr->drawLine(area.left() - 1, area.bottom() + 1, area.right() + 1, area.bottom() + 1);
        dr->drawLine(area.left(), area.bottom(), area.right(), area.bottom());
        dr->drawLine(area.right() + 1, area.top() + 1, area.right() + 1, area.bottom());
        dr->drawLine(area.right(), area.top(), area.right(), area.bottom());
    }
    else if (m_filltype == 4)
    {
        dr->setBrush( QBrush(QColor(m_solidcolor)) );
        dr->setPen(QPen(QColor(m_solidcolor), (int)(2 * m_wmult)));
        dr->fillRect(area, QBrush(QColor(m_solidcolor)));

        dr->setBrush( QBrush(QColor("#003367")) );
        dr->setPen(QPen(QColor("#003367"), (int)(2 * m_wmult)));

        dr->drawLine(area.left() - 1, area.top() - 1, area.right() + 1, area.top() - 1);

        dr->setBrush( QBrush(QColor("#003367")) );
        dr->setPen(QPen(QColor("#003367"), (int)(2 * m_wmult)));
        dr->drawLine(area.left() - 1, area.top(), area.left() - 1, area.bottom());

        dr->setBrush( QBrush(QColor("#002536")) );
        dr->setPen(QPen(QColor("#002536"), (int)(2 * m_wmult)));
        dr->drawLine(area.left() - 1, area.bottom() + 1, area.right() + 1, area.bottom() + 1);
        dr->drawLine(area.right() + 1, area.top() + 1, area.right() + 1, area.bottom());

    }
    else if (m_filltype == 5)
    {
        QString color = categoryColors[categoryMap[num]];
        if (color == "")
            color = categoryColors["none"];

        dr->setBrush(QBrush(color, Qt::Dense4Pattern));
        dr->setPen(QPen(QColor(color), (int)(2 * m_wmult)));
        dr->fillRect(area, QBrush(QColor(color), Qt::Dense4Pattern));
    }
    else if (m_filltype == 6 && categoryMap[num] != "Other")
         Blender(dr, area, num);
}

void UIGuideType::Blender(QPainter *dr, QRect area, int num, QString force)
{
    if (!m_window)
    {
        cout << "UIGuideType:Blender:Error! m_window undefined, set with SetWindow(MythDialog *)\n";
        return;
    }
    QString color = categoryColors[categoryMap[num]];
    if (color == "")
        color = categoryColors["none"];

    if (force.length() > 0)
        color = force;
    int alpha = 96;
    unsigned int *data = NULL;

    QBrush br = QBrush(color);
    QRgb blendcolor = br.color().rgb();
    blendcolor = qRgba(qRed(blendcolor), qGreen(blendcolor),
                 qBlue(blendcolor), alpha);
 
    QPixmap orig(area.width() + 1, area.height() + 1);
    orig.fill(m_window, m_screenloc.x() + area.left(), 
                        m_screenloc.y() + area.top());

    QImage bgimage = orig.convertToImage();

    for (int tmpy = 0; tmpy <= area.height(); tmpy++)
    {
         data = (unsigned int *)bgimage.scanLine(tmpy);
         for (int tmpx = 0; tmpx <= area.width(); tmpx++)
         {
              QRgb pixelcolor = data[tmpx];
              data[tmpx] = blendColors(pixelcolor, blendcolor,
                           alpha);
         }
    }

    dr->drawImage(area.left(), area.top(), bgimage);
}

void UIGuideType::drawText(QPainter *dr, int num)
{
    bool m_multi = false;
    if ((m_justification & Qt::WordBreak) > 0)
        m_multi = true;
    QRect area = drawArea[num];
    QString msg;
    if (categoryMap[num].stripWhiteSpace().length() > 0)
        msg = dataMap[num] + " (" + categoryMap[num] + ")";
    else  
        msg = dataMap[num];

    QPoint fontdrop = m_font->shadowOffset;

    area.setLeft(area.left() + m_textoffset.x());
    area.setTop(area.top() + m_textoffset.y());
    area.setHeight(area.height() - m_textoffset.x());
    area.setWidth(area.width() - m_textoffset.y());

    if (arrowUsage[num] == 1 || arrowUsage[num] == 3)
    {
        area.setLeft(area.left() + arrowImages[0].width());
    }
    if (arrowUsage[num] == 2 || arrowUsage[num] == 3)
    {
        area.setRight(area.right() - arrowImages[1].width());
    }
    

    if (m_cutdown == true)
        msg = cutDown(msg, &(m_font->face), m_multi, area.width(), area.height());
    if (m_cutdown == true && m_debug == true)
        cerr << "    +UIGuideType::CutDown Called.\n";

    dr->setFont(m_font->face);
    if (fontdrop.x() != 0 || fontdrop.y() != 0)
    {
        if (m_debug == true)
        cerr << "    +UIGuideType::Drawing shadow @ (" 
             << area.left() + fontdrop.x() << ", "
             << area.top() + fontdrop.y() << ")\n";
        dr->setBrush(m_font->dropColor);
        dr->setPen(QPen(m_font->dropColor, (int)(2 * m_wmult)));
        dr->drawText(area.left() + fontdrop.x(),
                     area.top() + fontdrop.y(),
                     area.width(),
                     area.height(), m_justification, msg);
    }

    dr->setBrush(m_font->color);
    dr->setPen(QPen(m_font->color, (int)(2 * m_wmult)));
    if (m_debug == true)
        cerr << "    +UIGuideType::Drawing @ ("
             << area.left() << ", " << area.top()
             << ")" << endl;

 
    if (m_debug == true)
        cerr << "    +UIGuideType::Data = " << msg << endl;

    dr->drawText(area.left(), area.top(), area.width(), area.height(),
                 m_justification, msg);
 
}

void UIGuideType::ResetRow(unsigned int row)
{
    if (!countMap.contains(row))
        return;

    countMap[row] = -1;
    for (unsigned int i = (unsigned int)(row * 100); 
         i < (unsigned int)((unsigned int)(row * 100) + 99); i++)
    {
        if (!dataMap.contains(i))
            return;
        dataMap.remove(i);
        recType.remove(i);
        recStat.remove(i);
        drawArea.remove(i);
        categoryMap.remove(i);
    }
}

void UIGuideType::SetProgramInfo(unsigned int row, int num, QRect area, 
                                 QString title, QString genre, int arrow, 
                                 int rect, int recs)
{
    if (num > countMap[row])
        countMap[row] = num;
    dataMap[(int)(row * 100) + num] = title;
    recType[(int)(row * 100) + num] = rect;
    recStat[(int)(row * 100) + num] = recs;
    drawArea[(int)(row * 100) + num] = area;
    categoryMap[(int)(row * 100) + num] = genre;
    arrowUsage[(int)(row * 100) + num] = arrow;
    if (row > m_count)
        m_count = row;
}

void UIGuideType::LoadImage(int recType, QString file)
{
    QString themeDir = gContext->GetThemeDir();
    QString filename = themeDir + file;

    QPixmap *pix = gContext->LoadScalePixmap(filename);
    
    if (pix)
    {
        recImages[recType] = *pix;
        delete pix;
    }
    else
        recImages[recType] = QPixmap();
}

void UIGuideType::SetArrow(int dir, QString file)
{
    QString themeDir = gContext->GetThemeDir();
    QString filename = themeDir + file;

    QPixmap *pix = gContext->LoadScalePixmap(filename);

    if (pix)
    {
        arrowImages[dir] = *pix;
        delete pix;
    }
    else
        arrowImages[dir] = QPixmap();
}

// **************************************************************


UIListType::UIListType(const QString &name, QRect area, int dorder)
          : UIType(name)
{
    m_name = name;
    m_area = area;
    m_order = dorder;
    m_active = false;
    m_columns = 0;
    m_current = -1;
    m_count = 0;
    m_justification = 0;
    m_uarrow = false;
    m_darrow = false;
    m_fill_type = -1;
}

UIListType::~UIListType()
{
}

void UIListType::Draw(QPainter *dr, int drawlayer, int context)
{
  if (m_context == context || m_context == -1)
  {
    if (drawlayer == m_order)
    {
        if (m_fill_type == 1 && m_active == true)
            dr->fillRect(m_fill_area, QBrush(m_fill_color, Qt::Dense4Pattern));

        QString tempWrite;
            int left = 0;
        fontProp *tmpfont = NULL;
        bool lastShown = true;
        QPoint fontdrop = QPoint(0, 0);

        if (m_debug == true)
            cerr << "   +UIListType::Draw() <- within Layer\n";

        for (int i = 0; i < m_count; i++)
        {
            if (m_active == true)
                tmpfont = &m_fontfcns[m_fonts["active"]];
            else
                tmpfont = &m_fontfcns[m_fonts["inactive"]];

            if (forceFonts[i] != "")
                tmpfont = &m_fontfcns[forceFonts[i]];

            fontdrop = tmpfont->shadowOffset;  

            dr->setFont(tmpfont->face);

            left = m_area.left();
            for (int j = 1; j <= m_columns; j++)
            {
                if (j > 1 && lastShown == true)
                    left = left + columnWidth[j - 1] + m_pad;

                if (m_debug == true)
                {
                    cerr << "      -Column #" << j 
                         << ", Column Context: " << columnContext[j]
                         << ", Draw Context: " << context << endl;
                }

                if (columnContext[j] != context && columnContext[j] != -1)
                {
                    lastShown = false;
                }
                else
                {
                    tempWrite = listData[i + (int)(100*j)];

                    if (tempWrite != "***FILLER***")
                    {
                        if (columnWidth[j] > 0)
                            tempWrite = cutDown(tempWrite, &(tmpfont->face),
                                                false, columnWidth[j]);
                    }
                    else
                        tempWrite = "";

                    if (fontdrop.x() != 0 || fontdrop.y() != 0)
                    {
                        dr->setBrush(tmpfont->dropColor);
                        dr->setPen(QPen(tmpfont->dropColor, 
                                        (int)(2 * m_wmult)));
                        dr->drawText((int)(left + fontdrop.x()),
                                     (int)(m_area.top() + (int)(i*m_selheight) +
                                     fontdrop.y()), m_area.width(), m_selheight,
                                     m_justification, tempWrite);
                    }
                    dr->setBrush(tmpfont->color);
                    dr->setPen(QPen(tmpfont->color, (int)(2 * m_wmult)));

                    dr->drawText(left, m_area.top() + (int)(i*m_selheight),
                                 m_area.width(), m_selheight, m_justification, 
                                 tempWrite);
                    dr->setFont(tmpfont->face);
                    if (m_debug == true)
                        cerr << "   +UIListType::Draw() Data: " 
                             << tempWrite << "\n";
                    lastShown = true;
                 }
              }
          }

          if (m_uarrow == true)
              dr->drawPixmap(m_uparrow_loc, m_uparrow);
          if (m_darrow == true)
              dr->drawPixmap(m_downarrow_loc, m_downarrow);
    }
    else if (drawlayer == 8 && m_current >= 0)
    {
        QString tempWrite;
        int left = 0;
        int i = m_current;
        fontProp *tmpfont = NULL;
        if (m_active == true)
            tmpfont = &m_fontfcns[m_fonts["selected"]];
        else
            tmpfont = &m_fontfcns[m_fonts["inactive"]];

        if (forceFonts[i] != "")
            tmpfont = &m_fontfcns[forceFonts[i]];

        bool lastShown = true;
        QPoint fontdrop = tmpfont->shadowOffset;

        dr->setFont(tmpfont->face);
        dr->drawPixmap(m_area.left() + m_selection_loc.x(),
                       m_area.top() + m_selection_loc.y() + 
                       (int)(m_current * m_selheight),
                       m_selection);

        left = m_area.left();
        for (int j = 1; j <= m_columns; j++)
        {
             if (j > 1 && lastShown == true)
                    left = left + columnWidth[j - 1] + m_pad;

             if (m_debug == true)
             {
                 cerr << "      -Column #" << j 
                      << ", Column Context: " << columnContext[j]
                      << ", Draw Context: " << context << endl;
             }
             if (columnContext[j] != context && columnContext[j] != -1)
             {
                 lastShown = false;
             }
             else
             {
                 tempWrite = listData[i + (int)(100*j)];

                 if (columnWidth[j] > 0)
                     tempWrite = cutDown(tempWrite, &(tmpfont->face), false, 
                                         columnWidth[j]);
        
                 if (fontdrop.x() != 0 || fontdrop.y() != 0)
                 {
                     dr->setBrush(tmpfont->dropColor);
                     dr->setPen(QPen(tmpfont->dropColor, (int)(2 * m_wmult)));
                     dr->drawText((int)(left + fontdrop.x()),
                                  (int)(m_area.top() + (int)(i*m_selheight) + 
                                  fontdrop.y()), m_area.width(), m_selheight, 
                                  m_justification, tempWrite);
                 }
                 dr->setBrush(tmpfont->color);
                 dr->setPen(QPen(tmpfont->color, (int)(2 * m_wmult)));

                 dr->drawText(left, m_area.top() + (int)(i*m_selheight),
                              m_area.width(), m_selheight, m_justification, 
                              tempWrite);

                 dr->setFont(tmpfont->face);
             }
        }
    }
    else
    {
        if (m_debug == true)
            cerr << "   +UIListType::Draw() <- outside (layer = " << drawlayer
                 << ", widget layer = " << m_order << "\n";
    }
  }
}

void UIListType::SetItemText(int num, int column, QString data)
{
    if (column > m_columns)
        m_columns = column;
    listData[(int)(num + (int)(column * 100))] = data;
}

QString UIListType::GetItemText(int num, int column)
{
    QString ret;
    ret = listData[(int)(num + (int)(column * 100))];
    return ret;
}

void UIListType::SetItemText(int num, QString data)
{
    m_columns = 1;
    listData[num + 100] = data;
}

// *****************************************************************

UIImageType::UIImageType(const QString &name, const QString &filename, int dorder, QPoint displaypos)
           : UIType(name)
{
    m_isvalid = false;
    m_flex = false;
    img = QPixmap();

    m_filename = filename;
    orig_filename = filename;
    m_displaypos = displaypos;
    m_order = dorder;
    m_force_x = -1;
    m_force_y = -1;
    m_drop_x = 0;
    m_drop_y = 0;
    m_show = false;
}

UIImageType::~UIImageType()
{
}

void UIImageType::LoadImage()
{
    QString file;
    int transparentFlag = gContext->GetNumSetting("PlayBoxTransparency", 1);
    if (m_flex == true)
    {
        if (transparentFlag == 1)
            m_filename = "trans-" + m_filename;
        else
            m_filename = "solid-" + m_filename;
    }

    QString themeDir = gContext->GetThemeDir();
    QString baseDir = gContext->GetShareDir() + "themes/default/";

    QString filename = themeDir + m_filename;

    if (m_force_x == -1 && m_force_y == -1)
    {
        QPixmap *tmppix = gContext->LoadScalePixmap(filename);
        if (tmppix)
        {
            img = *tmppix;
            m_show = true;
            
            delete tmppix;
            refresh();
        }
    }

    QFile checkFile(filename);

    if (checkFile.exists())
        file = themeDir + m_filename;
    else
        file = baseDir + m_filename;
    checkFile.setName(file);
    if (!checkFile.exists())
        file = "/tmp/" + m_filename;

    checkFile.setName(file);
    if (!checkFile.exists())
        file = m_filename;

    if (m_debug == true)
        cerr << "     -Filename: " << file << endl;

    if (m_hmult == 1 && m_wmult == 1 && m_force_x == -1 && m_force_y == -1)
    {
        if (img.load(file))
            m_show = true;
    }
    else
    {
        QImage *sourceImg = new QImage();
        if (sourceImg->load(file))
        {
            QImage scalerImg;
            int doX = sourceImg->width();
            int doY = sourceImg->height();
            if (m_force_x != -1)
            {
                doX = m_force_x;
                if (m_debug == true)
                    cerr << "         +Force X: " << doX << endl;
            }
            if (m_force_y != -1)
            {
                doY = m_force_y;
                if (m_debug == true)
                    cerr << "         +Force Y: " << doY << endl;
            }

            scalerImg = sourceImg->smoothScale((int)(doX * m_wmult),
                                               (int)(doY * m_hmult));
            m_show = true;
            img.convertFromImage(scalerImg);
            if (m_debug == true)
                    cerr << "     -Image: " << file << " loaded.\n";
        }
        else
        {
            m_show = false;
            if (m_debug == true)
                cerr << "     -Image: " << file << " failed to load.\n";
        }
        delete sourceImg;
    }
    
    //
    //  On the odd chance we are owned by a 
    //  MythThemedDialog, ask to be re-painted
    //
    
    refresh();
}

void UIImageType::Draw(QPainter *dr, int drawlayer, int context)
{
    if (m_context == context || m_context == -1)
    {
        if (drawlayer == m_order)
        {
            if (!img.isNull() && m_show == true)
            {
                if (m_debug == true)
                {
                    cerr << "   +UIImageType::Draw() <- inside Layer\n";
                    cerr << "       -Drawing @ (" << m_displaypos.x() << ", " << m_displaypos.y() << ")" << endl;
                    cerr << "       -Skip Section: (" << m_drop_x << ", " << m_drop_y << ")\n";
                }
                dr->drawPixmap(m_displaypos.x(), m_displaypos.y(), img, m_drop_x, m_drop_y);
            }
            else if (m_debug == true)
            {
                cerr << "   +UIImageType::Draw() <= Image is null\n";
            }
        }
    }
    else if (m_debug == true)
    {
            cerr << "   +UIImageType::Draw() <- outside (layer = " << drawlayer
                 << ", widget layer = " << m_order << "\n";
    }
}

void UIImageType::refresh()
{
    //
    //  Figure out how big we are
    //  and where we are in screen
    //  coordinates
    //
    
    QRect r = QRect(m_displaypos.x(),
                    m_displaypos.y(),
                    img.width(),
                    img.height());
    
    if(m_parent)
    {
        r.moveBy(m_parent->GetAreaRect().left(),
                 m_parent->GetAreaRect().top());

        //
        //  Tell someone who cares
        //
    
        emit requestUpdate(r);
    }
    else
    {
        //
        //  This happens when parent isn't set, 
        //  usually during initial parsing.
        //
        emit requestUpdate();
    }
}

// ******************************************************************

UIRepeatedImageType::UIRepeatedImageType(const QString &name, const QString &filename, int dorder, QPoint displaypos)
        : UIImageType(name, filename, dorder, displaypos)
{
    m_repeat = 0;
    m_highest_repeat = 0;
    m_orientation = 0;
}

void UIRepeatedImageType::Draw(QPainter *p, int drawlayer, int context)
{
    if (m_context == context || m_context == -1)
    {
        if (drawlayer == m_order)
        {
            if (!img.isNull() && m_show == true)
            {
                if (m_debug == true)
                {
                    cerr << "   +UIRepeatedImageType::Draw() <- inside Layer\n";
                    cerr << "       -Drawing @ (" << m_displaypos.x() << ", " << m_displaypos.y() << ")" << endl;
                    cerr << "       -Skip Section: (" << m_drop_x << ", " << m_drop_y << ")\n";
                }
                if(m_orientation == 0)
                {
                    for(int i = 0; i < m_repeat; i++)
                    {
                        p->drawPixmap(m_displaypos.x() + (i * img.width()), m_displaypos.y(), img, m_drop_x, m_drop_y);
                    }
                }
                else if (m_orientation == 1)
                {
                    for(int i = 0; i < m_repeat; i++)
                    {
                        p->drawPixmap(m_displaypos.x() - (i * img.width()), m_displaypos.y(), img, m_drop_x, m_drop_y);
                    }
                }
                else if (m_orientation == 2)
                {
                    for(int i = 0; i < m_repeat; i++)
                    {
                        p->drawPixmap(m_displaypos.x(),  m_displaypos.y() - (i * img.height()), img, m_drop_x, m_drop_y);
                    }
                }
                else if (m_orientation == 3)
                {
                    for(int i = 0; i < m_repeat; i++)
                    {
                        p->drawPixmap(m_displaypos.x(),  m_displaypos.y() + (i * img.height()), img, m_drop_x, m_drop_y);
                    }
                }
            } 
            else if (m_debug == true)
            {
                cerr << "   +UIImageType::Draw() <= Image is null\n";
            }
        }
      
    }
    else
    {
        if (m_debug == true)
        {
            cerr << "   +UIImageType::Draw() <- outside (layer = " << drawlayer
                 << ", widget layer = " << m_order << "\n";
        }
    }
}

void UIRepeatedImageType::setRepeat(int how_many)
{
    if(how_many >= 0)
    {
        m_repeat = how_many;
        if(how_many > m_highest_repeat)
        {
            m_highest_repeat = how_many;
        }
        refresh();
    }
}

void UIRepeatedImageType::setOrientation(int x)
{
    if(x < 0 || x > 3)
    {
        cerr << "uitypes.o: UIRepeatedImageType received an invalid request to set orientation to " << x << endl;
        return;
    }
    m_orientation = x ;
}

void UIRepeatedImageType::refresh()
{
    //
    //  Figure out how big we are
    //  and where we are in screen
    //  coordinates
    //
    
    QRect r = QRect(0,0,0,0);
    
    if(m_orientation == 0)
    {
        r = QRect(m_displaypos.x(),
                  m_displaypos.y(),
                  img.width() * m_highest_repeat,
                  img.height());
    }
    else if(m_orientation == 1)
    {
        r = QRect(m_displaypos.x() - (m_highest_repeat * img.width()),
                  m_displaypos.y(),
                  img.width() * (m_highest_repeat + 1),
                  img.height());
    }    
    else if(m_orientation == 2)
    {
        r = QRect(m_displaypos.x(),
                  m_displaypos.y() - (m_highest_repeat * img.height()),
                  img.width(),
                  img.height() * (m_highest_repeat + 1));
    }    
    else if (m_orientation == 3)
    {
        r = QRect(m_displaypos.x(),
                  m_displaypos.y(),
                  img.width(),
                  img.height() * m_highest_repeat);
    }    
    if(m_parent)
    {
        r.moveBy(m_parent->GetAreaRect().left(),
                 m_parent->GetAreaRect().top());

        //
        //  Tell someone who cares
        //
    
        emit requestUpdate(r);
    }
    else
    {
        //
        //  This happens when parent isn't set, 
        //  usually during initial parsing.
        //
        emit requestUpdate();
    }
}




// ******************************************************************

UITextType::UITextType(const QString &name, fontProp *font,
                       const QString &text, int dorder, QRect displayrect,
                       QRect altdisplayrect)
           : UIType(name)
{
    m_name = name;
    m_message = text;
    m_default_msg = text;
    m_font = font;
    m_displaysize = displayrect;
    m_origdisplaysize = displayrect;
    m_altdisplaysize = altdisplayrect;
    m_cutdown = true;
    m_order = dorder;
    m_justification = (Qt::AlignLeft | Qt::AlignTop);
}

UITextType::~UITextType()
{
}

void UITextType::UseAlternateArea(bool useAlt)
{
    if (useAlt && (m_altdisplaysize.width() > 1))
        m_displaysize = m_altdisplaysize;
    else
        m_displaysize = m_origdisplaysize;
}

void UITextType::SetText(const QString &text)
{
    m_message = text;
    refresh();
}

void UITextType::Draw(QPainter *dr, int drawlayer, int context)
{
  if (m_context == context || m_context == -1)
    if (drawlayer == m_order)
    {
        bool m_multi = false;
	if ((m_justification & Qt::WordBreak) > 0)
            m_multi = true;
        QPoint fontdrop = m_font->shadowOffset;
        QString msg = m_message;
        dr->setFont(m_font->face);
        if (m_cutdown == true)
            msg = cutDown(msg, &(m_font->face), m_multi, m_displaysize.width(), m_displaysize.height());
        if (m_cutdown == true && m_debug == true)
            cerr << "    +UITextType::CutDown Called.\n";

        if (fontdrop.x() != 0 || fontdrop.y() != 0)
        {
            if (m_debug == true)
                cerr << "    +UITextType::Drawing shadow @ (" 
                     << (int)(m_displaysize.left() + fontdrop.x()) << ", "
                     << (int)(m_displaysize.top() + fontdrop.y()) << ")" << endl;
            dr->setBrush(m_font->dropColor);
            dr->setPen(QPen(m_font->dropColor, (int)(2 * m_wmult)));
            dr->drawText((int)(m_displaysize.left() + fontdrop.x()),
                           (int)(m_displaysize.top() + fontdrop.y()),
                           m_displaysize.width(), 
                           m_displaysize.height(), m_justification, msg);
        }

        dr->setBrush(m_font->color);
        dr->setPen(QPen(m_font->color, (int)(2 * m_wmult)));
        if (m_debug == true)
                cerr << "    +UITextType::Drawing @ (" 
                     << (int)(m_displaysize.left()) << ", " << (int)(m_displaysize.top()) 
                     << ")" << endl;
        dr->drawText(m_displaysize.left(), m_displaysize.top(), 
                      m_displaysize.width(), m_displaysize.height(), m_justification, msg);
        if (m_debug == true)
        {
            cerr << "   +UITextType::Draw() <- inside Layer\n";
            cerr << "       -Message: " << m_message << " (cut: " << msg << ")" <<  endl;
        }
    }
    else
        if (m_debug == true)
        {
             cerr << "   +UITextType::Draw() <- outside (layer = " << drawlayer
                  << ", widget layer = " << m_order << "\n";
        }
}

/*
QString UITextType::cutDown(QString info, QFont *testFont, int maxwidth, int maxheight)
{
    QFontMetrics fm(*testFont);
    QRect curFontSize;

    curFontSize = fm.boundingRect(0, 0, maxwidth, maxheight, m_justification, info);

    if (curFontSize.height() > (int)(1.5 * maxheight))
    {
        QString testInfo = info;
        curFontSize = fm.boundingRect(0, 0, maxwidth, maxheight, m_justification, testInfo);
        int count = info.length();

        while (curFontSize.height() >= maxheight)
        {
            testInfo = info.left(count);
            curFontSize = fm.boundingRect(0, 0, maxwidth, maxheight, m_justification, testInfo);
            count = count--;
            if (count < 0)
                break;
        }
        testInfo = testInfo + "...";
        info = testInfo;
    }
    else if (curFontSize.width() > maxwidth)
    {
        int curFontWidth = fm.width(info);
        if (curFontWidth > maxwidth)
        {
            QString testInfo = "";
            curFontWidth = fm.width(testInfo);
            int tmaxwidth = maxwidth - fm.width("LLL");
            int count = 0;

            while (curFontWidth < tmaxwidth)
            {
                testInfo = info.left(count);
                curFontWidth = fm.width(testInfo);
                count = count + 1;
            }
            testInfo = testInfo + "...";
            info = testInfo;
        }
    }
    return info;

}
*/

void UITextType::calculateScreenArea()
{
    QRect r = m_displaysize;
    r.moveBy(m_parent->GetAreaRect().left(),
             m_parent->GetAreaRect().top());
    screen_area = r;
}


// ******************************************************************

UIStatusBarType::UIStatusBarType(QString &name, QPoint loc, int dorder)
               : UIType(name)
{
    m_location = loc;
    m_order = dorder;
    m_orientation = 0;
}

UIStatusBarType::~UIStatusBarType()
{
}

void UIStatusBarType::Draw(QPainter *dr, int drawlayer, int context)
{
    if (m_context == context || m_context == -1)
    {
        if (drawlayer == m_order)
        {
            if (m_debug == true)
            {
                cerr << "   +UIStatusBarType::Draw() <- within Layer\n";
            }
            int width = (int)((double)((double)m_container.width() - (double)(2*m_fillerSpace))
                    * (double)((double)m_used / (double)m_total));

            int height = (int)((double)((double)m_container.height() - (double)(2*m_fillerSpace))
                    * (double)((double)m_used / (double)m_total));

	        if (m_debug == true)
	        {
                cerr << "       -Width  = " << width << "\n";
                cerr << "       -Height = " << height << endl;
            }

            if(m_orientation == 0)
            {
                dr->drawPixmap(m_location.x(), m_location.y(), m_container);
                dr->drawPixmap(m_location.x(), m_location.y(), m_filler, 0, 0, width + m_fillerSpace);
            }
            else if(m_orientation == 1)
            {
                dr->drawPixmap(m_location.x(), m_location.y(), m_container);
                dr->drawPixmap(m_location.x() + width, m_location.y(), m_filler, width - m_fillerSpace, 0);
            }
            else if(m_orientation == 2)
            {
                dr->drawPixmap(m_location.x(), m_location.y(), m_container);
                dr->drawPixmap( m_location.x(), 
                                (m_location.y() + m_container.height()) - height, 
                                m_filler, 
                                0, 
                                (m_filler.height() - height) - m_fillerSpace);
            }
            else if(m_orientation == 3)
            {
                dr->drawPixmap(m_location.x(), m_location.y(), m_container);
                dr->drawPixmap(m_location.x(), m_location.y(), m_filler, 0, 0, -1, height + m_fillerSpace);
            }
        }
    }
}

void UIStatusBarType::calculateScreenArea()
{
    QRect r = QRect(m_location.x(), 
                    m_location.y(),
                    m_container.width(),
                    m_container.height());
    r.moveBy(m_parent->GetAreaRect().left(),
             m_parent->GetAreaRect().top());
    screen_area = r;
}

void UIStatusBarType::setOrientation(int x)
{
    if(x < 0 || x > 3)
    {
        cerr << "uitypes.o: UIStatusBarType received an invalid request to set orientation to " << x << endl;
        return;
    }
    m_orientation = x ;
}




// *********************************************************************

GenericTree::GenericTree()
{
    init();
}

GenericTree::GenericTree(const QString a_string)
{
    init();
    my_string = a_string;
}

GenericTree::GenericTree(QString a_string, int an_int)
{
    init();
    my_string = a_string;
    my_int = an_int;
}

GenericTree::GenericTree(QString a_string, int an_int, bool selectable_flag)
{
    init();
    my_string = a_string;
    my_int = an_int;
    selectable = selectable_flag;
}

void GenericTree::init()
{
    my_subnodes.setAutoDelete(true);

    my_parent = NULL;
    my_string = "";
    my_stringlist.clear();
    my_int = 0;
    my_subnodes.clear();
    my_flatened_subnodes.clear();
    my_ordered_subnodes.clear();
    my_selected_subnode = NULL;
    current_ordering_index = -1;
    selectable = false;
    //my_subnodes.setAutoDelete(true);
    
    //
    //  Use 4 here, because we know that's what 
    //  mythmusic wants (limits resizing)
    //
    
    my_attributes = new IntVector(4);
}

GenericTree* GenericTree::addNode(QString a_string)
{
    GenericTree *new_node = new GenericTree(a_string.stripWhiteSpace());
    new_node->setParent(this);
    my_subnodes.append(new_node);
    my_ordered_subnodes.append(new_node);

    return new_node;
}

GenericTree* GenericTree::addNode(QString a_string, int an_int)
{
    GenericTree *new_node = new GenericTree(a_string.stripWhiteSpace());
    new_node->setInt(an_int);
    new_node->setParent(this);
    my_subnodes.append(new_node);
    my_ordered_subnodes.append(new_node);
    return new_node;
}

GenericTree* GenericTree::addNode(QString a_string, int an_int, bool selectable_flag)
{
    GenericTree *new_node = new GenericTree(a_string.stripWhiteSpace());
    new_node->setInt(an_int);
    new_node->setParent(this);
    new_node->setSelectable(selectable_flag);
    my_subnodes.append(new_node);
    my_ordered_subnodes.append(new_node);
    return new_node;
}

int GenericTree::calculateDepth(int start)
{
    int current_depth;
    int found_depth;
    current_depth = start + 1;
    QPtrListIterator<GenericTree> it( my_subnodes );
    GenericTree *my_kids;

    while( (my_kids = it.current()) != 0)
    {
        found_depth = my_kids->calculateDepth(start + 1);
        if(found_depth > current_depth)
        {
            current_depth = found_depth;
        }
        ++it;
    }
    return current_depth;
}

GenericTree* GenericTree::findLeaf()
{
    if(my_subnodes.count() > 0)
    {
        return my_subnodes.getFirst()->findLeaf();
    }
    return this;
}

GenericTree* GenericTree::findLeaf(int ordering_index)
{
    if(my_subnodes.count() > 0)
    {
        GenericTree *first_kid_in_order = getChildAt(0, ordering_index);
        return first_kid_in_order->findLeaf(ordering_index);
    }
    return this;
}


GenericTree* GenericTree::findNode(QValueList<int> route_of_branches)
{
    //
    //  Starting from *this* node (which will often be root)
    //  find a set of branches that have id's that match
    //  the collection passed in route_of_branches. Return
    //  the end point of those branches (which will often be
    //  a leaf node).
    //
    //  In practical terms, mythmusic will use this to force the
    //  playback screen's ManagedTreeList to move to a given track
    //  in a given playlist (for example).
    //

    return recursiveNodeFinder(route_of_branches);
}

GenericTree* GenericTree::recursiveNodeFinder(QValueList<int> route_of_branches)
{
    if(checkNode(route_of_branches))
    {
        return this;
    }
    else
    {
        QPtrListIterator<GenericTree> it( my_subnodes );
        GenericTree *my_kids;
        while( (my_kids = it.current()) != 0)
        {
            GenericTree *sub_checker = my_kids->recursiveNodeFinder(route_of_branches);
            if(sub_checker)
            {
                return sub_checker;
            }
            else
            {
                ++it;
            }
        }
    }
    return NULL;
}

bool GenericTree::checkNode(QValueList<int> route_of_branches)
{
    bool found_it = true;
    GenericTree *parent_finder = this;

    for(int i = route_of_branches.count() - 1; i > -1 ; --i)
    {
        if(!(parent_finder->getInt() == (*route_of_branches.at(i))))
        {
            found_it = false;
        }
        if(i > 0)
        {
            if(parent_finder->getParent())
            {
                parent_finder = parent_finder->getParent();
            }
            else
            {
                found_it = false;
            }
        }
    }
    return found_it;
}

int GenericTree::getChildPosition(GenericTree *which_child)
{
    return my_subnodes.findRef(which_child);
}

int GenericTree::getChildPosition(GenericTree *which_child, int ordering_index)
{
    if(current_ordering_index != ordering_index)
    {
        reorderSubnodes(ordering_index);
        current_ordering_index = ordering_index;
    }
    return my_ordered_subnodes.findRef(which_child);
}

int GenericTree::getPosition()
{
    if(my_parent)
    {
        return my_parent->getChildPosition(this);
    }
    return 0;
}

int GenericTree::getPosition(int ordering_index)
{
    if(my_parent)
    {
        if(my_parent->getOrderingIndex() != ordering_index)
        {
            my_parent->reorderSubnodes(ordering_index);
            my_parent->setOrderingIndex(ordering_index);
        }
        return my_parent->getChildPosition(this, ordering_index);
    }
    return 0;
}

int GenericTree::siblingCount()
{
    if(my_parent)
    {
        return my_parent->childCount();
    }
    return 1;
}

void GenericTree::printTree(int margin)
{
    for(int i = 0; i < margin; i++)
    {
        cout << " " ;
    }
    cout << "GenericTreeNode: my int is " << my_int << ", my string is \"" << my_string << "\"" << endl;
    
    //
    //  recurse through my children
    //

    QPtrListIterator<GenericTree> it( my_ordered_subnodes );
    GenericTree *my_kids;
    while( (my_kids = it.current()) != 0)
    {
        my_kids->printTree(margin + 4);
        ++it;
    }
}

GenericTree* GenericTree::getChildAt(uint reference)
{
    if(reference >= my_ordered_subnodes.count())
    {
        cerr << "uitypes.o: out of bounds request to GenericTree::getChildAt()" << endl;
        return NULL;
    }
    return my_subnodes.at(reference);
}

GenericTree* GenericTree::getChildAt(uint reference, int ordering_index)
{
    if(reference >= my_ordered_subnodes.count())
    {
        cerr << "uitypes.o: out of bounds request to GenericTree::getChildAt()" << endl;
        return NULL;
    }
    if(ordering_index != current_ordering_index)
    {
        reorderSubnodes(ordering_index);
        current_ordering_index = ordering_index;
    }
    return my_ordered_subnodes.at(reference);
}

GenericTree* GenericTree::getSelectedChild(int ordering_index)
{
    if(my_selected_subnode)
    {
        return my_selected_subnode;
    }
    return getChildAt(0, ordering_index);
}

void GenericTree::becomeSelectedChild()
{
    if(my_parent)
    {
        my_parent->setSelectedChild(this);
    }
    else
    {
        cerr << "I can't make myself the selected child because I have no parent" << endl;
    }
}

GenericTree* GenericTree::prevSibling(int number_up)
{

    if(!my_parent)
    {
        //  
        //  I'm root = no siblings
        //
        
        return NULL;
    }

    int my_position = my_parent->getChildPosition(this);

    if(my_position < number_up)
    {
        //
        //  not enough siblings "above" me
        //
        
        return NULL;
    }
    
    return my_parent->getChildAt(my_position - number_up);
}

GenericTree* GenericTree::prevSibling(int number_up, int ordering_index)
{

    if(!my_parent)
    {
        //  
        //  I'm root = no siblings
        //
        
        return NULL;
    }

    int my_position = my_parent->getChildPosition(this, ordering_index);

    if(my_position < number_up)
    {
        //
        //  not enough siblings "above" me
        //
        
        return NULL;
    }
    
    return my_parent->getChildAt(my_position - number_up, ordering_index);
}

GenericTree* GenericTree::nextSibling(int number_down)
{
    if(!my_parent)
    {
        //  
        //  I'm root = no siblings
        //
        
        return NULL;
    }

    int my_position = my_parent->getChildPosition(this);

    if(my_position + number_down >= my_parent->childCount())
    {
        //
        //  not enough siblings "below" me
        //
        
        return NULL;
    }
    
    return my_parent->getChildAt(my_position + number_down);
}

GenericTree* GenericTree::nextSibling(int number_down, int ordering_index)
{
    if(!my_parent)
    {
        //  
        //  I'm root = no siblings
        //
        
        return NULL;
    }

    int my_position = my_parent->getChildPosition(this, ordering_index);

    if(my_position + number_down >= my_parent->childCount())
    {
        //
        //  not enough siblings "below" me
        //
        
        return NULL;
    }
    
    return my_parent->getChildAt(my_position + number_down, ordering_index);
}

GenericTree* GenericTree::getParent()
{
    if(my_parent)
    {
        return my_parent;
    }
    return NULL;
}

void GenericTree::setAttribute(uint attribute_position, int value_of_attribute)
{
    //
    //  You can use attibutes for anything you like.
    //  Mythmusic, for example, stores a value for 
    //  random ordering in the first "column" (0) and
    //  a value for "intelligent" (1) ordering in the 
    //  second column
    //
    
    if(my_attributes->size() < attribute_position + 1)
    {
        my_attributes->resize(attribute_position + 1, -1);
    }
    my_attributes->at(attribute_position) = value_of_attribute;
}

int GenericTree::getAttribute(uint which_one)
{
    if(my_attributes->size() < which_one + 1)
    {
        cerr << "uitypes.o: something asked a GenericTree node for an attribute that doesn't exist" << endl;
    }
    return my_attributes->at(which_one);
}

void GenericTree::reorderSubnodes(int ordering_index)
{
    //
    //  The nodes are there, we just want to re-order them
    //  according to attribute column defined by
    //  ordering_index
    //

    bool something_changed = false;
    if(my_ordered_subnodes.count() > 1)
    {
        something_changed = true;
    }

    while(something_changed)
    {
        something_changed = false;
        for(uint i = 0; i < my_ordered_subnodes.count() - 1;)
        {
            if(my_ordered_subnodes.at(i)->getAttribute(ordering_index)   >
               my_ordered_subnodes.at(i+1)->getAttribute(ordering_index) )
            {
                something_changed = true;
                GenericTree *temp = my_ordered_subnodes.take(i + 1);
                my_ordered_subnodes.insert(i, temp); 
            }
            ++i;
        }
    }
}

void GenericTree::addYourselfIfSelectable(QPtrList<GenericTree> *flat_list)
{
    if(selectable)
    {
        flat_list->append(this);
    }
    QPtrListIterator<GenericTree> it( my_subnodes );
    GenericTree *my_kids;
    while( (my_kids = it.current()) != 0)
    {
        my_kids->addYourselfIfSelectable(flat_list);
        ++it;
    }
}

void GenericTree::buildFlatListOfSubnodes(int ordering_index, bool scrambled_parents)
{
    //
    //  This builds a flat list of every SELECTABLE child 
    //  according to some some_ordering index.
    //
    
    my_flatened_subnodes.clear();

    QPtrListIterator<GenericTree> it( my_subnodes );
    GenericTree *my_kids;
    while( (my_kids = it.current()) != 0)
    {
        my_kids->addYourselfIfSelectable(&my_flatened_subnodes);
        ++it;
    }

    if(scrambled_parents)
    {
        //
        //  sort the flatened list in the order indicated
        //  by ordering_index
        //
        
        bool something_changed = false;
        if(my_flatened_subnodes.count() > 1)
        {
            something_changed = true;
        }

        while(something_changed)
        {
            something_changed = false;
            for(uint i = 0; i < my_flatened_subnodes.count() - 1;)
            {
                if(my_flatened_subnodes.at(i)->getAttribute(ordering_index)   >
                   my_flatened_subnodes.at(i+1)->getAttribute(ordering_index) )
                {
                    something_changed = true;
                    GenericTree *temp = my_flatened_subnodes.take(i + 1);
                    my_flatened_subnodes.insert(i, temp); 
                }
                ++i;
            }
        }
    }
    
    /*
    
    //
    //  Debugging
    //
    
    QPtrListIterator<GenericTree> iterator(my_flatened_subnodes);
    GenericTree *selectable_node;
    while( (selectable_node = iterator.current()) != 0)
    {
        cout << selectable_node->getString() << endl ;
        ++iterator;
    }
    
    */
}

GenericTree* GenericTree::nextPrevFromFlatList(bool forward_or_backward, bool wrap_around, GenericTree *active)
{
    int i = my_flatened_subnodes.findRef(active);
    if(i < 0)
    {
        cerr << "uitypes.o: Can't find active item on flatened list of subnodes" << endl;
        return NULL;
    }
    if(forward_or_backward)
    {
        ++i;
        if(i >= (int) my_flatened_subnodes.count())
        {
            if(wrap_around)
            {
                i = 0;
            }
            else
            {
                return NULL;
            }
        }
    }
    else
    {
        --i;
        if(i < 0)
        {
            if(wrap_around)
            {
                i = my_flatened_subnodes.count() - 1;
            }
            else
            {
                return NULL;
            }
        }
        
    }
    return my_flatened_subnodes.at(i);
}

GenericTree* GenericTree::getChildByName(QString a_name)
{
    GenericTree *oops;
    oops = NULL;

    QPtrListIterator<GenericTree> it( my_subnodes );
    GenericTree *my_kids;
    while( (my_kids = it.current()) != 0)
    {
        if(my_kids->getString() == a_name)
        {
            return my_kids;
        }
        ++it;
    }
    return oops;
}

void GenericTree::sortByString()
{

    bool something_changed = false;
    if(my_ordered_subnodes.count() > 1)
    {
        something_changed = true;
    }
    while(something_changed)
    {
        something_changed = false;
        for(uint i = 0; i < my_ordered_subnodes.count() - 1;)
        {
            if(QString::localeAwareCompare
                (
                    my_ordered_subnodes.at(i)->getString().lower(),
                    my_ordered_subnodes.at(i+1)->getString().lower()
                ) 
            > 0)
            {
                something_changed = true;
                GenericTree *temp = my_ordered_subnodes.take(i + 1);
                my_ordered_subnodes.insert(i, temp); 
            }
            ++i;
        }
    }

    QPtrListIterator<GenericTree> it( my_subnodes );
    GenericTree *my_kids;
    while( (my_kids = it.current()) != 0)
    {
        my_kids->sortByString();
        ++it;
    }
}

void GenericTree::sortBySelectable()
{
    bool something_changed = false;
    if(my_ordered_subnodes.count() > 1)
    {
        something_changed = true;
    }
    while(something_changed)
    {
        something_changed = false;
        for(uint i = 0; i < my_ordered_subnodes.count() - 1;)
        {
            if(my_ordered_subnodes.at(i)->isSelectable() &&
               !my_ordered_subnodes.at(i+1)->isSelectable())
            {
                something_changed = true;
                GenericTree *temp = my_ordered_subnodes.take(i + 1);
                my_ordered_subnodes.insert(i, temp); 
            }
            ++i;
        }
    }

    QPtrListIterator<GenericTree> it( my_subnodes );
    GenericTree *my_kids;
    while( (my_kids = it.current()) != 0)
    {
        my_kids->sortBySelectable();
        ++it;
    }
}

void GenericTree::deleteAllChildren()
{
    my_flatened_subnodes.clear();
    my_ordered_subnodes.clear();
    my_selected_subnode = NULL;
    current_ordering_index = -1;
    my_subnodes.setAutoDelete(true);
    
    //
    //  This may (?) be a memory leak ?
    //  perhaps we should recursively descend
    //  and prune from the bottom up (?).
    //
    my_subnodes.clear();
    my_subnodes.setAutoDelete(false);
}

GenericTree::~GenericTree()
{
    delete my_attributes;
}

// ********************************************************************

UIManagedTreeListType::UIManagedTreeListType(const QString & name)
                     : UIType(name)
{
    bins = 0;
    bin_corners.clear();
    screen_corners.clear();
    route_to_active.clear();
    resized_highlight_images.setAutoDelete(true);
    my_tree_data = NULL;
    current_node = NULL;
    active_node = NULL;
    active_parent = NULL;
    m_justification = (Qt::AlignLeft | Qt::AlignVCenter);
    active_bin = 0;
    tree_order = -1;
    visual_order = -1;
    show_whole_tree = false;
    scrambled_parents = false;
    color_selectables = false;
}

UIManagedTreeListType::~UIManagedTreeListType()
{
}

void UIManagedTreeListType::drawText(QPainter *p, 
                                    QString the_text, 
                                    QString font_name, 
                                    int x, int y, 
                                    int bin_number)
{
    fontProp *temp_font = NULL;
    QString a_string = QString("bin%1-%2").arg(bin_number).arg(font_name);
    temp_font = &m_fontfcns[m_fonts[a_string]];
    p->setFont(temp_font->face);
    p->setPen(QPen(temp_font->color, (int)(2 * m_wmult)));
    
    if(!show_whole_tree)
    {
        the_text = cutDown(the_text, &(temp_font->face), false, area.width() - 80, area.height());
        p->drawText(x, y, the_text);
    }
    else if(bin_number == bins)
    {
        the_text = cutDown(the_text, &(temp_font->face), false, bin_corners[bin_number].width() - right_arrow_image.width(), bin_corners[bin_number].height());
        p->drawText(x, y, the_text);
    }
    else if(bin_number == 1)
    {
        the_text = cutDown(the_text, &(temp_font->face), false, bin_corners[bin_number].width() - left_arrow_image.width(), bin_corners[bin_number].height());
        p->drawText(x + left_arrow_image.width(), y, the_text);
    }
    else
    {
        the_text = cutDown(the_text, &(temp_font->face), false, bin_corners[bin_number].width(), bin_corners[bin_number].height());
        p->drawText(x, y, the_text);
    }
}

void UIManagedTreeListType::Draw(QPainter *p, int drawlayer, int context)
{
    //  Do nothing if context is wrong;

    if(m_context != context)
    {
        if(m_context != -1)
        {
            return;
        }
    }

    if(drawlayer != m_order)
    {
        // not my turn
        return;
    }

    if(!current_node)
    {
        // no data (yet?)
        return;
    }

    bool draw_up_arrow = false;
    bool draw_down_arrow = false;

    //
    //  Draw each bin, using current_node and working up
    //  and/or down to tell us what to draw in each bin.
    //

    int starting_bin = 0;
    int ending_bin = 0;
    
    if(show_whole_tree)
    {
        starting_bin = bins;
        ending_bin = 0;
    }
    else
    {
        starting_bin = bins;
        ending_bin = bins - 1;
    }

    for(int i = starting_bin; i > ending_bin; --i)
    {
        GenericTree *hotspot_node = current_node;

        if(i < active_bin)
        {
            for(int j = 0; j < bins - i; j++)
            {
                if(hotspot_node)
                {
                    if(hotspot_node->getParent())
                    {
                        hotspot_node = hotspot_node->getParent();
                    }
                }
            }
        }
        if(i > active_bin)
        {
            for(int j = 0; j < i - active_bin; j++)
            {
                if(hotspot_node)
                {
                    if(hotspot_node->childCount() > 0)
                    {
                        hotspot_node = hotspot_node->getSelectedChild(visual_order);
                    }
                    else
                    {
                        hotspot_node = NULL;
                    }
                }
            }
        }
        
        //
        //  OK, we have the starting node for this bin (if it exists)
        //
        
        if(hotspot_node)
        {
            QString a_string = QString("bin%1-active").arg(i);
            fontProp *tmpfont = &m_fontfcns[m_fonts[a_string]];
            int x_location = bin_corners[i].left();
            int y_location = bin_corners[i].top() + (bin_corners[i].height() / 2)
                             + (QFontMetrics(tmpfont->face).height() / 2);
                             
            if(!show_whole_tree)
            {
                x_location = area.left();
                y_location = area.top() + (area.height() / 2) + (QFontMetrics(tmpfont->face).height() / 2);
            }

            if(i == bins)
            {
                draw_up_arrow = true;
                draw_down_arrow = true;

                //
                //  if required, move the hotspot up or down
                //  (beginning of list, end of list, etc.)
                //
                
                int position_in_list = hotspot_node->getPosition(visual_order);
                int number_in_list = hotspot_node->siblingCount();
    
                int number_of_slots = 0;
                int another_y_location = y_location - QFontMetrics(tmpfont->face).height();
                int a_limit = bin_corners[i].top();
                if(!show_whole_tree)
                {
                    a_limit = area.top();
                }
                while (another_y_location - QFontMetrics(tmpfont->face).height() > a_limit)
                {
                    another_y_location -= QFontMetrics(tmpfont->face).height();
                    ++number_of_slots;
                }
                
                if(position_in_list <= number_of_slots)
                {
                    draw_up_arrow = false;
                }
                if(position_in_list < number_of_slots)
                {
                    for(int j = 0; j < number_of_slots - position_in_list; j++)
                    {
                        y_location -= QFontMetrics(tmpfont->face).height();
                    }
                }
                if((number_in_list - position_in_list) <= number_of_slots &&
                   position_in_list > number_of_slots)
                {
                    draw_down_arrow = false;
                    if(number_in_list >= number_of_slots * 2 + 1)
                    { 
                        for(int j = 0; j <= number_of_slots - (number_in_list - position_in_list); j++)
                        {
                            y_location += QFontMetrics(tmpfont->face).height();
                        }
                    }
                    else
                    {
                        for(int j = 0; j <= position_in_list - (number_of_slots + 1); j++)
                        {
                            y_location += QFontMetrics(tmpfont->face).height();
                        }
                    }
                }
                if((number_in_list - position_in_list) == number_of_slots + 1)
                {
                    draw_down_arrow = false;
                }
                if(number_in_list <  (number_of_slots * 2) + 2)
                {
                    draw_down_arrow = false;
                    draw_up_arrow = false;
                }
            }
            
            QString font_name = "active";
            if(i > active_bin)
            {
                font_name = "inactive";
            }
            if(hotspot_node == active_node)
            {
                font_name = "selected";
            }
            if(i == active_bin && color_selectables && hotspot_node->isSelectable())
            {
                font_name = "selectable";
            }

            

            if(i == active_bin)
            {
                //
                //  Draw the highlight pixmap for this bin
                //
                
                if(show_whole_tree)
                {
                    p->drawPixmap(x_location, y_location - QFontMetrics(tmpfont->face).height() + QFontMetrics(tmpfont->face).descent(), (*highlight_map[i]));

                    //
                    //  Left or right arrows
                    //
                    if(i == bins && hotspot_node->childCount() > 0)
                    {
                        p->drawPixmap(x_location + (*highlight_map[i]).width() - right_arrow_image.width(),
                                      y_location - QFontMetrics(tmpfont->face).height() + right_arrow_image.height() / 2,
                                      right_arrow_image);
                    }
                    if(i == 1 && hotspot_node->getParent()->getParent())
                    {
                        p->drawPixmap(x_location,
                                      y_location - QFontMetrics(tmpfont->face).height() + left_arrow_image.height() / 2,
                                      left_arrow_image);
                    }
                }
                else
                {
                    p->drawPixmap(x_location, y_location - QFontMetrics(tmpfont->face).height() + QFontMetrics(tmpfont->face).descent(), (*highlight_map[0]));
                }
            }

            //
            //  Move this down for people with (slightly) broken fonts
            //
            
            QString msg = hotspot_node->getString();
            drawText(p, msg, font_name, x_location, y_location, i);

            if(i == bins)
            {
                //
                //  Draw up/down arrows
                //
                
                if(draw_up_arrow)
                {
                    if(show_whole_tree)
                    {
                        p->drawPixmap(bin_corners[i].right() - up_arrow_image.width(), 
                                      bin_corners[i].top(), 
                                      up_arrow_image);
                    }
                    else
                    {
                        p->drawPixmap(area.right() - up_arrow_image.width(), 
                                      area.top(), 
                                      up_arrow_image);
                    }
                }
                if(draw_down_arrow)
                {
                    if(show_whole_tree)
                    {
                        p->drawPixmap(bin_corners[i].right() - down_arrow_image.width(),
                                      bin_corners[i].bottom() - down_arrow_image.height(),
                                      down_arrow_image);
                    }
                    else
                    {
                        p->drawPixmap(area.right() - down_arrow_image.width(),
                                      area.bottom() - down_arrow_image.height(),
                                      down_arrow_image);
                    }
                }
            }
            
            //
            //  Do the ones above
            //
            
            int numb_above = 1;
            int still_yet_another_y_location = y_location - QFontMetrics(tmpfont->face).height();
            int a_limit = bin_corners[i].top();
            if(!show_whole_tree)
            {
                a_limit = area.top();
            }
            while (still_yet_another_y_location - QFontMetrics(tmpfont->face).height() > a_limit)
            {
                GenericTree *above = hotspot_node->prevSibling(numb_above, visual_order);
                if(above)
                {
                    if(i == active_bin && color_selectables && above->isSelectable())
                    {
                        font_name = "selectable";
                    }
                    else if(above == active_node)
                    {
                        font_name = "selected";
                    }
                    else if(i == active_bin)
                    {
                        font_name = "active";
                    }
                    else
                    {
                        font_name = "inactive";
                    }
                    msg = above->getString();
                    drawText(p, msg, font_name, x_location, still_yet_another_y_location, i);
                }    
                still_yet_another_y_location -= QFontMetrics(tmpfont->face).height();
                numb_above++;
            }
            

            //
            //  Do the ones below
            //
            
            int numb_below = 1;
            y_location += QFontMetrics(tmpfont->face).height();
            a_limit = bin_corners[i].bottom();
            if(!show_whole_tree)
            {
                a_limit = area.bottom();
            }
            while (y_location < a_limit)
            {
                GenericTree *below = hotspot_node->nextSibling(numb_below, visual_order);
                if(below)
                {
                    if(i == active_bin && color_selectables && below->isSelectable())
                    {
                        font_name = "selectable";
                    }
                    else if(below == active_node)
                    {
                        font_name = "selected";
                    }
                    else if(i == active_bin)
                    {
                        font_name = "active";
                    }
                    else
                    {
                        font_name = "inactive";
                    }
                    msg = below->getString();
                    drawText(p, msg, font_name, x_location, y_location, i);
                }    
                y_location += QFontMetrics(tmpfont->face).height();
                numb_below++;
            }

        }
        else
        {
            //
            //  This bin is empty
            //
            // p->eraseRect(bin_corners[i]);
        }
    }


    //
    //  Debugging, draw edges around bins
    //

    /*
    p->setPen(QColor(255,0,0));
    CornerMap::Iterator it;
    for ( it = bin_corners.begin(); it != bin_corners.end(); ++it )
    {
        p->drawRect(it.data());
    }
    */

}

void UIManagedTreeListType::moveToNode(QValueList<int> route_of_branches)
{
    current_node = my_tree_data->findNode(route_of_branches);
    if(!current_node)
    {
        current_node = my_tree_data->findLeaf();
    }
    active_node = current_node;
    active_parent = active_node->getParent();
    emit nodeSelected(current_node->getInt(), current_node->getAttributes());
}

void UIManagedTreeListType::moveToNodesFirstChild(QValueList<int> route_of_branches)
{
    GenericTree *finder = my_tree_data->findNode(route_of_branches);

    if(finder)
    {
        if(finder->childCount() > 0)
        {
            current_node = finder->getChildAt(0, tree_order);
            active_node = current_node;
            active_parent = active_node->getParent();
            emit nodeSelected(current_node->getInt(), current_node->getAttributes());
        }
        else
        {
            current_node = finder;
            active_node = NULL;
            active_parent = NULL;
            emit nodeSelected(current_node->getInt(), current_node->getAttributes());
        }    
    }
    else
    {
        current_node = my_tree_data->findLeaf();
        active_node = NULL;
    }
}

QValueList <int> * UIManagedTreeListType::getRouteToActive()
{
    if(active_node)
    {
        route_to_active.clear();
        GenericTree *climber = active_node;
        
        route_to_active.push_front(climber->getInt());
        while( (climber = climber->getParent()) )
        {
            route_to_active.push_front(climber->getInt());
        }
        return &route_to_active;   
    }
    return NULL;    
    return &route_to_active;
}

bool UIManagedTreeListType::tryToSetActive(QValueList <int> route)
{
    if(&route)
    {
        GenericTree *a_node = my_tree_data->findNode(route);
        if(a_node && a_node->isSelectable())
        {
            active_node = a_node;
            current_node = a_node;
            active_parent = active_node->getParent();
            return true;
        }
    }
    return false;
}

void UIManagedTreeListType::assignTreeData(GenericTree *a_tree)
{
    if(a_tree)
    {
        my_tree_data = a_tree;

        //
        //  By default, current_node is first branch at every
        //  level down till we hit a leaf node.
        //
    
        current_node = my_tree_data->findLeaf();
        active_bin = bins;
    }
    else
    {
        cerr << "uitypes.o: somebody just assigned me to assign tree data, but they gave me no data" << endl;
    }

}

void UIManagedTreeListType::setCurrentNode(GenericTree *a_node)
{
    //
    //  We should really do a recursive check of the tree
    //  to see if this node exists somewhere...
    //
    
    if(a_node)
    {
        current_node = a_node;
    }
}

void UIManagedTreeListType::makeHighlights()
{
    resized_highlight_images.clear();
    highlight_map.clear();

    //
    //  (pre-)Draw the highlight pixmaps
    //

    for(int i = 1; i <= bins; i++)
    {
        QImage temp_image = highlight_image.convertToImage();
        QPixmap *temp_pixmap = new QPixmap();
        fontProp *tmpfont = NULL;
        QString a_string = QString("bin%1-active").arg(i);
        tmpfont = &m_fontfcns[m_fonts[a_string]];
        temp_pixmap->convertFromImage(temp_image.smoothScale(bin_corners[i].width(), QFontMetrics(tmpfont->face).height() ));
        resized_highlight_images.append(temp_pixmap);
        highlight_map[i] = temp_pixmap;
    }
    
    //
    //  Make no tree version
    //
    
    QImage temp_image = highlight_image.convertToImage();
    QPixmap *temp_pixmap = new QPixmap();
    fontProp *tmpfont = NULL;
    QString a_string = QString("bin%1-active").arg(bins);
    tmpfont = &m_fontfcns[m_fonts[a_string]];
    temp_pixmap->convertFromImage(temp_image.smoothScale(area.width(), QFontMetrics(tmpfont->face).height() ));
    resized_highlight_images.append(temp_pixmap);
    highlight_map[0] = temp_pixmap;
    
    
}

bool UIManagedTreeListType::popUp()
{
    //
    //  Move the active node to the
    //  current active node's parent  
    //
    
    if(!current_node)
    {
        return false;
    }
    
    if(!current_node->getParent())
    {
        return false;
    }
    
    if(!current_node->getParent()->getParent())
    {
        //
        //  I be ultimate root
        //
        
        return false;
        
    }
    
    if(!show_whole_tree)
    {
        //
        //  Oh no you don't
        //
        
        return false;
    }
    
    if(active_bin > 1)
    {
        --active_bin;
        current_node = current_node->getParent();
        emit nodeEntered(current_node->getInt(), current_node->getAttributes());
    }
    else if(active_bin < bins)
    {
        ++active_bin;
    }
    refresh();
    return true;
}

bool UIManagedTreeListType::pushDown()
{
    //
    //  Move the active node to the
    //  current active node's first child
    //

    if(!current_node)
    {
        return false;
    }

    if(current_node->childCount() < 1)
    {
        //
        //  I be leaf
        return false;
    }
    
    if(!show_whole_tree)
    {
        //
        //  Bad tree, bad!
        //
        return false;
    }

    if(active_bin < bins)
    {
        ++active_bin;
        current_node = current_node->getSelectedChild(visual_order);
        emit nodeEntered(current_node->getInt(), current_node->getAttributes());
    }
    else if(active_bin > 1)
    {
        --active_bin;
    }

    refresh();    
    return true;
}

bool UIManagedTreeListType::moveUp(bool do_refresh)
{
    if(!current_node)
    {
        return false;
    }
    //
    //  Move the active node to the
    //  current active node's previous
    //  sibling  
    //

    GenericTree *new_node = current_node->prevSibling(1, visual_order);
    if(new_node)
    {
        current_node = new_node;
        if(do_refresh)
        {
            if(show_whole_tree)
            {
                for(int i = active_bin; i <= bins; i++)
                {
                    emit requestUpdate(screen_corners[i]);
                }
            }
            else
            {
                refresh();
            }
        }
        emit nodeEntered(current_node->getInt(), current_node->getAttributes());
        current_node->becomeSelectedChild();
        return true;
    }
    return false;
}

bool UIManagedTreeListType::moveDown(bool do_refresh)
{
    if(!current_node)
    {
        return false;
    }

    //
    //  Move the active node to the
    //  current active node's next
    //  sibling
    //

    GenericTree *new_node = current_node->nextSibling(1, visual_order);
    if(new_node)
    {
        current_node = new_node;
        if(do_refresh)
        {
            if(show_whole_tree)
            {
                for(int i = active_bin; i <= bins; i++)
                {
                    emit requestUpdate(screen_corners[i]);
                }
            }
            else
            {
                refresh();
            }
        }
        emit nodeEntered(current_node->getInt(), current_node->getAttributes());
        current_node->becomeSelectedChild();
        return true;
    }
    return false;
}

int UIManagedTreeListType::calculateEntriesInBin(int bin_number)
{
    //
    //  Given the various font metrics and the
    //  size of the bin, figure out how many lines
    //  the draw() method would use to know how many
    //  entries can appear in a given bin
    //
    
    if(bin_number < 1 || bin_number > bins)
    {
        return 0;
    }
    int return_value = 1;
    
    
    QString a_string = QString("bin%1-active").arg(bin_number);
    fontProp *tmpfont = &m_fontfcns[m_fonts[a_string]];
    int y_location =    bin_corners[bin_number].top() 
                        + (bin_corners[bin_number].height() / 2) 
                        + (QFontMetrics(tmpfont->face).height() / 2);
    if(!show_whole_tree)
    {
        y_location = area.top() + (area.height() / 2) + (QFontMetrics(tmpfont->face).height() / 2);
    }
    int another_y_location = y_location - QFontMetrics(tmpfont->face).height();
    int a_limit = bin_corners[bin_number].top();
    if(!show_whole_tree)
    {
        a_limit = area.top();
    }
    while (another_y_location - QFontMetrics(tmpfont->face).height() > a_limit)
    {
        another_y_location -= QFontMetrics(tmpfont->face).height();
         ++return_value;
    }
    y_location += QFontMetrics(tmpfont->face).height();
    a_limit = bin_corners[bin_number].bottom();
    if(!show_whole_tree)
    {
        a_limit = area.bottom();
    }
    while (y_location < a_limit)
    {
        y_location += QFontMetrics(tmpfont->face).height();
        ++return_value;
    }
    return return_value;
}

bool UIManagedTreeListType::pageUp()
{
    if(!current_node)
    {
        return false;
    }
    int entries_to_jump = calculateEntriesInBin(active_bin);
    for(int i = 0; i < entries_to_jump; i++)
    {
        if(!moveUp(false))
        {
            i = entries_to_jump;
        }
        
    }
    if(show_whole_tree)
    {
        for(int i = active_bin; i <= bins; i++)
        {
            emit requestUpdate(screen_corners[i]);
        }
    }
    else
    {
        refresh();
    }
    return true;
}

bool UIManagedTreeListType::pageDown()
{
    if(!current_node)
    {
        return false;
    }
    if(!current_node)
    {
        return false;
    }
    int entries_to_jump = calculateEntriesInBin(active_bin);
    for(int i = 0; i < entries_to_jump; i++)
    {
        if(!moveDown(false))
        {
            i = entries_to_jump;
        }
        
    }
    if(show_whole_tree)
    {
        for(int i = active_bin; i <= bins; i++)
        {
            emit requestUpdate(screen_corners[i]);
        }
    }
    else
    {
        refresh();
    }
    return true;
}

void UIManagedTreeListType::select()
{
    if(current_node)
    {
        if(current_node->isSelectable())
        {
            active_node = current_node;
            active_parent = active_node->getParent();
            if(show_whole_tree)
            {
                emit requestUpdate(screen_corners[active_bin]);
            }
            else
            {
                refresh();
            }
            emit nodeSelected(current_node->getInt(), current_node->getAttributes());
        }
        else
        {
            GenericTree *first_leaf = current_node->findLeaf(tree_order);
            if(first_leaf->isSelectable())
            {
                active_node = first_leaf;
                active_parent = current_node;
                active_parent->buildFlatListOfSubnodes(tree_order, scrambled_parents);
                refresh();
                emit nodeSelected(active_node->getInt(), active_node->getAttributes());
            }
        }
    }
}

void UIManagedTreeListType::activate()
{
    if(active_node)
    {
        emit requestUpdate(screen_corners[active_bin]);
        emit nodeSelected(active_node->getInt(), active_node->getAttributes());
    }
}

void UIManagedTreeListType::enter()
{
    if(current_node)
    {
        emit nodeEntered(current_node->getInt(), current_node->getAttributes());
    }
}

bool UIManagedTreeListType::nextActive(bool wrap_around, bool traverse_up_down)
{
    if(!active_node)
    {
        return false;
    }
    if(traverse_up_down && active_parent != active_node->getParent())
    {
        return complexInternalNextPrevActive(true, wrap_around);
    }

    //
    //  set a flag if active and current are sync'd
    //
    
    bool in_sync = false;
    
    if(current_node == active_node)
    {
        in_sync = true;
    }
    
    if(active_node)
    {
        GenericTree *test_node = active_node->nextSibling(1, tree_order);
        if(test_node)
        {
            active_node = test_node;
            if(in_sync)
            {
                current_node = active_node;
            }
            if(show_whole_tree)
            {
                emit requestUpdate(screen_corners[active_bin]);
            }
            else
            {
                refresh();
            }
            return true;
        }
        else if(wrap_around)
        {
            test_node = active_node->getParent();
            if(test_node)
            {
                test_node = test_node->getChildAt(0, tree_order);
                if(test_node)
                {
                    active_node = test_node;
                    if(in_sync)
                    {
                        current_node = active_node;
                    }
                    if(show_whole_tree)
                    {
                        emit requestUpdate(screen_corners[active_bin]);
                    }
                    else
                    {
                        refresh();
                    }
                    return true;
                }
            }
        }
    }
    return false;
}

bool UIManagedTreeListType::prevActive(bool wrap_around, bool traverse_up_down)
{
    if(!active_node)
    {
        return false;
    }
    if(traverse_up_down && active_parent != active_node->getParent())
    {
        return complexInternalNextPrevActive(false, wrap_around);
    }
    bool in_sync = false;
    
    if(current_node == active_node)
    {
        in_sync = true;
    }
    
    if(active_node)
    {
        GenericTree *test_node = active_node->prevSibling(1, tree_order);
        if(test_node)
        {
            active_node = test_node;
            if(in_sync)
            {
                current_node = active_node;
            }
            if(show_whole_tree)
            {
                emit requestUpdate(screen_corners[active_bin]);
            }
            else
            {
                refresh();
            }
            return true;
        }
        else if(wrap_around)
        {
            test_node = active_node->getParent();
            if(test_node)
            {
                int numb_children = test_node->childCount();
                if(numb_children > 0)
                {
                    test_node = test_node->getChildAt(numb_children - 1, tree_order);
                    if(test_node)
                    {
                        active_node = test_node;
                        if(in_sync)
                        {
                            current_node = active_node;
                        }
                        if(show_whole_tree)
                        {
                            emit requestUpdate(screen_corners[active_bin]);
                        }
                        else
                        {
                            refresh();
                        }
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

bool UIManagedTreeListType::complexInternalNextPrevActive(bool forward_or_back, bool wrap_around)
{
    if(active_parent)
    {
        bool in_sync = false;
        if(current_node == active_node)
        {
            in_sync = true;
        }
        GenericTree *next;
        next = active_parent->nextPrevFromFlatList(forward_or_back, wrap_around, active_node);
        if(next)
        {
            active_node = next;
            if(in_sync)
            {
                current_node = active_node;
            }
            return true;
        }
    }
    return false;
}

void UIManagedTreeListType::syncCurrentWithActive()
{
    current_node = active_node;
    requestUpdate();
}

void UIManagedTreeListType::calculateScreenArea()
{
    int i = 0;
    CornerMap::Iterator it;
    for ( it = bin_corners.begin(); it != bin_corners.end(); ++it )
    {
        QRect r = (*it);
        r.moveBy(m_parent->GetAreaRect().left(),
                 m_parent->GetAreaRect().top());
        ++i;
        screen_corners[i] = r;
    }
    
    screen_area = m_parent->GetAreaRect();
}


// ********************************************************************

UIPushButtonType::UIPushButtonType(const QString &name, QPixmap on, QPixmap off, QPixmap pushed)
                     : UIType(name)
{
    on_pixmap = on;
    off_pixmap = off;
    pushed_pixmap = pushed;
    currently_pushed = false;
    takes_focus = true;
    connect(&push_timer, SIGNAL(timeout()), this, SLOT(unPush()));
}


void UIPushButtonType::Draw(QPainter *p, int drawlayer, int context)
{
    if(context != m_context)
    {
        if(m_context != -1)
        {
            return;
        }
    }

    if(drawlayer != m_order)
    {
        // not my turn
        return;
    }

    if(currently_pushed)
    {
        p->drawPixmap(m_displaypos.x(), m_displaypos.y(), pushed_pixmap);
    }
    else
    {
        if(has_focus)
        {
            p->drawPixmap(m_displaypos.x(), m_displaypos.y(), on_pixmap);
        }
        else
        {
            p->drawPixmap(m_displaypos.x(), m_displaypos.y(), off_pixmap);
        }
    }
    
}

void UIPushButtonType::push()
{
    if(currently_pushed)
    {
        return;
    }
    currently_pushed = true;
    push_timer.start(300, TRUE);
    refresh();
    emit pushed();
}

void UIPushButtonType::unPush()
{
    currently_pushed = false;    
    refresh();
}

void UIPushButtonType::calculateScreenArea()
{
    int x, y, width, height;
    
    x  = m_displaypos.x();
    x += m_parent->GetAreaRect().left();

    y  = m_displaypos.y();
    y += m_parent->GetAreaRect().top();

    width = off_pixmap.width();
    if(on_pixmap.width() > width)
    {
        width = on_pixmap.width();
    }
    if(pushed_pixmap.width() > width)
    {
        width = pushed_pixmap.width();
    }

    height = off_pixmap.height();
    if(on_pixmap.height() > height)
    {
        height = on_pixmap.height();
    }
    if(pushed_pixmap.height() > height)
    {
        height = pushed_pixmap.height();
    }
    
    screen_area = QRect(x, y, width, height);
}

// ********************************************************************

UITextButtonType::UITextButtonType(const QString &name, QPixmap on, QPixmap off, QPixmap pushed)
                     : UIType(name)
{
    on_pixmap = on;
    off_pixmap = off;
    pushed_pixmap = pushed;
    m_text = "";
    currently_pushed = false;
    takes_focus = true;
    connect(&push_timer, SIGNAL(timeout()), this, SLOT(unPush()));
}


void UITextButtonType::Draw(QPainter *p, int drawlayer, int context)
{
    if(context != m_context)
    {
        if(m_context != -1)
        {
            return;
        }
    }

    if(drawlayer != m_order)
    {
        // not my turn
        return;
    }

    if(currently_pushed)
    {
        p->drawPixmap(m_displaypos.x(), m_displaypos.y(), pushed_pixmap);
    }
    else
    {
        if(has_focus)
        {
            p->drawPixmap(m_displaypos.x(), m_displaypos.y(), on_pixmap);
        }
        else
        {
            p->drawPixmap(m_displaypos.x(), m_displaypos.y(), off_pixmap);
        }
        p->setFont(m_font->face);
        p->setBrush(m_font->color);
        p->setPen(QPen(m_font->color, (int)(2 * m_wmult)));
        p->drawText(m_displaypos.x(),
                m_displaypos.y(),
                off_pixmap.width(),
                off_pixmap.height(),
                Qt::AlignCenter,
                m_text);
    }    
}

void UITextButtonType::push()
{
    if(currently_pushed)
    {
        return;
    }
    currently_pushed = true;
    push_timer.start(300, TRUE);
    refresh();
    emit pushed();
}

void UITextButtonType::unPush()
{
    currently_pushed = false;    
    refresh();
}

void UITextButtonType::setText(const QString some_text)
{
    m_text = some_text;
    refresh();
}

void UITextButtonType::calculateScreenArea()
{
    int x, y, width, height;
    
    x  = m_displaypos.x();
    x += m_parent->GetAreaRect().left();

    y  = m_displaypos.y();
    y += m_parent->GetAreaRect().top();

    width = off_pixmap.width();
    if(on_pixmap.width() > width)
    {
        width = on_pixmap.width();
    }
    if(pushed_pixmap.width() > width)
    {
        width = pushed_pixmap.width();
    }

    height = off_pixmap.height();
    if(on_pixmap.height() > height)
    {
        height = on_pixmap.height();
    }
    if(pushed_pixmap.height() > height)
    {
        height = pushed_pixmap.height();
    }
    
    screen_area = QRect(x, y, width, height);
}

// ********************************************************************

UICheckBoxType::UICheckBoxType(const QString &name, 
                               QPixmap checkedp, QPixmap uncheckedp,
                               QPixmap checked_highp, QPixmap unchecked_highp)
               :UIType(name)
{
    checked_pixmap = checkedp;
    unchecked_pixmap = uncheckedp;
    checked_pixmap_high = checked_highp;
    unchecked_pixmap_high = unchecked_highp;
    checked = false;
    label = "";
    takes_focus = true;
}

void UICheckBoxType::Draw(QPainter *p, int drawlayer, int context)
{
    if(context != m_context)
    {
        if(m_context != -1)
        {
            return;
        }
    }
    if(drawlayer != m_order)
    {   
    }
    if(has_focus)
    {
        if(checked)
        {
            p->drawPixmap(m_displaypos.x(), m_displaypos.y(), checked_pixmap_high);
        }
        else
        {
            p->drawPixmap(m_displaypos.x(), m_displaypos.y(), unchecked_pixmap_high);
        }
    }
    else
    {
        if(checked)
        {
            p->drawPixmap(m_displaypos.x(), m_displaypos.y(), checked_pixmap);
        }
        else
        {
            p->drawPixmap(m_displaypos.x(), m_displaypos.y(), unchecked_pixmap);
        }
    }
}

void UICheckBoxType::calculateScreenArea()
{
    int x, y, width, height;
    
    x  = m_displaypos.x();
    x += m_parent->GetAreaRect().left();

    y  = m_displaypos.y();
    y += m_parent->GetAreaRect().top();

    width = checked_pixmap.width();
    if(unchecked_pixmap.width() > width)
    {
        width = unchecked_pixmap.width();
    }
    if(checked_pixmap_high.width() > width)
    {
        width = checked_pixmap_high.width();
    }
    if(unchecked_pixmap_high.width() > width)
    {
        width = unchecked_pixmap_high.width();
    }

    height = checked_pixmap.height();
    if(unchecked_pixmap.height() > height)
    {
        height = unchecked_pixmap.height();
    }
    if(checked_pixmap_high.height() > height)
    {
        height = checked_pixmap_high.height();
    }
    if(unchecked_pixmap_high.height() > height)
    {
        height = unchecked_pixmap_high.height();
    }
    
    screen_area = QRect(x, y, width, height);

}

void UICheckBoxType::push()
{
    checked = !checked;
    refresh();
    emit pushed(checked);
}

void UICheckBoxType::setState(bool checked_or_not)
{
    checked = checked_or_not;
    refresh();
}

// ********************************************************************

UISelectorType::UISelectorType(const QString &name, 
                               QPixmap on, 
                               QPixmap off, 
                               QPixmap pushed,
                               QRect area)
               :UIPushButtonType(name, on, off, pushed)
{
    m_area = area;
    my_data.clear();
    my_data.setAutoDelete(true);
    current_data = NULL;
}

void UISelectorType::Draw(QPainter *p, int drawlayer, int context)
{
    if(context != m_context)
    {
        if(m_context != -1)
        {
            return;
        }
    }

    if(drawlayer != m_order)
    {
        // not my turn
        return;
    }

    if(currently_pushed)
    {
        p->drawPixmap(m_displaypos.x(), m_displaypos.y(), pushed_pixmap);
    }
    else
    {
        if(has_focus)
        {
            p->drawPixmap(m_displaypos.x(), m_displaypos.y(), on_pixmap);
        }
        else
        {
            p->drawPixmap(m_displaypos.x(), m_displaypos.y(), off_pixmap);
        }
    }
    if(current_data)
    {
        p->setFont(m_font->face);
        p->setBrush(m_font->color);
        p->setPen(QPen(m_font->color, (int)(2 * m_wmult)));
        p->drawText(m_displaypos.x() + on_pixmap.width() + 4,
                    m_displaypos.y() + 4,   // HACK!!!
                    m_area.right(),
                    m_area.bottom(),
                    Qt::AlignLeft,
                    current_data->getString());
    }
}

void UISelectorType::calculateScreenArea()
{
    QRect r = m_area;
    r.moveBy(m_parent->GetAreaRect().left(),
             m_parent->GetAreaRect().top());
    screen_area = r;
}

void UISelectorType::addItem(int an_int, const QString &a_string)
{
    IntStringPair *new_data = new IntStringPair(an_int, a_string);
    my_data.append(new_data);
    if(!current_data)
    {
        current_data = new_data;
    }
}

void UISelectorType::setToItem(int which_item)
{
    for(uint i = 0; i < my_data.count(); i++)
    {
        if(my_data.at(i)->getInt() == which_item)
        {
            current_data = my_data.at(i);
            refresh();
        }
    }
}

void UISelectorType::push(bool up_or_down)
{
    if(currently_pushed)
    {
        return;
    }
    currently_pushed = true;
    push_timer.start(300, TRUE);

    if(current_data)
    {
        my_data.find(current_data);
        if(up_or_down)
        {
            if(!(current_data = my_data.next()))
            {
                current_data = my_data.first();
            }
            
        }
        else
        {
            if(!(current_data = my_data.prev()))
            {
                current_data = my_data.last();
            }
        }
        emit pushed(current_data->getInt());
    }
    refresh();
}

void UISelectorType::unPush()
{
    currently_pushed = false;    
    refresh();
}

UISelectorType::~UISelectorType()
{
    my_data.clear();
}

// ********************************************************************

UIBlackHoleType::UIBlackHoleType(const QString &name)
                     : UIType(name)
{
}

void UIBlackHoleType::calculateScreenArea()
{
    QRect r = area;
    r.moveBy(m_parent->GetAreaRect().left(),
             m_parent->GetAreaRect().top());
    screen_area = r;
}

