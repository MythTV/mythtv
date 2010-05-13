// ANSI C headers
#include <cmath> // for ceilf

// C++ headers
#include <algorithm>
using namespace std;

// Qt headers
#include <QCoreApplication>
#include <QPainter>
#include <QLabel>

using namespace std;

#include "uitypes.h"
#include "mythdialogs.h"
#include "mythcontext.h"
#include "lcddevice.h"
#include "util.h"

#include "mythfontproperties.h"
#include "mythuihelper.h"
#include "x11colors.h"

#include "mythverbose.h"

#ifdef USING_MINGW
#undef LoadImage
#endif

LayerSet::LayerSet(const QString &name) :
    m_debug(false),
    m_context(-1),
    m_order(-1),
    m_name(name),
    numb_layers(-1),
    allTypes(new vector<UIType *>)
{
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
    if (a_number > numb_layers)
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

void LayerSet::DrawRegion(QPainter *dr, QRect &area, int drawlayer, int context)
{
  if (m_context == context || m_context == -1)
  {
    vector<UIType *>::iterator i = allTypes->begin();
    for (; i != allTypes->end(); i++)
    {
        if (m_debug == true)
            cerr << "-LayerSet::Draw\n";
        UIType *type = (*i);
        type->DrawRegion(dr, area, drawlayer, context);
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
            if (defText.isEmpty() || defText.contains('%'))
                item->SetText(QString(""));
        }
    }
}

void LayerSet::SetText(QHash<QString, QString> &infoMap)
{
    vector<UIType *>::iterator i = allTypes->begin();
    for (; i != allTypes->end(); i++)
    {
        UIType *type = (*i);
        if (UITextType *item = dynamic_cast<UITextType *>(type))
        {
            QHash<QString, QString>::Iterator riter = infoMap.begin();
            QString new_text = item->GetDefaultText();
            QString full_regex;

            if (new_text.isEmpty() && infoMap.contains(item->Name()))
            {
                new_text = infoMap[item->Name()];
            }
            else if (new_text.contains(QRegExp("%.*%")))
            {
                for (; riter != infoMap.end(); riter++)
                {
                    QString key = riter.key().toUpper();
                    QString data = *riter;

                    if (new_text.contains(key))
                    {
                        full_regex = QString("%") + key + QString("(\\|([^%|]*))?") +
                                     QString("(\\|([^%|]*))?") + QString("(\\|([^%]*))?%");
                        if (data.length())
                            new_text.replace(QRegExp(full_regex),
                                             QString("\\2") + data + QString("\\4"));
                        else
                            new_text.replace(QRegExp(full_regex), QString("\\6"));
                    }
                }
            }

            if (new_text.length())
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

void LayerSet::SetDrawFontShadow(bool state)
{
    vector<UIType *>::iterator i = allTypes->begin();
    for (; i != allTypes->end(); i++)
    {
        UIType *type = (*i);
        type->SetDrawFontShadow(state);
    }
}

// **************************************************************

UIType::UIType(const QString &name)
       : QObject(NULL)
{
    setObjectName(name);
    m_parent = NULL;
    m_name = name;
    m_debug = false;
    m_context = -1;
    m_order = -1;
    has_focus = false;
    takes_focus = false;
    screen_area = QRect(0,0,0,0);
    drawFontShadow = true;
    hidden = false;
}

void UIType::SetOrder(int order)
{
    m_order = order;
    if (m_parent)
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

void UIType::DrawRegion(QPainter *dr, QRect &area, int drawlayer, int context)
{
    (void)dr;
    (void)area;
    (void)drawlayer;
    (void)context;
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
    if (takes_focus)
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

void UIType::show()
{
    //
    //  In this base class, this is pretty simple. The method is virtual
    //  though, and inherited fancy classes like UIListBttn can/will/should
    //  set a timer and fade things on a show()
    //

    hidden = false;
    refresh();
}

void UIType::hide()
{
    hidden = true;
    refresh();
}

bool UIType::toggleShow()
{
    if (hidden)
    {
        show();
    }
    else
    {
        hide();
    }

    return !hidden;
}

QString UIType::cutDown(const QString &data, QFont *testFont, bool multiline,
                        int overload_width, int overload_height)
{
    int length = data.length();
    if (length == 0)
        return data;

    int maxwidth = screen_area.width();
    if (overload_width != -1)
        maxwidth = overload_width;
    int maxheight = screen_area.height();
    if (overload_height != -1)
        maxheight = overload_height;

    int justification = Qt::AlignLeft | Qt::TextWordWrap;
    QFontMetrics fm(*testFont);

    int margin = length - 1;
    int index = 0;
    int diff = 0;

    while (margin > 0)
    {
        if (multiline)
            diff = maxheight - fm.boundingRect(0, 0, maxwidth, maxheight,
                                               justification,
                                               data.left(index + margin)
                                               ).height();
        else
            diff = maxwidth - fm.width(data, index + margin);
        if (diff >= 0)
            index += margin;

        margin /= 2;

        if (index + margin >= length - 1)
            margin = (length - 1) - index;
    }

    if (index < length - 1)
    {
        QString tmpStr(data);
        tmpStr.truncate(index);
        if (index >= 3)
            tmpStr.replace(index - 3, 3, "...");
        return tmpStr;
    }

    return data;
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
    m_showSelAlways = true;
    has_focus = false;
    takes_focus = true;
}

UIListType::~UIListType()
{
}

void UIListType::Draw(QPainter *dr, int drawlayer, int context)
{
  if (hidden)
     return;

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
        int tempArrows = 0;

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
                int offsetRight = 0;
                int offsetLeft = 0;
                int caw = columnWidth[j];

                if (caw == 0)
                    caw = m_area.width();

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
                    tempArrows = listArrows[i + 100];

                    if (j == 1)
                    {
                        offsetLeft = m_leftarrow.width() + m_leftarrow_loc.x();
                        if (tempArrows & ARROW_LEFT)
                            dr->drawPixmap(left + m_leftarrow_loc.x(),
                                       m_area.top() + (int)(i * m_selheight) + m_leftarrow_loc.y(),
                                       m_leftarrow);

                    }

                    if (j == m_columns)
                    {
                        offsetRight = m_rightarrow.width()  - m_rightarrow_loc.x();
                        if (tempArrows & ARROW_RIGHT)
                            dr->drawPixmap(m_area.right() - offsetRight + m_rightarrow_loc.x(),
                                       m_area.top() + (int)(i * m_selheight) + m_rightarrow_loc.y(),
                                       m_rightarrow);
                    }

                    if (tempWrite != "***FILLER***")
                    {
                        if (columnWidth[j] > 0)
                            tempWrite = cutDown(tempWrite, &(tmpfont->face),
                                                false, columnWidth[j] - offsetRight - offsetLeft);
                    }
                    else
                        tempWrite = "";


                    if (drawFontShadow &&
                        (fontdrop.x() != 0 || fontdrop.y() != 0))
                    {
                        dr->setBrush(tmpfont->dropColor);
                        dr->setPen(QPen(tmpfont->dropColor,
                                        (int)(2 * m_wmult)));
                        dr->drawText((int)(left + fontdrop.x()) + offsetLeft,
                                     (int)(m_area.top() + (int)(i*m_selheight) +
                                     fontdrop.y()), caw - offsetRight, m_selheight,
                                     m_justification, tempWrite);
                    }
                    dr->setBrush(tmpfont->color);
                    dr->setPen(QPen(tmpfont->color, (int)(2 * m_wmult)));

                    dr->drawText(left + offsetLeft, m_area.top() + (int)(i*m_selheight),
                                 caw - offsetRight, m_selheight, m_justification,
                                 tempWrite);
                    dr->setFont(tmpfont->face);
                    if (m_debug == true)
                        cerr << "   +UIListType::Draw() Data: "
                             << tempWrite.toLocal8Bit().constData() << "\n";
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
        int tempArrows;

        if (m_active == true)
            tmpfont = &m_fontfcns[m_fonts["selected"]];
        else
            tmpfont = &m_fontfcns[m_fonts["inactive"]];

        if (forceFonts[i] != "")
            tmpfont = &m_fontfcns[forceFonts[i]];

        bool lastShown = true;
        QPoint fontdrop = tmpfont->shadowOffset;

        dr->setFont(tmpfont->face);

        if (m_active == true || m_showSelAlways)
            dr->drawPixmap(m_area.left() + m_selection_loc.x(),
                           m_area.top() + m_selection_loc.y() +
                           (int)(m_current * m_selheight),
                           m_selection);

        left = m_area.left();
        for (int j = 1; j <= m_columns; j++)
        {
            int offsetRight = 0;
            int offsetLeft = 0;
            int caw = columnWidth[j];

            if (caw == 0)
                caw = m_area.width();

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
                tempArrows = listArrows[i + 100];

                if (j == 1)
                {
                    offsetLeft = m_leftarrow.width() + m_leftarrow_loc.x();
                    if (tempArrows & ARROW_LEFT)
                    {
                        dr->drawPixmap(left + m_leftarrow_loc.x(),
                                       m_area.top() + (int)(i * m_selheight) + m_leftarrow_loc.y(),
                                       m_leftarrow);
                    }
                }

                if (j == m_columns)
                {
                    offsetRight = m_rightarrow.width() - m_rightarrow_loc.x();
                    if (tempArrows & ARROW_RIGHT)
                    {
                        dr->drawPixmap(m_area.right() - offsetRight + m_rightarrow_loc.x(),
                                       m_area.top() + (int)(i * m_selheight) + m_rightarrow_loc.y(),
                                       m_rightarrow);
                    }
                }

                 if (columnWidth[j] > 0)
                     tempWrite = cutDown(tempWrite, &(tmpfont->face), false,
                                         columnWidth[j] - offsetLeft - offsetRight);

                 if (drawFontShadow &&
                     (fontdrop.x() != 0 || fontdrop.y() != 0))
                 {
                     dr->setBrush(tmpfont->dropColor);
                     dr->setPen(QPen(tmpfont->dropColor, (int)(2 * m_wmult)));
                     dr->drawText((int)(left + fontdrop.x()) + offsetLeft,
                                  (int)(m_area.top() + (int)(i*m_selheight) +
                                  fontdrop.y()), caw - offsetRight, m_selheight,
                                  m_justification, tempWrite);
                 }
                 dr->setBrush(tmpfont->color);
                 dr->setPen(QPen(tmpfont->color, (int)(2 * m_wmult)));

                 dr->drawText(left + offsetLeft, m_area.top() + (int)(i*m_selheight),
                              caw - offsetRight, m_selheight, m_justification,
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

void UIListType::SetItemArrow(int num, int which)
{
    m_columns = 1;
    listArrows[num + 100] = which;
}

void UIListType::calculateScreenArea()
{
    QRect r2, r = m_area;

    // take the selection image position into account
    r2.setRect(r.x() + m_selection_loc.x(), r.y() + m_selection_loc.y(),
               m_selection.width(), m_selection.height());
    r = r.unite(r2);

    // take the up arrow image position into account
    r2.setRect(m_uparrow_loc.x(), m_uparrow_loc.y(),
              m_uparrow.width(), m_uparrow.height());
    r = r.unite(r2);

    // take the down arrow image position into account
    r2.setRect(m_downarrow_loc.x(), m_downarrow_loc.y(),
               m_downarrow.width(), m_downarrow.height());
    r = r.unite(r2);

    r.translate(m_parent->GetAreaRect().left(),
                m_parent->GetAreaRect().top());
    screen_area = r;
}


bool UIListType::takeFocus()
{
    SetActive(true);
    return UIType::takeFocus();
}

void UIListType::looseFocus()
{
    SetActive(false);
    UIType::looseFocus();
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
    m_transparent = gCoreContext->GetNumSetting("PlayBoxTransparency", 1);
}

UIImageType::~UIImageType()
{
}

void UIImageType::LoadImage()
{
    if (m_filename == "none")
    {
        m_show = false;
        return;
    }

    QString file;
    if (m_flex == true)
    {
        QString flexprefix = m_transparent ? "trans-" : "solid-";
        int pathStart = m_filename.lastIndexOf('/');
        if (pathStart < 0 )
            m_filename = flexprefix + m_filename;
        else
            m_filename.replace(pathStart, 1, "/" + flexprefix);
    }

    QString filename = GetMythUI()->GetThemeDir() + m_filename;

    if (m_force_x == -1 && m_force_y == -1)
    {
        QPixmap *tmppix = GetMythUI()->LoadScalePixmap(filename);
        if (tmppix)
        {
            img = *tmppix;
            m_show = true;

            delete tmppix;
            refresh();

            return;
        }
    }

    file = m_filename;

    if (!GetMythUI()->FindThemeFile(file))
    {
        VERBOSE(VB_IMPORTANT, "UIImageType::LoadImage() - Cannot find image: "
                << m_filename);
        m_show = false;
        return;
    }

    if (m_debug == true)
        VERBOSE(VB_GENERAL, "     -Filename: " << file);

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
                    VERBOSE(VB_GENERAL, "         +Force X: " << doX);
            }
            if (m_force_y != -1)
            {
                doY = m_force_y;
                if (m_debug == true)
                    VERBOSE(VB_GENERAL, "         +Force Y: " << doY);
            }

            scalerImg = sourceImg->scaled((int)(doX * m_wmult),
                                            (int)(doY * m_hmult),
                                            Qt::IgnoreAspectRatio,
                                            Qt::SmoothTransformation);
            m_show = true;
            img = QPixmap::fromImage(scalerImg);
            if (m_debug == true)
                VERBOSE(VB_GENERAL, "     -Image: " << file << " loaded.");
        }
        else
        {
            m_show = false;
            if (m_debug == true)
                VERBOSE(VB_GENERAL, "     -Image: " << file << " failed to load.");
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
    if (hidden)
    {
        return;
    }
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
                dr->drawPixmap(m_displaypos.x(), m_displaypos.y(), img, m_drop_x, m_drop_y, -1, -1);
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

    if (m_parent)
    {
        r.translate(m_parent->GetAreaRect().left(),
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
    m_highest_repeat = 1;
    m_orientation = 0;
}

void UIRepeatedImageType::Draw(QPainter *p, int drawlayer, int context)
{
    if (hidden)
        return;

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
                if (m_orientation == 0)
                {
                    for(int i = 0; i < m_repeat; i++)
                    {
                        p->drawPixmap(m_displaypos.x() + (i * img.width()), m_displaypos.y(), img, m_drop_x, m_drop_y, -1, -1);
                    }
                }
                else if (m_orientation == 1)
                {
                    for(int i = 0; i < m_repeat; i++)
                    {
                        p->drawPixmap(m_displaypos.x() - (i * img.width()), m_displaypos.y(), img, m_drop_x, m_drop_y, -1, -1);
                    }
                }
                else if (m_orientation == 2)
                {
                    for(int i = 0; i < m_repeat; i++)
                    {
                        p->drawPixmap(m_displaypos.x(),  m_displaypos.y() - (i * img.height()), img, m_drop_x, m_drop_y, -1, -1);
                    }
                }
                else if (m_orientation == 3)
                {
                    for(int i = 0; i < m_repeat; i++)
                    {
                        p->drawPixmap(m_displaypos.x(),  m_displaypos.y() + (i * img.height()), img, m_drop_x, m_drop_y, -1, -1);
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
    if (how_many >= 0)
    {
        m_repeat = how_many;
        if (how_many > m_highest_repeat)
        {
            m_highest_repeat = how_many;
        }
        refresh();
    }
}

void UIRepeatedImageType::setOrientation(int x)
{
    if (x < 0 || x > 3)
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

    if (m_orientation == 0)
    {
        r = QRect(m_displaypos.x(),
                  m_displaypos.y(),
                  img.width() * m_highest_repeat,
                  img.height());
    }
    else if (m_orientation == 1)
    {
        r = QRect(m_displaypos.x() - (m_highest_repeat * img.width()),
                  m_displaypos.y(),
                  img.width() * (m_highest_repeat + 1),
                  img.height());
    }
    else if (m_orientation == 2)
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
    if (m_parent)
    {
        r.translate(m_parent->GetAreaRect().left(),
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

// **************************************************************

UIImageGridType::UIImageGridType(const QString &name, int order)
    : UIType(name)
{
    m_name = name;
    m_order = order;

    activeFont = inactiveFont = selectedFont = NULL;
    window = NULL;
    defaultPixmap = normalPixmap = selectedPixmap = highlightedPixmap = NULL;

    topRow = rowCount = columnCount = itemCount = currentItem =
            borderWidth = padding = cellWidth = cellHeight =
            lastRow = lastColumn = curColumn = curRow = 0;

    textPos = UIImageGridType::textPosBottom;
    textHeight = 20;
    cutdown = true;
    setJustification((Qt::AlignLeft | Qt::AlignVCenter));
    allowFocus(true);
    showCheck = false;
    showSelected = false;

    allData = new QList<ImageGridItem*>;
}

UIImageGridType::~UIImageGridType(void)
{
    if (normalPixmap)
        delete normalPixmap;
    if (highlightedPixmap)
        delete highlightedPixmap;
    if (selectedPixmap)
        delete selectedPixmap;
    if (defaultPixmap)
        delete defaultPixmap;

    if (checkNonPixmap)
        delete checkNonPixmap;
    if (checkHalfPixmap)
        delete checkHalfPixmap;
    if (checkFullPixmap)
        delete checkFullPixmap;

    if (upArrowRegPixmap)
        delete upArrowRegPixmap;
    if (upArrowActPixmap)
        delete upArrowActPixmap;
    if (dnArrowRegPixmap)
        delete dnArrowRegPixmap;
    if (upArrowActPixmap)
        delete dnArrowActPixmap;

    reset();
    delete allData;
}

void UIImageGridType::reset(void)
{
    while (!allData->empty())
    {
        delete allData->back();
        allData->pop_back();
    }
    topRow = itemCount = currentItem = lastRow =
            lastColumn = curColumn = curRow = 0;
}

void  UIImageGridType::setCurrentPos(int pos)
{
    if (pos < 0 || pos > (int) allData->size() - 1)
        return;

    currentItem = pos;

    // make sure the selected item is visible
    if ((currentItem < topRow * columnCount) ||
            (currentItem >= (topRow + rowCount) * columnCount))
    {
        topRow = std::max(std::min(currentItem / columnCount,
                                   lastRow - rowCount + 1), 0);
        curRow = topRow;
    }

    curColumn = currentItem % columnCount;
    refresh();
}

void  UIImageGridType::setCurrentPos(QString value)
{
    for (uint i = 0; i < (uint)allData->size(); i++)
    {
        if ((*allData)[i]->text == value)
        {
            setCurrentPos(i);
            return;
        }
    }
}

ImageGridItem *UIImageGridType::getCurrentItem(void)
{
    return getItemAt(currentItem);
}

ImageGridItem *UIImageGridType::getItemAt(int pos)
{
    if (pos < 0 || pos > (int) allData->size() - 1)
        return NULL;

    return (*allData)[pos];
}

void UIImageGridType::appendItem(ImageGridItem *item)
{
    allData->append(item);
    itemCount = allData->size();
}

void UIImageGridType::updateItem(ImageGridItem *item)
{
    updateItem(allData->indexOf(item), item);
}

void UIImageGridType::updateItem(int itemNo, ImageGridItem *item)
{
    if (itemNo < 0 || itemNo > (int) allData->size() - 1)
        return;

    ImageGridItem *gridItem = (*allData)[itemNo];

    if (gridItem)
    {
        gridItem = item;
    }

    // if this item is visible update the imagegrid
    if ((itemNo >= topRow * columnCount) && (itemNo < (topRow + rowCount) * columnCount))
        refresh();
}

void UIImageGridType::removeItem(ImageGridItem *item)
{
    removeItem(allData->indexOf(item));
}

void UIImageGridType::removeItem(int itemNo)
{
    if (itemNo < 0 || itemNo > (int) allData->size() - 1)
        return;

    delete (*allData)[itemNo];
    allData->removeAt(itemNo);

    itemCount--;
    lastRow = std::max((int) ceilf((float) itemCount/columnCount) - 1, 0);
    lastColumn = std::max(itemCount - lastRow * columnCount - 1, 0);

    // make sure the selected item is still visible
    if (topRow + rowCount > lastRow)
        topRow = std::max(std::min(currentItem / columnCount,
                                   lastRow - rowCount + 1), 0);

    if (curRow > lastRow)
        curRow = topRow;

    refresh();
}

void UIImageGridType::setJustification(int jst)
{
    justification = jst;
    multilineText = (justification & Qt::TextWordWrap) > 0;
}

bool UIImageGridType::handleKeyPress(QString action)
{
    if (!has_focus)
        return false;

    if (action == "LEFT")
    {
        if (curRow == 0 && curColumn == 0)
            return true;

        curColumn--;
        if (curColumn < 0)
        {
            curColumn = columnCount - 1;
            curRow--;
            if (curRow < topRow)
                topRow = curRow;
        }
    }
    else if (action == "RIGHT")
    {
        if ((curRow * columnCount + curColumn) >= itemCount - 1)
            return true;

        curColumn++;
        if (curColumn >= columnCount)
        {
            curColumn = 0;
            curRow++;
            if (curRow >= topRow + rowCount)
                topRow++;
        }
    }
    else if (action == "UP")
    {
        if (curRow == 0)
        {
            curRow = lastRow;
            curColumn = std::min(curColumn, lastColumn);
            topRow  = std::max(curRow - rowCount + 1,0);
        }
        else
        {
            curRow--;
            if (curRow < topRow)
                topRow = curRow;
        }
    }
    else if (action == "DOWN")
    {
        if (curRow == lastRow)
        {
            curRow = 0;
            topRow = 0;
        }
        else
        {
            curRow++;

            if (curRow == lastRow)
                curColumn = std::min(curColumn, lastColumn);

            if (curRow >= topRow + rowCount)
                topRow++;
        }
    }
    else if (action == "PAGEUP")
    {
        if (curRow == 0)
            return true;
        else
            curRow = std::max(curRow - rowCount, 0);

        topRow = curRow;
    }
    else if (action == "PAGEDOWN")
    {
        if (curRow == lastRow)
            return true;
        else
            curRow += rowCount;

        if (curRow >= lastRow)
        {
            curRow = lastRow;
            curColumn = std::min(curColumn, lastColumn);
        }

        topRow = std::max(curRow - rowCount + 1,0);
    }
    else if (action == "SELECT" && showSelected)
    {
        ImageGridItem *item = NULL;
        if (currentItem <  (*allData).size())
            item = (*allData)[currentItem];
        if (item)
            item->selected = !item->selected;
    }
    else
        return false;


    currentItem = curRow * columnCount + curColumn;

    showUpArrow = (topRow != 0);
    showDnArrow = (topRow + rowCount <= lastRow);

    refresh();

    if (currentItem <  (*allData).size())
        emit itemChanged((*allData)[currentItem]);

    return true;
}

void UIImageGridType::Draw(QPainter *p, int drawlayer, int context)
{
    if (hidden)
        return;

    if ((m_context != context && m_context != -1) || drawlayer != m_order)
        return;

    // redraw the complete view rectangle
    QRect pr = displayRect;

    if (m_debug == true)
    {
        p->setPen(Qt::red);
        p->drawRect(QRect(displayRect.x(), displayRect.y(),
                    displayRect.width(), displayRect.height()));
    }

    int curPos = topRow * columnCount;

    for (int y = 0; y < rowCount; y++)
    {
        int ypos = displayRect.y() + y * (padding + cellHeight);

        for (int x = 0; x < columnCount; x++)
        {
            if (curPos >= itemCount)
                continue;

            int xpos = displayRect.x() + x * (padding + cellWidth);
            drawCell(p, curPos, xpos, ypos);

            curPos++;
        }
    }

    if (showScrollArrows)
    {
        if (showUpArrow)
            p->drawPixmap(displayRect.x(),
                          displayRect.bottom() - upArrowActPixmap->height(),
                          *upArrowActPixmap);
        else
            p->drawPixmap(displayRect.x(),
                          displayRect.bottom() - upArrowRegPixmap->height(),
                          *upArrowRegPixmap);
        if (showDnArrow)
            p->drawPixmap(displayRect.x() + upArrowRegPixmap->width() +
                    (int)(5 * m_wmult),
        displayRect.bottom() - dnArrowActPixmap->height(),
        *dnArrowActPixmap);
        else
            p->drawPixmap(displayRect.x() + upArrowRegPixmap->width() +
                    (int)(5 * m_wmult),
        displayRect.bottom() - dnArrowRegPixmap->height(),
        *dnArrowRegPixmap);
    }
}

void UIImageGridType::drawCell(QPainter *p, int curPos, int xpos, int ypos)
{
    QRect r(xpos, ypos, cellWidth, cellHeight);

    if (curPos == currentItem)
    {
        // highlighted
        if (m_debug == true)
            p->setPen(Qt::yellow);
        if (highlightedPixmap)
            p->drawPixmap(xpos, ypos, *highlightedPixmap);
    }
    else
    {
        if (m_debug == true)
            p->setPen(Qt::green);
        if (normalPixmap)
            p->drawPixmap(xpos, ypos, *normalPixmap);
    }

    // draw item image
    QPixmap *pixmap = NULL;
    QString filename = "";
    ImageGridItem *item = NULL;

    if (curPos <  (*allData).size())
        item = (*allData)[curPos];

    // use pixmap stored in item
    if (item)
        pixmap = item->pixmap;

    // use default pixmap if non found
    if (!pixmap)
        pixmap = defaultPixmap;

    if (pixmap && !pixmap->isNull())
        p->drawPixmap(xpos + imageRect.x() + ( (imageRect.width() - pixmap->width()) / 2 ),
                      ypos + imageRect.y() + ( (imageRect.height() - pixmap->height()) / 2 ),
                      *pixmap, 0, 0, -1, -1);

    if (m_debug == true)
    {
      p->setBrush(Qt::NoBrush);
      p->drawRect(r);
    }

    // draw text area
    drawText(p, curPos, xpos, ypos);
}

void UIImageGridType::drawText(QPainter *p, int curPos, int xpos, int ypos)
{
    QRect textRect(xpos, ypos, cellWidth, textHeight);
    if (textPos == UIImageGridType::textPosBottom)
        textRect.moveTop(ypos + cellHeight - textHeight);

    if (m_debug == true)
    {
        p->setBrush(Qt::NoBrush);
        p->setPen(Qt::blue);
        p->drawRect(textRect);
    }

    QString msg = "Invalid Item!!";
    ImageGridItem * item = NULL;

    if (curPos <  (*allData).size())
        item = (*allData)[curPos];

    if (item)
    {
        msg = item->text;

        if (showCheck)
        {
            QRect cr(checkRect);
            cr.translate(textRect.x(), textRect.y());
            if (item->selected)
                p->drawPixmap(cr, *checkFullPixmap);
            else
                p->drawPixmap(cr, *checkNonPixmap);

            textRect.setX(textRect.x() + cr.width() + (int)(5 * m_wmult));
        }
    }

    if (m_debug == true)
    {
        p->setBrush(Qt::NoBrush);
        p->setPen(Qt::blue);
        p->drawRect(textRect);
    }

    fontProp *font = has_focus ? activeFont : inactiveFont;

    if (item && item->selected && showSelected)
        font = selectedFont;

    if (cutdown)
        msg = cutDown(msg, &font->face, multilineText,
                      textRect.width(), textRect.height());

    p->setFont(font->face);

    if (font->shadowOffset.x() != 0 || font->shadowOffset.y() != 0)
    {
        p->setBrush(font->dropColor);
        p->setPen(QPen(font->dropColor, (int)(2 * m_wmult)));
        p->drawText(textRect.left() + font->shadowOffset.x(),
                    textRect.top() + font->shadowOffset.y(),
                    textRect.width(), textRect.height(), justification, msg);
    }

    p->setBrush(font->color);
    p->setPen(QPen(font->color, (int)(2 * m_wmult)));
    p->drawText(textRect, justification, msg);
}

void UIImageGridType::recalculateLayout(void)
{
    loadImages();

    int arrowHeight = 0;
    if (showScrollArrows)
        arrowHeight = upArrowRegPixmap->height() + (int)(5 * m_hmult);

    cellWidth = (displayRect.width() - (padding * (columnCount -1))) / columnCount;
    cellHeight = (displayRect.height() - arrowHeight -
            (padding * (rowCount -1))) / rowCount;
    lastRow = std::max((int) ceilf((float) itemCount/columnCount) - 1, 0);
    lastColumn = std::max(itemCount - lastRow * columnCount - 1, 0);


    // calc image item bounding rect
    int yoffset = 0;
    int bw = cellWidth;
    int bh = cellHeight - textHeight;
    int sw = (int) (7 * m_wmult);
    int sh = (int) (7 * m_hmult);

    imageRect.setX(sw);
    imageRect.setY(sh + yoffset);
    imageRect.setWidth((int) (bw - 2 * sw));
    imageRect.setHeight((int) (bh - 2 * sw));

    // centre check pixmap in text area
    int cw = checkFullPixmap->width();
    int ch = checkFullPixmap->height();
    checkRect = QRect(0, (textHeight - ch) / 2, cw, ch);

    loadCellImages();
}

QPixmap *UIImageGridType::createScaledPixmap(QString filename,
                                             int width, int height, Qt::AspectRatioMode mode)
{
    QPixmap *pixmap = NULL;

    if (filename != "")
    {
        QImage *img = GetMythUI()->LoadScaleImage(filename);
        if (!img)
        {
            cout << "Failed to load image"
                 << filename.toAscii().constData() << endl;
            return NULL;
        }
        else
        {
            img->scaled(width, height, mode,
                        Qt::SmoothTransformation);
            pixmap = new QPixmap(QPixmap::fromImage(*img));
            delete img;
        }
    }

    return pixmap;
}

void UIImageGridType::loadImages(void)
{
    MythUIHelper *ui = GetMythUI();
    checkNonPixmap = ui->LoadScalePixmap("lb-check-empty.png");
    checkHalfPixmap = ui->LoadScalePixmap("lb-check-half.png");
    checkFullPixmap = ui->LoadScalePixmap("lb-check-full.png");
    upArrowRegPixmap = ui->LoadScalePixmap("lb-uparrow-reg.png");
    upArrowActPixmap = ui->LoadScalePixmap("lb-uparrow-sel.png");
    dnArrowRegPixmap = ui->LoadScalePixmap("lb-dnarrow-reg.png");
    dnArrowActPixmap = ui->LoadScalePixmap("lb-dnarrow-sel.png");
}

void UIImageGridType::loadCellImages(void)
{
    int imgHeight = cellHeight - textHeight;
    int imgWidth = cellWidth;
    int sw = (int) (7 * m_wmult);
    int sh = (int) (7 * m_hmult);

    normalPixmap = createScaledPixmap(normalImage, imgWidth, imgHeight,
                                      Qt::IgnoreAspectRatio);
    highlightedPixmap = createScaledPixmap(highlightedImage, imgWidth, imgHeight,
                                           Qt::IgnoreAspectRatio);
    selectedPixmap = createScaledPixmap(selectedImage, imgWidth, imgHeight,
                                        Qt::IgnoreAspectRatio);
    defaultPixmap = createScaledPixmap(defaultImage, imgWidth - 2 * sw,
                                       imgHeight - 2 * sh,
                                       Qt::KeepAspectRatio);
}

void UIImageGridType::calculateScreenArea(void)
{
    QRect r = displayRect;
    r.translate(m_parent->GetAreaRect().left(),
                m_parent->GetAreaRect().top());
    screen_area = r;
}

QSize UIImageGridType::getImageItemSize(void)
{
    return QSize(imageRect.width(), imageRect.height());
}

// ******************************************************************

UITextType::UITextType(const QString &name, fontProp *font,
                       const QString &text, int dorder, QRect displayrect,
                       QRect altdisplayrect)
           : UIType(name)
{

    m_name = name;
    if (text.length())
        m_message = text;
    else
        m_message = " ";

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

void UITextType::SetFont(fontProp *font)
{
    m_font = font;
    refresh();
}

void UITextType::Draw(QPainter *dr, int drawlayer, int context)
{
    if (hidden)
    {
        return;
    }
    if (m_context == context || m_context == -1)
    {
        if (drawlayer == m_order)
        {
            bool m_multi = false;
            if ((m_justification & Qt::TextWordWrap) > 0)
                m_multi = true;
            QPoint fontdrop = m_font->shadowOffset;
            QString msg = m_message;
            dr->setFont(m_font->face);
            if (m_cutdown)
            {
                msg = cutDown(msg, &(m_font->face), m_multi,
                              m_displaysize.width(), m_displaysize.height());
            }
            if (m_cutdown && m_debug)
                cerr << "    +UITextType::CutDown Called.\n";

            if (drawFontShadow && (fontdrop.x() != 0 || fontdrop.y() != 0))
            {
                if (m_debug)
                {
                    cerr << "    +UITextType::Drawing shadow @ ("
                         << (int)(m_displaysize.left() + fontdrop.x()) << ", "
                         << (int)(m_displaysize.top() + fontdrop.y())
                         << ")" << endl;
                }

                dr->setBrush(m_font->dropColor);
                dr->setPen(QPen(m_font->dropColor, (int)(2 * m_wmult)));
                dr->drawText((int)(m_displaysize.left() + fontdrop.x()),
                             (int)(m_displaysize.top() + fontdrop.y()),
                             m_displaysize.width(),
                             m_displaysize.height(), m_justification, msg);
            }

            dr->setBrush(m_font->color);
            dr->setPen(QPen(m_font->color, (int)(2 * m_wmult)));

            if (m_debug)
            {
                cerr << "    +UITextType::Drawing @ ("
                     << (int)(m_displaysize.left()) << ", "
                     << (int)(m_displaysize.top()) << ")" << endl;
            }

            dr->drawText(m_displaysize.left(), m_displaysize.top(),
                         m_displaysize.width(), m_displaysize.height(),
                         m_justification, msg);

            if (m_debug)
            {
                cerr << "   +UITextType::Draw() <- inside Layer\n";
                cerr << "       -Message: "
                     << m_message.toLocal8Bit().constData()
                     << " (cut: "
                     << msg.toLocal8Bit().constData() << ")" <<  endl;
            }
        }
        else if (m_debug)
        {
            cerr << "   +UITextType::Draw() <- outside (layer = " << drawlayer
                 << ", widget layer = " << m_order << "\n";
        }
    }
}

void UITextType::calculateScreenArea()
{
    QRect r = m_displaysize;
    r.translate(m_parent->GetAreaRect().left(),
                m_parent->GetAreaRect().top());
    screen_area = r;
}

// ******************************************************************

UIRemoteEditType::UIRemoteEditType(const QString &name, fontProp *font,
                       const QString &text, int dorder, QRect displayrect)
           : UIType(name)
{
    m_font = font;
    m_text = text;
    m_displaysize = displayrect;
    m_order = dorder;
    edit = NULL;
    takes_focus = true;
}

void UIRemoteEditType::createEdit(MythThemedDialog* parent)
{
    m_parentDialog = parent;
    edit = new MythRemoteLineEdit(parent);

    edit->setFocusPolicy( Qt::NoFocus );
    edit->setText(m_text);
    edit->setCurrentFont(m_font->face);
    edit->setMinimumHeight(getScreenArea().height());
    edit->setMaximumHeight(getScreenArea().height());
    edit->setGeometry(getScreenArea());
    edit->setCharacterColors(m_unselected, m_selected, m_special);

    connect(edit, SIGNAL(tryingToLooseFocus(bool)),
            this, SLOT(takeFocusAwayFromEditor(bool)));
    connect(edit, SIGNAL(textChanged(QString)),
            this, SLOT(editorChanged(QString)));

    edit->show();
}

UIRemoteEditType::~UIRemoteEditType()
{
    if (edit)
    {
        edit->hide();
        edit->deleteLater();
        edit = NULL;
    }
}

void UIRemoteEditType::setText(const QString text)
{
    m_text = text;
    if (edit)
        edit->setText(text);
    //refresh();
}

QString UIRemoteEditType::getText()
{
    if (edit)
        return edit->text();
    else
        return QString::null;
}

void  UIRemoteEditType::setFont(fontProp *font)
{
    m_font = font;
    if (edit)
        edit->setCurrentFont(font->face);
}

void UIRemoteEditType::setCharacterColors(QColor unselected, QColor selected, QColor special)
{
    m_unselected = unselected;
    m_selected = selected;
    m_special = special;

    if (edit)
        edit->setCharacterColors(unselected, selected, special);
}

void UIRemoteEditType::show(void)
{
    if (edit)
        edit->show();

    UIType::show();
}

void UIRemoteEditType::hide(void)
{
    if (edit)
        edit->hide();

    UIType::hide();
}

void UIRemoteEditType::Draw(QPainter *dr, int drawlayer, int context)
{
    if (hidden)
    {
        if (edit && edit->isVisible())
            edit->hide();

        return;
    }

    if (m_context == context || m_context == -1)
    {
        if (drawlayer == m_order)
        {
            if (edit && !edit->isVisible())
                edit->show();

            dr = dr;
        }
    }
    else
    {
        // not in this context so hide the edit
        if (edit && edit->isVisible())
            edit->hide();
    }
}

void UIRemoteEditType::calculateScreenArea()
{
    QRect r = m_displaysize;
    r.translate(m_parent->GetAreaRect().left(),
                m_parent->GetAreaRect().top());
    screen_area = r;
}

void UIRemoteEditType::takeFocusAwayFromEditor(bool up_or_down)
{
    if (m_parentDialog)
        m_parentDialog->nextPrevWidgetFocus(up_or_down);

    looseFocus();
}

bool UIRemoteEditType::takeFocus()
{
    if (edit)
    {
        QTextCursor tmp = edit->textCursor();
        tmp.movePosition(QTextCursor::End);
        edit->setTextCursor(tmp);
        edit->setFocus();
    }

    return UIType::takeFocus();
}

void UIRemoteEditType::looseFocus()
{
    if (edit)
    {
        edit->clearFocus();
    }

    UIType::looseFocus();
}

void UIRemoteEditType::editorChanged(QString value)
{
    emit textChanged(value);
}

// ******************************************************************

UIStatusBarType::UIStatusBarType(QString &name, QPoint loc, int dorder)
               : UIType(name)
{
    m_location = loc;
    m_order = dorder;
    m_orientation = 0;
    m_used = 0;
    m_total = 100;
}

UIStatusBarType::~UIStatusBarType()
{
}

void UIStatusBarType::Draw(QPainter *dr, int drawlayer, int context)
{
    if (hidden)
        return;

    if (m_context == context || m_context == -1)
    {
        if (drawlayer == m_order)
        {
            if (m_debug == true)
            {
                cerr << "   +UIStatusBarType::Draw() <- within Layer\n";
            }
            
            // Width or height of 0 will draw the full image, not what we want for 0%
            if (m_used < 1)
                m_used = 1;
                
            int width = (int)((double)((double)m_container.width() - (double)(2*m_fillerSpace))
                    * (double)((double)m_used / (double)m_total));

            int height = (int)((double)((double)m_container.height() - (double)(2*m_fillerSpace))
                    * (double)((double)m_used / (double)m_total));
            
            if (m_debug == true)
            {
                cerr << "       -Width  = " << width << "\n";
                cerr << "       -Height = " << height << endl;
            }

            if (m_orientation == 0)
            {
                dr->drawPixmap(m_location.x(), m_location.y(), m_container);
                dr->drawPixmap(m_location.x(), m_location.y(), m_filler, 0, 0, width + m_fillerSpace, -1);
            }
            else if (m_orientation == 1)
            {
                dr->drawPixmap(m_location.x(), m_location.y(), m_container);
                dr->drawPixmap(m_location.x() + width, m_location.y(), m_filler, width - m_fillerSpace, 0, -1, -1);
            }
            else if (m_orientation == 2)
            {
                dr->drawPixmap(m_location.x(), m_location.y(), m_container);
                dr->drawPixmap( m_location.x(),
                                (m_location.y() + m_container.height()) - height,
                                m_filler,
                                0,
                                (m_filler.height() - height) - m_fillerSpace, -1, -1);
            }
            else if (m_orientation == 3)
            {
                dr->drawPixmap(m_location.x(), m_location.y(), m_container);
                dr->drawPixmap(m_location.x(), m_location.y(), m_filler, 0, 0, -1, height + m_fillerSpace );
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
    r.translate(m_parent->GetAreaRect().left(),
                m_parent->GetAreaRect().top());
    screen_area = r;
}

void UIStatusBarType::setOrientation(int x)
{
    if (x < 0 || x > 3)
    {
        cerr << "uitypes.o: UIStatusBarType received an invalid request to set orientation to " << x << endl;
        return;
    }
    m_orientation = x ;
}

// *********************************************************************

UIManagedTreeListType::UIManagedTreeListType(const QString & name)
                     : UIType(name)
{
    bins = 0;
    bin_corners.clear();
    screen_corners.clear();
    route_to_active.clear();
    my_tree_data = NULL;
    current_node = NULL;
    active_node = NULL;
    active_parent = NULL;
    m_justification = (Qt::AlignLeft | Qt::AlignVCenter);
    active_bin = 0;
    tree_order = -1;
    visual_order = -1;
    iconAttr = -1;
    show_whole_tree = false;
    scrambled_parents = false;
    color_selectables = false;
    selectPadding = 0;
    selectScale = false;
    selectPoint.setX(0);
    selectPoint.setY(0);
    upArrowOffset.setX(0);
    upArrowOffset.setY(0);
    downArrowOffset.setX(0);
    downArrowOffset.setY(0);
    leftArrowOffset.setX(0);
    leftArrowOffset.setY(0);
    rightArrowOffset.setX(0);
    rightArrowOffset.setY(0);
    incSearch = "";
}

UIManagedTreeListType::~UIManagedTreeListType()
{
    while (!resized_highlight_images.empty())
    {
        delete resized_highlight_images.back();
        resized_highlight_images.pop_back();
    }
}

void UIManagedTreeListType::drawText(QPainter *p,
                                    QString the_text,
                                    QString font_name,
                                    int x, int y,
                                    int bin_number,
                                    int icon_number)
{
    fontProp *temp_font = NULL;
    QString a_string = QString("bin%1-%2").arg(bin_number).arg(font_name);
    temp_font = &m_fontfcns[m_fonts[a_string]];
    p->setFont(temp_font->face);
    p->setPen(QPen(temp_font->color, (int)(2 * m_wmult)));

    if (!show_whole_tree)
    {
        the_text = cutDown(the_text, &(temp_font->face), false, area.width() - 80, area.height());
        p->drawText(x, y, the_text);
    }
    else if (bin_number == bins)
    {
        // See if we should leave room for an icon to the left of the text
        int iconDim = 0;
        if (iconAttr >= 0)
            iconDim = QFontMetrics(temp_font->face).height();

        the_text = cutDown(the_text, &(temp_font->face), false, bin_corners[bin_number].width() - right_arrow_image.width()-iconDim, bin_corners[bin_number].height());
        p->drawText(x+iconDim, y, the_text);
        if ((icon_number >= 0) && (iconMap.contains(icon_number)))
            p->drawPixmap(x, y-iconDim+QFontMetrics(temp_font->face).descent(),
                          *iconMap[icon_number]);
    }
    else if (bin_number == 1)
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
    if (hidden)
        return;

    //  Do nothing if context is wrong;

    if (m_context != context)
    {
        if (m_context != -1)
        {
            return;
        }
    }

    if (drawlayer != m_order)
    {
        // not my turn
        return;
    }

    if (!current_node)
    {
        // no data (yet?)
        return;
    }

    //
    //  Put something on the LCD device (if one exists)
    //

    if (class LCD *lcddev = LCD::Get())
    {
        QString msg = current_node->getString();
        GenericTree *parent = current_node->getParent();
        if (parent == NULL)
        {
            VERBOSE(VB_IMPORTANT, "UIManagedTreeListType: LCD sees no "
                 "parent to current_node" );
        }
        else
        {
            // add max of lcd height menu items either side of the selected node
            // let the lcdserver figure out which ones to display
            int pos = parent->getChildPosition(current_node, visual_order);
            bool selected;

            vector<GenericTree*> nodes = parent->getAllChildren(visual_order);
            QList<LCDMenuItem> menuItems;

            uint i =
                (pos > (int)lcddev->getLCDHeight()) ?
                pos - lcddev->getLCDHeight() : 0;

            uint count = 0;

            for (;i < nodes.size() && (count < lcddev->getLCDHeight() * 2); i++)
            {
                selected = (nodes[i] == current_node);

                menuItems.append(LCDMenuItem(selected, NOTCHECKABLE,
                                             nodes[i]->getString()));
                ++count;
            }

            QString title;
            title = (parent->getParent()) ? "<< " : "   ";
            title += (current_node->childCount () > 0) ? " >> " : "  ";
            if (!menuItems.isEmpty())
            {
                lcddev->switchToMenu(menuItems, title);
            }
        }
    }

    bool draw_up_arrow = false;
    bool draw_down_arrow = false;

    //
    //  Draw each bin, using current_node and working up
    //  and/or down to tell us what to draw in each bin.
    //

    int starting_bin = 0;
    int ending_bin = 0;

    if (show_whole_tree)
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

        if (i < active_bin)
        {
            for(int j = 0; j < active_bin - i; j++)
            {
                if (hotspot_node)
                {
                    if (hotspot_node->getParent())
                    {
                        hotspot_node = hotspot_node->getParent();
                    }
                }
            }
        }
        if (i > active_bin)
        {
            for(int j = 0; j < i - active_bin; j++)
            {
                if (hotspot_node)
                {
                    if (hotspot_node->childCount() > 0)
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

        if (hotspot_node)
        {
            QString a_string = QString("bin%1-active").arg(i);
            fontProp *tmpfont = &m_fontfcns[m_fonts[a_string]];
            int x_location = bin_corners[i].left();
            int y_location = bin_corners[i].top() + (bin_corners[i].height() / 2)
                             + (QFontMetrics(tmpfont->face).height() / 2);

            if (!show_whole_tree)
            {
                x_location = area.left();
                y_location = area.top() + (area.height() / 2) + (QFontMetrics(tmpfont->face).height() / 2);
            }

            if (i == bins)
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
                if (!show_whole_tree)
                {
                    a_limit = area.top();
                }
                while (another_y_location - QFontMetrics(tmpfont->face).height() > a_limit)
                {
                    another_y_location -= QFontMetrics(tmpfont->face).height();
                    ++number_of_slots;
                }

                if (position_in_list <= number_of_slots)
                {
                    draw_up_arrow = false;
                }
                if (position_in_list < number_of_slots)
                {
                    for(int j = 0; j < number_of_slots - position_in_list; j++)
                    {
                        y_location -= QFontMetrics(tmpfont->face).height();
                    }
                }
                if ((number_in_list - position_in_list) <= number_of_slots &&
                   position_in_list > number_of_slots)
                {
                    draw_down_arrow = false;
                    if (number_in_list >= number_of_slots * 2 + 1)
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
                if ((number_in_list - position_in_list) == number_of_slots + 1)
                {
                    draw_down_arrow = false;
                }
                if (number_in_list <  (number_of_slots * 2) + 2)
                {
                    draw_down_arrow = false;
                    draw_up_arrow = false;
                }
            }

            QString font_name = "active";
            if (i > active_bin)
            {
                font_name = "inactive";
            }
            if (hotspot_node == active_node)
            {
                font_name = "selected";
            }
            if (i == active_bin && color_selectables && hotspot_node->isSelectable())
            {
                font_name = "selectable";
            }



            if (i == active_bin)
            {
                //
                //  Draw the highlight pixmap for this bin
                //

                if (show_whole_tree)
                {
                    p->drawPixmap(x_location + selectPoint.x(), y_location - QFontMetrics(tmpfont->face).height() + QFontMetrics(tmpfont->face).descent() + selectPoint.y(), (*highlight_map[i]));
                    //p->drawPixmap(x_location, y_location - QFontMetrics(tmpfont->face).height() + QFontMetrics(tmpfont->face).descent(), (*highlight_map[i]));

                    //
                    //  Left or right arrows
                    //
                    if (i == bins && hotspot_node->childCount() > 0)
                    {
                        p->drawPixmap(x_location + (*highlight_map[i]).width() - right_arrow_image.width() + rightArrowOffset.x(),
                                      y_location + rightArrowOffset.y() - QFontMetrics(tmpfont->face).height() + right_arrow_image.height() / 2,
                                      right_arrow_image);
                    }
                    if (i == 1 && hotspot_node->getParent()->getParent())
                    {
                        p->drawPixmap(x_location + leftArrowOffset.x(),
                                      y_location + leftArrowOffset.y() - QFontMetrics(tmpfont->face).height() + left_arrow_image.height() / 2,
                                      left_arrow_image);
                    }
                }
                else
                {
                    p->drawPixmap(x_location + selectPoint.x(), y_location - QFontMetrics(tmpfont->face).height() + QFontMetrics(tmpfont->face).descent() + selectPoint.y(), (*highlight_map[0]));
                }
            }

            //
            //  Move this down for people with (slightly) broken fonts
            //

            QString msg = hotspot_node->getString();
            int icn = -1;
            if (iconAttr >= 0)
                icn = hotspot_node->getAttribute(iconAttr);
            drawText(p, msg, font_name, x_location, y_location, i, icn);

            if (i == bins)
            {
                //
                //  Draw up/down arrows
                //

                if (draw_up_arrow)
                {
                    if (show_whole_tree)
                    {
                        p->drawPixmap((bin_corners[i].right() - up_arrow_image.width()) + upArrowOffset.x(),
                                      bin_corners[i].top()  + upArrowOffset.y(),
                                      up_arrow_image);
                    }
                    else
                    {
                        p->drawPixmap((area.right() - up_arrow_image.width())+ upArrowOffset.x(),
                                      area.top() + upArrowOffset.y(),
                                      up_arrow_image);
                    }
                }
                if (draw_down_arrow)
                {
                    if (show_whole_tree)
                    {
                        p->drawPixmap((bin_corners[i].right() - down_arrow_image.width()) + downArrowOffset.x(),
                                      (bin_corners[i].bottom() - down_arrow_image.height())  + downArrowOffset.y(),
                                      down_arrow_image);
                    }
                    else
                    {
                        p->drawPixmap((area.right() - down_arrow_image.width()) + downArrowOffset.x(),
                                      (area.bottom() - down_arrow_image.height())  + downArrowOffset.y(),
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
            if (!show_whole_tree)
            {
                a_limit = area.top();
            }
            while (still_yet_another_y_location - QFontMetrics(tmpfont->face).height() > a_limit)
            {
                GenericTree *above = hotspot_node->prevSibling(numb_above, visual_order);
                if (above)
                {
                    if (i == active_bin && color_selectables && above->isSelectable())
                    {
                        font_name = "selectable";
                    }
                    else if (above == active_node)
                    {
                        font_name = "selected";
                    }
                    else if (i == active_bin)
                    {
                        font_name = "active";
                    }
                    else
                    {
                        font_name = "inactive";
                    }
                    msg = above->getString();
                    int icn = -1;
                    if (iconAttr >= 0)
                        icn = above->getAttribute(iconAttr);
                    drawText(p, msg, font_name, x_location, still_yet_another_y_location, i, icn);
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
            if (!show_whole_tree)
            {
                a_limit = area.bottom();
            }
            while (y_location < a_limit)
            {
                GenericTree *below = hotspot_node->nextSibling(numb_below, visual_order);
                if (below)
                {
                    if (i == active_bin && color_selectables && below->isSelectable())
                    {
                        font_name = "selectable";
                    }
                    else if (below == active_node)
                    {
                        font_name = "selected";
                    }
                    else if (i == active_bin)
                    {
                        font_name = "active";
                    }
                    else
                    {
                        font_name = "inactive";
                    }
                    msg = below->getString();
                    int icn = -1;
                    if (iconAttr >= 0)
                        icn = below->getAttribute(iconAttr);
                    drawText(p, msg, font_name, x_location, y_location, i, icn);
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

void UIManagedTreeListType::moveToNode(QList<int> route_of_branches)
{
    current_node = my_tree_data->findNode(route_of_branches);
    if (!current_node)
    {
        current_node = my_tree_data->findLeaf();
    }
    active_node = current_node;
    active_parent = active_node->getParent();
    emit nodeSelected(current_node->getInt(), current_node->getAttributes());
}

void UIManagedTreeListType::moveToNodesFirstChild(QList<int> route_of_branches)
{
    GenericTree *finder = my_tree_data->findNode(route_of_branches);

    if (finder)
    {
        if (finder->childCount() > 0)
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

QList<int> *UIManagedTreeListType::getRouteToActive(void)
{
    if (active_node)
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
}

QStringList UIManagedTreeListType::getRouteToCurrent()
{
    QStringList route_to_current;
    if (current_node)
    {
        GenericTree *climber = current_node;
        route_to_current.push_front(climber->getString());
        while( (climber = climber->getParent()) )
        {
            route_to_current.push_front(climber->getString());
        }
        return route_to_current;
    }
    return route_to_current;
}

bool UIManagedTreeListType::tryToSetActive(QList<int> route)
{
    GenericTree *a_node = my_tree_data->findNode(route);
    if (a_node && a_node->isSelectable())
    {
        active_node = a_node;
        current_node = a_node;
        active_parent = active_node->getParent();
        return true;
    }
    return false;
}

bool UIManagedTreeListType::tryToSetCurrent(QStringList route)
{
    if (!my_tree_data)
    {
        current_node = NULL;
        return false;
    }

    current_node = my_tree_data;

    //
    //
    //

    if (route.count() > 0)
    {
        if (route[0] == my_tree_data->getString())
        {
            for(int i = 1; i < route.count(); i ++)
            {
                GenericTree *descender = current_node->getChildByName(route[i]);
                if (descender)
                {
                    current_node = descender;
                }
                else
                {
                    return false;
                }
            }
            return true;
        }
        else
        {
            return false;
        }
    }
    return false;

}

void UIManagedTreeListType::assignTreeData(GenericTree *a_tree)
{
    if (a_tree)
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

    if (a_node)
    {
        current_node = a_node;
    }
}

int UIManagedTreeListType::getActiveBin()
{
    return active_bin;
}

void UIManagedTreeListType::setActiveBin(int a_bin)
{
    if (a_bin > bins)
    {
        active_bin = bins;
    }
    else
    {
        active_bin = a_bin;
    }
}

void UIManagedTreeListType::makeHighlights()
{
    while (!resized_highlight_images.empty())
    {
        delete resized_highlight_images.back();
        resized_highlight_images.pop_back();
    }
    highlight_map.clear();

    //
    //  (pre-)Draw the highlight pixmaps
    //

    for(int i = 1; i <= bins; i++)
    {
        if (selectScale)
        {
            QImage temp_image = highlight_image.toImage();
            fontProp *tmpfont = NULL;
            QString a_string = QString("bin%1-active").arg(i);
            tmpfont = &m_fontfcns[m_fonts[a_string]];
            QPixmap temp_pixmap = QPixmap::fromImage(
                temp_image.scaled(
                    bin_corners[i].width(),
                    QFontMetrics(tmpfont->face).height() + selectPadding,
                    Qt::IgnoreAspectRatio,
                    Qt::SmoothTransformation));

            QPixmap *new_pixmap = new QPixmap(temp_pixmap);
            resized_highlight_images.append(new_pixmap);
            highlight_map[i] = new_pixmap;
        }
        else
        {
            highlight_map[i] = &highlight_image;
        }
    }

    //
    //  Make no tree version
    //

    if (selectScale)
    {
        QImage temp_image = highlight_image.toImage();
        fontProp *tmpfont = NULL;
        QString a_string = QString("bin%1-active").arg(bins);
        tmpfont = &m_fontfcns[m_fonts[a_string]];
        QPixmap temp_pixmap = QPixmap::fromImage(
            temp_image.scaled(
                area.width(),
                QFontMetrics(tmpfont->face).height() + selectPadding,
                Qt::IgnoreAspectRatio,
                Qt::SmoothTransformation));
        QPixmap *new_pixmap = new QPixmap(temp_pixmap);
        resized_highlight_images.append(new_pixmap);
        highlight_map[0] = new_pixmap;
    }
    else
    {
        highlight_map[0] = &highlight_image;
    }

}

bool UIManagedTreeListType::popUp()
{
    //
    //  Move the active node to the
    //  current active node's parent
    //

    if (!current_node)
    {
        return false;
    }

    if (!current_node->getParent())
    {
        return false;
    }

    if (!current_node->getParent()->getParent())
    {
        //
        //  I be ultimate root
        //

        return false;

    }

    if (!show_whole_tree)
    {
        //
        //  Oh no you don't
        //

        return false;
    }

    if (active_bin > 1)
    {
        --active_bin;
        current_node = current_node->getParent();
        emit nodeEntered(current_node->getInt(), current_node->getAttributes());
    }
    else if (active_bin < bins)
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

    if (!current_node)
    {
        return false;
    }

    if (current_node->childCount() < 1)
    {
        //
        //  I be leaf
        return false;
    }

    if (!show_whole_tree)
    {
        //
        //  Bad tree, bad!
        //
        return false;
    }

    ++active_bin;
    if (active_bin > bins)
        active_bin = bins;

    current_node = current_node->getSelectedChild(visual_order);
    emit nodeEntered(current_node->getInt(), current_node->getAttributes());

    refresh();
    return true;
}

bool UIManagedTreeListType::moveUp(bool do_refresh)
{
    if (!current_node)
    {
        return false;
    }
    //
    //  Move the active node to the
    //  current active node's previous
    //  sibling
    //

    GenericTree *new_node = current_node->prevSibling(1, visual_order);
    if (new_node)
    {
        current_node = new_node;
        if (do_refresh)
        {
            if (show_whole_tree)
            {
                for (int i = active_bin; i <= bins; i++)
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

bool UIManagedTreeListType::moveUpByAmount(int number_up, bool do_refresh)
{
    if (!current_node)
    {
        return false;
    }
    //
    //  Move the active node to the
    //  current active node's previous
    //  sibling
    //

    GenericTree *new_node = current_node->prevSibling(number_up, visual_order);
    if (new_node)
    {
        current_node = new_node;
        if (do_refresh)
        {
            if (show_whole_tree)
            {
                for (int i = active_bin; i <= bins; i++)
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
    if (!current_node)
    {
        return false;
    }

    //
    //  Move the active node to the
    //  current active node's next
    //  sibling
    //

    GenericTree *new_node = current_node->nextSibling(1, visual_order);
    if (new_node)
    {
        current_node = new_node;
        if (do_refresh)
        {
            if (show_whole_tree)
            {
                for (int i = active_bin; i <= bins; i++)
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

bool UIManagedTreeListType::moveDownByAmount(int number_down, bool do_refresh)
{
    if (!current_node)
    {
        return false;
    }

    //
    //  Move the active node to the
    //  current active node's next
    //  sibling
    //

    GenericTree *new_node = current_node->nextSibling(number_down, visual_order);
    if (new_node)
    {
        current_node = new_node;
        if (do_refresh)
        {
            if (show_whole_tree)
            {
                for (int i = active_bin; i <= bins; i++)
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

bool UIManagedTreeListType::incSearchStart(void)
{
    MythPopupBox *popup = new MythPopupBox(GetMythMainWindow(),
                                           "incsearch_popup");

    QLabel *caption = popup->addLabel(tr("Search"), MythPopupBox::Large);
    caption->setAlignment(Qt::AlignCenter);

    MythComboBox *modeCombo = new MythComboBox(false, popup, "mode_combo" );
    modeCombo->insertItem(tr("Starts with text"));
    modeCombo->insertItem(tr("Contains text"));
    popup->addWidget(modeCombo);

    MythLineEdit *searchEdit = new MythLineEdit(false, popup, "mode_combo");
    searchEdit->setText(incSearch);
    popup->addWidget(searchEdit);
    searchEdit->setFocus();

    popup->addButton(tr("Search"));
    popup->addButton(tr("Cancel"));

    DialogCode res = popup->ExecPopup();

    if (kDialogCodeButton0 == res)
    {
        incSearch = searchEdit->text();
        bIncSearchContains = (modeCombo->currentIndex() == 1);
        incSearchNext();
    }

    popup->hide();
    popup->deleteLater();

    return (kDialogCodeButton0 == res);
}

bool UIManagedTreeListType::incSearchNext(void)
{
    if (!current_node)
    {
        return false;
    }

    //
    //  Move the active node to the node whose string value
    //  starts or contains the search text
    //

    GenericTree *new_node = current_node->nextSibling(1, visual_order);
    while (new_node)
    {
        if (bIncSearchContains)
        {
            if (new_node->getString().indexOf(
                    incSearch, 0, Qt::CaseInsensitive) != -1)
            {
                break;
            }
        }
        else
        {
            if (new_node->getString().startsWith(
                    incSearch, Qt::CaseInsensitive))
            {
                break;
            }
        }

        new_node = new_node->nextSibling(1, visual_order);
    }

    // if new_node is NULL, we reached the end of the list. wrap to the
    // beginning and try again
    if (!new_node)
    {
        new_node = current_node->getParent()->getChildAt(0, visual_order);

        while (new_node)
        {
            // we're back at the current_node, which means there are no
            // matching nodes
            if (new_node == current_node)
            {
                new_node = NULL;
                break;
            }

            if (bIncSearchContains)
            {
                if (new_node->getString().indexOf(
                        incSearch, 0, Qt::CaseInsensitive) != -1)
                {
                    break;
                }
            }
            else
            {
                if (new_node->getString().startsWith(
                        incSearch, Qt::CaseInsensitive))
                {
                    break;
                }
            }

            new_node = new_node->nextSibling(1, visual_order);
        }
    }

    if (new_node)
    {
        current_node = new_node;
        if (show_whole_tree)
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

    if (bin_number < 1 || bin_number > bins)
    {
        return 0;
    }
    int return_value = 1;


    QString a_string = QString("bin%1-active").arg(bin_number);
    fontProp *tmpfont = &m_fontfcns[m_fonts[a_string]];
    int y_location =    bin_corners[bin_number].top()
                        + (bin_corners[bin_number].height() / 2)
                        + (QFontMetrics(tmpfont->face).height() / 2);
    if (!show_whole_tree)
    {
        y_location = area.top() + (area.height() / 2) + (QFontMetrics(tmpfont->face).height() / 2);
    }
    int another_y_location = y_location - QFontMetrics(tmpfont->face).height();
    int a_limit = bin_corners[bin_number].top();
    if (!show_whole_tree)
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
    if (!show_whole_tree)
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
    if (!current_node)
    {
        return false;
    }
    int entries_to_jump = calculateEntriesInBin(active_bin);
    for(int i = 0; i < entries_to_jump; i++)
    {
        if (!moveUp(false))
        {
            i = entries_to_jump;
        }

    }
    if (show_whole_tree)
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
    if (!current_node)
    {
        return false;
    }
    if (!current_node)
    {
        return false;
    }
    int entries_to_jump = calculateEntriesInBin(active_bin);
    for(int i = 0; i < entries_to_jump; i++)
    {
        if (!moveDown(false))
        {
            i = entries_to_jump;
        }

    }
    if (show_whole_tree)
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
    if (current_node)
    {
        if (current_node->isSelectable())
        {
            active_node = current_node;
            active_parent = active_node->getParent();
            if (show_whole_tree)
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
            if (first_leaf->isSelectable())
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
    if (active_node)
    {
        emit requestUpdate(screen_corners[active_bin]);
        emit nodeSelected(active_node->getInt(), active_node->getAttributes());
    }
}

void UIManagedTreeListType::enter()
{
    if (current_node)
    {
        emit nodeEntered(current_node->getInt(), current_node->getAttributes());
    }
}

bool UIManagedTreeListType::nextActive(bool wrap_around, bool traverse_up_down)
{
    if (!active_node)
    {
        return false;
    }
    if (traverse_up_down && active_parent != active_node->getParent())
    {
        return complexInternalNextPrevActive(true, wrap_around);
    }

    //
    //  set a flag if active and current are sync'd
    //

    bool in_sync = false;

    if (current_node == active_node)
    {
        in_sync = true;
    }

    if (active_node)
    {
        GenericTree *test_node = active_node->nextSibling(1, tree_order);
        if (test_node)
        {
            active_node = test_node;
            if (in_sync)
            {
                current_node = active_node;
            }
            if (show_whole_tree)
            {
                emit requestUpdate(screen_corners[active_bin]);
            }
            else
            {
                refresh();
            }
            return true;
        }
        else if (wrap_around)
        {
            test_node = active_node->getParent();
            if (test_node)
            {
                test_node = test_node->getChildAt(0, tree_order);
                if (test_node)
                {
                    active_node = test_node;
                    if (in_sync)
                    {
                        current_node = active_node;
                    }
                    if (show_whole_tree)
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
    if (!active_node)
    {
        return false;
    }
    if (traverse_up_down && active_parent != active_node->getParent())
    {
        return complexInternalNextPrevActive(false, wrap_around);
    }
    bool in_sync = false;

    if (current_node == active_node)
    {
        in_sync = true;
    }

    if (active_node)
    {
        GenericTree *test_node = active_node->prevSibling(1, tree_order);
        if (test_node)
        {
            active_node = test_node;
            if (in_sync)
            {
                current_node = active_node;
            }
            if (show_whole_tree)
            {
                emit requestUpdate(screen_corners[active_bin]);
            }
            else
            {
                refresh();
            }
            return true;
        }
        else if (wrap_around)
        {
            test_node = active_node->getParent();
            if (test_node)
            {
                int numb_children = test_node->childCount();
                if (numb_children > 0)
                {
                    test_node = test_node->getChildAt(numb_children - 1, tree_order);
                    if (test_node)
                    {
                        active_node = test_node;
                        if (in_sync)
                        {
                            current_node = active_node;
                        }
                        if (show_whole_tree)
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
    if (active_parent)
    {
        bool in_sync = false;
        if (current_node == active_node)
        {
            in_sync = true;
        }
        GenericTree *next;
        next = active_parent->nextPrevFromFlatList(forward_or_back, wrap_around, active_node);
        if (next)
        {
            active_node = next;
            if (in_sync)
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
        r.translate(m_parent->GetAreaRect().left(),
                    m_parent->GetAreaRect().top());
        ++i;
        screen_corners[i] = r;
    }

    screen_area = m_parent->GetAreaRect();
}

// ********************************************************************

UIPushButtonType::UIPushButtonType(const QString &name, QPixmap on, QPixmap off, QPixmap pushed, QPixmap pushedon)
                     : UIType(name)
{
    on_pixmap = on;
    off_pixmap = off;
    pushed_pixmap = pushed;
    pushedon_pixmap = pushedon;
    currently_pushed = false;
    takes_focus = true;
    m_lockOn = false;
    connect(&push_timer, SIGNAL(timeout()), this, SLOT(unPush()));
}


void UIPushButtonType::Draw(QPainter *p, int drawlayer, int context)
{
    if (hidden)
        return;

    if (context != m_context)
    {
        if (m_context != -1)
        {
            return;
        }
    }

    if (drawlayer != m_order)
    {
        // not my turn
        return;
    }

    if (currently_pushed)
    {
        if (has_focus && !pushedon_pixmap.isNull())
        {
            p->drawPixmap(m_displaypos.x(), m_displaypos.y(), pushedon_pixmap);
        }
        else
        {
            p->drawPixmap(m_displaypos.x(), m_displaypos.y(), pushed_pixmap);
        }
    }
    else
    {
        if (has_focus)
        {
            p->drawPixmap(m_displaypos.x(), m_displaypos.y(), on_pixmap);
        }
        else
        {
            p->drawPixmap(m_displaypos.x(), m_displaypos.y(), off_pixmap);
        }
    }

}

void UIPushButtonType::setLockOn()
{
    m_lockOn = true;
}

void UIPushButtonType::push()
{
    if (currently_pushed)
    {
        return;
    }
    currently_pushed = true;

    refresh();

    if (m_lockOn)
    {
        emit pushed(Name());
        return;
    }

    push_timer.setSingleShot(true);
    push_timer.start(300);

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
    if (on_pixmap.width() > width)
    {
        width = on_pixmap.width();
    }
    if (pushed_pixmap.width() > width)
    {
        width = pushed_pixmap.width();
    }
    if (pushedon_pixmap.width() > width)
    {
        width = pushedon_pixmap.width();
    }

    height = off_pixmap.height();
    if (on_pixmap.height() > height)
    {
        height = on_pixmap.height();
    }
    if (pushed_pixmap.height() > height)
    {
        height = pushed_pixmap.height();
    }
    if (pushedon_pixmap.height() > height)
    {
        height = pushedon_pixmap.height();
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
    if (hidden)
        return;

    if (context != m_context)
    {
        if (m_context != -1)
        {
            return;
        }
    }

    if (drawlayer != m_order)
    {
        // not my turn
        return;
    }

    if (currently_pushed)
    {
        p->drawPixmap(m_displaypos.x(), m_displaypos.y(), pushed_pixmap);
    }
    else
    {
        if (has_focus)
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
    if (currently_pushed)
    {
        return;
    }
    currently_pushed = true;
    push_timer.setSingleShot(true);
    push_timer.start(300);
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
    if (on_pixmap.width() > width)
    {
        width = on_pixmap.width();
    }
    if (pushed_pixmap.width() > width)
    {
        width = pushed_pixmap.width();
    }

    height = off_pixmap.height();
    if (on_pixmap.height() > height)
    {
        height = on_pixmap.height();
    }
    if (pushed_pixmap.height() > height)
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
    if (hidden)
        return;

    if (context != m_context)
    {
        if (m_context != -1)
        {
            return;
        }
    }

    if (drawlayer != m_order)
    {
        return;
    }

    if (has_focus)
    {
        if (checked)
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
        if (checked)
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
    if (unchecked_pixmap.width() > width)
    {
        width = unchecked_pixmap.width();
    }
    if (checked_pixmap_high.width() > width)
    {
        width = checked_pixmap_high.width();
    }
    if (unchecked_pixmap_high.width() > width)
    {
        width = unchecked_pixmap_high.width();
    }

    height = checked_pixmap.height();
    if (unchecked_pixmap.height() > height)
    {
        height = unchecked_pixmap.height();
    }
    if (checked_pixmap_high.height() > height)
    {
        height = checked_pixmap_high.height();
    }
    if (unchecked_pixmap_high.height() > height)
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
    current_data = NULL;
}

void UISelectorType::Draw(QPainter *p, int drawlayer, int context)
{
    if (hidden)
        return;

    if (context != m_context)
    {
        if (m_context != -1)
        {
            return;
        }
    }

    if (drawlayer != m_order)
    {
        // not my turn
        return;
    }

    if (currently_pushed)
    {
        p->drawPixmap(m_displaypos.x(), m_displaypos.y(), pushed_pixmap);
    }
    else
    {
        if (has_focus)
        {
            p->drawPixmap(m_displaypos.x(), m_displaypos.y(), on_pixmap);
        }
        else
        {
            p->drawPixmap(m_displaypos.x(), m_displaypos.y(), off_pixmap);
        }
    }
    if (current_data)
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
    r.translate(m_parent->GetAreaRect().left(),
                m_parent->GetAreaRect().top());
    screen_area = r;
}

void UISelectorType::addItem(int an_int, const QString &a_string)
{
    IntStringPair *new_data = new IntStringPair(an_int, a_string);
    my_data.append(new_data);
    if (!current_data)
    {
        current_data = new_data;
    }
}

void UISelectorType::setToItem(int which_item)
{
    for(uint i = 0; i < (uint)my_data.size(); i++)
    {
        if (my_data[i]->getInt() == which_item)
        {
            current_data = my_data[i];
            refresh();
        }
    }
}

void UISelectorType::setToItem(const QString &which_item)
{
    for (uint i = 0; i < (uint)my_data.size(); i++)
    {
        if (my_data[i]->getString() == which_item)
        {
            current_data = my_data[i];
            refresh();
        }
    }
}

QString UISelectorType::getCurrentString()
{
    if (current_data)
        return current_data->getString();
    else
        return "";
}

int UISelectorType::getCurrentInt()
{
    if (current_data)
        return current_data->getInt();
    else
        return -1;
}


void UISelectorType::push(bool up_or_down)
{
    if (currently_pushed)
    {
        return;
    }
    currently_pushed = true;
    push_timer.setSingleShot(true);
    push_timer.start(300);

    if (current_data)
    {
        int cur_indx = my_data.indexOf(current_data);
        int next_indx = (up_or_down) ? cur_indx + 1 : cur_indx - 1;

        if (next_indx >= my_data.size())
            current_data = my_data.empty() ? NULL : my_data.front();
        else if (next_indx < 0)
            current_data = my_data.empty() ? NULL : my_data.back();
        else
            current_data = my_data[next_indx];

        if (current_data)
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
    while (!my_data.empty())
    {
        delete my_data.back();
        my_data.pop_back();
    }
}

// ********************************************************************

UIBlackHoleType::UIBlackHoleType(const QString &name)
                     : UIType(name)
{
}

void UIBlackHoleType::calculateScreenArea()
{
    QRect r = area;
    r.translate(m_parent->GetAreaRect().left(),
                m_parent->GetAreaRect().top());
    screen_area = r;
}

// ********************************************************************

UIKeyType::UIKeyType(const QString &name)
         : UIType(name)
{
    m_normalImg = m_focusedImg = m_downImg = m_downFocusedImg = NULL;
    m_normalFont = m_focusedFont = m_downFont = m_downFocusedFont = NULL;

    m_pos = QPoint(0, 0);

    m_bDown = false;
    m_bShift = false;
    m_bAlt = false;
    m_bToggle = false;

    takes_focus = true;
    connect(&m_pushTimer, SIGNAL(timeout()), this, SLOT(unPush()));
}

UIKeyType::~UIKeyType()
{
}

void UIKeyType::Draw(QPainter *dr, int drawlayer, int context)
{
    if (m_context == context || m_context == -1)
    {
        if (drawlayer == m_order)
        {
            fontProp *tempFont;

            // draw the button image
            if (!m_bDown)
            {
                if (!has_focus)
                {
                    dr->drawPixmap(m_pos.x(), m_pos.y(), *m_normalImg);
                    tempFont = m_normalFont;
                }
                else
                {
                    dr->drawPixmap(m_pos.x(), m_pos.y(), *m_focusedImg);
                    tempFont = m_focusedFont;
                }
            }
            else
            {
                if (!has_focus)
                {
                    dr->drawPixmap(m_pos.x(), m_pos.y(), *m_downImg);
                    tempFont = m_downFont;
                }
                else
                {
                    dr->drawPixmap(m_pos.x(), m_pos.y(), *m_downFocusedImg);
                    tempFont = m_downFocusedFont;
                }
            }

            dr->setFont(tempFont->face);

            // draw the button text
            QString text;

            if (!m_bShift)
            {
                if (!m_bAlt)
                    text = m_normalChar;
                else
                    text = m_altChar;
            }
            else
            {
                if (!m_bAlt)
                    text = m_shiftChar;
                else
                    text = m_shiftAltChar;
            }

            if (drawFontShadow &&
                (tempFont->shadowOffset.x() != 0 || tempFont->shadowOffset.y() != 0))
            {
                dr->setBrush(tempFont->dropColor);
                dr->setPen(QPen(tempFont->dropColor, (int)(2 * m_wmult)));
                dr->drawText(m_pos.x() + tempFont->shadowOffset.x(),
                             m_pos.y() + tempFont->shadowOffset.y(),
                             m_area.width(), m_area.height(),
                             Qt::AlignCenter, text);
            }

            dr->setBrush(tempFont->color);
            dr->setPen(QPen(tempFont->color, (int)(2 * m_wmult)));
            dr->drawText(m_pos.x(), m_pos.y(),
                         m_area.width(), m_area.height(),
                         Qt::AlignCenter,
                         text);
        }
    }
}

void UIKeyType::SetImages(QPixmap *normal, QPixmap *focused,
                          QPixmap *down, QPixmap *downFocused)
{
    m_normalImg = normal;
    m_focusedImg = focused;
    m_downImg = down;
    m_downFocusedImg = downFocused;
}

void UIKeyType::SetDefaultImages(QPixmap *normal, QPixmap *focused,
                                 QPixmap *down, QPixmap *downFocused)
{
    if (!m_normalImg) m_normalImg = normal;
    if (!m_focusedImg) m_focusedImg = focused;
    if (!m_downImg) m_downImg = down;
    if (!m_downFocusedImg) m_downFocusedImg = downFocused;
}

void UIKeyType::SetFonts(fontProp *normal, fontProp *focused,
                         fontProp *down, fontProp *downFocused)
{
    m_normalFont = normal;
    m_focusedFont = focused;
    m_downFont = down;
    m_downFocusedFont = downFocused;
}

void UIKeyType::SetDefaultFonts(fontProp *normal, fontProp *focused,
                                fontProp *down, fontProp *downFocused)
{
    if (!m_normalFont) m_normalFont = normal;
    if (!m_focusedFont) m_focusedFont = focused;
    if (!m_downFont) m_downFont = down;
    if (!m_downFocusedFont) m_downFocusedFont = downFocused;
}

void UIKeyType::SetChars(QString normal, QString shift, QString alt,
                         QString shiftAlt)
{
    m_normalChar = decodeChar(normal);
    m_shiftChar = decodeChar(shift);
    m_altChar = decodeChar(alt);
    m_shiftAltChar = decodeChar(shiftAlt);
}

QString UIKeyType::GetChar()
{
    if (!m_bShift && !m_bAlt)
        return m_normalChar;
    else if (m_bShift && !m_bAlt)
        return m_shiftChar;
    else if (!m_bShift && m_bAlt)
        return m_altChar;
    else if (m_bShift && m_bAlt)
        return m_shiftAltChar;

    return m_normalChar;
}

QString UIKeyType::decodeChar(QString c)
{
    QString res = "";

    while (c.length() > 0)
    {
        if (c.startsWith("0x"))
        {
            QString sCode = c.left(6);
            bool bOK;
            short nCode = sCode.toShort(&bOK, 16);

            c = c.mid(6);

            if (bOK)
            {
                QChar uc(nCode);
                res += QString(uc);
            }
            else
                cout <<  "UIKeyType::decodeChar - bad char code "
                     <<  "(" << sCode.toAscii().constData() << ")" << endl;
        }
        else
        {
            res += c.left(1);
            c = c.mid(1);
        }
    }

    return res;
}

void UIKeyType::SetMoves(QString moveLeft, QString moveRight, QString moveUp,
                         QString moveDown)
{
    m_moveLeft = moveLeft;
    m_moveRight = moveRight;
    m_moveUp = moveUp;
    m_moveDown = moveDown;
}

QString UIKeyType::GetMove(QString direction)
{
    QString res = m_moveLeft;

    if (direction == "Up")
        res = m_moveUp;
    else if (direction == "Down")
        res = m_moveDown;
    else if (direction == "Right")
        res = m_moveRight;

    return res;
}

void UIKeyType::calculateScreenArea()
{
    if (!m_normalImg)
        return;

    int width = m_normalImg->width();
    int height = m_normalImg->height();

    QRect r = QRect(m_pos.x(), m_pos.y(), width, height);
    r.translate(m_parent->GetAreaRect().left(),
                m_parent->GetAreaRect().top());
    screen_area = r;
    m_area = r;
}

void UIKeyType::SetShiftState(bool sh, bool ag)
{
    m_bShift = sh;
    m_bAlt = ag;
    refresh();
}

void UIKeyType::push()
{
    if (m_bToggle)
    {
        m_bDown = !m_bDown;
        refresh();
        emit pushed();

        return;
    }

    if (m_bDown)
        return;

    m_bDown = true;
    m_pushTimer.setSingleShot(true);
    m_pushTimer.start(300);
    refresh();
    emit pushed();
}

void UIKeyType::unPush()
{
    if (!m_bToggle)
    {
        m_bDown = false;
        refresh();
    }
}

// ********************************************************************

const int numcomps = 95;

const QString comps[numcomps][3] = {
        {"!", "!", (QChar)0xa1},    {"c", "/", (QChar)0xa2},
        {"l", "-", (QChar)0xa3},    {"o", "x", (QChar)0xa4},
        {"y", "-", (QChar)0xa5},    {"|", "|", (QChar)0xa6},
        {"s", "o", (QChar)0xa7},    {"\"", "\"", (QChar)0xa8},
        {"c", "o", (QChar)0xa9},    {"-", "a", (QChar)0xaa},
        {"<", "<", (QChar)0xab},    {"-", "|", (QChar)0xac},
        {"-", "-", (QChar)0xad},    {"r", "o", (QChar)0xae},
        {"^", "-", (QChar)0xaf},    {"^", "0", (QChar)0xb0},
        {"+", "-", (QChar)0xb1},    {"^", "2", (QChar)0xb2},
        {"^", "3", (QChar)0xb3},    {"/", "/", (QChar)0xb4},
        {"/", "u", (QChar)0xb5},    {"P", "!", (QChar)0xb6},
        {"^", ".", (QChar)0xb7},    {",", ",", (QChar)0xb8},
        {"^", "1", (QChar)0xb9},    {"_", "o", (QChar)0xba},
        {">", ">", (QChar)0xbb},    {"1", "4", (QChar)0xbc},
        {"1", "2", (QChar)0xbd},    {"3", "4", (QChar)0xbe},
        {"?", "?", (QChar)0xbf},    {"A", "`", (QChar)0xc0},
        {"A", "'", (QChar)0xc1},    {"A", "^", (QChar)0xc2},
        {"A", "~", (QChar)0xc3},    {"A", "\"", (QChar)0xc4},
        {"A", "*", (QChar)0xc5},    {"A", "E", (QChar)0xc6},
        {"C", ",", (QChar)0xc7},    {"E", "`", (QChar)0xc8},
        {"E", "'", (QChar)0xc9},    {"E", "^", (QChar)0xca},
        {"E", "\"", (QChar)0xcb},   {"I", "`", (QChar)0xcc},
        {"I", "'", (QChar)0xcd},    {"I", "^", (QChar)0xce},
        {"I", "\"", (QChar)0xcf},   {"D", "-", (QChar)0xd0},
        {"N", "~", (QChar)0xd1},    {"O", "`", (QChar)0xd2},
        {"O", "'", (QChar)0xd3},    {"O", "^", (QChar)0xd4},
        {"O", "~", (QChar)0xd5},    {"O", "\"", (QChar)0xd6},
        {"x", "x", (QChar)0xd7},    {"O", "/", (QChar)0xd8},
        {"U", "`", (QChar)0xd9},    {"U", "'", (QChar)0xda},
        {"U", "^", (QChar)0xdb},    {"U", "\"", (QChar)0xdc},
        {"Y", "'", (QChar)0xdd},    {"T", "H", (QChar)0xde},
        {"s", "s", (QChar)0xdf},    {"a", "`", (QChar)0xe0},
        {"a", "'", (QChar)0xe1},    {"a", "^", (QChar)0xe2},
        {"a", "~", (QChar)0xe3},    {"a", "\"", (QChar)0xe4},
        {"a", "*", (QChar)0xe5},    {"a", "e", (QChar)0xe6},
        {"c", ",", (QChar)0xe7},    {"e", "`", (QChar)0xe8},
        {"e", "'", (QChar)0xe9},    {"e", "^", (QChar)0xea},
        {"e", "\"", (QChar)0xeb},   {"i", "`", (QChar)0xec},
        {"i", "'", (QChar)0xed},    {"i", "^", (QChar)0xee},
        {"i", "\"", (QChar)0xef},   {"d", "-", (QChar)0xf0},
        {"n", "~", (QChar)0xf1},    {"o", "`", (QChar)0xf2},
        {"o", "'", (QChar)0xf3},    {"o", "^", (QChar)0xf4},
        {"o", "~", (QChar)0xf5},    {"o", "\"", (QChar)0xf6},
        {"-", ":", (QChar)0xf7},    {"o", "/", (QChar)0xf8},
        {"u", "`", (QChar)0xf9},    {"u", "'", (QChar)0xfa},
        {"u", "^", (QChar)0xfb},    {"u", "\"", (QChar)0xfc},
        {"y", "'", (QChar)0xfd},    {"t", "h", (QChar)0xfe},
        {"y", "\"", (QChar)0xff}
};

UIKeyboardType::UIKeyboardType(const QString &name, int order)
                    : UIType(name)
{
    m_order = order;
    m_container = NULL;
    m_parentEdit = NULL;
    m_parentDialog = NULL;
    m_bInitalized = false;
    m_focusedKey = m_doneKey = m_altKey = m_lockKey = NULL;
    m_shiftRKey = m_shiftLKey = NULL;

    m_bCompTrap = false;
    m_comp1 = "";
}


UIKeyboardType::~UIKeyboardType()
{
    if (m_container)
        delete m_container;
}

void UIKeyboardType::init()
{
    m_bInitalized = true;

    KeyList::iterator it = m_keyList.begin();
    for (; it != m_keyList.end(); ++it)
    {
        UIKeyType *key = *it;
        if (key->GetType() == "char")
        {
            connect(key, SIGNAL( pushed() ), this, SLOT( charKey() ) );
        }
        else if (key->GetType() == "shift")
        {
            if (!m_shiftLKey)
            {
                connect(key, SIGNAL( pushed() ), this, SLOT( shiftLOnOff() ) );
                m_shiftLKey = key;
                m_shiftLKey->SetToggleKey(true);
            }
            else
            {
                connect(key, SIGNAL( pushed() ), this, SLOT( shiftROnOff() ) );
                m_shiftRKey = key;
                m_shiftRKey->SetToggleKey(true);
            }
        }
        else if (key->GetType() == "del")
        {
            connect(key, SIGNAL( pushed() ), this, SLOT( delKey() ) );
        }
        else if (key->GetType() == "back")
        {
            connect(key, SIGNAL( pushed() ), this, SLOT( backspaceKey() ) );
        }
        else if (key->GetType() == "lock")
        {
            connect(key, SIGNAL( pushed() ), this, SLOT( lockOnOff() ) );
            m_lockKey = key;
            m_lockKey->SetToggleKey(true);
        }
        else if (key->GetType() == "done")
        {
            connect(key, SIGNAL( pushed() ), this, SLOT( close() ) );
            m_doneKey = key;
        }
        else if (key->GetType() == "moveleft")
        {
            connect(key, SIGNAL( pushed() ), this, SLOT( leftCursor() ) );
        }
        else if (key->GetType() == "moveright")
        {
            connect(key, SIGNAL( pushed() ), this, SLOT( rightCursor() ) );
        }
        else if (key->GetType() == "comp")
        {
            connect(key, SIGNAL( pushed() ), this, SLOT( compOnOff() ) );
        }
        else if (key->GetType() == "alt")
        {
            connect(key, SIGNAL( pushed() ), this, SLOT( altGrOnOff() ) );
            m_altKey = key;
            m_altKey->SetToggleKey(true);
        }
    }
}

void UIKeyboardType::Draw(QPainter *dr, int drawlayer, int context)
{
    if (!m_bInitalized)
        init();

    if (hidden)
        return;

    if (m_context == context || m_context == -1)
    {
        if (drawlayer == m_order)
        {
            dr = dr;
        }
    }
}

void UIKeyboardType::calculateScreenArea()
{
    QRect r = m_area;
    r.translate(m_parent->GetAreaRect().left(),
                m_parent->GetAreaRect().top());
    screen_area = r;
}

void UIKeyboardType::leftCursor()
{
    if (!m_parentEdit)
        return;

    if (m_parentEdit->inherits("QLineEdit"))
    {
        QLineEdit *par = (QLineEdit *)m_parentEdit;
        par->cursorBackward(m_shiftLKey->IsOn());
    }
    else if (m_parentEdit->inherits("QTextEdit"))
    {
        QTextEdit *par = (QTextEdit *)m_parentEdit;
        par->textCursor().movePosition(
            QTextCursor::PreviousCharacter, QTextCursor::MoveAnchor);
    }
    else
    {
        QKeyEvent *key = new QKeyEvent(
            QEvent::KeyPress, Qt::Key_Left, Qt::NoModifier, "", false, 1);
        QCoreApplication::postEvent(m_parentEdit, key);
    }
}

void UIKeyboardType::rightCursor()
{
    if (!m_parentEdit)
        return;

    if (m_parentEdit->inherits("QLineEdit"))
    {
        QLineEdit *par = (QLineEdit *)m_parentEdit;
        par->cursorForward(m_shiftLKey->IsOn());
    }
    else if (m_parentEdit->inherits("QTextEdit"))
    {
        QTextEdit *par = (QTextEdit *)m_parentEdit;
        par->textCursor().movePosition(
            QTextCursor::NextCharacter, QTextCursor::MoveAnchor);
    }
    else
    {
        QKeyEvent *key = new QKeyEvent(
            QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier, "", false, 1);
        QCoreApplication::postEvent(m_parentEdit, key);
    }
}

void UIKeyboardType::backspaceKey()
{
    if (!m_parentEdit)
        return;

    if (m_parentEdit->inherits("QLineEdit"))
    {
        QLineEdit *par = (QLineEdit *)m_parentEdit;
        par->backspace();
    }
    else if (m_parentEdit->inherits("MythRemoteLineEdit"))
    {
        MythRemoteLineEdit *par = (MythRemoteLineEdit *)m_parentEdit;
        par->backspace();
    }
    else
    {
        QKeyEvent *key = new QKeyEvent(
            QEvent::KeyPress, Qt::Key_Backspace, Qt::NoModifier, "", false, 1);
        QCoreApplication::postEvent(m_parentEdit, key);
    }
}

void UIKeyboardType::delKey()
{
    if (!m_parentEdit)
        return;

    if (m_parentEdit->inherits("QLineEdit"))
    {
        QLineEdit *par = (QLineEdit *)m_parentEdit;
        par->del();
    }
    else if (m_parentEdit->inherits("MythRemoteLineEdit"))
    {
        MythRemoteLineEdit *par = (MythRemoteLineEdit *)m_parentEdit;
        par->del();
    }
    else
    {
        QKeyEvent *key = new QKeyEvent(
            QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier, "", false, 1);
        QCoreApplication::postEvent(m_parentEdit, key);
    }
}

void UIKeyboardType::altGrOnOff()
{
    if (m_lockKey->IsOn())
    {
        m_shiftLKey->SetOn(false);
        if (m_shiftRKey) m_shiftRKey->SetOn(false);
        if (m_altKey) m_altKey->SetOn(false);
        m_lockKey->SetOn(false);
    }
    updateButtons();
}

void UIKeyboardType::compOnOff()
{
    m_bCompTrap = !m_bCompTrap;
    m_comp1 = "";
}

void UIKeyboardType::charKey()
{
    if (m_focusedKey && m_focusedKey->GetType() == "char")
    {
        insertChar(m_focusedKey->GetChar());
        shiftOff();
    }
}

void UIKeyboardType::insertChar(QString c)
{
    if (!m_bCompTrap)
    {
        if (m_parentEdit->inherits("QLineEdit"))
        {
            QLineEdit *par = (QLineEdit *)m_parentEdit;
            par->insert(c);
        }
        else if (m_parentEdit->inherits("MythRemoteLineEdit"))
        {
            MythRemoteLineEdit *par = (MythRemoteLineEdit *)m_parentEdit;
            par->insert(c);
        }
        else
        {
#ifdef QT3_SUPPORT
            QKeyEvent *key = new QKeyEvent(QEvent::KeyPress, 0, 0, 0, c, false, c.length());
#else // if !QT3_SUPPORT
            QKeyEvent *key = new QKeyEvent(QEvent::KeyPress, 0, Qt::NoModifier, c, false, c.length());
#endif //!QT3_SUPPORT
            QCoreApplication::postEvent(m_parentEdit, key);
        }
    }
    else
    {
        if (m_comp1.isEmpty()) m_comp1 = c;
        else
        {
            // Produce the composed key.
            for (int i=0; i<numcomps; i++)
            {
                if ((m_comp1 == comps[i][0]) && (c == comps[i][1]))
                {
                    if (m_parentEdit->inherits("QLineEdit"))
                    {
                        QLineEdit *par = (QLineEdit *)m_parentEdit;
                        par->insert(comps[i][2]);
                    }
                    else if (m_parentEdit->inherits("MythRemoteLineEdit"))
                    {
                        MythRemoteLineEdit *par = (MythRemoteLineEdit *)m_parentEdit;
                        par->insert(comps[i][2]);
                    }
                    else
                    {
#ifdef QT3_SUPPORT
                        QKeyEvent *key = new QKeyEvent(
                            QEvent::KeyPress, 0, 0, 0,
                            comps[i][2], false, comps[i][2].length());
#else // if !QT3_SUPPORT
                        QKeyEvent *key = new QKeyEvent(
                            QEvent::KeyPress, 0, Qt::NoModifier,
                            comps[i][2], false, comps[i][2].length());
#endif // !QT3_SUPPORT
                        QCoreApplication::postEvent(m_parentEdit, key);
                    }

                    break;
                }
            }
            // Reset compTrap.
            m_comp1 = "";
            m_bCompTrap = false;
        }
    }
}

void UIKeyboardType::lockOnOff()
{
    if (m_lockKey->IsOn())
    {
        if (!(m_altKey && m_altKey->IsOn()))
        {
            m_shiftLKey->SetOn(true);
            if (m_shiftRKey) m_shiftRKey->SetOn(true);
        }
    }
    else
    {
        m_shiftLKey->SetOn(false);
        if (m_shiftRKey) m_shiftRKey->SetOn(false);
        if (m_altKey) m_altKey->SetOn(false);
    }
    updateButtons();
}

void UIKeyboardType::shiftOff()
{
    if (!m_lockKey->IsOn())
    {
        m_shiftLKey->SetOn(false);
        if (m_shiftRKey) m_shiftRKey->SetOn(false);
        if (m_altKey) m_altKey->SetOn(false);
    }
    updateButtons();
}

void UIKeyboardType::close(void)
{
    if (!m_parentDialog)
        return;

    m_parentDialog->done(kDialogCodeAccepted);
}

void UIKeyboardType::updateButtons()
{
    bool bShift = m_shiftLKey->IsOn();
    bool bAlt = (m_altKey ? m_altKey->IsOn() : false);

    KeyList::iterator it = m_keyList.begin();
    for (; it != m_keyList.end(); ++it)
        (*it)->SetShiftState(bShift, bAlt);
}

void UIKeyboardType::shiftLOnOff()
{
    if (m_lockKey->IsOn())
    {
        m_shiftLKey->SetOn(false);
        if (m_shiftRKey) m_shiftRKey->SetOn(false);
        if (m_altKey) m_altKey->SetOn(false);
        m_lockKey->SetOn(false);
    }
    else if (m_shiftRKey) m_shiftRKey->SetOn(m_shiftLKey->IsOn());

    updateButtons();
}

void UIKeyboardType::shiftROnOff()
{
    if (!m_shiftRKey) return;

    if (m_lockKey->IsOn())
    {
        m_shiftLKey->SetOn(false);
        m_shiftRKey->SetOn(false);
        if (m_altKey) m_altKey->SetOn(false);
        m_lockKey->SetOn(false);
    }
    else m_shiftLKey->SetOn(m_shiftRKey->IsOn());

    updateButtons();
}

void UIKeyboardType::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("qt", e, actions, false);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "UP")
        {
            moveUp();
        }
        else if (action == "DOWN")
        {
            moveDown();
        }
        else if (action == "LEFT")
        {
            moveLeft();
        }
        else if (action == "RIGHT")
        {
            moveRight();
        }
        else if (action == "SELECT")
            m_focusedKey->activate();
        else
            handled = false;
    }

    if (!handled)
    {
        QKeyEvent *key = new QKeyEvent(
            e->type(), e->key(), e->modifiers(),
            e->text(), e->isAutoRepeat(), e->count());
        QCoreApplication::postEvent(m_parentEdit, key);
        m_parentEdit->setFocus();
    }
}

void UIKeyboardType::moveUp()
{
    if (!m_focusedKey)
    {
        m_focusedKey = m_doneKey;
        return;
    }

    UIKeyType *newKey = findKey(m_focusedKey->GetMove("Up"));

    if (newKey)
    {
        m_focusedKey->looseFocus();
        m_focusedKey = newKey;
        m_focusedKey->takeFocus();
    }
}

void UIKeyboardType::moveDown()
{
    if (!m_focusedKey)
    {
        m_focusedKey = m_doneKey;
        return;
    }

    UIKeyType *newKey = findKey(m_focusedKey->GetMove("Down"));

    if (newKey)
    {
        m_focusedKey->looseFocus();
        m_focusedKey = newKey;
        m_focusedKey->takeFocus();
    }
}

void UIKeyboardType::moveLeft()
{
    if (!m_focusedKey)
    {
        m_focusedKey = m_doneKey;
        return;
    }

    UIKeyType *newKey = findKey(m_focusedKey->GetMove("Left"));

    if (newKey)
    {
        m_focusedKey->looseFocus();
        m_focusedKey = newKey;
        m_focusedKey->takeFocus();
    }
}

void UIKeyboardType::moveRight()
{
    if (!m_focusedKey)
    {
        m_focusedKey = m_doneKey;
        return;
    }

    UIKeyType *newKey = findKey(m_focusedKey->GetMove("Right"));

    if (newKey)
    {
        m_focusedKey->looseFocus();
        m_focusedKey = newKey;
        m_focusedKey->takeFocus();
    }
}

UIKeyType *UIKeyboardType::findKey(QString keyName)
{
    KeyList::const_iterator it = m_keyList.begin();
    for (; it != m_keyList.end(); ++it)
    {
        if ((*it)->getName() == keyName)
            return (*it);
    }
    return NULL;
}

void UIKeyboardType::AddKey(UIKeyType *key)
{
    m_keyList.append(key);

    if (key->GetType().toLower() == "done")
    {
        key->takeFocus();
        m_focusedKey = key;
    }
}

// vim:set sw=4 expandtab:
