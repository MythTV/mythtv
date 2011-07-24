
#include <sstream>

#include "mythrect.h"
#include "mythmainwindow.h"

MythRect::MythRect()
         : QRect()
{
    Init();
}

MythRect::MythRect(int x, int y, int width, int height)
         : QRect(x, y, width, height)
{
    Init();
}

MythRect::MythRect(const QString &sX, const QString &sY, const QString &sWidth,
                   const QString &sHeight)
         : QRect()
{
    Init();
    setRect(sX,sY,sWidth,sHeight);
}

MythRect::MythRect(QRect rect)
          : QRect(rect)
{
    Init();
}

bool MythRect::operator== (const MythRect &other) const
{
    return ((m_percentWidth == other.m_percentWidth) &&
            (m_percentHeight == other.m_percentHeight) &&
            (m_percentX == other.m_percentX) &&
            (m_percentY == other.m_percentY) &&
            (m_offsetWidth == other.m_offsetWidth) &&
            (m_offsetHeight == other.m_offsetHeight) &&
            (m_offsetX == other.m_offsetX) &&
            (m_offsetY == other.m_offsetY) &&
            (QRect)(*this) == (QRect)other);
}

void MythRect::Init()
{
    m_needsUpdate = true;
    m_percentWidth = m_percentHeight = m_percentX = m_percentY = 0.0;
    m_offsetWidth = m_offsetHeight = m_offsetX = m_offsetY = 0;
}

void MythRect::CalculateArea(const MythRect & parentArea)
{
    QRect area  = parentArea.toQRect();
    if ((m_parentArea == area && !m_needsUpdate) || !parentArea.isValid())
        return;

    m_parentArea  = area;

    int w = width();
    int h = height();
    int X = x();
    int Y = y();

    if (m_percentX > 0.0)
        X = (int) (m_percentX * (float)m_parentArea.width()) + m_offsetX;
    if (m_percentY > 0.0)
        Y = (int) (m_percentY * (float)m_parentArea.height()) + m_offsetY;
    if (m_percentWidth > 0.0)
        w = (int) (m_percentWidth * (float)(m_parentArea.width() - X))
                   + m_offsetWidth;
    else if (m_offsetWidth != 0)
        w = m_parentArea.width() - X + m_offsetWidth;
    if (m_percentHeight > 0.0)
        h = (int) (m_percentHeight * (float)(m_parentArea.height() - Y))
                   + m_offsetHeight;
    else if (m_offsetHeight != 0)
        h = m_parentArea.height() - Y + m_offsetHeight;

    QRect::setRect(X,Y,w,h);

    m_needsUpdate = false;
}

void MythRect::NormRect(void)
{

    if (m_percentWidth == 0.0)
        QRect::setWidth(GetMythMainWindow()->NormX(width()));

    if (m_percentHeight == 0.0)
        QRect::setHeight(GetMythMainWindow()->NormY(height()));

    int X = 0;
    if (m_percentX == 0.0)
        X = GetMythMainWindow()->NormX(x());

    int Y = 0;
    if (m_percentY == 0.0)
        Y = GetMythMainWindow()->NormY(y());

    QRect::moveTopLeft(QPoint(X,Y));

    normalized();
}

void MythRect::setRect(const QString &sX, const QString &sY,
                       const QString &sWidth, const QString &sHeight)
{
    setX(sX);
    setY(sY);
    setWidth(sWidth);
    setHeight(sHeight);
}

/**
 * \brief parse the position
 *
 * \return true of position is absolute
 */
bool MythRect::parsePosition(float & percent, int & offset, int & absolute,
                             const QString &value)
{
    /*
      Position can be either an absolute, or a percentage with an
      optional offset.

      720
      -5
      33%
      75%+10
     */

    percent = 0.0;
    offset = 0;
    absolute = 0;

    if (value.isEmpty())
        return true;

    int  number;
    bool is_offset;
    std::istringstream  is(value.trimmed().toStdString().c_str());

    is_offset = (is.peek() == '+' || is.peek() == '-');
    is >> number;
    if (!is)
        return true;

    is >> std::ws;
    if (is.peek() != '%')
    {
        if (is_offset)
            offset = number;
        else
            absolute = number;
        return !is_offset;
    }
    is.ignore(1);

    percent = static_cast<float>(number) / 100.0;
    is >> offset;
    return false;
}

