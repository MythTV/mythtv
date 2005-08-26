#include <iostream>
using namespace std;

#include <qfontmetrics.h>

#include "mythuitype.h"
#include "mythimage.h"
#include "mythpainter.h"
#include "mythmainwindow.h"

MythUIType::MythUIType(QObject *parent, const char *name)
          : QObject(parent, name)
{
    m_Visible = true;
    m_CanHaveFocus = m_HasFocus = false;
    m_Area = QRect(0, 0, 0, 0);
    m_NeedsRedraw = false;
    m_Alpha = 255;
    m_AlphaChangeMode = m_AlphaChange = m_AlphaMin = 0;
    m_AlphaMax = 255;
    m_Moving = false;
    m_XYDestination = QPoint(0, 0);
    m_XYSpeed = QPoint(0, 0);

    m_Parent = NULL;
    if (parent)
    {
        m_Parent = dynamic_cast<MythUIType *>(parent);
        if (m_Parent)
            m_Parent->AddChild(this);
    }
    
    //
    //  Optional elements that draw a frame around any MythUIType if user sets
    //  debug true in the xml
    //

    m_debug_mode = false;
    m_debug_hor_line = NULL;
    m_debug_ver_line = NULL;
    m_debug_color = QColor(0,0,255);    // blue by default
}

MythUIType::~MythUIType()
{
    //
    //  delete debugging images if they exist
    //
    
    if(m_debug_hor_line)
    {
        m_debug_hor_line->DownRef();
        m_debug_hor_line = NULL;
    }
    if(m_debug_ver_line)
    {
        m_debug_ver_line->DownRef();
        m_debug_ver_line = NULL;
    }
}

void MythUIType::AddChild(MythUIType *child)
{
    if (!child)
    {
        return;
    }

    m_ChildrenList.push_back(child);
}

MythUIType *MythUIType::GetChild(const char *name, const char *inherits)
{
    QObject *ret = child(name, inherits);
    if (ret)
        return dynamic_cast<MythUIType *>(ret);
    return NULL;
}

QValueVector<MythUIType *> *MythUIType::GetAllChildren(void)
{
    return &m_ChildrenList;
}

bool MythUIType::NeedsRedraw(void)
{
    return m_NeedsRedraw;
}

void MythUIType::SetRedraw(void)
{
    m_NeedsRedraw = true;
    m_DirtyRect = m_Area;
    if (m_Parent)
        m_Parent->SetChildNeedsRedraw(this);
}

void MythUIType::SetChildNeedsRedraw(MythUIType *child)
{
    m_NeedsRedraw = true;

    QRect childRect = child->GetDirtyArea();
    childRect.moveBy(m_Area.x(), m_Area.y());

    if (m_DirtyRect != QRect())
        m_DirtyRect = m_DirtyRect.unite(childRect);
    else
        m_DirtyRect = childRect;

    // For poorly defined items
    QSize aSize = m_Area.size();
    aSize = aSize.expandedTo(m_DirtyRect.size());
    m_Area.setSize(aSize);

    if (m_Parent)
        m_Parent->SetChildNeedsRedraw(this);
}

bool MythUIType::CanTakeFocus(void)
{
    return m_CanHaveFocus;
}

void MythUIType::SetCanTakeFocus(bool set)
{
    m_CanHaveFocus = set;
}

void MythUIType::HandleMovementPulse(void)
{
    if (!m_Moving)
        return;

    QPoint curXY = m_Area.topLeft();

    int xdir = m_XYDestination.x() - curXY.x();
    int ydir = m_XYDestination.y() - curXY.y();

    curXY.setX(curXY.x() + m_XYSpeed.x());
    curXY.setY(curXY.y() + m_XYSpeed.y());

    if ((xdir > 0 && curXY.x() >= m_XYDestination.x()) ||
        (xdir < 0 && curXY.x() <= m_XYDestination.x()) ||
        (xdir == 0))
    {
        m_XYSpeed.setX(0);
    }

    if ((ydir > 0 && curXY.y() >= m_XYDestination.y()) ||
        (ydir <= 0 && curXY.y() <= m_XYDestination.y()) ||
        (ydir == 0))
    {
        m_XYSpeed.setY(0);
    }

    SetRedraw();

    if (m_XYSpeed.x() == 0 && m_XYSpeed.y() == 0)
    {
        m_Moving = false;
        emit FinishedMoving();
    }

    m_Area.moveTopLeft(curXY);
}

void MythUIType::HandleAlphaPulse(void)
{
    if (m_AlphaChangeMode == 0)
        return;

    m_Alpha += m_AlphaChange;
    if (m_Alpha > 255)
        m_Alpha = 255;
    if (m_Alpha < 0)
        m_Alpha = 0;

    if (m_Alpha >= m_AlphaMax || m_Alpha <= m_AlphaMin)
    {
        if (m_AlphaChangeMode == 2)
        {
            m_AlphaChange *= -1;
        }
        else
        {
            m_AlphaChangeMode = 0;
            m_AlphaChange = 0;
        }
    }

    SetRedraw();
}

void MythUIType::Pulse(void)
{
    HandleMovementPulse();
    HandleAlphaPulse();

    QValueVector<MythUIType *>::Iterator it;
    for (it = m_ChildrenList.begin(); it != m_ChildrenList.end(); ++it)
        (*it)->Pulse();
}

int MythUIType::CalcAlpha(int alphamod)
{
    return (int)(m_Alpha * (alphamod / 255.0));
}

void MythUIType::setDebugColor(QColor c)
{
    m_debug_color = c;
}

void MythUIType::makeDebugImages()
{
    //
    //  MythImage::FromQImage() deletes the QImage's
    //

    QImage *temp_image = new QImage(m_Area.width(), 1, 32);
    temp_image->fill(m_debug_color.rgb());
    m_debug_hor_line = MythImage::FromQImage(&temp_image);

    temp_image = new QImage(1, m_Area.height(), 32);
    temp_image->fill(m_debug_color.rgb());
    m_debug_ver_line = MythImage::FromQImage(&temp_image);
}

void MythUIType::DrawSelf(MythPainter *, int, int, int, QRect)
{
}

void MythUIType::Draw(MythPainter *p, int xoffset, int yoffset, int alphaMod,
                      QRect clipRect)
{
    if (!m_Visible)
    {
        return;
    }

    QRect realArea = m_Area;
    realArea.moveBy(xoffset, yoffset);

    if (!realArea.intersects(clipRect))
        return;

    DrawSelf(p, xoffset, yoffset, alphaMod, clipRect);

    QValueVector<MythUIType *>::Iterator it;
    for (it = m_ChildrenList.begin(); it != m_ChildrenList.end(); ++it)
    {
        (*it)->Draw(p, xoffset + m_Area.x(), yoffset + m_Area.y(), 
                    CalcAlpha(alphaMod), clipRect);
    }

    m_NeedsRedraw = false;
    m_DirtyRect = QRect();
 
    //
    //  If I'm in debugging mode, draw a frame at the edge of my area
    //
    
    if (m_debug_mode && m_Area.width() > 0 && m_Area.height() > 0)
    {
        if(!m_debug_hor_line || !m_debug_ver_line)
        {
            makeDebugImages();
        }
        
        //
        //  This is slow, but is only called when debug is set in the xml
        //

        QRect area = QRect(m_Area.left() + xoffset, m_Area.top() + yoffset, 1, m_Area.height());
        p->DrawImage(area, m_debug_ver_line, m_debug_ver_line->rect(), CalcAlpha(alphaMod)); 

        area = QRect(m_Area.right() + xoffset, m_Area.top() + yoffset, 1, m_Area.height());
        p->DrawImage(area, m_debug_ver_line, m_debug_ver_line->rect(), CalcAlpha(alphaMod)); 

        area = QRect(m_Area.left() + xoffset, m_Area.top() + yoffset, m_Area.width(), 1);
        p->DrawImage(area, m_debug_hor_line, m_debug_ver_line->rect(), CalcAlpha(alphaMod)); 

        area = QRect(m_Area.left() + xoffset, m_Area.bottom() + yoffset, m_Area.width(), 1);
        p->DrawImage(area, m_debug_hor_line, m_debug_ver_line->rect(), CalcAlpha(alphaMod)); 
    }
}

void MythUIType::SetPosition(int x, int y)
{
    SetPosition(QPoint(x, y));
}

void MythUIType::SetPosition(const QPoint &pos)
{
    if (m_Area.topLeft() == pos)
        return;

    m_Area.moveTopLeft(pos);
    SetRedraw();
}

