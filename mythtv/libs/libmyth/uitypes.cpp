#include <qimage.h>
#include <math.h>

#include <iostream>
using namespace std;

#include "uitypes.h"

#include "mythcontext.h"

LayerSet::LayerSet(const QString &name)
{
    m_name = name;
    m_context = -1;
    m_debug = false;
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
}

UIType *LayerSet::GetType(const QString &name)
{
    UIType *ret = NULL;
    if (typeList.contains(name))
        ret = typeList[name];

    return ret;
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

// **************************************************************

UIType::UIType(const QString &name)
      : QObject(NULL, name)
{
    m_name = name;
    m_debug = false;
    m_context = -1;
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


// **************************************************************

UIFontPairType::UIFontPairType(fontProp *activeFont, fontProp *inactiveFont)
{
    m_activeFont = activeFont;
    m_inactiveFont = inactiveFont;
}

UIFontPairType::~UIFontPairType()
{
}

// ***************************************************************

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
        if (m_active == true)
            tmpfont = &m_fontfcns[m_fonts["active"]];
        else
            tmpfont = &m_fontfcns[m_fonts["inactive"]];
        bool lastShown = true;
        QPoint fontdrop = tmpfont->shadowOffset;

        dr->setFont(tmpfont->face);

        if (m_debug == true)
            cerr << "   +UIListType::Draw() <- within Layer\n";

        for (int i = 0; i < m_count; i++)
        {
            left = m_area.left();
            for (int j = 1; j <= m_columns; j++)
            {
                if (j > 1 && lastShown == true)
                    left = left + columnWidth[j - 1] + m_pad;

                if (m_debug == true)
                 {
                    cerr << "      -Column #" << j << ", Column Context: " << columnContext[j]
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
                            tempWrite = cutDown(tempWrite, &(tmpfont->face), columnWidth[j]);
                    }
                    else
                        tempWrite = "";

                    if (fontdrop.x() != 0 || fontdrop.y() != 0)
                    {
                        dr->setBrush(tmpfont->dropColor);
                        dr->setPen(QPen(tmpfont->dropColor, (int)(2 * m_wmult)));
                        dr->drawText((int)(left + fontdrop.x()),
                                     (int)(m_area.top() + (int)(i*m_selheight) + fontdrop.y()),
                                     m_area.width(), m_selheight, m_justification, tempWrite);
                    }
                    dr->setBrush(tmpfont->color);
                    dr->setPen(QPen(tmpfont->color, (int)(2 * m_wmult)));
                    if (forceFonts[i] != "")
                    {
                        dr->setFont(m_fontfcns[forceFonts[i]].face);
                        QColor myColor = m_fontfcns[forceFonts[i]].color;
                        dr->setBrush(myColor);
                        dr->setPen(QPen(myColor, (int)(2 * m_wmult)));
                    }
                    dr->drawText(left, m_area.top() + (int)(i*m_selheight),
                                 m_area.width(), m_selheight, m_justification, tempWrite);
                    dr->setFont(tmpfont->face);
                    if (m_debug == true)
                        cerr << "   +UIListType::Draw() Data: " << tempWrite << "\n";
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
        bool lastShown = true;
        QPoint fontdrop = tmpfont->shadowOffset;

        dr->setFont(tmpfont->face);
        dr->drawPixmap(m_area.left() + m_selection_loc.x(),
                         m_area.top() + m_selection_loc.y() + (int)(m_current * m_selheight),
                         m_selection);

        left = m_area.left();
        for (int j = 1; j <= m_columns; j++)
        {
             if (j > 1 && lastShown == true)
                    left = left + columnWidth[j - 1] + m_pad;

             if (m_debug == true)
             {
                 cerr << "      -Column #" << j << ", Column Context: " << columnContext[j]
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
                     tempWrite = cutDown(tempWrite, &(tmpfont->face), columnWidth[j]);
        
                 if (fontdrop.x() != 0 || fontdrop.y() != 0)
                 {
                     dr->setBrush(tmpfont->dropColor);
                     dr->setPen(QPen(tmpfont->dropColor, (int)(2 * m_wmult)));
                     dr->drawText((int)(left + fontdrop.x()),
                                  (int)(m_area.top() + (int)(i*m_selheight) + fontdrop.y()),
                                  m_area.width(), m_selheight, m_justification, tempWrite);
                 }
                 dr->setBrush(tmpfont->color);
                 dr->setPen(QPen(tmpfont->color, (int)(2 * m_wmult)));
                 if (forceFonts[i] != "")
                 {   
                     dr->setFont(m_fontfcns[forceFonts[i]].face);
                     QColor myColor = m_fontfcns[forceFonts[i]].color;
                     dr->setBrush(myColor);
                     dr->setPen(QPen(myColor, (int)(2 * m_wmult)));
                 }

                 dr->drawText(left, m_area.top() + (int)(i*m_selheight),
                              m_area.width(), m_selheight, m_justification, tempWrite);

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

QString UIListType::cutDown(QString info, QFont *testFont, int maxwidth)
{
    QFontMetrics fm(*testFont);

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
    return info;

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
    m_filename = filename;
    m_displaypos = displaypos;
    m_order = dorder;
    m_force_x = -1;
    m_force_y = -1;
    m_drop_x = 0;
    m_drop_y = 0;
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

    QString baseDir = gContext->GetInstallPrefix();
    QString themeDir = gContext->FindThemeDir("");
    themeDir = themeDir + gContext->GetSetting("Theme") + "/";
    baseDir = baseDir + "/share/mythtv/themes/default/";

    QFile checkFile(themeDir + m_filename);

    if (checkFile.exists())
        file = themeDir + m_filename;
    else
        file = baseDir + m_filename;
    checkFile.setName(file);
    if (!checkFile.exists())
        file = "/tmp/" + m_filename;

    if (m_debug == true)
        cerr << "     -Filename: " << file << endl;

    if (m_hmult == 1 && m_wmult == 1 && m_force_x == -1 && m_force_y == -1)
    {
        img.load(file);
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
            img.convertFromImage(scalerImg);
            if (m_debug == true)
                    cerr << "     -Image: " << file << " loaded.\n";
        }
        else
            if (m_debug == true)
                cerr << "     -Image: " << file << " failed to load.\n";
        delete sourceImg;
    }
}

void UIImageType::Draw(QPainter *dr, int drawlayer, int context)
{
  if (m_context == context || m_context == -1)
    if (drawlayer == m_order)
    {
        if (m_debug == true)
        {
            cerr << "   +UIImageType::Draw() <- inside Layer\n";
            cerr << "       -Drawing @ (" << m_displaypos.x() << ", " << m_displaypos.y() << ")" << endl;
            cerr << "       -Skip Section: (" << m_drop_x << ", " << m_drop_y << ")\n";
        }
        dr->drawPixmap(m_displaypos.x(), m_displaypos.y(), img, m_drop_x, m_drop_y);
    }
    else
        if (m_debug == true)
        {
            cerr << "   +UIImageType::Draw() <- outside (layer = " << drawlayer
                 << ", widget layer = " << m_order << "\n";
        }
}

// ******************************************************************

UITextType::UITextType(const QString &name, fontProp *font,
                       const QString &text, int dorder, QRect displayrect)
           : UIType(name)
{
    m_message = text;
    m_font = font;

    m_displaysize = displayrect;
    m_centered = false;
    m_right = false;
    m_order = dorder;
    m_justification = Qt::AlignLeft;
}

UITextType::~UITextType()
{
}

void UITextType::SetText(const QString &text)
{
    m_message = text;
}

void UITextType::Draw(QPainter *dr, int drawlayer, int context)
{
  if (m_context == context || m_context == -1)
    if (drawlayer == m_order)
    {
        QPoint fontdrop = m_font->shadowOffset;
        QString msg = m_message;
        msg = cutDown(msg, &(m_font->face), m_displaysize.width());

        dr->setFont(m_font->face);
        if (fontdrop.x() != 0 || fontdrop.y() != 0)
        {
            dr->setBrush(m_font->dropColor);
            dr->setPen(QPen(m_font->dropColor, (int)(2 * m_wmult)));
            dr->drawText((int)(m_displaysize.left() + fontdrop.x()),
                           (int)(m_displaysize.top() + fontdrop.y()),
                         m_displaysize.width(), m_displaysize.height(), m_justification, m_message);
        }
        dr->setBrush(m_font->color);
        dr->setPen(QPen(m_font->color, (int)(2 * m_wmult)));
        dr->drawText(m_displaysize.left(), m_displaysize.top(), 
                      m_displaysize.width(), m_displaysize.height(), m_justification, m_message);
        if (m_debug == true)
        {
            cerr << "   +UITextType::Draw() <- inside Layer\n";
            cerr << "       -Message: " << m_message << endl;
        }
    }
    else
        if (m_debug == true)
        {
             cerr << "   +UITextType::Draw() <- outside (layer = " << drawlayer
                  << ", widget layer = " << m_order << "\n";
        }
}

QString UITextType::cutDown(QString info, QFont *testFont, int maxwidth)
{
    QFontMetrics fm(*testFont);

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
    return info;

}

// ******************************************************************

UIStatusBarType::UIStatusBarType(QString &name, QPoint loc, int dorder)
               : UIType(name)
{
    m_location = loc;
    m_order = dorder;
}

UIStatusBarType::~UIStatusBarType()
{
}

void UIStatusBarType::Draw(QPainter *dr, int drawlayer, int context)
{
  if (m_context == context || m_context == -1)
    if (drawlayer == m_order)
    {
 	if (m_debug == true)
            cerr << "   +UIStatusBarType::Draw() <- within Layer\n";
     
        int width = (int)((double)((double)m_container.width() - (double)(2*m_fillerSpace))
                    * (double)((double)m_used / (double)m_total));

	if (m_debug == true)
            cerr << "       -Width = " << width << "\n";

        dr->drawPixmap(m_location.x(), m_location.y(), m_container);
        dr->drawPixmap(m_location.x(), m_location.y(), m_filler, 0, 0, width + m_fillerSpace);
    }
}

// ***************************************************************

GenericTree::GenericTree()
{
    init();
}

GenericTree::GenericTree(const QString a_string)
{
    init();
    my_string = a_string;
}

GenericTree::GenericTree(int an_int)
{
    init();
    my_int = an_int;
}

void GenericTree::init()
{
    my_parent = NULL;
    my_string = "";
    my_stringlist.clear();
    my_int = 0;
    my_subnodes.clear();
    //my_subnodes.setAutoDelete(true);
}

GenericTree* GenericTree::addNode(QString a_string)
{
    GenericTree *new_node = new GenericTree(a_string);
    new_node->setParent(this);
    my_subnodes.append(new_node);
    
    return new_node;
}

GenericTree* GenericTree::addNode(QString a_string, int an_int)
{
    GenericTree *new_node = new GenericTree(a_string);
    new_node->setInt(an_int);
    new_node->setParent(this);
    my_subnodes.append(new_node);
    
    return new_node;
}

GenericTree::~GenericTree()
{
}


// ***************************************************************

UIManagedTreeListType::UIManagedTreeListType(const QString & name)
                     : UIType(name)
{
    bins = 0;
    bin_corners.clear();
    my_tree_data = NULL;
}

UIManagedTreeListType::~UIManagedTreeListType()
{
    //
    //  Funny, I have yet to see this appear
    //  (even when it isn't commented out)
    //
    cout << "Rage, rage against the dying of the light" << endl;
}

void UIManagedTreeListType::Draw(QPainter *p, int drawlayer, int context)
{
    
    //
    //  This (obviously) doesn't do much yet.
    //  The idea is to use the data in my_tree_data
    //  to list in the available bins.
    //

    p->setPen(QColor(255,0,0));
    CornerMap::Iterator it;
    for ( it = bin_corners.begin(); it != bin_corners.end(); ++it )
    {
        p->drawRect(it.data());
    }
}

void UIManagedTreeListType::assignTreeData(GenericTree *a_tree)
{
    my_tree_data = a_tree;
}

void UIManagedTreeListType::sayHelloWorld()
{
    cout << "From a UIManagedTreeListType Object: Hello World" << endl ;
}