void MythRect::setX(const QString &sX)
{
    int absoluteX;

    if (parsePosition(m_percentX, m_offsetX, absoluteX, sX))
        QRect::setX(absoluteX);
    else if (m_percentX == 0.0)
    {
        QRect::setX(m_offsetX);
        m_offsetX = 0;
    }
    else
        m_needsUpdate = true;
}

void MythRect::setY(const QString &sY)
{
    int absoluteY;

    if (parsePosition(m_percentY, m_offsetY, absoluteY, sY))
        QRect::setY(absoluteY);
    else if (m_percentY == 0.0)
    {
        QRect::setY(m_offsetY);
        m_offsetY = 0;
    }
    else
        m_needsUpdate = true;
}

void MythRect::setWidth(const QString &sWidth)
{
    int absoluteWidth;

    if (parsePosition(m_percentWidth, m_offsetWidth, absoluteWidth, sWidth))
        QRect::setWidth(absoluteWidth);
    else
        m_needsUpdate = true;
}

void MythRect::setHeight(const QString &sHeight)
{
    int absoluteHeight;

    if (parsePosition(m_percentHeight, m_offsetHeight, absoluteHeight, sHeight))
        QRect::setHeight(absoluteHeight);
    else
        m_needsUpdate = true;
}

MythPoint MythRect::topLeft(void) const
{
    MythPoint point;
    point.setX(getX());
    point.setY(getY());
    return point;
}

void MythRect::moveTopLeft(const MythPoint &point)
{
    moveLeft(point.getX());
    moveTop(point.getY());
}

void MythRect::moveLeft(const QString &sX)
{
    int absoluteX;

    if (parsePosition(m_percentX, m_offsetX, absoluteX, sX))
        QRect::moveLeft(absoluteX);
    else if (m_percentX == 0.0)
    {
        QRect::moveLeft(m_offsetX);
        m_offsetX = 0;
    }
    else // Move left to the absolute pos specified by the percentage/offset
        m_needsUpdate = true;
}

void MythRect::moveTop(const QString &sY)
{
    int absoluteY;

    if (parsePosition(m_percentY, m_offsetY, absoluteY, sY))
        QRect::moveTop(absoluteY);
    else if (m_percentY == 0.0)
    {
        QRect::moveTop(m_offsetY);
        m_offsetY = 0;
    }
    else  // Move top to the absolute pos specified by the percentage/offset
        m_needsUpdate = true;
}

QString MythRect::getX(void) const
{
    QString stringX;
    if (m_percentX > 0.0)
        stringX = QString("%1%").arg((int)(m_percentX * 100));
    else
        stringX = QString("%1").arg(x());
    if (m_offsetX != 0)
    {
        if (m_offsetX > 0)
            stringX += '+';
        stringX += QString("%1").arg(m_offsetX);
    }
    return stringX;
}

QString MythRect::getY(void) const
{
    QString stringY;
    if (m_percentY > 0.0)
        stringY = QString("%1%").arg((int)(m_percentY * 100));
    else
        stringY = QString("%1").arg(y());
    if (m_offsetY != 0)
    {
        if (m_offsetY > 0)
            stringY += '+';
        stringY += QString("%1").arg(m_offsetY);
    }
    return stringY;
}

QString MythRect::getWidth(void) const
{
    QString stringWidth;
    if (m_percentWidth > 0.0)
        stringWidth = QString("%1%").arg((int)(m_percentWidth * 100));
    else
        stringWidth = QString("%1").arg(width());
    if (m_offsetWidth != 0)
    {
        if (m_offsetWidth > 0)
            stringWidth += '+';
        stringWidth += QString("%1").arg(m_offsetWidth);
    }
    return stringWidth;
}