void MythUIType::SetArea(const QRect &rect)
{
    if (rect == m_Area)
        return;

    m_Area = rect;
    SetRedraw();
}

QRect MythUIType::GetArea(void) const
{
    return m_Area;
}

QRect MythUIType::GetDirtyArea(void) const
{
    return m_DirtyRect;
}

QString MythUIType::cutDown(const QString &data, QFont *font,
                            bool multiline, int overload_width,
                            int overload_height)
{
    int length = data.length();
    if (length == 0)
        return data;

    int maxwidth = m_Area.width();
    if (overload_width != -1)
        maxwidth = overload_width;
    int maxheight = m_Area.height();
    if (overload_height != -1)
        maxheight = overload_height;

    int justification = Qt::AlignLeft | Qt::WordBreak;
    QFontMetrics fm(*font);

    int margin = length - 1;
    int index = 0;
    int diff = 0;

    while (margin > 0)
    {
        if (multiline)
            diff = maxheight - fm.boundingRect(0, 0, maxwidth, maxheight,
                                               justification, data,
                                               index + margin).height();
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

bool MythUIType::IsVisible(void)
{
    return m_Visible;
}

void MythUIType::MoveTo(QPoint destXY, QPoint speedXY)
{
    if (!GetMythPainter()->SupportsAnimation())
        return;

    if (destXY.x() == m_Area.x() && destXY.y() == m_Area.y())
        return;

    m_Moving = true;

    m_XYDestination = destXY;
    m_XYSpeed = speedXY;
}

void MythUIType::AdjustAlpha(int mode, int alphachange, int minalpha,
                             int maxalpha)
{
    if (!GetMythPainter()->SupportsAlpha())
        return;

    m_AlphaChangeMode = mode;
    m_AlphaChange = alphachange;
    m_AlphaMin = minalpha;
    m_AlphaMax = maxalpha;

    if (m_Alpha > m_AlphaMax)
        m_Alpha = m_AlphaMax;
    if (m_Alpha < m_AlphaMin)
        m_Alpha = m_AlphaMin;
}

void MythUIType::SetAlpha(int newalpha)
{
    m_Alpha = newalpha;
    SetRedraw();
}

int MythUIType::GetAlpha(void)
{
    return m_Alpha;
}

bool MythUIType::keyPressEvent(QKeyEvent *)
{
    return false;
}

void MythUIType::customEvent(QCustomEvent *)
{
    return;
}

void MythUIType::LoseFocus(void)
{
    if (!m_CanHaveFocus || !m_HasFocus)
        return;

    emit LosingFocus();
    m_HasFocus = false;
    Refresh();
}

bool MythUIType::TakeFocus(void)
{
    if (!m_CanHaveFocus || m_HasFocus)
        return false;

    m_HasFocus = true;
    Refresh();
    emit TakingFocus();
    return true;
}

void MythUIType::Activate(void)
{
}

void MythUIType::Refresh(void)
{
    SetRedraw();
}

void MythUIType::SetVisible(bool visible)
{
    if (visible != m_Visible)
    {
        m_Visible = visible;
        SetRedraw();
    }
}

void MythUIType::Hide(void)
{
    m_Visible = false;
    SetRedraw();
    emit Hiding();
}

void MythUIType::Show(void)
{
    m_Visible = true;
    SetRedraw();
    emit Showing();
}

void MythUIType::AddFocusableChildrenToList(QPtrList<MythUIType> &focusList)
{
    if (m_HasFocus)
        focusList.append(this);

    QValueVector<MythUIType *>::Iterator it;
    for (it = m_ChildrenList.begin(); it != m_ChildrenList.end(); ++it)
        (*it)->AddFocusableChildrenToList(focusList);
}

QFont MythUIType::CreateFont(const QString &face, int pointSize,
                             int weight, bool italic)
{
    return GetMythMainWindow()->CreateFont(face, pointSize, weight, italic);
}

QRect MythUIType::NormRect(const QRect &rect)
{
    return GetMythMainWindow()->NormRect(rect);
}

QPoint MythUIType::NormPoint(const QPoint &point)
{
    return GetMythMainWindow()->NormPoint(point);
}

int MythUIType::NormX(const int x)
{
    return GetMythMainWindow()->NormX(x);
}

int MythUIType::NormY(const int y)
{
    return GetMythMainWindow()->NormY(y);
}