QString MythRect::getHeight(void) const
{
    QString stringHeight;
    if (m_percentHeight > 0.0)
        stringHeight = QString("%1%").arg((int)(m_percentHeight * 100));
    else
        stringHeight = QString("%1").arg(height());
    if (m_offsetHeight != 0)
    {
        if (m_offsetHeight > 0)
            stringHeight += '+';
        stringHeight += QString("%1").arg(m_offsetHeight);
    }
    return stringHeight;
}

QRect MythRect::toQRect() const
{
    return QRect(x(),y(),width(),height());
}

///////////////////////////////////////////////////////////////////

MythPoint::MythPoint()
         : QPoint()
{
    Init();
    valid = false;
}

MythPoint::MythPoint(int x, int y)
         : QPoint(x, y)
{
    Init();
}

MythPoint::MythPoint(const QString &sX, const QString &sY)
         : QPoint()
{
    Init();
    setX(sX);
    setY(sY);
}

MythPoint::MythPoint(QPoint point)
          : QPoint(point)
{
    Init();
}

void MythPoint::Init()
{
    m_needsUpdate = true;
    m_percentX = m_percentY = 0.0;
    m_offsetX = m_offsetY = 0;
    valid = true;
}

void MythPoint::CalculatePoint(const MythRect & parentArea)
{
    QRect area  = parentArea.toQRect();
    if ((m_parentArea == area && !m_needsUpdate) || !parentArea.isValid())
        return;

    m_parentArea  = area;

    int X = x();
    int Y = y();

    if (m_percentX > 0.0)
        X = (int) (m_percentX * (float)m_parentArea.width()) + m_offsetX;
    if (m_percentY > 0.0)
        Y = (int) (m_percentY * (float)m_parentArea.height()) + m_offsetY;

    QPoint::setX(X);
    QPoint::setY(Y);

    m_needsUpdate = false;
    valid = true;
}

void MythPoint::NormPoint(void)
{
    if (m_percentX == 0.0)
        QPoint::setX(GetMythMainWindow()->NormX(x()));

    if (m_percentY == 0.0)
        QPoint::setY(GetMythMainWindow()->NormY(y()));
}

/**
 * \brief parse the position
 *
 * \return true of position is absolute
 */
bool MythPoint::parsePosition(float & percent, int & offset, int & absolute,
                              const QString &value)
{
    /*
      Position can be either an absolute, or a percentage with an
      optional offset.

      720
      -5
      33%
      75%+10
     */

    percent = 0.0;
    offset = 0;
    absolute = 0;

    if (value.isEmpty())
        return true;

    int  number;
    std::istringstream  is(value.trimmed().toStdString().c_str());

    is >> number;
    if (!is)
        return true;

    is >> std::ws;
    if (is.peek() != '%')
    {
        absolute = number;
        return true;
    }
    is.ignore(1);

    percent = static_cast<float>(number) / 100.0;
    is >> offset;
    return false;
}

void MythPoint::setX(const QString &sX)
{
    int absoluteX;

    if (parsePosition(m_percentX, m_offsetX, absoluteX, sX))
        QPoint::setX(absoluteX);
    else
        m_needsUpdate = true;
}

void MythPoint::setY(const QString &sY)
{
    int absoluteY;

    if (parsePosition(m_percentY, m_offsetY, absoluteY, sY))
        QPoint::setY(absoluteY);
    else
        m_needsUpdate = true;
}

QString MythPoint::getX(void) const
{
    QString stringX;
    if (m_percentX > 0.0)
        stringX = QString("%1%").arg((int)(m_percentX * 100));
    else
        stringX = QString("%1").arg(x());
    if (m_offsetX != 0)
    {
        if (m_offsetX > 0)
            stringX += '+';
        stringX += QString("%1").arg(m_offsetX);
    }
    return stringX;
}

QString MythPoint::getY(void) const
{
    QString stringY;
    if (m_percentY > 0.0)
        stringY = QString("%1%").arg((int)(m_percentY * 100));
    else
        stringY = QString("%1").arg(y());
    if (m_offsetY != 0)
    {
        if (m_offsetY > 0)
            stringY += '+';
        stringY += QString("%1").arg(m_offsetY);
    }
    return stringY;
}

QPoint MythPoint::toQPoint() const
{
    return QPoint(x(),y());
}

